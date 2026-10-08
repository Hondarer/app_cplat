#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_parse_uint64(uint64_t *value_out, const char *text, int base)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_parse_uint64)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_parse_uint64"));

    return real_fn(value_out, text, base);
}

MOCK_WEAK_IMPL(int, cplat_parse_uint64, uint64_t *value_out, const char *text, int base)
{
    int mock_ret = -1;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_parse_uint64(value_out, text, base);
    }
    else
    {
        mock_ret = delegate_real_cplat_parse_uint64(value_out, text, base);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p, %s, %d", __func__, (void *)value_out, text, base);
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
