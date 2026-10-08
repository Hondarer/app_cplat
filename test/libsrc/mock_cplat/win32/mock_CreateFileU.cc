#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_WINDOWS)

HANDLE delegate_real_CreateFileU(const char *utf8_path, DWORD desired_access, DWORD share_mode,
                                 LPSECURITY_ATTRIBUTES security_attributes, DWORD creation_disposition,
                                 DWORD flags_and_attributes, HANDLE template_file)
{
    static auto real_fn =
        reinterpret_cast<decltype(&CreateFileU)>(resolveSharedSymbolOrExit(kLibCplatName, "CreateFileU"));

    return real_fn(utf8_path, desired_access, share_mode, security_attributes, creation_disposition,
                   flags_and_attributes, template_file);
}

MOCK_WEAK_IMPL(HANDLE, CreateFileU, const char *utf8_path, DWORD desired_access, DWORD share_mode,
               LPSECURITY_ATTRIBUTES security_attributes, DWORD creation_disposition, DWORD flags_and_attributes,
               HANDLE template_file)
{
    HANDLE mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->CreateFileU(utf8_path, desired_access, share_mode, security_attributes,
                                            creation_disposition, flags_and_attributes, template_file);
    }
    else
    {
        mock_ret = delegate_real_CreateFileU(utf8_path, desired_access, share_mode, security_attributes,
                                             creation_disposition, flags_and_attributes, template_file);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s %s, %lu, %lu, 0x%p, %lu, %lu, 0x%p", __func__, (utf8_path != nullptr) ? utf8_path : "(null)",
               desired_access, share_mode, (void *)security_attributes, creation_disposition, flags_and_attributes,
               (void *)template_file);
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
