/**
 *******************************************************************************
 *  @file           tracer.c
 *  @brief          トレース プロバイダーを管理する機能を実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/04/03
 *  @version        1.0.0
 *
 *  cplat_tracer_create / cplat_tracer_dispose / cplat_tracer_start / cplat_tracer_stop / cplat_tracer_write など
 *  公開 API の実装と、内部レジストリ管理機能を提供します。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */
#include <cplat/base/result.h>
#include <cplat/crt/string.h>
#include <cplat/crt/stdlib.h>
#include <cplat/clock/clock.h>
#include <cplat/crt/path.h>
#include <cplat/crt/stdio.h>
#include <cplat/runtime/process.h>
#include <cplat/sync/sync.h>
#include <cplat/trace/tracer.h>
#include <cplat/trace/trace_file.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <inttypes.h>

#include <cplat/trace/trace_common.h>
#include <cplat/trace/tracer_internal.h>
#include <cplat/trace/backends/file/trace_file_internal.h>

#if defined(PLATFORM_LINUX)
    #include <cplat/trace/backends/syslog/syslog_internal.h>
#elif defined(PLATFORM_WINDOWS)
    #include <cplat/trace/backends/etw/etw_internal.h>
    #include <cplat/trace/backends/eventlog/eventlog_internal.h>
#endif /* PLATFORM_ */

/* ===== Windows: TraceLogging プロバイダー定義 ===== */

#if defined(PLATFORM_WINDOWS)

    #include <cplat/base/windows_sdk.h>
    #include <TraceLoggingProvider.h>

TRACELOGGING_DEFINE_PROVIDER(s_trace_provider_ref, CPLAT_TRACER_DEFAULT_PROVIDER_NAME,
                             CPLAT_TRACER_DEFAULT_PROVIDER_GUID);

static size_t s_trace_ref = 0;
static cplat_etw_provider *s_etw_handle = NULL;
static cplat_eventlog_sink *s_eventlog_handle = NULL;
static cplat_local_lock *s_registry_lock;
static cplat_once_flag s_registry_lock_once = {0};

#elif defined(PLATFORM_LINUX)

    #include <syslog.h>
    #include <unistd.h>

static cplat_local_lock *s_registry_lock;
static cplat_once_flag s_registry_lock_once = {0};

#endif /* PLATFORM_ */

/** プロセス名取得失敗時のフォールバック名。 */
#define FALLBACK_NAME "unknown"

/** registry の初期容量。 */
#define TRACE_REGISTRY_INITIAL_CAPACITY 8

enum trace_handle_state
{
    TRACE_HANDLE_ACTIVE = 0,
    TRACE_HANDLE_DISPOSING,
    TRACE_HANDLE_DISPOSED
};

/**
 *  @brief  トレース フック エントリ構造体 (内部定義) です。
 */
struct cplat_tracer_hook_entry
{
    cplat_tracer_hook_fn fn;
    void *context;
    struct cplat_tracer_hook_entry *next;
};

/**
 *  @brief  トレース プロバイダー ハンドル構造体 (内部定義) です。
 */
struct cplat_tracer
{
    int64_t identifier;

#if defined(PLATFORM_LINUX)
    cplat_syslog_sink *syslog_handle;
    char *effective_name;
#elif defined(PLATFORM_WINDOWS)
    char *service_name;
    char *eventlog_instance_name;
#endif /* PLATFORM_ */

    cplat_trace_file_sink *file_handle;
    char *file_path;
    char *file_name;
    int64_t file_identifier;
    size_t file_max_bytes;
    int file_generations;
    int file_flags;

    cplat_local_rwlock *config_rwlock;
    cplat_tracer_concurrency_mode concurrency_mode;

    cplat_trace_level os_level;
#if defined(PLATFORM_WINDOWS)
    cplat_trace_level etw_level;
#endif /* PLATFORM_WINDOWS */
    cplat_trace_level file_level;
    cplat_trace_level stderr_level;
    int running;
    int lifecycle_state;

    cplat_tracer_hook_entry *hook_head;
};

struct trace_registry
{
    cplat_tracer **items;
    size_t count;
    size_t capacity;
    size_t shutdown_started;
};

static struct trace_registry s_trace_registry = {0};
static cplat_once_flag s_trace_shutdown_once = {0};
static int s_registry_lock_init_result = CPLAT_ERR_UNKNOWN;
static int s_shutdown_registration_result = CPLAT_ERR_UNKNOWN;

static void trace_shutdown_callback(const cplat_shutdown_event *event, void *context);

static void init_registry_lock(void)
{
    s_registry_lock_init_result = cplat_local_lock_create(&s_registry_lock);
}

static void register_trace_shutdown_callback(void)
{
    s_shutdown_registration_result = cplat_shutdown_register(trace_shutdown_callback, NULL);
}

static void trace_shutdown_callback(const cplat_shutdown_event *event, void *context)
{
    (void)context;
    cplat_internal_trace_registry_dispose_all_on_shutdown(event);
}

/**
 *  @brief          レジストリの排他ロックを取得します。
 */
static int registry_lock(void)
{
    cplat_call_once(&s_registry_lock_once, init_registry_lock);
    if (s_registry_lock_init_result != CPLAT_OK)
    {
        return -1;
    }
    if (cplat_local_lock_lock(s_registry_lock, CPLAT_SYNC_WAIT_FOREVER) != CPLAT_OK)
    {
        return -1;
    }
    return 0;
}

/**
 *  @brief          レジストリの排他ロックを解放します。
 */
static void registry_unlock(void)
{
    (void)cplat_local_lock_unlock(s_registry_lock);
}

/**
 *  @brief          レジストリを拡張する (ロック保持中) です。
 *  @return         成功時 0、メモリ確保失敗時 -1。
 */
static int registry_expand_locked(void)
{
    cplat_tracer **new_items;
    size_t new_capacity;

    if (s_trace_registry.capacity == 0)
    {
        new_capacity = TRACE_REGISTRY_INITIAL_CAPACITY;
    }
    else
    {
        if (s_trace_registry.capacity > SIZE_MAX / 2)
        {
            return -1;
        }
        new_capacity = s_trace_registry.capacity * 2;
    }

    if (new_capacity > SIZE_MAX / sizeof(cplat_tracer *))
    {
        return -1;
    }

    new_items = (cplat_tracer **)cplat_realloc(s_trace_registry.items, new_capacity, sizeof(cplat_tracer *));
    if (new_items == NULL)
    {
        return -1;
    }

    s_trace_registry.items = new_items;
    s_trace_registry.capacity = new_capacity;
    return 0;
}

#if defined(PLATFORM_WINDOWS)
static int windows_backend_acquire_locked(void)
{
    if (s_trace_ref == 0)
    {
        s_etw_handle = cplat_etw_provider_create(s_trace_provider_ref);
        if (s_etw_handle == NULL)
        {
            return -1;
        }
        s_eventlog_handle = cplat_eventlog_sink_create(CPLAT_TRACER_DEFAULT_PROVIDER_NAME);
    }
    s_trace_ref++;
    return 0;
}

static void windows_backend_release_locked(void)
{
    if (s_trace_ref == 0)
    {
        return;
    }
    s_trace_ref--;
    if (s_trace_ref == 0)
    {
        cplat_etw_provider_dispose(s_etw_handle);
        s_etw_handle = NULL;
        cplat_eventlog_sink_dispose(s_eventlog_handle);
        s_eventlog_handle = NULL;
    }
}
#endif /* PLATFORM_WINDOWS */

/**
 *  @brief          ハンドルをレジストリに登録します。
 *  @param[in]      handle  登録するトレース プロバイダー ハンドル。
 *  @return         成功時 0、シャットダウン中またはメモリ不足時 -1。
 */
static int registry_register_handle(cplat_tracer *handle)
{
    int rc = 0;

    if (registry_lock() != 0)
    {
        return -1;
    }

    if (s_trace_registry.shutdown_started)
    {
        rc = -1;
    }
    else
    {
        if (s_trace_registry.count == s_trace_registry.capacity)
        {
            rc = registry_expand_locked();
        }
        if (rc == 0)
        {
#if defined(PLATFORM_WINDOWS)
            rc = windows_backend_acquire_locked();
#endif /* PLATFORM_WINDOWS */
        }
        if (rc == 0)
        {
            s_trace_registry.items[s_trace_registry.count++] = handle;
        }
    }

    registry_unlock();
    return rc;
}

/**
 *  @brief          ハンドルをレジストリから削除します。
 *  @param[in]      handle  削除するトレース プロバイダー ハンドル。
 */
static int registry_unregister_handle(cplat_tracer *handle)
{
    size_t i;
    int found = 0;

    if (registry_lock() != 0)
    {
        return -1;
    }

    for (i = 0; i < s_trace_registry.count; i++)
    {
        if (s_trace_registry.items[i] == handle)
        {
            s_trace_registry.count--;
            s_trace_registry.items[i] = s_trace_registry.items[s_trace_registry.count];
            s_trace_registry.items[s_trace_registry.count] = NULL;
#if defined(PLATFORM_WINDOWS)
            windows_backend_release_locked();
#endif /* PLATFORM_WINDOWS */
            found = 1;
            break;
        }
    }

    registry_unlock();
    if (found)
    {
        return 0;
    }
    return -1;
}

/* Doxygen コメントは、ヘッダーに記載 */

size_t cplat_internal_trace_registry_count(void)
{
    size_t count;

    if (registry_lock() != 0)
    {
        return 0;
    }
    count = s_trace_registry.count;
    registry_unlock();
    return count;
}

/* Doxygen コメントは、ヘッダーに記載 */

size_t cplat_internal_trace_registry_capacity(void)
{
    size_t capacity;

    if (registry_lock() != 0)
    {
        return 0;
    }
    capacity = s_trace_registry.capacity;
    registry_unlock();
    return capacity;
}

/**
 *  @brief          ハンドルがアクティブか判定します。
 *  @param[in]      handle  判定対象のトレース プロバイダー ハンドル。
 *  @return         アクティブの場合 1、それ以外 0。
 */
static int handle_is_active(const cplat_tracer *handle)
{
    return handle != NULL && handle->lifecycle_state == TRACE_HANDLE_ACTIVE && !s_trace_registry.shutdown_started;
}

/**
 *  @brief          解放処理を開始します。
 *  @param[in]      handle  解放対象のトレース プロバイダー ハンドル。
 *  @return         成功時 0、ハンドルが NULL またはアクティブでない場合 -1。
 */
static int begin_dispose(cplat_tracer *handle)
{
    if (handle == NULL || handle->lifecycle_state != TRACE_HANDLE_ACTIVE)
    {
        return -1;
    }

    handle->lifecycle_state = TRACE_HANDLE_DISPOSING;
    return 0;
}

#if defined(PLATFORM_LINUX)

/**
 *  @brief          トレース レベルを syslog レベルに変換します。
 *  @param[in]      lv  変換元のトレース レベル。
 *  @return         対応する syslog レベル値。
 */
static int to_syslog_level(const cplat_trace_level lv)
{
    switch (lv)
    {
    case CPLAT_TRACE_LEVEL_FORCE_CRITICAL:
    case CPLAT_TRACE_LEVEL_CRITICAL:
        return LOG_CRIT;
    case CPLAT_TRACE_LEVEL_FORCE_ERROR:
    case CPLAT_TRACE_LEVEL_ERROR:
        return LOG_ERR;
    case CPLAT_TRACE_LEVEL_FORCE_WARNING:
    case CPLAT_TRACE_LEVEL_WARNING:
        return LOG_WARNING;
    case CPLAT_TRACE_LEVEL_INFO:
        return LOG_INFO;
    /* 強制出力は syslog 側の既定の設定で捨てられないよう、最下位から 1 段引き上げる */
    case CPLAT_TRACE_LEVEL_FORCE_INFO:
    case CPLAT_TRACE_LEVEL_FORCE_VERBOSE:
    case CPLAT_TRACE_LEVEL_FORCE_DEBUG:
    case CPLAT_TRACE_LEVEL_FORCE_NONE:
        return LOG_INFO;
    case CPLAT_TRACE_LEVEL_VERBOSE:
        return LOG_DEBUG;
    case CPLAT_TRACE_LEVEL_DEBUG:
        return LOG_DEBUG;
    case CPLAT_TRACE_LEVEL_NONE:
    default:
        return LOG_DEBUG;
    }
}

#elif defined(PLATFORM_WINDOWS)

/**
 *  @brief          トレース レベルを ETW レベルに変換します。
 *  @param[in]      lv  変換元のトレース レベル。
 *  @return         対応する ETW レベル値。
 */
static int to_etw_level(const cplat_trace_level lv)
{
    switch (lv)
    {
    case CPLAT_TRACE_LEVEL_FORCE_CRITICAL:
    case CPLAT_TRACE_LEVEL_CRITICAL:
        return 1;
    case CPLAT_TRACE_LEVEL_FORCE_ERROR:
    case CPLAT_TRACE_LEVEL_ERROR:
        return 2;
    case CPLAT_TRACE_LEVEL_FORCE_WARNING:
    case CPLAT_TRACE_LEVEL_WARNING:
        return 3;
    case CPLAT_TRACE_LEVEL_INFO:
        return 4;
    /* 強制出力は ETW セッション側の既定の設定で捨てられないよう、最下位から 1 段引き上げる */
    case CPLAT_TRACE_LEVEL_FORCE_INFO:
    case CPLAT_TRACE_LEVEL_FORCE_VERBOSE:
    case CPLAT_TRACE_LEVEL_FORCE_DEBUG:
    case CPLAT_TRACE_LEVEL_FORCE_NONE:
        return 4;
    case CPLAT_TRACE_LEVEL_VERBOSE:
        return 5;
    case CPLAT_TRACE_LEVEL_DEBUG:
        return 5;
    default:
        return 5;
    }
}

#endif /* PLATFORM_ */

/**
 *  @brief          プロセスの実行ファイル パスからベース名を取得します。
 *  @param[in,out]  buf       パス文字列を格納するバッファー。
 *  @param[in]      buf_size  バッファーのバイト数。
 *  @return         ベース名へのポインター。失敗時は FALLBACK_NAME。
 *
 *  cplat_process_get_executable_path() で取得したパスは UTF-8 で
 *  セパレーターが '/' に統一されるため、プラットフォーム非依存で処理できます。
 */
static const char *get_process_basename(char *buf, const size_t buf_size)
{
    if (cplat_process_get_executable_path(buf, buf_size) != CPLAT_OK)
    {
        return FALLBACK_NAME;
    }

    return cplat_path_basename(buf);
}

/**
 *  @brief          設定の排他ロック (書き込みロック) を取得します。
 *  @param[in]      handle  ロック対象のトレース プロバイダー ハンドル。
 */
static int config_lock_exclusive(cplat_tracer *handle)
{
    if (handle->concurrency_mode == CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED)
    {
        return CPLAT_OK;
    }
    return cplat_local_rwlock_lock_exclusive(handle->config_rwlock, CPLAT_SYNC_WAIT_FOREVER);
}

/**
 *  @brief          設定の排他ロック (書き込みロック) を解放します。
 *  @param[in]      handle  ロック解放対象のトレース プロバイダー ハンドル。
 */
static void config_unlock_exclusive(cplat_tracer *handle)
{
    if (handle->concurrency_mode == CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED)
    {
        (void)cplat_local_rwlock_unlock_exclusive(handle->config_rwlock);
    }
}

#define LOCK_TIMEOUT_MS 100

/**
 *  @brief          タイムアウト付きで設定の共有ロック (読み取りロック) を取得します。
 *  @param[in]      handle  ロック対象のトレース プロバイダー ハンドル。
 *  @return         成功時 0、タイムアウト時 -1。
 */
static int config_lock_shared_timed(cplat_tracer *handle)
{
    if (handle->concurrency_mode == CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED)
    {
        return 0;
    }
    if (cplat_local_rwlock_lock_shared(handle->config_rwlock, LOCK_TIMEOUT_MS) == CPLAT_OK)
    {
        return 0;
    }
    else
    {
        return -1;
    }
}

/**
 *  @brief          設定の共有ロック (読み取りロック) を解放します。
 *  @param[in]      handle  ロック解放対象のトレース プロバイダー ハンドル。
 */
static void config_unlock_shared(cplat_tracer *handle)
{
    if (handle->concurrency_mode == CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED)
    {
        (void)cplat_local_rwlock_unlock_shared(handle->config_rwlock);
    }
}

/**
 *  @brief          アクティブ検証と設定の共有ロック取得をまとめて行います。
 *  @param[in]      handle  対象のトレース プロバイダー ハンドル。
 *  @return         成功時 0、失敗時 -1。
 *
 *  ロック取得前後の 2 回、ハンドルがアクティブであることを検証します
 *  (ロック待機中に dispose が進行した場合を検出するため)。\n
 *  成功時は共有ロックを保持したまま戻るため、呼び出し元は
 *  config_unlock_shared() で解放すること。失敗時はロックを保持しません。
 */
static int tracer_enter_shared(cplat_tracer *handle)
{
    if (!handle_is_active(handle))
    {
        return -1;
    }
    if (config_lock_shared_timed(handle) != 0)
    {
        return -1;
    }
    if (handle->lifecycle_state != TRACE_HANDLE_ACTIVE)
    {
        config_unlock_shared(handle);
        return -1;
    }
    return 0;
}

/**
 *  @brief          started 状態を要求する共有ロック取得を行います。
 *  @param[in]      handle  対象のトレース プロバイダー ハンドル。
 *  @return         成功時 0、失敗時 (非アクティブまたは stopped) -1。
 *
 *  tracer_enter_shared() の検証に加えて running であることを要求します。
 *  成功時は共有ロックを保持したまま戻ります。失敗時はロックを保持しません。
 */
static int tracer_enter_shared_running(cplat_tracer *handle)
{
    if (tracer_enter_shared(handle) != 0)
    {
        return -1;
    }
    if (!handle->running)
    {
        config_unlock_shared(handle);
        return -1;
    }
    return 0;
}

/**
 *  @brief          アクティブ検証と設定の排他ロック取得をまとめて行います。
 *  @param[in]      handle  対象のトレース プロバイダー ハンドル。
 *  @return         成功時 0、失敗時 -1。
 *
 *  ロック取得前後の 2 回、ハンドルがアクティブであることを検証します
 *  (ロック待機中に dispose が進行した場合を検出するため)。\n
 *  成功時は排他ロックを保持したまま戻るため、呼び出し元は
 *  config_unlock_exclusive() で解放すること。失敗時はロックを保持しません。
 */
static int tracer_enter_exclusive(cplat_tracer *handle)
{
    if (!handle_is_active(handle))
    {
        return -1;
    }
    if (config_lock_exclusive(handle) != CPLAT_OK)
    {
        return -1;
    }
    if (handle->lifecycle_state != TRACE_HANDLE_ACTIVE)
    {
        config_unlock_exclusive(handle);
        return -1;
    }
    return 0;
}

/**
 *  @brief          stopped 状態を要求する排他ロック取得を行います。
 *  @param[in]      handle  対象のトレース プロバイダー ハンドル。
 *  @return         成功時 0、失敗時 (非アクティブまたは started) -1。
 *
 *  tracer_enter_exclusive() の検証に加えて stopped であることを要求します
 *  (started 中の変更を許可しない設定 API 向け)。\n
 *  成功時は排他ロックを保持したまま戻ります。失敗時はロックを保持しません。
 */
static int tracer_enter_exclusive_stopped(cplat_tracer *handle)
{
    if (tracer_enter_exclusive(handle) != 0)
    {
        return -1;
    }
    if (handle->running)
    {
        config_unlock_exclusive(handle);
        return -1;
    }
    return 0;
}

/**
 *  @brief          識別子を付加した有効名の文字列を構築します。
 *  @param[in]      name        サービス名 (NULL の場合はプロセス ベース名を使用)。
 *  @param[in]      identifier  識別子 (0 の場合はサフィックスなし)。
 *  @return         ヒープ確保された有効名文字列。呼び出し元が free すること。
 *                  失敗時は NULL。
 */
static char *build_effective_name(const char *name, const int64_t identifier)
{
    char path_buf[256];
    const char *base;
    char *result;

    if (name != NULL)
    {
        base = name;
    }
    else
    {
        base = get_process_basename(path_buf, sizeof(path_buf));
    }

    if (identifier == 0)
    {
        return cplat_strdup(base);
    }

    {
        int id_len;
        size_t base_len;
        size_t total;

        id_len = snprintf(NULL, 0, "%" PRId64, identifier); /* 置換対象外: 必要長の照会 */
        base_len = strlen(base);
        total = base_len + 1 + (size_t)id_len + 1;

        result = (char *)cplat_malloc(total);
        if (result == NULL)
        {
            return NULL;
        }
        if (cplat_snprintf(result, total, "%s_%" PRId64, base, identifier) != CPLAT_OK)
        {
            cplat_free(result);
            return NULL;
        }
        return result;
    }
}

#if defined(PLATFORM_WINDOWS)
/**
 *  @brief          EventLog に渡す元のインスタンス名を構築します。
 *  @param[in]      name  インスタンス名。NULL の場合はプロセス ベース名を使用。
 *  @return         ヒープ確保されたインスタンス名。呼び出し元が free すること。
 */
static char *build_eventlog_instance_name(const char *name)
{
    char path_buf[256];
    const char *base;

    if (name != NULL)
    {
        base = name;
    }
    else
    {
        base = get_process_basename(path_buf, sizeof(path_buf));
    }

    return cplat_strdup(base);
}
#endif /* PLATFORM_WINDOWS */

/**
 *  @brief          ハンドルが保持する有効名を取得します。
 *  @param[in]      handle  対象のトレース プロバイダー ハンドル。
 *  @return         有効名文字列へのポインター。
 */
static const char *tracer_effective_name(const cplat_tracer *handle)
{
#if defined(PLATFORM_LINUX)
    return handle->effective_name;
#elif defined(PLATFORM_WINDOWS)
    return handle->service_name;
#endif /* PLATFORM_ */
}

#if defined(PLATFORM_WINDOWS)
/**
 *  @brief          名前の末尾にある ".exe" を除去する (インプレース) です。
 *  @param[in,out]  name  対象の名前文字列。
 *
 *  Windows ではプロセス名 (実行ファイルのベース名) が ".exe" で終わるため、
 *  トレース ファイル名からは除去する。大文字小文字は区別しません。
 */
static void strip_exe_suffix(char *name)
{
    size_t name_len = strlen(name);

    if (name_len >= 4 && cplat_strncasecmp(&name[name_len - 4], ".exe", 4) == 0)
    {
        name[name_len - 4] = '\0';
    }
}
#endif /* PLATFORM_WINDOWS */

/**
 *  @brief          トレース ファイル名 (ファイル識別込み) を解決します。
 *  @param[in]      handle          対象のトレース プロバイダー ハンドル。
 *  @param[out]     file_name_out   解決した名前を格納するバッファー。
 *  @param[in]      file_name_size  バッファーのバイト数。
 *  @return         成功時 0、失敗時 (バッファー不足) -1。
 *
 *  ファイル名が未設定 (NULL) の場合はプロセス名 (実行ファイルのベース名) を使用します。
 *  Windows ではプロセス名末尾の ".exe" を除去する (明示設定された名前には適用しない)。\n
 *  ファイル識別が 0 以外の場合は "_{ファイル識別}" を付加します。
 */
static int resolve_file_name(const cplat_tracer *handle, char *file_name_out, const size_t file_name_size)
{
    char name_buf[256];
    int ret;

    if (handle->file_name != NULL)
    {
        ret = cplat_snprintf(name_buf, sizeof(name_buf), "%s", handle->file_name);
    }
    else
    {
        char path_buf[256];

        ret = cplat_snprintf(name_buf, sizeof(name_buf), "%s", get_process_basename(path_buf, sizeof(path_buf)));
#if defined(PLATFORM_WINDOWS)
        if (ret == CPLAT_OK)
        {
            strip_exe_suffix(name_buf);
        }
#endif /* PLATFORM_WINDOWS */
    }
    if (ret != CPLAT_OK)
    {
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    if (handle->file_identifier != 0)
    {
        ret = cplat_snprintf(file_name_out, file_name_size, "%s_%" PRId64, name_buf, handle->file_identifier);
    }
    else
    {
        ret = cplat_snprintf(file_name_out, file_name_size, "%s", name_buf);
    }
    if (ret != CPLAT_OK)
    {
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }
    return CPLAT_OK;
}

/**
 *  @brief          ファイル トレースの既定パスを構築します。
 *  @param[in]      handle     対象のトレース プロバイダー ハンドル。
 *  @param[out]     path_out   構築したパスを格納するバッファー。
 *  @param[in]      path_size  バッファーのバイト数。
 *  @return         成功時 0、失敗時 -1。
 *
 *  実行ファイルのディレクトリ配下の log/{ファイル名}.log を構築する
 *  (ファイル名は resolve_file_name で解決する)。\n
 *  実行ファイル パスの取得に失敗した場合はカレント ディレクトリ相対の
 *  log/{ファイル名}.log にフォールバックします。
 */
static int build_default_file_path(const cplat_tracer *handle, char *path_out, const size_t path_size)
{
    char exe_path[PLATFORM_PATH_MAX];
    char exe_dir[PLATFORM_PATH_MAX];
    char name_buf[280];
    char log_file_name[288];

    if (resolve_file_name(handle, name_buf, sizeof(name_buf)) != 0)
    {
        return -1;
    }

    if (cplat_snprintf(log_file_name, sizeof(log_file_name), "%s.log", name_buf) != CPLAT_OK)
    {
        return -1;
    }

    if (cplat_process_get_executable_path(exe_path, sizeof(exe_path)) == CPLAT_OK)
    {
        if (cplat_path_dirname(exe_dir, sizeof(exe_dir), NULL, exe_path) == CPLAT_OK)
        {
            if (strcmp(exe_dir, ".") != 0)
            {
                return cplat_path_join(path_out, path_size, NULL, exe_dir, "log", log_file_name);
            }
        }
    }

    return cplat_path_join(path_out, path_size, NULL, "log", log_file_name);
}

/**
 *  @brief          指定パラメーターでファイル トレース sink を開いて返します。
 *  @param[in]      handle       既定パス解決に用いるハンドル (file_name 等を参照)。
 *  @param[in]      path         出力ファイル パス。NULL の場合は既定パスを解決します。
 *  @param[in]      max_bytes    1 ファイルあたりの最大バイト数。
 *  @param[in]      generations  保持する旧世代数。
 *  @param[in]      flags        動作フラグ。
 *  @return         生成した sink。失敗時 NULL。
 *
 *  呼び出し側で config の排他ロックを保持していることを前提とします。
 *  cplat_tracer_start と cplat_tracer_set_file_level の双方から使用します。
 */
static cplat_trace_file_sink *open_file_sink_with(const cplat_tracer *handle, const char *path, const size_t max_bytes,
                                                  const int generations, const int flags)
{
    const char *eff_path = path;
    char default_path[PLATFORM_PATH_MAX];

    if (eff_path == NULL)
    {
        if (build_default_file_path(handle, default_path, sizeof(default_path)) == 0)
        {
            eff_path = default_path;
        }
    }
    if (eff_path == NULL)
    {
        return NULL;
    }
    return cplat_trace_file_sink_create(eff_path, max_bytes, generations, flags);
}

/**
 *  @brief          クリーンアップのためハンドルのトレース出力を停止します。
 *  @param[in]      handle  停止対象のトレース プロバイダー ハンドル。
 *  @return         常に 0。
 *
 *  ファイル トレースが開いていた場合はトレース ファイルを閉じます。
 *  ファイル トレースの設定は保持され、次回の start で改めてファイルを開きます。
 */
static int stop_handle_for_cleanup(cplat_tracer *handle)
{
    if (config_lock_exclusive(handle) != CPLAT_OK)
    {
        return CPLAT_ERR_UNKNOWN;
    }
    if (!handle->running)
    {
        config_unlock_exclusive(handle);
        return CPLAT_OK;
    }

    handle->running = 0;
    if (handle->file_handle != NULL)
    {
        cplat_trace_file_sink_dispose(handle->file_handle);
        handle->file_handle = NULL;
    }
    config_unlock_exclusive(handle);
    return CPLAT_OK;
}

/**
 *  @brief          通常のハンドル解放処理を行います。
 *  @param[in]      handle  解放対象のトレース プロバイダー ハンドル。
 */
static void trace_handle_release_normal(cplat_tracer *handle)
{
    cplat_tracer_hook_entry *hook;
    cplat_tracer_hook_entry *next;

    if (handle->file_handle != NULL)
    {
        cplat_trace_file_sink_dispose(handle->file_handle);
        handle->file_handle = NULL;
    }

    for (hook = handle->hook_head; hook != NULL; hook = next)
    {
        next = hook->next;
        cplat_free(hook);
    }
    handle->hook_head = NULL;

    cplat_free(handle->file_path);
    handle->file_path = NULL;
    cplat_free(handle->file_name);
    handle->file_name = NULL;

#if defined(PLATFORM_LINUX)
    cplat_syslog_sink_dispose(handle->syslog_handle);
    cplat_free(handle->effective_name);
    if (handle->concurrency_mode == CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED)
    {
        (void)cplat_local_rwlock_dispose(handle->config_rwlock);
    }
#elif defined(PLATFORM_WINDOWS)
    cplat_free(handle->eventlog_instance_name);
    cplat_free(handle->service_name);
    if (handle->concurrency_mode == CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED)
    {
        (void)cplat_local_rwlock_dispose(handle->config_rwlock);
    }
#endif /* PLATFORM_ */

    handle->lifecycle_state = TRACE_HANDLE_DISPOSED;
    cplat_free(handle);
}

/**
 *  @brief          アンロード時のハンドル解放処理を行います。
 *  @param[in]      handle  解放対象のトレース プロバイダー ハンドル。
 */
static void trace_handle_release_on_shutdown(cplat_tracer *handle)
{
    cplat_tracer_hook_entry *hook;
    cplat_tracer_hook_entry *next;

    if (handle->file_handle != NULL)
    {
        cplat_internal_trace_file_sink_dispose_on_shutdown(handle->file_handle);
        handle->file_handle = NULL;
    }

    cplat_free(handle->file_path);
    handle->file_path = NULL;
    cplat_free(handle->file_name);
    handle->file_name = NULL;

    for (hook = handle->hook_head; hook != NULL; hook = next)
    {
        next = hook->next;
        cplat_free(hook);
    }
    handle->hook_head = NULL;

#if defined(PLATFORM_LINUX)
    cplat_internal_syslog_sink_dispose_on_shutdown(handle->syslog_handle);
    cplat_free(handle->effective_name);
#elif defined(PLATFORM_WINDOWS)
    cplat_free(handle->eventlog_instance_name);
    cplat_free(handle->service_name);
#endif /* PLATFORM_ */

    if (handle->concurrency_mode == CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED)
    {
        (void)cplat_local_rwlock_dispose(handle->config_rwlock);
    }

    handle->lifecycle_state = TRACE_HANDLE_DISPOSED;
    cplat_free(handle);
}

/* Doxygen コメントは、ヘッダーに記載 */

cplat_tracer *cplat_tracer_create(const cplat_tracer_concurrency_mode concurrency_mode)
{
    cplat_tracer *handle;
    char path_buf[256];
    const char *effective_name;

    if (concurrency_mode != CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED &&
        concurrency_mode != CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED)
    {
        return NULL;
    }

    cplat_call_once(&s_trace_shutdown_once, register_trace_shutdown_callback);
    if (s_shutdown_registration_result != CPLAT_OK)
    {
        return NULL;
    }

    if (s_trace_registry.shutdown_started)
    {
        return NULL;
    }

    effective_name = get_process_basename(path_buf, sizeof(path_buf));

#if defined(PLATFORM_LINUX)
    {
        cplat_syslog_sink *sp;

        sp = cplat_syslog_sink_create(effective_name, LOG_USER);
        if (sp == NULL)
        {
            return NULL;
        }

        handle = (cplat_tracer *)cplat_malloc(sizeof(cplat_tracer));
        if (handle == NULL)
        {
            cplat_syslog_sink_dispose(sp);
            return NULL;
        }

        handle->identifier = 0;
        handle->syslog_handle = sp;
        handle->effective_name = cplat_strdup(effective_name);
        handle->os_level = CPLAT_TRACER_DEFAULT_OS_LEVEL;
        handle->file_level = CPLAT_TRACER_DEFAULT_FILE_LEVEL;
        handle->file_handle = NULL;
        handle->file_path = NULL;
        handle->file_name = NULL;
        handle->file_identifier = 0;
        handle->file_max_bytes = 0;
        handle->file_generations = 0;
        handle->file_flags = 0;
        handle->stderr_level = CPLAT_TRACER_DEFAULT_STDERR_LEVEL;
        handle->running = 0;
        handle->lifecycle_state = TRACE_HANDLE_ACTIVE;
        handle->config_rwlock = NULL;
        handle->concurrency_mode = concurrency_mode;
        handle->hook_head = NULL;

        if (handle->effective_name == NULL)
        {
            cplat_syslog_sink_dispose(sp);
            cplat_free(handle);
            return NULL;
        }

        if (concurrency_mode == CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED &&
            cplat_local_rwlock_create(&handle->config_rwlock) != CPLAT_OK)
        {
            cplat_syslog_sink_dispose(sp);
            cplat_free(handle->effective_name);
            cplat_free(handle);
            return NULL;
        }
    }
#elif defined(PLATFORM_WINDOWS)
    {
        char *svc;
        char *eventlog_name;

        svc = cplat_strdup(effective_name);
        if (svc == NULL)
        {
            return NULL;
        }
        eventlog_name = cplat_strdup(effective_name);
        if (eventlog_name == NULL)
        {
            cplat_free(svc);
            return NULL;
        }

        handle = (cplat_tracer *)cplat_malloc(sizeof(cplat_tracer));
        if (handle == NULL)
        {
            cplat_free(eventlog_name);
            cplat_free(svc);
            return NULL;
        }

        handle->identifier = 0;
        handle->service_name = svc;
        handle->eventlog_instance_name = eventlog_name;
        handle->os_level = CPLAT_TRACER_DEFAULT_OS_LEVEL;
        handle->etw_level = CPLAT_TRACER_DEFAULT_ETW_LEVEL;
        handle->file_level = CPLAT_TRACER_DEFAULT_FILE_LEVEL;
        handle->file_handle = NULL;
        handle->file_path = NULL;
        handle->file_name = NULL;
        handle->file_identifier = 0;
        handle->file_max_bytes = 0;
        handle->file_generations = 0;
        handle->file_flags = 0;
        handle->stderr_level = CPLAT_TRACER_DEFAULT_STDERR_LEVEL;
        handle->running = 0;
        handle->lifecycle_state = TRACE_HANDLE_ACTIVE;
        handle->config_rwlock = NULL;
        handle->concurrency_mode = concurrency_mode;
        handle->hook_head = NULL;

        if (concurrency_mode == CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED &&
            cplat_local_rwlock_create(&handle->config_rwlock) != CPLAT_OK)
        {
            cplat_free(handle->eventlog_instance_name);
            cplat_free(handle->service_name);
            cplat_free(handle);
            return NULL;
        }
    }
#endif /* PLATFORM_ */

    if (registry_register_handle(handle) != 0)
    {
#if defined(PLATFORM_LINUX)
        cplat_syslog_sink_dispose(handle->syslog_handle);
        cplat_free(handle->effective_name);
#elif defined(PLATFORM_WINDOWS)
        cplat_free(handle->eventlog_instance_name);
        cplat_free(handle->service_name);
#endif /* PLATFORM_ */
        if (concurrency_mode == CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED)
        {
            (void)cplat_local_rwlock_dispose(handle->config_rwlock);
        }
        cplat_free(handle);
        return NULL;
    }

    return handle;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_tracer_start(cplat_tracer *handle)
{
    int result = CPLAT_OK;

    if (tracer_enter_exclusive(handle) != 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }
    if (handle->running)
    {
        config_unlock_exclusive(handle);
        return CPLAT_OK;
    }

    /* ファイル トレースが有効な場合、この時点の設定 (パスと有効名) でトレース ファイルを開く。
     * 失敗しても started 状態へは遷移し、ファイル以外のトレース出力を継続する (best-effort)。 */
    if (handle->file_level != CPLAT_TRACE_LEVEL_NONE && handle->file_handle == NULL)
    {
        handle->file_handle = open_file_sink_with(handle, handle->file_path, handle->file_max_bytes,
                                                  handle->file_generations, handle->file_flags);
        if (handle->file_handle == NULL)
        {
            result = CPLAT_ERR_UNKNOWN;
        }
    }

    handle->running = 1;
    config_unlock_exclusive(handle);
    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_tracer_stop(cplat_tracer *handle)
{
    if (!handle_is_active(handle))
    {
        return CPLAT_ERR_UNKNOWN;
    }

    return stop_handle_for_cleanup(handle);
}

/* Doxygen コメントは、ヘッダーに記載 */

cplat_tracer_state cplat_tracer_get_state(cplat_tracer *handle)
{
    cplat_tracer_state state;

    if (tracer_enter_shared(handle) != 0)
    {
        return CPLAT_TRACER_STATE_DISPOSED;
    }

    if (handle->running)
    {
        state = CPLAT_TRACER_STATE_STARTED;
    }
    else
    {
        state = CPLAT_TRACER_STATE_STOPPED;
    }
    config_unlock_shared(handle);
    return state;
}

#define MAX_BODY (CPLAT_TRACER_MESSAGE_MAX_BYTES - 1)

/**
 *  @brief          UTF-8 文字境界を考慮して文字列を切り詰める位置を返します。
 *  @param[in]      s    切り詰め対象の UTF-8 文字列。
 *  @param[in]      pos  切り詰め開始位置 (バイト単位)。
 *  @return         文字境界に合わせた切り詰め位置。
 */
static size_t utf8_safe_truncate(const char *s, const size_t pos)
{
    size_t result = pos;

    while (result > 0 && ((unsigned char)s[result] & 0xC0) == 0x80)
    {
        result--;
    }
    return result;
}

static int should_output(const cplat_trace_level msg_level, const cplat_trace_level threshold);

static int has_output_target(const cplat_tracer *handle, const cplat_trace_level level)
{
    if (handle->hook_head != NULL || should_output(level, handle->stderr_level))
    {
        return 1;
    }
    if (handle->file_handle != NULL && should_output(level, handle->file_level))
    {
        return 1;
    }
#if defined(PLATFORM_LINUX)
    return should_output(level, handle->os_level);
#elif defined(PLATFORM_WINDOWS)
    return should_output(level, handle->etw_level) || should_output(level, handle->os_level);
#endif /* PLATFORM_ */
}

/**
 *  @brief          OS ネイティブのバックエンドにメッセージを書き込みます。
 *  @param[in]      handle      書き込み先のトレース プロバイダー ハンドル。
 *  @param[in]      level       トレース レベル。
 *  @param[in]      timestamp   書き込みに使用する実時刻。NULL の場合は内部で現在時刻を取得。
 *  @param[in]      msg         書き込むメッセージ文字列。
 *  @return         成功時 0、失敗時 -1。
 *
 *  Linux では os_level で syslog をゲートします。\n
 *  Windows では ETW を etw_level で、EventLog を os_level で独立してゲートします。
 *  各レベル判定を内部で行うため、本関数は常に呼び出せます。
 */
static int write_os_backends(cplat_tracer *handle, const cplat_trace_level level, const cplat_timespec *timestamp,
                             const char *msg)
{
#if defined(PLATFORM_LINUX)
    if (should_output(level, handle->os_level))
    {
        return cplat_syslog_sink_write(handle->syslog_handle, to_syslog_level(level), timestamp, msg);
    }
    return CPLAT_OK;
#elif defined(PLATFORM_WINDOWS)
    int etw_result = 0;
    int eventlog_result = 0;

    (void)timestamp;

    if (should_output(level, handle->etw_level))
    {
        etw_result = cplat_etw_provider_write(s_etw_handle, to_etw_level(level), handle->service_name, msg);
    }

    if (should_output(level, handle->os_level))
    {
        eventlog_result = cplat_eventlog_sink_write(s_eventlog_handle, (int)level, handle->file_identifier,
                                                    handle->eventlog_instance_name, handle->identifier, msg);
    }

    if (etw_result != 0 || eventlog_result != 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }
    return CPLAT_OK;
#endif /* PLATFORM_ */
}

/**
 *  @brief          メッセージを出力すべきか判定します。
 *  @param[in]      msg_level   出力するメッセージのトレース レベル。
 *  @param[in]      threshold   出力閾値となるトレース レベル。
 *  @return         出力すべき場合 1、出力不要の場合 0。
 */
static int should_output(const cplat_trace_level msg_level, const cplat_trace_level threshold)
{
    if (threshold == CPLAT_TRACE_LEVEL_NONE)
    {
        return 0;
    }
    return (int)msg_level <= (int)threshold;
}

/**
 *  @brief          スレッショルド レベルとして指定できる値かどうかを判定します。
 *  @param[in]      level 判定するトレース レベル。
 *  @return         指定できる場合は 0 以外、指定できない場合は 0 を返します。
 *
 *  強制出力のレベルは、どの通常のスレッショルド レベルよりも小さい値です。\n
 *  スレッショルド レベルとして指定すると、強制出力の要求だけが通る状態になるため受け付けません。
 */
static int is_valid_threshold(const cplat_trace_level level)
{
    return ((int)level >= (int)CPLAT_TRACE_LEVEL_CRITICAL) && ((int)level <= (int)CPLAT_TRACE_LEVEL_NONE);
}

#define STDERR_TS_BUF_SIZE (CPLAT_CLOCK_ISO8601_LOCAL_MSEC_LEN + 1)

/**
 *  @brief          タイムスタンプとトレース レベルを付加して stderr にエントリを書き込みます。
 *  @param[in]      level  トレース レベル。
 *  @param[in]      timestamp_text  事前整形済みタイムスタンプ文字列。
 *  @param[in]      msg    書き込むメッセージ文字列。
 */
static void write_stderr_entry(const cplat_trace_level level, const char *timestamp_text, const char *msg)
{
    fprintf(stderr, "%s %c %s\n", timestamp_text, cplat_internal_trace_level_char(level), msg);
}

/**
 *  @brief          OS プロバイダ・ファイル・stderr の各出力先にメッセージを書き込みます。
 *  @param[in]      handle  書き込み先のトレース プロバイダー ハンドル。
 *  @param[in]      level   トレース レベル。
 *  @param[in]      timestamp  書き込みに使用する実時刻。NULL の場合は内部で現在時刻を取得。
 *  @param[in]      msg        書き込むメッセージ文字列。
 *  @return         全出力先で成功時 0、いずれかで失敗時 -1。
 */
static int write_dual(cplat_tracer *handle, const cplat_trace_level level, const cplat_timespec *timestamp,
                      const char *msg)
{
    int os_result = 0;
    int file_result = 0;
    int timestamp_fallback_used = 0;
    int needs_text_timestamp;
    cplat_timespec resolved;
    const cplat_timespec *effective_timestamp = NULL;
    char ts[STDERR_TS_BUF_SIZE];

    needs_text_timestamp = (handle->file_handle != NULL && should_output(level, handle->file_level)) ||
                           should_output(level, handle->stderr_level) ||
#if defined(PLATFORM_LINUX)
                           should_output(level, handle->os_level) ||
#endif
                           handle->hook_head != NULL || timestamp != NULL;

    if (needs_text_timestamp)
    {
        if (cplat_internal_trace_resolve_timestamp(timestamp, &resolved, &timestamp_fallback_used) != 0)
        {
            return CPLAT_ERR_UNKNOWN;
        }
        effective_timestamp = &resolved;

        if (cplat_internal_trace_format_local_timestamp(ts, sizeof(ts), effective_timestamp) != 0)
        {
            return CPLAT_ERR_UNKNOWN;
        }
    }

    os_result = write_os_backends(handle, level, effective_timestamp, msg);

    if (handle->file_handle != NULL && should_output(level, handle->file_level))
    {
        file_result =
            cplat_internal_trace_file_sink_write_text(handle->file_handle, (int)level, effective_timestamp, ts, msg);
    }

    if (should_output(level, handle->stderr_level))
    {
        write_stderr_entry(level, ts, msg);
    }

    if (handle->hook_head != NULL)
    {
        cplat_tracer_hook_entry *head = handle->hook_head;
        if (head->fn != NULL)
        {
            head->fn(head->next, handle, level, effective_timestamp, msg, head->context);
        }
    }

    if (timestamp_fallback_used || os_result != 0 || file_result != 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }
    else
    {
        return CPLAT_OK;
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_tracer_write_at(cplat_tracer *handle, const cplat_trace_level level, const cplat_timespec *timestamp,
                          const char *message)
{
    const char *msg;
    char buf[CPLAT_TRACER_MESSAGE_MAX_BYTES];
    size_t len;
    int ret;

    if (handle == NULL || message == NULL)
    {
        return CPLAT_OK;
    }
    if (tracer_enter_shared_running(handle) != 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }
    if (has_output_target(handle, level) == 0 && timestamp == NULL)
    {
        config_unlock_shared(handle);
        return CPLAT_OK;
    }

    msg = message;
    len = strlen(message);
    if (len > MAX_BODY)
    {
        size_t safe_len = utf8_safe_truncate(message, MAX_BODY);
        memcpy(buf, message, safe_len);
        buf[safe_len] = '\0';
        msg = buf;
    }

    ret = write_dual(handle, level, timestamp, msg);
    config_unlock_shared(handle);
    return ret;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_tracer_vwritef_at(cplat_tracer *handle, const cplat_trace_level level, const cplat_timespec *timestamp,
                            const char *format, va_list args)
{
    char buf[CPLAT_TRACER_MESSAGE_MAX_BYTES];
    int ret;

    if (handle == NULL || format == NULL)
    {
        return CPLAT_OK;
    }
    if (tracer_enter_shared_running(handle) != 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }
    if (has_output_target(handle, level) == 0 && timestamp == NULL)
    {
        config_unlock_shared(handle);
        return CPLAT_OK;
    }

    vsnprintf(buf, sizeof(buf), format, args); /* 置換対象外: 意図的な切り詰め */

    ret = write_dual(handle, level, timestamp, buf);
    config_unlock_shared(handle);
    return ret;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_tracer_writef_at(cplat_tracer *handle, const cplat_trace_level level, const cplat_timespec *timestamp,
                           const char *format, ...)
{
    va_list args;
    int ret;

    va_start(args, format);
    ret = cplat_tracer_vwritef_at(handle, level, timestamp, format, args);
    va_end(args);

    return ret;
}

/* Doxygen コメントは、ヘッダーに記載 */

const char *cplat_tracer_hex_sep(const char *message)
{
    if ((message != NULL) && (message[0] != '\0'))
    {
        return " ";
    }
    return "";
}

/* Doxygen コメントは、ヘッダーに記載 */

const char *cplat_tracer_hex_msg(const char *message)
{
    if (message != NULL)
    {
        return message;
    }
    return "";
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_tracer_write_with_source(cplat_tracer *handle, const cplat_trace_level level, const cplat_timespec *timestamp,
                                   const char *file, const int line, const char *message)
{
    if (message != NULL)
    {
        return cplat_tracer_writef_at(handle, level, timestamp, "[%s:%d] %s", file, line, message);
    }

    return cplat_tracer_writef_at(handle, level, timestamp, "[%s:%d]", file, line);
}

static const char s_hex_chars[] = "0123456789ABCDEF";
#define ELLIPSIS_LEN 3

/**
 *  @brief          16 進ダンプ メッセージを構築して出力先に書き込む内部実装です。
 *  @param[in]      handle      書き込み先のトレース プロバイダー ハンドル。
 *  @param[in]      level       トレース レベル。
 *  @param[in]      timestamp   書き込みに使用する実時刻。NULL の場合は内部で現在時刻を取得。
 *  @param[in]      data        ダンプ対象のバイト列。
 *  @param[in]      size        バイト列のサイズ。
 *  @param[in]      label       メッセージに付加するラベル (NULL 可)。
 *  @return         成功時 0、失敗時 -1。
 */
static int hex_write_impl(cplat_tracer *handle, const cplat_trace_level level, const cplat_timespec *timestamp,
                          const void *data, const size_t size, const char *label)
{
    char buf[CPLAT_TRACER_MESSAGE_MAX_BYTES];
    const unsigned char *bytes = (const unsigned char *)data;
    size_t pos = 0;
    size_t max_data_bytes;
    size_t effective_size;
    int truncated = 0;
    size_t i;

    effective_size = size;

    if (handle == NULL || data == NULL || size == 0)
    {
        return CPLAT_OK;
    }

    if (label != NULL && label[0] != '\0')
    {
        size_t lbl_len = strlen(label);
        if (lbl_len + 2 >= MAX_BODY)
        {
            size_t copy_len;
            if (lbl_len < MAX_BODY)
            {
                copy_len = lbl_len;
            }
            else
            {
                copy_len = MAX_BODY;
            }
            memcpy(buf, label, copy_len);
            buf[copy_len] = '\0';
            return write_dual(handle, level, timestamp, buf);
        }
        memcpy(buf, label, lbl_len);
        buf[lbl_len] = ':';
        buf[lbl_len + 1] = ' ';
        pos = lbl_len + 2;
    }

    /* 以降の書き込みは、終端を含めて buf[MAX_BODY] までに収める。
     * 1 バイトは区切りの空白を含めて 3 文字で、先頭の 1 バイトだけ空白を付けない。 */
    max_data_bytes = (MAX_BODY - pos + 1) / 3;

    if (effective_size > max_data_bytes)
    {
        truncated = 1;
        if (pos + ELLIPSIS_LEN > MAX_BODY)
        {
            buf[pos] = '\0';
            return write_dual(handle, level, timestamp, buf);
        }
        /* 本体 (3 * max_data_bytes - 1 文字) と末尾の " ..." (4 文字) の合計を MAX_BODY - pos 以下に収める。 */
        max_data_bytes = (MAX_BODY - pos - ELLIPSIS_LEN) / 3;
        if (max_data_bytes == 0)
        {
            memcpy(buf + pos, "...", ELLIPSIS_LEN);
            pos += ELLIPSIS_LEN;
            buf[pos] = '\0';
            return write_dual(handle, level, timestamp, buf);
        }
        effective_size = max_data_bytes;
    }

    for (i = 0; i < effective_size; i++)
    {
        if (i > 0)
        {
            buf[pos++] = ' ';
        }
        buf[pos++] = s_hex_chars[(bytes[i] >> 4) & 0x0F];
        buf[pos++] = s_hex_chars[bytes[i] & 0x0F];
    }

    if (truncated)
    {
        buf[pos++] = ' ';
        buf[pos++] = '.';
        buf[pos++] = '.';
        buf[pos++] = '.';
    }
    buf[pos] = '\0';

    return write_dual(handle, level, timestamp, buf);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_tracer_write_hex_at(cplat_tracer *handle, const cplat_trace_level level, const cplat_timespec *timestamp,
                              const void *data, const size_t size, const char *message)
{
    int ret;

    if (handle == NULL || data == NULL || size == 0)
    {
        return CPLAT_OK;
    }
    if (tracer_enter_shared_running(handle) != 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }
    if (has_output_target(handle, level) == 0 && timestamp == NULL)
    {
        config_unlock_shared(handle);
        return CPLAT_OK;
    }

    ret = hex_write_impl(handle, level, timestamp, data, size, message);
    config_unlock_shared(handle);
    return ret;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_tracer_vwrite_hexf_at(cplat_tracer *handle, const cplat_trace_level level, const cplat_timespec *timestamp,
                                const void *data, const size_t size, const char *format, va_list args)
{
    char label[CPLAT_TRACER_MESSAGE_MAX_BYTES];
    int ret;

    if (handle == NULL || data == NULL || size == 0)
    {
        return CPLAT_OK;
    }
    if (tracer_enter_shared_running(handle) != 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }
    if (has_output_target(handle, level) == 0 && timestamp == NULL)
    {
        config_unlock_shared(handle);
        return CPLAT_OK;
    }

    if (format != NULL)
    {
        vsnprintf(label, sizeof(label), format, args); /* 置換対象外: 意図的な切り詰め */
        ret = hex_write_impl(handle, level, timestamp, data, size, label);
    }
    else
    {
        ret = hex_write_impl(handle, level, timestamp, data, size, NULL);
    }

    config_unlock_shared(handle);
    return ret;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_tracer_write_hexf_at(cplat_tracer *handle, const cplat_trace_level level, const cplat_timespec *timestamp,
                               const void *data, const size_t size, const char *format, ...)
{
    va_list args;
    int ret;

    va_start(args, format);
    ret = cplat_tracer_vwrite_hexf_at(handle, level, timestamp, data, size, format, args);
    va_end(args);

    return ret;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_tracer_set_name(cplat_tracer *handle, const char *name, const int64_t identifier)
{
    char *effective;

    if (identifier < 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (tracer_enter_exclusive_stopped(handle) != 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    effective = build_effective_name(name, identifier);
    if (effective == NULL)
    {
        config_unlock_exclusive(handle);
        return CPLAT_ERR_OUT_OF_MEMORY;
    }

#if defined(PLATFORM_LINUX)
    {
        int ret;

        ret = cplat_syslog_sink_rename(handle->syslog_handle, effective);
        if (ret != 0)
        {
            cplat_free(effective);
            config_unlock_exclusive(handle);
            return CPLAT_ERR_UNKNOWN;
        }
        cplat_free(handle->effective_name);
        handle->effective_name = effective;
        handle->identifier = identifier;
        config_unlock_exclusive(handle);
        return CPLAT_OK;
    }
#elif defined(PLATFORM_WINDOWS)
    {
        char *eventlog_name = build_eventlog_instance_name(name);
        if (eventlog_name == NULL)
        {
            cplat_free(effective);
            config_unlock_exclusive(handle);
            return CPLAT_ERR_OUT_OF_MEMORY;
        }
        cplat_free(handle->eventlog_instance_name);
        cplat_free(handle->service_name);
        handle->eventlog_instance_name = eventlog_name;
        handle->service_name = effective;
        handle->identifier = identifier;
        config_unlock_exclusive(handle);
        return CPLAT_OK;
    }
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_tracer_get_name(cplat_tracer *handle, char *name_out, const size_t name_size)
{
    int ret;

    if (name_out == NULL || name_size == 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (tracer_enter_shared(handle) != 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }
    ret = cplat_snprintf(name_out, name_size, "%s", tracer_effective_name(handle));
    config_unlock_shared(handle);

    if (ret != CPLAT_OK)
    {
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int64_t cplat_tracer_get_identifier(cplat_tracer *handle)
{
    int64_t identifier;

    if (tracer_enter_shared(handle) != 0)
    {
        return -1;
    }
    identifier = handle->identifier;
    config_unlock_shared(handle);
    return identifier;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_tracer_set_file_name(cplat_tracer *handle, const char *name, const int64_t identifier)
{
    char *name_copy = NULL;

    if (!handle_is_active(handle))
    {
        return CPLAT_ERR_UNKNOWN;
    }
    if (identifier < 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    /* 失敗時に設定を変更しないよう、名前の複製をロック取得前に確保する */
    if (name != NULL)
    {
        name_copy = cplat_strdup(name);
        if (name_copy == NULL)
        {
            return CPLAT_ERR_OUT_OF_MEMORY;
        }
    }

    if (tracer_enter_exclusive_stopped(handle) != 0)
    {
        cplat_free(name_copy);
        return CPLAT_ERR_UNKNOWN;
    }

    cplat_free(handle->file_name);
    handle->file_name = name_copy;
    handle->file_identifier = identifier;
    config_unlock_exclusive(handle);
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_tracer_get_file_name(cplat_tracer *handle, char *file_name_out, const size_t file_name_size)
{
    int ret;

    if (file_name_out == NULL || file_name_size == 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (tracer_enter_shared(handle) != 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }
    ret = resolve_file_name(handle, file_name_out, file_name_size);
    config_unlock_shared(handle);
    return ret;
}

/* Doxygen コメントは、ヘッダーに記載 */

int64_t cplat_tracer_get_file_identifier(cplat_tracer *handle)
{
    int64_t identifier;

    if (tracer_enter_shared(handle) != 0)
    {
        return -1;
    }
    identifier = handle->file_identifier;
    config_unlock_shared(handle);
    return identifier;
}

/* Doxygen コメントは、ヘッダーに記載 */

cplat_trace_level cplat_tracer_get_os_level(cplat_tracer *handle)
{
    cplat_trace_level lv;

    if (tracer_enter_shared(handle) != 0)
    {
        return CPLAT_TRACE_LEVEL_NONE;
    }
    lv = handle->os_level;
    config_unlock_shared(handle);
    return lv;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_tracer_set_os_level(cplat_tracer *handle, const cplat_trace_level level)
{
    if (!is_valid_threshold(level))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (tracer_enter_exclusive(handle) != 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    handle->os_level = level;
    config_unlock_exclusive(handle);
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

cplat_trace_level cplat_tracer_get_etw_level(cplat_tracer *handle)
{
#if defined(PLATFORM_WINDOWS)
    cplat_trace_level lv;

    if (tracer_enter_shared(handle) != 0)
    {
        return CPLAT_TRACE_LEVEL_NONE;
    }
    lv = handle->etw_level;
    config_unlock_shared(handle);
    return lv;
#else  /* PLATFORM_LINUX */
    /* Linux では ETW が存在しないため常に NONE を返す。 */
    (void)handle;
    return CPLAT_TRACE_LEVEL_NONE;
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_tracer_set_etw_level(cplat_tracer *handle, const cplat_trace_level level)
{
    if (!is_valid_threshold(level))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

#if defined(PLATFORM_WINDOWS)
    if (tracer_enter_exclusive(handle) != 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    handle->etw_level = level;
    config_unlock_exclusive(handle);
    return CPLAT_OK;
#else  /* PLATFORM_LINUX */
    /* Linux では ETW が存在しないため何もしない。 */
    (void)handle;
    (void)level;
    return CPLAT_OK;
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

cplat_trace_level cplat_tracer_get_file_level(cplat_tracer *handle)
{
    cplat_trace_level lv;

    if (tracer_enter_shared(handle) != 0)
    {
        return CPLAT_TRACE_LEVEL_NONE;
    }
    lv = handle->file_level;
    config_unlock_shared(handle);
    return lv;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_tracer_set_file_level(cplat_tracer *handle, const char *path, const cplat_trace_level level,
                                const size_t max_bytes, const int generations, const int flags)
{
    char *path_copy = NULL;

    if (!is_valid_threshold(level))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    /* 失敗時に設定を変更しないよう、パスの複製をロック取得前に確保する */
    if (path != NULL)
    {
        path_copy = cplat_strdup(path);
        if (path_copy == NULL)
        {
            return CPLAT_ERR_OUT_OF_MEMORY;
        }
    }

    if (tracer_enter_exclusive(handle) != 0)
    {
        cplat_free(path_copy);
        return CPLAT_ERR_UNKNOWN;
    }

    if (handle->running)
    {
        /* started 中はレベルを即時反映する。排他ロック下で原子的に切り替えるため、
         * 旧閾値と新閾値の両方で出力対象となるトレースを取りこぼさない。 */

        /* ケース 1: ファイル出力を無効化する。 */
        if (level == CPLAT_TRACE_LEVEL_NONE)
        {
            if (handle->file_handle != NULL)
            {
                cplat_trace_file_sink_dispose(handle->file_handle);
                handle->file_handle = NULL;
            }
            cplat_free(handle->file_path);
            handle->file_path = path_copy;
            handle->file_level = CPLAT_TRACE_LEVEL_NONE;
            handle->file_max_bytes = max_bytes;
            handle->file_generations = generations;
            handle->file_flags = flags;
            config_unlock_exclusive(handle);
            return CPLAT_OK;
        }

        /* ケース 2: 構造パラメーターが一致し、すでにファイルが開いている場合はしきい値のみ変更する。
         * 再オープンを伴わないため、同一パスの一時的なオープン失敗で稼働中の出力を失わない。 */
        if (handle->file_handle != NULL &&
            ((path == NULL && handle->file_path == NULL) ||
             (path != NULL && handle->file_path != NULL && strcmp(path, handle->file_path) == 0)) &&
            max_bytes == handle->file_max_bytes && generations == handle->file_generations &&
            flags == handle->file_flags)
        {
            handle->file_level = level;
            config_unlock_exclusive(handle);
            cplat_free(path_copy);
            return CPLAT_OK;
        }

        /* ケース 3: 新パラメーターでファイルを開き直す。先に新 sink を開き、成功時のみ差し替える。
         * オープン失敗時は旧ハンドルと旧設定を保持して CPLAT_ERR_UNKNOWN を返す。 */
        {
            cplat_trace_file_sink *new_sink = open_file_sink_with(handle, path, max_bytes, generations, flags);
            if (new_sink == NULL)
            {
                config_unlock_exclusive(handle);
                cplat_free(path_copy);
                return CPLAT_ERR_UNKNOWN;
            }
            if (handle->file_handle != NULL)
            {
                cplat_trace_file_sink_dispose(handle->file_handle);
            }
            handle->file_handle = new_sink;
            cplat_free(handle->file_path);
            handle->file_path = path_copy;
            handle->file_level = level;
            handle->file_max_bytes = max_bytes;
            handle->file_generations = generations;
            handle->file_flags = flags;
            config_unlock_exclusive(handle);
            return CPLAT_OK;
        }
    }

    /* stopped 中は設定を記録するのみ (ファイルのオープンは start に遅延する)。 */
    if (handle->file_handle != NULL)
    {
        cplat_trace_file_sink_dispose(handle->file_handle);
        handle->file_handle = NULL;
    }

    cplat_free(handle->file_path);
    handle->file_path = path_copy;
    handle->file_level = level;
    handle->file_max_bytes = max_bytes;
    handle->file_generations = generations;
    handle->file_flags = flags;

    config_unlock_exclusive(handle);
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

cplat_trace_level cplat_tracer_get_stderr_level(cplat_tracer *handle)
{
    cplat_trace_level lv;

    if (tracer_enter_shared(handle) != 0)
    {
        return CPLAT_TRACE_LEVEL_NONE;
    }
    lv = handle->stderr_level;
    config_unlock_shared(handle);
    return lv;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_tracer_set_stderr_level(cplat_tracer *handle, const cplat_trace_level level)
{
    if (!is_valid_threshold(level))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (tracer_enter_exclusive(handle) != 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    handle->stderr_level = level;
    config_unlock_exclusive(handle);
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_tracer_dispose(cplat_tracer **handle)
{
    cplat_tracer *target;

    if (handle == NULL || *handle == NULL)
    {
        return;
    }
    target = *handle;
    if (begin_dispose(target) != 0)
    {
        return;
    }

    if (stop_handle_for_cleanup(target) != CPLAT_OK)
    {
        target->lifecycle_state = TRACE_HANDLE_ACTIVE;
        return;
    }
    if (registry_unregister_handle(target) != 0)
    {
        target->lifecycle_state = TRACE_HANDLE_ACTIVE;
        return;
    }
    trace_handle_release_normal(target);
    *handle = NULL;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_trace_registry_dispose_all_on_shutdown(const cplat_shutdown_event *event)
{
    cplat_tracer **items;
    size_t count;
    size_t i;

    if (event == NULL || registry_lock() != 0)
    {
        return;
    }

    if (s_trace_registry.shutdown_started)
    {
        registry_unlock();
        return;
    }

    s_trace_registry.shutdown_started = 1;
    items = s_trace_registry.items;
    count = s_trace_registry.count;

    s_trace_registry.items = NULL;
    s_trace_registry.count = 0;
    s_trace_registry.capacity = 0;
    registry_unlock();

    for (i = 0; i < count; i++)
    {
        cplat_tracer *handle = items[i];

        if (handle == NULL || handle->lifecycle_state == TRACE_HANDLE_DISPOSED)
        {
            continue;
        }

        handle->lifecycle_state = TRACE_HANDLE_DISPOSING;
        handle->running = 0;
        trace_handle_release_on_shutdown(handle);
    }

#if defined(PLATFORM_WINDOWS)
    if (s_etw_handle != NULL)
    {
        cplat_internal_etw_provider_dispose_on_shutdown(s_etw_handle, event);
        s_etw_handle = NULL;
    }
    if (s_eventlog_handle != NULL)
    {
        cplat_internal_eventlog_sink_dispose_on_shutdown(s_eventlog_handle, event);
        s_eventlog_handle = NULL;
    }
    s_trace_ref = 0;
#endif /* PLATFORM_WINDOWS */

    cplat_free(items);
    if (s_registry_lock != NULL)
    {
        cplat_local_lock_dispose(s_registry_lock);
        s_registry_lock = NULL;
        s_registry_lock_init_result = CPLAT_ERR_UNKNOWN;
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

cplat_tracer_hook_entry *cplat_tracer_set_hook(cplat_tracer *handle, cplat_tracer_hook_fn fn, void *context)
{
    cplat_tracer_hook_entry *entry;

    if (!handle_is_active(handle) || fn == NULL)
    {
        return NULL;
    }

    entry = (cplat_tracer_hook_entry *)cplat_malloc(sizeof(cplat_tracer_hook_entry));
    if (entry == NULL)
    {
        return NULL;
    }

    if (tracer_enter_exclusive_stopped(handle) != 0)
    {
        cplat_free(entry);
        return NULL;
    }

    entry->fn = fn;
    entry->context = context;
    entry->next = handle->hook_head;
    handle->hook_head = entry;

    config_unlock_exclusive(handle);
    return entry;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_tracer_remove_hook(cplat_tracer *handle, cplat_tracer_hook_entry *hook_entry)
{
    cplat_tracer_hook_entry **pp;

    if (hook_entry == NULL)
    {
        return;
    }
    if (tracer_enter_exclusive_stopped(handle) != 0)
    {
        return;
    }

    for (pp = &handle->hook_head; *pp != NULL; pp = &(*pp)->next)
    {
        if (*pp == hook_entry)
        {
            *pp = hook_entry->next;
            cplat_free(hook_entry);
            break;
        }
    }

    config_unlock_exclusive(handle);
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_tracer_call_next_hook(cplat_tracer_hook_entry *prev, cplat_tracer *handle, const cplat_trace_level level,
                                 const cplat_timespec *timestamp, const char *message)
{
    if (prev == NULL || prev->fn == NULL)
    {
        return;
    }
    prev->fn(prev->next, handle, level, timestamp, message, prev->context);
}
