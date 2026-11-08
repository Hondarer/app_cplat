#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_clock_get_monotonic_ms(uint64_t *milliseconds_out)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_clock_get_monotonic_ms)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_clock_get_monotonic_ms"));
    return real_fn(milliseconds_out);
}

MOCK_WEAK_IMPL(int, cplat_clock_get_monotonic_ms, uint64_t *milliseconds_out)
{
    int mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_clock_get_monotonic_ms(milliseconds_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_clock_get_monotonic_ms(milliseconds_out);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s -> %d\n", __func__, mock_ret);
    }

    return mock_ret;
}
