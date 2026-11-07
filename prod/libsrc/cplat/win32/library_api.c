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
    #include <stdlib.h>

/* Doxygen コメントは、ヘッダーに記載 */

HMODULE LoadLibraryU(const char *utf8_file_name)
{
    HMODULE result;
    wchar_t *wfile_name;

    wfile_name = cplat_utf8_to_wstr_alloc(utf8_file_name);
    if (wfile_name == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    result = LoadLibraryW(wfile_name);
    cplat_free(wfile_name);

    return result;
}

#endif /* PLATFORM_WINDOWS */
