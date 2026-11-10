#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_WINDOWS)

SERVICE_STATUS_HANDLE delegate_real_RegisterServiceCtrlHandlerExU(const char *lpServiceName,
                                                                  LPHANDLER_FUNCTION_EX lpHandlerProc, LPVOID lpContext)
{
    static auto real_fn = reinterpret_cast<decltype(&RegisterServiceCtrlHandlerExU)>(
        resolveSharedSymbolOrExit(kLibCplatName, "RegisterServiceCtrlHandlerExU"));

    return real_fn(lpServiceName, lpHandlerProc, lpContext);
}

MOCK_WEAK_IMPL(SERVICE_STATUS_HANDLE, RegisterServiceCtrlHandlerExU, const char *lpServiceName,
               LPHANDLER_FUNCTION_EX lpHandlerProc, LPVOID lpContext)
{
    SERVICE_STATUS_HANDLE mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->RegisterServiceCtrlHandlerExU(lpServiceName, lpHandlerProc, lpContext);
    }
    else
    {
        mock_ret = delegate_real_RegisterServiceCtrlHandlerExU(lpServiceName, lpHandlerProc, lpContext);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s %s, 0x%p, 0x%p", __func__, (lpServiceName != nullptr) ? lpServiceName : "(null)",
               (void *)lpHandlerProc, (void *)lpContext);
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
