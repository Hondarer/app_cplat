#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_timespec_add_ms(const cplat_timespec *ts, uint64_t timeout_ms, cplat_timespec *sum_out)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_timespec_add_ms)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_timespec_add_ms"));
    return real_fn(ts, timeout_ms, sum_out);
}

MOCK_WEAK_IMPL(int, cplat_timespec_add_ms, const cplat_timespec *ts, uint64_t timeout_ms, cplat_timespec *sum_out)
{
    int mock_ret;
    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_timespec_add_ms(ts, timeout_ms, sum_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_timespec_add_ms(ts, timeout_ms, sum_out);
    }
    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }
    return mock_ret;
}
