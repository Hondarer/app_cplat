#include <testfw.h>
#include <mock_cplat.h>

void delegate_real_cplat_console_init(void)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_console_init)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_console_init"));

    real_fn();
}

MOCK_WEAK_IMPL(void, cplat_console_init, void)
{
    if (_mock_cplat != nullptr)
    {
        _mock_cplat->cplat_console_init();
    }
    else
    {
        delegate_real_cplat_console_init();
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }
}
