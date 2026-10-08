#include <testfw.h>
#include <mock_cplat.h>

void delegate_real_cplat_error_clear(cplat_error *error)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_error_clear)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_error_clear"));

    real_fn(error);
}

MOCK_WEAK_IMPL(void, cplat_error_clear, cplat_error *error)
{
    if (_mock_cplat != nullptr)
    {
        _mock_cplat->cplat_error_clear(error);
    }
    else
    {
        delegate_real_cplat_error_clear(error);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }
}
