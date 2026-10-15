#include <testfw.h>
#include <mock_cplat.h>

void delegate_real_cplat_thread_yield(void)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_thread_yield)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_thread_yield"));

    real_fn();
}

MOCK_WEAK_IMPL(void, cplat_thread_yield, void)
{
    if (_mock_cplat != nullptr)
    {
        _mock_cplat->cplat_thread_yield();
    }
    else
    {
        delegate_real_cplat_thread_yield();
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }
}
