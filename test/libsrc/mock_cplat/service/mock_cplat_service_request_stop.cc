#include <testfw.h>
#include <mock_cplat.h>

void delegate_real_cplat_service_request_stop(void)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_service_request_stop)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_service_request_stop"));

    real_fn();
}

MOCK_WEAK_IMPL(void, cplat_service_request_stop, void)
{
    if (_mock_cplat != nullptr)
    {
        _mock_cplat->cplat_service_request_stop();
    }
    else
    {
        delegate_real_cplat_service_request_stop();
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }
}
