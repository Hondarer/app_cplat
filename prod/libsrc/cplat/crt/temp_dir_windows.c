/**
 *******************************************************************************
 *  @file           temp_dir_windows.c
 *  @brief          Windows の一時ディレクトリ取得を実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/11/24
 *  @version        1.0.0
 *
 *  GetTempPath2W / GetTempPathW による一時ディレクトリの取得を実装します。\n
 *  Linux では空の翻訳単位になります。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/crt/temp_dir_internal.h>
#include <cplat/crt/stdlib.h>

#if defined(PLATFORM_WINDOWS)

    #include <cplat/base/error_internal.h>
    #include <cplat/base/result.h>
    #include <cplat/base/windows_sdk.h>
    #include <cplat/sync/atomic.h>
    #include <cplat/sync/sync.h>

    #include <errno.h>

static void resolve_temp_path_fn(void);
static DWORD call_temp_path_api(DWORD capacity, LPWSTR buffer);

static cplat_once_flag s_resolve_once = {CPLAT_ATOMIC_INIT(0)};
static cplat_internal_temp_path_w_fn s_resolved_fn = NULL;
static cplat_internal_temp_path_w_fn s_test_resolver = NULL;

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_temp_dir_set_resolver_for_test(cplat_internal_temp_path_w_fn resolver)
{
    s_test_resolver = resolver;
    s_resolved_fn = NULL;
    cplat_atomic_store_i32(&s_resolve_once.state, 0, CPLAT_MEMORY_ORDER_RELAXED);
}

int cplat_internal_get_temp_dir_w(wchar_t **dir_out, size_t *length_out, cplat_error *detail_out)
{
    DWORD capacity;
    wchar_t *buffer;
    int attempt;

    if (dir_out == NULL)
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    *dir_out = NULL;
    if (length_out != NULL)
    {
        *length_out = 0U;
    }

    if (s_test_resolver == NULL)
    {
        cplat_call_once(&s_resolve_once, resolve_temp_path_fn);
    }

    /*
     * GetTempPathW / GetTempPath2W の戻り値は、成功時に NUL を含まない長さ、
     * 容量不足時に必要な長さ (最大 MAX_PATH + 1 = 261) となる。
     * Windows SDK (10.0.26100.0) の fileapi.h が成功を return > 0 && return < BufferLength と
     * 注釈しているため、戻り値が容量と等しい場合も切り詰め (不足) として再確保する。
     * 初回で 261 文字を 1 回で受けられるよう MAX_PATH + 2 要素から始める。
     */
    capacity = (DWORD)(MAX_PATH + 2U);
    buffer = (wchar_t *)cplat_malloc((size_t)capacity * sizeof(wchar_t));
    if (buffer == NULL)
    {
        return cplat_internal_error_report_errno(detail_out, ENOMEM);
    }

    for (attempt = 0; attempt < 3; ++attempt)
    {
        DWORD written = call_temp_path_api(capacity, buffer);
        DWORD next_capacity;
        wchar_t *next_buffer;

        if (written == 0u)
        {
            const DWORD error_code = GetLastError();

            cplat_free(buffer);
            return cplat_internal_error_report_windows_error(detail_out, error_code);
        }

        if (written < capacity)
        {
            buffer[written] = L'\0';
            *dir_out = buffer;
            if (length_out != NULL)
            {
                *length_out = (size_t)written;
            }
            return cplat_internal_error_report_success(detail_out);
        }

        if (attempt == 2)
        {
            break;
        }

        next_capacity = written + 1u;
        if (next_capacity < written)
        {
            cplat_free(buffer);
            return cplat_internal_error_report_windows_error_as(detail_out, ERROR_INSUFFICIENT_BUFFER,
                                                                CPLAT_ERR_BUFFER_TOO_SMALL);
        }

        next_buffer = (wchar_t *)cplat_malloc((size_t)next_capacity * sizeof(wchar_t));
        if (next_buffer == NULL)
        {
            cplat_free(buffer);
            return cplat_internal_error_report_errno(detail_out, ENOMEM);
        }

        cplat_free(buffer);
        buffer = next_buffer;
        capacity = next_capacity;
    }

    cplat_free(buffer);
    return cplat_internal_error_report_windows_error_as(detail_out, ERROR_INSUFFICIENT_BUFFER,
                                                        CPLAT_ERR_BUFFER_TOO_SMALL);
}

static void resolve_temp_path_fn(void)
{
    HMODULE kernel32;
    void *symbol = NULL;

    /*
     * GetTempPath2W は Windows 11 / Server 2022 以降、および 2025 年 3 月以降の更新を適用した
     * Windows 10 version 1607 以降で利用できる。同一 OS 版でも更新の有無で関数の有無が変わるため、
     * 実行時に解決し、存在しない場合は GetTempPathW へフォールバックする。
     * see: https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-gettemppath2w
     * see: https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-gettemppathw
     */
    kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (kernel32 != NULL)
    {
        symbol = (void *)GetProcAddress(kernel32, "GetTempPath2W");
    }

    s_resolved_fn = (cplat_internal_temp_path_w_fn)symbol;
}

static DWORD call_temp_path_api(DWORD capacity, LPWSTR buffer)
{
    if (s_test_resolver != NULL)
    {
        return s_test_resolver(capacity, buffer);
    }

    if (s_resolved_fn != NULL)
    {
        return s_resolved_fn(capacity, buffer);
    }

    return GetTempPathW(capacity, buffer);
}

#endif /* PLATFORM_WINDOWS */
