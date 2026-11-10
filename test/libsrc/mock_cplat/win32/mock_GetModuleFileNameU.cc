#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_WINDOWS)

DWORD delegate_real_GetModuleFileNameU(HMODULE hModule, char *lpFilename, DWORD nSize)
{
    static auto real_fn =
        reinterpret_cast<decltype(&GetModuleFileNameU)>(resolveSharedSymbolOrExit(kLibCplatName, "GetModuleFileNameU"));

    return real_fn(hModule, lpFilename, nSize);
}

MOCK_WEAK_IMPL(DWORD, GetModuleFileNameU, HMODULE hModule, char *lpFilename, DWORD nSize)
{
    DWORD mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->GetModuleFileNameU(hModule, lpFilename, nSize);
    }
    else
    {
        mock_ret = delegate_real_GetModuleFileNameU(hModule, lpFilename, nSize);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p, 0x%p, %lu", __func__, (void *)hModule, (void *)lpFilename, nSize);
        if (getTraceLevel() >= TRACE_DETAIL)
        {
            printf(" -> %lu\n", mock_ret);
        }
        else
        {
            printf("\n");
        }
    }

    return mock_ret;
}

#endif /* PLATFORM_WINDOWS */
