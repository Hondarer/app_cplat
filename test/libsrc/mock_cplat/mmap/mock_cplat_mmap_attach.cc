#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_mmap_attach(const char *path, cplat_mmap_access access, size_t create_size, cplat_mmap **map,
                                    cplat_error *detail_out)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_mmap_attach)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_mmap_attach"));

    return real_fn(path, access, create_size, map, detail_out);
}

MOCK_WEAK_IMPL(int, cplat_mmap_attach, const char *path, cplat_mmap_access access, size_t create_size, cplat_mmap **map,
               cplat_error *detail_out)
{
    int mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_mmap_attach(path, access, create_size, map, detail_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_mmap_attach(path, access, create_size, map, detail_out);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }

    return mock_ret;
}
