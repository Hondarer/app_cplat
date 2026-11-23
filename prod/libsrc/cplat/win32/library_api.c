/**
 *******************************************************************************
 *  @file           library_api.c
 *  @brief          ライブラリのロードに関する Win32 API の UTF-8 ラッパーを実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/06/09
 *  @version        1.0.0
 *
 *  LoadLibraryU を実装します。\n
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

/* Doxygen コメントは、ヘッダーに記載 */

HMODULE LoadLibraryU(const char *lpLibFileName)
{
    HMODULE result;
    wchar_t *wfile_name = NULL;
    int rc;

    rc = cplat_utf8_to_wstr_alloc(&wfile_name, lpLibFileName);
    if (rc != CPLAT_OK)
    {
        cplat_internal_win32_set_last_error_from_result(rc);
        return NULL;
    }

    result = LoadLibraryW(wfile_name);
    cplat_internal_win32_free_keep_last_error(wfile_name);

    return result;
}

#endif /* PLATFORM_WINDOWS */
