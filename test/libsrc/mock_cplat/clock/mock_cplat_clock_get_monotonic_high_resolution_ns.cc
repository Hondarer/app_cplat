#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_clock_get_monotonic_high_resolution_ns(uint64_t *nanoseconds_out)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_clock_get_monotonic_high_resolution_ns)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_clock_get_monotonic_high_resolution_ns"));
    return real_fn(nanoseconds_out);
}

MOCK_WEAK_IMPL(int, cplat_clock_get_monotonic_high_resolution_ns, uint64_t *nanoseconds_out)
{
    int mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_clock_get_monotonic_high_resolution_ns(nanoseconds_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_clock_get_monotonic_high_resolution_ns(nanoseconds_out);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s -> %d\n", __func__, mock_ret);
    }

    return mock_ret;
}
