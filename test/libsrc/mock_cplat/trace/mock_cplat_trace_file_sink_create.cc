#include <testfw.h>
#include <mock_cplat.h>

cplat_trace_file_sink *delegate_real_cplat_trace_file_sink_create(const char *path, size_t max_bytes, int generations,
                                                                  int flags)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_trace_file_sink_create)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_trace_file_sink_create"));

    return real_fn(path, max_bytes, generations, flags);
}

MOCK_WEAK_IMPL(cplat_trace_file_sink *, cplat_trace_file_sink_create, const char *path, size_t max_bytes,
               int generations, int flags)
{
    cplat_trace_file_sink *mock_ret = nullptr;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_trace_file_sink_create(path, max_bytes, generations, flags);
    }
    else
    {
        mock_ret = delegate_real_cplat_trace_file_sink_create(path, max_bytes, generations, flags);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s \"%s\", %d", __func__, path != nullptr ? path : "(null)", flags);
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
