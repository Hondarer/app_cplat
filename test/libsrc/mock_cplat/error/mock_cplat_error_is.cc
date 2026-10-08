#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_error_is(const cplat_error *error, cplat_error_cause cause)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_error_is)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_error_is"));

    return real_fn(error, cause);
}

MOCK_WEAK_IMPL(int, cplat_error_is, const cplat_error *error, cplat_error_cause cause)
{
    int mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_error_is(error, cause);
    }
    else
    {
        mock_ret = delegate_real_cplat_error_is(error, cause);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }

    return mock_ret;
}
