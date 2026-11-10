#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_process_get_exit_code(cplat_process *process, int *exit_code_out)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_process_get_exit_code)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_process_get_exit_code"));

    return real_fn(process, exit_code_out);
}

MOCK_WEAK_IMPL(int, cplat_process_get_exit_code, cplat_process *process, int *exit_code_out)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_process_get_exit_code(process, exit_code_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_process_get_exit_code(process, exit_code_out);
    }

    return mock_ret;
}
