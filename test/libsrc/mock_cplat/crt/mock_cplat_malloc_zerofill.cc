#include <testfw.h>
#include <mock_cplat.h>

void *delegate_real_cplat_malloc_zerofill(size_t size)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_malloc_zerofill)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_malloc_zerofill"));

    return real_fn(size);
}

MOCK_WEAK_IMPL(void *, cplat_malloc_zerofill, size_t size)
{
    void *mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_malloc_zerofill(size);
    }
    else
    {
        mock_ret = delegate_real_cplat_malloc_zerofill(size);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }

    return mock_ret;
}
