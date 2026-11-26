#include <cplat/base/platform.h>

#if defined(PLATFORM_WINDOWS)

    #include <cplat/base/windows_sdk.h>
    #include <cplat/trace/etw.h>
    #include <testfw.h>
    #include <mock_cplat.h>
    #include <mock_windows.h>

    #include <chrono>
    #include <condition_variable>
    #include <cstdint>
    #include <cstring>
    #include <mutex>
    #include <vector>

    #include "trace_etw_session.inject.h"

namespace
{
struct property_spec
{
    const wchar_t *name;
    USHORT flags;
    USHORT in_type;
    USHORT length;
    USHORT count;
    USHORT count_property_index;
};

std::vector<unsigned char> build_trace_event_info(const std::vector<property_spec> &specs)
{
    const std::size_t prop_count = specs.size();
    const std::size_t header_size =
        sizeof(TRACE_EVENT_INFO) + (prop_count > 1U ? (prop_count - 1U) * sizeof(EVENT_PROPERTY_INFO) : 0U);
    std::size_t total_size = header_size;

    for (const property_spec &spec : specs)
    {
        if (spec.name != nullptr)
        {
            total_size += (std::wcslen(spec.name) + 1U) * sizeof(wchar_t);
        }
    }

    std::vector<unsigned char> buffer(total_size, 0U);
    TRACE_EVENT_INFO *info = reinterpret_cast<TRACE_EVENT_INFO *>(buffer.data());
    std::size_t name_cursor = header_size;

    info->TopLevelPropertyCount = static_cast<ULONG>(prop_count);
    for (std::size_t index = 0U; index < prop_count; index++)
    {
        EVENT_PROPERTY_INFO *prop = &info->EventPropertyInfoArray[index];
        const property_spec &spec = specs[index];

        prop->Flags = static_cast<PROPERTY_FLAGS>(spec.flags);
        if ((spec.flags & PropertyStruct) == 0U)
        {
            prop->nonStructType.InType = spec.in_type;
        }
        prop->length = spec.length;
        prop->count = spec.count;
        if ((spec.flags & PropertyParamCount) != 0U)
        {
            prop->countPropertyIndex = spec.count_property_index;
        }
        if (spec.name != nullptr)
        {
            const std::size_t name_bytes = (std::wcslen(spec.name) + 1U) * sizeof(wchar_t);

            std::memcpy(buffer.data() + name_cursor, spec.name, name_bytes);
            prop->NameOffset = static_cast<ULONG>(name_cursor);
            name_cursor += name_bytes;
        }
    }

    return buffer;
}
} // namespace

class trace_etw_sessionTest : public Test
{
protected:
    void TearDown() override
    {
        // 停止の待機の上限を短くしたテストが失敗しても、後続のテストへ持ち越さない。
        test_trace_etw_session_reset_stop_abandon_timeout_ms();
    }
};

// 文字列 2 個と整数 1 個のイベントで、Service、Message、ProcessId が得られることの確認
TEST_F(trace_etw_sessionTest, extract_reads_service_message_and_process_id)
{
    // Arrange
    const std::vector<property_spec> specs = {
        {L"Service", 0U, TDH_INTYPE_ANSISTRING, 0U, 1U, 0U},
        {L"Message", 0U, TDH_INTYPE_ANSISTRING, 0U, 1U, 0U},
        {L"ProcessId", 0U, TDH_INTYPE_UINT32, 0U, 1U, 0U},
    };
    const std::vector<unsigned char> info_buf = build_trace_event_info(specs);
    const TRACE_EVENT_INFO *info = reinterpret_cast<const TRACE_EVENT_INFO *>(info_buf.data());
    const unsigned char user_data[] = {
        's', 'v', 'c', '\0',
        'h', 'e', 'l', 'l', 'o', '\0',
        0xD2U, 0x04U, 0x00U, 0x00U,
    };
    EVENT_RECORD event_record = {};
    const char *service = nullptr;
    const char *message = nullptr;
    uint32_t process_id = 0U;

    event_record.UserData = const_cast<unsigned char *>(user_data);
    event_record.UserDataLength = static_cast<USHORT>(sizeof(user_data));

    // Pre-Assert

    // Act
    test_trace_etw_session_extract_event_fields(
        &event_record, info, &service, &message,
        &process_id); // [手順] - 文字列 2 個と整数 1 個のイベントを指定して test_trace_etw_session_extract_event_fields() を呼び出す。

    // Assert
    ASSERT_NE(nullptr, service); // [確認_正常系] - service が nullptr 以外であること。
    EXPECT_STREQ("svc", service); // [確認_正常系] - service が "svc" と一致すること。
    ASSERT_NE(nullptr, message); // [確認_正常系] - message が nullptr 以外であること。
    EXPECT_STREQ("hello", message); // [確認_正常系] - message が "hello" と一致すること。
    EXPECT_EQ(1234U, process_id); // [確認_正常系] - process_id が 1234U と一致すること。
}

// 構造体のプロパティに出会ったときに読み取りを打ち切り、後続の Message を読まないことの確認
TEST_F(trace_etw_sessionTest, extract_stops_at_struct_property)
{
    // Arrange
    const std::vector<property_spec> specs = {
        {L"Service", 0U, TDH_INTYPE_ANSISTRING, 0U, 1U, 0U},
        {L"Nested", PropertyStruct, 0U, 0U, 1U, 0U},
        {L"Message", 0U, TDH_INTYPE_ANSISTRING, 0U, 1U, 0U},
    };
    const std::vector<unsigned char> info_buf = build_trace_event_info(specs);
    const TRACE_EVENT_INFO *info = reinterpret_cast<const TRACE_EVENT_INFO *>(info_buf.data());
    const unsigned char user_data[] = {'s', 'v', 'c', '\0', 'x', '\0', 'm', 's', 'g', '\0'};
    EVENT_RECORD event_record = {};
    const char *service = nullptr;
    const char *message = nullptr;
    uint32_t process_id = 0U;

    event_record.UserData = const_cast<unsigned char *>(user_data);
    event_record.UserDataLength = static_cast<USHORT>(sizeof(user_data));

    // Pre-Assert

    // Act
    test_trace_etw_session_extract_event_fields(
        &event_record, info, &service, &message,
        &process_id); // [手順] - 構造体のプロパティを含むイベントを指定して test_trace_etw_session_extract_event_fields() を呼び出す。

    // Assert
    ASSERT_NE(nullptr, service); // [確認_正常系] - service が nullptr 以外であること。
    EXPECT_STREQ("svc", service); // [確認_正常系] - service が "svc" と一致すること。
    EXPECT_EQ(nullptr, message); // [確認_正常系] - 構造体の後ろの message が nullptr であること。
}

// 固定の要素数の配列プロパティに出会ったときに読み取りを打ち切り、後続を読まないことの確認
TEST_F(trace_etw_sessionTest, extract_stops_at_fixed_count_array_property)
{
    // Arrange
    const std::vector<property_spec> specs = {
        {L"Service", 0U, TDH_INTYPE_ANSISTRING, 0U, 1U, 0U},
        {L"Items", PropertyParamFixedCount, TDH_INTYPE_ANSISTRING, 0U, 2U, 0U},
        {L"Message", 0U, TDH_INTYPE_ANSISTRING, 0U, 1U, 0U},
    };
    const std::vector<unsigned char> info_buf = build_trace_event_info(specs);
    const TRACE_EVENT_INFO *info = reinterpret_cast<const TRACE_EVENT_INFO *>(info_buf.data());
    const unsigned char user_data[] = {'s', 'v', 'c', '\0', 'a', '\0', 'b', '\0', 'm', 's', 'g', '\0'};
    EVENT_RECORD event_record = {};
    const char *service = nullptr;
    const char *message = nullptr;
    uint32_t process_id = 0U;

    event_record.UserData = const_cast<unsigned char *>(user_data);
    event_record.UserDataLength = static_cast<USHORT>(sizeof(user_data));

    // Pre-Assert

    // Act
    test_trace_etw_session_extract_event_fields(
        &event_record, info, &service, &message,
        &process_id); // [手順] - 固定の要素数の配列プロパティを含むイベントを指定して test_trace_etw_session_extract_event_fields() を呼び出す。

    // Assert
    ASSERT_NE(nullptr, service); // [確認_正常系] - service が nullptr 以外であること。
    EXPECT_STREQ("svc", service); // [確認_正常系] - service が "svc" と一致すること。
    EXPECT_EQ(nullptr, message); // [確認_正常系] - 固定長配列の後ろの message が nullptr であること。
}

// 要素数を別のプロパティに持つ配列プロパティに出会ったときに読み取りを打ち切り、後続を読まないことの確認
TEST_F(trace_etw_sessionTest, extract_stops_at_variable_count_array_property)
{
    // Arrange
    const std::vector<property_spec> specs = {
        {L"Service", 0U, TDH_INTYPE_ANSISTRING, 0U, 1U, 0U},
        {L"Count", 0U, TDH_INTYPE_UINT32, 0U, 1U, 0U},
        {L"Items", PropertyParamCount, TDH_INTYPE_ANSISTRING, 0U, 1U, 1U},
        {L"Message", 0U, TDH_INTYPE_ANSISTRING, 0U, 1U, 0U},
    };
    const std::vector<unsigned char> info_buf = build_trace_event_info(specs);
    const TRACE_EVENT_INFO *info = reinterpret_cast<const TRACE_EVENT_INFO *>(info_buf.data());
    const unsigned char user_data[] = {
        's', 'v', 'c', '\0',
        0x02U, 0x00U, 0x00U, 0x00U,
        'a', '\0', 'b', '\0',
        'm', 's', 'g', '\0',
    };
    EVENT_RECORD event_record = {};
    const char *service = nullptr;
    const char *message = nullptr;
    uint32_t process_id = 0U;

    event_record.UserData = const_cast<unsigned char *>(user_data);
    event_record.UserDataLength = static_cast<USHORT>(sizeof(user_data));

    // Pre-Assert

    // Act
    test_trace_etw_session_extract_event_fields(
        &event_record, info, &service, &message,
        &process_id); // [手順] - 可変長配列プロパティを含むイベントを指定して test_trace_etw_session_extract_event_fields() を呼び出す。

    // Assert
    ASSERT_NE(nullptr, service); // [確認_正常系] - service が nullptr 以外であること。
    EXPECT_STREQ("svc", service); // [確認_正常系] - service が "svc" と一致すること。
    EXPECT_EQ(nullptr, message); // [確認_正常系] - 可変長配列の後ろの message が nullptr であること。
}

// 名前なしプロパティに出会ったときに読み取りを打ち切り、後続の Message を読まないことの確認
TEST_F(trace_etw_sessionTest, extract_stops_at_unnamed_property)
{
    // Arrange
    const std::vector<property_spec> specs = {
        {L"Service", 0U, TDH_INTYPE_ANSISTRING, 0U, 1U, 0U},
        {nullptr, 0U, TDH_INTYPE_ANSISTRING, 0U, 1U, 0U},
        {L"Message", 0U, TDH_INTYPE_ANSISTRING, 0U, 1U, 0U},
    };
    const std::vector<unsigned char> info_buf = build_trace_event_info(specs);
    const TRACE_EVENT_INFO *info = reinterpret_cast<const TRACE_EVENT_INFO *>(info_buf.data());
    const unsigned char user_data[] = {'s', 'v', 'c', '\0', 's', 'k', 'i', 'p', '\0', 'm', 's', 'g', '\0'};
    EVENT_RECORD event_record = {};
    const char *service = nullptr;
    const char *message = nullptr;
    uint32_t process_id = 0U;

    event_record.UserData = const_cast<unsigned char *>(user_data);
    event_record.UserDataLength = static_cast<USHORT>(sizeof(user_data));

    // Pre-Assert

    // Act
    test_trace_etw_session_extract_event_fields(
        &event_record, info, &service, &message,
        &process_id); // [手順] - 名前なしプロパティを含むイベントを指定して test_trace_etw_session_extract_event_fields() を呼び出す。

    // Assert
    ASSERT_NE(nullptr, service); // [確認_正常系] - service が nullptr 以外であること。
    EXPECT_STREQ("svc", service); // [確認_正常系] - service が "svc" と一致すること。
    EXPECT_EQ(nullptr, message); // [確認_正常系] - 名前なしプロパティの後ろの message が nullptr であること。
}

// 長さ指定プロパティを持つ文字列に出会ったときに読み取りを打ち切り、Service を読まないことの確認
TEST_F(trace_etw_sessionTest, extract_stops_at_string_with_length_property)
{
    // Arrange
    const std::vector<property_spec> specs = {
        {L"Len", 0U, TDH_INTYPE_UINT32, 0U, 1U, 0U},
        {L"Service", PropertyParamLength, TDH_INTYPE_ANSISTRING, 0U, 1U, 0U},
    };
    const std::vector<unsigned char> info_buf = build_trace_event_info(specs);
    const TRACE_EVENT_INFO *info = reinterpret_cast<const TRACE_EVENT_INFO *>(info_buf.data());
    const unsigned char user_data[] = {
        0x03U, 0x00U, 0x00U, 0x00U,
        'a', 'b', 'c',
        'm', 's', 'g', '\0',
    };
    EVENT_RECORD event_record = {};
    const char *service = nullptr;
    const char *message = nullptr;
    uint32_t process_id = 0U;

    event_record.UserData = const_cast<unsigned char *>(user_data);
    event_record.UserDataLength = static_cast<USHORT>(sizeof(user_data));

    // Pre-Assert

    // Act
    test_trace_etw_session_extract_event_fields(
        &event_record, info, &service, &message,
        &process_id); // [手順] - PropertyParamLength が立つ Service を含むイベントを指定して test_trace_etw_session_extract_event_fields() を呼び出す。

    // Assert
    EXPECT_EQ(nullptr, service); // [確認_正常系] - service が nullptr であること。
}

// 固定長の文字列プロパティに出会ったときに読み取りを打ち切り、Service を読まないことの確認
TEST_F(trace_etw_sessionTest, extract_stops_at_fixed_length_string)
{
    // Arrange
    const std::vector<property_spec> specs = {
        {L"Service", PropertyParamFixedLength, TDH_INTYPE_ANSISTRING, 3U, 1U, 0U},
    };
    const std::vector<unsigned char> info_buf = build_trace_event_info(specs);
    const TRACE_EVENT_INFO *info = reinterpret_cast<const TRACE_EVENT_INFO *>(info_buf.data());
    const unsigned char user_data[] = {'a', 'b', 'c'};
    EVENT_RECORD event_record = {};
    const char *service = nullptr;
    const char *message = nullptr;
    uint32_t process_id = 0U;

    event_record.UserData = const_cast<unsigned char *>(user_data);
    event_record.UserDataLength = static_cast<USHORT>(sizeof(user_data));

    // Pre-Assert

    // Act
    test_trace_etw_session_extract_event_fields(
        &event_record, info, &service, &message,
        &process_id); // [手順] - PropertyParamFixedLength が立ち length が 3 の Service を指定して test_trace_etw_session_extract_event_fields() を呼び出す。

    // Assert
    EXPECT_EQ(nullptr, service); // [確認_正常系] - service が nullptr であること。
}

namespace
{

struct etw_stop_sync
{
    std::mutex mtx;
    std::condition_variable cv;
    bool close_called = false;
    bool stop_succeeded = false;
    bool process_timed_out = false;
};

} // namespace

// ControlTraceW が失敗した場合、join より前に CloseTrace を 1 回だけ呼び、ERROR_CTX_CLOSE_PENDING でも join が完了することの確認
TEST_F(trace_etw_sessionTest, stop_calls_close_trace_before_join_when_control_trace_fails)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    etw_stop_sync sync;
    cplat_etw_session *session = nullptr;
    const PROCESSTRACE_HANDLE expected_trace_handle = static_cast<PROCESSTRACE_HANDLE>(22U);

    // Pre-Assert
    EXPECT_CALL(mock_windows, StartTraceW(_, _, _, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, CONTROLTRACE_ID *trace_id, LPCWSTR, PEVENT_TRACE_PROPERTIES)
            {
                *trace_id = 11U;
                return static_cast<ULONG>(ERROR_SUCCESS);
            }); // [Pre-Assert確認_正常系] - StartTraceW() が呼ばれセッション ハンドルを返すこと。
    EXPECT_CALL(mock_windows, EnableTraceEx2(_, _, _, 11U, _, _, _, _, _, _, _))
        .WillOnce(Return(static_cast<ULONG>(ERROR_SUCCESS))); // [Pre-Assert確認_正常系] - EnableTraceEx2() が成功すること。
    EXPECT_CALL(mock_windows, OpenTraceW(_, _, _, _))
        .WillOnce(Return(expected_trace_handle)); // [Pre-Assert確認_正常系] - OpenTraceW() が有効なトレース ハンドルを返すこと。
    EXPECT_CALL(mock_windows, ProcessTrace(_, _, _, _, 1UL, nullptr, nullptr))
        .WillOnce(
            [&sync](const char *, const int, const char *, PROCESSTRACE_HANDLE *, ULONG, LPFILETIME, LPFILETIME)
            {
                std::unique_lock<std::mutex> lock(sync.mtx);
                const bool signaled =
                    sync.cv.wait_for(lock, std::chrono::seconds(5), [&sync]() { return sync.close_called; });
                sync.process_timed_out = !signaled;
                return static_cast<ULONG>(ERROR_SUCCESS);
            }); // [Pre-Assert確認_正常系] - ProcessTrace() が CloseTrace() の呼び出しまで待機して終了すること。

    {
        InSequence seq;
        EXPECT_CALL(mock_windows, ControlTraceW(_, _, _, 11U, nullptr, _, EVENT_TRACE_CONTROL_STOP))
            .WillOnce(Return(static_cast<ULONG>(ERROR_ACCESS_DENIED))); // [Pre-Assert確認_異常系] - ControlTraceW() が ERROR_ACCESS_DENIED を返すこと。
        EXPECT_CALL(mock_windows, CloseTrace(_, _, _, expected_trace_handle))
            .WillOnce(
                [&sync](const char *, const int, const char *, PROCESSTRACE_HANDLE)
                {
                    std::lock_guard<std::mutex> lock(sync.mtx);
                    sync.close_called = true;
                    sync.cv.notify_all();
                    return static_cast<ULONG>(ERROR_CTX_CLOSE_PENDING);
                }); // [Pre-Assert確認_異常系] - join より前に CloseTrace() が 1 回だけ呼ばれ ERROR_CTX_CLOSE_PENDING を返すこと。
        EXPECT_CALL(mock_cplat, cplat_thread_join(_))
            .WillOnce(DoDefault()); // [Pre-Assert確認_正常系] - CloseTrace() の後に cplat_thread_join() が呼ばれること。
    }
    EXPECT_CALL(mock_cplat, cplat_thread_timed_join(_, _))
        .Times(0); // [Pre-Assert確認_正常系] - CloseTrace() が成功した場合は期限付きの待機を使わないこと。

    // Act
    int start_ret = cplat_etw_session_start(
        "UnitTestEtwStopFail", "0dfe6031-5678-4688-aee8-611340997caa",
        [](const cplat_etw_event *, void *) {}, nullptr, &session); // [手順] - ETW セッションを開始する。
    ASSERT_EQ(CPLAT_OK, start_ret); // [状態確認] - cplat_etw_session_start() が CPLAT_OK を返すこと。
    ASSERT_NE(nullptr, session);    // [状態確認] - session が nullptr 以外であること。

    cplat_etw_session_stop(session); // [手順] - ControlTraceW() が失敗する状態で cplat_etw_session_stop() を呼び出す。

    // Assert
    EXPECT_FALSE(sync.process_timed_out); // [確認_正常系] - ProcessTrace() がタイムアウトせず CloseTrace() の通知で終了したこと。
}

// ControlTraceW が成功した場合、join の後に CloseTrace を 1 回だけ呼ぶことの確認
TEST_F(trace_etw_sessionTest, stop_calls_close_trace_after_join_when_control_trace_succeeds)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    etw_stop_sync sync;
    cplat_etw_session *session = nullptr;
    const PROCESSTRACE_HANDLE expected_trace_handle = static_cast<PROCESSTRACE_HANDLE>(33U);

    // Pre-Assert
    EXPECT_CALL(mock_windows, StartTraceW(_, _, _, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, CONTROLTRACE_ID *trace_id, LPCWSTR, PEVENT_TRACE_PROPERTIES)
            {
                *trace_id = 12U;
                return static_cast<ULONG>(ERROR_SUCCESS);
            }); // [Pre-Assert確認_正常系] - StartTraceW() が呼ばれセッション ハンドルを返すこと。
    EXPECT_CALL(mock_windows, EnableTraceEx2(_, _, _, 12U, _, _, _, _, _, _, _))
        .WillOnce(Return(static_cast<ULONG>(ERROR_SUCCESS))); // [Pre-Assert確認_正常系] - EnableTraceEx2() が成功すること。
    EXPECT_CALL(mock_windows, OpenTraceW(_, _, _, _))
        .WillOnce(Return(expected_trace_handle)); // [Pre-Assert確認_正常系] - OpenTraceW() が有効なトレース ハンドルを返すこと。
    EXPECT_CALL(mock_windows, ProcessTrace(_, _, _, _, 1UL, nullptr, nullptr))
        .WillOnce(
            [&sync](const char *, const int, const char *, PROCESSTRACE_HANDLE *, ULONG, LPFILETIME, LPFILETIME)
            {
                std::unique_lock<std::mutex> lock(sync.mtx);
                const bool signaled =
                    sync.cv.wait_for(lock, std::chrono::seconds(5), [&sync]() { return sync.stop_succeeded; });
                sync.process_timed_out = !signaled;
                return static_cast<ULONG>(ERROR_SUCCESS);
            }); // [Pre-Assert確認_正常系] - ProcessTrace() が ControlTraceW() の成功まで待機して終了すること。

    {
        InSequence seq;
        EXPECT_CALL(mock_windows, ControlTraceW(_, _, _, 12U, nullptr, _, EVENT_TRACE_CONTROL_STOP))
            .WillOnce(
                [&sync](const char *, const int, const char *, CONTROLTRACE_ID, LPCWSTR, PEVENT_TRACE_PROPERTIES,
                        ULONG)
                {
                    std::lock_guard<std::mutex> lock(sync.mtx);
                    sync.stop_succeeded = true;
                    sync.cv.notify_all();
                    return static_cast<ULONG>(ERROR_SUCCESS);
                }); // [Pre-Assert確認_正常系] - ControlTraceW() が成功すること。
        EXPECT_CALL(mock_cplat, cplat_thread_join(_))
            .WillOnce(DoDefault()); // [Pre-Assert確認_正常系] - ControlTraceW() の後に cplat_thread_join() が呼ばれること。
        EXPECT_CALL(mock_windows, CloseTrace(_, _, _, expected_trace_handle))
            .WillOnce(Return(static_cast<ULONG>(ERROR_SUCCESS))); // [Pre-Assert確認_正常系] - join の後に CloseTrace() が 1 回だけ呼ばれること。
    }
    EXPECT_CALL(mock_cplat, cplat_thread_timed_join(_, _))
        .Times(0); // [Pre-Assert確認_正常系] - ControlTraceW() が成功した場合は期限付きの待機を使わないこと。

    // Act
    int start_ret = cplat_etw_session_start(
        "UnitTestEtwStopOk", "0dfe6031-5678-4688-aee8-611340997caa",
        [](const cplat_etw_event *, void *) {}, nullptr, &session); // [手順] - ETW セッションを開始する。
    ASSERT_EQ(CPLAT_OK, start_ret); // [状態確認] - cplat_etw_session_start() が CPLAT_OK を返すこと。
    ASSERT_NE(nullptr, session);    // [状態確認] - session が nullptr 以外であること。

    cplat_etw_session_stop(session); // [手順] - ControlTraceW() が成功する状態で cplat_etw_session_stop() を呼び出す。

    // Assert
    EXPECT_FALSE(sync.process_timed_out); // [確認_正常系] - ProcessTrace() がタイムアウトせず ControlTraceW() の成功で終了したこと。
}

// CloseTrace が ERROR_SUCCESS と ERROR_CTX_CLOSE_PENDING のどちらを返しても同じ停止動作になることの確認
TEST_F(trace_etw_sessionTest, stop_handles_close_trace_success_and_pending_equivalently)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    etw_stop_sync sync_fail_path;
    etw_stop_sync sync_ok_path;
    cplat_etw_session *session_fail_path = nullptr;
    cplat_etw_session *session_ok_path = nullptr;
    const PROCESSTRACE_HANDLE handle_one = static_cast<PROCESSTRACE_HANDLE>(44U);
    const PROCESSTRACE_HANDLE handle_two = static_cast<PROCESSTRACE_HANDLE>(55U);

    // Pre-Assert
    EXPECT_CALL(mock_windows, StartTraceW(_, _, _, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, CONTROLTRACE_ID *trace_id, LPCWSTR, PEVENT_TRACE_PROPERTIES)
            {
                *trace_id = 13U;
                return static_cast<ULONG>(ERROR_SUCCESS);
            })
        .WillOnce(
            [](const char *, const int, const char *, CONTROLTRACE_ID *trace_id, LPCWSTR, PEVENT_TRACE_PROPERTIES)
            {
                *trace_id = 14U;
                return static_cast<ULONG>(ERROR_SUCCESS);
            }); // [Pre-Assert確認_正常系] - 2 回の StartTraceW() がそれぞれセッション ハンドルを返すこと。
    EXPECT_CALL(mock_windows, EnableTraceEx2(_, _, _, _, _, _, _, _, _, _, _))
        .Times(2)
        .WillRepeatedly(Return(static_cast<ULONG>(ERROR_SUCCESS))); // [Pre-Assert確認_正常系] - EnableTraceEx2() が 2 回成功すること。
    EXPECT_CALL(mock_windows, OpenTraceW(_, _, _, _))
        .WillOnce(Return(handle_one))
        .WillOnce(Return(handle_two)); // [Pre-Assert確認_正常系] - 2 回の OpenTraceW() がそれぞれのハンドルを返すこと。
    // 2 つのセッションの作業スレッドは並行して動き、ProcessTrace() を呼ぶ順序は決まらない。
    // 呼び出しの順序で動作を割り当てると、1 つ目のセッションの停止条件を 2 つ目のスレッドが待ち、
    // 1 つ目の join が待ち続けることがあるため、渡されたハンドルで、どちらのセッションかを判定する。
    EXPECT_CALL(mock_windows, ProcessTrace(_, _, _, _, 1UL, nullptr, nullptr))
        .Times(2)
        .WillRepeatedly(
            [&sync_fail_path, &sync_ok_path, &handle_one](const char *, const int, const char *,
                                                          PROCESSTRACE_HANDLE *handles, ULONG, LPFILETIME,
                                                          LPFILETIME)
            {
                const bool is_fail_path = (handles != nullptr) && (handles[0] == handle_one);
                etw_stop_sync &sync = is_fail_path ? sync_fail_path : sync_ok_path;
                std::unique_lock<std::mutex> lock(sync.mtx);
                const bool signaled = sync.cv.wait_for(lock, std::chrono::seconds(5),
                                                       [&sync, is_fail_path]()
                                                       { return is_fail_path ? sync.close_called : sync.stop_succeeded; });
                sync.process_timed_out = !signaled;
                return static_cast<ULONG>(ERROR_SUCCESS);
            }); // [Pre-Assert確認_正常系] - 2 つの ProcessTrace() が、渡されたハンドルのセッションの停止条件まで待機して終了すること。
    {
        InSequence seq_fail;
        EXPECT_CALL(mock_windows, ControlTraceW(_, _, _, 13U, nullptr, _, EVENT_TRACE_CONTROL_STOP))
            .WillOnce(Return(static_cast<ULONG>(ERROR_ACCESS_DENIED))); // [Pre-Assert確認_異常系] - 1 つ目の停止要求が失敗すること。
        EXPECT_CALL(mock_windows, CloseTrace(_, _, _, handle_one))
            .WillOnce(
                [&sync_fail_path](const char *, const int, const char *, PROCESSTRACE_HANDLE)
                {
                    std::lock_guard<std::mutex> lock(sync_fail_path.mtx);
                    sync_fail_path.close_called = true;
                    sync_fail_path.cv.notify_all();
                    return static_cast<ULONG>(ERROR_SUCCESS);
                }); // [Pre-Assert確認_正常系] - 1 つ目の CloseTrace() が join より前に ERROR_SUCCESS を返すこと。
        EXPECT_CALL(mock_cplat, cplat_thread_join(_))
            .WillOnce(DoDefault()); // [Pre-Assert確認_正常系] - 1 つ目の join が実行されること。
        EXPECT_CALL(mock_windows, ControlTraceW(_, _, _, 14U, nullptr, _, EVENT_TRACE_CONTROL_STOP))
            .WillOnce(
                [&sync_ok_path](const char *, const int, const char *, CONTROLTRACE_ID, LPCWSTR,
                                PEVENT_TRACE_PROPERTIES, ULONG)
                {
                    std::lock_guard<std::mutex> lock(sync_ok_path.mtx);
                    sync_ok_path.stop_succeeded = true;
                    sync_ok_path.cv.notify_all();
                    return static_cast<ULONG>(ERROR_SUCCESS);
                }); // [Pre-Assert確認_正常系] - 2 つ目の停止要求が成功すること。
        EXPECT_CALL(mock_cplat, cplat_thread_join(_))
            .WillOnce(DoDefault()); // [Pre-Assert確認_正常系] - 2 つ目の join が実行されること。
        EXPECT_CALL(mock_windows, CloseTrace(_, _, _, handle_two))
            .WillOnce(Return(static_cast<ULONG>(ERROR_CTX_CLOSE_PENDING))); // [Pre-Assert確認_正常系] - 2 つ目の CloseTrace() が join 後に ERROR_CTX_CLOSE_PENDING を返すこと。
    }

    // Act
    ASSERT_EQ(CPLAT_OK, cplat_etw_session_start(
                            "UnitTestEtwCloseSuccess", "0dfe6031-5678-4688-aee8-611340997caa",
                            [](const cplat_etw_event *, void *) {}, nullptr, &session_fail_path));
    ASSERT_EQ(CPLAT_OK, cplat_etw_session_start(
                            "UnitTestEtwClosePending", "0dfe6031-5678-4688-aee8-611340997caa",
                            [](const cplat_etw_event *, void *) {}, nullptr, &session_ok_path));

    cplat_etw_session_stop(session_fail_path); // [手順] - 停止失敗かつ CloseTrace() が ERROR_SUCCESS を返す経路を実行する。
    cplat_etw_session_stop(session_ok_path); // [手順] - 停止成功かつ CloseTrace() が ERROR_CTX_CLOSE_PENDING を返す経路を実行する。

    // Assert
    EXPECT_FALSE(sync_fail_path.process_timed_out); // [確認_正常系] - ERROR_SUCCESS 側のワーカー スレッドが正常終了したこと。
    EXPECT_FALSE(sync_ok_path.process_timed_out); // [確認_正常系] - ERROR_CTX_CLOSE_PENDING 側のワーカー スレッドが正常終了したこと。
}

// NULL セッションを渡した cplat_etw_session_stop が何もしないことの確認
TEST_F(trace_etw_sessionTest, stop_with_null_session_does_nothing)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;

    // Pre-Assert
    EXPECT_CALL(mock_windows, ControlTraceW(_, _, _, _, _, _, _))
        .Times(0); // [Pre-Assert確認_正常系] - NULL 指定時は ControlTraceW() を呼ばないこと。
    EXPECT_CALL(mock_windows, CloseTrace(_, _, _, _))
        .Times(0); // [Pre-Assert確認_正常系] - NULL 指定時は CloseTrace() を呼ばないこと。

    // Act
    cplat_etw_session_stop(nullptr); // [手順] - nullptr を指定して cplat_etw_session_stop() を呼び出す。

    // Assert
    SUCCEED(); // [確認_正常系] - クラッシュせず制御が戻ること。
}

namespace
{

// 停止の要求とトレースを閉じる処理の両方が失敗する経路で、処理スレッドとテストの順序を固定する障壁です。
struct etw_abandon_sync
{
    std::mutex mtx;
    std::condition_variable cv;
    cplat_etw_session *captured_session = nullptr;
    bool first_delivered = false;
    bool callback_entered = false;
    bool sleep_called = false;
    bool callback_finished = false;
    bool callback_wait_timed_out = false;
    bool release = false;
    bool release_timed_out = false;
    bool process_returned = false;
    int callback_count = 0;
};

// 利用側のコールバックとして、呼ばれた回数を数えます。
void count_callback(const cplat_etw_event *, void *context)
{
    etw_abandon_sync *sync = static_cast<etw_abandon_sync *>(context);
    std::lock_guard<std::mutex> lock(sync->mtx);
    sync->callback_count++;
}

// 利用側のコールバックとして、停止が実行中のコールバックの終了を待ち始めるまで戻りません。
void blocking_callback(const cplat_etw_event *, void *context)
{
    etw_abandon_sync *sync = static_cast<etw_abandon_sync *>(context);
    std::unique_lock<std::mutex> lock(sync->mtx);

    sync->callback_entered = true;
    sync->cv.notify_all();
    sync->callback_wait_timed_out =
        !sync->cv.wait_for(lock, std::chrono::seconds(5), [sync]() { return sync->sleep_called; });
    sync->callback_finished = true;
}

// テストが解放するまで戻らない ProcessTrace() の終わりの部分です。
void wait_release_and_return(etw_abandon_sync &sync)
{
    std::unique_lock<std::mutex> lock(sync.mtx);

    sync.release_timed_out = !sync.cv.wait_for(lock, std::chrono::seconds(5), [&sync]() { return sync.release; });
}

} // namespace

// ControlTraceW と CloseTrace が失敗し、ProcessTrace が戻らない場合に、停止が期限の後に処理スレッドを切り離して戻ることの確認
TEST_F(trace_etw_sessionTest, stop_abandons_worker_after_timeout_when_control_and_close_trace_fail)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    etw_abandon_sync sync;
    cplat_etw_session *session = nullptr;
    cplat_thread *detached_thread = nullptr;
    int timed_join_result = CPLAT_OK;
    bool process_returned_at_stop = true;
    const PROCESSTRACE_HANDLE expected_trace_handle = static_cast<PROCESSTRACE_HANDLE>(66U);
    test_trace_etw_session_set_stop_abandon_timeout_ms(100); // [状態] - 停止の待機の上限を 100 [ms] に短くする。

    // Pre-Assert
    EXPECT_CALL(mock_windows, StartTraceW(_, _, _, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, CONTROLTRACE_ID *trace_id, LPCWSTR, PEVENT_TRACE_PROPERTIES)
            {
                *trace_id = 15U;
                return static_cast<ULONG>(ERROR_SUCCESS);
            }); // [Pre-Assert確認_正常系] - StartTraceW() が呼ばれセッション ハンドルを返すこと。
    EXPECT_CALL(mock_windows, EnableTraceEx2(_, _, _, 15U, _, _, _, _, _, _, _))
        .WillOnce(Return(static_cast<ULONG>(ERROR_SUCCESS))); // [Pre-Assert確認_正常系] - EnableTraceEx2() が成功すること。
    EXPECT_CALL(mock_windows, OpenTraceW(_, _, _, _))
        .WillOnce(Return(expected_trace_handle)); // [Pre-Assert確認_正常系] - OpenTraceW() が有効なトレース ハンドルを返すこと。
    EXPECT_CALL(mock_windows, ProcessTrace(_, _, _, _, 1UL, nullptr, nullptr))
        .WillOnce(
            [&sync](const char *, const int, const char *, PROCESSTRACE_HANDLE *, ULONG, LPFILETIME, LPFILETIME)
            {
                wait_release_and_return(sync);
                std::lock_guard<std::mutex> lock(sync.mtx);
                sync.process_returned = true;
                return static_cast<ULONG>(ERROR_SUCCESS);
            }); // [Pre-Assert確認_正常系] - ProcessTrace() がテストの解放まで戻らないこと。

    {
        InSequence seq;
        EXPECT_CALL(mock_windows, ControlTraceW(_, _, _, 15U, nullptr, _, EVENT_TRACE_CONTROL_STOP))
            .WillOnce(Return(static_cast<ULONG>(ERROR_ACCESS_DENIED))); // [Pre-Assert確認_異常系] - ControlTraceW() が ERROR_ACCESS_DENIED を返すこと。
        EXPECT_CALL(mock_windows, CloseTrace(_, _, _, expected_trace_handle))
            .WillOnce(Return(static_cast<ULONG>(ERROR_INVALID_HANDLE))); // [Pre-Assert確認_異常系] - CloseTrace() が 1 回だけ呼ばれ ERROR_INVALID_HANDLE を返すこと。
        EXPECT_CALL(mock_cplat, cplat_thread_timed_join(_, 100))
            .WillOnce(
                [&timed_join_result](cplat_thread *thread, int timeout_ms)
                {
                    timed_join_result = delegate_real_cplat_thread_timed_join(thread, timeout_ms);
                    return timed_join_result;
                }); // [Pre-Assert確認_異常系] - CloseTrace() の後に、上限の 100 [ms] で cplat_thread_timed_join() が呼ばれること。
        EXPECT_CALL(mock_cplat, cplat_thread_detach(_))
            .WillOnce(SaveArg<0>(&detached_thread)); // [Pre-Assert確認_異常系] - 期限の後に cplat_thread_detach() で処理スレッドを切り離すこと。
    }
    EXPECT_CALL(mock_cplat, cplat_thread_join(_))
        .Times(0); // [Pre-Assert確認_異常系] - 無期限の待機を使わないこと。

    // Act
    int start_ret = cplat_etw_session_start("UnitTestEtwStopAbandon", "0dfe6031-5678-4688-aee8-611340997caa",
                                            count_callback, &sync, &session); // [手順] - ETW セッションを開始する。
    ASSERT_EQ(CPLAT_OK, start_ret); // [状態確認] - cplat_etw_session_start() が CPLAT_OK を返すこと。
    ASSERT_NE(nullptr, session);    // [状態確認] - session が nullptr 以外であること。

    cplat_etw_session_stop(session); // [手順] - ControlTraceW() と CloseTrace() が失敗する状態で cplat_etw_session_stop() を呼び出す。
    {
        std::lock_guard<std::mutex> lock(sync.mtx);
        process_returned_at_stop = sync.process_returned;
    }

    // Assert
    EXPECT_EQ(CPLAT_ERR_TIMEOUT, timed_join_result); // [確認_異常系] - 処理スレッドの待機が CPLAT_ERR_TIMEOUT で期限切れになったこと。
    EXPECT_FALSE(process_returned_at_stop); // [確認_異常系] - 停止が戻った時点で ProcessTrace() が戻っていないこと。
    EXPECT_NE(nullptr, detached_thread); // [確認_異常系] - 切り離した処理スレッドのハンドルが nullptr 以外であること。

    // Cleanup
    {
        std::lock_guard<std::mutex> lock(sync.mtx);
        sync.release = true;
        sync.cv.notify_all();
    }
    if (detached_thread != nullptr)
    {
        delegate_real_cplat_thread_join(detached_thread);
        test_trace_etw_session_dispose_abandoned(session);
    }
}

// 停止が処理スレッドを切り離して戻った後に、ProcessTrace がイベントを渡しても、利用側のコールバックを呼ばないことの確認
TEST_F(trace_etw_sessionTest, stop_suppresses_callback_after_abandoning_worker)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    etw_abandon_sync sync;
    cplat_etw_session *session = nullptr;
    cplat_thread *detached_thread = nullptr;
    bool first_delivered = false;
    bool process_returned = false;
    const PROCESSTRACE_HANDLE expected_trace_handle = static_cast<PROCESSTRACE_HANDLE>(77U);
    test_trace_etw_session_set_stop_abandon_timeout_ms(100); // [状態] - 停止の待機の上限を 100 [ms] に短くする。

    // Pre-Assert
    EXPECT_CALL(mock_windows, StartTraceW(_, _, _, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, CONTROLTRACE_ID *trace_id, LPCWSTR, PEVENT_TRACE_PROPERTIES)
            {
                *trace_id = 16U;
                return static_cast<ULONG>(ERROR_SUCCESS);
            }); // [Pre-Assert確認_正常系] - StartTraceW() が呼ばれセッション ハンドルを返すこと。
    EXPECT_CALL(mock_windows, EnableTraceEx2(_, _, _, 16U, _, _, _, _, _, _, _))
        .WillOnce(Return(static_cast<ULONG>(ERROR_SUCCESS))); // [Pre-Assert確認_正常系] - EnableTraceEx2() が成功すること。
    EXPECT_CALL(mock_windows, OpenTraceW(_, _, _, _))
        .WillOnce(
            [&sync, expected_trace_handle](const char *, const int, const char *, PEVENT_TRACE_LOGFILEW logfile)
            {
                sync.captured_session = static_cast<cplat_etw_session *>(logfile->Context);
                return expected_trace_handle;
            }); // [Pre-Assert確認_正常系] - OpenTraceW() がセッションを Context に受け取り、有効なトレース ハンドルを返すこと。
    EXPECT_CALL(mock_windows, ProcessTrace(_, _, _, _, 1UL, nullptr, nullptr))
        .WillOnce(
            [&sync](const char *, const int, const char *, PROCESSTRACE_HANDLE *, ULONG, LPFILETIME, LPFILETIME)
            {
                cplat_etw_event event = {};

                event.message = "before";
                test_trace_etw_session_deliver_event(sync.captured_session, &event);
                {
                    std::lock_guard<std::mutex> lock(sync.mtx);
                    sync.first_delivered = true;
                    sync.cv.notify_all();
                }

                wait_release_and_return(sync);
                event.message = "after";
                test_trace_etw_session_deliver_event(sync.captured_session, &event);

                std::lock_guard<std::mutex> lock(sync.mtx);
                sync.process_returned = true;
                sync.cv.notify_all();
                return static_cast<ULONG>(ERROR_SUCCESS);
            }); // [Pre-Assert確認_正常系] - ProcessTrace() が停止の前と、停止が戻った後に 1 件ずつイベントを渡すこと。

    {
        InSequence seq;
        EXPECT_CALL(mock_windows, ControlTraceW(_, _, _, 16U, nullptr, _, EVENT_TRACE_CONTROL_STOP))
            .WillOnce(Return(static_cast<ULONG>(ERROR_ACCESS_DENIED))); // [Pre-Assert確認_異常系] - ControlTraceW() が ERROR_ACCESS_DENIED を返すこと。
        EXPECT_CALL(mock_windows, CloseTrace(_, _, _, expected_trace_handle))
            .WillOnce(Return(static_cast<ULONG>(ERROR_INVALID_HANDLE))); // [Pre-Assert確認_異常系] - CloseTrace() が ERROR_INVALID_HANDLE を返すこと。
        EXPECT_CALL(mock_cplat, cplat_thread_timed_join(_, 100))
            .WillOnce(DoDefault()); // [Pre-Assert確認_異常系] - 上限の 100 [ms] で cplat_thread_timed_join() が呼ばれること。
        EXPECT_CALL(mock_cplat, cplat_thread_detach(_))
            .WillOnce(SaveArg<0>(&detached_thread)); // [Pre-Assert確認_異常系] - 期限の後に cplat_thread_detach() で処理スレッドを切り離すこと。
    }

    // Act
    int start_ret = cplat_etw_session_start("UnitTestEtwStopSuppress", "0dfe6031-5678-4688-aee8-611340997caa",
                                            count_callback, &sync, &session); // [手順] - ETW セッションを開始する。
    ASSERT_EQ(CPLAT_OK, start_ret); // [状態確認] - cplat_etw_session_start() が CPLAT_OK を返すこと。
    ASSERT_NE(nullptr, session);    // [状態確認] - session が nullptr 以外であること。
    {
        std::unique_lock<std::mutex> lock(sync.mtx);
        first_delivered =
            sync.cv.wait_for(lock, std::chrono::seconds(5), [&sync]() { return sync.first_delivered; }); // [手順] - 停止の前のイベントの受け渡しを待つ。
    }
    ASSERT_TRUE(first_delivered); // [状態確認] - 停止の前のイベントの受け渡しが 5 秒以内に終わること。

    cplat_etw_session_stop(session); // [手順] - ControlTraceW() と CloseTrace() が失敗する状態で cplat_etw_session_stop() を呼び出す。
    {
        std::unique_lock<std::mutex> lock(sync.mtx);
        sync.release = true;
        sync.cv.notify_all();
        process_returned =
            sync.cv.wait_for(lock, std::chrono::seconds(5), [&sync]() { return sync.process_returned; }); // [手順] - 停止が戻った後に ProcessTrace() へイベントを渡させ、戻るのを待つ。
    }
    ASSERT_TRUE(process_returned); // [状態確認] - ProcessTrace() が 5 秒以内に戻ること。

    // Assert
    EXPECT_EQ(1, sync.callback_count); // [確認_異常系] - 利用側のコールバックが停止の前の 1 回だけ呼ばれ、停止が戻った後には呼ばれないこと。

    // Cleanup
    if (detached_thread != nullptr)
    {
        delegate_real_cplat_thread_join(detached_thread);
        test_trace_etw_session_dispose_abandoned(session);
    }
}

// 利用側のコールバックの実行中に期限を超えた場合、停止がそのコールバックの終了を待ってから戻ることの確認
TEST_F(trace_etw_sessionTest, stop_waits_running_callback_before_abandoning_worker)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    etw_abandon_sync sync;
    cplat_etw_session *session = nullptr;
    cplat_thread *detached_thread = nullptr;
    bool callback_entered = false;
    bool callback_finished_at_stop = false;
    const PROCESSTRACE_HANDLE expected_trace_handle = static_cast<PROCESSTRACE_HANDLE>(88U);
    test_trace_etw_session_set_stop_abandon_timeout_ms(100); // [状態] - 停止の待機の上限を 100 [ms] に短くする。

    // Pre-Assert
    EXPECT_CALL(mock_windows, StartTraceW(_, _, _, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, CONTROLTRACE_ID *trace_id, LPCWSTR, PEVENT_TRACE_PROPERTIES)
            {
                *trace_id = 17U;
                return static_cast<ULONG>(ERROR_SUCCESS);
            }); // [Pre-Assert確認_正常系] - StartTraceW() が呼ばれセッション ハンドルを返すこと。
    EXPECT_CALL(mock_windows, EnableTraceEx2(_, _, _, 17U, _, _, _, _, _, _, _))
        .WillOnce(Return(static_cast<ULONG>(ERROR_SUCCESS))); // [Pre-Assert確認_正常系] - EnableTraceEx2() が成功すること。
    EXPECT_CALL(mock_windows, OpenTraceW(_, _, _, _))
        .WillOnce(
            [&sync, expected_trace_handle](const char *, const int, const char *, PEVENT_TRACE_LOGFILEW logfile)
            {
                sync.captured_session = static_cast<cplat_etw_session *>(logfile->Context);
                return expected_trace_handle;
            }); // [Pre-Assert確認_正常系] - OpenTraceW() がセッションを Context に受け取り、有効なトレース ハンドルを返すこと。
    EXPECT_CALL(mock_windows, ProcessTrace(_, _, _, _, 1UL, nullptr, nullptr))
        .WillOnce(
            [&sync](const char *, const int, const char *, PROCESSTRACE_HANDLE *, ULONG, LPFILETIME, LPFILETIME)
            {
                cplat_etw_event event = {};

                event.message = "running";
                test_trace_etw_session_deliver_event(sync.captured_session, &event);
                wait_release_and_return(sync);
                return static_cast<ULONG>(ERROR_SUCCESS);
            }); // [Pre-Assert確認_正常系] - ProcessTrace() がイベントを 1 件渡し、テストの解放まで戻らないこと。
    EXPECT_CALL(mock_cplat, cplat_sleep_ms(1))
        .Times(AtLeast(1))
        .WillRepeatedly(
            [&sync](int ms)
            {
                {
                    std::lock_guard<std::mutex> lock(sync.mtx);
                    sync.sleep_called = true;
                    sync.cv.notify_all();
                }
                delegate_real_cplat_sleep_ms(ms);
            }); // [Pre-Assert確認_異常系] - 期限の後に、実行中のコールバックの終了を cplat_sleep_ms(1) の繰り返しで待つこと。

    {
        InSequence seq;
        EXPECT_CALL(mock_windows, ControlTraceW(_, _, _, 17U, nullptr, _, EVENT_TRACE_CONTROL_STOP))
            .WillOnce(Return(static_cast<ULONG>(ERROR_ACCESS_DENIED))); // [Pre-Assert確認_異常系] - ControlTraceW() が ERROR_ACCESS_DENIED を返すこと。
        EXPECT_CALL(mock_windows, CloseTrace(_, _, _, expected_trace_handle))
            .WillOnce(Return(static_cast<ULONG>(ERROR_INVALID_HANDLE))); // [Pre-Assert確認_異常系] - CloseTrace() が ERROR_INVALID_HANDLE を返すこと。
        EXPECT_CALL(mock_cplat, cplat_thread_timed_join(_, 100))
            .WillOnce(DoDefault()); // [Pre-Assert確認_異常系] - 上限の 100 [ms] で cplat_thread_timed_join() が呼ばれること。
        EXPECT_CALL(mock_cplat, cplat_thread_detach(_))
            .WillOnce(SaveArg<0>(&detached_thread)); // [Pre-Assert確認_異常系] - コールバックの終了の後に cplat_thread_detach() で処理スレッドを切り離すこと。
    }

    // Act
    int start_ret = cplat_etw_session_start("UnitTestEtwStopRunning", "0dfe6031-5678-4688-aee8-611340997caa",
                                            blocking_callback, &sync, &session); // [手順] - ETW セッションを開始する。
    ASSERT_EQ(CPLAT_OK, start_ret); // [状態確認] - cplat_etw_session_start() が CPLAT_OK を返すこと。
    ASSERT_NE(nullptr, session);    // [状態確認] - session が nullptr 以外であること。
    {
        std::unique_lock<std::mutex> lock(sync.mtx);
        callback_entered =
            sync.cv.wait_for(lock, std::chrono::seconds(5), [&sync]() { return sync.callback_entered; }); // [手順] - 利用側のコールバックの開始を待つ。
    }
    ASSERT_TRUE(callback_entered); // [状態確認] - 利用側のコールバックが 5 秒以内に始まること。

    cplat_etw_session_stop(session); // [手順] - 利用側のコールバックの実行中に、ControlTraceW() と CloseTrace() が失敗する状態で cplat_etw_session_stop() を呼び出す。
    {
        std::lock_guard<std::mutex> lock(sync.mtx);
        callback_finished_at_stop = sync.callback_finished;
    }

    // Assert
    EXPECT_TRUE(callback_finished_at_stop); // [確認_異常系] - 停止が戻った時点で、利用側のコールバックが終わっていること。
    EXPECT_FALSE(sync.callback_wait_timed_out); // [確認_異常系] - 利用側のコールバックが、停止の cplat_sleep_ms(1) による待機の開始を受けて終わったこと。

    // Cleanup
    {
        std::lock_guard<std::mutex> lock(sync.mtx);
        sync.release = true;
        sync.cv.notify_all();
    }
    if (detached_thread != nullptr)
    {
        delegate_real_cplat_thread_join(detached_thread);
        test_trace_etw_session_dispose_abandoned(session);
    }
}

#endif /* PLATFORM_WINDOWS */
