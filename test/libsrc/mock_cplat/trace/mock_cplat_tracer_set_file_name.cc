#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_tracer_set_file_name(cplat_tracer *handle, const char *name, int64_t identifier)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_tracer_set_file_name)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_tracer_set_file_name"));

    return real_fn(handle, name, identifier);
}

MOCK_WEAK_IMPL(int, cplat_tracer_set_file_name, cplat_tracer *handle, const char *name, int64_t identifier)
{
    int mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_tracer_set_file_name(handle, name, identifier);
    }
    else
    {
        mock_ret = delegate_real_cplat_tracer_set_file_name(handle, name, identifier);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }

    return mock_ret;
}
