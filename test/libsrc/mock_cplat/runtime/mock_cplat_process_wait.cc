#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_process_wait(cplat_process *process, int timeout_ms)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_process_wait)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_process_wait"));

    return real_fn(process, timeout_ms);
}

MOCK_WEAK_IMPL(int, cplat_process_wait, cplat_process *process, int timeout_ms)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_process_wait(process, timeout_ms);
    }
    else
    {
        mock_ret = delegate_real_cplat_process_wait(process, timeout_ms);
    }

    return mock_ret;
}
