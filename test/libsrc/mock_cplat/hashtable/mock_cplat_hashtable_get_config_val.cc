#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_hashtable_get_config_val(const cplat_hashtable *ht, cplat_hashtable_config *config_out)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_hashtable_get_config_val)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_hashtable_get_config_val"));

    return real_fn(ht, config_out);
}

MOCK_WEAK_IMPL(int, cplat_hashtable_get_config_val, const cplat_hashtable *ht, cplat_hashtable_config *config_out)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_hashtable_get_config_val(ht, config_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_hashtable_get_config_val(ht, config_out);
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
