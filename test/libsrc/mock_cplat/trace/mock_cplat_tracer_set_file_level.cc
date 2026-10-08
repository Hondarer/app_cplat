#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_tracer_set_file_level(cplat_tracer *handle, const char *path, cplat_trace_level level,
                                              size_t max_bytes, int generations, int flags)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_tracer_set_file_level)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_tracer_set_file_level"));

    return real_fn(handle, path, level, max_bytes, generations, flags);
}

MOCK_WEAK_IMPL(int, cplat_tracer_set_file_level, cplat_tracer *handle, const char *path, cplat_trace_level level,
               size_t max_bytes, int generations, int flags)
{
    int mock_ret = 0;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_tracer_set_file_level(handle, path, level, max_bytes, generations, flags);
    }
    else
    {
        mock_ret = delegate_real_cplat_tracer_set_file_level(handle, path, level, max_bytes, generations, flags);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p, %s, %d, %zu, %d, %d", __func__, (void *)handle, path, (int)level, max_bytes, generations,
               flags);
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
