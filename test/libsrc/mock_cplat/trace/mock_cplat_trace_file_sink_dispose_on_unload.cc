#include <testfw.h>
#include <cplat/trace/backends/file/trace_file_internal.h>

void (*g_test_file_shutdown_hook)(void) = nullptr;

void cplat_internal_trace_file_sink_dispose_on_shutdown(cplat_trace_file_sink *handle)
{
    (void)handle;
    if (g_test_file_shutdown_hook != nullptr)
    {
        g_test_file_shutdown_hook();
    }
}
