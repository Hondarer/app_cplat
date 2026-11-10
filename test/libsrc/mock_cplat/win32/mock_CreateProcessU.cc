#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_WINDOWS)

BOOL delegate_real_CreateProcessU(const char *lpApplicationName, const char *lpCommandLine,
                                  LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes,
                                  BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment,
                                  const char *lpCurrentDirectory, LPSTARTUPINFOW lpStartupInfo,
                                  LPPROCESS_INFORMATION lpProcessInformation)
{
    static auto real_fn =
        reinterpret_cast<decltype(&CreateProcessU)>(resolveSharedSymbolOrExit(kLibCplatName, "CreateProcessU"));

    return real_fn(lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles,
                   dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation);
}

MOCK_WEAK_IMPL(BOOL, CreateProcessU, const char *lpApplicationName, const char *lpCommandLine,
               LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes,
               BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment, const char *lpCurrentDirectory,
               LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
{
    BOOL mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->CreateProcessU(lpApplicationName, lpCommandLine, lpProcessAttributes,
                                               lpThreadAttributes, bInheritHandles, dwCreationFlags, lpEnvironment,
                                               lpCurrentDirectory, lpStartupInfo, lpProcessInformation);
    }
    else
    {
        mock_ret = delegate_real_CreateProcessU(lpApplicationName, lpCommandLine, lpProcessAttributes,
                                                lpThreadAttributes, bInheritHandles, dwCreationFlags, lpEnvironment,
                                                lpCurrentDirectory, lpStartupInfo, lpProcessInformation);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s %s, %s, 0x%p, 0x%p, %d, %lu, 0x%p, %s, 0x%p, 0x%p", __func__,
               (lpApplicationName != nullptr) ? lpApplicationName : "(null)",
               (lpCommandLine != nullptr) ? lpCommandLine : "(null)", (void *)lpProcessAttributes,
               (void *)lpThreadAttributes, bInheritHandles, dwCreationFlags, (void *)lpEnvironment,
               (lpCurrentDirectory != nullptr) ? lpCurrentDirectory : "(null)", (void *)lpStartupInfo,
               (void *)lpProcessInformation);
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
