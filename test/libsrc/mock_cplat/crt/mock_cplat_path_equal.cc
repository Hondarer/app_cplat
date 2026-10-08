#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_path_equal(const char *lhs, const char *rhs, int *equal_out, cplat_error *detail_out)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_path_equal)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_path_equal"));

    return real_fn(lhs, rhs, equal_out, detail_out);
}

MOCK_WEAK_IMPL(int, cplat_path_equal, const char *lhs, const char *rhs, int *equal_out, cplat_error *detail_out)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_path_equal(lhs, rhs, equal_out, detail_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_path_equal(lhs, rhs, equal_out, detail_out);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        const char *lhs_text = "(null)";
        const char *rhs_text = "(null)";
        if (lhs != nullptr)
        {
            lhs_text = lhs;
        }
        if (rhs != nullptr)
        {
            rhs_text = rhs;
        }
        printf("  > %s %s %s", __func__, lhs_text, rhs_text);
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
