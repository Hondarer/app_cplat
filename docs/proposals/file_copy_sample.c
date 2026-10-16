/**
 *******************************************************************************
 *  @file           file_copy_sample.c
 *  @brief          条件付きファイル コピー関数のサンプル実装です。
 *  @author         Tetsuo Honda
 *  @date           2026/08/26
 *
 *  設計の意図、判断の根拠、制限事項は file-copy-design.md に記載します。
 *  本ファイルには実装だけを置きます。
 *
 *  本ファイルは prompt/ 配下の作業用サンプルであり、ビルド対象ではありません。
 *  app へ組み込む際は、結果コードの define と関数プロトタイプを
 *  prod/include/<lib>/ の公開ヘッダーへ移し、実装を prod/libsrc/<lib>/ へ配置してください。
 *  あわせて sample_ と SAMPLE_ の接頭辞を、配置先の lib 名へ置き換えてください。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/base/error.h>
#include <cplat/base/platform.h>
#include <cplat/base/result.h>
#include <cplat/clock/timespec.h>
#include <cplat/crt/file.h>
#include <cplat/crt/stdlib.h>
#include <cplat/crt/sys/stat.h>

#include <stddef.h>
#include <stdint.h>

/* ------------------------------------------------------------------------- */
/*  結果コード (app へ移す際は公開ヘッダーへ移動する)                        */
/* ------------------------------------------------------------------------- */

#define SAMPLE_OK      0 /**< コピーを実行しました。 */
#define SAMPLE_SKIPPED 1 /**< from が新しくないため、コピーを省略しました。 */

#define SAMPLE_ERR                  (-1) /**< 分類済みコードに該当しない、その他のエラーです。 */
#define SAMPLE_ERR_INVALID_ARGUMENT (-2) /**< 引数が不正です (NULL など)。 */
#define SAMPLE_ERR_NOT_FOUND        (-3) /**< from または to が通常ファイルとして存在しません。 */
#define SAMPLE_ERR_SIZE_MISMATCH    (-4) /**< from と to のファイル サイズが一致しません。 */
#define SAMPLE_ERR_OUT_OF_MEMORY    (-5) /**< 転送バッファーを確保できません。 */

/* ------------------------------------------------------------------------- */
/*  内部定数                                                                  */
/* ------------------------------------------------------------------------- */

/*
 *  転送バッファーのサイズです。
 *  1 回開いて全体を 1 度だけ通す逐次処理では、ブロック単位の読み書きが最良であり、
 *  64 KB 程度をまとめる指針に従います。
 *  see: app/cplat/docs/fileio-api-selection-guideline.md の「判断手順」
 */
#define SAMPLE_COPY_BUFFER_SIZE ((size_t)64 * 1024)

/* ------------------------------------------------------------------------- */
/*  公開関数のプロトタイプ (app へ移す際は公開ヘッダーへ移動する)            */
/* ------------------------------------------------------------------------- */

/**
 *  @brief          from が to より新しい場合に限り、from の内容で to を上書きします。
 *  @param[in]      from_path   コピー元のパス (UTF-8)。NULL を渡してはなりません。
 *  @param[in]      to_path     コピー先のパス (UTF-8)。NULL を渡してはなりません。
 *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
 *                  エラー詳細を設定せず、返却しません。
 *                  NULL 以外を指定した場合、成功時は空の値を格納します。
 *  @return         @ref SAMPLE_OK 、@ref SAMPLE_SKIPPED 、@ref SAMPLE_ERR 、
 *                  @ref SAMPLE_ERR_INVALID_ARGUMENT 、@ref SAMPLE_ERR_NOT_FOUND 、
 *                  @ref SAMPLE_ERR_SIZE_MISMATCH 、@ref SAMPLE_ERR_OUT_OF_MEMORY の
 *                  いずれかを返します。
 *
 *  次の条件をすべて満たす場合にコピーを実行し、@ref SAMPLE_OK を返します。
 *
 *  - @p from_path と @p to_path が、いずれも通常ファイルとして存在する
 *  - 両者のファイル サイズが一致する
 *  - @p from_path の最終更新日時が @p to_path より新しい
 *
 *  最終更新日時の条件だけが成立しない場合は、コピーを行わずに @ref SAMPLE_SKIPPED を返します。
 *  これはエラーではありません。
 *
 *  @par            排他制御
 *  本関数は排他制御を行いません。
 *  呼び出し側は、@p from_path と @p to_path の双方について、
 *  他プロセスによる更新を排除した状態で呼び出してください。
 *
 *  @par            中断時の回復
 *  コピーの途中で失敗した場合、@p to_path の最終更新日時をコピー開始前の値へ戻します。
 *  次回の呼び出しで再び @p from_path のほうが新しくなるため、コピーが再試行され、
 *  壊れた内容は上書きされます。\n
 *  ただし日時を戻す前にプロセスが停止した場合は回復できません。
 *  日時を戻した直後の @p to_path は、内容が壊れているにもかかわらず日時だけが正常に見えます。
 *
 *  @note           コピーに成功すると、@p to_path の最終更新日時を @p from_path と同じ値に設定します。
 *                  内容と日時の両方が一致した状態になり、2 回目以降の呼び出しは
 *                  @ref SAMPLE_SKIPPED になります。
 *
 *  @par            スレッド セーフ
 *  本関数は条件付きスレッド セーフです。\n
 *  異なるパスに対する呼び出しは同時に実行できます。\n
 *  同一パスに対する操作は、呼び出し側で同期してください。
 */
int sample_file_copy_if_newer(const char *from_path, const char *to_path, cplat_error *detail_out);

/* ------------------------------------------------------------------------- */
/*  内部関数                                                                  */
/* ------------------------------------------------------------------------- */

/*
 *  パスのファイル情報を取得し、失敗を SAMPLE_* の結果コードへ分類します。
 *  対象が存在しない場合と、通常ファイルでない場合を SAMPLE_ERR_NOT_FOUND に寄せます。
 *  種別の判定は cplat_file_stat_is_regular() が OS 差異を吸収します。
 */
static int stat_regular_file(cplat_file_stat_t *stat_out, cplat_error *detail, const char *path)
{
    if (cplat_stat(stat_out, detail, path) != CPLAT_OK)
    {
        if (cplat_error_is(detail, CPLAT_CAUSE_NOT_FOUND) != 0)
        {
            return SAMPLE_ERR_NOT_FOUND;
        }

        return SAMPLE_ERR;
    }

    if (cplat_file_stat_is_regular(stat_out) == 0)
    {
        return SAMPLE_ERR_NOT_FOUND;
    }

    return SAMPLE_OK;
}

/*
 *  from_file の内容を to_file の先頭から書き込みます。
 *  cplat_file_read() は終端以外でも要求より少ない量を返すため、
 *  読み取り量が 0 になるまで繰り返します。
 */
static int copy_content(cplat_file *from_file, cplat_file *to_file, uint8_t *buffer, size_t buffer_size,
                        int64_t expected_size, cplat_error *detail)
{
    int64_t copied_size = 0;

    for (;;)
    {
        size_t read_size = 0;

        if (cplat_file_read(from_file, buffer, buffer_size, &read_size, detail) != CPLAT_OK)
        {
            return SAMPLE_ERR;
        }

        if (read_size == 0U)
        {
            break;
        }

        if (cplat_file_write(to_file, buffer, read_size, detail) != CPLAT_OK)
        {
            return SAMPLE_ERR;
        }

        copied_size += (int64_t)read_size;
    }

    /* 事前に確認したサイズと転送量が一致しない場合は、排他の前提が崩れている。 */
    if (copied_size != expected_size)
    {
        return SAMPLE_ERR;
    }

    return SAMPLE_OK;
}

/*
 *  コピーの本体です。資源を確保したあとの失敗を goto で 1 箇所へ集約するため、
 *  事前条件の判定と分離しています。
 *
 *  from_timestamp はコピー成功時に to_path へ設定する値、
 *  to_timestamp_backup は失敗時に to_path へ戻す値です。
 */
static int copy_verified(const char *from_path, const char *to_path, int64_t expected_size,
                         const cplat_timespec *from_timestamp, const cplat_timespec *to_timestamp_backup,
                         cplat_error *detail)
{
    cplat_file from_file;
    cplat_file to_file;
    uint8_t *buffer = NULL;
    int result = SAMPLE_OK;

    cplat_file_init(&from_file);
    cplat_file_init(&to_file);

    buffer = (uint8_t *)cplat_calloc(SAMPLE_COPY_BUFFER_SIZE, sizeof(*buffer));
    if (buffer == NULL)
    {
        return SAMPLE_ERR_OUT_OF_MEMORY;
    }

    /*
     *  外部で from と to のアクセスが排他されている前提のため、
     *  cplat_stat() による確認と、ここでのオープンとの間に実体は入れ替わらない。
     *  Coverity の TOCTOU 検出はこの前提を判定できないため、注釈で抑制する。
     *  coverity[TOCTOU]
     */
    if (cplat_file_open(&from_file, from_path, CPLAT_FILE_OPEN_READ, detail) != CPLAT_OK)
    {
        result = SAMPLE_ERR;
        goto cleanup;
    }

    /*
     *  CPLAT_FILE_OPEN_CREATE と CPLAT_FILE_OPEN_TRUNCATE は指定しない。
     *  既存のファイル実体を保ったまま先頭から上書きし、
     *  外部の排他がファイル実体に結び付いている場合でも破綻させないため。
     *  サイズの一致を事前条件にしているため、先頭からの上書きで全長が置き換わる。
     *  coverity[TOCTOU]
     */
    if (cplat_file_open(&to_file, to_path, CPLAT_FILE_OPEN_WRITE, detail) != CPLAT_OK)
    {
        result = SAMPLE_ERR;
        goto cleanup;
    }

    result = copy_content(&from_file, &to_file, buffer, SAMPLE_COPY_BUFFER_SIZE, expected_size, detail);
    if (result != SAMPLE_OK)
    {
        goto cleanup;
    }

    if (cplat_file_flush(&to_file, detail) != CPLAT_OK)
    {
        result = SAMPLE_ERR;
        goto cleanup;
    }

    /* 内容と日時の両方をコピー元にそろえる。次回の呼び出しは省略になる。 */
    if (cplat_file_set_modified_timestamp(&to_file, from_timestamp, detail) != CPLAT_OK)
    {
        result = SAMPLE_ERR;
        goto cleanup;
    }

cleanup:
    if (cplat_file_close(&to_file, detail) != CPLAT_OK)
    {
        /* 書き込み側のクローズ失敗は転送の失敗と同義のため、成功を上書きする。 */
        if (result == SAMPLE_OK)
        {
            result = SAMPLE_ERR;
        }
    }

    (void)cplat_file_close(&from_file, detail);
    cplat_free(buffer);

    if (result != SAMPLE_OK)
    {
        /*
         *  コピー先の最終更新日時を開始前の値へ戻し、次回の呼び出しで再試行させる。
         *  ここで失敗しても報告済みの結果を上書きしない。最善努力の回復であり、
         *  戻せなかった場合は壊れた内容が残る。
         */
        (void)cplat_file_set_path_modified_timestamp(to_path, to_timestamp_backup, NULL);
    }

    return result;
}

/*
 *  事前条件を判定し、成立した場合にコピーの本体を呼び出します。
 *  資源を確保していない区間のため、早期の return で復帰します。
 */
static int copy_if_newer(const char *from_path, const char *to_path, cplat_error *detail)
{
    cplat_file_stat_t from_stat;
    cplat_file_stat_t to_stat;
    cplat_timespec from_timestamp;
    cplat_timespec to_timestamp;
    int result;

    if ((from_path == NULL) || (to_path == NULL))
    {
        return SAMPLE_ERR_INVALID_ARGUMENT;
    }

    result = stat_regular_file(&from_stat, detail, from_path);
    if (result != SAMPLE_OK)
    {
        return result;
    }

    result = stat_regular_file(&to_stat, detail, to_path);
    if (result != SAMPLE_OK)
    {
        return result;
    }

    /* st_size の型は Linux が off_t、Windows が __int64 のため、int64_t へそろえる。 */
    if ((int64_t)from_stat.st_size != (int64_t)to_stat.st_size)
    {
        return SAMPLE_ERR_SIZE_MISMATCH;
    }

    /*
     *  cplat_stat() の st_mtime は秒精度のため、日時の比較には使わない。
     *  サブ秒まで見る専用 API で取得し、同一秒内の更新を取りこぼさないようにする。
     */
    if (cplat_file_get_path_modified_timestamp(from_path, &from_timestamp, detail) != CPLAT_OK)
    {
        return SAMPLE_ERR;
    }

    if (cplat_file_get_path_modified_timestamp(to_path, &to_timestamp, detail) != CPLAT_OK)
    {
        return SAMPLE_ERR;
    }

    if (cplat_timespec_cmp(&from_timestamp, &to_timestamp) <= 0)
    {
        return SAMPLE_SKIPPED;
    }

    return copy_verified(from_path, to_path, (int64_t)from_stat.st_size, &from_timestamp, &to_timestamp, detail);
}

/* ------------------------------------------------------------------------- */
/*  公開関数                                                                  */
/* ------------------------------------------------------------------------- */

/* Doxygen コメントは、プロトタイプに記載 */

int sample_file_copy_if_newer(const char *from_path, const char *to_path, cplat_error *detail_out)
{
    cplat_error detail;
    int result;

    /*
     *  detail_out は NULL を許容する一方、内部では要因の分類に詳細エラーを使う。
     *  そのためローカルの詳細エラーで受け、復帰前に 1 箇所で書き戻す。
     */
    cplat_error_clear(&detail);

    result = copy_if_newer(from_path, to_path, &detail);

    if (detail_out != NULL)
    {
        *detail_out = detail;
    }

    return result;
}
