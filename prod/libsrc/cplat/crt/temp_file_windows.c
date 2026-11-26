/**
 *******************************************************************************
 *  @file           temp_file_windows.c
 *  @brief          Windows の一時ファイルの作成を実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/11/26
 *  @version        1.0.0
 *
 *  乱数の名前と CreateFileW の CREATE_NEW による一時ファイルの作成を実装します。\n
 *  Linux では空の翻訳単位になります。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/crt/temp_file_internal.h>
#include <cplat/crt/stdlib.h>

#if defined(PLATFORM_WINDOWS)

    #include <cplat/base/error_internal.h>
    #include <cplat/base/result.h>
    #include <cplat/base/windows_sdk.h>
    #include <cplat/crt/temp_dir_internal.h>
    #include <cplat/crypto/random.h>

    #include <errno.h>
    #include <stdint.h>
    #include <string.h>
    #include <wchar.h>

    /* 名前に用いる乱数のバイト数 [byte]。16 進表記で 2 倍の文字数になる。 */
    #define TEMP_FILE_RANDOM_BYTES 8U

    /* 名前の乱数部の文字数 [wchar_t]。 */
    #define TEMP_FILE_RANDOM_CHARS (TEMP_FILE_RANDOM_BYTES * 2U)

    /* 拡張パスの接頭辞の最大の文字数 [wchar_t] (`\\?\UNC\` の 8 文字)。 */
    #define TEMP_FILE_EXTENDED_PREFIX_MAX 8U

static size_t count_prefix_chars(const wchar_t *prefix);
static void normalize_separators(wchar_t *path);
static const wchar_t *select_extended_prefix(const wchar_t *dir, size_t full_length, size_t *skip_out);
static void write_random_name(wchar_t *dest, const uint8_t *random_bytes);

/* ファイル名の末尾に付ける拡張子。 */
static const wchar_t s_temp_file_suffix[] = L".tmp";

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_create_temp_file_w(const wchar_t *prefix, wchar_t **path_out, cplat_error *detail_out)
{
    const size_t suffix_chars = (sizeof(s_temp_file_suffix) / sizeof(s_temp_file_suffix[0])) - 1U;
    wchar_t *dir = NULL;
    wchar_t *path;
    const wchar_t *extended_prefix;
    size_t dir_length = 0U;
    size_t prefix_chars;
    size_t separator_chars;
    size_t extended_chars;
    size_t skip_chars = 0U;
    size_t full_length;
    size_t capacity;
    size_t name_offset;
    DWORD last_error = ERROR_FILE_EXISTS;
    int attempt;
    int rc;

    if (path_out == NULL)
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    *path_out = NULL;

    /*
     * GetTempPath2W / GetTempPathW の実行時選択と十分な領域の確保を内部の共通関数へ集約する。
     * GetTempFileNameW はディレクトリ部が MAX_PATH - 14 文字を超えると失敗し、出力先も MAX_PATH 文字に
     * 限られるため使わず、名前を乱数で作って CREATE_NEW で作成する。
     * see: https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-gettempfilenamew
     */
    rc = cplat_internal_get_temp_dir_w(&dir, &dir_length, detail_out);
    if (rc != CPLAT_OK)
    {
        return rc;
    }

    normalize_separators(dir);

    prefix_chars = count_prefix_chars(prefix);
    separator_chars = (dir_length > 0U && dir[dir_length - 1U] != L'\\') ? 1U : 0U;

    /* 一時ディレクトリの長さは OS の拡張パスの上限 (32767 文字) で抑えられるが、加算の桁あふれを念のため防ぐ。 */
    if (dir_length >
        ((SIZE_MAX / sizeof(wchar_t)) - (TEMP_FILE_EXTENDED_PREFIX_MAX + 1U + CPLAT_INTERNAL_TEMP_FILE_PREFIX_MAX +
                                         TEMP_FILE_RANDOM_CHARS + suffix_chars + 1U)))
    {
        cplat_free(dir);
        return cplat_internal_error_report_errno(detail_out, ENOMEM);
    }

    full_length = dir_length + separator_chars + prefix_chars + TEMP_FILE_RANDOM_CHARS + suffix_chars;

    /*
     * Win32 のファイル API は、拡張パスの接頭辞 (\\?\) のないパスを MAX_PATH 文字 (終端 NUL を含む) に制限する。
     * 一時ディレクトリが長い場合だけ接頭辞を付け、通常の長さのパスは従来どおり接頭辞なしで返す。
     * see: https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation
     */
    extended_prefix = select_extended_prefix(dir, full_length, &skip_chars);
    extended_chars = wcslen(extended_prefix);

    capacity = extended_chars + (full_length - skip_chars) + 1U;
    path = (wchar_t *)cplat_malloc(capacity * sizeof(wchar_t));
    if (path == NULL)
    {
        cplat_free(dir);
        return cplat_internal_error_report_errno(detail_out, ENOMEM);
    }

    name_offset = 0U;
    memcpy(path + name_offset, extended_prefix, extended_chars * sizeof(wchar_t));
    name_offset += extended_chars;
    memcpy(path + name_offset, dir + skip_chars, (dir_length - skip_chars) * sizeof(wchar_t));
    name_offset += dir_length - skip_chars;
    if (separator_chars != 0U)
    {
        path[name_offset] = L'\\';
        name_offset += 1U;
    }
    if (prefix_chars != 0U)
    {
        memcpy(path + name_offset, prefix, prefix_chars * sizeof(wchar_t));
        name_offset += prefix_chars;
    }
    cplat_free(dir);

    for (attempt = 0; attempt < CPLAT_INTERNAL_TEMP_FILE_ATTEMPT_MAX; ++attempt)
    {
        uint8_t random_bytes[TEMP_FILE_RANDOM_BYTES];
        HANDLE handle;

        rc = cplat_random_bytes(random_bytes, sizeof(random_bytes));
        if (rc != CPLAT_OK)
        {
            cplat_free(path);
            return cplat_internal_error_report_errno_as(detail_out, EIO, rc);
        }

        write_random_name(path + name_offset, random_bytes);
        memcpy(path + name_offset + TEMP_FILE_RANDOM_CHARS, s_temp_file_suffix, sizeof(s_temp_file_suffix));

        /*
         * CREATE_NEW は同じ名前のファイルがある場合に失敗するため、ほかのプロセスが作ったファイルを開いたり
         * 切り詰めたりしない。呼び出し元が後で開き直せるよう、共有のモードはすべて許す。
         * see: https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew
         */
        handle = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                             CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (handle != INVALID_HANDLE_VALUE)
        {
            /* 空のファイルの作成は済んでおり、閉じる処理が失敗しても作成の結果は変わらないため、戻り値は明示的に破棄する。 */
            (void)CloseHandle(handle);
            *path_out = path;
            return cplat_internal_error_report_success(detail_out);
        }

        last_error = GetLastError();
        if (last_error != ERROR_FILE_EXISTS && last_error != ERROR_ALREADY_EXISTS)
        {
            cplat_free(path);
            return cplat_internal_error_report_windows_error(detail_out, last_error);
        }
    }

    cplat_free(path);
    return cplat_internal_error_report_windows_error(detail_out, last_error);
}

/**
 *  @brief          接頭辞のうち、名前に採用する文字数を数えます。
 *  @param[in]      prefix 接頭辞。NULL 可。
 *  @return         NUL の前までの文字数と `CPLAT_INTERNAL_TEMP_FILE_PREFIX_MAX` の小さいほうを返します。
 *                  @p prefix が NULL の場合は 0 を返します。
 */
static size_t count_prefix_chars(const wchar_t *prefix)
{
    size_t count = 0U;

    if (prefix == NULL)
    {
        return 0U;
    }

    while (count < CPLAT_INTERNAL_TEMP_FILE_PREFIX_MAX && prefix[count] != L'\0')
    {
        ++count;
    }

    return count;
}

/**
 *  @brief          パスの区切りの '/' を '\\' へそろえます。
 *  @param[in,out]  path 対象のパス (NUL 終端)。
 *
 *  拡張パスの接頭辞を付けたパスは OS が区切りを変換しないため、接頭辞を付ける前にそろえる。
 */
static void normalize_separators(wchar_t *path)
{
    for (wchar_t *p = path; *p != L'\0'; ++p)
    {
        if (*p == L'/')
        {
            *p = L'\\';
        }
    }
}

/**
 *  @brief          完全なパスの長さから、前置する拡張パスの接頭辞を選びます。
 *  @param[in]      dir         区切りを '\\' へそろえた一時ディレクトリ。
 *  @param[in]      full_length 接頭辞を付けない場合の完全なパスの長さ (NUL を含まない) [wchar_t]。
 *  @param[out]     skip_out    @p dir の先頭から読み飛ばす文字数の格納先。
 *  @return         前置する接頭辞を返します。付けない場合は空文字列を返します。
 *
 *  UNC のパスは先頭の `\\` を `\\?\UNC\` へ置き換えるため、@p skip_out に 2 を格納する。
 */
static const wchar_t *select_extended_prefix(const wchar_t *dir, const size_t full_length, size_t *skip_out)
{
    *skip_out = 0U;

    if (full_length < (size_t)MAX_PATH)
    {
        return L"";
    }

    if (dir[0] == L'\\' && dir[1] == L'\\')
    {
        /* すでに拡張パス (\\?\) またはデバイスのパス (\\.\) であれば、そのまま使う。 */
        if ((dir[2] == L'?' || dir[2] == L'.') && dir[3] == L'\\')
        {
            return L"";
        }

        *skip_out = 2U;
        return L"\\\\?\\UNC\\";
    }

    return L"\\\\?\\";
}

/**
 *  @brief          乱数のバイト列を、小文字の 16 進表記で書き込みます。
 *  @param[out]     dest         書き込み先。`TEMP_FILE_RANDOM_CHARS` 文字の領域が必要です (NUL は書き込まない)。
 *  @param[in]      random_bytes `TEMP_FILE_RANDOM_BYTES` バイトの乱数。
 */
static void write_random_name(wchar_t *dest, const uint8_t *random_bytes)
{
    static const wchar_t hex_digits[] = L"0123456789abcdef";
    size_t i;

    for (i = 0U; i < TEMP_FILE_RANDOM_BYTES; ++i)
    {
        dest[i * 2U] = hex_digits[(random_bytes[i] >> 4U) & 0x0FU];
        dest[(i * 2U) + 1U] = hex_digits[random_bytes[i] & 0x0FU];
    }
}

#endif /* PLATFORM_WINDOWS */
