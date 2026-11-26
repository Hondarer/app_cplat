/* テスト対象ソース ファイルの注入用追加ソース
 * このソースはテスト対象ソースの末尾に結合されます
 * この static 関数へのアクセサーによって
 * テスト プログラムからテスト対象ソースの static 関数にアクセスできます
 */
#ifndef _IN_TEST_SRC
    #include "trace_etw_session.c"
#endif /* _IN_TEST_SRC */

#include "trace_etw_session.inject.h"

#if defined(PLATFORM_WINDOWS)

void test_trace_etw_session_extract_event_fields(PEVENT_RECORD event_record, const TRACE_EVENT_INFO *info,
                                                 const char **service_out, const char **message_out,
                                                 uint32_t *process_id_out)
{
    extract_event_fields(event_record, info, service_out, message_out, process_id_out);
}

/* 停止の要求とトレースを閉じる処理の両方が失敗した場合の待機の上限 [ms] を設定する。 */
void test_trace_etw_session_set_stop_abandon_timeout_ms(int timeout_ms)
{
    s_stop_abandon_timeout_ms = timeout_ms;
}

/* 待機の上限を既定値へ戻す。 */
void test_trace_etw_session_reset_stop_abandon_timeout_ms(void)
{
    s_stop_abandon_timeout_ms = STOP_ABANDON_TIMEOUT_MS;
}

/* 利用側のコールバックへの受け渡しを、イベントの復元を経ずに呼び出す。 */
void test_trace_etw_session_deliver_event(cplat_etw_session *session, const cplat_etw_event *event)
{
    deliver_event(session, event);
}

/* 停止が切り離したセッションの資源を、テストが処理スレッドの終了を確かめた後に解放する。 */
void test_trace_etw_session_dispose_abandoned(cplat_etw_session *session)
{
    cplat_free(session->session_name_w);
    cplat_free(session->properties);
    cplat_free(session);
}

#endif /* PLATFORM_WINDOWS */
