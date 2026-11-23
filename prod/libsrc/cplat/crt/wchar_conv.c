/**
 *******************************************************************************
 *  @file           wchar_conv.c
 *  @brief          Windows で UTF-8 とワイド文字列を相互変換する API を実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/06/08
 *  @version        1.0.0
 *
 *  Windows 上で UTF-8 文字列とワイド文字列 (UTF-16LE) を相互変換する
 *  関数を提供します。変換できない文字列は置換せず、結果コードで通知します。\n
 *  Linux では空の翻訳単位になります。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/crt/wchar_conv.h>

#if defined(PLATFORM_WINDOWS)

    #include <cplat/base/result_internal.h>
    #include <cplat/base/windows_sdk.h>
    #include <cplat/crt/stdlib.h>
    #include <cplat/win32/wchar_conv_internal.h>
    #include <errno.h>
    #include <limits.h>
    #include <string.h>

/**
 *  @brief          Win32 の文字コード変換失敗を共通結果コードへ分類します。
 *  @return         `GetLastError()` に対応する共通結果コードを返します。
 *
 *  不正な入力と出力容量の不足を一般的な Win32 エラー分類より優先して扱います。
 */
static int conversion_result_from_last_error(void)
{
    const DWORD error_code = GetLastError();

    if (error_code == ERROR_NO_UNICODE_TRANSLATION)
    {
        return CPLAT_ERR_INVALID_ENCODING;
    }

    if (error_code == ERROR_INSUFFICIENT_BUFFER)
    {
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    return cplat_internal_result_from_windows_error(error_code);
}

/**
 *  @brief          UTF-8 を指定された長さで厳密にワイド文字列へ変換します。
 *  @param[out]     dest       変換先。
 *  @param[in]      dest_size  変換先の要素数。
 *  @param[in]      src        変換元。
 *  @param[in]      src_len    入力バイト数。-1 は NUL 終端を含めた変換です。
 *  @return         成功時は CPLAT_OK、失敗時は共通結果コードを返します。
 */
static int utf8_to_wide(wchar_t *dest, const size_t dest_size, const char *src, const int src_len)
{
    int converted;

    if (dest != NULL && dest_size > 0U)
    {
        /* 引数の誤りでも有効な出力先を空にし、前回の内容を誤用させません。 */
        dest[0] = L'\0';
    }

    if (dest == NULL || src == NULL || dest_size == 0U || dest_size > (size_t)INT_MAX)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    /* フラグなしでは不正な UTF-8 が U+FFFD に置き換わるため、厳密変換を指定します。
     * see: https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar
     * see: https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte
     */
    converted = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src, src_len, dest, (int)dest_size);
    if (converted == 0)
    {
        const int result = conversion_result_from_last_error();
        dest[0] = L'\0';
        return result;
    }

    return CPLAT_OK;
}

/**
 *  @brief          ワイド文字列を UTF-8 へ厳密に変換します。
 *  @param[out]     dest       変換先。
 *  @param[in]      dest_size  変換先のバイト数。
 *  @param[in]      wtext      変換元。
 *  @return         成功時は CPLAT_OK、失敗時は共通結果コードを返します。
 */
static int wide_to_utf8(char *dest, const size_t dest_size, const wchar_t *wtext)
{
    int converted;

    if (dest != NULL && dest_size > 0U)
    {
        dest[0] = '\0';
    }

    if (dest == NULL || wtext == NULL || dest_size == 0U || dest_size > (size_t)INT_MAX)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    /* WC_ERR_INVALID_CHARS は対になっていない UTF-16 サロゲートを拒否します。
     * see: https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte
     */
    converted = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wtext, -1, dest, (int)dest_size, NULL, NULL);
    if (converted == 0)
    {
        const int result = conversion_result_from_last_error();
        dest[0] = '\0';
        return result;
    }

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_utf8_to_wide_n(wchar_t **wtext_out, int *count_out, const char *src, const int src_len)
{
    int required;
    wchar_t *wtext;
    int converted;
    int result;

    if (wtext_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    *wtext_out = NULL;
    if (count_out != NULL)
    {
        *count_out = 0;
    }

    if (src == NULL || src_len == 0 || src_len < -1)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    /* 長さ取得にも厳密のフラグを指定し、割り当て前に不正な UTF-8 を拒否します。
     * see: https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar
     */
    required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src, src_len, NULL, 0);
    if (required == 0)
    {
        return conversion_result_from_last_error();
    }

    wtext = (wchar_t *)cplat_calloc((size_t)required, sizeof(*wtext));
    if (wtext == NULL)
    {
        return CPLAT_ERR_OUT_OF_MEMORY;
    }

    /* ワイド文字列へ変換する。失敗時は確保したバッファーを解放する。 */
    converted = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src, src_len, wtext, required);
    if (converted == 0)
    {
        result = conversion_result_from_last_error();
        cplat_internal_win32_free_keep_last_error(wtext);
        return result;
    }

    *wtext_out = wtext;
    if (count_out != NULL)
    {
        *count_out = converted;
    }

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_utf8_to_wstr(wchar_t *dest, const size_t dest_size, size_t *count_out, const char *utf8_text)
{
    int result;

    if (count_out != NULL)
    {
        *count_out = 0U;
    }

    result = utf8_to_wide(dest, dest_size, utf8_text, -1);
    if (result == CPLAT_OK && count_out != NULL)
    {
        *count_out = wcslen(dest) + 1U;
    }

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_wstr_to_utf8(char *dest, const size_t dest_size, size_t *count_out, const wchar_t *wtext)
{
    int result;

    if (count_out != NULL)
    {
        *count_out = 0U;
    }

    result = wide_to_utf8(dest, dest_size, wtext);
    if (result == CPLAT_OK && count_out != NULL)
    {
        *count_out = strlen(dest) + 1U;
    }

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_utf8_to_wpath(wchar_t *dest, const size_t dest_size, size_t *count_out, const char *utf8_path)
{
    return cplat_utf8_to_wstr(dest, dest_size, count_out, utf8_path);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_wpath_to_utf8(char *dest, const size_t dest_size, size_t *count_out, const wchar_t *wpath)
{
    int result;

    result = cplat_wstr_to_utf8(dest, dest_size, count_out, wpath);
    if (result != CPLAT_OK)
    {
        return result;
    }

    /* Windows API が返す '\\' を '/' に正規化します。 */
    for (char *p = dest; *p != '\0'; ++p)
    {
        if (*p == '\\')
        {
            *p = '/';
        }
    }

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_utf8_to_wstr_alloc(wchar_t **wtext_out, const char *utf8_text)
{
    return cplat_internal_utf8_to_wide_n(wtext_out, NULL, utf8_text, -1);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_wstr_to_utf8_alloc(char **utf8_text_out, const wchar_t *wtext)
{
    int required;
    char *utf8_text;
    int converted;
    int result;

    if (utf8_text_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    *utf8_text_out = NULL;

    if (wtext == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    /* 長さ取得でも対になっていないサロゲートを拒否します。
     * see: https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte
     */
    required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wtext, -1, NULL, 0, NULL, NULL);
    if (required == 0)
    {
        return conversion_result_from_last_error();
    }

    utf8_text = (char *)cplat_calloc((size_t)required, sizeof(*utf8_text));
    if (utf8_text == NULL)
    {
        return CPLAT_ERR_OUT_OF_MEMORY;
    }

    /* UTF-8 文字列へ変換する。失敗時は確保したバッファーを解放する。 */
    converted = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wtext, -1, utf8_text, required, NULL, NULL);
    if (converted == 0)
    {
        result = conversion_result_from_last_error();
        cplat_internal_win32_free_keep_last_error(utf8_text);
        return result;
    }

    *utf8_text_out = utf8_text;
    return CPLAT_OK;
}

#endif /* PLATFORM_WINDOWS */
