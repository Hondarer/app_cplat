#include <testfw.h>
#include <cplat/base/platform.h>

#if defined(PLATFORM_LINUX)

    #include <cplat/trace/backends/syslog/syslog_internal.h>

void (*g_test_syslog_shutdown_hook)(void) = nullptr;

void cplat_internal_syslog_sink_dispose_on_shutdown(cplat_syslog_sink *handle)
{
    (void)handle;
    if (g_test_syslog_shutdown_hook != nullptr)
    {
        g_test_syslog_shutdown_hook();
    }
}

#endif /* PLATFORM_LINUX */
