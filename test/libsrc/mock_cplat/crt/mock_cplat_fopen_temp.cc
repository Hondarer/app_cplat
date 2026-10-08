#include <testfw.h>
#include <mock_cplat.h>

FILE *delegate_real_cplat_fopen_temp(const char *prefix, const char *modes, char *path_out, size_t path_size,
                                     cplat_error *detail_out)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_fopen_temp)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_fopen_temp"));

    return real_fn(prefix, modes, path_out, path_size, detail_out);
}

MOCK_WEAK_IMPL(FILE *, cplat_fopen_temp, const char *prefix, const char *modes, char *path_out, size_t path_size,
               cplat_error *detail_out)
{
    FILE *mock_ret = nullptr;
    const char *prefix_text = "(null)";
    const char *modes_text = "(null)";

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_fopen_temp(prefix, modes, path_out, path_size, detail_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_fopen_temp(prefix, modes, path_out, path_size, detail_out);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        if (prefix != nullptr)
        {
            prefix_text = prefix;
        }
        if (modes != nullptr)
        {
            modes_text = modes;
        }
        printf("  > %s %s, %s, 0x%p, %zu, 0x%p", __func__, prefix_text, modes_text, (void *)path_out, path_size,
               (void *)detail_out);
        if (getTraceLevel() >= TRACE_DETAIL)
        {
            printf(" -> 0x%p\n", (void *)mock_ret);
        }
        else
        {
            printf("\n");
        }
    }

    return mock_ret;
}
