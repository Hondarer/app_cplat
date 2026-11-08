#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_clock_get_realtime(cplat_timespec *ts)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_clock_get_realtime)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_clock_get_realtime"));
    return real_fn(ts);
}

MOCK_WEAK_IMPL(int, cplat_clock_get_realtime, cplat_timespec *ts)
{
    int mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_clock_get_realtime(ts);
    }
    else
    {
        mock_ret = delegate_real_cplat_clock_get_realtime(ts);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s -> %d\n", __func__, mock_ret);
    }

    return mock_ret;
}
