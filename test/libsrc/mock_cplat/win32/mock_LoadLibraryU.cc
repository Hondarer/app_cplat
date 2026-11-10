#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_WINDOWS)

HMODULE delegate_real_LoadLibraryU(const char *lpLibFileName)
{
    static auto real_fn =
        reinterpret_cast<decltype(&LoadLibraryU)>(resolveSharedSymbolOrExit(kLibCplatName, "LoadLibraryU"));

    return real_fn(lpLibFileName);
}

MOCK_WEAK_IMPL(HMODULE, LoadLibraryU, const char *lpLibFileName)
{
    HMODULE mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->LoadLibraryU(lpLibFileName);
    }
    else
    {
        mock_ret = delegate_real_LoadLibraryU(lpLibFileName);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s %s", __func__, (lpLibFileName != nullptr) ? lpLibFileName : "(null)");
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
