#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_WINDOWS)

int delegate_real_cplat_eventlog_sink_write(cplat_eventlog_sink *handle, int level, int64_t file_identifier,
                                            const char *instance_name, int64_t instance_identifier, const char *message)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_eventlog_sink_write)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_eventlog_sink_write"));

    return real_fn(handle, level, file_identifier, instance_name, instance_identifier, message);
}

MOCK_WEAK_IMPL(int, cplat_eventlog_sink_write, cplat_eventlog_sink *handle, int level, int64_t file_identifier,
               const char *instance_name, int64_t instance_identifier, const char *message)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_eventlog_sink_write(handle, level, file_identifier, instance_name,
                                                          instance_identifier, message);
    }
    else
    {
        mock_ret = delegate_real_cplat_eventlog_sink_write(handle, level, file_identifier, instance_name,
                                                           instance_identifier, message);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        const char *instance_name_text = "(null)";
        const char *message_text = "(null)";
        if (instance_name != nullptr)
        {
            instance_name_text = instance_name;
        }
        if (message != nullptr)
        {
            message_text = message;
        }
        printf("  > %s %d %lld \"%s\" %lld \"%s\"", __func__, level, (long long)file_identifier, instance_name_text,
               (long long)instance_identifier, message_text);
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

#endif /* PLATFORM_WINDOWS */
