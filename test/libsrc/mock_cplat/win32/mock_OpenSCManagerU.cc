#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_WINDOWS)

SC_HANDLE delegate_real_OpenSCManagerU(const char *lpMachineName, const char *lpDatabaseName, DWORD dwDesiredAccess)
{
    static auto real_fn =
        reinterpret_cast<decltype(&OpenSCManagerU)>(resolveSharedSymbolOrExit(kLibCplatName, "OpenSCManagerU"));

    return real_fn(lpMachineName, lpDatabaseName, dwDesiredAccess);
}

MOCK_WEAK_IMPL(SC_HANDLE, OpenSCManagerU, const char *lpMachineName, const char *lpDatabaseName, DWORD dwDesiredAccess)
{
    SC_HANDLE mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->OpenSCManagerU(lpMachineName, lpDatabaseName, dwDesiredAccess);
    }
    else
    {
        mock_ret = delegate_real_OpenSCManagerU(lpMachineName, lpDatabaseName, dwDesiredAccess);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s %s, %s, %lu", __func__, (lpMachineName != nullptr) ? lpMachineName : "(null)",
               (lpDatabaseName != nullptr) ? lpDatabaseName : "(null)", dwDesiredAccess);
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
