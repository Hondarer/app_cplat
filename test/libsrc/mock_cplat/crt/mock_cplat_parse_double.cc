#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_parse_double(double *value_out, const char *text)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_parse_double)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_parse_double"));

    return real_fn(value_out, text);
}

MOCK_WEAK_IMPL(int, cplat_parse_double, double *value_out, const char *text)
{
    int mock_ret = -1;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_parse_double(value_out, text);
    }
    else
    {
        mock_ret = delegate_real_cplat_parse_double(value_out, text);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p, %s", __func__, (void *)value_out, text);
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
