#include <testfw.h>
#include <mock_cplat.h>

void delegate_real_cplat_regex_iter_dispose(cplat_regex_iter *iter)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_regex_iter_dispose)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_regex_iter_dispose"));

    real_fn(iter);
}

MOCK_WEAK_IMPL(void, cplat_regex_iter_dispose, cplat_regex_iter *iter)
{
    if (_mock_cplat != nullptr)
    {
        _mock_cplat->cplat_regex_iter_dispose(iter);
    }
    else
    {
        delegate_real_cplat_regex_iter_dispose(iter);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }
}
