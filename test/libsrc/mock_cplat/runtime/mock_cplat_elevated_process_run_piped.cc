#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_elevated_process_run_piped(const char *arguments, cplat_elevated_process_output_fn output_fn,
                                                   void *context, int *exit_code, int *handled)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_elevated_process_run_piped)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_elevated_process_run_piped"));

    return real_fn(arguments, output_fn, context, exit_code, handled);
}

MOCK_WEAK_IMPL(int, cplat_elevated_process_run_piped, const char *arguments, cplat_elevated_process_output_fn output_fn,
               void *context, int *exit_code, int *handled)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_elevated_process_run_piped(arguments, output_fn, context, exit_code, handled);
    }
    else
    {
        mock_ret = delegate_real_cplat_elevated_process_run_piped(arguments, output_fn, context, exit_code, handled);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s ", __func__);
        if (arguments != nullptr)
        {
            printf("%s", arguments);
        }
        else
        {
            printf("(null)");
        }
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
