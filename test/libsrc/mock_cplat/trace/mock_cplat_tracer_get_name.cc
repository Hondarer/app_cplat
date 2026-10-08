#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_tracer_get_name(cplat_tracer *handle, char *name_out, size_t name_size)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_tracer_get_name)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_tracer_get_name"));

    return real_fn(handle, name_out, name_size);
}

MOCK_WEAK_IMPL(int, cplat_tracer_get_name, cplat_tracer *handle, char *name_out, size_t name_size)
{
    int mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_tracer_get_name(handle, name_out, name_size);
    }
    else
    {
        mock_ret = delegate_real_cplat_tracer_get_name(handle, name_out, name_size);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }

    return mock_ret;
}
