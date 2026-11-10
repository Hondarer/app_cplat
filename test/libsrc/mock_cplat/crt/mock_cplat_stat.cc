#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_stat(cplat_file_stat_t *stat_out, cplat_error *detail_out, const char *path)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_stat)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_stat"));

    return real_fn(stat_out, detail_out, path);
}

MOCK_WEAK_IMPL(int, cplat_stat, cplat_file_stat_t *stat_out, cplat_error *detail_out, const char *path)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_stat(stat_out, detail_out, path);
    }
    else
    {
        mock_ret = delegate_real_cplat_stat(stat_out, detail_out, path);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p, %s", __func__, (void *)stat_out, path);
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
