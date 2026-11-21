/* テスト対象ソース ファイルの注入用追加ヘッダー
 * このヘッダーをテスト プログラムが参照することで
 * テスト プログラムからテスト対象ソースの static 変数にアクセスできます
 */
#ifndef TRACER_INJECT_H
#define TRACER_INJECT_H

#include <stddef.h>

#include <cplat/trace/tracer.h>
#include <cplat/trace/trace_file.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /* tracer.c のファイル内 static 変数 s_trace_registry の shutdown 状態を戻すアクセサー。
       shutdown_started は本来一度立つと戻らないため、shutdown を検証したテストの後に
       同一プロセスで実行される他のテストが tracer を生成できるようにする。 */
    extern void test_trace_registry_reset_shutdown_state(void);
    extern void test_trace_registry_set_shutdown_started(size_t shutdown_started);
    extern void test_trace_registry_set_counts(size_t count, size_t capacity);
    extern void test_trace_registry_reinit_lock(void);
    extern void test_trace_registry_null_lock(void);
    extern int test_trace_registry_append_null(void);

    extern int test_tracer_handle_is_active(const cplat_tracer *handle);
    extern int test_tracer_begin_dispose(cplat_tracer *handle);
#if defined(PLATFORM_LINUX)
    extern int test_tracer_to_syslog_level(cplat_trace_level level);
#elif defined(PLATFORM_WINDOWS)
    extern int test_tracer_to_eventlog_level(cplat_trace_level level);
#endif /* PLATFORM_ */
    extern size_t test_tracer_utf8_safe_truncate(const char *text, size_t position);
    extern int test_tracer_build_default_file_path(const cplat_tracer *handle, char *path_out, size_t path_size);
    extern int test_tracer_hex_write_impl(cplat_tracer *handle, cplat_trace_level level,
                                          const cplat_timespec *timestamp, const void *data, size_t size,
                                          const char *label);

    extern void test_tracer_set_lifecycle_state(cplat_tracer *handle, int lifecycle_state);
    extern void test_tracer_set_running(cplat_tracer *handle, int running);
    extern void test_tracer_set_file_handle(cplat_tracer *handle, cplat_trace_file_sink *file_handle);
    extern void test_tracer_set_hook_fn(cplat_tracer_hook_entry *entry, cplat_tracer_hook_fn fn);
    extern void test_tracer_unregister(cplat_tracer *handle);
    extern void test_tracer_install_null_fn_hook(cplat_tracer *handle);
    extern void test_tracer_clear_hook_head(cplat_tracer *handle);
    extern void test_tracer_call_next_null(cplat_tracer *handle);
    extern void test_tracer_call_next_null_fn(cplat_tracer *handle);
    extern void test_tracer_call_next_with_fn(cplat_tracer *handle, cplat_tracer_hook_fn fn);
    extern void test_tracer_clear_file_path(cplat_tracer *handle);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TRACER_INJECT_H */
