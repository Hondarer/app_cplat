#include <testfw.h>
#include <mock_cplat.h>

void delegate_real_cplat_exit(int code)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_exit)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_exit"));

    real_fn(code);
}

MOCK_WEAK_IMPL(void, cplat_exit, int code)
{
    if (_mock_cplat != nullptr)
    {
        _mock_cplat->cplat_exit(code);
    }
    else
    {
        delegate_real_cplat_exit(code);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }
}
