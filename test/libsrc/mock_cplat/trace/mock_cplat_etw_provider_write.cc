#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_WINDOWS)

int delegate_real_cplat_etw_provider_write(cplat_etw_provider *handle, int level, const char *service,
                                           const char *message)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_etw_provider_write)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_etw_provider_write"));

    return real_fn(handle, level, service, message);
}

MOCK_WEAK_IMPL(int, cplat_etw_provider_write, cplat_etw_provider *handle, int level, const char *service,
               const char *message)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_etw_provider_write(handle, level, service, message);
    }
    else
    {
        mock_ret = delegate_real_cplat_etw_provider_write(handle, level, service, message);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        const char *message_text = "(null)";
        if (message != nullptr)
        {
            message_text = message;
        }
        printf("  > %s %d \"%s\"", __func__, level, message_text);
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
