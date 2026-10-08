#include <stdarg.h>
#include <vector>
#include <stdio.h>
#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_vstat_fmt(cplat_file_stat_t *buf, cplat_error *detail_out, const char *format, va_list args)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_vstat_fmt)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_vstat_fmt"));

    return real_fn(buf, detail_out, format, args);
}

MOCK_WEAK_IMPL(int, cplat_vstat_fmt, cplat_file_stat_t *buf, cplat_error *detail_out, const char *format, va_list args)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    std::vector<char> path = mock_cplat_expand_format(format, args);

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_vstat_fmt(buf, detail_out, path.data());
    }
    else
    {
        mock_ret = delegate_real_cplat_vstat_fmt(buf, detail_out, format, args);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p, %s", __func__, (void *)buf, path.data());
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
