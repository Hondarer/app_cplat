#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_service_wait_for_stop(int timeout_ms)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_service_wait_for_stop)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_service_wait_for_stop"));

    return real_fn(timeout_ms);
}

MOCK_WEAK_IMPL(int, cplat_service_wait_for_stop, int timeout_ms)
{
    int mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_service_wait_for_stop(timeout_ms);
    }
    else
    {
        mock_ret = delegate_real_cplat_service_wait_for_stop(timeout_ms);
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
