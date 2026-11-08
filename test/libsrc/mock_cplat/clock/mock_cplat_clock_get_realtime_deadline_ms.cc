#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_clock_get_realtime_deadline_ms(uint64_t timeout_ms, struct timespec *deadline_out)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_clock_get_realtime_deadline_ms)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_clock_get_realtime_deadline_ms"));
    return real_fn(timeout_ms, deadline_out);
}

MOCK_WEAK_IMPL(int, cplat_clock_get_realtime_deadline_ms, uint64_t timeout_ms, struct timespec *deadline_out)
{
    int mock_ret;
    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_clock_get_realtime_deadline_ms(timeout_ms, deadline_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_clock_get_realtime_deadline_ms(timeout_ms, deadline_out);
    }
    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }
    return mock_ret;
}
