#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_WINDOWS)

HANDLE delegate_real_CreateFileU(const char *lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                                 LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
                                 DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
    static auto real_fn =
        reinterpret_cast<decltype(&CreateFileU)>(resolveSharedSymbolOrExit(kLibCplatName, "CreateFileU"));

    return real_fn(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition,
                   dwFlagsAndAttributes, hTemplateFile);
}

MOCK_WEAK_IMPL(HANDLE, CreateFileU, const char *lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
               LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
               HANDLE hTemplateFile)
{
    HANDLE mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->CreateFileU(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
                                            dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
    }
    else
    {
        mock_ret = delegate_real_CreateFileU(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
                                             dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s %s, %lu, %lu, 0x%p, %lu, %lu, 0x%p", __func__, (lpFileName != nullptr) ? lpFileName : "(null)",
               dwDesiredAccess, dwShareMode, (void *)lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes,
               (void *)hTemplateFile);
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
