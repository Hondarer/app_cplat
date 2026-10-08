#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_mmap_flush(cplat_mmap *map, void *address, size_t length, cplat_error *detail_out)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_mmap_flush)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_mmap_flush"));

    return real_fn(map, address, length, detail_out);
}

MOCK_WEAK_IMPL(int, cplat_mmap_flush, cplat_mmap *map, void *address, size_t length, cplat_error *detail_out)
{
    int mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_mmap_flush(map, address, length, detail_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_mmap_flush(map, address, length, detail_out);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }

    return mock_ret;
}
