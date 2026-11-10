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
    #include <cplat/crt/wchar_conv.h>
    #include <stdlib.h>
    #include <string.h>
    #include <wchar.h>

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
    wchar_t wbuf[32768]; /* 拡張パスの最大長に合わせた固定バッファー */
    DWORD wlen;
    char *utf8;
    size_t utf8_len;
    DWORD written;

    if (lpFilename == NULL || nSize == 0)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    wlen = GetModuleFileNameW(hModule, wbuf, (DWORD)(sizeof(wbuf) / sizeof(wbuf[0])));
    if (wlen == 0)
    {
        return 0;
    }

    /* パス区切り文字の正規化は行わない (cplat_wstr_to_utf8_alloc() を使用)。 */
    utf8 = cplat_wstr_to_utf8_alloc(wbuf);
    if (utf8 == NULL)
    {
        SetLastError(ERROR_OUTOFMEMORY);
        return 0;
    }

    utf8_len = strlen(utf8);
    if (utf8_len >= (size_t)nSize)
    {
        /* バッファー不足: 切り詰めて NUL 終端する。 */
        memcpy(lpFilename, utf8, (size_t)(nSize - 1U));
        lpFilename[nSize - 1U] = '\0';
        cplat_free(utf8);
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return nSize - 1U;
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
    char *utf8_copy;
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

    /* nNumberOfCharsToWrite [byte] だけを対象とするため、NUL 終端した複製を作ってから変換する。 */
    utf8_copy = (char *)cplat_malloc((size_t)nNumberOfCharsToWrite + 1u);
    if (utf8_copy == NULL)
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }

    memcpy(utf8_copy, lpBuffer, (size_t)nNumberOfCharsToWrite);
    utf8_copy[nNumberOfCharsToWrite] = '\0';

    wtext = cplat_utf8_to_wstr_alloc(utf8_copy);
    cplat_free(utf8_copy);

    if (wtext == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    result = WriteConsoleW(hConsoleOutput, wtext, (DWORD)wcslen(wtext), &wwritten, lpReserved);
    if (result && lpNumberOfCharsWritten != NULL && wwritten == (DWORD)wcslen(wtext))
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
