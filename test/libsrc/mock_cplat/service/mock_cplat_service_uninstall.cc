#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_service_uninstall(const cplat_service_definition *def)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_service_uninstall)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_service_uninstall"));

    return real_fn(def);
}

MOCK_WEAK_IMPL(int, cplat_service_uninstall, const cplat_service_definition *def)
{
    int mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_service_uninstall(def);
    }
    else
    {
        mock_ret = delegate_real_cplat_service_uninstall(def);
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
