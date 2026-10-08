#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_tracer_write_with_source(cplat_tracer *handle, cplat_trace_level level,
                                                 const cplat_timespec *timestamp, const char *file, int line,
                                                 const char *message)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_tracer_write_with_source)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_tracer_write_with_source"));

    return real_fn(handle, level, timestamp, file, line, message);
}

MOCK_WEAK_IMPL(int, cplat_tracer_write_with_source, cplat_tracer *handle, cplat_trace_level level,
               const cplat_timespec *timestamp, const char *file, int line, const char *message)
{
    int mock_ret = 0;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_tracer_write_with_source(handle, level, timestamp, file, line, message);
    }
    else
    {
        mock_ret = delegate_real_cplat_tracer_write_with_source(handle, level, timestamp, file, line, message);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p, %d, %s:%d, %s -> %d\n", __func__, (void *)handle, (int)level,
               file == NULL ? "(null)" : file, line, message == NULL ? "(null)" : message, mock_ret);
    }

    return mock_ret;
}
