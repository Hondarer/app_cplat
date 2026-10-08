#include <testfw.h>
#include <mock_cplat.h>

void delegate_real_cplat_error_get_last(cplat_error *error_out)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_error_get_last)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_error_get_last"));

    real_fn(error_out);
}

MOCK_WEAK_IMPL(void, cplat_error_get_last, cplat_error *error_out)
{
    if (_mock_cplat != nullptr)
    {
        _mock_cplat->cplat_error_get_last(error_out);
    }
    else
    {
        delegate_real_cplat_error_get_last(error_out);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }
}
