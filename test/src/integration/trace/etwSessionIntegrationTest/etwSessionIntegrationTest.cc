#include <cplat/base/platform.h>

#if defined(PLATFORM_WINDOWS)

    #include <cplat/base/windows_sdk.h>
    #include <TraceLoggingProvider.h>
    #include <testfw.h>
    #include <cplat/trace/etw.h>

    #include <mutex>
    #include <vector>
    #include <string>
    #include <utility>
    #include <cstdio>

CPLAT_ETW_DEFINE_PROVIDER(s_test_provider, "EtwSessionTest",
                          (0x0dfe6031, 0x5678, 0x4688, 0xae, 0xe8, 0x61, 0x13, 0x40, 0x99, 0x7c, 0xaa));

    #define TEST_PROVIDER_GUID "0dfe6031-5678-4688-aee8-611340997caa"

struct EventCollector
{
    std::mutex mtx;
    struct EventRecord
    {
        int level;
        uint32_t process_id;
        bool has_service;
        int64_t timestamp_100ns;
        std::string event_name;
        std::string service;
        std::string message;
    };
    std::vector<EventRecord> events;
};

// [サブ手順 名前=etwSessionIntegrationTest.collect_callback]
static void collect_callback(const cplat_etw_event *event, void *context)
{
    EventCollector *collector = static_cast<EventCollector *>(context);
    std::lock_guard<std::mutex> lock(collector->mtx);
    ASSERT_NE(nullptr, event);
    // [状態確認] - event が nullptr 以外であること。
    EventCollector::EventRecord record;
    record.level = event->level;
    record.process_id = event->process_id;
    record.has_service = (event->service != nullptr);
    record.timestamp_100ns = event->timestamp_100ns;
    if (event->event_name != nullptr)
    {
        record.event_name = event->event_name;
    }
    if (event->service != nullptr)
    {
        record.service = event->service;
    }
    if (event->message != nullptr)
    {
        record.message = event->message;
    }
    collector->events.push_back(record);
}
// [サブ手順終了]

static int s_session_counter = 0;

static std::string make_session_name()
{
    char buf[128];
    snprintf(buf, sizeof(buf), "EtwSessionTest_%lu_%d", (unsigned long)GetCurrentProcessId(), ++s_session_counter);
    return std::string(buf);
}

class etwSessionIntegrationTest : public Test
{
};

// NULL セッションで stop を呼び出しても安全なことの確認
TEST_F(etwSessionIntegrationTest, test_session_stop_with_null)
{
    // Arrange

    // Pre-Assert

    // Act
    cplat_etw_session_stop(nullptr); // [手順] - nullptr のセッションを指定して cplat_etw_session_stop() を呼び出す。

    // Assert
    SUCCEED(); // [確認_正常系] - SUCCEED() に到達すること。(nullptr を渡した cplat_etw_session_stop() がクラッシュしないことの確認)
}

// session_start の必須引数 NULL が ERR_INVALID_ARGUMENT で拒否されることの確認
TEST_F(etwSessionIntegrationTest, test_session_start_null_params)
{
    // Arrange
    cplat_etw_session *session = nullptr; // [状態] - session の受け取り先を nullptr で初期化する。

    // Pre-Assert

    // Act
    int actual_ret_null_name = cplat_etw_session_start(
        nullptr, TEST_PROVIDER_GUID, collect_callback, nullptr,
        &session); // [手順] - session_name に nullptr を指定して cplat_etw_session_start() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_name); // [確認_異常系] - session_name が nullptr の cplat_etw_session_start() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        nullptr,
        session); // [確認_異常系] - session_name が nullptr の cplat_etw_session_start() の後の session が nullptr であること。

    // Act_2
    int actual_ret_null_provider_guid = cplat_etw_session_start(
        "test", nullptr, collect_callback, nullptr,
        &session); // [手順] - provider_guid に nullptr を指定して cplat_etw_session_start() を呼び出す。

    // Assert_2
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_provider_guid); // [確認_異常系] - provider_guid が nullptr の cplat_etw_session_start() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。

    // Act_3
    int actual_ret_null_callback = cplat_etw_session_start(
        "test", TEST_PROVIDER_GUID, nullptr, nullptr,
        &session); // [手順] - callback に nullptr を指定して cplat_etw_session_start() を呼び出す。

    // Assert_3
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_callback); // [確認_異常系] - callback が nullptr の cplat_etw_session_start() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。

    // Act_4
    int actual_ret_null_session_out = cplat_etw_session_start(
        "test", TEST_PROVIDER_GUID, collect_callback, nullptr,
        nullptr); // [手順] - session の受け取り先に nullptr を指定して cplat_etw_session_start() を呼び出す。

    // Assert_4
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_session_out); // [確認_異常系] - 受け取り先が nullptr の cplat_etw_session_start() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// 不正な GUID 文字列が ERR_INVALID_ARGUMENT で拒否されることの確認
TEST_F(etwSessionIntegrationTest, test_session_start_invalid_guid)
{
    // Arrange
    cplat_etw_session *session = nullptr; // [状態] - session の受け取り先を nullptr で初期化する。

    // Pre-Assert

    // Act
    int actual_ret_invalid_guid = cplat_etw_session_start(
        "test", "not-a-guid", collect_callback, nullptr,
        &session); // [手順] - 不正な GUID 文字列 "not-a-guid" を指定して cplat_etw_session_start() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_invalid_guid); // [確認_異常系] - cplat_etw_session_start() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        nullptr,
        session); // [確認_異常系] - 不正な GUID 文字列 "not-a-guid" の cplat_etw_session_start() の後の session が nullptr であること。
}

class etwSessionSubscribeIntegrationTest : public Test
{
  protected:
    // [サブ手順 名前=etwSessionSubscribeIntegrationTest.SetUp]
    void SetUp() override
    {
        int status = cplat_etw_session_check_access();
        ASSERT_NE(CPLAT_ERR_PERMISSION_DENIED, status)
            << "ETW session の開始権限がありません。Administrators または "
               "\"Performance Log Users\" が必要です。\n"
               "対処方法: net localgroup \"Performance Log Users\" %USERNAME% /add\n"
               "          この操作後にサインアウト/サインインが必要です。";
        // [状態確認] - status が CPLAT_ERR_PERMISSION_DENIED 以外であること。
        ASSERT_EQ(CPLAT_OK, status) << "cplat_etw_session_check_access failed (status=" << status << ")";
        // [状態確認] - cplat_etw_session_check_access() の戻り値が CPLAT_OK であること。
    }
    // [サブ手順終了]
};

// ASCII メッセージが session で購読・受信できることの確認
// [サブ手順参照 名前=etwSessionSubscribeIntegrationTest.SetUp]
TEST_F(etwSessionSubscribeIntegrationTest, test_subscribe_ascii)
{
    // Arrange
    std::string session_name = make_session_name(); // [状態] - プロセス固有の session 名を生成する。
    EventCollector collector;

    cplat_etw_provider *handle =
        cplat_etw_provider_create(s_test_provider); // [状態] - 登録済みの ETW provider を用意する。
    ASSERT_NE(nullptr,
              handle); // [状態確認] - cplat_etw_provider_create() の戻り値が nullptr 以外であること。

    cplat_etw_session *session = nullptr;
    int start_result = cplat_etw_session_start(session_name.c_str(), TEST_PROVIDER_GUID, collect_callback, &collector,
                                               &session); // [状態] - 収集 callback 付きで ETW session を開始する。
    ASSERT_EQ(CPLAT_OK, start_result); // [状態確認] - cplat_etw_session_start() の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    Sleep(200);
    cplat_etw_provider_write(
        handle, 4, nullptr,
        "hello world"); // [手順] - ASCII メッセージを指定して cplat_etw_provider_write() を呼び出す。
    cplat_etw_session_stop(
        session); // [手順] - 受信を確定させるために session を指定して cplat_etw_session_stop() を呼び出す。

    // Assert
    bool found = false;
    for (const auto &evt : collector.events)
    {
        if (evt.message == "hello world")
        {
            EXPECT_EQ(
                4,
                evt.level); // [確認_正常系] - "hello world" の evt.level が 4 であること。(INFO レベルであることの確認)
            EXPECT_EQ(
                (uint32_t)GetCurrentProcessId(),
                evt.process_id); // [確認_正常系] - evt.process_id が GetCurrentProcessId() の戻り値と一致すること。
            EXPECT_EQ("Trace", evt.event_name); // [確認_正常系] - evt.event_name が "Trace" であること。
            EXPECT_FALSE(
                evt.has_service); // [確認_正常系] - evt.has_service が false であること。(Service なしで受信されることの確認)
            EXPECT_NE(
                (int64_t)0,
                evt.timestamp_100ns); // [確認_正常系] - evt.timestamp_100ns が 0 以外であること。(ETW タイムスタンプが設定されていることの確認)
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "Expected event 'hello world' not found";
    // [確認_正常系] - found が true であること。("hello world" のイベントを受信したことの確認)

    // Cleanup
    cplat_etw_provider_dispose(handle);
}

// 日本語 UTF-8 メッセージが session で購読・受信できることの確認
// [サブ手順参照 名前=etwSessionSubscribeIntegrationTest.SetUp]
TEST_F(etwSessionSubscribeIntegrationTest, test_subscribe_utf8_japanese)
{
    // Arrange
    std::string session_name = make_session_name(); // [状態] - プロセス固有の session 名を生成する。
    EventCollector collector;
    const char *msg = "\xe8\xa8\x88\xe7\xae\x97\xe7\xb5\x90\xe6\x9e\x9c: "
                      "\xe6\x88\x90\xe5\x8a\x9f";

    cplat_etw_provider *handle =
        cplat_etw_provider_create(s_test_provider); // [状態] - 登録済みの ETW provider を用意する。
    ASSERT_NE(nullptr,
              handle); // [状態確認] - cplat_etw_provider_create() の戻り値が nullptr 以外であること。

    cplat_etw_session *session = nullptr;
    int start_result = cplat_etw_session_start(session_name.c_str(), TEST_PROVIDER_GUID, collect_callback, &collector,
                                               &session); // [状態] - 収集 callback 付きで ETW session を開始する。
    ASSERT_EQ(CPLAT_OK, start_result); // [状態確認] - cplat_etw_session_start() の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    Sleep(200);
    cplat_etw_provider_write(handle, 3, nullptr,
                             msg); // [手順] - 日本語 UTF-8 メッセージを指定して cplat_etw_provider_write() を呼び出す。
    cplat_etw_session_stop(
        session); // [手順] - 受信を確定させるために session を指定して cplat_etw_session_stop() を呼び出す。

    // Assert
    bool found = false;
    for (const auto &evt : collector.events)
    {
        if (evt.message == msg)
        {
            EXPECT_EQ(
                3,
                evt.level); // [確認_正常系] - "計算結果: 成功" の evt.level が 3 であること。(WARNING レベルであることの確認)
            EXPECT_EQ(
                (uint32_t)GetCurrentProcessId(),
                evt.process_id); // [確認_正常系] - evt.process_id が GetCurrentProcessId() の戻り値と一致すること。
            EXPECT_EQ("Trace", evt.event_name); // [確認_正常系] - evt.event_name が "Trace" であること。
            EXPECT_FALSE(
                evt.has_service); // [確認_正常系] - evt.has_service が false であること。(Service なしで受信されることの確認)
            EXPECT_NE(
                (int64_t)0,
                evt.timestamp_100ns); // [確認_正常系] - evt.timestamp_100ns が 0 以外であること。(ETW タイムスタンプが設定されていることの確認)
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "Expected UTF-8 Japanese event not found";
    // [確認_正常系] - found が true であること。(UTF-8 の日本語イベントを受信したことの確認)

    // Cleanup
    cplat_etw_provider_dispose(handle);
}

// ASCII と絵文字を含む混在 UTF-8 メッセージが購読・受信できることの確認
// [サブ手順参照 名前=etwSessionSubscribeIntegrationTest.SetUp]
TEST_F(etwSessionSubscribeIntegrationTest, test_subscribe_utf8_mixed)
{
    // Arrange
    std::string session_name = make_session_name(); // [状態] - プロセス固有の session 名を生成する。
    EventCollector collector;
    const char *msg = "Hello "
                      "\xe4\xb8\x96\xe7\x95\x8c"
                      " "
                      "\xf0\x9f\x8c\x8d"
                      " World";

    cplat_etw_provider *handle =
        cplat_etw_provider_create(s_test_provider); // [状態] - 登録済みの ETW provider を用意する。
    ASSERT_NE(nullptr,
              handle); // [状態確認] - cplat_etw_provider_create() の戻り値が nullptr 以外であること。

    cplat_etw_session *session = nullptr;
    int start_result = cplat_etw_session_start(session_name.c_str(), TEST_PROVIDER_GUID, collect_callback, &collector,
                                               &session); // [状態] - 収集 callback 付きで ETW session を開始する。
    ASSERT_EQ(CPLAT_OK, start_result); // [状態確認] - cplat_etw_session_start() の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    Sleep(200);
    cplat_etw_provider_write(handle, 2, nullptr,
                             msg); // [手順] - 混在 UTF-8 メッセージを指定して cplat_etw_provider_write() を呼び出す。
    cplat_etw_session_stop(
        session); // [手順] - 受信を確定させるために session を指定して cplat_etw_session_stop() を呼び出す。

    // Assert
    bool found = false;
    for (const auto &evt : collector.events)
    {
        if (evt.message == msg)
        {
            EXPECT_EQ(
                2,
                evt.level); // [確認_正常系] - "Hello 世界 🌍 World" の evt.level が 2 であること。(ERROR レベルであることの確認)
            EXPECT_EQ(
                (uint32_t)GetCurrentProcessId(),
                evt.process_id); // [確認_正常系] - evt.process_id が GetCurrentProcessId() の戻り値と一致すること。
            EXPECT_EQ("Trace", evt.event_name); // [確認_正常系] - evt.event_name が "Trace" であること。
            EXPECT_FALSE(
                evt.has_service); // [確認_正常系] - evt.has_service が false であること。(Service なしで受信されることの確認)
            EXPECT_NE(
                (int64_t)0,
                evt.timestamp_100ns); // [確認_正常系] - evt.timestamp_100ns が 0 以外であること。(ETW タイムスタンプが設定されていることの確認)
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "Expected UTF-8 mixed event not found";
    // [確認_正常系] - found が true であること。(混在 UTF-8 のイベントを受信したことの確認)

    // Cleanup
    cplat_etw_provider_dispose(handle);
}

// 全レベルのイベントが購読・受信できることの確認
// [サブ手順参照 名前=etwSessionSubscribeIntegrationTest.SetUp]
TEST_F(etwSessionSubscribeIntegrationTest, test_subscribe_multiple_levels)
{
    // Arrange
    std::string session_name = make_session_name(); // [状態] - プロセス固有の session 名を生成する。
    EventCollector collector;

    cplat_etw_provider *handle =
        cplat_etw_provider_create(s_test_provider); // [状態] - 登録済みの ETW provider を用意する。
    ASSERT_NE(nullptr,
              handle); // [状態確認] - cplat_etw_provider_create() の戻り値が nullptr 以外であること。

    cplat_etw_session *session = nullptr;
    int start_result = cplat_etw_session_start(session_name.c_str(), TEST_PROVIDER_GUID, collect_callback, &collector,
                                               &session); // [状態] - 収集 callback 付きで ETW session を開始する。
    ASSERT_EQ(CPLAT_OK, start_result); // [状態確認] - cplat_etw_session_start() の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    Sleep(200);
    cplat_etw_provider_write(
        handle, 1, nullptr,
        "critical_msg"); // [手順] - CRITICAL レベルのメッセージを指定して cplat_etw_provider_write() を呼び出す。
    cplat_etw_provider_write(
        handle, 2, nullptr,
        "error_msg"); // [手順] - ERROR レベルのメッセージを指定して cplat_etw_provider_write() を呼び出す。
    cplat_etw_provider_write(
        handle, 3, nullptr,
        "warning_msg"); // [手順] - WARNING レベルのメッセージを指定して cplat_etw_provider_write() を呼び出す。
    cplat_etw_provider_write(
        handle, 4, nullptr,
        "info_msg"); // [手順] - INFO レベルのメッセージを指定して cplat_etw_provider_write() を呼び出す。
    cplat_etw_provider_write(
        handle, 5, nullptr,
        "verbose_msg"); // [手順] - VERBOSE レベルのメッセージを指定して cplat_etw_provider_write() を呼び出す。
    cplat_etw_session_stop(
        session); // [手順] - 受信を確定させるために session を指定して cplat_etw_session_stop() を呼び出す。

    // Assert
    bool saw_critical = false;
    bool saw_error = false;
    bool saw_warning = false;
    bool saw_info = false;
    bool saw_verbose = false;

    for (const auto &evt : collector.events)
    {
        if (evt.message == "critical_msg")
        {
            EXPECT_EQ(1, evt.level);
            // [確認_正常系] - "critical_msg" の evt.level が 1 であること。(CRITICAL レベルであることの確認)
            EXPECT_EQ((uint32_t)GetCurrentProcessId(), evt.process_id);
            // [確認_正常系] - evt.process_id が GetCurrentProcessId() の戻り値と一致すること。
            EXPECT_EQ("Trace", evt.event_name);
            // [確認_正常系] - evt.event_name が "Trace" であること。
            saw_critical = true;
        }
        else if (evt.message == "error_msg")
        {
            EXPECT_EQ(2, evt.level);
            // [確認_正常系] - "error_msg" の evt.level が 2 であること。(ERROR レベルであることの確認)
            EXPECT_EQ((uint32_t)GetCurrentProcessId(), evt.process_id);
            // [確認_正常系] - evt.process_id が GetCurrentProcessId() の戻り値と一致すること。
            EXPECT_EQ("Trace", evt.event_name);
            // [確認_正常系] - evt.event_name が "Trace" であること。
            saw_error = true;
        }
        else if (evt.message == "warning_msg")
        {
            EXPECT_EQ(3, evt.level);
            // [確認_正常系] - "warning_msg" の evt.level が 3 であること。(WARNING レベルであることの確認)
            EXPECT_EQ((uint32_t)GetCurrentProcessId(), evt.process_id);
            // [確認_正常系] - evt.process_id が GetCurrentProcessId() の戻り値と一致すること。
            EXPECT_EQ("Trace", evt.event_name);
            // [確認_正常系] - evt.event_name が "Trace" であること。
            saw_warning = true;
        }
        else if (evt.message == "info_msg")
        {
            EXPECT_EQ(4, evt.level);
            // [確認_正常系] - "info_msg" の evt.level が 4 であること。(INFO レベルであることの確認)
            EXPECT_EQ((uint32_t)GetCurrentProcessId(), evt.process_id);
            // [確認_正常系] - evt.process_id が GetCurrentProcessId() の戻り値と一致すること。
            EXPECT_EQ("Trace", evt.event_name);
            // [確認_正常系] - evt.event_name が "Trace" であること。
            saw_info = true;
        }
        else if (evt.message == "verbose_msg")
        {
            EXPECT_EQ(5, evt.level);
            // [確認_正常系] - "verbose_msg" の evt.level が 5 であること。(VERBOSE レベルであることの確認)
            EXPECT_EQ((uint32_t)GetCurrentProcessId(), evt.process_id);
            // [確認_正常系] - evt.process_id が GetCurrentProcessId() の戻り値と一致すること。
            EXPECT_EQ("Trace", evt.event_name);
            // [確認_正常系] - evt.event_name が "Trace" であること。
            saw_verbose = true;
        }
        EXPECT_FALSE(
            evt.has_service); // [確認_正常系 回数=5] - evt.has_service が false であること。(既存ケースでは Service なしで受信されることの確認)
        EXPECT_NE(
            (int64_t)0,
            evt.timestamp_100ns); // [確認_正常系 回数=5] - evt.timestamp_100ns が 0 以外であること。(各イベントに ETW タイムスタンプが設定されていることの確認)
    }

    EXPECT_TRUE(saw_critical); // [確認_正常系] - saw_critical が true であること。(CRITICAL が受信されることの確認)
    EXPECT_TRUE(saw_error);    // [確認_正常系] - saw_error が true であること。(ERROR が受信されることの確認)
    EXPECT_TRUE(saw_warning);  // [確認_正常系] - saw_warning が true であること。(WARNING が受信されることの確認)
    EXPECT_TRUE(saw_info);     // [確認_正常系] - saw_info が true であること。(INFO が受信されることの確認)
    EXPECT_TRUE(saw_verbose);  // [確認_正常系] - saw_verbose が true であること。(VERBOSE が受信されることの確認)

    // Cleanup
    cplat_etw_provider_dispose(handle);
}

// 空文字列メッセージが購読・受信できることの確認
// [サブ手順参照 名前=etwSessionSubscribeIntegrationTest.SetUp]
TEST_F(etwSessionSubscribeIntegrationTest, test_subscribe_empty_string)
{
    // Arrange
    std::string session_name = make_session_name(); // [状態] - プロセス固有の session 名を生成する。
    EventCollector collector;

    cplat_etw_provider *handle =
        cplat_etw_provider_create(s_test_provider); // [状態] - 登録済みの ETW provider を用意する。
    ASSERT_NE(nullptr,
              handle); // [状態確認] - cplat_etw_provider_create() の戻り値が nullptr 以外であること。

    cplat_etw_session *session = nullptr;
    int start_result = cplat_etw_session_start(session_name.c_str(), TEST_PROVIDER_GUID, collect_callback, &collector,
                                               &session); // [状態] - 収集 callback 付きで ETW session を開始する。
    ASSERT_EQ(CPLAT_OK, start_result); // [状態確認] - cplat_etw_session_start() の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    Sleep(200);
    cplat_etw_provider_write(handle, 5, nullptr,
                             ""); // [手順] - 空文字列を指定して cplat_etw_provider_write() を呼び出す。
    cplat_etw_session_stop(
        session); // [手順] - 受信を確定させるために session を指定して cplat_etw_session_stop() を呼び出す。

    // Assert
    bool found = false;
    for (const auto &evt : collector.events)
    {
        if (evt.message.empty())
        {
            EXPECT_EQ(
                5, evt.level); // [確認_正常系] - 空文字列の evt.level が 5 であること。(VERBOSE レベルであることの確認)
            EXPECT_EQ(
                (uint32_t)GetCurrentProcessId(),
                evt.process_id); // [確認_正常系] - evt.process_id が GetCurrentProcessId() の戻り値と一致すること。
            EXPECT_EQ("Trace", evt.event_name); // [確認_正常系] - evt.event_name が "Trace" であること。
            EXPECT_FALSE(
                evt.has_service); // [確認_正常系] - evt.has_service が false であること。(Service なしで受信されることの確認)
            EXPECT_NE(
                (int64_t)0,
                evt.timestamp_100ns); // [確認_正常系] - evt.timestamp_100ns が 0 以外であること。(ETW タイムスタンプが設定されていることの確認)
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "Expected empty-string event not found";
    // [確認_正常系] - found が true であること。(空文字列のイベントを受信したことの確認)

    // Cleanup
    cplat_etw_provider_dispose(handle);
}

// Service と Message を持つイベントが購読・受信できることの確認
// [サブ手順参照 名前=etwSessionSubscribeIntegrationTest.SetUp]
TEST_F(etwSessionSubscribeIntegrationTest, test_subscribe_service_and_message)
{
    // Arrange
    std::string session_name = make_session_name(); // [状態] - プロセス固有の session 名を生成する。
    EventCollector collector;

    cplat_etw_provider *handle =
        cplat_etw_provider_create(s_test_provider); // [状態] - 登録済みの ETW provider を用意する。
    ASSERT_NE(nullptr,
              handle); // [状態確認] - cplat_etw_provider_create() の戻り値が nullptr 以外であること。

    cplat_etw_session *session = nullptr;
    int start_result = cplat_etw_session_start(session_name.c_str(), TEST_PROVIDER_GUID, collect_callback, &collector,
                                               &session); // [状態] - 収集 callback 付きで ETW session を開始する。
    ASSERT_EQ(CPLAT_OK, start_result); // [状態確認] - cplat_etw_session_start() の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    Sleep(200);
    cplat_etw_provider_write(
        handle, 4, "worker-1",
        "service_msg"); // [手順] - Service と Message を持つイベントを指定して cplat_etw_provider_write() を呼び出す。
    cplat_etw_session_stop(
        session); // [手順] - 受信を確定させるために session を指定して cplat_etw_session_stop() を呼び出す。

    // Assert
    bool found = false;
    for (const auto &evt : collector.events)
    {
        if (evt.message == "service_msg")
        {
            EXPECT_EQ(
                4,
                evt.level); // [確認_正常系] - "service_msg" の evt.level が 4 であること。(INFO レベルであることの確認)
            EXPECT_EQ(
                (uint32_t)GetCurrentProcessId(),
                evt.process_id); // [確認_正常系] - evt.process_id が GetCurrentProcessId() の戻り値と一致すること。
            EXPECT_EQ("Trace", evt.event_name); // [確認_正常系] - evt.event_name が "Trace" であること。
            EXPECT_TRUE(
                evt.has_service); // [確認_正常系] - evt.has_service が true であること。(Service フィールドが受信されることの確認)
            EXPECT_EQ("worker-1", evt.service); // [確認_正常系] - evt.service が "worker-1" であること。
            EXPECT_NE(
                (int64_t)0,
                evt.timestamp_100ns); // [確認_正常系] - evt.timestamp_100ns が 0 以外であること。(ETW タイムスタンプが設定されていることの確認)
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "Expected event with service field not found";
    // [確認_正常系] - found が true であること。(Service フィールドを持つイベントを受信したことの確認)

    // Cleanup
    cplat_etw_provider_dispose(handle);
}

#endif /* PLATFORM_ */
