#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_service_run_console(const cplat_service_definition *def, int *exit_code_out)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_service_run_console)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_service_run_console"));

    return real_fn(def, exit_code_out);
}

MOCK_WEAK_IMPL(int, cplat_service_run_console, const cplat_service_definition *def, int *exit_code_out)
{
    int mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_service_run_console(def, exit_code_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_service_run_console(def, exit_code_out);
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
