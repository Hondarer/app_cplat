#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_hashtable_delete_with_key_length(cplat_hashtable *ht, const void *key, size_t key_length)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_hashtable_delete_with_key_length)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_hashtable_delete_with_key_length"));

    return real_fn(ht, key, key_length);
}

MOCK_WEAK_IMPL(int, cplat_hashtable_delete_with_key_length, cplat_hashtable *ht, const void *key, size_t key_length)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_hashtable_delete_with_key_length(ht, key, key_length);
    }
    else
    {
        mock_ret = delegate_real_cplat_hashtable_delete_with_key_length(ht, key, key_length);
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
