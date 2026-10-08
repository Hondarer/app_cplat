/**
 *******************************************************************************
 *  @file           trace_file.c
 *  @brief          ファイルへトレースを出力するプロバイダーを実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/04/03
 *  @version        1.0.0
 *
 *  ファイルへのトレース ログ書き込みプロバイダーを提供します。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/base/result.h>
#include <cplat/crt/string.h>
#include <cplat/crt/stdlib.h>
#include <cplat/clock/clock.h>
#include <cplat/crt/file.h>
#include <cplat/crt/path.h>
#include <cplat/crt/stdio.h>
#include <cplat/crt/sys/stat.h>
#include <cplat/sync/sync.h>
#include <cplat/trace/trace_file.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cplat/trace/trace_common.h>
#include <cplat/trace/backends/file/trace_file_internal.h>

/* ===== 内部定数 ===== */

/** 1 行分のスタック バッファー サイズ。 */
#define TRACE_FILE_LINE_BUF 1100

/** ファイル書き込みロック取得のタイムアウト (ミリ秒)。 */
#define FILE_LOCK_TIMEOUT_MS 100

/** タイムスタンプ部分の文字数 ("YYYY-MM-DDTHH:MM:SS.sss+09:00" = 29 文字)。 */
#define TRACE_FILE_TS_LEN CPLAT_CLOCK_ISO8601_LOCAL_MSEC_LEN

/** ローテーション排他用ロック ファイルのサフィックス。 */
#define TRACE_FILE_LOCK_SUFFIX ".lock"

/** パスに付加するサフィックスの最大長 (".lock\0" = 6 文字 >= ".999\0" = 5 文字)。 */
#define TRACE_FILE_SUFFIX_MAX 6

/** ファイル オープン失敗時のリトライ回数。 */
#define TRACE_FILE_OPEN_RETRY_COUNT 3

/** ファイル オープン失敗時のリトライ間隔 (ミリ秒)。 */
#define TRACE_FILE_OPEN_RETRY_INTERVAL_MS 3000

/** プロセス内 sink レジストリの初期容量。 */
#define SINK_REGISTRY_INITIAL_CAPACITY 4

/* ===== 内部構造体 ===== */

/**
 *  @brief  ファイル トレース プロバイダー ハンドル構造体 (内部定義) です。
 */
struct cplat_trace_file_sink
{
    /** ヒープ確保済みファイル パス文字列。 */
    char *path;
    /** ローテーション排他用ロック ファイルのパス文字列 (共有モードのみ非 NULL)。 */
    char *lock_path;
    /** ファイル 1 世代あたりの最大バイト数。 */
    size_t max_bytes;
    /** 現ファイルへの書き込み済みバイト数 (単一プロセス モードのインメモリ追跡)。 */
    size_t current_bytes;
    /** スレッド安全のための mutex。 */
    cplat_local_lock *mutex;
    /** ローテーション排他用プロセス間ロック (共有モードのみ非 NULL)。 */
    cplat_interprocess_lock *rotate_lock;
    /** オープン中ファイルの同一性 (共有モードで使用)。 */
    cplat_file_id self_id;
    /** 低レベル ファイル I/O ハンドル。 */
    cplat_file file;
    /** 保持する旧世代数。 */
    int generations;
    /** sink の動作フラグ。 */
    int flags;
    /** 共有モードの場合 1 (CPLAT_TRACE_FILE_SINK_SHARED 指定時)。 */
    int shared;
    /** mutex が初期化済みかどうかのフラグ。 */
    int mutex_initialized;
    /** self_id が有効かどうかのフラグ。 */
    int self_id_valid;
    /** 64 bit アーキテクチャーで構造体末尾の暗黙パディングを防ぐ。 */
    int pad;
};

/* ===== プロセス内 sink レジストリ ===== */

/**
 *  @brief  プロセス内で同一パスの sink を共有するためのレジストリ エントリです。
 */
struct sink_registry_entry
{
    /** 正規化済みパス文字列 (ヒープ確保)。 */
    char *key;
    /** 共有対象の sink。 */
    cplat_trace_file_sink *sink;
    /** 参照カウント。0 になったら解放する。 */
    int refcount;
    /** 明示的アラインメント。 */
    unsigned int pad;
};

struct sink_registry
{
    struct sink_registry_entry *items;
    size_t count;
    size_t capacity;
};

static struct sink_registry s_sink_registry = {0};
static cplat_local_lock *s_sink_registry_lock;
static cplat_once_flag s_sink_registry_lock_once = {0};

static void init_sink_registry_lock(void)
{
    (void)cplat_local_lock_create(&s_sink_registry_lock);
}

/**
 *  @brief  sink レジストリの排他ロックを取得します。
 */
static void sink_registry_lock(void)
{
    cplat_call_once(&s_sink_registry_lock_once, init_sink_registry_lock);
    cplat_local_lock_lock(s_sink_registry_lock, CPLAT_SYNC_WAIT_FOREVER);
}

/**
 *  @brief  sink レジストリの排他ロックを解放します。
 */
static void sink_registry_unlock(void)
{
    cplat_local_lock_unlock(s_sink_registry_lock);
}

/**
 *  @brief          レジストリ キー用にパスを正規化した文字列を確保します。
 *  @param[in]      path  対象のファイル パス。
 *  @return         ヒープ確保された正規化済みパス。呼び出し元が free すること。失敗時 NULL。
 *
 *  cplat_path_get_full で絶対化する。絶対化に失敗した場合は元のパス文字列をそのまま使用します。
 */
static char *build_registry_key(const char *path)
{
    char full[PLATFORM_PATH_MAX];
    const char *src = path;
    char *key;
    size_t len;

    if (cplat_path_get_full(full, sizeof(full), NULL, path) == CPLAT_OK)
    {
        src = full;
    }

    len = strlen(src);
    key = (char *)cplat_malloc(len + 1);
    if (key == NULL)
    {
        return NULL;
    }
    memcpy(key, src, len + 1);
    return key;
}

/**
 *  @brief          レジストリ キー同士を比較します。
 *  @param[in]      lhs  比較する 1 つ目のキー。
 *  @param[in]      rhs  比較する 2 つ目のキー。
 *  @return         一致時 1、不一致時 0。
 *
 *  Windows ではファイル システムの慣習に合わせて大文字小文字を区別しません。
 */
static int registry_key_equals(const char *lhs, const char *rhs)
{
#if defined(PLATFORM_WINDOWS)
    return cplat_strcasecmp(lhs, rhs) == 0;
#else  /* PLATFORM_WINDOWS */
    return strcmp(lhs, rhs) == 0;
#endif /* PLATFORM_WINDOWS */
}

/**
 *  @brief          キーが一致するレジストリ エントリを検索する (ロック保持中) です。
 *  @param[in]      key  検索する正規化済みパス。
 *  @return         一致したエントリ。見つからない場合 NULL。
 */
static struct sink_registry_entry *sink_registry_find_by_key_locked(const char *key)
{
    size_t i;

    for (i = 0; i < s_sink_registry.count; i++)
    {
        if (registry_key_equals(s_sink_registry.items[i].key, key))
        {
            return &s_sink_registry.items[i];
        }
    }
    return NULL;
}

/**
 *  @brief          sink ポインターが一致するレジストリ エントリを検索する (ロック保持中) です。
 *  @param[in]      sink  検索する sink。
 *  @return         一致したエントリ。見つからない場合 NULL。
 */
static struct sink_registry_entry *sink_registry_find_by_sink_locked(const cplat_trace_file_sink *sink)
{
    size_t i;

    for (i = 0; i < s_sink_registry.count; i++)
    {
        if (s_sink_registry.items[i].sink == sink)
        {
            return &s_sink_registry.items[i];
        }
    }
    return NULL;
}

/**
 *  @brief          sink をレジストリへ登録する (ロック保持中) です。
 *  @param[in]      key   正規化済みパス。成功時はレジストリが所有権を持つ。
 *  @param[in]      sink  登録する sink。
 *  @return         成功時 0、メモリ確保失敗時 -1。
 */
static int sink_registry_register_locked(char *key, cplat_trace_file_sink *sink)
{
    if (s_sink_registry.count == s_sink_registry.capacity)
    {
        struct sink_registry_entry *new_items;
        size_t new_capacity;

        if (s_sink_registry.capacity == 0)
        {
            new_capacity = SINK_REGISTRY_INITIAL_CAPACITY;
        }
        else
        {
            new_capacity = s_sink_registry.capacity * 2;
        }

        new_items = (struct sink_registry_entry *)cplat_realloc(s_sink_registry.items, new_capacity,
                                                                sizeof(struct sink_registry_entry));
        if (new_items == NULL)
        {
            return -1;
        }
        s_sink_registry.items = new_items;
        s_sink_registry.capacity = new_capacity;
    }

    s_sink_registry.items[s_sink_registry.count].key = key;
    s_sink_registry.items[s_sink_registry.count].sink = sink;
    s_sink_registry.items[s_sink_registry.count].refcount = 1;
    s_sink_registry.items[s_sink_registry.count].pad = 0;
    s_sink_registry.count++;
    return 0;
}

/**
 *  @brief          レジストリ エントリを削除する (ロック保持中)。key の解放は呼び出し元が行います。
 *  @param[in]      entry  削除するエントリ (レジストリ配列内を指すこと)。
 */
static void sink_registry_remove_locked(struct sink_registry_entry *entry)
{
    s_sink_registry.count--;
    *entry = s_sink_registry.items[s_sink_registry.count];
    s_sink_registry.items[s_sink_registry.count].key = NULL;
    s_sink_registry.items[s_sink_registry.count].sink = NULL;
    s_sink_registry.items[s_sink_registry.count].refcount = 0;
    s_sink_registry.items[s_sink_registry.count].pad = 0;
}

/* ===== 内部ヘルパー関数 ===== */

/**
 *  @brief  基本オープン フラグを返します。
 *
 *  `cplat_file_open()` は排他的オープンを提供しません。
 *  単一プロセス モードではプロセス間の調停を行わないため、呼び出し側が他プロセスから
 *  同一パスへ書き込まないことを保証する必要があります。
 */
static int base_open_flags(void)
{
    return CPLAT_FILE_OPEN_CREATE | CPLAT_FILE_OPEN_APPEND;
}

/**
 *  @brief  ファイルを追記モードで 1 回だけ開き current_bytes を初期サイズで初期化します。
 *  @param[in,out]  p  ファイル sink。
 *  @return 成功 0 / 失敗 -1。
 *
 *  親ディレクトリが存在しない場合は cplat_makedirs で自動生成する (best-effort)。\n
 *  生成に失敗しても後続の cplat_file_open の結果で最終判定する。\n
 *  共有モードではオープンしたファイルの同一性を self_id にキャッシュする
 *  (取得失敗時は self_id_valid = 0 とし、次回書き込み時に開き直す)。
 */
static int open_file(cplat_trace_file_sink *p)
{
    int flags = base_open_flags();

    if ((p->flags & CPLAT_TRACE_FILE_SINK_OS_BUFFERED) == 0)
    {
        flags |= CPLAT_FILE_OPEN_WRITE_THROUGH;
    }

    p->self_id_valid = 0;

    if (cplat_file_open(&p->file, p->path, flags, NULL) != CPLAT_OK)
    {
        p->current_bytes = 0;
        return -1;
    }

    if (cplat_file_get_size(&p->file, &p->current_bytes, NULL) != CPLAT_OK)
    {
        p->current_bytes = 0;
    }

    if (p->shared != 0)
    {
        if (cplat_file_get_id(&p->file, &p->self_id, NULL) == CPLAT_OK)
        {
            p->self_id_valid = 1;
        }
    }

    return 0;
}

/**
 *  @brief  ファイルを追記モードで開き、失敗時は一定間隔で再試行します。
 *  @param[in,out]  p  ファイル sink。
 *  @return 成功 0 / 失敗 -1。
 *
 *  sink の生成中だけ使用し、生成後のローカル mutex を保持した処理からは呼び出しません。
 */
static int open_file_with_retry(cplat_trace_file_sink *p)
{
    int retry_count;

    if (open_file(p) == 0)
    {
        return 0;
    }

    for (retry_count = 0; retry_count < TRACE_FILE_OPEN_RETRY_COUNT; retry_count++)
    {
        cplat_sleep_ms(TRACE_FILE_OPEN_RETRY_INTERVAL_MS);
        if (open_file(p) == 0)
        {
            return 0;
        }
    }

    return -1;
}

/**
 *  @brief  ローテーション後の新規ファイルを空で作成して 1 回だけ開く (単一プロセス モード用) です。
 *  @param[in,out]  p  ファイル sink。
 *  @return 成功 0 / 失敗 -1。
 *
 *  current_bytes は必ず 0 に設定されます。
 */
static int open_file_truncate(cplat_trace_file_sink *p)
{
    int flags = base_open_flags() | CPLAT_FILE_OPEN_TRUNCATE;

    if ((p->flags & CPLAT_TRACE_FILE_SINK_OS_BUFFERED) == 0)
    {
        flags |= CPLAT_FILE_OPEN_WRITE_THROUGH;
    }

    p->current_bytes = 0;

    return cplat_file_open(&p->file, p->path, flags, NULL);
}

/**
 *  @brief  オープン中のファイルが path の現在の実体を指しているか判定する (共有モード用) です。
 *  @return 一致 1 / 不一致または判定不能 0。
 *
 *  他プロセスのローテーションで path がリネームされると、自ハンドルは旧世代を指したままになる。\n
 *  パスの現在の同一性とオープン時にキャッシュした self_id を比較して検出します。
 */
static int sink_points_to_current_file(const cplat_trace_file_sink *p)
{
    cplat_file_id path_id;

    if (p->self_id_valid == 0)
    {
        return 0;
    }

    if (cplat_file_get_path_id(p->path, &path_id, NULL) != CPLAT_OK)
    {
        return 0;
    }

    if (path_id.volume == p->self_id.volume && path_id.index == p->self_id.index)
    {
        return 1;
    }

    return 0;
}

/**
 *  @brief  開いているファイルを閉じます。未開の場合は何もしません (べき等です)。
 */
static void close_file(cplat_trace_file_sink *p)
{
    (void)cplat_file_close(&p->file, NULL);
}

/**
 *  @brief  トレース ファイルをローテーションします。
 *
 *  ローカル mutex 保持中から呼び出される。ファイルの再オープンは 1 回だけ試行する。\n
 *  リネームに失敗した場合はその世代でカスケードを打ち切り、
 *  呼び出し元をブロックせずに続行する (ベスト エフォート)。
 */
static void rotate_file(cplat_trace_file_sink *p)
{
    /* パス構築用スタック バッファー */
    char old_path[PLATFORM_PATH_MAX];
    char new_path[PLATFORM_PATH_MAX];
    int gen;
    cplat_error detail;

    close_file(p);

    /* 最老世代のファイルを削除する (失敗は無視) */
    if (cplat_snprintf(new_path, sizeof(new_path), "%s.%d", p->path, p->generations) == CPLAT_OK)
    {
        (void)cplat_remove(new_path, NULL);
    }

    /* path.(gen-1) → path.gen のカスケード リネーム */
    for (gen = p->generations; gen >= 1; gen--)
    {
        /* 移動先: path.gen */
        if (cplat_snprintf(new_path, sizeof(new_path), "%s.%d", p->path, gen) != CPLAT_OK)
        {
            break;
        }

        /* 移動元: gen==1 のときは path そのもの */
        if (gen == 1)
        {
            /* old_path に path をコピー */
            if (cplat_snprintf(old_path, sizeof(old_path), "%s", p->path) != CPLAT_OK)
            {
                break;
            }
        }
        else
        {
            if (cplat_snprintf(old_path, sizeof(old_path), "%s.%d", p->path, gen - 1) != CPLAT_OK)
            {
                break;
            }
        }

        cplat_error_clear(&detail);
        if (cplat_rename(old_path, new_path, &detail) != 0)
        {
            if (detail.result == CPLAT_ERR_NOT_FOUND)
            {
                continue;
            }

            /* リネーム失敗: カスケードをここで打ち切る */
            break;
        }
    }

    /* 新規ファイルを作成して開く (失敗しても未オープンのまま続行) */
    if (p->shared != 0)
    {
        /* 共有モード: リネーム直後に他プロセスの writer が path を再作成して
           書き込んでいる場合があるため、切り詰めずに追記モードで開く。 */
        open_file(p);
    }
    else
    {
        open_file_truncate(p);
    }
}

/**
 *  @brief  共有モードのローテーション判定と実行を行います。
 *
 *  ローカル mutex 保持中、書き込み成功直後に呼び出される。\n
 *  全プロセス合計の実サイズ (ハンドル基準) が max_bytes 未満なら何もしない。\n
 *  しきい値以上の場合はプロセス間ロックの取得を非ブロッキングで試行し、ロック下で同一性と実サイズを
 *  再確認してからローテーションする。他プロセスがローテーション済みの場合は
 *  再度開くだけにとどめる。\n
 *  プロセス間ロックの取得に失敗した場合はローテーションを見送る
 *  (次回書き込み時に再試行するため、肥大化は一時的に留まる)。
 */
static void check_rotate_shared(cplat_trace_file_sink *p)
{
    size_t real_bytes;

    /* インメモリ集計ではなく実サイズで判定する (複数 writer の合計を反映) */
    if (cplat_file_get_size(&p->file, &real_bytes, NULL) != CPLAT_OK)
    {
        return;
    }
    if (real_bytes < p->max_bytes)
    {
        return;
    }

    if (cplat_interprocess_lock_try_lock(p->rotate_lock) != CPLAT_OK)
    {
        return;
    }

    if (sink_points_to_current_file(p) == 0)
    {
        /* 他プロセスが先にローテーション済み: 開き直すだけにする */
        close_file(p);
        open_file(p);
    }
    else
    {
        /* ロック下で実サイズを再確認してからローテーションする */
        if (cplat_file_get_size(&p->file, &real_bytes, NULL) == CPLAT_OK && real_bytes >= p->max_bytes)
        {
            rotate_file(p);
        }
    }

    cplat_interprocess_lock_unlock(p->rotate_lock);
}

/**
 *  @brief  ハンドルが保持する資源を解放します。
 *
 *  create 失敗時と dispose 系の共通処理。\n
 *  未確保 (NULL) のメンバーは何もしません。
 */
static void free_sink(cplat_trace_file_sink *p)
{
    close_file(p);

    if (p->rotate_lock != NULL)
    {
        cplat_interprocess_lock_dispose(p->rotate_lock);
        p->rotate_lock = NULL;
    }

    if (p->mutex_initialized != 0)
    {
        cplat_local_lock_dispose(p->mutex);
        p->mutex_initialized = 0;
    }

    cplat_free(p->lock_path);
    cplat_free(p->path);
    cplat_free(p);
}

/**
 *  @brief          新規 sink を生成してファイルを開く (レジストリ登録は行わない) です。
 *  @param[in]      path         出力ファイル パス。
 *  @param[in]      path_len     path のバイト数。
 *  @param[in]      max_bytes    1 ファイルあたりの最大バイト数。0 で既定値を使用。
 *  @param[in]      generations  保持する旧世代数。0 以下で既定値を使用。
 *  @param[in]      flags        動作フラグ。
 *  @return         成功時: ハンドル。失敗時: NULL。
 */
static cplat_trace_file_sink *create_new_sink(const char *path, const size_t path_len, const size_t max_bytes,
                                              const int generations, const int flags)
{
    cplat_trace_file_sink *handle;
    char dir[PLATFORM_PATH_MAX];

    handle = (cplat_trace_file_sink *)cplat_malloc(sizeof(cplat_trace_file_sink));
    if (handle == NULL)
    {
        return NULL;
    }

    /* 失敗パスで free_sink を使えるよう、先にすべてのメンバーを安全な値にする */
    handle->path = NULL;
    handle->lock_path = NULL;
    handle->mutex = NULL;
    handle->rotate_lock = NULL;
    handle->mutex_initialized = 0;
    handle->self_id_valid = 0;
    handle->current_bytes = 0;
    handle->flags = flags;
    cplat_file_init(&handle->file);

    handle->shared = 0;
    if ((flags & CPLAT_TRACE_FILE_SINK_SHARED) != 0)
    {
        handle->shared = 1;
    }

    if (max_bytes > 0)
    {
        handle->max_bytes = max_bytes;
    }
    else
    {
        handle->max_bytes = CPLAT_TRACE_FILE_SINK_DEFAULT_MAX_BYTES;
    }
    if (generations > 0)
    {
        handle->generations = generations;
    }
    else
    {
        handle->generations = CPLAT_TRACE_FILE_SINK_DEFAULT_GENERATIONS;
    }

    /* パス文字列をヒープへ複製する */
    handle->path = (char *)cplat_malloc(path_len + 1);
    if (handle->path == NULL)
    {
        free_sink(handle);
        return NULL;
    }
    memcpy(handle->path, path, path_len + 1);

    /* 同期プリミティブを初期化する */
    if (cplat_local_lock_create(&handle->mutex) != CPLAT_OK)
    {
        free_sink(handle);
        return NULL;
    }
    handle->mutex_initialized = 1;

    /* 親ディレクトリの確認と生成は初回だけ行う。ローテーション後の再オープンでは繰り返さない。 */
    if (cplat_path_dirname(dir, sizeof(dir), NULL, handle->path) == CPLAT_OK && strcmp(dir, ".") != 0)
    {
        (void)cplat_makedirs(dir, NULL);
    }

    /* ファイルを開く; 失敗したらリソースを解放して NULL を返す */
    if (open_file_with_retry(handle) != 0)
    {
        free_sink(handle);
        return NULL;
    }

    /* 共有モード: ローテーション排他用のプロセス間ロックを開く */
    if (handle->shared != 0)
    {
        handle->lock_path = (char *)cplat_malloc(path_len + sizeof(TRACE_FILE_LOCK_SUFFIX));
        if (handle->lock_path == NULL)
        {
            free_sink(handle);
            return NULL;
        }
        (void)cplat_snprintf(handle->lock_path, path_len + sizeof(TRACE_FILE_LOCK_SUFFIX), "%s%s", path,
                             TRACE_FILE_LOCK_SUFFIX);

        if (cplat_interprocess_lock_open(handle->lock_path, &handle->rotate_lock) != CPLAT_OK)
        {
            handle->rotate_lock = NULL;
            free_sink(handle);
            return NULL;
        }
    }

    return handle;
}

/* ===== 公開 API ===== */

/* Doxygen コメントは、ヘッダーに記載 */

cplat_trace_file_sink *cplat_trace_file_sink_create(const char *path, const size_t max_bytes, const int generations,
                                                    const int flags)
{
    cplat_trace_file_sink *handle;
    struct sink_registry_entry *entry;
    size_t path_len;
    int requested_shared = 0;
    char *key;

    if (path == NULL || flags < 0)
    {
        return NULL;
    }

    path_len = strlen(path);

    /* パスが長すぎてローテーションやロック ファイルのサフィックスを付加できない場合は拒否する */
    if (path_len + TRACE_FILE_SUFFIX_MAX >= (size_t)PLATFORM_PATH_MAX)
    {
        return NULL;
    }

    if ((flags & CPLAT_TRACE_FILE_SINK_SHARED) != 0)
    {
        requested_shared = 1;
    }

    key = build_registry_key(path);
    if (key == NULL)
    {
        return NULL;
    }

    /* プロセス内で同一パスの sink を共有する (占有モードのプロセス内調停)。
     * 生成自体もロック内で行い、同一パスの並行生成を直列化する。 */
    sink_registry_lock();

    entry = sink_registry_find_by_key_locked(key);
    if (entry != NULL)
    {
        /* 占有モードと共有モードの混在は意味的に矛盾するため拒否する */
        if (entry->sink->shared != requested_shared)
        {
            sink_registry_unlock();
            cplat_free(key);
            return NULL;
        }
        entry->refcount++;
        handle = entry->sink;
        sink_registry_unlock();
        cplat_free(key);
        return handle;
    }

    handle = create_new_sink(path, path_len, max_bytes, generations, flags);
    if (handle == NULL)
    {
        sink_registry_unlock();
        cplat_free(key);
        return NULL;
    }

    if (sink_registry_register_locked(key, handle) != 0)
    {
        sink_registry_unlock();
        free_sink(handle);
        cplat_free(key);
        return NULL;
    }

    sink_registry_unlock();
    return handle;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_trace_file_sink_write_text(cplat_trace_file_sink *handle, const int level,
                                              const cplat_timespec *timestamp, const char *timestamp_text,
                                              const char *message)
{
    char buf[TRACE_FILE_LINE_BUF];
    int len;
    int ret;

    (void)timestamp;
    if (handle == NULL || message == NULL)
    {
        return CPLAT_OK;
    }
    if (timestamp_text == NULL)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    /* 1 行全体をスタック バッファーへフォーマットする (syscall 回数を最小化) */
    len =
        snprintf(buf, sizeof(buf), "%s %c %s\n", timestamp_text,
                 cplat_internal_trace_level_char((cplat_trace_level)level), message); /* 置換対象外: 意図的な切り詰め */
    if (len <= 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }
    if (len >= (int)sizeof(buf))
    {
        /* 切り詰め: バッファー末尾を必ず改行で終端する */
        len = (int)sizeof(buf) - 1;
        buf[len - 1] = '\n';
    }

    /* ロック取得 (タイムアウト付き) */
    if (cplat_local_lock_lock(handle->mutex, FILE_LOCK_TIMEOUT_MS) != CPLAT_OK)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    /* 共有モード: 他プロセスのローテーションで path の実体が入れ替わっていたら開き直す */
    if (handle->shared != 0)
    {
        if (sink_points_to_current_file(handle) == 0)
        {
            close_file(handle);
            if (open_file(handle) != 0)
            {
                cplat_local_lock_unlock(handle->mutex);
                return CPLAT_ERR_UNKNOWN;
            }
        }
    }

    /* ファイルへ書き込む (FILE_FLAG_WRITE_THROUGH / O_DSYNC により自動フラッシュ) */
    ret = cplat_file_write(&handle->file, buf, (size_t)len, NULL);

    /* 書き込み成功時: サイズを追跡しローテーションしきい値を確認する */
    if (ret == 0)
    {
        if (handle->shared != 0)
        {
            check_rotate_shared(handle);
        }
        else
        {
            /* size_t のオーバーフローを防ぐ (実用上は発生しないが防御的に扱う) */
            if (handle->current_bytes <= (size_t)-1 - (size_t)len)
            {
                handle->current_bytes += (size_t)len;
            }

            if (handle->current_bytes >= handle->max_bytes)
            {
                rotate_file(handle);
            }
        }
    }

    /* ロック解放 */
    cplat_local_lock_unlock(handle->mutex);

    if (ret != 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }
    else
    {
        return CPLAT_OK;
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_trace_file_sink_write(cplat_trace_file_sink *handle, const int level, const cplat_timespec *timestamp,
                                const char *message)
{
    char timestamp_text[TRACE_FILE_TS_LEN + 1];
    cplat_timespec resolved;
    int fallback_used = 0;
    int ret;

    if (handle == NULL || message == NULL)
    {
        return CPLAT_OK;
    }

    if (cplat_internal_trace_resolve_timestamp(timestamp, &resolved, &fallback_used) != 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }
    if (cplat_internal_trace_format_local_timestamp(timestamp_text, sizeof(timestamp_text), &resolved) != 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    ret = cplat_internal_trace_file_sink_write_text(handle, level, timestamp, timestamp_text, message);
    if (ret != CPLAT_OK || fallback_used != 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_trace_file_sink_dispose(cplat_trace_file_sink *handle)
{
    struct sink_registry_entry *entry;
    char *key_to_free = NULL;
    int should_free = 1;

    if (handle == NULL)
    {
        return;
    }

    sink_registry_lock();
    entry = sink_registry_find_by_sink_locked(handle);
    if (entry != NULL)
    {
        entry->refcount--;
        if (entry->refcount > 0)
        {
            should_free = 0;
        }
        else
        {
            key_to_free = entry->key;
            sink_registry_remove_locked(entry);
        }
    }
    sink_registry_unlock();

    cplat_free(key_to_free);
    if (should_free)
    {
        free_sink(handle);
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_trace_file_sink_dispose_on_shutdown(cplat_trace_file_sink *handle)
{
    struct sink_registry_entry *entry;

    if (handle == NULL)
    {
        return;
    }

    /* shutdown 経路ではロックを取得しない (呼び出し側がスレッドの静止を保証する) */
    entry = sink_registry_find_by_sink_locked(handle);
    if (entry != NULL)
    {
        entry->refcount--;
        if (entry->refcount > 0)
        {
            return;
        }
        cplat_free(entry->key);
        sink_registry_remove_locked(entry);
    }

    free_sink(handle);
}
