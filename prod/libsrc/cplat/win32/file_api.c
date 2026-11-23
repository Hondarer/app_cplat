/**
 *******************************************************************************
 *  @file           file_api.c
 *  @brief          ファイルとパイプに関する Win32 API の UTF-8 ラッパーを実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/06/09
 *  @version        1.0.0
 *
 *  CreateFileU / CreateNamedPipeU / GetModuleFileNameU を実装します。\n
 *  Linux では空の翻訳単位になります。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/win32/win32.h>
#include <cplat/crt/stdlib.h>

#if defined(PLATFORM_WINDOWS)

    #include <cplat/crt/path.h>
    #include <cplat/crt/utf8.h>
    #include <cplat/crt/wchar_conv.h>
    #include <cplat/win32/wchar_conv_internal.h>
    #include <limits.h>
    #include <string.h>
    #include <wchar.h>

    /* Windows の拡張パス最大長 (32767 文字と終端 NUL) に合わせた中間 wchar_t 配列の要素数 [wchar_t]。 */
    #define CPLAT_WIN32_LONG_PATH_WCHAR_CAP 32768U

static_assert(CPLAT_WIN32_LONG_PATH_WCHAR_CAP > PLATFORM_PATH_MAX, "long path buffer must exceed PLATFORM_PATH_MAX");

/* Doxygen コメントは、ヘッダーに記載 */

HANDLE CreateFileU(const char *lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                   LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
                   HANDLE hTemplateFile)
{
    HANDLE result;
    wchar_t *wpath = NULL;
    int rc;

    rc = cplat_utf8_to_wstr_alloc(&wpath, lpFileName);
    if (rc != CPLAT_OK)
    {
        cplat_internal_win32_set_last_error_from_result(rc);
        return INVALID_HANDLE_VALUE;
    }

    result = CreateFileW(wpath, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition,
                         dwFlagsAndAttributes, hTemplateFile);
    cplat_internal_win32_free_keep_last_error(wpath);

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

HANDLE CreateNamedPipeU(const char *lpName, DWORD dwOpenMode, DWORD dwPipeMode, DWORD nMaxInstances,
                        DWORD nOutBufferSize, DWORD nInBufferSize, DWORD nDefaultTimeOut,
                        LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
    HANDLE result;
    wchar_t *wname = NULL;
    int rc;

    rc = cplat_utf8_to_wstr_alloc(&wname, lpName);
    if (rc != CPLAT_OK)
    {
        cplat_internal_win32_set_last_error_from_result(rc);
        return INVALID_HANDLE_VALUE;
    }

    result = CreateNamedPipeW(wname, dwOpenMode, dwPipeMode, nMaxInstances, nOutBufferSize, nInBufferSize,
                              nDefaultTimeOut, lpSecurityAttributes);
    cplat_internal_win32_free_keep_last_error(wname);

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

DWORD GetModuleFileNameU(HMODULE hModule, char *lpFilename, DWORD nSize)
{
    wchar_t *wbuf;
    DWORD wlen;
    char *utf8 = NULL;
    size_t utf8_len;
    DWORD written;
    int rc;

    if (lpFilename == NULL || nSize == 0U)
    {
        cplat_internal_win32_set_last_error_from_result(CPLAT_ERR_INVALID_ARGUMENT);
        return 0U;
    }

    wbuf = (wchar_t *)cplat_malloc((size_t)CPLAT_WIN32_LONG_PATH_WCHAR_CAP * sizeof(wchar_t));
    if (wbuf == NULL)
    {
        cplat_internal_win32_set_last_error_from_result(CPLAT_ERR_OUT_OF_MEMORY);
        return 0U;
    }

    wlen = GetModuleFileNameW(hModule, wbuf, (DWORD)CPLAT_WIN32_LONG_PATH_WCHAR_CAP);
    if (wlen == 0U || wlen >= (DWORD)CPLAT_WIN32_LONG_PATH_WCHAR_CAP)
    {
        /* 解放が最終エラーを変えても、GetModuleFileNameW() の最終エラーを呼び出し元へ伝えます。 */
        cplat_internal_win32_free_keep_last_error(wbuf);
        return 0U;
    }

    wbuf[wlen] = L'\0';

    /* パス区切り文字は正規化せず、UTF-16 の最終エラーを厳密に分類します。 */
    rc = cplat_wstr_to_utf8_alloc(&utf8, wbuf);
    cplat_internal_win32_free_keep_last_error(wbuf);
    if (rc != CPLAT_OK)
    {
        cplat_internal_win32_set_last_error_from_result(rc);
        return 0U;
    }

    utf8_len = strlen(utf8);
    if (utf8_len >= (size_t)nSize)
    {
        size_t keep = cplat_internal_utf8_trimmed_length(utf8, (size_t)nSize - 1U);

        memcpy(lpFilename, utf8, keep);
        lpFilename[keep] = '\0';
        cplat_internal_win32_free_keep_last_error(utf8);
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return nSize;
    }

    memcpy(lpFilename, utf8, utf8_len + 1U);
    written = (DWORD)utf8_len;
    cplat_internal_win32_free_keep_last_error(utf8);

    return written;
}

/* Doxygen コメントは、ヘッダーに記載 */

BOOL WriteConsoleU(HANDLE hConsoleOutput, const char *lpBuffer, DWORD nNumberOfCharsToWrite,
                   LPDWORD lpNumberOfCharsWritten, LPVOID lpReserved)
{
    BOOL result;
    int wcount = 0;
    wchar_t *wtext = NULL;
    DWORD wwritten = 0U;
    int rc;

    if (lpNumberOfCharsWritten != NULL)
    {
        *lpNumberOfCharsWritten = 0U;
    }

    if (lpBuffer == NULL || nNumberOfCharsToWrite > (DWORD)INT_MAX)
    {
        cplat_internal_win32_set_last_error_from_result(CPLAT_ERR_INVALID_ARGUMENT);
        return FALSE;
    }

    if (nNumberOfCharsToWrite == 0U)
    {
        return WriteConsoleW(hConsoleOutput, L"", 0U, &wwritten, lpReserved);
    }

    /*
     * 入力は NUL 終端とは限らず、途中に NUL を含み得るため、バイト数を直接指定して変換する。
     * see: https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar
     */
    rc = cplat_internal_utf8_to_wide_n(&wtext, &wcount, lpBuffer, (int)nNumberOfCharsToWrite);
    if (rc != CPLAT_OK)
    {
        cplat_internal_win32_set_last_error_from_result(rc);
        return FALSE;
    }

    result = WriteConsoleW(hConsoleOutput, wtext, (DWORD)wcount, &wwritten, lpReserved);
    /*
     * WriteConsoleW() は、書き込んだ UTF-16 の単位数を返す。
     * UTF-16 の単位数と UTF-8 のバイト数は比例しないため、書き込んだ単位数からバイト数を換算せず、
     * 部分書き込みでは 0 を格納する。
     * see: https://learn.microsoft.com/en-us/windows/console/writeconsole
     */
    if (result && lpNumberOfCharsWritten != NULL && wwritten == (DWORD)wcount)
    {
        *lpNumberOfCharsWritten = nNumberOfCharsToWrite;
    }

    cplat_internal_win32_free_keep_last_error(wtext);

    return result;
}

/*
 * UTF-16 文字列を UTF-8 へ変換して固定長バッファーへ書き込みます。
 * utf8_buf が NULL、または size が 0 のときは CPLAT_OK を返します。
 * 変換に失敗したときは対応する結果コードを返し、収まらないときは CPLAT_ERR_BUFFER_TOO_SMALL を返します。
 */
static int copy_wstr_as_utf8(char *utf8_buf, const DWORD size, const wchar_t *wtext)
{
    char *utf8 = NULL;
    size_t utf8_len;
    int rc;

    if (utf8_buf == NULL || size == 0U)
    {
        return CPLAT_OK;
    }

    rc = cplat_wstr_to_utf8_alloc(&utf8, wtext);
    if (rc != CPLAT_OK)
    {
        return rc;
    }

    utf8_len = strlen(utf8);
    if (utf8_len + 1U > (size_t)size)
    {
        cplat_internal_win32_free_keep_last_error(utf8);
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    memcpy(utf8_buf, utf8, utf8_len + 1U);
    cplat_internal_win32_free_keep_last_error(utf8);

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

BOOL GetVolumePathNameU(const char *lpszFileName, char *lpszVolumePathName, DWORD cchBufferLength)
{
    wchar_t *wpath = NULL;
    wchar_t wroot[PLATFORM_PATH_MAX];
    int rc;

    rc = cplat_utf8_to_wstr_alloc(&wpath, lpszFileName);
    if (rc != CPLAT_OK)
    {
        cplat_internal_win32_set_last_error_from_result(rc);
        return FALSE;
    }

    if (!GetVolumePathNameW(wpath, wroot, (DWORD)(sizeof(wroot) / sizeof(wroot[0]))))
    {
        cplat_internal_win32_free_keep_last_error(wpath);
        return FALSE;
    }

    cplat_internal_win32_free_keep_last_error(wpath);

    rc = copy_wstr_as_utf8(lpszVolumePathName, cchBufferLength, wroot);
    if (rc != CPLAT_OK)
    {
        cplat_internal_win32_set_last_error_from_result(rc);
        return FALSE;
    }

    return TRUE;
}

/* Doxygen コメントは、ヘッダーに記載 */

BOOL GetVolumeInformationU(const char *lpRootPathName, char *lpVolumeNameBuffer, DWORD nVolumeNameSize,
                           LPDWORD lpVolumeSerialNumber, LPDWORD lpMaximumComponentLength, LPDWORD lpFileSystemFlags,
                           char *lpFileSystemNameBuffer, DWORD nFileSystemNameSize)
{
    wchar_t *wroot = NULL;
    wchar_t wvolume[MAX_PATH + 1];
    wchar_t wfs[MAX_PATH + 1];
    int rc = CPLAT_OK;

    if (lpRootPathName != NULL)
    {
        rc = cplat_utf8_to_wstr_alloc(&wroot, lpRootPathName);
        if (rc != CPLAT_OK)
        {
            cplat_internal_win32_set_last_error_from_result(rc);
            return FALSE;
        }
    }

    wvolume[0] = L'\0';
    wfs[0] = L'\0';

    if (!GetVolumeInformationW(wroot, wvolume, (DWORD)(sizeof(wvolume) / sizeof(wvolume[0])), lpVolumeSerialNumber,
                               lpMaximumComponentLength, lpFileSystemFlags, wfs, (DWORD)(sizeof(wfs) / sizeof(wfs[0]))))
    {
        cplat_internal_win32_free_keep_last_error(wroot);
        return FALSE;
    }

    cplat_internal_win32_free_keep_last_error(wroot);

    rc = copy_wstr_as_utf8(lpVolumeNameBuffer, nVolumeNameSize, wvolume);
    if (rc != CPLAT_OK)
    {
        cplat_internal_win32_set_last_error_from_result(rc);
        return FALSE;
    }

    rc = copy_wstr_as_utf8(lpFileSystemNameBuffer, nFileSystemNameSize, wfs);
    if (rc != CPLAT_OK)
    {
        cplat_internal_win32_set_last_error_from_result(rc);
        return FALSE;
    }

    return TRUE;
}

#endif /* PLATFORM_WINDOWS */
