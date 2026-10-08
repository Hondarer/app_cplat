#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_ipv4_to_string(uint32_t address, char *buffer, size_t buffer_size, cplat_error *detail_out)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_ipv4_to_string)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_ipv4_to_string"));

    return real_fn(address, buffer, buffer_size, detail_out);
}

MOCK_WEAK_IMPL(int, cplat_ipv4_to_string, uint32_t address, char *buffer, size_t buffer_size, cplat_error *detail_out)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_ipv4_to_string(address, buffer, buffer_size, detail_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_ipv4_to_string(address, buffer, buffer_size, detail_out);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s", __func__);
        if (getTraceLevel() >= TRACE_DETAIL)
        {
            printf(" -> %d\n", mock_ret);
        }
        else
        {
            printf("\n");
        }
    }

    return mock_ret;
}
