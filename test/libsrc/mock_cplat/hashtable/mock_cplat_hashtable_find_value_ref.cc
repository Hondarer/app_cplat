#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_hashtable_find_value_ref(const cplat_hashtable *ht, const void *key, const void **value_out)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_hashtable_find_value_ref)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_hashtable_find_value_ref"));

    return real_fn(ht, key, value_out);
}

MOCK_WEAK_IMPL(int, cplat_hashtable_find_value_ref, const cplat_hashtable *ht, const void *key, const void **value_out)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_hashtable_find_value_ref(ht, key, value_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_hashtable_find_value_ref(ht, key, value_out);
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
