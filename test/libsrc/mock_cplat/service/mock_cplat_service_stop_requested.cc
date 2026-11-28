#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_service_stop_requested(void)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_service_stop_requested)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_service_stop_requested"));

    return real_fn();
}

MOCK_WEAK_IMPL(int, cplat_service_stop_requested, void)
{
    int mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_service_stop_requested();
    }
    else
    {
        mock_ret = delegate_real_cplat_service_stop_requested();
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
