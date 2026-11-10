#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_WINDOWS)

BOOL delegate_real_ChangeServiceConfig2U(SC_HANDLE hService, DWORD dwInfoLevel, LPVOID lpInfo)
{
    static auto real_fn = reinterpret_cast<decltype(&ChangeServiceConfig2U)>(
        resolveSharedSymbolOrExit(kLibCplatName, "ChangeServiceConfig2U"));

    return real_fn(hService, dwInfoLevel, lpInfo);
}

MOCK_WEAK_IMPL(BOOL, ChangeServiceConfig2U, SC_HANDLE hService, DWORD dwInfoLevel, LPVOID lpInfo)
{
    BOOL mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->ChangeServiceConfig2U(hService, dwInfoLevel, lpInfo);
    }
    else
    {
        mock_ret = delegate_real_ChangeServiceConfig2U(hService, dwInfoLevel, lpInfo);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p, %lu, 0x%p", __func__, (void *)hService, dwInfoLevel, lpInfo);
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
