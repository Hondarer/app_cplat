#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_tracer_write_hex_at(cplat_tracer *handle, cplat_trace_level level,
                                            const cplat_timespec *timestamp, const void *data, size_t size,
                                            const char *message)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_tracer_write_hex_at)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_tracer_write_hex_at"));

    return real_fn(handle, level, timestamp, data, size, message);
}

MOCK_WEAK_IMPL(int, cplat_tracer_write_hex_at, cplat_tracer *handle, cplat_trace_level level,
               const cplat_timespec *timestamp, const void *data, size_t size, const char *message)
{
    int mock_ret = 0;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_tracer_write_hex_at(handle, level, timestamp, data, size, message);
    }
    else
    {
        mock_ret = delegate_real_cplat_tracer_write_hex_at(handle, level, timestamp, data, size, message);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p, %d, 0x%p, 0x%p, %zu, %s", __func__, (void *)handle, (int)level, (const void *)timestamp,
               data, size, message);
        if (getTraceLevel() >= TRACE_DETAIL)
        {
            printf(" -> %d\n", mock_ret);
        }
        else
        {
            printf("\n");
        }
    }

    return mock_ret;
}
