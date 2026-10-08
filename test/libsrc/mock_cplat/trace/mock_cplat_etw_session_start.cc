#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_WINDOWS)

int delegate_real_cplat_etw_session_start(const char *session_name, const char *provider_guid_str,
                                          cplat_etw_event_fn callback, void *context, cplat_etw_session **session_out)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_etw_session_start)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_etw_session_start"));

    return real_fn(session_name, provider_guid_str, callback, context, session_out);
}

MOCK_WEAK_IMPL(int, cplat_etw_session_start, const char *session_name, const char *provider_guid_str,
               cplat_etw_event_fn callback, void *context, cplat_etw_session **session_out)
{
    int mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret =
            _mock_cplat->cplat_etw_session_start(session_name, provider_guid_str, callback, context, session_out);
    }
    else
    {
        mock_ret =
            delegate_real_cplat_etw_session_start(session_name, provider_guid_str, callback, context, session_out);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s \"%s\"", __func__, session_name != nullptr ? session_name : "(null)");
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
