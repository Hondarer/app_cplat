/**
 *******************************************************************************
 *  @file           traceHookTest.cc
 *  @brief          cplat_tracer フック API の単体テスト。
 *  @author         Tetsuo Honda
 *  @date           2026/05/02
 *  @version        1.0.0
 *******************************************************************************
 */
#include <testfw.h>
#include <mock_cplat.h>
#include <cplat/trace/tracer.h>
#include "traceSyncMock.h"
#include <string>
#include <cstring>
#include <ctime>
#include <cstdio>
#include <cstdint>
#include <vector>

#include <cplat/trace/tracer_internal.h>

#if defined(PLATFORM_LINUX)
    #include <syslog.h>
#endif

using testing::_;
using testing::NiceMock;
using testing::NotNull;
using testing::Return;

namespace
{

static void set_valid_deadline(struct timespec *abs_timeout)
{
    abs_timeout->tv_sec = (time_t)(time(NULL) + 1);
    abs_timeout->tv_nsec = 0;
}

static void set_fixed_realtime(cplat_timespec *ts)
{
    ts->tv_sec = 1714100645LL;
    ts->tv_nsec = 678000000;
}

static cplat_timespec make_fixed_timestamp(void)
{
    cplat_timespec ts;
    ts.tv_sec = 1714100645LL;
    ts.tv_nsec = 678000000;
    return ts;
}

struct HookRecord
{
    cplat_tracer_hook_entry *prev;
    cplat_tracer *handle;
    cplat_timespec timestamp;
    std::string message;
    void *context;
    cplat_trace_level level;
    uint32_t padding;
};

static std::vector<HookRecord> g_hook_records;

static void reset_hook_records()
{
    g_hook_records.clear();
}

static void recording_hook(cplat_tracer_hook_entry *prev, cplat_tracer *handle, cplat_trace_level level,
                           const cplat_timespec *timestamp, const char *message, void *context)
{
    HookRecord rec;
    rec.prev = prev;
    rec.handle = handle;
    rec.level = level;
    rec.padding = 0;
    rec.timestamp = *timestamp;
    if (message != NULL)
    {
        rec.message = message;
    }
    else
    {
        rec.message = "";
    }
    rec.context = context;
    g_hook_records.push_back(rec);
}

} // namespace

class traceHookTest : public Test
{
  protected:
    NiceMock<Mock_cplat> mock_cplat;
    cplat_trace_file_sink *file_handle_ =
        reinterpret_cast<cplat_trace_file_sink *>(static_cast<uintptr_t>(0x2200));

#if defined(PLATFORM_LINUX)
    cplat_syslog_sink *os_handle_ = reinterpret_cast<cplat_syslog_sink *>(static_cast<uintptr_t>(0x1100));
#elif defined(PLATFORM_WINDOWS)
    cplat_etw_provider *os_handle_ = reinterpret_cast<cplat_etw_provider *>(static_cast<uintptr_t>(0x1100));
#endif

    void SetUp() override
    {
        set_trace_sync_mock_defaults(mock_cplat);
        reset_hook_records();

        ON_CALL(mock_cplat, cplat_shutdown_register(_, _)).WillByDefault(Return(CPLAT_OK));
        ON_CALL(mock_cplat, cplat_clock_get_realtime_deadline_ms(_, _))
            .WillByDefault([](uint64_t, struct timespec *abs_timeout) { set_valid_deadline(abs_timeout); });
        ON_CALL(mock_cplat, cplat_clock_get_realtime(_)).WillByDefault([](cplat_timespec *ts) { set_fixed_realtime(ts); });
        ON_CALL(mock_cplat, cplat_clock_format_realtime_iso8601_local(_, _, _))
            .WillByDefault(
                [](char *buf, size_t buf_size, const cplat_timespec *)
                {
                    snprintf(buf, buf_size, "%s", "2026-04-26T03:04:05.678+09:00");
                    return 0;
                });
        ON_CALL(mock_cplat, cplat_trace_file_sink_create(_, _, _, _)).WillByDefault(Return(file_handle_));
        ON_CALL(mock_cplat, cplat_trace_file_sink_write(_, _, _, _)).WillByDefault(Return(CPLAT_OK));
        ON_CALL(mock_cplat, cplat_trace_file_sink_dispose(_)).WillByDefault(Return());

#if defined(PLATFORM_LINUX)
        ON_CALL(mock_cplat, cplat_syslog_sink_create(_, _)).WillByDefault(Return(os_handle_));
        ON_CALL(mock_cplat, cplat_syslog_sink_write(_, _, _, _)).WillByDefault(Return(CPLAT_OK));
        ON_CALL(mock_cplat, cplat_syslog_sink_rename(_, _)).WillByDefault(Return(CPLAT_OK));
        ON_CALL(mock_cplat, cplat_syslog_sink_dispose(_)).WillByDefault(Return());
#elif defined(PLATFORM_WINDOWS)
        ON_CALL(mock_cplat, cplat_etw_provider_create(_)).WillByDefault(Return(os_handle_));
        ON_CALL(mock_cplat, cplat_etw_provider_write(_, _, _, _)).WillByDefault(Return(CPLAT_OK));
        ON_CALL(mock_cplat, cplat_etw_provider_dispose(_)).WillByDefault(Return());
#endif
    }

    cplat_tracer *create_tracer()
    {
        cplat_tracer *handle = cplat_tracer_create(CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED);
        EXPECT_NE((cplat_tracer *)NULL, handle);
        // [状態確認] - `(cplat_tracer *)NULL` と `handle` が異なること。
        return handle;
    }
};

// フックが未設定のとき set_hook が有効なエントリを返すことの確認
TEST_F(traceHookTest, test_set_hook_returns_non_null)
{
    // Arrange
    cplat_tracer *tracer = create_tracer(); // [状態] - 生成済みの tracer を用意する。

    // Pre-Assert

    // Act
    cplat_tracer_hook_entry *entry = cplat_tracer_set_hook(
        tracer, recording_hook, nullptr); // [手順] - cplat_tracer_set_hook でフックを登録する。

    // Assert
    EXPECT_NE((cplat_tracer_hook_entry *)NULL, entry); // [確認_正常系] - エントリが NULL でないこと。

    // Cleanup
    cplat_tracer_remove_hook(tracer, entry);
    cplat_tracer_dispose(&tracer);
}

// handle が NULL のとき set_hook が NULL を返すことの確認
TEST_F(traceHookTest, test_set_hook_null_handle_returns_null)
{
    // Arrange

    // Pre-Assert

    // Act
    cplat_tracer_hook_entry *entry = cplat_tracer_set_hook(
        nullptr, recording_hook, nullptr); // [手順] - handle に NULL を渡して cplat_tracer_set_hook を呼び出す。

    // Assert
    EXPECT_EQ((cplat_tracer_hook_entry *)NULL,
              entry); // [確認_異常系] - cplat_tracer_set_hook の戻り値が NULL であること。
}

// fn が NULL のとき set_hook が NULL を返すことの確認
TEST_F(traceHookTest, test_set_hook_null_fn_returns_null)
{
    // Arrange
    cplat_tracer *tracer = create_tracer(); // [状態] - 生成済みの tracer を用意する。

    // Pre-Assert

    // Act
    cplat_tracer_hook_entry *entry = cplat_tracer_set_hook(
        tracer, nullptr, nullptr); // [手順] - fn に NULL を渡して cplat_tracer_set_hook を呼び出す。

    // Assert
    EXPECT_EQ((cplat_tracer_hook_entry *)NULL,
              entry); // [確認_異常系] - cplat_tracer_set_hook の戻り値が NULL であること。

    // Cleanup
    cplat_tracer_dispose(&tracer);
}

// started 状態では set_hook が NULL を返すことの確認
TEST_F(traceHookTest, test_set_hook_while_started_returns_null)
{
    // Arrange
    cplat_tracer *tracer = create_tracer();
    cplat_tracer_start(tracer); // [状態] - started 状態の tracer を用意する。

    // Pre-Assert

    // Act
    cplat_tracer_hook_entry *entry = cplat_tracer_set_hook(
        tracer, recording_hook, nullptr); // [手順] - started 状態で cplat_tracer_set_hook を呼び出す。

    // Assert
    EXPECT_EQ((cplat_tracer_hook_entry *)NULL,
              entry); // [確認_異常系] - cplat_tracer_set_hook の戻り値が NULL であること。

    // Cleanup
    cplat_tracer_stop(tracer);
    cplat_tracer_dispose(&tracer);
}

// フックを登録してから write するとコールバックが呼ばれることの確認
TEST_F(traceHookTest, test_hook_is_called_on_write)
{
    // Arrange
    cplat_tracer *tracer = create_tracer();
    cplat_tracer_hook_entry *entry = cplat_tracer_set_hook(
        tracer, recording_hook,
        reinterpret_cast<void *>(0xABCD)); // [状態] - context 0xABCD 付きで記録用フックを登録する。
    ASSERT_NE((cplat_tracer_hook_entry *)NULL, entry); // [状態確認] - フック エントリが非 NULL であること。
    cplat_tracer_start(tracer); // [状態] - tracer を started 状態とする。

    cplat_timespec ts = make_fixed_timestamp(); // [状態] - 固定タイムスタンプを用意する。

    // Pre-Assert

    // Act
    int rc = cplat_tracer_write_at(tracer, CPLAT_TRACE_LEVEL_INFO, &ts,
                                    "hello hook"); // [手順] - INFO レベルで "hello hook" を書き込む。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              rc);                        // [確認_正常系] - cplat_tracer_write_at の戻り値が CPLAT_OK であること。
    ASSERT_EQ(1u, g_hook_records.size()); // [確認_正常系] - フックが 1 回呼ばれること。
    EXPECT_EQ(tracer, g_hook_records[0].handle); // [確認_正常系] - フックに tracer の handle が渡ること。
    EXPECT_EQ(CPLAT_TRACE_LEVEL_INFO, g_hook_records[0].level); // [確認_正常系] - フックに INFO レベルが渡ること。
    EXPECT_EQ("hello hook", g_hook_records[0].message); // [確認_正常系] - フックに message "hello hook" が渡ること。
    EXPECT_EQ(reinterpret_cast<void *>(0xABCD),
              g_hook_records[0].context);                     // [確認_正常系] - フックに context 0xABCD が渡ること。
    EXPECT_EQ(ts.tv_sec, g_hook_records[0].timestamp.tv_sec); // [確認_正常系] - タイムスタンプの秒が一致すること。
    EXPECT_EQ(ts.tv_nsec,
              g_hook_records[0].timestamp.tv_nsec); // [確認_正常系] - タイムスタンプのナノ秒が一致すること。

    // Cleanup
    cplat_tracer_stop(tracer);
    cplat_tracer_remove_hook(tracer, entry);
    cplat_tracer_dispose(&tracer);
}

// CPLAT_TRACE_LEVEL_NONE で要求した場合もフックが呼ばれることの確認
TEST_F(traceHookTest, test_hook_is_called_for_none_level)
{
    // Arrange
    cplat_tracer *tracer = create_tracer();
    cplat_tracer_hook_entry *entry =
        cplat_tracer_set_hook(tracer, recording_hook, nullptr); // [状態] - 記録用フックを登録する。
    ASSERT_NE((cplat_tracer_hook_entry *)NULL, entry); // [状態確認] - フック エントリが非 NULL であること。
    cplat_tracer_start(tracer); // [状態] - tracer を started 状態とする。

    cplat_timespec ts = make_fixed_timestamp();

    // Pre-Assert

    // Act
    cplat_tracer_write_at(tracer, CPLAT_TRACE_LEVEL_NONE, &ts,
                           "none level message"); // [手順] - NONE レベルで "none level message" を書き込む。

    // Assert
    ASSERT_EQ(1u, g_hook_records.size());                          // [確認_正常系] - フックが 1 回呼ばれること。
    EXPECT_EQ(CPLAT_TRACE_LEVEL_NONE, g_hook_records[0].level); // [確認_正常系] - フックに NONE レベルが渡ること。
    EXPECT_EQ("none level message", g_hook_records[0].message);    // [確認_正常系] - フックに message が渡ること。

    // Cleanup
    cplat_tracer_stop(tracer);
    cplat_tracer_remove_hook(tracer, entry);
    cplat_tracer_dispose(&tracer);
}

// フックが設定されていない場合に write が通常通り成功することの確認 (性能パス)
TEST_F(traceHookTest, test_no_hook_write_succeeds)
{
    // Arrange
    cplat_tracer *tracer = create_tracer();
    cplat_tracer_start(tracer); // [状態] - フック未登録のまま started 状態の tracer を用意する。

    cplat_timespec ts = make_fixed_timestamp();

    // Pre-Assert

    // Act
    int rc = cplat_tracer_write_at(tracer, CPLAT_TRACE_LEVEL_INFO, &ts,
                                    "no hook"); // [手順] - INFO レベルで "no hook" を書き込む。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              rc);                        // [確認_正常系] - cplat_tracer_write_at の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0u, g_hook_records.size()); // [確認_正常系] - フックが呼ばれないこと。

    // Cleanup
    cplat_tracer_stop(tracer);
    cplat_tracer_dispose(&tracer);
}

// remove_hook 後はコールバックが呼ばれないことの確認
TEST_F(traceHookTest, test_hook_not_called_after_remove)
{
    // Arrange
    cplat_tracer *tracer = create_tracer();
    cplat_tracer_hook_entry *entry = cplat_tracer_set_hook(tracer, recording_hook, nullptr); // [状態] - 記録用フックを登録する。
    ASSERT_NE((cplat_tracer_hook_entry *)NULL, entry); // [状態確認] - フック エントリが非 NULL であること。

    cplat_tracer_remove_hook(tracer, entry); // [状態] - 登録済みフックを解除した状態とする。

    cplat_tracer_start(tracer);
    cplat_timespec ts = make_fixed_timestamp();

    // Pre-Assert

    // Act
    cplat_tracer_write_at(tracer, CPLAT_TRACE_LEVEL_INFO, &ts,
                           "after remove"); // [手順] - INFO レベルで "after remove" を書き込む。

    // Assert
    EXPECT_EQ(0u, g_hook_records.size()); // [確認_正常系] - 解除済みフックが呼ばれないこと。

    // Cleanup
    cplat_tracer_stop(tracer);
    cplat_tracer_dispose(&tracer);
}

// 複数フックのチェーンで最後に登録したものから順に呼ばれることの確認
TEST_F(traceHookTest, test_hook_chain_order)
{
    // Arrange
    static std::vector<int> call_order;
    call_order.clear();

    struct HookCtx
    {
        int id;
    };
    static HookCtx ctx1 = {1};
    static HookCtx ctx2 = {2};

    auto chain_fn = [](cplat_tracer_hook_entry *prev, cplat_tracer *handle, cplat_trace_level level,
                       const cplat_timespec *timestamp, const char *message, void *context)
    {
        HookCtx *c = reinterpret_cast<HookCtx *>(context);
        call_order.push_back(c->id);
        cplat_tracer_call_next_hook(prev, handle, level, timestamp, message);
    }; // [状態] - 呼び出し順を記録して次のフックへ委譲するチェーン フックを用意する。

    cplat_tracer *tracer = create_tracer();
    cplat_tracer_hook_entry *e1 =
        cplat_tracer_set_hook(tracer, chain_fn, &ctx1); // [状態] - id=1 のフックを先に登録する。
    cplat_tracer_hook_entry *e2 =
        cplat_tracer_set_hook(tracer, chain_fn, &ctx2); // [状態] - id=2 のフックを後から登録する。
    ASSERT_NE((cplat_tracer_hook_entry *)NULL, e1); // [状態確認] - id=1 のフック エントリが非 NULL であること。
    ASSERT_NE((cplat_tracer_hook_entry *)NULL, e2); // [状態確認] - id=2 のフック エントリが非 NULL であること。

    cplat_tracer_start(tracer);
    cplat_timespec ts = make_fixed_timestamp();

    // Pre-Assert

    // Act
    cplat_tracer_write_at(tracer, CPLAT_TRACE_LEVEL_INFO, &ts,
                           "chain test"); // [手順] - INFO レベルで "chain test" を書き込む。

    // Assert
    ASSERT_EQ(2u, call_order.size()); // [確認_正常系] - 2 つのフックが両方呼ばれること。
    EXPECT_EQ(2, call_order[0]);      // [確認_正常系] - 最後に登録した id=2 が最初に呼ばれること。
    EXPECT_EQ(1, call_order[1]);      // [確認_正常系] - 次に id=1 が呼ばれること。

    // Cleanup
    cplat_tracer_stop(tracer);
    cplat_tracer_remove_hook(tracer, e2);
    cplat_tracer_remove_hook(tracer, e1);
    cplat_tracer_dispose(&tracer);
}

// call_next_hook に NULL を渡しても何も起きないことの確認
TEST_F(traceHookTest, test_call_next_hook_null_prev)
{
    // Arrange
    cplat_tracer *tracer = create_tracer();
    cplat_tracer_start(tracer); // [状態] - started 状態の tracer を用意する。

    cplat_timespec ts = make_fixed_timestamp();

    // Pre-Assert

    // Act
    // Assert
    EXPECT_NO_FATAL_FAILURE(cplat_tracer_call_next_hook(
        nullptr, tracer, CPLAT_TRACE_LEVEL_INFO, &ts,
        "test")); // [手順] - prev に NULL を渡して cplat_tracer_call_next_hook を呼び出す。
                  // [確認_正常系] - 致命的失敗なく完了すること。

    // Cleanup
    cplat_tracer_stop(tracer);
    cplat_tracer_dispose(&tracer);
}

// writef 経由でもフックが呼ばれることの確認
TEST_F(traceHookTest, test_hook_called_via_writef)
{
    // Arrange
    cplat_tracer *tracer = create_tracer();
    cplat_tracer_hook_entry *entry =
        cplat_tracer_set_hook(tracer, recording_hook, nullptr); // [状態] - 記録用フックを登録する。
    ASSERT_NE((cplat_tracer_hook_entry *)NULL, entry); // [状態確認] - フック エントリが非 NULL であること。
    cplat_tracer_start(tracer); // [状態] - tracer を started 状態とする。

    cplat_timespec ts = make_fixed_timestamp();

    // Pre-Assert

    // Act
    cplat_tracer_writef_at(tracer, CPLAT_TRACE_LEVEL_WARNING, &ts, "fmt %d",
                            42); // [手順] - WARNING レベルでフォーマット "fmt %d" と引数 42 を書き込む。

    // Assert
    ASSERT_EQ(1u, g_hook_records.size()); // [確認_正常系] - フックが 1 回呼ばれること。
    EXPECT_EQ(CPLAT_TRACE_LEVEL_WARNING,
              g_hook_records[0].level); // [確認_正常系] - フックに WARNING レベルが渡ること。
    /* cplat_tracer_writef_at はフォーマット展開後の文字列が渡される */
    EXPECT_NE(std::string::npos,
              g_hook_records[0].message.find("42")); // [確認_正常系] - フックに展開後の "42" を含む文字列が渡ること。

    // Cleanup
    cplat_tracer_stop(tracer);
    cplat_tracer_remove_hook(tracer, entry);
    cplat_tracer_dispose(&tracer);
}

// タイムスタンプが NULL でも解決済みタイムスタンプがフックに渡ることの確認
TEST_F(traceHookTest, test_hook_receives_resolved_timestamp)
{
    // Arrange
    cplat_tracer *tracer = create_tracer();
    cplat_tracer_hook_entry *entry =
        cplat_tracer_set_hook(tracer, recording_hook, nullptr); // [状態] - 記録用フックを登録する。
    ASSERT_NE((cplat_tracer_hook_entry *)NULL, entry); // [状態確認] - フック エントリが非 NULL であること。
    cplat_tracer_start(tracer); // [状態] - tracer を started 状態とする。

    // Pre-Assert
    // [Pre-Assert手順] - cplat_clock_get_realtime は SetUp のモックで固定値 {1714100645, 678000000} を返却する。

    // Act
    cplat_tracer_write_at(
        tracer, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "ts resolve test"); // [手順] - timestamp に NULL を渡して書き込み、内部での現在時刻取得を促す。

    // Assert
    ASSERT_EQ(1u, g_hook_records.size()); // [確認_正常系] - フックが 1 回呼ばれること。
    EXPECT_EQ(1714100645LL,
              g_hook_records[0].timestamp.tv_sec); // [確認_正常系] - モックで固定した秒 1714100645 が渡ること。
    EXPECT_EQ(678000000,
              g_hook_records[0].timestamp.tv_nsec); // [確認_正常系] - モックで固定したナノ秒 678000000 が渡ること。

    // Cleanup
    cplat_tracer_stop(tracer);
    cplat_tracer_remove_hook(tracer, entry);
    cplat_tracer_dispose(&tracer);
}

// started 状態では remove_hook が何もしないことの確認
TEST_F(traceHookTest, test_remove_hook_while_started_does_nothing)
{
    // Arrange
    cplat_tracer *tracer = create_tracer();
    cplat_tracer_hook_entry *entry =
        cplat_tracer_set_hook(tracer, recording_hook, nullptr); // [状態] - 記録用フックを登録する。
    ASSERT_NE((cplat_tracer_hook_entry *)NULL, entry); // [状態確認] - フック エントリが非 NULL であること。
    cplat_tracer_start(tracer); // [状態] - tracer を started 状態とする。

    // Pre-Assert

    // Act
    cplat_tracer_remove_hook(tracer, entry); // [手順] - started 状態のまま cplat_tracer_remove_hook を呼び出す。

    cplat_timespec ts = make_fixed_timestamp();
    cplat_tracer_write_at(tracer, CPLAT_TRACE_LEVEL_INFO, &ts,
                           "hook still active"); // [手順] - INFO レベルで "hook still active" を書き込む。

    // Assert
    EXPECT_EQ(1u, g_hook_records.size()); // [確認_正常系] - フックが解除されず 1 回呼ばれること。

    // Cleanup
    cplat_tracer_stop(tracer);
    cplat_tracer_remove_hook(tracer, entry);
    cplat_tracer_dispose(&tracer);
}
