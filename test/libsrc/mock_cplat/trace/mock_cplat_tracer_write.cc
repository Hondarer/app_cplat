#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_tracer_write_at(cplat_tracer *handle, cplat_trace_level level, const cplat_timespec *timestamp,
                                        const char *message)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_tracer_write_at)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_tracer_write_at"));

    return real_fn(handle, level, timestamp, message);
}

MOCK_WEAK_IMPL(int, cplat_tracer_write_at, cplat_tracer *handle, cplat_trace_level level,
               const cplat_timespec *timestamp, const char *message)
{
    int mock_ret = 0;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_tracer_write_at(handle, level, timestamp, message);
    }
    else
    {
        mock_ret = delegate_real_cplat_tracer_write_at(handle, level, timestamp, message);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p, %d, 0x%p, %s", __func__, (void *)handle, (int)level, (const void *)timestamp, message);
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
