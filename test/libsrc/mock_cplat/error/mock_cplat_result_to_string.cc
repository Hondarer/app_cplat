#include <testfw.h>
#include <mock_cplat.h>

const char *delegate_real_cplat_result_to_string(int result)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_result_to_string)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_result_to_string"));

    return real_fn(result);
}

MOCK_WEAK_IMPL(const char *, cplat_result_to_string, int result)
{
    const char *mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_result_to_string(result);
    }
    else
    {
        mock_ret = delegate_real_cplat_result_to_string(result);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }

    return mock_ret;
}
