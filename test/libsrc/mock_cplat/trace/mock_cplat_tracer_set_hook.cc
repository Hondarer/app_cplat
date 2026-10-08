#include <testfw.h>
#include <mock_cplat.h>

cplat_tracer_hook_entry *delegate_real_cplat_tracer_set_hook(cplat_tracer *handle, cplat_tracer_hook_fn fn,
                                                             void *context)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_tracer_set_hook)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_tracer_set_hook"));

    return real_fn(handle, fn, context);
}

MOCK_WEAK_IMPL(cplat_tracer_hook_entry *, cplat_tracer_set_hook, cplat_tracer *handle, cplat_tracer_hook_fn fn,
               void *context)
{
    cplat_tracer_hook_entry *mock_ret = nullptr;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_tracer_set_hook(handle, fn, context);
    }
    else
    {
        mock_ret = delegate_real_cplat_tracer_set_hook(handle, fn, context);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p, 0x%p, 0x%p", __func__, (void *)handle, (void *)(uintptr_t)fn, context);
        if (getTraceLevel() >= TRACE_DETAIL)
        {
            printf(" -> 0x%p\n", (void *)mock_ret);
        }
        else
        {
            printf("\n");
        }
    }

    return mock_ret;
}
