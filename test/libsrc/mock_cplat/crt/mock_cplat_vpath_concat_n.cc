#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_vpath_concat_n(char *path_out, size_t path_size, cplat_error *detail_out, size_t part_count,
                                       va_list args)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_vpath_concat_n)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_vpath_concat_n"));

    return real_fn(path_out, path_size, detail_out, part_count, args);
}

MOCK_WEAK_IMPL(int, cplat_vpath_concat_n, char *path_out, size_t path_size, cplat_error *detail_out, size_t part_count,
               va_list args)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_vpath_concat_n(path_out, path_size, detail_out, part_count, args);
    }
    else
    {
        mock_ret = delegate_real_cplat_vpath_concat_n(path_out, path_size, detail_out, part_count, args);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }

    return mock_ret;
}
