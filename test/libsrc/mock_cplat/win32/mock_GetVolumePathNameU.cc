#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_WINDOWS)

BOOL delegate_real_GetVolumePathNameU(const char *lpszFileName, char *lpszVolumePathName, DWORD cchBufferLength)
{
    static auto real_fn =
        reinterpret_cast<decltype(&GetVolumePathNameU)>(resolveSharedSymbolOrExit(kLibCplatName, "GetVolumePathNameU"));

    return real_fn(lpszFileName, lpszVolumePathName, cchBufferLength);
}

MOCK_WEAK_IMPL(BOOL, GetVolumePathNameU, const char *lpszFileName, char *lpszVolumePathName, DWORD cchBufferLength)
{
    BOOL mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->GetVolumePathNameU(lpszFileName, lpszVolumePathName, cchBufferLength);
    }
    else
    {
        mock_ret = delegate_real_GetVolumePathNameU(lpszFileName, lpszVolumePathName, cchBufferLength);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s %s, 0x%p, %lu", __func__, (lpszFileName != nullptr) ? lpszFileName : "(null)",
               (void *)lpszVolumePathName, cchBufferLength);
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
