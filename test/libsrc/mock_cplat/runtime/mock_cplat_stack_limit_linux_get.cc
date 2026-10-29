#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_LINUX)

int delegate_real_cplat_stack_limit_linux_get(uint64_t *soft_limit_bytes_out, uint64_t *hard_limit_bytes_out, cplat_error *detail_out)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_stack_limit_linux_get)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_stack_limit_linux_get"));

    return real_fn(soft_limit_bytes_out, hard_limit_bytes_out, detail_out);
}

MOCK_WEAK_IMPL(int, cplat_stack_limit_linux_get, uint64_t *soft_limit_bytes_out, uint64_t *hard_limit_bytes_out, cplat_error *detail_out)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_stack_limit_linux_get(soft_limit_bytes_out, hard_limit_bytes_out, detail_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_stack_limit_linux_get(soft_limit_bytes_out, hard_limit_bytes_out, detail_out);
    }

    return mock_ret;
}

#endif /* PLATFORM_LINUX */
