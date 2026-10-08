#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_localtime(struct tm *local_tm, const time_t *timep)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_localtime)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_localtime"));

    return real_fn(local_tm, timep);
}

MOCK_WEAK_IMPL(int, cplat_localtime, struct tm *local_tm, const time_t *timep)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_localtime(local_tm, timep);
    }
    else
    {
        mock_ret = delegate_real_cplat_localtime(local_tm, timep);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p, 0x%p", __func__, (void *)local_tm, (const void *)timep);
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
