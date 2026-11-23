/**
 *******************************************************************************
 *  @file           service_api.c
 *  @brief          SCM とサービスに関する Win32 API の UTF-8 ラッパーを実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/06/09
 *  @version        1.0.0
 *
 *  OpenSCManagerU / CreateServiceU / OpenServiceU / ChangeServiceConfig2U /
 *  RegisterServiceCtrlHandlerExU / StartServiceCtrlDispatcherU を実装します。\n
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

SC_HANDLE OpenSCManagerU(const char *lpMachineName, const char *lpDatabaseName, DWORD dwDesiredAccess)
{
    SC_HANDLE result;
    wchar_t *wmachine = NULL;
    wchar_t *wdatabase = NULL;
    int rc = CPLAT_OK;

    if (lpMachineName != NULL)
    {
        rc = cplat_utf8_to_wstr_alloc(&wmachine, lpMachineName);
    }
    if (rc == CPLAT_OK && lpDatabaseName != NULL)
    {
        rc = cplat_utf8_to_wstr_alloc(&wdatabase, lpDatabaseName);
    }

    if (rc != CPLAT_OK)
    {
        cplat_internal_win32_free_keep_last_error(wmachine);
        cplat_internal_win32_free_keep_last_error(wdatabase);
        cplat_internal_win32_set_last_error_from_result(rc);
        return NULL;
    }

    result = OpenSCManagerW(wmachine, wdatabase, dwDesiredAccess);
    cplat_internal_win32_free_keep_last_error(wmachine);
    cplat_internal_win32_free_keep_last_error(wdatabase);

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

SC_HANDLE CreateServiceU(SC_HANDLE hSCManager, const char *lpServiceName, const char *lpDisplayName,
                         DWORD dwDesiredAccess, DWORD dwServiceType, DWORD dwStartType, DWORD dwErrorControl,
                         const char *lpBinaryPathName, const char *lpLoadOrderGroup, LPDWORD lpdwTagId,
                         const char *lpDependencies, const char *lpServiceStartName, const char *lpPassword)
{
    SC_HANDLE result;
    wchar_t *wservice_name = NULL;
    wchar_t *wdisplay_name = NULL;
    wchar_t *wbinary_path_name = NULL;
    wchar_t *wload_order_group = NULL;
    wchar_t *wdependencies = NULL;
    wchar_t *wservice_start_name = NULL;
    wchar_t *wpassword = NULL;
    int rc = CPLAT_OK;

    if (lpServiceName != NULL)
    {
        rc = cplat_utf8_to_wstr_alloc(&wservice_name, lpServiceName);
    }
    if (rc == CPLAT_OK && lpDisplayName != NULL)
    {
        rc = cplat_utf8_to_wstr_alloc(&wdisplay_name, lpDisplayName);
    }
    if (rc == CPLAT_OK && lpBinaryPathName != NULL)
    {
        rc = cplat_utf8_to_wstr_alloc(&wbinary_path_name, lpBinaryPathName);
    }
    if (rc == CPLAT_OK && lpLoadOrderGroup != NULL)
    {
        rc = cplat_utf8_to_wstr_alloc(&wload_order_group, lpLoadOrderGroup);
    }
    if (rc == CPLAT_OK && lpDependencies != NULL)
    {
        rc = cplat_utf8_to_wstr_alloc(&wdependencies, lpDependencies);
    }
    if (rc == CPLAT_OK && lpServiceStartName != NULL)
    {
        rc = cplat_utf8_to_wstr_alloc(&wservice_start_name, lpServiceStartName);
    }
    if (rc == CPLAT_OK && lpPassword != NULL)
    {
        rc = cplat_utf8_to_wstr_alloc(&wpassword, lpPassword);
    }

    if (rc != CPLAT_OK)
    {
        cplat_internal_win32_free_keep_last_error(wservice_name);
        cplat_internal_win32_free_keep_last_error(wdisplay_name);
        cplat_internal_win32_free_keep_last_error(wbinary_path_name);
        cplat_internal_win32_free_keep_last_error(wload_order_group);
        cplat_internal_win32_free_keep_last_error(wdependencies);
        cplat_internal_win32_free_keep_last_error(wservice_start_name);
        cplat_internal_win32_free_keep_last_error(wpassword);
        cplat_internal_win32_set_last_error_from_result(rc);
        return NULL;
    }

    result = CreateServiceW(hSCManager, wservice_name, wdisplay_name, dwDesiredAccess, dwServiceType, dwStartType,
                            dwErrorControl, wbinary_path_name, wload_order_group, lpdwTagId, wdependencies,
                            wservice_start_name, wpassword);
    cplat_internal_win32_free_keep_last_error(wservice_name);
    cplat_internal_win32_free_keep_last_error(wdisplay_name);
    cplat_internal_win32_free_keep_last_error(wbinary_path_name);
    cplat_internal_win32_free_keep_last_error(wload_order_group);
    cplat_internal_win32_free_keep_last_error(wdependencies);
    cplat_internal_win32_free_keep_last_error(wservice_start_name);
    cplat_internal_win32_free_keep_last_error(wpassword);

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

SC_HANDLE OpenServiceU(SC_HANDLE hSCManager, const char *lpServiceName, DWORD dwDesiredAccess)
{
    SC_HANDLE result;
    wchar_t *wservice_name = NULL;
    int rc;

    rc = cplat_utf8_to_wstr_alloc(&wservice_name, lpServiceName);
    if (rc != CPLAT_OK)
    {
        cplat_internal_win32_set_last_error_from_result(rc);
        return NULL;
    }

    result = OpenServiceW(hSCManager, wservice_name, dwDesiredAccess);
    cplat_internal_win32_free_keep_last_error(wservice_name);

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

BOOL ChangeServiceConfig2U(SC_HANDLE hService, DWORD dwInfoLevel, LPVOID lpInfo)
{
    BOOL result;
    wchar_t *wtext = NULL;
    const SERVICE_DESCRIPTIONA *desc;
    SERVICE_DESCRIPTIONW desc_w;
    int rc = CPLAT_OK;

    if (dwInfoLevel != SERVICE_CONFIG_DESCRIPTION || lpInfo == NULL)
    {
        cplat_internal_win32_set_last_error_from_result(CPLAT_ERR_INVALID_ARGUMENT);
        return FALSE;
    }

    desc = (const SERVICE_DESCRIPTIONA *)lpInfo;
    if (desc->lpDescription != NULL)
    {
        rc = cplat_utf8_to_wstr_alloc(&wtext, desc->lpDescription);
        if (rc != CPLAT_OK)
        {
            cplat_internal_win32_set_last_error_from_result(rc);
            return FALSE;
        }
    }

    desc_w.lpDescription = wtext;
    result = ChangeServiceConfig2W(hService, SERVICE_CONFIG_DESCRIPTION, &desc_w);
    cplat_internal_win32_free_keep_last_error(wtext);

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

SERVICE_STATUS_HANDLE RegisterServiceCtrlHandlerExU(const char *lpServiceName, LPHANDLER_FUNCTION_EX lpHandlerProc,
                                                    LPVOID lpContext)
{
    SERVICE_STATUS_HANDLE result;
    wchar_t *wservice_name = NULL;
    int rc;

    rc = cplat_utf8_to_wstr_alloc(&wservice_name, lpServiceName);
    if (rc != CPLAT_OK)
    {
        cplat_internal_win32_set_last_error_from_result(rc);
        return NULL;
    }

    result = RegisterServiceCtrlHandlerExW(wservice_name, lpHandlerProc, lpContext);
    cplat_internal_win32_free_keep_last_error(wservice_name);

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

BOOL StartServiceCtrlDispatcherU(const SERVICE_TABLE_ENTRYU *lpServiceStartTable)
{
    size_t count;
    SERVICE_TABLE_ENTRYW *w_table;
    size_t i;
    BOOL result;
    int rc = CPLAT_OK;

    if (lpServiceStartTable == NULL)
    {
        cplat_internal_win32_set_last_error_from_result(CPLAT_ERR_INVALID_ARGUMENT);
        return FALSE;
    }

    /* テーブルのエントリ数を数える (終端: lpServiceName == NULL)。 */
    count = 0;
    while (lpServiceStartTable[count].lpServiceName != NULL)
    {
        count++;
    }

    /* W テーブルを確保する (終端要素を含むため count + 1)。 */
    w_table = (SERVICE_TABLE_ENTRYW *)cplat_calloc(count + 1U, sizeof(SERVICE_TABLE_ENTRYW));
    if (w_table == NULL)
    {
        cplat_internal_win32_set_last_error_from_result(CPLAT_ERR_OUT_OF_MEMORY);
        return FALSE;
    }

    /* 各エントリのサービス名を wide 化する (calloc() で lpServiceName は全 NULL 初期化済み)。 */
    for (i = 0; i < count; i++)
    {
        rc = cplat_utf8_to_wstr_alloc(&w_table[i].lpServiceName, lpServiceStartTable[i].lpServiceName);
        w_table[i].lpServiceProc = lpServiceStartTable[i].lpServiceProc;
        if (rc != CPLAT_OK)
        {
            break;
        }
    }

    /* 終端要素は calloc() で 0 初期化済み。 */
    if (rc == CPLAT_OK)
    {
        result = StartServiceCtrlDispatcherW(w_table);
    }
    else
    {
        result = FALSE;
    }

    for (i = 0; i < count; i++)
    {
        cplat_internal_win32_free_keep_last_error(w_table[i].lpServiceName);
    }
    cplat_internal_win32_free_keep_last_error(w_table);

    if (rc != CPLAT_OK)
    {
        cplat_internal_win32_set_last_error_from_result(rc);
    }

    return result;
}

#endif /* PLATFORM_WINDOWS */
