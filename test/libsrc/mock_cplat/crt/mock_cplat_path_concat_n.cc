#include <stdarg.h>
#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_path_concat_n(char *path_out, size_t path_size, cplat_error *detail_out, size_t part_count, ...)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_vpath_concat_n)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_vpath_concat_n"));
    va_list args;
    int mock_ret;

    va_start(args, part_count);
    mock_ret = real_fn(path_out, path_size, detail_out, part_count, args);
    va_end(args);
    return mock_ret;
}

MOCK_WEAK_IMPL(int, cplat_path_concat_n, char *path_out, size_t path_size, cplat_error *detail_out, size_t part_count,
               ...)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;
    va_list args;

    va_start(args, part_count);
    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_path_concat_n(path_out, path_size, detail_out, part_count, args);
    }
    else
    {
        mock_ret = delegate_real_cplat_vpath_concat_n(path_out, path_size, detail_out, part_count, args);
    }
    va_end(args);

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s %zu", __func__, part_count);
        if (getTraceLevel() >= TRACE_DETAIL)
        {
            printf(" -> %d\n", mock_ret);
        }
        else
        {
            printf("\n");
        }
    }

    return mock_ret;
}
