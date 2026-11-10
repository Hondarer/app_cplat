#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_WINDOWS)

BOOL delegate_real_GetVolumeInformationU(const char *lpRootPathName, char *lpVolumeNameBuffer, DWORD nVolumeNameSize,
                                         LPDWORD lpVolumeSerialNumber, LPDWORD lpMaximumComponentLength,
                                         LPDWORD lpFileSystemFlags, char *lpFileSystemNameBuffer,
                                         DWORD nFileSystemNameSize)
{
    static auto real_fn = reinterpret_cast<decltype(&GetVolumeInformationU)>(
        resolveSharedSymbolOrExit(kLibCplatName, "GetVolumeInformationU"));

    return real_fn(lpRootPathName, lpVolumeNameBuffer, nVolumeNameSize, lpVolumeSerialNumber, lpMaximumComponentLength,
                   lpFileSystemFlags, lpFileSystemNameBuffer, nFileSystemNameSize);
}

MOCK_WEAK_IMPL(BOOL, GetVolumeInformationU, const char *lpRootPathName, char *lpVolumeNameBuffer, DWORD nVolumeNameSize,
               LPDWORD lpVolumeSerialNumber, LPDWORD lpMaximumComponentLength, LPDWORD lpFileSystemFlags,
               char *lpFileSystemNameBuffer, DWORD nFileSystemNameSize)
{
    BOOL mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->GetVolumeInformationU(lpRootPathName, lpVolumeNameBuffer, nVolumeNameSize,
                                                      lpVolumeSerialNumber, lpMaximumComponentLength, lpFileSystemFlags,
                                                      lpFileSystemNameBuffer, nFileSystemNameSize);
    }
    else
    {
        mock_ret = delegate_real_GetVolumeInformationU(lpRootPathName, lpVolumeNameBuffer, nVolumeNameSize,
                                                       lpVolumeSerialNumber, lpMaximumComponentLength,
                                                       lpFileSystemFlags, lpFileSystemNameBuffer, nFileSystemNameSize);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s %s, 0x%p, %lu, 0x%p, 0x%p, 0x%p, 0x%p, %lu", __func__,
               (lpRootPathName != nullptr) ? lpRootPathName : "(null)", (void *)lpVolumeNameBuffer, nVolumeNameSize,
               (void *)lpVolumeSerialNumber, (void *)lpMaximumComponentLength, (void *)lpFileSystemFlags,
               (void *)lpFileSystemNameBuffer, nFileSystemNameSize);
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
