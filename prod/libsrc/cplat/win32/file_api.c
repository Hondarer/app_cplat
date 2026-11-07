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

HANDLE CreateFileU(const char *utf8_path, DWORD desired_access, DWORD share_mode,
                   LPSECURITY_ATTRIBUTES security_attributes, DWORD creation_disposition, DWORD flags_and_attributes,
                   HANDLE template_file)
{
    HANDLE result;
    wchar_t *wpath;

    wpath = cplat_utf8_to_wstr_alloc(utf8_path);
    if (wpath == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return INVALID_HANDLE_VALUE;
    }

    result = CreateFileW(wpath, desired_access, share_mode, security_attributes, creation_disposition,
                         flags_and_attributes, template_file);
    cplat_free(wpath);

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

HANDLE CreateNamedPipeU(const char *utf8_name, DWORD open_mode, DWORD pipe_mode, DWORD max_instances,
                        DWORD out_buffer_size, DWORD in_buffer_size, DWORD default_timeout,
                        LPSECURITY_ATTRIBUTES security_attributes)
{
    HANDLE result;
    wchar_t *wname;

    wname = cplat_utf8_to_wstr_alloc(utf8_name);
    if (wname == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return INVALID_HANDLE_VALUE;
    }

    result = CreateNamedPipeW(wname, open_mode, pipe_mode, max_instances, out_buffer_size, in_buffer_size,
                              default_timeout, security_attributes);
    cplat_free(wname);

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

DWORD GetModuleFileNameU(HMODULE module, char *utf8_buf, DWORD size)
{
    wchar_t wbuf[32768]; /* 拡張パスの最大長に合わせた固定バッファー */
    DWORD wlen;
    char *utf8;
    size_t utf8_len;
    DWORD written;

    if (utf8_buf == NULL || size == 0)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    wlen = GetModuleFileNameW(module, wbuf, (DWORD)(sizeof(wbuf) / sizeof(wbuf[0])));
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
    if (utf8_len >= (size_t)size)
    {
        /* バッファー不足: 切り詰めて NUL 終端する。 */
        memcpy(utf8_buf, utf8, (size_t)(size - 1U));
        utf8_buf[size - 1U] = '\0';
        cplat_free(utf8);
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return size - 1U;
    }

    memcpy(utf8_buf, utf8, utf8_len + 1U);
    written = (DWORD)utf8_len;
    cplat_free(utf8);

    return written;
}

/* Doxygen コメントは、ヘッダーに記載 */

BOOL WriteConsoleU(HANDLE console, const char *utf8_text, DWORD utf8_length, DWORD *written_length, void *reserved)
{
    BOOL result;
    char *utf8_copy;
    wchar_t *wtext;
    DWORD wwritten = 0;

    if (written_length != NULL)
    {
        *written_length = 0;
    }

    if (utf8_text == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    /* utf8_length [byte] だけを対象とするため、NUL 終端した複製を作ってから変換する。 */
    utf8_copy = (char *)cplat_malloc((size_t)utf8_length + 1u);
    if (utf8_copy == NULL)
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }

    memcpy(utf8_copy, utf8_text, (size_t)utf8_length);
    utf8_copy[utf8_length] = '\0';

    wtext = cplat_utf8_to_wstr_alloc(utf8_copy);
    cplat_free(utf8_copy);

    if (wtext == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    result = WriteConsoleW(console, wtext, (DWORD)wcslen(wtext), &wwritten, reserved);
    if (result && written_length != NULL && wwritten == (DWORD)wcslen(wtext))
    {
        *written_length = utf8_length;
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

BOOL GetVolumePathNameU(const char *utf8_path, char *utf8_volume_root, DWORD size)
{
    wchar_t *wpath;
    wchar_t wroot[PLATFORM_PATH_MAX];

    wpath = cplat_utf8_to_wstr_alloc(utf8_path);
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

    return copy_wstr_as_utf8(utf8_volume_root, size, wroot);
}

/* Doxygen コメントは、ヘッダーに記載 */

BOOL GetVolumeInformationU(const char *utf8_root_path, char *utf8_volume_name, DWORD volume_name_size,
                           DWORD *serial_number, DWORD *max_component_length, DWORD *file_system_flags,
                           char *utf8_file_system_name, DWORD file_system_name_size)
{
    wchar_t *wroot = NULL;
    wchar_t wvolume[MAX_PATH + 1];
    wchar_t wfs[MAX_PATH + 1];

    if (utf8_root_path != NULL)
    {
        wroot = cplat_utf8_to_wstr_alloc(utf8_root_path);
        if (wroot == NULL)
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }
    }

    wvolume[0] = L'\0';
    wfs[0] = L'\0';

    if (!GetVolumeInformationW(wroot, wvolume, (DWORD)(sizeof(wvolume) / sizeof(wvolume[0])), serial_number,
                               max_component_length, file_system_flags, wfs, (DWORD)(sizeof(wfs) / sizeof(wfs[0]))))
    {
        cplat_free(wroot);
        return FALSE;
    }

    cplat_free(wroot);

    if (!copy_wstr_as_utf8(utf8_volume_name, volume_name_size, wvolume))
    {
        return FALSE;
    }

    return copy_wstr_as_utf8(utf8_file_system_name, file_system_name_size, wfs);
}

#endif /* PLATFORM_WINDOWS */
