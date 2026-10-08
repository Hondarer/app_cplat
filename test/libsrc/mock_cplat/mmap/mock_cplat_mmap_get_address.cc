#include <testfw.h>
#include <mock_cplat.h>

void *delegate_real_cplat_mmap_get_address(const cplat_mmap *map)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_mmap_get_address)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_mmap_get_address"));

    return real_fn(map);
}

MOCK_WEAK_IMPL(void *, cplat_mmap_get_address, const cplat_mmap *map)
{
    void *mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_mmap_get_address(map);
    }
    else
    {
        mock_ret = delegate_real_cplat_mmap_get_address(map);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }

    return mock_ret;
}
