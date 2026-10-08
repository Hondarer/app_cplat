/* テスト対象ソース ファイルの注入用追加ソース
 * このソースはテスト対象ソースの末尾に結合されます
 * この static 変数へのアクセサーによって
 * テスト プログラムからテスト対象ソースの static 変数にアクセスできます
 */
#ifndef _IN_TEST_SRC
    #include "tracer.c"
#endif /* _IN_TEST_SRC */

#include "tracer.inject.h"

#include <stdlib.h>
#include <string.h>

void test_trace_registry_reset_shutdown_state(void)
{
    cplat_once_flag reset_once = {0};

    s_trace_registry.shutdown_started = 0;
    s_trace_shutdown_once = reset_once;
    s_registry_lock_once = reset_once;
    s_registry_lock = NULL;
    s_registry_lock_init_result = CPLAT_ERR_UNKNOWN;
    s_shutdown_registration_result = CPLAT_ERR_UNKNOWN;
}

void test_trace_registry_set_shutdown_started(const size_t shutdown_started)
{
    s_trace_registry.shutdown_started = shutdown_started;
}

void test_trace_registry_set_counts(const size_t count, const size_t capacity)
{
    s_trace_registry.count = count;
    s_trace_registry.capacity = capacity;
}

void test_trace_registry_reinit_lock(void)
{
    cplat_once_flag reset_once = {0};

    s_registry_lock_once = reset_once;
    s_registry_lock = NULL;
    s_registry_lock_init_result = CPLAT_ERR_UNKNOWN;
}

void test_trace_registry_null_lock(void)
{
    s_registry_lock = NULL;
}

int test_trace_registry_append_null(void)
{
    if (s_trace_registry.count >= s_trace_registry.capacity)
    {
        return CPLAT_ERR_OUT_OF_MEMORY;
    }

    s_trace_registry.items[s_trace_registry.count] = NULL;
    s_trace_registry.count++;
    return CPLAT_OK;
}

int test_tracer_handle_is_active(const cplat_tracer *handle)
{
    return handle_is_active(handle);
}

int test_tracer_begin_dispose(cplat_tracer *handle)
{
    return begin_dispose(handle);
}

#if defined(PLATFORM_LINUX)
int test_tracer_to_syslog_level(const cplat_trace_level level)
{
    return to_syslog_level(level);
}
#endif /* PLATFORM_LINUX */

size_t test_tracer_utf8_safe_truncate(const char *text, const size_t position)
{
    return utf8_safe_truncate(text, position);
}

int test_tracer_build_default_file_path(const cplat_tracer *handle, char *path_out, const size_t path_size)
{
    return build_default_file_path(handle, path_out, path_size);
}

int test_tracer_hex_write_impl(cplat_tracer *handle, const cplat_trace_level level, const cplat_timespec *timestamp,
                               const void *data, const size_t size, const char *label)
{
    return hex_write_impl(handle, level, timestamp, data, size, label);
}

void test_tracer_set_lifecycle_state(cplat_tracer *handle, const int lifecycle_state)
{
    handle->lifecycle_state = lifecycle_state;
}

void test_tracer_set_running(cplat_tracer *handle, const int running)
{
    handle->running = running;
}

void test_tracer_set_file_handle(cplat_tracer *handle, cplat_trace_file_sink *file_handle)
{
    handle->file_handle = file_handle;
}

void test_tracer_set_hook_fn(cplat_tracer_hook_entry *entry, cplat_tracer_hook_fn fn)
{
    entry->fn = fn;
}

void test_tracer_unregister(cplat_tracer *handle)
{
    (void)registry_unregister_handle(handle);
}

void test_tracer_install_null_fn_hook(cplat_tracer *handle)
{
    cplat_tracer_hook_entry *entry = (cplat_tracer_hook_entry *)calloc(1, sizeof(*entry));
    handle->hook_head = entry;
}

void test_tracer_clear_hook_head(cplat_tracer *handle)
{
    free(handle->hook_head);
    handle->hook_head = NULL;
}

void test_tracer_call_next_null(cplat_tracer *handle)
{
    cplat_tracer_call_next_hook(NULL, handle, CPLAT_TRACE_LEVEL_INFO, NULL, "x");
}

void test_tracer_call_next_null_fn(cplat_tracer *handle)
{
    cplat_tracer_hook_entry entry = {0};

    cplat_tracer_call_next_hook(&entry, handle, CPLAT_TRACE_LEVEL_INFO, NULL, "x");
}

void test_tracer_call_next_with_fn(cplat_tracer *handle, cplat_tracer_hook_fn fn)
{
    cplat_tracer_hook_entry entry = {0};

    entry.fn = fn;
    cplat_tracer_call_next_hook(&entry, handle, CPLAT_TRACE_LEVEL_INFO, NULL, "x");
}

void test_tracer_clear_file_path(cplat_tracer *handle)
{
    free(handle->file_path);
    handle->file_path = NULL;
}
