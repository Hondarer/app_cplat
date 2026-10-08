#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_hashtable_add(cplat_hashtable *ht, const void *key, const void *value,
                                      cplat_hashtable_add_deleted_policy deleted_policy)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_hashtable_add)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_hashtable_add"));

    return real_fn(ht, key, value, deleted_policy);
}

MOCK_WEAK_IMPL(int, cplat_hashtable_add, cplat_hashtable *ht, const void *key, const void *value,
               cplat_hashtable_add_deleted_policy deleted_policy)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_hashtable_add(ht, key, value, deleted_policy);
    }
    else
    {
        mock_ret = delegate_real_cplat_hashtable_add(ht, key, value, deleted_policy);
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
