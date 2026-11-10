#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_WINDOWS)

HANDLE delegate_real_CreateNamedPipeU(const char *lpName, DWORD dwOpenMode, DWORD dwPipeMode, DWORD nMaxInstances,
                                      DWORD nOutBufferSize, DWORD nInBufferSize, DWORD nDefaultTimeOut,
                                      LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
    static auto real_fn =
        reinterpret_cast<decltype(&CreateNamedPipeU)>(resolveSharedSymbolOrExit(kLibCplatName, "CreateNamedPipeU"));

    return real_fn(lpName, dwOpenMode, dwPipeMode, nMaxInstances, nOutBufferSize, nInBufferSize, nDefaultTimeOut,
                   lpSecurityAttributes);
}

MOCK_WEAK_IMPL(HANDLE, CreateNamedPipeU, const char *lpName, DWORD dwOpenMode, DWORD dwPipeMode, DWORD nMaxInstances,
               DWORD nOutBufferSize, DWORD nInBufferSize, DWORD nDefaultTimeOut,
               LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
    HANDLE mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->CreateNamedPipeU(lpName, dwOpenMode, dwPipeMode, nMaxInstances, nOutBufferSize,
                                                 nInBufferSize, nDefaultTimeOut, lpSecurityAttributes);
    }
    else
    {
        mock_ret = delegate_real_CreateNamedPipeU(lpName, dwOpenMode, dwPipeMode, nMaxInstances, nOutBufferSize,
                                                  nInBufferSize, nDefaultTimeOut, lpSecurityAttributes);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s %s, %lu, %lu, %lu, %lu, %lu, %lu, 0x%p", __func__, (lpName != nullptr) ? lpName : "(null)",
               dwOpenMode, dwPipeMode, nMaxInstances, nOutBufferSize, nInBufferSize, nDefaultTimeOut,
               (void *)lpSecurityAttributes);
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
