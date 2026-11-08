#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_timespec_normalize(cplat_timespec *ts)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_timespec_normalize)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_timespec_normalize"));
    return real_fn(ts);
}

MOCK_WEAK_IMPL(int, cplat_timespec_normalize, cplat_timespec *ts)
{
    int mock_ret;
    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_timespec_normalize(ts);
    }
    else
    {
        mock_ret = delegate_real_cplat_timespec_normalize(ts);
    }
    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }
    return mock_ret;
}
