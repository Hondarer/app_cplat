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

SC_HANDLE OpenSCManagerU(const char *utf8_machine_name, const char *utf8_database_name, DWORD desired_access)
{
    SC_HANDLE result;
    wchar_t *wmachine = NULL;
    wchar_t *wdatabase = NULL;

    if (utf8_machine_name != NULL)
    {
        wmachine = cplat_utf8_to_wstr_alloc(utf8_machine_name);
    }

    if (utf8_database_name != NULL)
    {
        wdatabase = cplat_utf8_to_wstr_alloc(utf8_database_name);
    }

    if ((utf8_machine_name != NULL && wmachine == NULL) || (utf8_database_name != NULL && wdatabase == NULL))
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        cplat_free(wmachine);
        cplat_free(wdatabase);
        return NULL;
    }

    result = OpenSCManagerW(wmachine, wdatabase, desired_access);
    cplat_free(wmachine);
    cplat_free(wdatabase);

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

SC_HANDLE CreateServiceU(SC_HANDLE scm, const char *utf8_service_name, const char *utf8_display_name,
                         DWORD desired_access, DWORD service_type, DWORD start_type, DWORD error_control,
                         const char *utf8_binary_path_name, const char *utf8_load_order_group, LPDWORD tag_id,
                         const char *utf8_dependencies, const char *utf8_service_start_name, const char *utf8_password)
{
    SC_HANDLE result;
    wchar_t *wservice_name = NULL;
    wchar_t *wdisplay_name = NULL;
    wchar_t *wbinary_path_name = NULL;
    wchar_t *wload_order_group = NULL;
    wchar_t *wdependencies = NULL;
    wchar_t *wservice_start_name = NULL;
    wchar_t *wpassword = NULL;

    if (utf8_service_name != NULL)
    {
        wservice_name = cplat_utf8_to_wstr_alloc(utf8_service_name);
    }

    if (utf8_display_name != NULL)
    {
        wdisplay_name = cplat_utf8_to_wstr_alloc(utf8_display_name);
    }

    if (utf8_binary_path_name != NULL)
    {
        wbinary_path_name = cplat_utf8_to_wstr_alloc(utf8_binary_path_name);
    }

    if (utf8_load_order_group != NULL)
    {
        wload_order_group = cplat_utf8_to_wstr_alloc(utf8_load_order_group);
    }

    if (utf8_dependencies != NULL)
    {
        wdependencies = cplat_utf8_to_wstr_alloc(utf8_dependencies);
    }

    if (utf8_service_start_name != NULL)
    {
        wservice_start_name = cplat_utf8_to_wstr_alloc(utf8_service_start_name);
    }

    if (utf8_password != NULL)
    {
        wpassword = cplat_utf8_to_wstr_alloc(utf8_password);
    }

    if ((utf8_service_name != NULL && wservice_name == NULL) || (utf8_display_name != NULL && wdisplay_name == NULL) ||
        (utf8_binary_path_name != NULL && wbinary_path_name == NULL) ||
        (utf8_load_order_group != NULL && wload_order_group == NULL) ||
        (utf8_dependencies != NULL && wdependencies == NULL) ||
        (utf8_service_start_name != NULL && wservice_start_name == NULL) ||
        (utf8_password != NULL && wpassword == NULL))
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

    result =
        CreateServiceW(scm, wservice_name, wdisplay_name, desired_access, service_type, start_type, error_control,
                       wbinary_path_name, wload_order_group, tag_id, wdependencies, wservice_start_name, wpassword);
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

SC_HANDLE OpenServiceU(SC_HANDLE scm, const char *utf8_service_name, DWORD desired_access)
{
    SC_HANDLE result;
    wchar_t *wservice_name;

    wservice_name = cplat_utf8_to_wstr_alloc(utf8_service_name);
    if (wservice_name == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    result = OpenServiceW(scm, wservice_name, desired_access);
    cplat_free(wservice_name);

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

BOOL ChangeServiceConfig2U(SC_HANDLE service, DWORD info_level, const char *utf8_text)
{
    BOOL result;
    wchar_t *wtext;
    SERVICE_DESCRIPTIONW desc_w;

    if (info_level != SERVICE_CONFIG_DESCRIPTION)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    if (utf8_text != NULL)
    {
        wtext = cplat_utf8_to_wstr_alloc(utf8_text);
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
    result = ChangeServiceConfig2W(service, SERVICE_CONFIG_DESCRIPTION, &desc_w);
    cplat_free(wtext);

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

SERVICE_STATUS_HANDLE RegisterServiceCtrlHandlerExU(const char *utf8_service_name, LPHANDLER_FUNCTION_EX handler_proc,
                                                    LPVOID context)
{
    SERVICE_STATUS_HANDLE result;
    wchar_t *wservice_name;

    wservice_name = cplat_utf8_to_wstr_alloc(utf8_service_name);
    if (wservice_name == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    result = RegisterServiceCtrlHandlerExW(wservice_name, handler_proc, context);
    cplat_free(wservice_name);

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

BOOL StartServiceCtrlDispatcherU(const cplat_service_entry_u *service_table)
{
    size_t count;
    SERVICE_TABLE_ENTRYW *w_table;
    size_t i;
    BOOL ok;
    BOOL result;

    if (service_table == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    /* テーブルのエントリ数を数える (終端: service_name == NULL)。 */
    count = 0;
    while (service_table[count].service_name != NULL)
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
        w_table[i].lpServiceName = cplat_utf8_to_wstr_alloc(service_table[i].service_name);
        w_table[i].lpServiceProc = service_table[i].service_proc;
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
