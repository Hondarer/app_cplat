#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_regex_matches(const cplat_regex *regex, const char *text, size_t text_len,
                                      unsigned int match_flags, cplat_regex_match *matches_out, size_t matches_capacity,
                                      int *matched_out, cplat_error *detail_out)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_regex_matches)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_regex_matches"));

    return real_fn(regex, text, text_len, match_flags, matches_out, matches_capacity, matched_out, detail_out);
}

MOCK_WEAK_IMPL(int, cplat_regex_matches, const cplat_regex *regex, const char *text, size_t text_len,
               unsigned int match_flags, cplat_regex_match *matches_out, size_t matches_capacity, int *matched_out,
               cplat_error *detail_out)
{
    int mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_regex_matches(regex, text, text_len, match_flags, matches_out, matches_capacity,
                                                    matched_out, detail_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_regex_matches(regex, text, text_len, match_flags, matches_out, matches_capacity,
                                                     matched_out, detail_out);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }

    return mock_ret;
}
