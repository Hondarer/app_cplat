#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_hashtable_create_growable(const cplat_hashtable_config *initial_config,
                                                  const cplat_hashtable_growth_config *growth_config,
                                                  cplat_hashtable **ht_out)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_hashtable_create_growable)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_hashtable_create_growable"));

    return real_fn(initial_config, growth_config, ht_out);
}

MOCK_WEAK_IMPL(int, cplat_hashtable_create_growable, const cplat_hashtable_config *initial_config,
               const cplat_hashtable_growth_config *growth_config, cplat_hashtable **ht_out)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_hashtable_create_growable(initial_config, growth_config, ht_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_hashtable_create_growable(initial_config, growth_config, ht_out);
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
