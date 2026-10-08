#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_regex_iter_next(cplat_regex_iter *iter, cplat_regex_match *matches_out, size_t matches_capacity,
                                        int *has_match_out, cplat_error *detail_out)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_regex_iter_next)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_regex_iter_next"));

    return real_fn(iter, matches_out, matches_capacity, has_match_out, detail_out);
}

MOCK_WEAK_IMPL(int, cplat_regex_iter_next, cplat_regex_iter *iter, cplat_regex_match *matches_out,
               size_t matches_capacity, int *has_match_out, cplat_error *detail_out)
{
    int mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_regex_iter_next(iter, matches_out, matches_capacity, has_match_out, detail_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_regex_iter_next(iter, matches_out, matches_capacity, has_match_out, detail_out);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }

    return mock_ret;
}
