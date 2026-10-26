#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_LINUX)

int delegate_real_cplat_process_priority_linux_get_policy(uint32_t thread_id, cplat_process_priority_linux_policy *policy_out, int *static_priority_out, int *reset_on_fork_out, cplat_error *detail_out)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_process_priority_linux_get_policy)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_process_priority_linux_get_policy"));

    return real_fn(thread_id, policy_out, static_priority_out, reset_on_fork_out, detail_out);
}

MOCK_WEAK_IMPL(int, cplat_process_priority_linux_get_policy, uint32_t thread_id, cplat_process_priority_linux_policy *policy_out, int *static_priority_out, int *reset_on_fork_out, cplat_error *detail_out)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_process_priority_linux_get_policy(thread_id, policy_out, static_priority_out, reset_on_fork_out, detail_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_process_priority_linux_get_policy(thread_id, policy_out, static_priority_out, reset_on_fork_out, detail_out);
    }

    return mock_ret;
}

#endif /* PLATFORM_LINUX */
