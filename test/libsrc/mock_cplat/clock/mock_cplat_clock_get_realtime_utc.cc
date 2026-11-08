#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_clock_get_realtime_utc(struct tm *utc_tm, int32_t *tv_nsec)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_clock_get_realtime_utc)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_clock_get_realtime_utc"));
    return real_fn(utc_tm, tv_nsec);
}

MOCK_WEAK_IMPL(int, cplat_clock_get_realtime_utc, struct tm *utc_tm, int32_t *tv_nsec)
{
    int mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_clock_get_realtime_utc(utc_tm, tv_nsec);
    }
    else
    {
        mock_ret = delegate_real_cplat_clock_get_realtime_utc(utc_tm, tv_nsec);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s -> %d\n", __func__, mock_ret);
    }

    return mock_ret;
}
