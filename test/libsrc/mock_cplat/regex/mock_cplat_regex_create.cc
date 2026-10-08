#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_regex_create(const char *pattern, unsigned int flags, cplat_regex **regex_out,
                                     cplat_error *detail_out)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_regex_create)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_regex_create"));

    return real_fn(pattern, flags, regex_out, detail_out);
}

MOCK_WEAK_IMPL(int, cplat_regex_create, const char *pattern, unsigned int flags, cplat_regex **regex_out,
               cplat_error *detail_out)
{
    int mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_regex_create(pattern, flags, regex_out, detail_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_regex_create(pattern, flags, regex_out, detail_out);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }

    return mock_ret;
}
