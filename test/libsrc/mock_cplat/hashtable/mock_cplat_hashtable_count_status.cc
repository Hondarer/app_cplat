#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_hashtable_count_status(const cplat_hashtable *ht, size_t *in_use_out, size_t *deleted_out,
                                               size_t *empty_out)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_hashtable_count_status)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_hashtable_count_status"));

    return real_fn(ht, in_use_out, deleted_out, empty_out);
}

MOCK_WEAK_IMPL(int, cplat_hashtable_count_status, const cplat_hashtable *ht, size_t *in_use_out, size_t *deleted_out,
               size_t *empty_out)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_hashtable_count_status(ht, in_use_out, deleted_out, empty_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_hashtable_count_status(ht, in_use_out, deleted_out, empty_out);
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
