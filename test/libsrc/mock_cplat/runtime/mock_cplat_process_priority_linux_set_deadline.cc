#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_LINUX)

int delegate_real_cplat_process_priority_linux_set_deadline(uint32_t thread_id, const cplat_process_priority_linux_deadline *deadline, int reset_on_fork, cplat_error *detail_out)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_process_priority_linux_set_deadline)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_process_priority_linux_set_deadline"));

    return real_fn(thread_id, deadline, reset_on_fork, detail_out);
}

MOCK_WEAK_IMPL(int, cplat_process_priority_linux_set_deadline, uint32_t thread_id, const cplat_process_priority_linux_deadline *deadline, int reset_on_fork, cplat_error *detail_out)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_process_priority_linux_set_deadline(thread_id, deadline, reset_on_fork, detail_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_process_priority_linux_set_deadline(thread_id, deadline, reset_on_fork, detail_out);
    }

    return mock_ret;
}

#endif /* PLATFORM_LINUX */
