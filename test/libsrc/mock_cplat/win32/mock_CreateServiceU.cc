#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_WINDOWS)

SC_HANDLE delegate_real_CreateServiceU(SC_HANDLE hSCManager, const char *lpServiceName, const char *lpDisplayName,
                                       DWORD dwDesiredAccess, DWORD dwServiceType, DWORD dwStartType,
                                       DWORD dwErrorControl, const char *lpBinaryPathName, const char *lpLoadOrderGroup,
                                       LPDWORD lpdwTagId, const char *lpDependencies, const char *lpServiceStartName,
                                       const char *lpPassword)
{
    static auto real_fn =
        reinterpret_cast<decltype(&CreateServiceU)>(resolveSharedSymbolOrExit(kLibCplatName, "CreateServiceU"));

    return real_fn(hSCManager, lpServiceName, lpDisplayName, dwDesiredAccess, dwServiceType, dwStartType,
                   dwErrorControl, lpBinaryPathName, lpLoadOrderGroup, lpdwTagId, lpDependencies, lpServiceStartName,
                   lpPassword);
}

MOCK_WEAK_IMPL(SC_HANDLE, CreateServiceU, SC_HANDLE hSCManager, const char *lpServiceName, const char *lpDisplayName,
               DWORD dwDesiredAccess, DWORD dwServiceType, DWORD dwStartType, DWORD dwErrorControl,
               const char *lpBinaryPathName, const char *lpLoadOrderGroup, LPDWORD lpdwTagId,
               const char *lpDependencies, const char *lpServiceStartName, const char *lpPassword)
{
    SC_HANDLE mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->CreateServiceU(hSCManager, lpServiceName, lpDisplayName, dwDesiredAccess, dwServiceType,
                                               dwStartType, dwErrorControl, lpBinaryPathName, lpLoadOrderGroup,
                                               lpdwTagId, lpDependencies, lpServiceStartName, lpPassword);
    }
    else
    {
        mock_ret = delegate_real_CreateServiceU(
            hSCManager, lpServiceName, lpDisplayName, dwDesiredAccess, dwServiceType, dwStartType, dwErrorControl,
            lpBinaryPathName, lpLoadOrderGroup, lpdwTagId, lpDependencies, lpServiceStartName, lpPassword);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p, %s, %s, %lu, %lu, %lu, %lu, %s, %s, 0x%p, %s, %s, %s", __func__, (void *)hSCManager,
               (lpServiceName != nullptr) ? lpServiceName : "(null)",
               (lpDisplayName != nullptr) ? lpDisplayName : "(null)", dwDesiredAccess, dwServiceType, dwStartType,
               dwErrorControl, (lpBinaryPathName != nullptr) ? lpBinaryPathName : "(null)",
               (lpLoadOrderGroup != nullptr) ? lpLoadOrderGroup : "(null)", (void *)lpdwTagId,
               (lpDependencies != nullptr) ? lpDependencies : "(null)",
               (lpServiceStartName != nullptr) ? lpServiceStartName : "(null)",
               (lpPassword != nullptr) ? "(masked)" : "(null)");
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
