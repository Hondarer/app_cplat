#include <testfw.h>
#include <mock_cplat.h>

int64_t delegate_real_cplat_tracer_get_file_identifier(cplat_tracer *handle)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_tracer_get_file_identifier)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_tracer_get_file_identifier"));

    return real_fn(handle);
}

MOCK_WEAK_IMPL(int64_t, cplat_tracer_get_file_identifier, cplat_tracer *handle)
{
    int64_t mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_tracer_get_file_identifier(handle);
    }
    else
    {
        mock_ret = delegate_real_cplat_tracer_get_file_identifier(handle);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }

    return mock_ret;
}
