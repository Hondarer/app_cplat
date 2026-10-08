#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_WINDOWS)

void delegate_real_cplat_error_capture_windows_error(cplat_error *error, unsigned long error_code)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_error_capture_windows_error)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_error_capture_windows_error"));

    real_fn(error, error_code);
}

MOCK_WEAK_IMPL(void, cplat_error_capture_windows_error, cplat_error *error, unsigned long error_code)
{
    if (_mock_cplat != nullptr)
    {
        _mock_cplat->cplat_error_capture_windows_error(error, error_code);
    }
    else
    {
        delegate_real_cplat_error_capture_windows_error(error, error_code);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }
}

#endif /* PLATFORM_WINDOWS */
