/**
 *******************************************************************************
 *  @file           process_api.c
 *  @brief          プロセスに関する Win32 API の UTF-8 ラッパーを実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/06/09
 *  @version        1.0.0
 *
 *  CreateProcessU を実装します。\n
 *  Linux では空の翻訳単位になります。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/win32/win32.h>
#include <cplat/crt/stdlib.h>

#if defined(PLATFORM_WINDOWS)

    #include <cplat/crt/wchar_conv.h>
    #include <cplat/win32/wchar_conv_internal.h>
    #include <stdlib.h>

/* Doxygen コメントは、ヘッダーに記載 */

BOOL CreateProcessU(const char *lpApplicationName, const char *lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes,
                    LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags,
                    LPVOID lpEnvironment, const char *lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo,
                    LPPROCESS_INFORMATION lpProcessInformation)
{
    BOOL result;
    wchar_t *wapp = NULL;
    wchar_t *wcmdline = NULL; /* CreateProcessW() が書き換え可能なバッファーを要求するため確保する */
    wchar_t *wcurdir = NULL;
    int rc = CPLAT_OK;

    if (lpApplicationName != NULL)
    {
        rc = cplat_utf8_to_wstr_alloc(&wapp, lpApplicationName);
    }

    if (rc == CPLAT_OK && lpCommandLine != NULL)
    {
        rc = cplat_utf8_to_wstr_alloc(&wcmdline, lpCommandLine);
    }

    if (rc == CPLAT_OK && lpCurrentDirectory != NULL)
    {
        rc = cplat_utf8_to_wstr_alloc(&wcurdir, lpCurrentDirectory);
    }

    if (rc != CPLAT_OK)
    {
        cplat_internal_win32_free_keep_last_error(wapp);
        cplat_internal_win32_free_keep_last_error(wcmdline);
        cplat_internal_win32_free_keep_last_error(wcurdir);
        cplat_internal_win32_set_last_error_from_result(rc);
        return FALSE;
    }

    result = CreateProcessW(wapp, wcmdline, lpProcessAttributes, lpThreadAttributes, bInheritHandles, dwCreationFlags,
                            lpEnvironment, wcurdir, lpStartupInfo, lpProcessInformation);
    cplat_internal_win32_free_keep_last_error(wapp);
    cplat_internal_win32_free_keep_last_error(wcmdline);
    cplat_internal_win32_free_keep_last_error(wcurdir);

    return result;
}

#endif /* PLATFORM_WINDOWS */
