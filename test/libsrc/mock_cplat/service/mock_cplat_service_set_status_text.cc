#include <testfw.h>
#include <mock_cplat.h>

void delegate_real_cplat_service_set_status_text(const char *text)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_service_set_status_text)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_service_set_status_text"));

    real_fn(text);
}

MOCK_WEAK_IMPL(void, cplat_service_set_status_text, const char *text)
{
    if (_mock_cplat != nullptr)
    {
        _mock_cplat->cplat_service_set_status_text(text);
    }
    else
    {
        delegate_real_cplat_service_set_status_text(text);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }
}
