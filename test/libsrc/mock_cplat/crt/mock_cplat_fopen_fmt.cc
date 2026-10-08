#include <stdarg.h>
#include <vector>
#include <stdio.h>
#include <testfw.h>
#include <mock_cplat.h>

FILE *delegate_real_cplat_fopen_fmt(const char *modes, cplat_error *detail_out, const char *format, ...)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_fopen_fmt)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_fopen_fmt"));

    return real_fn(modes, detail_out, "%s", format);
}

MOCK_WEAK_IMPL(FILE *, cplat_fopen_fmt, const char *modes, cplat_error *detail_out, const char *format, ...)
{
    FILE *mock_ret = nullptr;

    std::vector<char> buf;
    {
        va_list args;
        va_start(args, format);
        buf = mock_cplat_expand_format(format, args);
        va_end(args);
    }

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_fopen_fmt(modes, detail_out, buf.data());
    }
    else
    {
        mock_ret = delegate_real_cplat_fopen_fmt(modes, detail_out, buf.data());
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s %s, 0x%p, %s", __func__, modes, (void *)detail_out, buf.data());
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
