#include <stdarg.h>
#include <vector>
#include <stdio.h>
#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_mkdir_fmt(cplat_error *detail_out, const char *format, ...)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_mkdir_fmt)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_mkdir_fmt"));

    return real_fn(detail_out, "%s", format);
}

MOCK_WEAK_IMPL(int, cplat_mkdir_fmt, cplat_error *detail_out, const char *format, ...)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    std::vector<char> buf;
    {
        va_list args;
        va_start(args, format);
        buf = mock_cplat_expand_format(format, args);
        va_end(args);
    }

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_mkdir_fmt(detail_out, buf.data());
    }
    else
    {
        mock_ret = delegate_real_cplat_mkdir_fmt(detail_out, buf.data());
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s %s", __func__, buf.data());
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
