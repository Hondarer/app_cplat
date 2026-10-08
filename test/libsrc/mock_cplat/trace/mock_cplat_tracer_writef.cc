#include <stdarg.h>
#include <vector>
#include <stdio.h>
#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_tracer_writef_at(cplat_tracer *handle, cplat_trace_level level, const cplat_timespec *timestamp,
                                         const char *format, ...)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_tracer_writef_at)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_tracer_writef_at"));

    return real_fn(handle, level, timestamp, "%s", format);
}

MOCK_WEAK_IMPL(int, cplat_tracer_writef_at, cplat_tracer *handle, cplat_trace_level level,
               const cplat_timespec *timestamp, const char *format, ...)
{
    int mock_ret = 0;

    std::vector<char> buf;
    {
        va_list args;
        va_start(args, format);
        buf = mock_cplat_expand_format(format, args);
        va_end(args);
    }

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_tracer_writef_at(handle, level, timestamp, buf.data());
    }
    else
    {
        mock_ret = delegate_real_cplat_tracer_writef_at(handle, level, timestamp, buf.data());
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p, %d, 0x%p, %s", __func__, (void *)handle, (int)level, (const void *)timestamp, buf.data());
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
