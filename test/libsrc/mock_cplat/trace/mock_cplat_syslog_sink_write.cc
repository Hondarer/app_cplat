#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_LINUX)

int delegate_real_cplat_syslog_sink_write(cplat_syslog_sink *handle, int level, const cplat_timespec *timestamp,
                                          const char *message)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_syslog_sink_write)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_syslog_sink_write"));

    return real_fn(handle, level, timestamp, message);
}

MOCK_WEAK_IMPL(int, cplat_syslog_sink_write, cplat_syslog_sink *handle, int level, const cplat_timespec *timestamp,
               const char *message)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_syslog_sink_write(handle, level, timestamp, message);
    }
    else
    {
        mock_ret = delegate_real_cplat_syslog_sink_write(handle, level, timestamp, message);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        const char *timestamp_text = "null";
        const char *message_text = "(null)";
        if (timestamp != nullptr)
        {
            timestamp_text = "set";
        }
        if (message != nullptr)
        {
            message_text = message;
        }
        printf("  > %s %d ts=%s \"%s\"", __func__, level, timestamp_text, message_text);
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

#endif /* PLATFORM_LINUX */
