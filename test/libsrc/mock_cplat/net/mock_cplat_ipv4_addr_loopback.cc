#include <testfw.h>
#include <mock_cplat.h>

uint32_t delegate_real_cplat_ipv4_addr_loopback(void)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_ipv4_addr_loopback)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_ipv4_addr_loopback"));
    return real_fn();
}

MOCK_WEAK_IMPL(uint32_t, cplat_ipv4_addr_loopback, void)
{
    uint32_t mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_ipv4_addr_loopback();
    }
    else
    {
        mock_ret = delegate_real_cplat_ipv4_addr_loopback();
    }

    return mock_ret;
}
