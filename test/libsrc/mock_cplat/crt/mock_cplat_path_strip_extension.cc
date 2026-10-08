#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_path_strip_extension(char *path_out, size_t path_size, cplat_error *detail_out,
                                             const char *path)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_path_strip_extension)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_path_strip_extension"));

    return real_fn(path_out, path_size, detail_out, path);
}

MOCK_WEAK_IMPL(int, cplat_path_strip_extension, char *path_out, size_t path_size, cplat_error *detail_out,
               const char *path)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_path_strip_extension(path_out, path_size, detail_out, path);
    }
    else
    {
        mock_ret = delegate_real_cplat_path_strip_extension(path_out, path_size, detail_out, path);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        const char *path_text = "(null)";
        if (path != nullptr)
        {
            path_text = path;
        }
        printf("  > %s %s", __func__, path_text);
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
