#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_regex_replace(const cplat_regex *regex, const char *text, size_t text_len,
                                      const char *replacement, unsigned int flags, char *result_out, size_t result_size,
                                      size_t *required_size_out, cplat_error *detail_out)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_regex_replace)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_regex_replace"));

    return real_fn(regex, text, text_len, replacement, flags, result_out, result_size, required_size_out, detail_out);
}

MOCK_WEAK_IMPL(int, cplat_regex_replace, const cplat_regex *regex, const char *text, size_t text_len,
               const char *replacement, unsigned int flags, char *result_out, size_t result_size,
               size_t *required_size_out, cplat_error *detail_out)
{
    int mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_regex_replace(regex, text, text_len, replacement, flags, result_out, result_size,
                                                    required_size_out, detail_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_regex_replace(regex, text, text_len, replacement, flags, result_out, result_size,
                                                     required_size_out, detail_out);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }

    return mock_ret;
}
