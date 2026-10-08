#include <testfw.h>
#include <mock_cplat.h>

void delegate_real_cplat_tracer_call_next_hook(cplat_tracer_hook_entry *prev, cplat_tracer *handle,
                                               cplat_trace_level level, const cplat_timespec *timestamp,
                                               const char *message)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_tracer_call_next_hook)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_tracer_call_next_hook"));

    real_fn(prev, handle, level, timestamp, message);
}

MOCK_WEAK_IMPL(void, cplat_tracer_call_next_hook, cplat_tracer_hook_entry *prev, cplat_tracer *handle,
               cplat_trace_level level, const cplat_timespec *timestamp, const char *message)
{
    if (_mock_cplat != nullptr)
    {
        _mock_cplat->cplat_tracer_call_next_hook(prev, handle, level, timestamp, message);
    }
    else
    {
        delegate_real_cplat_tracer_call_next_hook(prev, handle, level, timestamp, message);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p, 0x%p, %d, 0x%p, %s\n", __func__, (void *)prev, (void *)handle, (int)level,
               (const void *)timestamp, message);
    }
}
