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

#endif /* PLATFORM_WINDOWS */
