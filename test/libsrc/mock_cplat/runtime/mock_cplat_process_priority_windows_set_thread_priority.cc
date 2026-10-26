#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_WINDOWS)

int delegate_real_cplat_process_priority_windows_set_thread_priority(uint32_t thread_id, cplat_process_priority_windows_thread_priority thread_priority, cplat_error *detail_out)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_process_priority_windows_set_thread_priority)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_process_priority_windows_set_thread_priority"));

    return real_fn(thread_id, thread_priority, detail_out);
}

MOCK_WEAK_IMPL(int, cplat_process_priority_windows_set_thread_priority, uint32_t thread_id, cplat_process_priority_windows_thread_priority thread_priority, cplat_error *detail_out)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_process_priority_windows_set_thread_priority(thread_id, thread_priority, detail_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_process_priority_windows_set_thread_priority(thread_id, thread_priority, detail_out);
    }

    return mock_ret;
}

#endif /* PLATFORM_WINDOWS */
