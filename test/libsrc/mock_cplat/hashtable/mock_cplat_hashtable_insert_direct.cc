#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_hashtable_insert_direct(cplat_hashtable *ht, uint64_t record, const void *key, int status,
                                                const void *value, const cplat_timespec *timestamp, uint64_t generation)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_hashtable_insert_direct)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_hashtable_insert_direct"));

    return real_fn(ht, record, key, status, value, timestamp, generation);
}

MOCK_WEAK_IMPL(int, cplat_hashtable_insert_direct, cplat_hashtable *ht, uint64_t record, const void *key, int status,
               const void *value, const cplat_timespec *timestamp, uint64_t generation)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_hashtable_insert_direct(ht, record, key, status, value, timestamp, generation);
    }
    else
    {
        mock_ret = delegate_real_cplat_hashtable_insert_direct(ht, record, key, status, value, timestamp, generation);
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
