#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_tracer_start(cplat_tracer *handle)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_tracer_start)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_tracer_start"));

    return real_fn(handle);
}

MOCK_WEAK_IMPL(int, cplat_tracer_start, cplat_tracer *handle)
{
    int mock_ret = 0;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_tracer_start(handle);
    }
    else
    {
        mock_ret = delegate_real_cplat_tracer_start(handle);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p", __func__, (void *)handle);
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
