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
    #include <stdlib.h>

/* Doxygen コメントは、ヘッダーに記載 */

SC_HANDLE OpenSCManagerU(const char *lpMachineName, const char *lpDatabaseName, DWORD dwDesiredAccess)
{
    SC_HANDLE result;
    wchar_t *wmachine = NULL;
    wchar_t *wdatabase = NULL;

    if (lpMachineName != NULL)
    {
        wmachine = cplat_utf8_to_wstr_alloc(lpMachineName);
    }

    if (lpDatabaseName != NULL)
    {
        wdatabase = cplat_utf8_to_wstr_alloc(lpDatabaseName);
    }

    if ((lpMachineName != NULL && wmachine == NULL) || (lpDatabaseName != NULL && wdatabase == NULL))
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        cplat_free(wmachine);
        cplat_free(wdatabase);
        return NULL;
    }

    result = OpenSCManagerW(wmachine, wdatabase, dwDesiredAccess);
    cplat_free(wmachine);
    cplat_free(wdatabase);

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

    if (lpServiceName != NULL)
    {
        wservice_name = cplat_utf8_to_wstr_alloc(lpServiceName);
    }

    if (lpDisplayName != NULL)
    {
        wdisplay_name = cplat_utf8_to_wstr_alloc(lpDisplayName);
    }

    if (lpBinaryPathName != NULL)
    {
        wbinary_path_name = cplat_utf8_to_wstr_alloc(lpBinaryPathName);
    }

    if (lpLoadOrderGroup != NULL)
    {
        wload_order_group = cplat_utf8_to_wstr_alloc(lpLoadOrderGroup);
    }

    if (lpDependencies != NULL)
    {
        wdependencies = cplat_utf8_to_wstr_alloc(lpDependencies);
    }

    if (lpServiceStartName != NULL)
    {
        wservice_start_name = cplat_utf8_to_wstr_alloc(lpServiceStartName);
    }

    if (lpPassword != NULL)
    {
        wpassword = cplat_utf8_to_wstr_alloc(lpPassword);
    }

    if ((lpServiceName != NULL && wservice_name == NULL) || (lpDisplayName != NULL && wdisplay_name == NULL) ||
        (lpBinaryPathName != NULL && wbinary_path_name == NULL) ||
        (lpLoadOrderGroup != NULL && wload_order_group == NULL) || (lpDependencies != NULL && wdependencies == NULL) ||
        (lpServiceStartName != NULL && wservice_start_name == NULL) || (lpPassword != NULL && wpassword == NULL))
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        cplat_free(wservice_name);
        cplat_free(wdisplay_name);
        cplat_free(wbinary_path_name);
        cplat_free(wload_order_group);
        cplat_free(wdependencies);
        cplat_free(wservice_start_name);
        cplat_free(wpassword);
        return NULL;
    }

    result = CreateServiceW(hSCManager, wservice_name, wdisplay_name, dwDesiredAccess, dwServiceType, dwStartType,
                            dwErrorControl, wbinary_path_name, wload_order_group, lpdwTagId, wdependencies,
                            wservice_start_name, wpassword);
    cplat_free(wservice_name);
    cplat_free(wdisplay_name);
    cplat_free(wbinary_path_name);
    cplat_free(wload_order_group);
    cplat_free(wdependencies);
    cplat_free(wservice_start_name);
    cplat_free(wpassword);

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

SC_HANDLE OpenServiceU(SC_HANDLE hSCManager, const char *lpServiceName, DWORD dwDesiredAccess)
{
    SC_HANDLE result;
    wchar_t *wservice_name;

    wservice_name = cplat_utf8_to_wstr_alloc(lpServiceName);
    if (wservice_name == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    result = OpenServiceW(hSCManager, wservice_name, dwDesiredAccess);
    cplat_free(wservice_name);

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

BOOL ChangeServiceConfig2U(SC_HANDLE hService, DWORD dwInfoLevel, LPVOID lpInfo)
{
    BOOL result;
    wchar_t *wtext;
    const SERVICE_DESCRIPTIONA *desc;
    SERVICE_DESCRIPTIONW desc_w;

    if (dwInfoLevel != SERVICE_CONFIG_DESCRIPTION || lpInfo == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    desc = (const SERVICE_DESCRIPTIONA *)lpInfo;

    if (desc->lpDescription != NULL)
    {
        wtext = cplat_utf8_to_wstr_alloc(desc->lpDescription);
        if (wtext == NULL)
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }
    }
    else
    {
        wtext = NULL;
    }

    desc_w.lpDescription = wtext;
    result = ChangeServiceConfig2W(hService, SERVICE_CONFIG_DESCRIPTION, &desc_w);
    cplat_free(wtext);

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

SERVICE_STATUS_HANDLE RegisterServiceCtrlHandlerExU(const char *lpServiceName, LPHANDLER_FUNCTION_EX lpHandlerProc,
                                                    LPVOID lpContext)
{
    SERVICE_STATUS_HANDLE result;
    wchar_t *wservice_name;

    wservice_name = cplat_utf8_to_wstr_alloc(lpServiceName);
    if (wservice_name == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    result = RegisterServiceCtrlHandlerExW(wservice_name, lpHandlerProc, lpContext);
    cplat_free(wservice_name);

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

BOOL StartServiceCtrlDispatcherU(const SERVICE_TABLE_ENTRYU *lpServiceStartTable)
{
    size_t count;
    SERVICE_TABLE_ENTRYW *w_table;
    size_t i;
    BOOL ok;
    BOOL result;

    if (lpServiceStartTable == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
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
        SetLastError(ERROR_OUTOFMEMORY);
        return FALSE;
    }

    /* 各エントリのサービス名を wide 化する (calloc() で lpServiceName は全 NULL 初期化済み)。 */
    for (i = 0; i < count; i++)
    {
        w_table[i].lpServiceName = cplat_utf8_to_wstr_alloc(lpServiceStartTable[i].lpServiceName);
        w_table[i].lpServiceProc = lpServiceStartTable[i].lpServiceProc;
    }

    /* 終端要素は calloc() で 0 初期化済み。 */

    /* 変換の成否を確認する。 */
    ok = TRUE;

    for (i = 0; i < count; i++)
    {
        if (w_table[i].lpServiceName == NULL)
        {
            ok = FALSE;
            break;
        }
    }

    if (ok)
    {
        result = StartServiceCtrlDispatcherW(w_table);
    }
    else
    {
        SetLastError(ERROR_OUTOFMEMORY);
        result = FALSE;
    }

    for (i = 0; i < count; i++)
    {
        cplat_free(w_table[i].lpServiceName);
    }

    cplat_free(w_table);

    return result;
}

#endif /* PLATFORM_WINDOWS */
