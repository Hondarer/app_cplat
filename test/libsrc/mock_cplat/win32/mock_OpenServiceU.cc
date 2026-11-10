#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_WINDOWS)

SC_HANDLE delegate_real_OpenServiceU(SC_HANDLE hSCManager, const char *lpServiceName, DWORD dwDesiredAccess)
{
    static auto real_fn =
        reinterpret_cast<decltype(&OpenServiceU)>(resolveSharedSymbolOrExit(kLibCplatName, "OpenServiceU"));

    return real_fn(hSCManager, lpServiceName, dwDesiredAccess);
}

MOCK_WEAK_IMPL(SC_HANDLE, OpenServiceU, SC_HANDLE hSCManager, const char *lpServiceName, DWORD dwDesiredAccess)
{
    SC_HANDLE mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->OpenServiceU(hSCManager, lpServiceName, dwDesiredAccess);
    }
    else
    {
        mock_ret = delegate_real_OpenServiceU(hSCManager, lpServiceName, dwDesiredAccess);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p, %s, %lu", __func__, (void *)hSCManager,
               (lpServiceName != nullptr) ? lpServiceName : "(null)", dwDesiredAccess);
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
