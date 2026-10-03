#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_string_catalog_filter_source_publish(void *source, size_t source_size, const void *image,
                                                             size_t image_size, uint64_t catalog_id,
                                                             const cplat_string_catalog_filter_source_lock *lock,
                                                             uint64_t *revision_out)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_string_catalog_filter_source_publish)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_string_catalog_filter_source_publish"));

    return real_fn(source, source_size, image, image_size, catalog_id, lock, revision_out);
}

MOCK_WEAK_IMPL(int, cplat_string_catalog_filter_source_publish, void *source, size_t source_size, const void *image,
               size_t image_size, uint64_t catalog_id, const cplat_string_catalog_filter_source_lock *lock,
               uint64_t *revision_out)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_string_catalog_filter_source_publish(source, source_size, image, image_size,
                                                                           catalog_id, lock, revision_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_string_catalog_filter_source_publish(source, source_size, image, image_size,
                                                                            catalog_id, lock, revision_out);
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
