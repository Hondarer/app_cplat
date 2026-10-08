#include <testfw.h>
#include <mock_cplat.h>

void delegate_real_cplat_error_capture_errno(cplat_error *error, int errno_value)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_error_capture_errno)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_error_capture_errno"));

    real_fn(error, errno_value);
}

MOCK_WEAK_IMPL(void, cplat_error_capture_errno, cplat_error *error, int errno_value)
{
    if (_mock_cplat != nullptr)
    {
        _mock_cplat->cplat_error_capture_errno(error, errno_value);
    }
    else
    {
        delegate_real_cplat_error_capture_errno(error, errno_value);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }
}
