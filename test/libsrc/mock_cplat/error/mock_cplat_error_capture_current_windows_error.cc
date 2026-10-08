#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_WINDOWS)

void delegate_real_cplat_error_capture_current_windows_error(cplat_error *error)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_error_capture_current_windows_error)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_error_capture_current_windows_error"));

    real_fn(error);
}

MOCK_WEAK_IMPL(void, cplat_error_capture_current_windows_error, cplat_error *error)
{
    if (_mock_cplat != nullptr)
    {
        _mock_cplat->cplat_error_capture_current_windows_error(error);
    }
    else
    {
        delegate_real_cplat_error_capture_current_windows_error(error);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }
}

#endif /* PLATFORM_WINDOWS */
