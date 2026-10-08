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

static void collect_callback(const cplat_etw_event *event, void *context)
{
    EventCollector *collector = static_cast<EventCollector *>(context);
    std::lock_guard<std::mutex> lock(collector->mtx);
    ASSERT_NE((const cplat_etw_event *)NULL, event);
    // [状態確認] - `(const cplat_etw_event *)NULL` と `event` が異なること。
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
    cplat_etw_session_stop(NULL); // [手順] - NULL セッションで cplat_etw_session_stop を呼び出す。

    // Assert
    SUCCEED(); // [確認_正常系] - クラッシュせずに完了すること。
}

// session_start の必須引数 NULL が ERR_INVALID_ARGUMENT で拒否されることの確認
TEST_F(etwSessionIntegrationTest, test_session_start_null_params)
{
    // Arrange
    cplat_etw_session *session = NULL; // [状態] - session の受け取り先を NULL で初期化する。

    // Pre-Assert

    // Act
    int actual_ret_null_name =
        cplat_etw_session_start(NULL, TEST_PROVIDER_GUID, collect_callback, NULL,
                                   &session); // [手順] - session_name に NULL を渡して session_start を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_name); // [確認_異常系] - session_name が NULL の場合に cplat_etw_session_start の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ((cplat_etw_session *)NULL,
              session); // [確認_異常系] - session_name が NULL の場合に session に NULL が格納されること。

    // Act_2
    int actual_ret_null_provider_guid =
        cplat_etw_session_start("test", NULL, collect_callback, NULL,
                                   &session); // [手順] - provider_guid に NULL を渡して session_start を呼び出す。

    // Assert_2
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_provider_guid); // [確認_異常系] - provider_guid が NULL の場合に cplat_etw_session_start の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。

    // Act_3
    int actual_ret_null_callback =
        cplat_etw_session_start("test", TEST_PROVIDER_GUID, NULL, NULL,
                                   &session); // [手順] - callback に NULL を渡して session_start を呼び出す。

    // Assert_3
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_callback); // [確認_異常系] - callback が NULL の場合に cplat_etw_session_start の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。

    // Act_4
    int actual_ret_null_session_out =
        cplat_etw_session_start("test", TEST_PROVIDER_GUID, collect_callback, NULL,
                                   NULL); // [手順] - session の受け取り先に NULL を渡して session_start を呼び出す。

    // Assert_4
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_session_out); // [確認_異常系] - 受け取り先が NULL の場合に cplat_etw_session_start の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// 不正な GUID 文字列が ERR_INVALID_ARGUMENT で拒否されることの確認
TEST_F(etwSessionIntegrationTest, test_session_start_invalid_guid)
{
    // Arrange
    cplat_etw_session *session = NULL; // [状態] - session の受け取り先を NULL で初期化する。

    // Pre-Assert

    // Act
    int actual_ret_invalid_guid =
        cplat_etw_session_start("test", "not-a-guid", collect_callback, NULL,
                                   &session); // [手順] - 不正な GUID 文字列 "not-a-guid" で session_start を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_invalid_guid); // [確認_異常系] - cplat_etw_session_start の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ((cplat_etw_session *)NULL, session); // [確認_異常系] - session に NULL が格納されること。
}

class etwSessionSubscribeIntegrationTest : public Test
{
  protected:
    void SetUp() override
    {
        int status = cplat_etw_session_check_access();
        ASSERT_NE(CPLAT_ERR_PERMISSION_DENIED, status)
            << "ETW session の開始権限がありません。Administrators または "
               "\"Performance Log Users\" が必要です。\n"
               "対処方法: net localgroup \"Performance Log Users\" %USERNAME% /add\n"
               "          この操作後にサインアウト/サインインが必要です。";
        // [状態確認] - `CPLAT_ERR_PERMISSION_DENIED` と `status` が異なること。
        ASSERT_EQ(CPLAT_OK, status) << "cplat_etw_session_check_access failed (status=" << status << ")";
        // [状態確認] - `cplat_etw_session_check_access()` の戻り値が `CPLAT_OK` であること。
    }
};

// ASCII メッセージが session で購読・受信できることの確認
TEST_F(etwSessionSubscribeIntegrationTest, test_subscribe_ascii)
{
    // Arrange
    std::string session_name = make_session_name(); // [状態] - プロセス固有の session 名を生成する。
    EventCollector collector;

    cplat_etw_provider *handle = cplat_etw_provider_create(s_test_provider); // [状態] - 登録済みの ETW provider を用意する。
    ASSERT_NE((cplat_etw_provider *)NULL, handle); // [状態確認] - ハンドルが非 NULL であること。

    cplat_etw_session *session = NULL;
    int start_result =
        cplat_etw_session_start(session_name.c_str(), TEST_PROVIDER_GUID, collect_callback, &collector,
                                   &session); // [状態] - 収集 callback 付きで ETW session を開始する。
    ASSERT_EQ(CPLAT_OK, start_result); // [状態確認] - cplat_etw_session_start の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    Sleep(200);
    cplat_etw_provider_write(handle, 4, NULL, "hello world"); // [手順] - ASCII メッセージを書き込む。
    cplat_etw_session_stop(session);                          // [手順] - session を停止して受信を確定させる。

    // Assert
    bool found = false;
    for (const auto &evt : collector.events)
    {
        if (evt.message == "hello world")
        {
            EXPECT_EQ(4, evt.level);                                    // [確認_正常系] - INFO レベルで受信されること。
            EXPECT_EQ((uint32_t)GetCurrentProcessId(), evt.process_id); // [確認_正常系] - 発行元 PID が受信されること。
            EXPECT_EQ("Trace", evt.event_name);                         // [確認_正常系] - イベント名が受信されること。
            EXPECT_FALSE(evt.has_service);              // [確認_正常系] - Service なしで受信されること。
            EXPECT_NE((int64_t)0, evt.timestamp_100ns); // [確認_正常系] - ETW タイムスタンプが設定されること。
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "Expected event 'hello world' not found";
    // [確認_正常系] - `found` が true であること。

    // Cleanup
    cplat_etw_provider_dispose(handle);
}

// 日本語 UTF-8 メッセージが session で購読・受信できることの確認
TEST_F(etwSessionSubscribeIntegrationTest, test_subscribe_utf8_japanese)
{
    // Arrange
    std::string session_name = make_session_name(); // [状態] - プロセス固有の session 名を生成する。
    EventCollector collector;
    const char *msg = "\xe8\xa8\x88\xe7\xae\x97\xe7\xb5\x90\xe6\x9e\x9c: "
                      "\xe6\x88\x90\xe5\x8a\x9f";

    cplat_etw_provider *handle = cplat_etw_provider_create(s_test_provider); // [状態] - 登録済みの ETW provider を用意する。
    ASSERT_NE((cplat_etw_provider *)NULL, handle); // [状態確認] - ハンドルが非 NULL であること。

    cplat_etw_session *session = NULL;
    int start_result =
        cplat_etw_session_start(session_name.c_str(), TEST_PROVIDER_GUID, collect_callback, &collector,
                                   &session); // [状態] - 収集 callback 付きで ETW session を開始する。
    ASSERT_EQ(CPLAT_OK, start_result); // [状態確認] - cplat_etw_session_start の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    Sleep(200);
    cplat_etw_provider_write(handle, 3, NULL, msg); // [手順] - 日本語 UTF-8 メッセージを書き込む。
    cplat_etw_session_stop(session);                // [手順] - session を停止して受信を確定させる。

    // Assert
    bool found = false;
    for (const auto &evt : collector.events)
    {
        if (evt.message == msg)
        {
            EXPECT_EQ(3, evt.level); // [確認_正常系] - WARNING レベルで受信されること。
            EXPECT_EQ((uint32_t)GetCurrentProcessId(), evt.process_id); // [確認_正常系] - 発行元 PID が受信されること。
            EXPECT_EQ("Trace", evt.event_name);                         // [確認_正常系] - イベント名が受信されること。
            EXPECT_FALSE(evt.has_service);              // [確認_正常系] - Service なしで受信されること。
            EXPECT_NE((int64_t)0, evt.timestamp_100ns); // [確認_正常系] - ETW タイムスタンプが設定されること。
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "Expected UTF-8 Japanese event not found";
    // [確認_正常系] - `found` が true であること。

    // Cleanup
    cplat_etw_provider_dispose(handle);
}

// ASCII と絵文字を含む混在 UTF-8 メッセージが購読・受信できることの確認
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

    cplat_etw_provider *handle = cplat_etw_provider_create(s_test_provider); // [状態] - 登録済みの ETW provider を用意する。
    ASSERT_NE((cplat_etw_provider *)NULL, handle); // [状態確認] - ハンドルが非 NULL であること。

    cplat_etw_session *session = NULL;
    int start_result =
        cplat_etw_session_start(session_name.c_str(), TEST_PROVIDER_GUID, collect_callback, &collector,
                                   &session); // [状態] - 収集 callback 付きで ETW session を開始する。
    ASSERT_EQ(CPLAT_OK, start_result); // [状態確認] - cplat_etw_session_start の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    Sleep(200);
    cplat_etw_provider_write(handle, 2, NULL, msg); // [手順] - 混在 UTF-8 メッセージを書き込む。
    cplat_etw_session_stop(session);                // [手順] - session を停止して受信を確定させる。

    // Assert
    bool found = false;
    for (const auto &evt : collector.events)
    {
        if (evt.message == msg)
        {
            EXPECT_EQ(2, evt.level); // [確認_正常系] - ERROR レベルで受信されること。
            EXPECT_EQ((uint32_t)GetCurrentProcessId(), evt.process_id); // [確認_正常系] - 発行元 PID が受信されること。
            EXPECT_EQ("Trace", evt.event_name);                         // [確認_正常系] - イベント名が受信されること。
            EXPECT_FALSE(evt.has_service);              // [確認_正常系] - Service なしで受信されること。
            EXPECT_NE((int64_t)0, evt.timestamp_100ns); // [確認_正常系] - ETW タイムスタンプが設定されること。
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "Expected UTF-8 mixed event not found";
    // [確認_正常系] - `found` が true であること。

    // Cleanup
    cplat_etw_provider_dispose(handle);
}

// 全レベルのイベントが購読・受信できることの確認
TEST_F(etwSessionSubscribeIntegrationTest, test_subscribe_multiple_levels)
{
    // Arrange
    std::string session_name = make_session_name(); // [状態] - プロセス固有の session 名を生成する。
    EventCollector collector;

    cplat_etw_provider *handle = cplat_etw_provider_create(s_test_provider); // [状態] - 登録済みの ETW provider を用意する。
    ASSERT_NE((cplat_etw_provider *)NULL, handle); // [状態確認] - ハンドルが非 NULL であること。

    cplat_etw_session *session = NULL;
    int start_result =
        cplat_etw_session_start(session_name.c_str(), TEST_PROVIDER_GUID, collect_callback, &collector,
                                   &session); // [状態] - 収集 callback 付きで ETW session を開始する。
    ASSERT_EQ(CPLAT_OK, start_result); // [状態確認] - cplat_etw_session_start の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    Sleep(200);
    cplat_etw_provider_write(handle, 1, NULL, "critical_msg"); // [手順] - CRITICAL を書き込む。
    cplat_etw_provider_write(handle, 2, NULL, "error_msg");    // [手順] - ERROR を書き込む。
    cplat_etw_provider_write(handle, 3, NULL, "warning_msg");  // [手順] - WARNING を書き込む。
    cplat_etw_provider_write(handle, 4, NULL, "info_msg");     // [手順] - INFO を書き込む。
    cplat_etw_provider_write(handle, 5, NULL, "verbose_msg");  // [手順] - VERBOSE を書き込む。
    cplat_etw_session_stop(session);                           // [手順] - session を停止して受信を確定させる。

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
            // [確認_正常系] - 受信イベントのレベルが `1` であること。
            EXPECT_EQ((uint32_t)GetCurrentProcessId(), evt.process_id);
            // [確認_正常系] - 受信イベントのプロセス ID が発行元プロセスの ID であること。
            EXPECT_EQ("Trace", evt.event_name);
            // [確認_正常系] - 受信イベントの名前が `"Trace"` であること。
            saw_critical = true;
        }
        else if (evt.message == "error_msg")
        {
            EXPECT_EQ(2, evt.level);
            // [確認_正常系] - 受信イベントのレベルが `2` であること。
            EXPECT_EQ((uint32_t)GetCurrentProcessId(), evt.process_id);
            // [確認_正常系] - 受信イベントのプロセス ID が発行元プロセスの ID であること。
            EXPECT_EQ("Trace", evt.event_name);
            // [確認_正常系] - 受信イベントの名前が `"Trace"` であること。
            saw_error = true;
        }
        else if (evt.message == "warning_msg")
        {
            EXPECT_EQ(3, evt.level);
            // [確認_正常系] - 受信イベントのレベルが `3` であること。
            EXPECT_EQ((uint32_t)GetCurrentProcessId(), evt.process_id);
            // [確認_正常系] - 受信イベントのプロセス ID が発行元プロセスの ID であること。
            EXPECT_EQ("Trace", evt.event_name);
            // [確認_正常系] - 受信イベントの名前が `"Trace"` であること。
            saw_warning = true;
        }
        else if (evt.message == "info_msg")
        {
            EXPECT_EQ(4, evt.level);
            // [確認_正常系] - 受信イベントのレベルが `4` であること。
            EXPECT_EQ((uint32_t)GetCurrentProcessId(), evt.process_id);
            // [確認_正常系] - 受信イベントのプロセス ID が発行元プロセスの ID であること。
            EXPECT_EQ("Trace", evt.event_name);
            // [確認_正常系] - 受信イベントの名前が `"Trace"` であること。
            saw_info = true;
        }
        else if (evt.message == "verbose_msg")
        {
            EXPECT_EQ(5, evt.level);
            // [確認_正常系] - 受信イベントのレベルが `5` であること。
            EXPECT_EQ((uint32_t)GetCurrentProcessId(), evt.process_id);
            // [確認_正常系] - 受信イベントのプロセス ID が発行元プロセスの ID であること。
            EXPECT_EQ("Trace", evt.event_name);
            // [確認_正常系] - 受信イベントの名前が `"Trace"` であること。
            saw_verbose = true;
        }
        EXPECT_FALSE(evt.has_service);              // [確認_正常系 回数=5] - 既存ケースでは Service なしで受信されること。
        EXPECT_NE((int64_t)0, evt.timestamp_100ns); // [確認_正常系 回数=5] - 各イベントに ETW タイムスタンプが設定されること。
    }

    EXPECT_TRUE(saw_critical); // [確認_正常系] - CRITICAL が受信されること。
    EXPECT_TRUE(saw_error);    // [確認_正常系] - ERROR が受信されること。
    EXPECT_TRUE(saw_warning);  // [確認_正常系] - WARNING が受信されること。
    EXPECT_TRUE(saw_info);     // [確認_正常系] - INFO が受信されること。
    EXPECT_TRUE(saw_verbose);  // [確認_正常系] - VERBOSE が受信されること。

    // Cleanup
    cplat_etw_provider_dispose(handle);
}

// 空文字列メッセージが購読・受信できることの確認
TEST_F(etwSessionSubscribeIntegrationTest, test_subscribe_empty_string)
{
    // Arrange
    std::string session_name = make_session_name(); // [状態] - プロセス固有の session 名を生成する。
    EventCollector collector;

    cplat_etw_provider *handle = cplat_etw_provider_create(s_test_provider); // [状態] - 登録済みの ETW provider を用意する。
    ASSERT_NE((cplat_etw_provider *)NULL, handle); // [状態確認] - ハンドルが非 NULL であること。

    cplat_etw_session *session = NULL;
    int start_result =
        cplat_etw_session_start(session_name.c_str(), TEST_PROVIDER_GUID, collect_callback, &collector,
                                   &session); // [状態] - 収集 callback 付きで ETW session を開始する。
    ASSERT_EQ(CPLAT_OK, start_result); // [状態確認] - cplat_etw_session_start の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    Sleep(200);
    cplat_etw_provider_write(handle, 5, NULL, ""); // [手順] - 空文字列を書き込む。
    cplat_etw_session_stop(session);               // [手順] - session を停止して受信を確定させる。

    // Assert
    bool found = false;
    for (const auto &evt : collector.events)
    {
        if (evt.message.empty())
        {
            EXPECT_EQ(5, evt.level); // [確認_正常系] - 空文字列が VERBOSE で受信されること。
            EXPECT_EQ((uint32_t)GetCurrentProcessId(), evt.process_id); // [確認_正常系] - 発行元 PID が受信されること。
            EXPECT_EQ("Trace", evt.event_name);                         // [確認_正常系] - イベント名が受信されること。
            EXPECT_FALSE(evt.has_service);              // [確認_正常系] - Service なしで受信されること。
            EXPECT_NE((int64_t)0, evt.timestamp_100ns); // [確認_正常系] - ETW タイムスタンプが設定されること。
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "Expected empty-string event not found";
    // [確認_正常系] - `found` が true であること。

    // Cleanup
    cplat_etw_provider_dispose(handle);
}

// Service と Message を持つイベントが購読・受信できることの確認
TEST_F(etwSessionSubscribeIntegrationTest, test_subscribe_service_and_message)
{
    // Arrange
    std::string session_name = make_session_name(); // [状態] - プロセス固有の session 名を生成する。
    EventCollector collector;

    cplat_etw_provider *handle = cplat_etw_provider_create(s_test_provider); // [状態] - 登録済みの ETW provider を用意する。
    ASSERT_NE((cplat_etw_provider *)NULL, handle); // [状態確認] - ハンドルが非 NULL であること。

    cplat_etw_session *session = NULL;
    int start_result =
        cplat_etw_session_start(session_name.c_str(), TEST_PROVIDER_GUID, collect_callback, &collector,
                                   &session); // [状態] - 収集 callback 付きで ETW session を開始する。
    ASSERT_EQ(CPLAT_OK, start_result); // [状態確認] - cplat_etw_session_start の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    Sleep(200);
    cplat_etw_provider_write(handle, 4, "worker-1",
                                "service_msg"); // [手順] - Service と Message を持つイベントを書き込む。
    cplat_etw_session_stop(session);         // [手順] - session を停止して受信を確定させる。

    // Assert
    bool found = false;
    for (const auto &evt : collector.events)
    {
        if (evt.message == "service_msg")
        {
            EXPECT_EQ(4, evt.level);                                    // [確認_正常系] - INFO レベルで受信されること。
            EXPECT_EQ((uint32_t)GetCurrentProcessId(), evt.process_id); // [確認_正常系] - 発行元 PID が受信されること。
            EXPECT_EQ("Trace", evt.event_name);                         // [確認_正常系] - イベント名が受信されること。
            EXPECT_TRUE(evt.has_service);               // [確認_正常系] - Service フィールドが受信されること。
            EXPECT_EQ("worker-1", evt.service);         // [確認_正常系] - Service 名が復元されること。
            EXPECT_NE((int64_t)0, evt.timestamp_100ns); // [確認_正常系] - ETW タイムスタンプが設定されること。
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "Expected event with service field not found";
    // [確認_正常系] - `found` が true であること。

    // Cleanup
    cplat_etw_provider_dispose(handle);
}

#endif /* PLATFORM_ */
