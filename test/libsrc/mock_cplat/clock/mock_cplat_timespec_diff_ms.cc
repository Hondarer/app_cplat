#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_timespec_diff_ms(const cplat_timespec *end, const cplat_timespec *start,
                                         int64_t *milliseconds_out)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_timespec_diff_ms)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_timespec_diff_ms"));
    return real_fn(end, start, milliseconds_out);
}

MOCK_WEAK_IMPL(int, cplat_timespec_diff_ms, const cplat_timespec *end, const cplat_timespec *start,
               int64_t *milliseconds_out)
{
    int mock_ret;
    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_timespec_diff_ms(end, start, milliseconds_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_timespec_diff_ms(end, start, milliseconds_out);
    }
    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }
    return mock_ret;
}
