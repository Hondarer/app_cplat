#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_WINDOWS)

DWORD delegate_real_GetModuleFileNameU(HMODULE module, char *utf8_buf, DWORD size)
{
    static auto real_fn =
        reinterpret_cast<decltype(&GetModuleFileNameU)>(resolveSharedSymbolOrExit(kLibCplatName, "GetModuleFileNameU"));

    return real_fn(module, utf8_buf, size);
}

MOCK_WEAK_IMPL(DWORD, GetModuleFileNameU, HMODULE module, char *utf8_buf, DWORD size)
{
    DWORD mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->GetModuleFileNameU(module, utf8_buf, size);
    }
    else
    {
        mock_ret = delegate_real_GetModuleFileNameU(module, utf8_buf, size);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p, 0x%p, %lu", __func__, (void *)module, (void *)utf8_buf, size);
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
