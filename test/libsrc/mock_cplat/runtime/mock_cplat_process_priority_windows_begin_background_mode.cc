#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_WINDOWS)

int delegate_real_cplat_process_priority_windows_begin_background_mode(cplat_error *detail_out)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_process_priority_windows_begin_background_mode)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_process_priority_windows_begin_background_mode"));

    return real_fn(detail_out);
}

MOCK_WEAK_IMPL(int, cplat_process_priority_windows_begin_background_mode, cplat_error *detail_out)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_process_priority_windows_begin_background_mode(detail_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_process_priority_windows_begin_background_mode(detail_out);
    }

    return mock_ret;
}

#endif /* PLATFORM_WINDOWS */
