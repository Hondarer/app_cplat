#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_LINUX)

int delegate_real_cplat_process_priority_linux_get_utilization_clamp(uint32_t thread_id, uint32_t *minimum_out, uint32_t *maximum_out, cplat_error *detail_out)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_process_priority_linux_get_utilization_clamp)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_process_priority_linux_get_utilization_clamp"));

    return real_fn(thread_id, minimum_out, maximum_out, detail_out);
}

MOCK_WEAK_IMPL(int, cplat_process_priority_linux_get_utilization_clamp, uint32_t thread_id, uint32_t *minimum_out, uint32_t *maximum_out, cplat_error *detail_out)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_process_priority_linux_get_utilization_clamp(thread_id, minimum_out, maximum_out, detail_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_process_priority_linux_get_utilization_clamp(thread_id, minimum_out, maximum_out, detail_out);
    }

    return mock_ret;
}

#endif /* PLATFORM_LINUX */
