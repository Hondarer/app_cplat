#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_hashtable_buffer_size(const cplat_hashtable *ht, size_t *mgmt_size_out, size_t *data_size_out)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_hashtable_buffer_size)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_hashtable_buffer_size"));

    return real_fn(ht, mgmt_size_out, data_size_out);
}

MOCK_WEAK_IMPL(int, cplat_hashtable_buffer_size, const cplat_hashtable *ht, size_t *mgmt_size_out,
               size_t *data_size_out)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_hashtable_buffer_size(ht, mgmt_size_out, data_size_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_hashtable_buffer_size(ht, mgmt_size_out, data_size_out);
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
