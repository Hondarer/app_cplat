#include <testfw.h>
#include <mock_cplat.h>

void delegate_real_cplat_regex_dispose(cplat_regex *regex)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_regex_dispose)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_regex_dispose"));

    real_fn(regex);
}

MOCK_WEAK_IMPL(void, cplat_regex_dispose, cplat_regex *regex)
{
    if (_mock_cplat != nullptr)
    {
        _mock_cplat->cplat_regex_dispose(regex);
    }
    else
    {
        delegate_real_cplat_regex_dispose(regex);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }
}
