#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_regex_iter_create(const cplat_regex *regex, const char *text, size_t text_len,
                                          unsigned int match_flags, cplat_regex_iter **iter_out,
                                          cplat_error *detail_out)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_regex_iter_create)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_regex_iter_create"));

    return real_fn(regex, text, text_len, match_flags, iter_out, detail_out);
}

MOCK_WEAK_IMPL(int, cplat_regex_iter_create, const cplat_regex *regex, const char *text, size_t text_len,
               unsigned int match_flags, cplat_regex_iter **iter_out, cplat_error *detail_out)
{
    int mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_regex_iter_create(regex, text, text_len, match_flags, iter_out, detail_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_regex_iter_create(regex, text, text_len, match_flags, iter_out, detail_out);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }

    return mock_ret;
}
