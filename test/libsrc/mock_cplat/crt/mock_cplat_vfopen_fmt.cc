#include <stdarg.h>
#include <vector>
#include <stdio.h>
#include <testfw.h>
#include <mock_cplat.h>

FILE *delegate_real_cplat_vfopen_fmt(const char *modes, cplat_error *detail_out, const char *format, va_list args)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_vfopen_fmt)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_vfopen_fmt"));

    return real_fn(modes, detail_out, format, args);
}

MOCK_WEAK_IMPL(FILE *, cplat_vfopen_fmt, const char *modes, cplat_error *detail_out, const char *format, va_list args)
{
    FILE *mock_ret = nullptr;

    std::vector<char> buf = mock_cplat_expand_format(format, args);

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_vfopen_fmt(modes, detail_out, buf.data());
    }
    else
    {
        mock_ret = delegate_real_cplat_vfopen_fmt(modes, detail_out, format, args);
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
