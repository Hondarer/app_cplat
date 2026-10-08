#include <testfw.h>
#include <mock_cplat.h>

void delegate_real_cplat_timespec_add(const cplat_timespec *a, const cplat_timespec *b, cplat_timespec *result)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_timespec_add)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_timespec_add"));

    real_fn(a, b, result);
}

MOCK_WEAK_IMPL(void, cplat_timespec_add, const cplat_timespec *a, const cplat_timespec *b, cplat_timespec *result)
{
    if (_mock_cplat != nullptr)
    {
        _mock_cplat->cplat_timespec_add(a, b, result);
    }
    else
    {
        delegate_real_cplat_timespec_add(a, b, result);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }
}
