#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_tracer_write_hex_with_source(cplat_tracer *handle, cplat_trace_level level,
                                                     const cplat_timespec *timestamp, const void *data, size_t size,
                                                     const char *file, int line, const char *message)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_tracer_write_hex_with_source)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_tracer_write_hex_with_source"));
    return real_fn(handle, level, timestamp, data, size, file, line, message);
}

MOCK_WEAK_IMPL(int, cplat_tracer_write_hex_with_source, cplat_tracer *handle, cplat_trace_level level,
               const cplat_timespec *timestamp, const void *data, size_t size, const char *file, int line,
               const char *message)
{
    int mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret =
            _mock_cplat->cplat_tracer_write_hex_with_source(handle, level, timestamp, data, size, file, line, message);
    }
    else
    {
        mock_ret =
            delegate_real_cplat_tracer_write_hex_with_source(handle, level, timestamp, data, size, file, line, message);
    }

    return mock_ret;
}
