#include <stdarg.h>
#include <vector>
#include <stdio.h>
#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_vaccess_fmt(int mode, cplat_error *detail_out, const char *format, va_list args)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_vaccess_fmt)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_vaccess_fmt"));

    return real_fn(mode, detail_out, format, args);
}

MOCK_WEAK_IMPL(int, cplat_vaccess_fmt, int mode, cplat_error *detail_out, const char *format, va_list args)
{
    int mock_ret = -1;

    std::vector<char> buf = mock_cplat_expand_format(format, args);

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_vaccess_fmt(mode, detail_out, buf.data());
    }
    else
    {
        mock_ret = delegate_real_cplat_vaccess_fmt(mode, detail_out, format, args);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s %d, %s", __func__, mode, buf.data());
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
