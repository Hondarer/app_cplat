#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_timespec_cmp(const cplat_timespec *a, const cplat_timespec *b)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_timespec_cmp)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_timespec_cmp"));

    return real_fn(a, b);
}

MOCK_WEAK_IMPL(int, cplat_timespec_cmp, const cplat_timespec *a, const cplat_timespec *b)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_timespec_cmp(a, b);
    }
    else
    {
        mock_ret = delegate_real_cplat_timespec_cmp(a, b);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s", __func__);
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
