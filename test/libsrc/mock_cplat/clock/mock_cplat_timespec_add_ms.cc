#include <testfw.h>
#include <mock_cplat.h>

void delegate_real_cplat_timespec_add_ms(const cplat_timespec *ts, uint64_t timeout_ms, cplat_timespec *result)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_timespec_add_ms)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_timespec_add_ms"));

    real_fn(ts, timeout_ms, result);
}

MOCK_WEAK_IMPL(void, cplat_timespec_add_ms, const cplat_timespec *ts, uint64_t timeout_ms, cplat_timespec *result)
{
    if (_mock_cplat != nullptr)
    {
        _mock_cplat->cplat_timespec_add_ms(ts, timeout_ms, result);
    }
    else
    {
        delegate_real_cplat_timespec_add_ms(ts, timeout_ms, result);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s %llu\n", __func__, (unsigned long long)timeout_ms);
    }
}
