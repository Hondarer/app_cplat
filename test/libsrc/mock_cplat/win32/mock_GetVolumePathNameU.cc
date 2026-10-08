#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_WINDOWS)

BOOL delegate_real_GetVolumePathNameU(const char *utf8_path, char *utf8_volume_root, DWORD size)
{
    static auto real_fn =
        reinterpret_cast<decltype(&GetVolumePathNameU)>(resolveSharedSymbolOrExit(kLibCplatName, "GetVolumePathNameU"));

    return real_fn(utf8_path, utf8_volume_root, size);
}

MOCK_WEAK_IMPL(BOOL, GetVolumePathNameU, const char *utf8_path, char *utf8_volume_root, DWORD size)
{
    BOOL mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->GetVolumePathNameU(utf8_path, utf8_volume_root, size);
    }
    else
    {
        mock_ret = delegate_real_GetVolumePathNameU(utf8_path, utf8_volume_root, size);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s %s, 0x%p, %lu", __func__, (utf8_path != nullptr) ? utf8_path : "(null)",
               (void *)utf8_volume_root, size);
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
