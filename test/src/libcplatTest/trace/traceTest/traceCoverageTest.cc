#include <testfw.h>
#include <mock_cplat.h>
#include <cplat/runtime/shutdown.h>
#include <cplat/trace/tracer.h>
#include <cplat/trace/tracer_internal.h>
#include <cstring>
#include <string>
#include <vector>

#include "tracer.inject.h"
#include "traceSyncMock.h"

#if defined(PLATFORM_LINUX)
    #include <syslog.h>
#endif /* PLATFORM_LINUX */

extern void (*g_test_file_shutdown_hook)(void);

using testing::_;
using testing::DoDefault;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;

namespace
{
const int kLifecycleDisposing = 1;
const int kLifecycleDisposed = 2;

void coverage_hook(cplat_tracer_hook_entry *prev, cplat_tracer *handle, cplat_trace_level level,
                   const cplat_timespec *timestamp, const char *message, void *context)
{
    (void)prev;
    (void)handle;
    (void)level;
    (void)timestamp;
    (void)message;
    (void)context;
}
} // namespace

class traceCoverageTest : public Test
{
  protected:
    NiceMock<Mock_cplat> mock_cplat;
    cplat_trace_file_sink *file_handle_ = reinterpret_cast<cplat_trace_file_sink *>(static_cast<uintptr_t>(0x2200));
#if defined(PLATFORM_LINUX)
    cplat_syslog_sink *os_handle_ = reinterpret_cast<cplat_syslog_sink *>(static_cast<uintptr_t>(0x1100));
#elif defined(PLATFORM_WINDOWS)
    cplat_etw_provider *os_handle_ = reinterpret_cast<cplat_etw_provider *>(static_cast<uintptr_t>(0x1100));
    cplat_eventlog_sink *eventlog_handle_ = reinterpret_cast<cplat_eventlog_sink *>(static_cast<uintptr_t>(0x1300));
#endif /* PLATFORM_ */

    // [サブ手順 名前=traceCoverageTest.SetUp]
    void SetUp() override
    {
        set_trace_sync_mock_defaults(mock_cplat);
        test_trace_registry_reset_shutdown_state();
        ON_CALL(mock_cplat, cplat_shutdown_register(_, _)).WillByDefault(Return(CPLAT_OK));
        // [状態] - cplat_shutdown_register() から CPLAT_OK を返すように既定動作を設定する。
        ON_CALL(mock_cplat, cplat_trace_file_sink_create(_, _, _, _)).WillByDefault(Return(file_handle_));
        // [状態] - cplat_trace_file_sink_create() から file_handle_ を返すように既定動作を設定する。
        ON_CALL(mock_cplat, cplat_trace_file_sink_write(_, _, _, _)).WillByDefault(Return(CPLAT_OK));
        // [状態] - cplat_trace_file_sink_write() から CPLAT_OK を返すように既定動作を設定する。
        ON_CALL(mock_cplat, cplat_trace_file_sink_dispose(_)).WillByDefault(Return());
        // [状態] - cplat_trace_file_sink_dispose() から戻るように既定動作を設定する。
        ON_CALL(mock_cplat, cplat_process_get_executable_path(_, _))
            .WillByDefault(
                [](char *path_out, size_t path_size)
                {
                    snprintf(path_out, path_size, "%s", "/opt/bin/myapp");
                    return CPLAT_OK;
                });
        // [状態] - cplat_process_get_executable_path() から、"/opt/bin/myapp" を格納して CPLAT_OK を返すように既定動作を設定する。
#if defined(PLATFORM_LINUX)
        ON_CALL(mock_cplat, cplat_syslog_sink_create(_, _)).WillByDefault(Return(os_handle_));
        // [状態] - cplat_syslog_sink_create() から os_handle_ を返すように既定動作を設定する。
        ON_CALL(mock_cplat, cplat_syslog_sink_write(_, _, _, _)).WillByDefault(Return(CPLAT_OK));
        // [状態] - cplat_syslog_sink_write() から CPLAT_OK を返すように既定動作を設定する。
        ON_CALL(mock_cplat, cplat_syslog_sink_rename(_, _)).WillByDefault(Return(CPLAT_OK));
        // [状態] - cplat_syslog_sink_rename() から CPLAT_OK を返すように既定動作を設定する。
        ON_CALL(mock_cplat, cplat_syslog_sink_dispose(_)).WillByDefault(Return());
        // [状態] - cplat_syslog_sink_dispose() から戻るように既定動作を設定する。
#elif defined(PLATFORM_WINDOWS)
        ON_CALL(mock_cplat, cplat_etw_provider_create(_)).WillByDefault(Return(os_handle_));
        // [状態] - cplat_etw_provider_create() から os_handle_ を返すように既定動作を設定する。
        ON_CALL(mock_cplat, cplat_etw_provider_write(_, _, _, _)).WillByDefault(Return(CPLAT_OK));
        // [状態] - cplat_etw_provider_write() から CPLAT_OK を返すように既定動作を設定する。
        ON_CALL(mock_cplat, cplat_etw_provider_dispose(_)).WillByDefault(Return());
        // [状態] - cplat_etw_provider_dispose() から戻るように既定動作を設定する。
        ON_CALL(mock_cplat, cplat_eventlog_sink_create(_)).WillByDefault(Return(eventlog_handle_));
        // [状態] - cplat_eventlog_sink_create() から eventlog_handle_ を返すように既定動作を設定する。
        ON_CALL(mock_cplat, cplat_eventlog_sink_write(_, _, _, _, _, _)).WillByDefault(Return(CPLAT_OK));
        // [状態] - cplat_eventlog_sink_write() から CPLAT_OK を返すように既定動作を設定する。
        ON_CALL(mock_cplat, cplat_eventlog_sink_dispose(_)).WillByDefault(Return());
        // [状態] - cplat_eventlog_sink_dispose() から戻るように既定動作を設定する。
#endif /* PLATFORM_ */
    }
    // [サブ手順終了]

    // [サブ手順 名前=traceCoverageTest.TearDown]
    void TearDown() override
    {
        test_trace_registry_reset_shutdown_state();
    }
    // [サブ手順終了]
};

#if defined(PLATFORM_LINUX)

// syslog レベル変換が各トレース レベルと default を返すことの確認
// [サブ手順参照 名前=traceCoverageTest.SetUp]
TEST_F(traceCoverageTest, to_syslog_level_covers_all_cases)
{
    // Arrange
    cplat_trace_level invalid_level = CPLAT_TRACE_LEVEL_NONE;
    memset(&invalid_level, 0x7F, sizeof(invalid_level));

    // Pre-Assert

    // Act
    int critical_level = test_tracer_to_syslog_level(
        CPLAT_TRACE_LEVEL_CRITICAL); // [手順] - CRITICAL を指定して test_tracer_to_syslog_level() を呼び出す。
    int error_level = test_tracer_to_syslog_level(
        CPLAT_TRACE_LEVEL_ERROR); // [手順] - ERROR を指定して test_tracer_to_syslog_level() を呼び出す。
    int warning_level = test_tracer_to_syslog_level(
        CPLAT_TRACE_LEVEL_WARNING); // [手順] - WARNING を指定して test_tracer_to_syslog_level() を呼び出す。
    int info_level = test_tracer_to_syslog_level(
        CPLAT_TRACE_LEVEL_INFO); // [手順] - INFO を指定して test_tracer_to_syslog_level() を呼び出す。
    int verbose_level = test_tracer_to_syslog_level(
        CPLAT_TRACE_LEVEL_VERBOSE); // [手順] - VERBOSE を指定して test_tracer_to_syslog_level() を呼び出す。
    int debug_level = test_tracer_to_syslog_level(
        CPLAT_TRACE_LEVEL_DEBUG); // [手順] - DEBUG を指定して test_tracer_to_syslog_level() を呼び出す。
    int none_level = test_tracer_to_syslog_level(
        CPLAT_TRACE_LEVEL_NONE); // [手順] - NONE を指定して test_tracer_to_syslog_level() を呼び出す。
    int default_level = test_tracer_to_syslog_level(
        invalid_level); // [手順] - 未定義レベルを指定して test_tracer_to_syslog_level() を呼び出す。

    // Assert
    EXPECT_EQ(
        LOG_CRIT,
        critical_level); // [確認_正常系] - CRITICAL を変換した test_tracer_to_syslog_level() の戻り値が LOG_CRIT であること。
    EXPECT_EQ(
        LOG_ERR,
        error_level); // [確認_正常系] - ERROR を変換した test_tracer_to_syslog_level() の戻り値が LOG_ERR であること。
    EXPECT_EQ(
        LOG_WARNING,
        warning_level); // [確認_正常系] - WARNING を変換した test_tracer_to_syslog_level() の戻り値が LOG_WARNING であること。
    EXPECT_EQ(
        LOG_INFO,
        info_level); // [確認_正常系] - INFO を変換した test_tracer_to_syslog_level() の戻り値が LOG_INFO であること。
    EXPECT_EQ(
        LOG_DEBUG,
        verbose_level); // [確認_正常系] - VERBOSE を変換した test_tracer_to_syslog_level() の戻り値が LOG_DEBUG であること。
    EXPECT_EQ(
        LOG_DEBUG,
        debug_level); // [確認_正常系] - DEBUG を変換した test_tracer_to_syslog_level() の戻り値が LOG_DEBUG であること。
    EXPECT_EQ(
        LOG_DEBUG,
        none_level); // [確認_正常系] - NONE を変換した test_tracer_to_syslog_level() の戻り値が LOG_DEBUG であること。
    EXPECT_EQ(
        LOG_DEBUG,
        default_level); // [確認_正常系] - 未定義レベルを変換した test_tracer_to_syslog_level() の戻り値が LOG_DEBUG であること。
}
// [サブ手順参照 名前=traceCoverageTest.TearDown]

// 強制出力のレベルが、対応する重大度へ変換されることの確認
// [サブ手順参照 名前=traceCoverageTest.SetUp]
TEST_F(traceCoverageTest, to_syslog_level_maps_force_levels)
{
    // Arrange

    // Pre-Assert

    // Act
    int critical_level = test_tracer_to_syslog_level(
        CPLAT_TRACE_LEVEL_FORCE_CRITICAL); // [手順] - FORCE_CRITICAL を指定して test_tracer_to_syslog_level() を呼び出す。
    int error_level = test_tracer_to_syslog_level(
        CPLAT_TRACE_LEVEL_FORCE_ERROR); // [手順] - FORCE_ERROR を指定して test_tracer_to_syslog_level() を呼び出す。
    int warning_level = test_tracer_to_syslog_level(
        CPLAT_TRACE_LEVEL_FORCE_WARNING); // [手順] - FORCE_WARNING を指定して test_tracer_to_syslog_level() を呼び出す。
    int info_level = test_tracer_to_syslog_level(
        CPLAT_TRACE_LEVEL_FORCE_INFO); // [手順] - FORCE_INFO を指定して test_tracer_to_syslog_level() を呼び出す。
    int verbose_level = test_tracer_to_syslog_level(
        CPLAT_TRACE_LEVEL_FORCE_VERBOSE); // [手順] - FORCE_VERBOSE を指定して test_tracer_to_syslog_level() を呼び出す。
    int debug_level = test_tracer_to_syslog_level(
        CPLAT_TRACE_LEVEL_FORCE_DEBUG); // [手順] - FORCE_DEBUG を指定して test_tracer_to_syslog_level() を呼び出す。
    int none_level = test_tracer_to_syslog_level(
        CPLAT_TRACE_LEVEL_FORCE_NONE); // [手順] - FORCE_NONE を指定して test_tracer_to_syslog_level() を呼び出す。

    // Assert
    EXPECT_EQ(
        LOG_CRIT,
        critical_level); // [確認_正常系] - FORCE_CRITICAL を変換した test_tracer_to_syslog_level() の戻り値が LOG_CRIT であること。
    EXPECT_EQ(
        LOG_ERR,
        error_level); // [確認_正常系] - FORCE_ERROR を変換した test_tracer_to_syslog_level() の戻り値が LOG_ERR であること。
    EXPECT_EQ(
        LOG_WARNING,
        warning_level); // [確認_正常系] - FORCE_WARNING を変換した test_tracer_to_syslog_level() の戻り値が LOG_WARNING であること。
    EXPECT_EQ(
        LOG_INFO,
        info_level); // [確認_正常系] - FORCE_INFO を変換した test_tracer_to_syslog_level() の戻り値が LOG_INFO であること。
    EXPECT_EQ(
        LOG_INFO,
        verbose_level); // [確認_正常系] - FORCE_VERBOSE を変換した test_tracer_to_syslog_level() の戻り値が LOG_INFO であること。(FORCE_VERBOSE が常時記録の範囲であることの確認)
    EXPECT_EQ(
        LOG_INFO,
        debug_level); // [確認_正常系] - FORCE_DEBUG を変換した test_tracer_to_syslog_level() の戻り値が LOG_INFO であること。(FORCE_DEBUG が常時記録の範囲であることの確認)
    EXPECT_EQ(
        LOG_INFO,
        none_level); // [確認_正常系] - FORCE_NONE を変換した test_tracer_to_syslog_level() の戻り値が LOG_INFO であること。
}
// [サブ手順参照 名前=traceCoverageTest.TearDown]

// 強制出力と通常のレベルの相互変換が対応していることの確認
// [サブ手順参照 名前=traceCoverageTest.SetUp]
TEST_F(traceCoverageTest, force_level_conversion_macros_are_symmetric)
{
    // Arrange

    // Pre-Assert

    // Act
    cplat_trace_level forced = CPLAT_TRACE_LEVEL_TO_FORCE(
        CPLAT_TRACE_LEVEL_WARNING); // [手順] - WARNING を指定して CPLAT_TRACE_LEVEL_TO_FORCE() を呼び出す。
    cplat_trace_level restored = CPLAT_TRACE_LEVEL_FROM_FORCE(
        forced); // [手順] - 強制出力のレベルを指定して CPLAT_TRACE_LEVEL_FROM_FORCE() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_TRACE_LEVEL_FORCE_WARNING,
        forced); // [確認_正常系] - WARNING を変換した CPLAT_TRACE_LEVEL_TO_FORCE() の戻り値が CPLAT_TRACE_LEVEL_FORCE_WARNING であること。
    EXPECT_EQ(
        CPLAT_TRACE_LEVEL_WARNING,
        restored); // [確認_正常系] - 強制出力を通常へ戻した CPLAT_TRACE_LEVEL_FROM_FORCE() の戻り値が CPLAT_TRACE_LEVEL_WARNING であること。
    EXPECT_NE(
        0,
        CPLAT_TRACE_LEVEL_IS_FORCE(
            forced)); // [確認_正常系] - forced に対する CPLAT_TRACE_LEVEL_IS_FORCE() の戻り値が 0 以外であること。(強制出力と判定できることの確認)
    EXPECT_EQ(
        0,
        CPLAT_TRACE_LEVEL_IS_FORCE(
            restored)); // [確認_正常系] - restored に対する CPLAT_TRACE_LEVEL_IS_FORCE() の戻り値が 0 であること。(通常のレベルは強制出力と判定しないことの確認)
}
// [サブ手順参照 名前=traceCoverageTest.TearDown]

// syslog sink 生成失敗と rwlock 生成失敗で create が NULL を返すことの確認
// [サブ手順参照 名前=traceCoverageTest.SetUp]
TEST_F(traceCoverageTest, create_fails_when_syslog_or_rwlock_setup_fails)
{
    // Arrange
    cplat_tracer *syslog_failure = nullptr;
    cplat_tracer *rwlock_failure = nullptr;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_syslog_sink_create(_, _))
        .WillOnce(Return(nullptr))
        .WillRepeatedly(
            Return(os_handle_)); // [Pre-Assert確認_異常系] - cplat_syslog_sink_create() が 1 回以上呼び出されること。
    // [Pre-Assert手順] - cplat_syslog_sink_create() から 1 回目は nullptr、以降は os_handle_ を返す。
    EXPECT_CALL(mock_cplat, cplat_local_rwlock_create(_))
        .WillOnce(Return(CPLAT_ERR_UNKNOWN))
        .WillRepeatedly(
            DoDefault()); // [Pre-Assert確認_異常系] - cplat_local_rwlock_create() が 1 回以上呼び出されること。
    // [Pre-Assert手順] - cplat_local_rwlock_create() から 1 回目は CPLAT_ERR_UNKNOWN を返し、以降は既定動作へ委譲する。

    // Act
    syslog_failure = cplat_tracer_create(
        CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED); // [手順] - syslog sink の生成が失敗する状態で cplat_tracer_create() を呼び出す。
    rwlock_failure = cplat_tracer_create(
        CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED); // [手順] - rwlock の生成が失敗する状態で cplat_tracer_create() を呼び出す。

    // Assert
    EXPECT_EQ(
        nullptr,
        syslog_failure); // [確認_異常系] - syslog sink 生成失敗時の cplat_tracer_create() の戻り値が nullptr であること。
    EXPECT_EQ(
        nullptr,
        rwlock_failure); // [確認_異常系] - rwlock 生成失敗時の cplat_tracer_create() の戻り値が nullptr であること。
}
// [サブ手順参照 名前=traceCoverageTest.TearDown]

#endif /* PLATFORM_LINUX */

// シャットダウン中の生成拒否と非アクティブ dispose を処理することの確認
// [サブ手順参照 名前=traceCoverageTest.SetUp]
TEST_F(traceCoverageTest, shutdown_and_inactive_dispose_paths)
{
    // Arrange
    cplat_tracer *rejected = nullptr;
    cplat_tracer *handle = cplat_tracer_create(
        CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED); // [状態] - 生成済みのトレース ハンドルを用意する。
    ASSERT_NE(nullptr, handle); // [状態確認] - cplat_tracer_create() の戻り値が nullptr 以外であること。
    int first_dispose = 0;
    int second_begin = 0;

    // Pre-Assert

    // Act
    test_trace_registry_set_shutdown_started(1U);
    rejected = cplat_tracer_create(
        CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED); // [手順] - シャットダウン開始後の状態で cplat_tracer_create() を呼び出す。
    test_trace_registry_reset_shutdown_state();
    first_dispose = test_tracer_begin_dispose(
        handle); // [手順] - アクティブ ハンドルを指定して test_tracer_begin_dispose() を呼び出す。
    second_begin = test_tracer_begin_dispose(
        handle);                   // [手順] - 非アクティブ ハンドルを指定して test_tracer_begin_dispose() を呼び出す。
    cplat_tracer_dispose(nullptr); // [手順] - nullptr を指定して cplat_tracer_dispose() を呼び出す。
    cplat_tracer_dispose(&handle); // [手順] - 解放開始済みハンドルを指定して cplat_tracer_dispose() を呼び出す。
    int null_active = test_tracer_handle_is_active(
        nullptr); // [手順] - nullptr ハンドルを指定して test_tracer_handle_is_active() を呼び出す。
    int null_begin = test_tracer_begin_dispose(
        nullptr); // [手順] - nullptr ハンドルを指定して test_tracer_begin_dispose() を呼び出す。

    // Assert
    EXPECT_EQ(nullptr,
              rejected); // [確認_異常系] - シャットダウン開始後の cplat_tracer_create() の戻り値が nullptr であること。
    EXPECT_EQ(
        0,
        first_dispose); // [確認_正常系] - 初回の test_tracer_begin_dispose() の戻り値が 0 であること。(アクティブ ハンドルの解放を開始できたことの確認)
    EXPECT_EQ(
        -1,
        second_begin); // [確認_異常系] - 非アクティブ ハンドルに対する test_tracer_begin_dispose() の戻り値が -1 であること。(解放の再開始が拒否されたことの確認)
    EXPECT_EQ(
        0,
        null_active); // [確認_異常系] - nullptr ハンドルに対する test_tracer_handle_is_active() の戻り値が 0 であること。(nullptr ハンドルが非アクティブであることの確認)
    EXPECT_EQ(
        -1,
        null_begin); // [確認_異常系] - nullptr ハンドルに対する test_tracer_begin_dispose() の戻り値が -1 であること。(nullptr ハンドルの解放開始が拒否されたことの確認)
}
// [サブ手順参照 名前=traceCoverageTest.TearDown]

// 共有ロック失敗とロック中のライフサイクル変化を処理することの確認
// [サブ手順参照 名前=traceCoverageTest.SetUp]
TEST_F(traceCoverageTest, enter_shared_fails_on_timeout_and_lifecycle_change)
{
    // Arrange
    cplat_tracer *handle = cplat_tracer_create(
        CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED); // [状態] - 生成済みのトレース ハンドルを用意する。
    ASSERT_NE(nullptr, handle); // [状態確認] - cplat_tracer_create() の戻り値が nullptr 以外であること。
    cplat_trace_level file_level = CPLAT_TRACE_LEVEL_DEBUG;
    cplat_trace_level stderr_level = CPLAT_TRACE_LEVEL_DEBUG;
    cplat_trace_level os_level = CPLAT_TRACE_LEVEL_DEBUG;
    cplat_tracer_state state = CPLAT_TRACER_STATE_STARTED;
    int start_result = CPLAT_OK;
    int write_result = CPLAT_OK;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_local_rwlock_lock_shared(_, _))
        .WillOnce(Return(CPLAT_ERR_TIMEOUT))
        .WillOnce(Invoke(
            [handle](cplat_local_rwlock *, int)
            {
                test_tracer_set_lifecycle_state(handle, kLifecycleDisposing);
                return CPLAT_OK;
            }))
        .WillOnce(Return(CPLAT_ERR_TIMEOUT))
        .WillRepeatedly(
            DoDefault()); // [Pre-Assert確認_異常系] - cplat_local_rwlock_lock_shared() が 3 回以上呼び出されること。
    // [Pre-Assert手順] - cplat_local_rwlock_lock_shared() から 1 回目と 3 回目は CPLAT_ERR_TIMEOUT を返し、2 回目は、ライフサイクルを DISPOSING へ変更して CPLAT_OK を返し、以降は既定動作へ委譲する。

    // Act
    file_level = cplat_tracer_get_file_level(
        handle); // [手順] - 共有ロックがタイムアウトする状態で cplat_tracer_get_file_level() を呼び出す。
    stderr_level = cplat_tracer_get_stderr_level(
        handle); // [手順] - ロック中に DISPOSING へ変わる状態で cplat_tracer_get_stderr_level() を呼び出す。
    test_tracer_set_lifecycle_state(handle, kLifecycleDisposed);
    start_result =
        cplat_tracer_start(handle); // [手順] - 非アクティブ ハンドルを指定して cplat_tracer_start() を呼び出す。
    test_tracer_set_lifecycle_state(handle, 0);
    test_tracer_set_running(handle, 1);
    write_result = cplat_tracer_write(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "msg"); // [手順] - 共有ロックの失敗が残っている状態で、INFO レベルと "msg" を指定して cplat_tracer_write() を呼び出す。
    os_level =
        cplat_tracer_get_os_level(handle);  // [手順] - ロック失敗後の状態で cplat_tracer_get_os_level() を呼び出す。
    state = cplat_tracer_get_state(handle); // [手順] - ロック失敗後の状態で cplat_tracer_get_state() を呼び出す。
    int stop_inactive =
        cplat_tracer_stop(handle); // [手順] - 非アクティブ化したハンドルを指定して cplat_tracer_stop() を呼び出す。
    (void)os_level;
    (void)state;
    (void)stop_inactive;

    // Assert
    EXPECT_EQ(
        CPLAT_TRACE_LEVEL_NONE,
        file_level); // [確認_異常系] - 共有ロック失敗時の cplat_tracer_get_file_level() の戻り値が CPLAT_TRACE_LEVEL_NONE であること。
    EXPECT_EQ(
        CPLAT_TRACE_LEVEL_NONE,
        stderr_level); // [確認_異常系] - DISPOSING 時の cplat_tracer_get_stderr_level() の戻り値が CPLAT_TRACE_LEVEL_NONE であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        start_result); // [確認_異常系] - 排他ロック失敗時の cplat_tracer_start() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        write_result); // [確認_異常系] - 共有ロック失敗時の cplat_tracer_write() の戻り値が CPLAT_ERR_UNKNOWN であること。

    // Cleanup
    test_tracer_set_lifecycle_state(handle, 0);
    cplat_tracer_dispose(&handle);
}
// [サブ手順参照 名前=traceCoverageTest.TearDown]

// 名前設定とファイル設定の失敗枝を処理することの確認
// [サブ手順参照 名前=traceCoverageTest.SetUp]
TEST_F(traceCoverageTest, setters_cover_invalid_and_allocation_failures)
{
    // Arrange
    cplat_tracer *handle = cplat_tracer_create(
        CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED); // [状態] - 生成済みのトレース ハンドルを用意する。
    ASSERT_NE(nullptr, handle); // [状態確認] - cplat_tracer_create() の戻り値が nullptr 以外であること。
    int negative_name = CPLAT_OK;
    int name_null = CPLAT_OK;
    int file_name_inactive = CPLAT_OK;
#if defined(PLATFORM_LINUX)
    int file_name_oom = CPLAT_OK;
    int file_level_oom = CPLAT_OK;
    int rename_failure = CPLAT_OK;
#endif /* PLATFORM_LINUX */
    int stderr_inactive = CPLAT_OK;
    int os_inactive = CPLAT_OK;

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_cplat, cplat_strdup(_))
        .WillOnce(Invoke(delegate_real_cplat_strdup))
        .WillOnce(Return(nullptr))
        .WillOnce(Return(nullptr))
        .WillRepeatedly(Invoke(
            delegate_real_cplat_strdup)); // [Pre-Assert確認_異常系] - cplat_strdup() が 3 回以上呼び出されること。
    // [Pre-Assert手順] - cplat_strdup() から 1 回目は製品 (prod) の処理へ委譲し、2 回目と 3 回目は nullptr を返し、以降は製品 (prod) の処理へ委譲する。
    EXPECT_CALL(mock_cplat, cplat_syslog_sink_rename(_, _))
        .WillOnce(Return(CPLAT_OK))
        .WillOnce(Return(-1))
        .WillRepeatedly(
            Return(CPLAT_OK)); // [Pre-Assert確認_異常系] - cplat_syslog_sink_rename() が 2 回以上呼び出されること。
    // [Pre-Assert手順] - cplat_syslog_sink_rename() から 1 回目は CPLAT_OK、2 回目は -1、以降は CPLAT_OK を返す。
#endif                                     /* PLATFORM_LINUX */

    // Act
    negative_name = cplat_tracer_set_name(
        handle, "n", -1); // [手順] - 負の identifier を指定して cplat_tracer_set_name() を呼び出す。
    name_null = cplat_tracer_set_name(
        handle, nullptr,
        0); // [手順] - name に nullptr、identifier に 0 を指定して cplat_tracer_set_name() を呼び出す。
    test_tracer_set_lifecycle_state(handle, kLifecycleDisposed);
    file_name_inactive = cplat_tracer_set_file_name(
        handle, "log", 0); // [手順] - 非アクティブ ハンドルを指定して cplat_tracer_set_file_name() を呼び出す。
    test_tracer_set_lifecycle_state(handle, 0);
#if defined(PLATFORM_LINUX)
    file_name_oom = cplat_tracer_set_file_name(
        handle, "log", 0); // [手順] - cplat_strdup() が失敗する状態で cplat_tracer_set_file_name() を呼び出す。
    file_level_oom = cplat_tracer_set_file_level(
        handle, "/tmp/a.log", CPLAT_TRACE_LEVEL_INFO, 0, 0,
        0); // [手順] - パスの複製が失敗する状態で cplat_tracer_set_file_level() を呼び出す。
    rename_failure = cplat_tracer_set_name(
        handle, "renamed",
        0); // [手順] - cplat_syslog_sink_rename() が失敗する状態で cplat_tracer_set_name() を呼び出す。
#endif                                                            /* PLATFORM_LINUX */
    test_tracer_set_lifecycle_state(handle, kLifecycleDisposed);
    stderr_inactive = cplat_tracer_set_stderr_level(
        handle,
        CPLAT_TRACE_LEVEL_ERROR); // [手順] - 非アクティブ ハンドルを指定して cplat_tracer_set_stderr_level() を呼び出す。
    os_inactive = cplat_tracer_set_os_level(
        handle,
        CPLAT_TRACE_LEVEL_ERROR); // [手順] - 非アクティブ ハンドルを指定して cplat_tracer_set_os_level() を呼び出す。
    cplat_tracer_hook_entry *hook_inactive = cplat_tracer_set_hook(
        handle, nullptr, nullptr); // [手順] - 非アクティブ ハンドルを指定して cplat_tracer_set_hook() を呼び出す。
    cplat_tracer_remove_hook(handle,
                             nullptr); // [手順] - hook に nullptr を指定して cplat_tracer_remove_hook() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        negative_name); // [確認_異常系] - 負の identifier での cplat_tracer_set_name() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_OK,
              name_null); // [確認_正常系] - name が nullptr の cplat_tracer_set_name() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        file_name_inactive); // [確認_異常系] - 非アクティブ ハンドルでの cplat_tracer_set_file_name() の戻り値が CPLAT_ERR_UNKNOWN であること。
#if defined(PLATFORM_LINUX)
    EXPECT_EQ(
        CPLAT_ERR_OUT_OF_MEMORY,
        file_name_oom); // [確認_異常系] - strdup 失敗時の cplat_tracer_set_file_name() の戻り値が CPLAT_ERR_OUT_OF_MEMORY であること。
    EXPECT_EQ(
        CPLAT_ERR_OUT_OF_MEMORY,
        file_level_oom); // [確認_異常系] - パス複製失敗時の cplat_tracer_set_file_level() の戻り値が CPLAT_ERR_OUT_OF_MEMORY であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        rename_failure); // [確認_異常系] - syslog rename 失敗時の cplat_tracer_set_name() の戻り値が CPLAT_ERR_UNKNOWN であること。
#endif                                            /* PLATFORM_LINUX */
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        stderr_inactive); // [確認_異常系] - 非アクティブ ハンドルでの cplat_tracer_set_stderr_level() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        os_inactive); // [確認_異常系] - 非アクティブ ハンドルでの cplat_tracer_set_os_level() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        nullptr,
        hook_inactive); // [確認_異常系] - 非アクティブ ハンドルでの cplat_tracer_set_hook() の戻り値が nullptr であること。

    // Cleanup
    test_tracer_set_lifecycle_state(handle, 0);
    cplat_tracer_dispose(&handle);
}
// [サブ手順参照 名前=traceCoverageTest.TearDown]

// 既定パス構築失敗と稼働中の file sink 再オープン失敗を処理することの確認
// [サブ手順参照 名前=traceCoverageTest.SetUp]
TEST_F(traceCoverageTest, file_sink_open_failures)
{
    // Arrange
    cplat_tracer *handle = cplat_tracer_create(
        CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED); // [状態] - 生成済みのトレース ハンドルを用意する。
    ASSERT_NE(nullptr, handle); // [状態確認] - cplat_tracer_create() の戻り値が nullptr 以外であること。
    char path[1] = {};
    int default_path = CPLAT_OK;
    int start_result = CPLAT_OK;
    int reopen_result = CPLAT_OK;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_process_get_executable_path(_, _))
        .WillRepeatedly(Return(
            CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_異常系] - cplat_process_get_executable_path() が任意の回数呼び出されること。
    // [Pre-Assert手順] - cplat_process_get_executable_path() から CPLAT_ERR_UNKNOWN を返す。
    EXPECT_CALL(mock_cplat, cplat_trace_file_sink_create(_, _, _, _))
        .WillOnce(Return(nullptr))
        .WillOnce(Return(nullptr))
        .WillRepeatedly(Return(
            file_handle_)); // [Pre-Assert確認_異常系] - cplat_trace_file_sink_create() が 2 回以上呼び出されること。
    // [Pre-Assert手順] - cplat_trace_file_sink_create() から 1 回目と 2 回目は nullptr、以降は file_handle_ を返す。

    // Act
    default_path = test_tracer_build_default_file_path(
        handle, path,
        sizeof(path)); // [手順] - 既定パスの構築が失敗する状態で test_tracer_build_default_file_path() を呼び出す。
    int actual_ret_set_file_level = cplat_tracer_set_file_level(
        handle, nullptr, CPLAT_TRACE_LEVEL_INFO, 0, 0,
        0); // [手順] - path に nullptr を指定して cplat_tracer_set_file_level() を呼び出す。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_set_file_level); // [確認_正常系] - nullptr パスでの cplat_tracer_set_file_level() の戻り値が CPLAT_OK であること。
    start_result =
        cplat_tracer_start(handle); // [手順] - file sink の生成が失敗する状態で cplat_tracer_start() を呼び出す。
    test_tracer_set_running(handle, 1);
    test_tracer_set_file_handle(handle, file_handle_);
    reopen_result = cplat_tracer_set_file_level(
        handle, "/tmp/b.log", CPLAT_TRACE_LEVEL_DEBUG, 10, 1,
        0); // [手順] - 稼働中に新しい sink の生成が失敗する状態で cplat_tracer_set_file_level() を呼び出す。

    // Assert
    EXPECT_NE(
        CPLAT_OK,
        default_path); // [確認_異常系] - 1 バイト出力先に対する test_tracer_build_default_file_path() の戻り値が CPLAT_OK 以外であること。(既定パス構築が失敗したことの確認)
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        start_result); // [確認_異常系] - sink 生成失敗時の cplat_tracer_start() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        reopen_result); // [確認_異常系] - 再オープン失敗時の cplat_tracer_set_file_level() の戻り値が CPLAT_ERR_UNKNOWN であること。

    // Cleanup
    test_tracer_set_file_handle(handle, nullptr);
    cplat_tracer_dispose(&handle);
}
// [サブ手順参照 名前=traceCoverageTest.TearDown]

// snprintf 失敗と write / hex のセンチネルを処理することの確認
// [サブ手順参照 名前=traceCoverageTest.SetUp]
TEST_F(traceCoverageTest, snprintf_and_hex_edge_paths)
{
    // Arrange
    cplat_tracer *handle = cplat_tracer_create(
        CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED); // [状態] - 生成済みのトレース ハンドルを用意する。
    ASSERT_NE(nullptr, handle); // [状態確認] - cplat_tracer_create() の戻り値が nullptr 以外であること。
    char name[32] = {};
    const unsigned char data[4] = {0x01, 0x02, 0x03, 0x04};
    std::string long_label(1024, 'L');
    std::string mid_label(1019, 'M');
    int name_result = CPLAT_OK;
    int hex_null = CPLAT_OK;
    int hex_empty = CPLAT_OK;
    int hex_long = CPLAT_OK;
    int hex_mid = CPLAT_OK;
    int writef_null = CPLAT_OK;
    int hexf_null = CPLAT_OK;

    ASSERT_EQ(CPLAT_OK, cplat_tracer_start(handle)); // [状態] - tracer を started 状態とする。
    // [状態確認] - cplat_tracer_start() の戻り値が CPLAT_OK であること。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_snprintf(_, _, _))
        .WillOnce(Return(CPLAT_ERR_UNKNOWN))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - cplat_snprintf() が 1 回以上呼び出されること。
    // [Pre-Assert手順] - cplat_snprintf() から 1 回目は CPLAT_ERR_UNKNOWN を返し、以降は既定動作へ委譲する。

    // Act
    name_result = cplat_tracer_get_file_name(
        handle, name,
        sizeof(name)); // [手順] - cplat_snprintf() が失敗する状態で cplat_tracer_get_file_name() を呼び出す。
    hex_null = test_tracer_hex_write_impl(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr, nullptr, 1U,
        "l"); // [手順] - data に nullptr を指定して test_tracer_hex_write_impl() を呼び出す。
    hex_empty =
        cplat_tracer_write_hex_at(handle, CPLAT_TRACE_LEVEL_INFO, nullptr, data, 0U,
                                  "l"); // [手順] - size に 0 を指定して cplat_tracer_write_hex_at() を呼び出す。
    hex_long = test_tracer_hex_write_impl(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr, data, sizeof(data),
        long_label
            .c_str()); // [手順] - MAX_BODY に近い長さの label を指定して test_tracer_hex_write_impl() を呼び出す。
    hex_mid = test_tracer_hex_write_impl(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr, data, 400U,
        mid_label
            .c_str()); // [手順] - 省略記号だけが入る残り幅になる label を指定して test_tracer_hex_write_impl() を呼び出す。
    writef_null =
        cplat_tracer_writef_at(nullptr, CPLAT_TRACE_LEVEL_INFO, nullptr, "%s",
                               "x"); // [手順] - handle に nullptr を指定して cplat_tracer_writef_at() を呼び出す。
    hexf_null = cplat_tracer_vwrite_hexf_at(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr, nullptr, 1U, "%s",
        nullptr); // [手順] - data に nullptr を指定して cplat_tracer_vwrite_hexf_at() を呼び出す。
    int write_hexf_null_format = cplat_tracer_write_hexf_at(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr, data, sizeof(data),
        nullptr); // [手順] - format に nullptr を指定して cplat_tracer_write_hexf_at() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_BUFFER_TOO_SMALL,
        name_result); // [確認_異常系] - cplat_snprintf() 失敗時の cplat_tracer_get_file_name() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_EQ(
        CPLAT_OK,
        hex_null); // [確認_正常系] - data が nullptr の test_tracer_hex_write_impl() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              hex_empty); // [確認_正常系] - size が 0 の cplat_tracer_write_hex_at() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        hex_long); // [確認_正常系] - 長い label での test_tracer_hex_write_impl() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              hex_mid); // [確認_正常系] - 残り幅が狭い test_tracer_hex_write_impl() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        writef_null); // [確認_正常系] - nullptr ハンドルでの cplat_tracer_writef_at() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        hexf_null); // [確認_正常系] - data が nullptr の cplat_tracer_vwrite_hexf_at() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        write_hexf_null_format); // [確認_正常系] - format が nullptr の cplat_tracer_write_hexf_at() の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_tracer_dispose(&handle);
}
// [サブ手順参照 名前=traceCoverageTest.TearDown]

// hook の確保失敗とシャットダウンの二重呼び出しを処理することの確認
// [サブ手順参照 名前=traceCoverageTest.SetUp]
TEST_F(traceCoverageTest, hook_alloc_failure_and_shutdown_repeat)
{
    // Arrange
    cplat_tracer *handle = cplat_tracer_create(
        CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED); // [状態] - 生成済みのトレース ハンドルを用意する。
    ASSERT_NE(nullptr, handle); // [状態確認] - cplat_tracer_create() の戻り値が nullptr 以外であること。
    cplat_shutdown_event event = {};
    cplat_tracer_hook_entry *hook = reinterpret_cast<cplat_tracer_hook_entry *>(static_cast<uintptr_t>(0x1));

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_malloc(_))
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_malloc() が 1 回呼び出されること。
                                    // [Pre-Assert手順] - cplat_malloc() から nullptr を返す。

    // Act
    cplat_tracer_hook_entry *created =
        cplat_tracer_set_hook(handle, coverage_hook,
                              nullptr); // [手順] - cplat_malloc() が失敗する状態で cplat_tracer_set_hook() を呼び出す。
    test_trace_registry_append_null();
    cplat_internal_trace_registry_dispose_all_on_shutdown(
        &event); // [手順] - nullptr エントリを含むレジストリの状態で cplat_internal_trace_registry_dispose_all_on_shutdown() を呼び出す。
    cplat_internal_trace_registry_dispose_all_on_shutdown(
        &event); // [手順] - シャットダウン済みレジストリの状態で cplat_internal_trace_registry_dispose_all_on_shutdown() を呼び出す。
    cplat_internal_trace_registry_dispose_all_on_shutdown(
        nullptr); // [手順] - event に nullptr を指定して cplat_internal_trace_registry_dispose_all_on_shutdown() を呼び出す。
    cplat_tracer_remove_hook(
        handle, hook); // [手順] - シャットダウン後のハンドルを指定して cplat_tracer_remove_hook() を呼び出す。

    // Assert
    EXPECT_EQ(nullptr,
              created); // [確認_異常系] - malloc 失敗時の cplat_tracer_set_hook() の戻り値が nullptr であること。

    // Cleanup
    test_trace_registry_reset_shutdown_state();
}
// [サブ手順参照 名前=traceCoverageTest.TearDown]

// タイムスタンプ解決失敗が write 経路へ伝播することの確認
// [サブ手順参照 名前=traceCoverageTest.SetUp]
TEST_F(traceCoverageTest, write_fails_when_timestamp_resolution_fails)
{
    // Arrange
    cplat_tracer *handle = cplat_tracer_create(
        CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED); // [状態] - 生成済みのトレース ハンドルを用意する。
    ASSERT_NE(nullptr, handle); // [状態確認] - cplat_tracer_create() の戻り値が nullptr 以外であること。
    cplat_timespec ts = {};
    ts.tv_sec = 1;
    ts.tv_nsec = 0;
    int write_result = CPLAT_OK;

    ASSERT_EQ(CPLAT_OK, cplat_tracer_start(handle)); // [状態] - tracer を started 状態とする。
    // [状態確認] - cplat_tracer_start() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_tracer_set_stderr_level(
                            handle, CPLAT_TRACE_LEVEL_DEBUG)); // [状態] - stderr レベルを DEBUG とする。
    // [状態確認] - cplat_tracer_set_stderr_level() の戻り値が CPLAT_OK であること。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_clock_get_realtime(_))
        .WillOnce(
            [](cplat_timespec *resolved)
            {
                resolved->tv_sec = -1;
                resolved->tv_nsec = -1;
            })
        .WillRepeatedly(
            DoDefault()); // [Pre-Assert確認_異常系] - cplat_clock_get_realtime() が 1 回以上呼び出されること。
    // [Pre-Assert手順] - cplat_clock_get_realtime() から 1 回目は、不正な時刻を設定して戻り、以降は既定動作へ委譲する。
    EXPECT_CALL(mock_cplat, cplat_clock_format_realtime_iso8601_local(_, _, _))
        .WillOnce(Return(CPLAT_ERR_UNKNOWN))
        .WillRepeatedly(
            DoDefault()); // [Pre-Assert確認_異常系] - cplat_clock_format_realtime_iso8601_local() が 1 回以上呼び出されること。
    // [Pre-Assert手順] - cplat_clock_format_realtime_iso8601_local() から 1 回目は CPLAT_ERR_UNKNOWN を返し、以降は既定動作へ委譲する。

    // Act
    write_result = cplat_tracer_write(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "msg"); // [手順] - 時刻解決が失敗する状態で、INFO レベルと "msg" を指定して cplat_tracer_write() を呼び出す。
    int format_result = cplat_tracer_write(
        handle, CPLAT_TRACE_LEVEL_INFO, &ts,
        "msg"); // [手順] - 時刻整形が失敗する状態で、明示タイムスタンプと "msg" を指定して cplat_tracer_write() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        write_result); // [確認_異常系] - 時刻解決失敗時の cplat_tracer_write() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        format_result); // [確認_異常系] - 時刻整形失敗時の cplat_tracer_write() の戻り値が CPLAT_ERR_UNKNOWN であること。

    // Cleanup
    cplat_tracer_dispose(&handle);
}
// [サブ手順参照 名前=traceCoverageTest.TearDown]

// 残っている複合条件を inject と設定 API で充足することの確認
// [サブ手順参照 名前=traceCoverageTest.SetUp]
TEST_F(traceCoverageTest, remaining_compound_conditions)
{
    // Arrange
    cplat_tracer *handle = cplat_tracer_create(
        CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED); // [状態] - 生成済みのトレース ハンドルを用意する。
    ASSERT_NE(nullptr, handle); // [状態確認] - cplat_tracer_create() の戻り値が nullptr 以外であること。
    char tiny_name[2] = {};
    char empty_label[] = "";
    const unsigned char data[2] = {0x11, 0x22};
    int active_during_shutdown = 0;
    int name_small = CPLAT_OK;
    int writef_null_fmt = CPLAT_OK;
    int hex_null_handle = CPLAT_OK;
    int hex_empty_label = CPLAT_OK;
    int start_none = CPLAT_OK;
    int utf8_cut = 0;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_process_get_executable_path(_, _))
        .WillOnce(
            [](char *path_out, size_t path_size)
            {
                snprintf(path_out, path_size, "%s", "myapp");
                return CPLAT_OK;
            })
        .WillRepeatedly(
            [](char *path_out, size_t path_size)
            {
                snprintf(path_out, path_size, "%s", "/opt/bin/myapp");
                return CPLAT_OK;
            }); // [Pre-Assert確認_正常系] - cplat_process_get_executable_path() が 1 回以上呼び出されること。
    // [Pre-Assert手順] - cplat_process_get_executable_path() から 1 回目は "myapp"、以降は "/opt/bin/myapp" を格納して CPLAT_OK を返す。

    // Act
    test_tracer_unregister(handle); // [手順] - 登録済みハンドルを指定して test_tracer_unregister() を呼び出す。
    test_tracer_unregister(handle); // [手順] - 未登録ハンドルを指定して test_tracer_unregister() を呼び出す。
    test_trace_registry_set_shutdown_started(1U);
    active_during_shutdown = test_tracer_handle_is_active(
        handle); // [手順] - シャットダウン中の状態で test_tracer_handle_is_active() を呼び出す。
    test_trace_registry_reset_shutdown_state();
    name_small = cplat_tracer_get_file_name(
        handle, tiny_name,
        sizeof(tiny_name)); // [手順] - 2 バイトの出力先を指定して cplat_tracer_get_file_name() を呼び出す。
    char default_path[64] = {};
    (void)test_tracer_build_default_file_path(
        handle, default_path,
        sizeof(
            default_path)); // [手順] - 実行ファイル名だけのパスになる状態で test_tracer_build_default_file_path() を呼び出す。
    int actual_ret_start = cplat_tracer_start(handle); // [手順] - cplat_tracer_start() を呼び出す。
    ASSERT_EQ(CPLAT_OK, actual_ret_start);   // [確認_正常系] - cplat_tracer_start() の戻り値が CPLAT_OK であること。
    start_none =
        cplat_tracer_start(handle); // [手順] - すでに running のハンドルを指定して cplat_tracer_start() を呼び出す。
    writef_null_fmt =
        cplat_tracer_writef_at(handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
                               nullptr); // [手順] - format に nullptr を指定して cplat_tracer_writef_at() を呼び出す。
    hex_null_handle = test_tracer_hex_write_impl(
        nullptr, CPLAT_TRACE_LEVEL_INFO, nullptr, data, sizeof(data),
        "l"); // [手順] - nullptr ハンドルを指定して test_tracer_hex_write_impl() を呼び出す。
    hex_empty_label = test_tracer_hex_write_impl(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr, data, sizeof(data),
        empty_label); // [手順] - 空の label を指定して test_tracer_hex_write_impl() を呼び出す。
    test_tracer_install_null_fn_hook(handle);
    (void)cplat_tracer_write(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "hook"); // [手順] - fn が nullptr の hook を登録した状態で、"hook" を指定して cplat_tracer_write() を呼び出す。
    test_tracer_clear_hook_head(handle);
    utf8_cut = (int)test_tracer_utf8_safe_truncate(
        "\xE3\x81\x82",
        2U); // [手順] - 継続バイトの位置になる長さ 2 を指定して test_tracer_utf8_safe_truncate() を呼び出す。
    test_tracer_set_running(handle, 1);
    test_tracer_set_file_handle(handle, file_handle_);
    int actual_ret_set_level_debug_null = cplat_tracer_set_file_level(
        handle, nullptr, CPLAT_TRACE_LEVEL_DEBUG, 0, 0,
        0); // [手順] - 稼働中に path を nullptr のまま、しきい値だけを変えて cplat_tracer_set_file_level() を呼び出す。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_set_level_debug_null); // [確認_正常系] - 稼働中に path が nullptr の cplat_tracer_set_file_level() の戻り値が CPLAT_OK であること。
    int actual_ret_set_level_same_info = cplat_tracer_set_file_level(
        handle, "/tmp/same.log", CPLAT_TRACE_LEVEL_INFO, 8, 2,
        1); // [手順] - 稼働中に "/tmp/same.log" を指定して cplat_tracer_set_file_level() を呼び出す。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_set_level_same_info); // [確認_正常系] - "/tmp/same.log" へ INFO を設定した cplat_tracer_set_file_level() の戻り値が CPLAT_OK であること。
    int actual_ret_set_level_same_debug = cplat_tracer_set_file_level(
        handle, "/tmp/same.log", CPLAT_TRACE_LEVEL_DEBUG, 8, 2,
        1); // [手順] - 稼働中に同一の構造パラメーターのまま、しきい値だけを変えて cplat_tracer_set_file_level() を呼び出す。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_set_level_same_debug); // [確認_正常系] - 同一構造パラメーターでしきい値だけ変えた cplat_tracer_set_file_level() の戻り値が CPLAT_OK であること。
    int actual_ret_set_level_other = cplat_tracer_set_file_level(
        handle, "/tmp/other.log", CPLAT_TRACE_LEVEL_DEBUG, 8, 2,
        1); // [手順] - 稼働中にパスを変えて、開き直すように cplat_tracer_set_file_level() を呼び出す。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_set_level_other); // [確認_正常系] - パスを変えて開き直した cplat_tracer_set_file_level() の戻り値が CPLAT_OK であること。
    test_tracer_set_file_handle(handle, nullptr);
    int actual_ret_set_level_none = cplat_tracer_set_file_level(
        handle, nullptr, CPLAT_TRACE_LEVEL_NONE, 0, 0,
        0); // [手順] - 稼働中に file なしで、出力を無効化するように cplat_tracer_set_file_level() を呼び出す。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_set_level_none); // [確認_正常系] - 稼働中・file なしで出力を無効化した cplat_tracer_set_file_level() の戻り値が CPLAT_OK であること。
    test_tracer_set_file_handle(handle, nullptr);
    int actual_ret_set_level_opened = cplat_tracer_set_file_level(
        handle, "/tmp/opened.log", CPLAT_TRACE_LEVEL_INFO, 1, 0,
        0); // [手順] - 稼働中に旧ハンドルなしで、新しい sink を開くように cplat_tracer_set_file_level() を呼び出す。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_set_level_opened); // [確認_正常系] - 稼働中・旧ハンドルなしで新しい sink を開いた cplat_tracer_set_file_level() の戻り値が CPLAT_OK であること。
    (void)cplat_tracer_write_hex_at(nullptr, CPLAT_TRACE_LEVEL_INFO, nullptr, data, sizeof(data), "l");
    (void)cplat_tracer_write_hex_at(handle, CPLAT_TRACE_LEVEL_INFO, nullptr, nullptr, sizeof(data), "l");
    (void)cplat_tracer_write_hexf_at(nullptr, CPLAT_TRACE_LEVEL_INFO, nullptr, data, sizeof(data), "%s", "l");
    (void)cplat_tracer_get_file_name(handle, nullptr, 8);
    (void)cplat_tracer_get_file_name(handle, tiny_name, 0);
    (void)cplat_tracer_get_identifier(nullptr);
    {
        // Pre-Assert
        EXPECT_CALL(mock_cplat, cplat_snprintf(_, _, _))
            .WillOnce(DoDefault())
            .WillOnce(DoDefault())
            .WillOnce(Return(
                CPLAT_ERR_BUFFER_TOO_SMALL)); // [Pre-Assert確認_正常系] - cplat_snprintf() が 3 回呼び出されること。
        // [Pre-Assert手順] - cplat_snprintf() から 1 回目と 2 回目は既定動作へ委譲し、3 回目は CPLAT_ERR_BUFFER_TOO_SMALL を返す。
        // Arrange
        char path_buf[64] = {};
        // Act
        (void)test_tracer_build_default_file_path(
            handle, path_buf,
            sizeof(
                path_buf)); // [手順] - .log 付与の cplat_snprintf() が過大長を返す状態で test_tracer_build_default_file_path() を呼び出す。
    }
    cplat_tracer_remove_hook(handle, reinterpret_cast<cplat_tracer_hook_entry *>(static_cast<uintptr_t>(0x2)));
    test_tracer_call_next_null(handle); // [手順] - prev に nullptr を指定して test_tracer_call_next_null() を呼び出す。
    test_tracer_call_next_with_fn(
        handle, coverage_hook); // [手順] - fn 付きの prev を指定して test_tracer_call_next_with_fn() を呼び出す。
    test_tracer_set_file_handle(handle, nullptr);
    cplat_tracer_dispose(
        &handle); // [手順] - tracer が所有する同期リソースを解放するように cplat_tracer_dispose() を呼び出す。

    // Assert
    EXPECT_EQ(
        0,
        active_during_shutdown); // [確認_異常系] - シャットダウン中の test_tracer_handle_is_active() の戻り値が 0 であること。(シャットダウン中は非アクティブであることの確認)
    EXPECT_EQ(
        CPLAT_ERR_BUFFER_TOO_SMALL,
        name_small); // [確認_異常系] - 2 バイト出力先の cplat_tracer_get_file_name() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_EQ(CPLAT_OK,
              start_none); // [確認_正常系] - 再 start の cplat_tracer_start() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        writef_null_fmt); // [確認_正常系] - format が nullptr の cplat_tracer_writef_at() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        hex_null_handle); // [確認_正常系] - nullptr ハンドルの test_tracer_hex_write_impl() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        hex_empty_label); // [確認_正常系] - 空 label の test_tracer_hex_write_impl() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        0,
        utf8_cut); // [確認_正常系] - 継続バイト位置での test_tracer_utf8_safe_truncate() の戻り値が 0 であること。(切り詰め位置が先頭であることの確認)
}
// [サブ手順参照 名前=traceCoverageTest.TearDown]

// 稼働中に file を閉じたあとの通常解放を処理することの確認
// [サブ手順参照 名前=traceCoverageTest.SetUp]
TEST_F(traceCoverageTest, release_normal_disposes_open_file_and_hooks)
{
    // Arrange
    cplat_tracer *handle = cplat_tracer_create(
        CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED); // [状態] - 生成済みのトレース ハンドルを用意する。
    ASSERT_NE(nullptr, handle); // [状態確認] - cplat_tracer_create() の戻り値が nullptr 以外であること。
    int started = CPLAT_OK;

    // Pre-Assert

    // Act
    cplat_tracer_hook_entry *hook = cplat_tracer_set_hook(handle, coverage_hook, nullptr);
    int actual_ret_set_file_level = cplat_tracer_set_file_level(
        handle, "/tmp/c.log", CPLAT_TRACE_LEVEL_INFO, 0, 0,
        0); // [手順] - パス "/tmp/c.log" と INFO レベルを指定して cplat_tracer_set_file_level() を呼び出す。
    ASSERT_EQ(CPLAT_OK,
              actual_ret_set_file_level); // [状態確認] - cplat_tracer_set_file_level() の戻り値が CPLAT_OK であること。
    started = cplat_tracer_start(handle); // [手順] - ファイル出力が有効な状態で cplat_tracer_start() を呼び出す。
    (void)hook;
    cplat_tracer_dispose(
        &handle); // [手順] - 開いているファイルと hook を持つハンドルを指定して cplat_tracer_dispose() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        started); // [確認_正常系] - ファイル出力を有効にした cplat_tracer_start() の戻り値が CPLAT_OK であること。
}
// [サブ手順参照 名前=traceCoverageTest.TearDown]

// 登録直前のシャットダウンと、停止中の残存ファイル ハンドルを処理することの確認
// [サブ手順参照 名前=traceCoverageTest.SetUp]
TEST_F(traceCoverageTest, register_during_shutdown_and_stale_file_handle)
{
    // Arrange
    cplat_tracer *handle = cplat_tracer_create(
        CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED); // [状態] - 生成済みのトレース ハンドルを用意する。
    ASSERT_NE(nullptr, handle); // [状態確認] - cplat_tracer_create() の戻り値が nullptr 以外であること。
#if defined(PLATFORM_LINUX)
    cplat_tracer *rejected = nullptr;
#endif /* PLATFORM_LINUX */
    int stop_result = CPLAT_OK;
    int file_level_result = CPLAT_OK;
    int os_level_seen = 0;
    cplat_shutdown_event event = {};

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_cplat, cplat_syslog_sink_create(_, _))
        .WillOnce(Invoke(
            [this](const char *, int)
            {
                test_trace_registry_set_shutdown_started(1U);
                return os_handle_;
            })); // [Pre-Assert確認_正常系] - cplat_syslog_sink_create() が 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_syslog_sink_create() から、shutdown 開始フラグを立ててダミー sink を返す。
#endif           /* PLATFORM_LINUX */

    // Act
#if defined(PLATFORM_LINUX)
    rejected = cplat_tracer_create(
        CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED); // [手順] - syslog sink の生成中にシャットダウンが開始される状態で cplat_tracer_create() を呼び出す。
#endif                                            /* PLATFORM_LINUX */
    test_trace_registry_reset_shutdown_state();
    test_tracer_set_running(handle, 0);
    test_tracer_set_file_handle(handle, file_handle_);
    file_level_result = cplat_tracer_set_file_level(
        handle, nullptr, CPLAT_TRACE_LEVEL_NONE, 0, 0,
        0); // [手順] - 停止中に残っている file ハンドルを閉じるように、path に nullptr を指定して cplat_tracer_set_file_level() を呼び出す。
    test_tracer_set_file_handle(handle, file_handle_);
    test_tracer_set_lifecycle_state(handle, kLifecycleDisposed);
    stop_result =
        cplat_tracer_stop(handle); // [手順] - 非アクティブ ハンドルを指定して cplat_tracer_stop() を呼び出す。
    os_level_seen = (int)cplat_tracer_get_os_level(
        handle); // [手順] - 非アクティブ ハンドルを指定して cplat_tracer_get_os_level() を呼び出す。
    test_tracer_set_lifecycle_state(handle, 0);
    test_tracer_set_file_handle(handle, file_handle_);
    test_trace_registry_append_null();
    cplat_internal_trace_registry_dispose_all_on_shutdown(
        &event); // [手順] - 開いている file ハンドルを持つ状態で cplat_internal_trace_registry_dispose_all_on_shutdown() を呼び出す。

    // Assert
#if defined(PLATFORM_LINUX)
    EXPECT_EQ(
        nullptr,
        rejected); // [確認_異常系] - 登録直前のシャットダウンでの cplat_tracer_create() の戻り値が nullptr であること。
#endif                                         /* PLATFORM_LINUX */
    EXPECT_EQ(
        CPLAT_OK,
        file_level_result); // [確認_正常系] - 停止中に残存 file ハンドルを閉じた cplat_tracer_set_file_level() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        stop_result); // [確認_異常系] - 非アクティブ ハンドルでの cplat_tracer_stop() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        (int)CPLAT_TRACE_LEVEL_NONE,
        os_level_seen); // [確認_異常系] - 非アクティブ ハンドルでの cplat_tracer_get_os_level() の戻り値が CPLAT_TRACE_LEVEL_NONE であること。

    // Cleanup
    test_trace_registry_reset_shutdown_state();
}
// [サブ手順参照 名前=traceCoverageTest.TearDown]

// 排他ロック待ち中の dispose と set_file_level の enter 失敗を処理することの確認
// [サブ手順参照 名前=traceCoverageTest.SetUp]
TEST_F(traceCoverageTest, exclusive_lock_lifecycle_and_set_file_level_enter_failure)
{
    // Arrange
    cplat_tracer *handle = cplat_tracer_create(
        CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED); // [状態] - 生成済みのトレース ハンドルを用意する。
    ASSERT_NE(nullptr, handle); // [状態確認] - cplat_tracer_create() の戻り値が nullptr 以外であること。
    int start_result = CPLAT_OK;
    int file_level_result = CPLAT_OK;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_local_rwlock_lock_exclusive(_, _))
        .WillOnce(Invoke(
            [handle](cplat_local_rwlock *, int)
            {
                test_tracer_set_lifecycle_state(handle, kLifecycleDisposing);
                return CPLAT_OK;
            }))
        .WillRepeatedly(
            DoDefault()); // [Pre-Assert確認_異常系] - cplat_local_rwlock_lock_exclusive() が 1 回以上呼び出されること。
    // [Pre-Assert手順] - cplat_local_rwlock_lock_exclusive() から 1 回目は、ライフサイクルを DISPOSING へ変更して CPLAT_OK を返し、以降は既定動作へ委譲する。

    // Act
    start_result =
        cplat_tracer_start(handle); // [手順] - ロック中に DISPOSING へ変わる状態で cplat_tracer_start() を呼び出す。
    test_tracer_set_lifecycle_state(handle, kLifecycleDisposed);
    file_level_result = cplat_tracer_set_file_level(
        handle, "/tmp/d.log", CPLAT_TRACE_LEVEL_INFO, 0, 0,
        0); // [手順] - 非アクティブ ハンドルとパス "/tmp/d.log" を指定して cplat_tracer_set_file_level() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        start_result); // [確認_異常系] - ロック中に DISPOSING へ変わった cplat_tracer_start() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        file_level_result); // [確認_異常系] - 非アクティブ ハンドルでの cplat_tracer_set_file_level() の戻り値が CPLAT_ERR_UNKNOWN であること。

    // Cleanup
    test_tracer_set_lifecycle_state(handle, 0);
    cplat_tracer_dispose(&handle);
}
// [サブ手順参照 名前=traceCoverageTest.TearDown]

// 既定パスの snprintf 失敗、NULL パスの sink 生成、残存 file の通常解放を処理することの確認
// [サブ手順参照 名前=traceCoverageTest.SetUp]
TEST_F(traceCoverageTest, default_path_snprintf_failure_and_normal_file_release)
{
    // Arrange
    cplat_tracer *handle = cplat_tracer_create(
        CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED); // [状態] - 生成済みのトレース ハンドルを用意する。
    ASSERT_NE(nullptr, handle); // [状態確認] - cplat_tracer_create() の戻り値が nullptr 以外であること。
    char path[64] = {};
    int default_path = CPLAT_OK;
    int start_result = CPLAT_OK;
    unsigned char payload[400];
    std::string label_1021(1021, 'A');
    std::string label_1018(1018, 'B');
    int hex_plain = CPLAT_OK;
    int hex_long_label = CPLAT_OK;
    int hex_ellipsis_only = CPLAT_OK;

    memset(payload, 0xAB, sizeof(payload));

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_trace_file_sink_create(_, _, _, _))
        .WillRepeatedly(Return(
            file_handle_)); // [Pre-Assert確認_正常系] - cplat_trace_file_sink_create() が任意の回数呼び出されること。
    // [Pre-Assert手順] - cplat_trace_file_sink_create() から file_handle_ を返す。

    // Act
    {
        // Pre-Assert
        EXPECT_CALL(mock_cplat, cplat_snprintf(_, _, _))
            .WillOnce(DoDefault())
            .WillOnce(DoDefault())
            .WillOnce(Return(CPLAT_ERR_UNKNOWN))
            .WillOnce(DoDefault())
            .WillOnce(DoDefault())
            .WillOnce(Return(CPLAT_ERR_UNKNOWN))
            .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - cplat_snprintf() が 6 回以上呼び出されること。
        // [Pre-Assert手順] - cplat_snprintf() から 3 回目と 6 回目は CPLAT_ERR_UNKNOWN を返し、他は既定動作へ委譲する。
        // Act
        default_path = test_tracer_build_default_file_path(
            handle, path,
            sizeof(
                path)); // [手順] - ファイル名の組み立て後の .log 付与で cplat_snprintf() が失敗する状態で test_tracer_build_default_file_path() を呼び出す。
        int actual_ret_set_file_level = cplat_tracer_set_file_level(
            handle, nullptr, CPLAT_TRACE_LEVEL_INFO, 0, 0,
            0); // [手順] - path に nullptr を指定して cplat_tracer_set_file_level() を呼び出す。
        ASSERT_EQ(
            CPLAT_OK,
            actual_ret_set_file_level); // [確認_正常系] - nullptr パスでの cplat_tracer_set_file_level() の戻り値が CPLAT_OK であること。
        start_result = cplat_tracer_start(
            handle); // [手順] - 既定パスの構築と sink の生成が失敗する状態で cplat_tracer_start() を呼び出す。
    }
    test_tracer_set_running(handle, 0);
    test_tracer_set_file_handle(handle, file_handle_);
    cplat_tracer_dispose(
        &handle); // [手順] - 停止中に file ハンドルが残っているハンドルを指定して cplat_tracer_dispose() を呼び出す。
    handle = cplat_tracer_create(CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED);
    ASSERT_NE(nullptr,
              handle); // [状態確認] - 再生成した cplat_tracer_create() の戻り値が nullptr 以外であること。
    int actual_ret_start = cplat_tracer_start(handle); // [手順] - cplat_tracer_start() を呼び出す。
    ASSERT_EQ(CPLAT_OK,
              actual_ret_start); // [確認_正常系] - hex 出力前の cplat_tracer_start() の戻り値が CPLAT_OK であること。
    hex_plain = test_tracer_hex_write_impl(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr, payload, sizeof(payload),
        nullptr); // [手順] - label に nullptr、長い payload を指定して test_tracer_hex_write_impl() を呼び出す。
    hex_long_label = test_tracer_hex_write_impl(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr, payload, sizeof(payload),
        label_1021.c_str()); // [手順] - 長さ 1021 の label を指定して test_tracer_hex_write_impl() を呼び出す。
    hex_ellipsis_only = test_tracer_hex_write_impl(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr, payload, sizeof(payload),
        label_1018
            .c_str()); // [手順] - 省略記号だけが入る残り幅になる長さ 1018 の label を指定して test_tracer_hex_write_impl() を呼び出す。

    // Assert
    EXPECT_EQ(
        -1,
        default_path); // [確認_異常系] - cplat_snprintf() 失敗時の test_tracer_build_default_file_path() の戻り値が -1 であること。(既定パス構築が失敗したことの確認)
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        start_result); // [確認_異常系] - 既定パス失敗時の cplat_tracer_start() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(CPLAT_OK,
              hex_plain); // [確認_正常系] - label なしの test_tracer_hex_write_impl() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        hex_long_label); // [確認_正常系] - 長さ 1021 の label での test_tracer_hex_write_impl() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        hex_ellipsis_only); // [確認_正常系] - 残り幅が狭い test_tracer_hex_write_impl() の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_tracer_dispose(&handle);
}
// [サブ手順参照 名前=traceCoverageTest.TearDown]

// 残っている C2 分岐を設定変更と inject で充足することの確認
// [サブ手順参照 名前=traceCoverageTest.SetUp]
TEST_F(traceCoverageTest, remaining_gcov_branches)
{
    // Arrange
    cplat_tracer *handle = cplat_tracer_create(
        CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED); // [状態] - 生成済みのトレース ハンドルを用意する。
    ASSERT_NE(nullptr, handle); // [状態確認] - cplat_tracer_create() の戻り値が nullptr 以外であること。
    cplat_tracer *disposed = cplat_tracer_create(
        CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED); // [状態] - 破棄対象のトレース ハンドルを用意する。
    ASSERT_NE(nullptr,
              disposed); // [状態確認] - 破棄対象の cplat_tracer_create() の戻り値が nullptr 以外であること。
    const unsigned char data[2] = {0xAA, 0xBB};
    cplat_timespec invalid_ts = {};
    invalid_ts.tv_sec = -1;
    invalid_ts.tv_nsec = -1;
    char tiny_name[2] = {};
    char name_buf[32] = {};
    int dirname_fail = CPLAT_OK;
    int quiet_write = CPLAT_OK;
    int fallback_write = CPLAT_OK;
    int os_fail_write = CPLAT_OK;
    int file_fail_write = CPLAT_OK;
    int hex_size_zero = CPLAT_OK;
    int hex_not_running = CPLAT_OK;
    int hexf_not_running = CPLAT_OK;
    int start_already_open = CPLAT_OK;
    int name_null = CPLAT_OK;
    int name_zero = CPLAT_OK;
    int name_small = CPLAT_OK;
    int name_snprintf = CPLAT_OK;
    int disable_open_file = CPLAT_OK;
    cplat_shutdown_event event = {};

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_process_get_executable_path(_, _))
        .WillOnce(
            [](char *path_out, size_t path_size)
            {
                snprintf(path_out, path_size, "%s", "/opt/bin/myapp");
                return CPLAT_OK;
            })
        .WillOnce(
            [](char *path_out, size_t path_size)
            {
                if (path_size > 0U)
                {
                    path_out[0] = '\0';
                }
                return CPLAT_OK;
            })
        .WillOnce(
            [](char *path_out, size_t path_size)
            {
                snprintf(path_out, path_size, "%s", "/opt/bin/myapp");
                return CPLAT_OK;
            })
        .WillOnce(
            [](char *path_out, size_t path_size)
            {
                snprintf(path_out, path_size, "%s", "myapp");
                return CPLAT_OK;
            })
        .WillRepeatedly(
            [](char *path_out, size_t path_size)
            {
                snprintf(path_out, path_size, "%s", "/opt/bin/myapp");
                return CPLAT_OK;
            }); // [Pre-Assert確認_異常系] - cplat_process_get_executable_path() が 4 回以上呼び出されること。
    // [Pre-Assert手順] - cplat_process_get_executable_path() から 1 回目は "/opt/bin/myapp"、2 回目は空文字、3 回目は "/opt/bin/myapp"、4 回目は "myapp"、以降は "/opt/bin/myapp" を格納して CPLAT_OK を返す。
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_cplat, cplat_syslog_sink_write(_, _, _, _))
        .WillOnce(Return(-1))
        .WillRepeatedly(
            Return(CPLAT_OK)); // [Pre-Assert確認_正常系] - cplat_syslog_sink_write() が 1 回以上呼び出されること。
    // [Pre-Assert手順] - cplat_syslog_sink_write() から 1 回目は -1、以降は CPLAT_OK を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_cplat, cplat_eventlog_sink_write(_, _, _, _, _, _))
        .WillOnce(Return(-1))
        .WillRepeatedly(
            Return(CPLAT_OK)); // [Pre-Assert確認_正常系] - cplat_eventlog_sink_write() が 1 回以上呼び出されること。
    // [Pre-Assert手順] - cplat_eventlog_sink_write() から 1 回目は -1、以降は CPLAT_OK を返す。
#endif /* PLATFORM_ */
    EXPECT_CALL(mock_cplat, cplat_trace_file_sink_write(_, _, _, _))
        .WillOnce(Return(-1))
        .WillRepeatedly(
            Return(CPLAT_OK)); // [Pre-Assert確認_異常系] - cplat_trace_file_sink_write() が 1 回以上呼び出されること。
    // [Pre-Assert手順] - cplat_trace_file_sink_write() から 1 回目は -1、以降は CPLAT_OK を返す。

    // Act
    {
        char path_buf[64] = {};
        dirname_fail = test_tracer_build_default_file_path(
            handle, path_buf,
            sizeof(
                path_buf)); // [手順] - 実行ファイル パスが空の状態で test_tracer_build_default_file_path() を呼び出す。
        (void)test_tracer_build_default_file_path(
            handle, path_buf,
            sizeof(
                path_buf)); // [手順] - 実行ファイル名だけのパスになる状態で test_tracer_build_default_file_path() を呼び出す。
    }
    int actual_ret_set_level_none =
        cplat_tracer_set_file_level(handle, nullptr, CPLAT_TRACE_LEVEL_NONE, 0, 0,
                                    0); // [手順] - NONE レベルを指定して cplat_tracer_set_file_level() を呼び出す。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_set_level_none); // [確認_正常系] - 開始前に file 出力を無効化した cplat_tracer_set_file_level() の戻り値が CPLAT_OK であること。
    int actual_ret_start = cplat_tracer_start(handle); // [手順] - cplat_tracer_start() を呼び出す。
    ASSERT_EQ(CPLAT_OK, actual_ret_start); // [確認_正常系] - cplat_tracer_start() の戻り値が CPLAT_OK であること。
    quiet_write = cplat_tracer_write(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "quiet"); // [手順] - 全出力先が無効な状態で、INFO レベルと "quiet" を指定して cplat_tracer_write() を呼び出す。
    {
        cplat_timespec only_ts = {};
        only_ts.tv_sec = 1;
        only_ts.tv_nsec = 0;
        (void)cplat_tracer_write(
            handle, CPLAT_TRACE_LEVEL_INFO, &only_ts,
            "ts-only"); // [手順] - 出力先なしで、明示タイムスタンプと "ts-only" を指定して cplat_tracer_write() を呼び出す。
    }
    (void)cplat_tracer_write_hexf_at(handle, CPLAT_TRACE_LEVEL_INFO, nullptr, data, 0U, "%s",
                                     "z"); // [手順] - size に 0 を指定して cplat_tracer_write_hexf_at() を呼び出す。
    int actual_ret_set_stderr_debug = cplat_tracer_set_stderr_level(
        handle,
        CPLAT_TRACE_LEVEL_DEBUG); // [手順] - DEBUG レベルを指定して cplat_tracer_set_stderr_level() を呼び出す。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_set_stderr_debug); // [確認_正常系] - DEBUG を指定した cplat_tracer_set_stderr_level() の戻り値が CPLAT_OK であること。
    fallback_write = cplat_tracer_write(
        handle, CPLAT_TRACE_LEVEL_INFO, &invalid_ts,
        "fallback"); // [手順] - 不正な明示タイムスタンプと "fallback" を指定して cplat_tracer_write() を呼び出す。
    int actual_ret_set_os_debug = cplat_tracer_set_os_level(
        handle, CPLAT_TRACE_LEVEL_DEBUG); // [手順] - DEBUG レベルを指定して cplat_tracer_set_os_level() を呼び出す。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_set_os_debug); // [確認_正常系] - DEBUG を指定した cplat_tracer_set_os_level() の戻り値が CPLAT_OK であること。
    os_fail_write = cplat_tracer_write(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "os"); // [手順] - OS バックエンドの書き込みが失敗する状態で、"os" を指定して cplat_tracer_write() を呼び出す。
    int actual_ret_set_os_none = cplat_tracer_set_os_level(
        handle, CPLAT_TRACE_LEVEL_NONE); // [手順] - NONE レベルを指定して cplat_tracer_set_os_level() を呼び出す。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_set_os_none); // [確認_正常系] - NONE を指定した cplat_tracer_set_os_level() の戻り値が CPLAT_OK であること。
    int actual_ret_set_stderr_none = cplat_tracer_set_stderr_level(
        handle, CPLAT_TRACE_LEVEL_NONE); // [手順] - NONE レベルを指定して cplat_tracer_set_stderr_level() を呼び出す。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_set_stderr_none); // [確認_正常系] - NONE を指定した cplat_tracer_set_stderr_level() の戻り値が CPLAT_OK であること。
    test_tracer_set_file_handle(handle, file_handle_);
    int actual_ret_set_level_fail = cplat_tracer_set_file_level(
        handle, "/tmp/fail.log", CPLAT_TRACE_LEVEL_DEBUG, 8, 1,
        0); // [手順] - パス "/tmp/fail.log" と DEBUG レベルを指定して cplat_tracer_set_file_level() を呼び出す。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_set_level_fail); // [確認_正常系] - "/tmp/fail.log" を指定した cplat_tracer_set_file_level() の戻り値が CPLAT_OK であること。
    file_fail_write = cplat_tracer_write(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "file"); // [手順] - file の書き込みが失敗する状態で、"file" を指定して cplat_tracer_write() を呼び出す。
    hex_size_zero =
        test_tracer_hex_write_impl(handle, CPLAT_TRACE_LEVEL_INFO, nullptr, data, 0U,
                                   "l"); // [手順] - size に 0 を指定して test_tracer_hex_write_impl() を呼び出す。
    int actual_ret_stop = cplat_tracer_stop(handle); // [手順] - cplat_tracer_stop() を呼び出す。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_stop); // [確認_正常系] - hex 書き込みの前の cplat_tracer_stop() の戻り値が CPLAT_OK であること。
    hex_not_running =
        cplat_tracer_write_hex_at(handle, CPLAT_TRACE_LEVEL_INFO, nullptr, data, sizeof(data),
                                  "l"); // [手順] - 停止中の状態で cplat_tracer_write_hex_at() を呼び出す。
    hexf_not_running =
        cplat_tracer_write_hexf_at(handle, CPLAT_TRACE_LEVEL_INFO, nullptr, data, sizeof(data), "%s",
                                   "l"); // [手順] - 停止中の状態で cplat_tracer_write_hexf_at() を呼び出す。
    test_tracer_set_file_handle(handle, file_handle_);
    start_already_open =
        cplat_tracer_start(handle); // [手順] - すでに file ハンドルがある状態で cplat_tracer_start() を呼び出す。
    name_null = cplat_tracer_get_name(handle, nullptr,
                                      8U); // [手順] - 出力先に nullptr を指定して cplat_tracer_get_name() を呼び出す。
    name_zero = cplat_tracer_get_name(handle, tiny_name,
                                      0U); // [手順] - 出力サイズに 0 を指定して cplat_tracer_get_name() を呼び出す。
    name_small = cplat_tracer_get_name(
        handle, tiny_name,
        sizeof(tiny_name)); // [手順] - 2 バイトの出力先を指定して cplat_tracer_get_name() を呼び出す。
    {
        // Pre-Assert
        EXPECT_CALL(mock_cplat, cplat_snprintf(_, _, _))
            .WillOnce(Return(CPLAT_ERR_UNKNOWN))
            .WillRepeatedly(DoDefault()); // [Pre-Assert確認_正常系] - cplat_snprintf() が 1 回以上呼び出されること。
        // [Pre-Assert手順] - cplat_snprintf() から 1 回目は CPLAT_ERR_UNKNOWN を返し、以降は既定動作へ委譲する。
        // Act
        name_snprintf = cplat_tracer_get_name(
            handle, name_buf,
            sizeof(name_buf)); // [手順] - cplat_snprintf() が失敗する状態で cplat_tracer_get_name() を呼び出す。
    }
    disable_open_file = cplat_tracer_set_file_level(
        handle, nullptr, CPLAT_TRACE_LEVEL_NONE, 0, 0,
        0); // [手順] - 稼働中に開いている file を無効化するように、NONE レベルを指定して cplat_tracer_set_file_level() を呼び出す。
    test_tracer_set_running(handle, 1);
    test_tracer_set_file_handle(handle, file_handle_);
    int actual_ret_set_level_combo = cplat_tracer_set_file_level(
        handle, "/tmp/combo.log", CPLAT_TRACE_LEVEL_INFO, 8, 2,
        1); // [手順] - 複合パラメーターを指定して cplat_tracer_set_file_level() を呼び出す。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_set_level_combo); // [確認_正常系] - "/tmp/combo.log" を指定した cplat_tracer_set_file_level() の戻り値が CPLAT_OK であること。
    int actual_ret_set_level_keep_path = cplat_tracer_set_file_level(
        handle, nullptr, CPLAT_TRACE_LEVEL_DEBUG, 8, 2,
        1); // [手順] - path に nullptr を指定し、file_path がある状態で cplat_tracer_set_file_level() を呼び出す。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_set_level_keep_path); // [確認_正常系] - path が nullptr かつ file_path ありの cplat_tracer_set_file_level() の戻り値が CPLAT_OK であること。
    test_tracer_set_file_handle(handle, file_handle_);
    test_tracer_clear_file_path(handle);
    int actual_ret_set_level_nopath = cplat_tracer_set_file_level(
        handle, "/tmp/nopath.log", CPLAT_TRACE_LEVEL_INFO, 8, 2,
        1); // [手順] - file_path が nullptr で、path を指定して cplat_tracer_set_file_level() を呼び出す。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_set_level_nopath); // [確認_正常系] - file_path が nullptr かつ path ありの cplat_tracer_set_file_level() の戻り値が CPLAT_OK であること。
    int actual_ret_set_level_same2 = cplat_tracer_set_file_level(
        handle, "/tmp/same2.log", CPLAT_TRACE_LEVEL_INFO, 8, 2,
        1); // [手順] - パス "/tmp/same2.log" を指定して cplat_tracer_set_file_level() を呼び出す。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_set_level_same2); // [確認_正常系] - "/tmp/same2.log" へ INFO を設定した cplat_tracer_set_file_level() の戻り値が CPLAT_OK であること。
    int actual_ret_set_level_max_bytes = cplat_tracer_set_file_level(
        handle, "/tmp/same2.log", CPLAT_TRACE_LEVEL_INFO, 16, 2,
        1); // [手順] - 同一パスのまま max_bytes だけを変えて cplat_tracer_set_file_level() を呼び出す。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_set_level_max_bytes); // [確認_正常系] - 同一パスで max_bytes だけ変えた cplat_tracer_set_file_level() の戻り値が CPLAT_OK であること。
    int actual_ret_set_level_generations = cplat_tracer_set_file_level(
        handle, "/tmp/same2.log", CPLAT_TRACE_LEVEL_INFO, 16, 3,
        1); // [手順] - 同一パスのまま generations だけを変えて cplat_tracer_set_file_level() を呼び出す。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_set_level_generations); // [確認_正常系] - 同一パスで generations だけ変えた cplat_tracer_set_file_level() の戻り値が CPLAT_OK であること。
    int actual_ret_set_level_flags = cplat_tracer_set_file_level(
        handle, "/tmp/same2.log", CPLAT_TRACE_LEVEL_INFO, 16, 3,
        0); // [手順] - 同一パスのまま flags だけを変えて cplat_tracer_set_file_level() を呼び出す。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_set_level_flags); // [確認_正常系] - 同一パスで flags だけ変えた cplat_tracer_set_file_level() の戻り値が CPLAT_OK であること。
    int actual_ret_stop_after = cplat_tracer_stop(handle); // [手順] - cplat_tracer_stop() を呼び出す。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_stop_after); // [確認_正常系] - hook 操作の前の cplat_tracer_stop() の戻り値が CPLAT_OK であること。
    cplat_tracer_hook_entry *hook = cplat_tracer_set_hook(handle, coverage_hook, nullptr);
    cplat_tracer_hook_entry *hook2 = cplat_tracer_set_hook(handle, coverage_hook, nullptr);
    cplat_tracer_remove_hook(handle, reinterpret_cast<cplat_tracer_hook_entry *>(static_cast<uintptr_t>(0x3)));
    cplat_tracer_remove_hook(handle,
                             hook2); // [手順] - 登録済みの hook を指定して cplat_tracer_remove_hook() を呼び出す。
    cplat_tracer_remove_hook(handle, hook);
    test_tracer_call_next_null_fn(
        handle); // [手順] - fn が nullptr の prev を指定して test_tracer_call_next_null_fn() を呼び出す。

    // Pre-Assert_2
    EXPECT_CALL(mock_cplat, cplat_local_rwlock_lock_shared(_, _))
        .WillOnce(Return(CPLAT_ERR_TIMEOUT))
        .WillOnce(Return(CPLAT_ERR_TIMEOUT))
        .WillRepeatedly(
            DoDefault()); // [Pre-Assert確認_異常系] - cplat_local_rwlock_lock_shared() が 2 回以上呼び出されること。
    // [Pre-Assert手順] - cplat_local_rwlock_lock_shared() から 1 回目と 2 回目は CPLAT_ERR_TIMEOUT を返し、以降は既定動作へ委譲する。

    // Act_2
    (void)cplat_tracer_get_file_name(
        handle, name_buf,
        sizeof(name_buf)); // [手順] - 共有ロックが失敗する状態で cplat_tracer_get_file_name() を呼び出す。
    (void)cplat_tracer_get_name(
        handle, name_buf, sizeof(name_buf)); // [手順] - 共有ロックが失敗する状態で cplat_tracer_get_name() を呼び出す。
    test_tracer_unregister(handle);
    test_tracer_set_lifecycle_state(disposed, kLifecycleDisposed);
    cplat_internal_trace_registry_dispose_all_on_shutdown(
        &event); // [手順] - DISPOSED ハンドルを含むレジストリの状態で cplat_internal_trace_registry_dispose_all_on_shutdown() を呼び出す。

    // Assert
    EXPECT_EQ(
        0,
        dirname_fail); // [確認_正常系] - 空の実行ファイル パスに対する test_tracer_build_default_file_path() の戻り値が 0 であること。(相対パスへフォールバックしたことの確認)
    EXPECT_EQ(CPLAT_OK,
              quiet_write); // [確認_正常系] - 全出力先無効の cplat_tracer_write() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        fallback_write); // [確認_異常系] - 不正時刻の cplat_tracer_write() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        os_fail_write); // [確認_異常系] - OS バックエンド書き込み失敗時の cplat_tracer_write() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        file_fail_write); // [確認_異常系] - file 失敗時の cplat_tracer_write() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        CPLAT_OK,
        hex_size_zero); // [確認_正常系] - size が 0 の test_tracer_hex_write_impl() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        hex_not_running); // [確認_異常系] - 停止中の cplat_tracer_write_hex_at() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        hexf_not_running); // [確認_異常系] - 停止中の cplat_tracer_write_hexf_at() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        CPLAT_OK,
        start_already_open); // [確認_正常系] - file ハンドルありの cplat_tracer_start() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        name_null); // [確認_異常系] - 出力先が nullptr の cplat_tracer_get_name() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        name_zero); // [確認_異常系] - サイズが 0 の cplat_tracer_get_name() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_BUFFER_TOO_SMALL,
        name_small); // [確認_異常系] - 2 バイト出力先の cplat_tracer_get_name() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_EQ(
        CPLAT_ERR_BUFFER_TOO_SMALL,
        name_snprintf); // [確認_異常系] - cplat_snprintf() 失敗時の cplat_tracer_get_name() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_EQ(
        CPLAT_OK,
        disable_open_file); // [確認_正常系] - 稼働中に file を無効化した cplat_tracer_set_file_level() の戻り値が CPLAT_OK であること。

    // Cleanup
    test_trace_registry_reset_shutdown_state();
    test_tracer_set_lifecycle_state(handle, 0);
    test_tracer_set_file_handle(handle, nullptr);
    cplat_tracer_dispose(&handle);
}
// [サブ手順参照 名前=traceCoverageTest.TearDown]

// ロック失敗、容量あふれ、caller-managed、二重 shutdown を充足することの確認
// [サブ手順参照 名前=traceCoverageTest.SetUp]
TEST_F(traceCoverageTest, remaining_lock_overflow_and_caller_managed_paths)
{
    // Arrange
    cplat_tracer *managed =
        cplat_tracer_create(CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED); // [状態] - tracer-managed ハンドルを用意する。
    ASSERT_NE(nullptr,
              managed); // [状態確認] - tracer-managed の cplat_tracer_create() の戻り値が nullptr 以外であること。
    cplat_tracer *caller =
        cplat_tracer_create(CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED); // [状態] - caller-managed ハンドルを用意する。
    ASSERT_NE(nullptr,
              caller); // [状態確認] - caller-managed の cplat_tracer_create() の戻り値が nullptr 以外であること。
    cplat_tracer *overflow_half = nullptr;
    cplat_tracer *overflow_max = nullptr;
    cplat_tracer *register_fail = nullptr;
    cplat_tracer *register_fail_caller = nullptr;
    cplat_tracer *shutdown_fail = nullptr;
    cplat_tracer *lock_clear = nullptr;
    cplat_tracer *empty = nullptr;
    cplat_shutdown_event first_event = {};
    cplat_shutdown_event event = {};
    cplat_shutdown_event lock_clear_event = {};
    size_t capacity_on_lock_fail = 99U;
    int exclusive_fail = CPLAT_OK;
    int caller_start = CPLAT_OK;
    int caller_write = CPLAT_OK;
    int dispose_lock_fail = 0;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_local_lock_lock(_, _))
        .WillOnce(Return(CPLAT_ERR_UNKNOWN))
        .WillOnce(Return(CPLAT_ERR_UNKNOWN))
        .WillOnce(Return(CPLAT_ERR_UNKNOWN))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - cplat_local_lock_lock() が 3 回以上呼び出されること。
    // [Pre-Assert手順] - cplat_local_lock_lock() から 1 回目から 3 回目は CPLAT_ERR_UNKNOWN を返し、以降は既定動作へ委譲する。

    // Act
    capacity_on_lock_fail =
        cplat_internal_trace_registry_capacity(); // [手順] - lock が失敗する状態で cplat_internal_trace_registry_capacity() を呼び出す。
    test_tracer_unregister(managed); // [手順] - lock が失敗する状態で test_tracer_unregister() を呼び出す。
    register_fail = cplat_tracer_create(
        CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED); // [手順] - lock が失敗する状態で、tracer-managed を指定して cplat_tracer_create() を呼び出す。
    test_trace_registry_set_counts((~(size_t)0) / 2U + 1U, (~(size_t)0) / 2U + 1U);
    overflow_max = cplat_tracer_create(
        CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED); // [手順] - 容量が SIZE_MAX/2 を超える状態で cplat_tracer_create() を呼び出す。
    test_trace_registry_set_counts((~(size_t)0) / 2U, (~(size_t)0) / 2U);
    overflow_half = cplat_tracer_create(
        CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED); // [手順] - 容量が SIZE_MAX/2 の状態で cplat_tracer_create() を呼び出す。
    test_trace_registry_set_counts(2U, 8U);
    caller_start =
        cplat_tracer_start(caller); // [手順] - caller-managed ハンドルを指定して cplat_tracer_start() を呼び出す。
    caller_write = cplat_tracer_write(
        caller, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "caller"); // [手順] - caller-managed ハンドルと INFO レベルと "caller" を指定して cplat_tracer_write() を呼び出す。

    // Pre-Assert_2
    EXPECT_CALL(mock_cplat, cplat_local_rwlock_lock_exclusive(_, _))
        .WillOnce(Return(CPLAT_ERR_UNKNOWN))
        .WillOnce(Return(CPLAT_ERR_UNKNOWN))
        .WillRepeatedly(
            DoDefault()); // [Pre-Assert確認_異常系] - cplat_local_rwlock_lock_exclusive() が 2 回以上呼び出されること。
    // [Pre-Assert手順] - cplat_local_rwlock_lock_exclusive() から 1 回目と 2 回目は CPLAT_ERR_UNKNOWN を返し、以降は既定動作へ委譲する。

    // Act_2
    exclusive_fail = cplat_tracer_set_stderr_level(
        managed,
        CPLAT_TRACE_LEVEL_DEBUG); // [手順] - 排他ロックが失敗する状態で、DEBUG レベルを指定して cplat_tracer_set_stderr_level() を呼び出す。
    cplat_tracer_dispose(&managed); // [手順] - 排他ロックが失敗する状態で cplat_tracer_dispose() を呼び出す。
    if (managed != nullptr)
    {
        dispose_lock_fail = 1;
        cplat_tracer_dispose(&managed);
    }
    (void)cplat_tracer_stop(caller);
    (void)cplat_tracer_set_hook(
        caller, coverage_hook,
        nullptr); // [手順] - shutdown 解放用の hook を指定して cplat_tracer_set_hook() を呼び出す。
    cplat_internal_trace_registry_dispose_all_on_shutdown(
        &first_event); // [手順] - hook 付き caller-managed を持つ状態で cplat_internal_trace_registry_dispose_all_on_shutdown() を呼び出す。
    test_trace_registry_reinit_lock();
    (void)cplat_internal_trace_registry_capacity();
    cplat_internal_trace_registry_dispose_all_on_shutdown(
        &event); // [手順] - すでに shutdown 済みのレジストリの状態で cplat_internal_trace_registry_dispose_all_on_shutdown() を呼び出す。
    test_trace_registry_reset_shutdown_state();
    cplat_tracer_dispose(&empty); // [手順] - nullptr のハンドル変数を指定して cplat_tracer_dispose() を呼び出す。

    // Pre-Assert_3
    EXPECT_CALL(mock_cplat, cplat_shutdown_register(_, _))
        .WillOnce(Return(CPLAT_ERR_UNKNOWN))
        .WillRepeatedly(
            DoDefault()); // [Pre-Assert確認_異常系] - cplat_shutdown_register() が 1 回以上呼び出されること。
    // [Pre-Assert手順] - cplat_shutdown_register() から 1 回目は CPLAT_ERR_UNKNOWN を返し、以降は既定動作へ委譲する。
    EXPECT_CALL(mock_cplat, cplat_process_get_executable_path(_, _))
        .WillOnce(
            [](char *path_out, size_t path_size)
            {
                test_trace_registry_set_shutdown_started(1U);
                snprintf(path_out, path_size, "%s", "/opt/bin/myapp");
                return CPLAT_OK;
            })
        .WillRepeatedly(
            DoDefault()); // [Pre-Assert確認_異常系] - cplat_process_get_executable_path() が 1 回以上呼び出されること。
    // [Pre-Assert手順] - cplat_process_get_executable_path() から、shutdown 開始フラグを立て、通常パスを格納して CPLAT_OK を返す。

    // Act_3
    shutdown_fail = cplat_tracer_create(
        CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED); // [手順] - shutdown の登録が失敗する状態で cplat_tracer_create() を呼び出す。
    test_trace_registry_reset_shutdown_state();
    register_fail_caller = cplat_tracer_create(
        CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED); // [手順] - 登録直前にシャットダウンが開始される状態で、caller-managed を指定して cplat_tracer_create() を呼び出す。
    test_trace_registry_reset_shutdown_state();
    lock_clear = cplat_tracer_create(CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED);
    ASSERT_NE(nullptr,
              lock_clear); // [状態確認] - file sink 解放用の cplat_tracer_create() の戻り値が nullptr 以外であること。
    test_tracer_set_file_handle(lock_clear, file_handle_);
    g_test_file_shutdown_hook = test_trace_registry_null_lock;
    cplat_internal_trace_registry_dispose_all_on_shutdown(
        &lock_clear_event); // [手順] - file sink の解放中にレジストリ lock を nullptr にする状態で cplat_internal_trace_registry_dispose_all_on_shutdown() を呼び出す。
    g_test_file_shutdown_hook = nullptr;
    lock_clear = nullptr;

    // Assert
    EXPECT_EQ(
        0U,
        capacity_on_lock_fail); // [確認_異常系] - lock 失敗時の cplat_internal_trace_registry_capacity() の戻り値が 0U であること。(容量を取得できないことの確認)
    EXPECT_EQ(nullptr,
              register_fail); // [確認_異常系] - lock 失敗時の cplat_tracer_create() の戻り値が nullptr であること。
    EXPECT_EQ(
        nullptr,
        overflow_max); // [確認_異常系] - 容量が SIZE_MAX/2 を超える状態での cplat_tracer_create() の戻り値が nullptr であること。
    EXPECT_EQ(
        nullptr,
        overflow_half); // [確認_異常系] - 容量が SIZE_MAX/2 の状態での cplat_tracer_create() の戻り値が nullptr であること。
    EXPECT_EQ(CPLAT_OK,
              caller_start); // [確認_正常系] - caller-managed の cplat_tracer_start() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              caller_write); // [確認_正常系] - caller-managed の cplat_tracer_write() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        exclusive_fail); // [確認_異常系] - 排他ロック失敗時の cplat_tracer_set_stderr_level() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        1,
        dispose_lock_fail); // [確認_異常系] - dispose_lock_fail が 1 であること。(排他ロック失敗の dispose がハンドルを残したことの確認)
    EXPECT_EQ(
        nullptr,
        shutdown_fail); // [確認_異常系] - shutdown 登録失敗時の cplat_tracer_create() の戻り値が nullptr であること。
    EXPECT_EQ(
        nullptr,
        register_fail_caller); // [確認_異常系] - 登録直前のシャットダウンでの caller-managed の cplat_tracer_create() の戻り値が nullptr であること。

    // Cleanup
    test_trace_registry_reset_shutdown_state();
}
// [サブ手順参照 名前=traceCoverageTest.TearDown]
