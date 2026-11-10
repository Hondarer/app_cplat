#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_process_run_sync(const cplat_process_options *options, int timeout_ms, int *exit_code_out)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_process_run_sync)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_process_run_sync"));

    return real_fn(options, timeout_ms, exit_code_out);
}

MOCK_WEAK_IMPL(int, cplat_process_run_sync, const cplat_process_options *options, int timeout_ms, int *exit_code_out)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_process_run_sync(options, timeout_ms, exit_code_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_process_run_sync(options, timeout_ms, exit_code_out);
    }

    return mock_ret;
}
