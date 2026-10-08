#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_string_catalog_filter_slot_check(cplat_string_catalog_filter_slot *slot, const void *image,
                                                         size_t image_size,
                                                         cplat_string_catalog_filter_diagnostic *diagnostics,
                                                         size_t diagnostic_capacity, size_t *invalid_count_out,
                                                         cplat_string_catalog_filter_warning *warnings,
                                                         size_t warning_capacity, size_t *warning_count_out)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_string_catalog_filter_slot_check)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_string_catalog_filter_slot_check"));

    return real_fn(slot, image, image_size, diagnostics, diagnostic_capacity, invalid_count_out, warnings,
                   warning_capacity, warning_count_out);
}

MOCK_WEAK_IMPL(int, cplat_string_catalog_filter_slot_check, cplat_string_catalog_filter_slot *slot, const void *image,
               size_t image_size, cplat_string_catalog_filter_diagnostic *diagnostics, size_t diagnostic_capacity,
               size_t *invalid_count_out, cplat_string_catalog_filter_warning *warnings, size_t warning_capacity,
               size_t *warning_count_out)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_string_catalog_filter_slot_check(slot, image, image_size, diagnostics,
                                                                       diagnostic_capacity, invalid_count_out, warnings,
                                                                       warning_capacity, warning_count_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_string_catalog_filter_slot_check(slot, image, image_size, diagnostics,
                                                                        diagnostic_capacity, invalid_count_out,
                                                                        warnings, warning_capacity, warning_count_out);
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
