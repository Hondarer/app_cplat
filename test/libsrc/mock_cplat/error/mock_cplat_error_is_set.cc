#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_error_is_set(const cplat_error *error)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_error_is_set)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_error_is_set"));

    return real_fn(error);
}

MOCK_WEAK_IMPL(int, cplat_error_is_set, const cplat_error *error)
{
    int mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_error_is_set(error);
    }
    else
    {
        mock_ret = delegate_real_cplat_error_is_set(error);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }

    return mock_ret;
}
