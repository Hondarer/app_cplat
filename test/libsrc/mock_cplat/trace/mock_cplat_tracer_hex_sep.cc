#include <testfw.h>
#include <mock_cplat.h>

const char *delegate_real_cplat_tracer_hex_sep(const char *message)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_tracer_hex_sep)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_tracer_hex_sep"));

    return real_fn(message);
}

MOCK_WEAK_IMPL(const char *, cplat_tracer_hex_sep, const char *message)
{
    const char *mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_tracer_hex_sep(message);
    }
    else
    {
        mock_ret = delegate_real_cplat_tracer_hex_sep(message);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s %s -> %s\n", __func__, message == NULL ? "(null)" : message,
               mock_ret == NULL ? "(null)" : mock_ret);
    }

    return mock_ret;
}
