#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_WINDOWS)

SC_HANDLE delegate_real_CreateServiceU(SC_HANDLE scm, const char *utf8_service_name, const char *utf8_display_name,
                                       DWORD desired_access, DWORD service_type, DWORD start_type, DWORD error_control,
                                       const char *utf8_binary_path_name, const char *utf8_load_order_group,
                                       LPDWORD tag_id, const char *utf8_dependencies,
                                       const char *utf8_service_start_name, const char *utf8_password)
{
    static auto real_fn =
        reinterpret_cast<decltype(&CreateServiceU)>(resolveSharedSymbolOrExit(kLibCplatName, "CreateServiceU"));

    return real_fn(scm, utf8_service_name, utf8_display_name, desired_access, service_type, start_type, error_control,
                   utf8_binary_path_name, utf8_load_order_group, tag_id, utf8_dependencies, utf8_service_start_name,
                   utf8_password);
}

MOCK_WEAK_IMPL(SC_HANDLE, CreateServiceU, SC_HANDLE scm, const char *utf8_service_name, const char *utf8_display_name,
               DWORD desired_access, DWORD service_type, DWORD start_type, DWORD error_control,
               const char *utf8_binary_path_name, const char *utf8_load_order_group, LPDWORD tag_id,
               const char *utf8_dependencies, const char *utf8_service_start_name, const char *utf8_password)
{
    SC_HANDLE mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->CreateServiceU(scm, utf8_service_name, utf8_display_name, desired_access, service_type,
                                               start_type, error_control, utf8_binary_path_name, utf8_load_order_group,
                                               tag_id, utf8_dependencies, utf8_service_start_name, utf8_password);
    }
    else
    {
        mock_ret = delegate_real_CreateServiceU(scm, utf8_service_name, utf8_display_name, desired_access, service_type,
                                                start_type, error_control, utf8_binary_path_name, utf8_load_order_group,
                                                tag_id, utf8_dependencies, utf8_service_start_name, utf8_password);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p, %s, %s, %lu, %lu, %lu, %lu, %s, %s, 0x%p, %s, %s, %s", __func__, (void *)scm,
               (utf8_service_name != nullptr) ? utf8_service_name : "(null)",
               (utf8_display_name != nullptr) ? utf8_display_name : "(null)", desired_access, service_type, start_type,
               error_control, (utf8_binary_path_name != nullptr) ? utf8_binary_path_name : "(null)",
               (utf8_load_order_group != nullptr) ? utf8_load_order_group : "(null)", (void *)tag_id,
               (utf8_dependencies != nullptr) ? utf8_dependencies : "(null)",
               (utf8_service_start_name != nullptr) ? utf8_service_start_name : "(null)",
               (utf8_password != nullptr) ? "(masked)" : "(null)");
        if (getTraceLevel() >= TRACE_DETAIL)
        {
            printf(" -> 0x%p\n", (void *)mock_ret);
        }
        else
        {
            printf("\n");
        }
    }

    return mock_ret;
}

#endif /* PLATFORM_WINDOWS */
