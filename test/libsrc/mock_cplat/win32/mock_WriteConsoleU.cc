#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_WINDOWS)

BOOL delegate_real_WriteConsoleU(HANDLE hConsoleOutput, const char *lpBuffer, DWORD nNumberOfCharsToWrite,
                                 LPDWORD lpNumberOfCharsWritten, LPVOID lpReserved)
{
    static auto real_fn =
        reinterpret_cast<decltype(&WriteConsoleU)>(resolveSharedSymbolOrExit(kLibCplatName, "WriteConsoleU"));

    return real_fn(hConsoleOutput, lpBuffer, nNumberOfCharsToWrite, lpNumberOfCharsWritten, lpReserved);
}

MOCK_WEAK_IMPL(BOOL, WriteConsoleU, HANDLE hConsoleOutput, const char *lpBuffer, DWORD nNumberOfCharsToWrite,
               LPDWORD lpNumberOfCharsWritten, LPVOID lpReserved)
{
    BOOL mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->WriteConsoleU(hConsoleOutput, lpBuffer, nNumberOfCharsToWrite, lpNumberOfCharsWritten,
                                              lpReserved);
    }
    else
    {
        mock_ret = delegate_real_WriteConsoleU(hConsoleOutput, lpBuffer, nNumberOfCharsToWrite, lpNumberOfCharsWritten,
                                               lpReserved);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p, %s, %lu, 0x%p, 0x%p", __func__, (void *)hConsoleOutput,
               (lpBuffer != nullptr) ? lpBuffer : "(null)", nNumberOfCharsToWrite, (void *)lpNumberOfCharsWritten,
               lpReserved);
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
