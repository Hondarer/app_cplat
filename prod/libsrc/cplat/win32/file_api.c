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
    #include <assert.h>
    #include <limits.h>
    #include <stdlib.h>
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
    wchar_t *wpath;

    wpath = cplat_utf8_to_wstr_alloc(lpFileName);
    if (wpath == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return INVALID_HANDLE_VALUE;
    }

    result = CreateFileW(wpath, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition,
                         dwFlagsAndAttributes, hTemplateFile);
    cplat_free(wpath);

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

HANDLE CreateNamedPipeU(const char *lpName, DWORD dwOpenMode, DWORD dwPipeMode, DWORD nMaxInstances,
                        DWORD nOutBufferSize, DWORD nInBufferSize, DWORD nDefaultTimeOut,
                        LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
    HANDLE result;
    wchar_t *wname;

    wname = cplat_utf8_to_wstr_alloc(lpName);
    if (wname == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return INVALID_HANDLE_VALUE;
    }

    result = CreateNamedPipeW(wname, dwOpenMode, dwPipeMode, nMaxInstances, nOutBufferSize, nInBufferSize,
                              nDefaultTimeOut, lpSecurityAttributes);
    cplat_free(wname);

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

DWORD GetModuleFileNameU(HMODULE hModule, char *lpFilename, DWORD nSize)
{
    wchar_t *wbuf;
    DWORD wlen;
    char *utf8;
    size_t utf8_len;
    DWORD written;

    if (lpFilename == NULL || nSize == 0)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    wbuf = (wchar_t *)cplat_malloc((size_t)CPLAT_WIN32_LONG_PATH_WCHAR_CAP * sizeof(wchar_t));
    if (wbuf == NULL)
    {
        SetLastError(ERROR_OUTOFMEMORY);
        return 0;
    }

    wlen = GetModuleFileNameW(hModule, wbuf, (DWORD)CPLAT_WIN32_LONG_PATH_WCHAR_CAP);
    if (wlen == 0 || wlen >= (DWORD)CPLAT_WIN32_LONG_PATH_WCHAR_CAP)
    {
        /* 解放の処理が最終エラーを変えても、GetModuleFileNameW() の最終エラーを呼び出し元へ伝える。 */
        DWORD err_code = GetLastError();

        cplat_free(wbuf);
        SetLastError(err_code);
        return 0;
    }

    wbuf[wlen] = L'\0';

    /* パス区切り文字の正規化は行わない (cplat_wstr_to_utf8_alloc() を使用)。 */
    utf8 = cplat_wstr_to_utf8_alloc(wbuf);
    cplat_free(wbuf);
    if (utf8 == NULL)
    {
        SetLastError(ERROR_OUTOFMEMORY);
        return 0;
    }

    utf8_len = strlen(utf8);
    if (utf8_len >= (size_t)nSize)
    {
        size_t keep = cplat_internal_utf8_trimmed_length(utf8, (size_t)nSize - 1U);

        memcpy(lpFilename, utf8, keep);
        lpFilename[keep] = '\0';
        cplat_free(utf8);
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return nSize;
    }

    memcpy(lpFilename, utf8, utf8_len + 1U);
    written = (DWORD)utf8_len;
    cplat_free(utf8);

    return written;
}

/* Doxygen コメントは、ヘッダーに記載 */

BOOL WriteConsoleU(HANDLE hConsoleOutput, const char *lpBuffer, DWORD nNumberOfCharsToWrite,
                   LPDWORD lpNumberOfCharsWritten, LPVOID lpReserved)
{
    BOOL result;
    int wcount;
    wchar_t *wtext;
    DWORD wwritten = 0;

    if (lpNumberOfCharsWritten != NULL)
    {
        *lpNumberOfCharsWritten = 0;
    }

    if (lpBuffer == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (nNumberOfCharsToWrite > (DWORD)INT_MAX)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (nNumberOfCharsToWrite == 0U)
    {
        result = WriteConsoleW(hConsoleOutput, L"", 0, &wwritten, lpReserved);
        return result;
    }

    /*
     * 入力は NUL 終端とは限らず、途中に NUL を含み得るため、バイト数を直接指定して変換する。
     * see: https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar
     */
    wcount = MultiByteToWideChar(CP_UTF8, 0, lpBuffer, (int)nNumberOfCharsToWrite, NULL, 0);
    if (wcount <= 0)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    wtext = (wchar_t *)cplat_malloc((size_t)wcount * sizeof(wchar_t));
    if (wtext == NULL)
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }

    if (MultiByteToWideChar(CP_UTF8, 0, lpBuffer, (int)nNumberOfCharsToWrite, wtext, wcount) <= 0)
    {
        cplat_free(wtext);
        SetLastError(ERROR_INVALID_PARAMETER);
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

    cplat_free(wtext);

    return result;
}

/*
 * UTF-16 文字列を UTF-8 へ変換して固定長バッファーへ書き込む。
 * 成功時、または utf8_buf が NULL か size が 0 のときは TRUE を返す。
 * 変換失敗時は ERROR_INVALID_PARAMETER、収まらない場合は ERROR_INSUFFICIENT_BUFFER を設定して FALSE を返す。
 */
static BOOL copy_wstr_as_utf8(char *utf8_buf, DWORD size, const wchar_t *wtext)
{
    char *utf8;
    size_t utf8_len;

    if (utf8_buf == NULL || size == 0u)
    {
        return TRUE;
    }

    utf8 = cplat_wstr_to_utf8_alloc(wtext);
    if (utf8 == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    utf8_len = strlen(utf8);
    if (utf8_len + 1u > (size_t)size)
    {
        cplat_free(utf8);
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }

    memcpy(utf8_buf, utf8, utf8_len + 1u);
    cplat_free(utf8);

    return TRUE;
}

/* Doxygen コメントは、ヘッダーに記載 */

BOOL GetVolumePathNameU(const char *lpszFileName, char *lpszVolumePathName, DWORD cchBufferLength)
{
    wchar_t *wpath;
    wchar_t wroot[PLATFORM_PATH_MAX];

    wpath = cplat_utf8_to_wstr_alloc(lpszFileName);
    if (wpath == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (!GetVolumePathNameW(wpath, wroot, (DWORD)(sizeof(wroot) / sizeof(wroot[0]))))
    {
        cplat_free(wpath);
        return FALSE;
    }

    cplat_free(wpath);

    return copy_wstr_as_utf8(lpszVolumePathName, cchBufferLength, wroot);
}

/* Doxygen コメントは、ヘッダーに記載 */

BOOL GetVolumeInformationU(const char *lpRootPathName, char *lpVolumeNameBuffer, DWORD nVolumeNameSize,
                           LPDWORD lpVolumeSerialNumber, LPDWORD lpMaximumComponentLength, LPDWORD lpFileSystemFlags,
                           char *lpFileSystemNameBuffer, DWORD nFileSystemNameSize)
{
    wchar_t *wroot = NULL;
    wchar_t wvolume[MAX_PATH + 1];
    wchar_t wfs[MAX_PATH + 1];

    if (lpRootPathName != NULL)
    {
        wroot = cplat_utf8_to_wstr_alloc(lpRootPathName);
        if (wroot == NULL)
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }
    }

    wvolume[0] = L'\0';
    wfs[0] = L'\0';

    if (!GetVolumeInformationW(wroot, wvolume, (DWORD)(sizeof(wvolume) / sizeof(wvolume[0])), lpVolumeSerialNumber,
                               lpMaximumComponentLength, lpFileSystemFlags, wfs, (DWORD)(sizeof(wfs) / sizeof(wfs[0]))))
    {
        cplat_free(wroot);
        return FALSE;
    }

    cplat_free(wroot);

    if (!copy_wstr_as_utf8(lpVolumeNameBuffer, nVolumeNameSize, wvolume))
    {
        return FALSE;
    }

    return copy_wstr_as_utf8(lpFileSystemNameBuffer, nFileSystemNameSize, wfs);
}

#endif /* PLATFORM_WINDOWS */
