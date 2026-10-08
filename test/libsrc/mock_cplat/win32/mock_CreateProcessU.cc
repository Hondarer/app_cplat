#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_WINDOWS)

BOOL delegate_real_CreateProcessU(const char *utf8_application_name, const char *utf8_command_line,
                                  LPSECURITY_ATTRIBUTES process_attributes, LPSECURITY_ATTRIBUTES thread_attributes,
                                  BOOL inherit_handles, DWORD creation_flags, LPVOID environment,
                                  const char *utf8_current_directory, LPSTARTUPINFOW startup_info,
                                  LPPROCESS_INFORMATION process_information)
{
    static auto real_fn =
        reinterpret_cast<decltype(&CreateProcessU)>(resolveSharedSymbolOrExit(kLibCplatName, "CreateProcessU"));

    return real_fn(utf8_application_name, utf8_command_line, process_attributes, thread_attributes, inherit_handles,
                   creation_flags, environment, utf8_current_directory, startup_info, process_information);
}

MOCK_WEAK_IMPL(BOOL, CreateProcessU, const char *utf8_application_name, const char *utf8_command_line,
               LPSECURITY_ATTRIBUTES process_attributes, LPSECURITY_ATTRIBUTES thread_attributes, BOOL inherit_handles,
               DWORD creation_flags, LPVOID environment, const char *utf8_current_directory,
               LPSTARTUPINFOW startup_info, LPPROCESS_INFORMATION process_information)
{
    BOOL mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->CreateProcessU(utf8_application_name, utf8_command_line, process_attributes,
                                               thread_attributes, inherit_handles, creation_flags, environment,
                                               utf8_current_directory, startup_info, process_information);
    }
    else
    {
        mock_ret = delegate_real_CreateProcessU(utf8_application_name, utf8_command_line, process_attributes,
                                                thread_attributes, inherit_handles, creation_flags, environment,
                                                utf8_current_directory, startup_info, process_information);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s %s, %s, 0x%p, 0x%p, %d, %lu, 0x%p, %s, 0x%p, 0x%p", __func__,
               (utf8_application_name != nullptr) ? utf8_application_name : "(null)",
               (utf8_command_line != nullptr) ? utf8_command_line : "(null)", (void *)process_attributes,
               (void *)thread_attributes, inherit_handles, creation_flags, (void *)environment,
               (utf8_current_directory != nullptr) ? utf8_current_directory : "(null)", (void *)startup_info,
               (void *)process_information);
        if (getTraceLevel() >= TRACE_DETAIL)
        {
            printf(" -> %d\n", mock_ret);
        }
        else
        {
            printf("\n");
        }
    }

    return mock_ret;
}

#endif /* PLATFORM_WINDOWS */
