#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_WINDOWS)

BOOL delegate_real_StartServiceCtrlDispatcherU(const SERVICE_TABLE_ENTRYU *lpServiceStartTable)
{
    static auto real_fn = reinterpret_cast<decltype(&StartServiceCtrlDispatcherU)>(
        resolveSharedSymbolOrExit(kLibCplatName, "StartServiceCtrlDispatcherU"));

    return real_fn(lpServiceStartTable);
}

MOCK_WEAK_IMPL(BOOL, StartServiceCtrlDispatcherU, const SERVICE_TABLE_ENTRYU *lpServiceStartTable)
{
    BOOL mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->StartServiceCtrlDispatcherU(lpServiceStartTable);
    }
    else
    {
        mock_ret = delegate_real_StartServiceCtrlDispatcherU(lpServiceStartTable);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p", __func__, (const void *)lpServiceStartTable);
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
