#include <testfw.h>
#include <mock_cplat.h>

void delegate_real_cplat_timespec_normalize(cplat_timespec *ts)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_timespec_normalize)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_timespec_normalize"));

    real_fn(ts);
}

MOCK_WEAK_IMPL(void, cplat_timespec_normalize, cplat_timespec *ts)
{
    if (_mock_cplat != nullptr)
    {
        _mock_cplat->cplat_timespec_normalize(ts);
    }
    else
    {
        delegate_real_cplat_timespec_normalize(ts);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }
}
