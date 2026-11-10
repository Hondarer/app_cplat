/**
 *******************************************************************************
 *  @file           wchar_conv.c
 *  @brief          Windows で UTF-8 とワイド文字列を相互変換する API を実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/06/08
 *  @version        1.0.0
 *
 *  Windows 上で UTF-8 文字列とワイド文字列 (UTF-16LE) を相互変換する
 *  関数を提供します。\n
 *  Linux では空の翻訳単位になります。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/crt/wchar_conv.h>
#include <cplat/crt/stdlib.h>

#if defined(PLATFORM_WINDOWS)

    #include <cplat/base/windows_sdk.h>
    #include <limits.h>
    #include <stdlib.h>

/**
 *  @brief          UTF-8 をワイド文字列へ変換します (スラッシュ正規化なし)。
 *  @param[out]     dest       変換先。
 *  @param[in]      dest_size  @p dest の要素数。
 *  @param[in]      utf8_text  変換元。
 *  @return         変換後の文字数 (NUL 込み)。失敗時は -1 です。
 */
static int utf8_to_wide(wchar_t *dest, const size_t dest_size, const char *utf8_text)
{
    int converted;

    if (utf8_text == NULL || dest == NULL || dest_size == 0 || dest_size > (size_t)INT_MAX)
    {
        return -1;
    }

    /*
     * MultiByteToWideChar() は失敗時に 0 を返す。公開契約の失敗値は -1 である。
     * see: https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar
     */
    converted = MultiByteToWideChar(CP_UTF8, 0, utf8_text, -1, dest, (int)dest_size);
    if (converted <= 0)
    {
        return -1;
    }

    return converted;
}

/**
 *  @brief          ワイド文字列を UTF-8 へ変換します (スラッシュ正規化なし)。
 *  @param[out]     dest       変換先。
 *  @param[in]      dest_size  @p dest のバイト数。
 *  @param[in]      wtext      変換元。
 *  @return         変換後のバイト数 (NUL 込み)。失敗時は -1 です。
 */
static int wide_to_utf8(char *dest, const size_t dest_size, const wchar_t *wtext)
{
    int converted;

    if (dest == NULL || dest_size == 0 || wtext == NULL || dest_size > (size_t)INT_MAX)
    {
        return -1;
    }

    /*
     * WideCharToMultiByte() は失敗時に 0 を返す。公開契約の失敗値は -1 である。
     * see: https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-widechartomultibyte
     */
    converted = WideCharToMultiByte(CP_UTF8, 0, wtext, -1, dest, (int)dest_size, NULL, NULL);
    if (converted <= 0)
    {
        return -1;
    }

    return converted;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_utf8_to_wstr(wchar_t *dest, const size_t dest_size, const char *utf8_text)
{
    return utf8_to_wide(dest, dest_size, utf8_text);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_wstr_to_utf8(char *dest, const size_t dest_size, const wchar_t *wtext)
{
    return wide_to_utf8(dest, dest_size, wtext);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_utf8_to_wpath(wchar_t *dest, const size_t dest_size, const char *utf8_path)
{
    return utf8_to_wide(dest, dest_size, utf8_path);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_wpath_to_utf8(char *dest, const size_t dest_size, const wchar_t *wpath)
{
    int n;

    n = wide_to_utf8(dest, dest_size, wpath);
    if (n <= 0)
    {
        return -1;
    }

    /* Windows API が返す '\\' を '/' に正規化する。 */
    for (char *p = dest; *p != '\0'; ++p)
    {
        if (*p == '\\')
        {
            *p = '/';
        }
    }

    return n;
}

/* Doxygen コメントは、ヘッダーに記載 */

wchar_t *cplat_utf8_to_wstr_alloc(const char *utf8_text)
{
    wchar_t *wtext;
    int wtext_count;

    if (utf8_text == NULL)
    {
        return NULL;
    }

    /* 変換に必要なバッファー要素数 (終端 NUL を含む) を算出する。 */
    wtext_count = MultiByteToWideChar(CP_UTF8, 0, utf8_text, -1, NULL, 0);
    if (wtext_count <= 0)
    {
        return NULL;
    }

    wtext = (wchar_t *)cplat_calloc((size_t)wtext_count, sizeof(*wtext));
    if (wtext == NULL)
    {
        return NULL;
    }

    /* ワイド文字列へ変換する。失敗時は確保したバッファーを解放する。 */
    if (MultiByteToWideChar(CP_UTF8, 0, utf8_text, -1, wtext, wtext_count) <= 0)
    {
        cplat_free(wtext);
        return NULL;
    }

    return wtext;
}

/* Doxygen コメントは、ヘッダーに記載 */

char *cplat_wstr_to_utf8_alloc(const wchar_t *wtext)
{
    int utf8_count;
    char *utf8_text;

    if (wtext == NULL)
    {
        return NULL;
    }

    /* 変換に必要なバッファー バイト数 (終端 NUL を含む) を算出する。 */
    utf8_count = WideCharToMultiByte(CP_UTF8, 0, wtext, -1, NULL, 0, NULL, NULL);
    if (utf8_count <= 0)
    {
        return NULL;
    }

    utf8_text = (char *)cplat_calloc((size_t)utf8_count, sizeof(*utf8_text));
    if (utf8_text == NULL)
    {
        return NULL;
    }

    /* UTF-8 文字列へ変換する。失敗時は確保したバッファーを解放する。 */
    if (WideCharToMultiByte(CP_UTF8, 0, wtext, -1, utf8_text, utf8_count, NULL, NULL) <= 0)
    {
        cplat_free(utf8_text);
        return NULL;
    }

    return utf8_text;
}

#endif /* PLATFORM_WINDOWS */
