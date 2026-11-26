/* テスト対象ソース ファイルの注入用追加ヘッダー
 * このヘッダーをテスト プログラムが参照することで
 * テスト プログラムからテスト対象ソースの static 関数にアクセスできます
 */
#ifndef TRACE_ETW_SESSION_INJECT_H
#define TRACE_ETW_SESSION_INJECT_H

#include <cplat/base/platform.h>

#if defined(PLATFORM_WINDOWS)

    #include <cplat/base/windows_sdk.h>
    #include <cplat/trace/etw.h>
    #include <evntcons.h>
    #include <tdh.h>
    #include <stdint.h>

    #ifdef __cplusplus
extern "C"
{
    #endif /* __cplusplus */

    extern void test_trace_etw_session_extract_event_fields(PEVENT_RECORD event_record, const TRACE_EVENT_INFO *info,
                                                            const char **service_out, const char **message_out,
                                                            uint32_t *process_id_out);

    extern void test_trace_etw_session_set_stop_abandon_timeout_ms(int timeout_ms);

    extern void test_trace_etw_session_reset_stop_abandon_timeout_ms(void);

    extern void test_trace_etw_session_deliver_event(cplat_etw_session *session, const cplat_etw_event *event);

    extern void test_trace_etw_session_dispose_abandoned(cplat_etw_session *session);

    #ifdef __cplusplus
}
    #endif /* __cplusplus */

#endif /* PLATFORM_WINDOWS */

#endif /* TRACE_ETW_SESSION_INJECT_H */
