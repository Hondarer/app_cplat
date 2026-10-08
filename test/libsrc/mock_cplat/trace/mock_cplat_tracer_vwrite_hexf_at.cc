#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_tracer_vwrite_hexf_at(cplat_tracer *handle, cplat_trace_level level,
                                              const cplat_timespec *timestamp, const void *data, size_t size,
                                              const char *format, va_list args)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_tracer_vwrite_hexf_at)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_tracer_vwrite_hexf_at"));

    return real_fn(handle, level, timestamp, data, size, format, args);
}

MOCK_WEAK_IMPL(int, cplat_tracer_vwrite_hexf_at, cplat_tracer *handle, cplat_trace_level level,
               const cplat_timespec *timestamp, const void *data, size_t size, const char *format, va_list args)
{
    int mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_tracer_vwrite_hexf_at(handle, level, timestamp, data, size, format, args);
    }
    else
    {
        mock_ret = delegate_real_cplat_tracer_vwrite_hexf_at(handle, level, timestamp, data, size, format, args);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }

    return mock_ret;
}
