#include "traceFileRegistryTest.h"

#include <testfw.h>
#include <mock_cplat.h>
#include <cplat/trace/trace_file.h>
#include <cplat/crt/file.h>
#include <cplat/sync/sync.h>
#include <string>
#include <cstring>
#include <ctime>
#include <cstdio>

using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::HasSubstr;
using testing::InSequence;
using testing::NiceMock;
using testing::Return;
using testing::StrEq;

namespace
{

static void set_valid_deadline(struct timespec *abs_timeout)
{
    abs_timeout->tv_sec = (time_t)(time(nullptr) + 1);
    abs_timeout->tv_nsec = 0;
}

static void set_fixed_realtime(cplat_timespec *ts)
{
    ts->tv_sec = 1714100645LL;
    ts->tv_nsec = 678000000;
}

static cplat_timespec make_fixed_timestamp(void)
{
    cplat_timespec timestamp;
    timestamp.tv_sec = 1714100645LL;
    timestamp.tv_nsec = 678000000;
    return timestamp;
}

/* cplat_file_open() の flags 引数と製品コードの base_open_flags() に合わせて int を返す */
static int open_flags_default(void)
{
    return CPLAT_FILE_OPEN_CREATE | CPLAT_FILE_OPEN_APPEND | CPLAT_FILE_OPEN_WRITE_THROUGH;
}

static int open_flags_truncate(void)
{
    return open_flags_default() | CPLAT_FILE_OPEN_TRUNCATE;
}

static int open_flags_buffered(void)
{
    return CPLAT_FILE_OPEN_CREATE | CPLAT_FILE_OPEN_APPEND;
}

// 共有モード テストで使う既定のファイル同一性インデックス
constexpr uint64_t kDefaultFileIndex = 100;

static void set_file_id(cplat_file_id *id_out, uint64_t index)
{
    id_out->volume = 1;
    id_out->index = index;
}

} // namespace

class trace_fileTest : public traceFileRegistryTest
{
  protected:
    // [サブ手順 名前=trace_fileTest.SetUp]
    void SetUp() override
    {
        traceFileRegistryTest::SetUp(); // [サブ手順参照 名前=traceFileRegistryTest.SetUp]

        ON_CALL(mock_cplat, cplat_clock_get_realtime_deadline_ms(_, _))
            .WillByDefault(
                [](uint64_t, struct timespec *abs_timeout)
                {
                    set_valid_deadline(abs_timeout);
                    return CPLAT_OK;
                });

        // [状態] - cplat_clock_get_realtime_deadline_ms() から、有効な期限を設定して戻るように既定動作を設定する。
        ON_CALL(mock_cplat, cplat_clock_get_realtime(_))
            .WillByDefault(
                [](cplat_timespec *ts)
                {
                    set_fixed_realtime(ts);
                    return CPLAT_OK;
                });

        // [状態] - cplat_clock_get_realtime() から、固定の現在時刻を設定して戻るように既定動作を設定する。
        ON_CALL(mock_cplat, cplat_clock_format_realtime_iso8601_local(_, _, _))
            .WillByDefault(
                [](char *buf, size_t buf_size, const cplat_timespec *)
                {
                    snprintf(buf, buf_size, "%s", "2026-04-26T03:04:05.678+09:00");
                    return 0;
                });
        // [状態] - cplat_clock_format_realtime_iso8601_local() から、"2026-04-26T03:04:05.678+09:00" を格納して 0 を返すように既定動作を設定する。
        ON_CALL(mock_cplat, cplat_file_open(_, _, _, _)).WillByDefault(Return(CPLAT_OK));
        // [状態] - cplat_file_open() から CPLAT_OK を返すように既定動作を設定する。
        ON_CALL(mock_cplat, cplat_file_get_size(_, _, _))
            .WillByDefault(
                [](const cplat_file *, size_t *size_out, cplat_error *)
                {
                    *size_out = 0;
                    return 0;
                });
        // [状態] - cplat_file_get_size() から、サイズに 0 を設定して 0 を返すように既定動作を設定する。
        ON_CALL(mock_cplat, cplat_file_write(_, _, _, _)).WillByDefault(Return(CPLAT_OK));
        // [状態] - cplat_file_write() から CPLAT_OK を返すように既定動作を設定する。
        ON_CALL(mock_cplat, cplat_file_close(_, _)).WillByDefault(Return(CPLAT_OK));
        // [状態] - cplat_file_close() から CPLAT_OK を返すように既定動作を設定する。
        ON_CALL(mock_cplat, cplat_remove(_, _)).WillByDefault(Return(0));
        // [状態] - cplat_remove() から 0 を返すように既定動作を設定する。
        ON_CALL(mock_cplat, cplat_rename(_, _, _)).WillByDefault(Return(0));
        // [状態] - cplat_rename() から 0 を返すように既定動作を設定する。

        // 共有モード用の既定動作: ハンドルとパスの同一性は常に一致させる
        ON_CALL(mock_cplat, cplat_file_get_id(_, _, _))
            .WillByDefault(
                [](const cplat_file *, cplat_file_id *id_out, cplat_error *)
                {
                    set_file_id(id_out, kDefaultFileIndex);
                    return 0;
                });
        // [状態] - cplat_file_get_id() から、既定のファイル同一性を設定して 0 を返すように既定動作を設定する。
        ON_CALL(mock_cplat, cplat_file_get_path_id(_, _, _))
            .WillByDefault(
                [](const char *, cplat_file_id *id_out, cplat_error *)
                {
                    set_file_id(id_out, kDefaultFileIndex);
                    return 0;
                });
        // [状態] - cplat_file_get_path_id() から、既定のファイル同一性を設定して 0 を返すように既定動作を設定する。

        // プロセス間ロックはダミー ハンドルで成功させる (実体は作らない)
        ON_CALL(mock_cplat, cplat_interprocess_lock_open(_, _))
            .WillByDefault(
                [](const char *, cplat_interprocess_lock **lock)
                {
                    static int dummy_lock = 0;
                    *lock = (cplat_interprocess_lock *)&dummy_lock;
                    return CPLAT_OK;
                });
        // [状態] - cplat_interprocess_lock_open() から、ダミーのロックを設定して CPLAT_OK を返すように既定動作を設定する。
        ON_CALL(mock_cplat, cplat_interprocess_lock_try_lock(_)).WillByDefault(Return(CPLAT_OK));
        // [状態] - cplat_interprocess_lock_try_lock() から CPLAT_OK を返すように既定動作を設定する。
        ON_CALL(mock_cplat, cplat_interprocess_lock_unlock(_)).WillByDefault(Return(CPLAT_OK));
        // [状態] - cplat_interprocess_lock_unlock() から CPLAT_OK を返すように既定動作を設定する。
        ON_CALL(mock_cplat, cplat_interprocess_lock_dispose(_)).WillByDefault(Return());
        // [状態] - cplat_interprocess_lock_dispose() から戻るように既定動作を設定する。
    }
    // [サブ手順終了]
};

// NULL path では create が失敗することの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_create_returns_null_for_null_path)
{
    // Arrange

    // Pre-Assert

    // Act
    cplat_trace_file_sink *handle = cplat_trace_file_sink_create(
        nullptr, 0, 0, 0); // [手順] - path に nullptr を指定して cplat_trace_file_sink_create() を呼び出す。

    // Assert
    EXPECT_EQ(nullptr,
              handle); // [確認_異常系] - cplat_trace_file_sink_create() の戻り値が nullptr であること。
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

// create が既定 open flags でファイルを開くことの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_create_opens_file_with_default_flags)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_default(), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - cplat_file_open() が "trace.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_file_open() から 0 を返す。
    EXPECT_CALL(mock_cplat, cplat_file_get_size(_, _, _))
        .WillOnce(
            [](const cplat_file *, size_t *size_out, cplat_error *)
            {
                *size_out = 123;
                return 0;
            }); // [Pre-Assert確認_正常系] - cplat_file_get_size() が 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_file_get_size() から、サイズに 123 を設定して 0 を返す。

    // Act
    cplat_trace_file_sink *handle = cplat_trace_file_sink_create(
        "trace.log", 0, 0,
        0); // [手順] - パス "trace.log" と既定値を指定して cplat_trace_file_sink_create() を呼び出す。

    // Assert
    EXPECT_NE(nullptr,
              handle); // [確認_正常系] - cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。

    // Cleanup
    EXPECT_CALL(mock_cplat, cplat_file_close(_, _)).Times(AtLeast(1)); // dispose 時の close を許容する。
    // [Pre-Assert確認_正常系] - cplat_file_close() が 1 回以上呼び出されること。
    cplat_trace_file_sink_dispose(handle);
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

// OS バッファー指定時に write-through を付けずにファイルを開くことの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_create_opens_buffered_file_without_write_through)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("buffered.log"), open_flags_buffered(), _))
        .WillOnce(Return(
            CPLAT_OK)); // [Pre-Assert確認_正常系] - cplat_file_open() が "buffered.log" と等しい文字列、open_flags_buffered() の戻り値を指定して 1 回呼び出されること。(write-through を含まない flags で開いたことの確認)
                        // [Pre-Assert手順] - cplat_file_open() から CPLAT_OK を返す。

    // Act
    cplat_trace_file_sink *handle = cplat_trace_file_sink_create(
        "buffered.log", 0, 0,
        CPLAT_TRACE_FILE_SINK_OS_BUFFERED); // [手順] - パス "buffered.log" と OS バッファー モードを指定して cplat_trace_file_sink_create() を呼び出す。

    // Assert
    ASSERT_NE(nullptr,
              handle); // [確認_正常系] - cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

// create のファイル オープンが初回失敗後にリトライされることの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_create_retries_file_open_after_initial_failure)
{
    // Arrange

    // Pre-Assert
    InSequence seq;
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_default(), _))
        .WillOnce(Return(
            -1)); // [Pre-Assert確認_正常系] - 初回の cplat_file_open() が "trace.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。
                  // [Pre-Assert手順] - cplat_file_open() から -1 を返す。
    EXPECT_CALL(mock_cplat, cplat_sleep_ms(3000))
        .WillOnce(Return()); // [Pre-Assert確認_正常系] - cplat_sleep_ms() が 3000 を指定して 1 回呼び出されること。
                             // [Pre-Assert手順] - cplat_sleep_ms() から戻る。
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_default(), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - リトライの cplat_file_open() が "trace.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_file_open() から 0 を返す。
    EXPECT_CALL(mock_cplat, cplat_file_get_size(_, _, _))
        .WillOnce(
            [](const cplat_file *, size_t *size_out, cplat_error *)
            {
                *size_out = 0;
                return 0;
            }); // [Pre-Assert確認_正常系] - リトライ成功後の cplat_file_get_size() が 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_file_get_size() から、サイズに 0 を設定して 0 を返す。

    // Act
    cplat_trace_file_sink *handle = cplat_trace_file_sink_create(
        "trace.log", 0, 0,
        0); // [手順] - 初回の cplat_file_open() が失敗する状態で、パス "trace.log" を指定して cplat_trace_file_sink_create() を呼び出す。

    // Assert
    EXPECT_NE(nullptr,
              handle); // [確認_正常系] - cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。

    // Cleanup
    EXPECT_CALL(mock_cplat, cplat_file_close(_, _)).Times(AtLeast(1)); // dispose 時の close を許容する。
    // [Pre-Assert確認_異常系] - cplat_file_close() が 1 回以上呼び出されること。
    cplat_trace_file_sink_dispose(handle);
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

// create のファイル オープンがリトライ上限まで失敗した場合に NULL を返すことの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_create_returns_null_after_file_open_retry_exhausted)
{
    // Arrange

    // Pre-Assert
    InSequence seq;
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_default(), _))
        .WillOnce(Return(
            -1)); // [Pre-Assert確認_異常系] - 初回の cplat_file_open() が "trace.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。
                  // [Pre-Assert手順] - cplat_file_open() から -1 を返す。
    EXPECT_CALL(mock_cplat, cplat_sleep_ms(3000))
        .WillOnce(
            Return()); // [Pre-Assert確認_異常系] - 1 回目のリトライ前に cplat_sleep_ms() が 3000 を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - cplat_sleep_ms() から戻る。
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_default(), _))
        .WillOnce(Return(
            -1)); // [Pre-Assert確認_異常系] - 1 回目のリトライの cplat_file_open() が "trace.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。
                  // [Pre-Assert手順] - cplat_file_open() から -1 を返す。
    EXPECT_CALL(mock_cplat, cplat_sleep_ms(3000))
        .WillOnce(
            Return()); // [Pre-Assert確認_異常系] - 2 回目のリトライ前に cplat_sleep_ms() が 3000 を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - cplat_sleep_ms() から戻る。
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_default(), _))
        .WillOnce(Return(
            -1)); // [Pre-Assert確認_異常系] - 2 回目のリトライの cplat_file_open() が "trace.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。
                  // [Pre-Assert手順] - cplat_file_open() から -1 を返す。
    EXPECT_CALL(mock_cplat, cplat_sleep_ms(3000))
        .WillOnce(
            Return()); // [Pre-Assert確認_異常系] - 3 回目のリトライ前に cplat_sleep_ms() が 3000 を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - cplat_sleep_ms() から戻る。
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_default(), _))
        .WillOnce(Return(
            -1)); // [Pre-Assert確認_異常系] - 3 回目のリトライの cplat_file_open() が "trace.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。
                  // [Pre-Assert手順] - cplat_file_open() から -1 を返す。
    EXPECT_CALL(mock_cplat, cplat_file_close(_, _))
        .Times(AtLeast(
            1)); // [Pre-Assert確認_異常系] - cplat_file_close() が 1 回以上呼び出されること。(失敗経路で開いたファイルを閉じることの確認)

    // Act
    cplat_trace_file_sink *handle = cplat_trace_file_sink_create(
        "trace.log", 0, 0,
        0); // [手順] - cplat_file_open() が失敗し続ける状態で、パス "trace.log" を指定して cplat_trace_file_sink_create() を呼び出す。

    // Assert
    EXPECT_EQ(nullptr,
              handle); // [確認_異常系] - cplat_trace_file_sink_create() の戻り値が nullptr であること。
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

// INFO 行が固定タイムスタンプと I marker で書き込まれることの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_write_formats_info_line)
{
    // Arrange
    cplat_trace_file_sink *handle =
        cplat_trace_file_sink_create("trace.log", 0, 0, 0); // [状態] - 初期化済みの file sink を用意する。
    ASSERT_NE(nullptr,
              handle); // [状態確認] - cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_file_write(_, _, _, _))
        .WillOnce(
            [](cplat_file *, const void *buf, size_t len, cplat_error *)
            {
                std::string actual((const char *)buf, len);
                EXPECT_EQ("2026-04-26T03:04:05.678+09:00 I hello\n", actual);
                // [Pre-Assert確認_正常系] - actual が "2026-04-26T03:04:05.678+09:00 I hello\n" であること。
                return 0;
            }); // [Pre-Assert確認_正常系] - cplat_file_write() が 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_file_write() から、書式化した行を確認して 0 を返す。

    // Act
    int result = cplat_trace_file_sink_write(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "hello"); // [手順] - INFO レベルと "hello" を指定して cplat_trace_file_sink_write() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_trace_file_sink_write() の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

// DEBUG 行が D marker で書き込まれることの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_write_formats_debug_marker)
{
    // Arrange
    cplat_trace_file_sink *handle =
        cplat_trace_file_sink_create("trace.log", 0, 0, 0); // [状態] - 初期化済みの file sink を用意する。
    ASSERT_NE(nullptr,
              handle); // [状態確認] - cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_file_write(_, _, _, _))
        .WillOnce(
            [](cplat_file *, const void *buf, size_t len, cplat_error *)
            {
                std::string actual((const char *)buf, len);
                EXPECT_EQ("2026-04-26T03:04:05.678+09:00 D debug line\n", actual);
                // [Pre-Assert確認_正常系] - actual が "2026-04-26T03:04:05.678+09:00 D debug line\n" であること。
                return 0;
            }); // [Pre-Assert確認_正常系] - cplat_file_write() が 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_file_write() から、D marker の行を確認して 0 を返す。

    // Act
    int result = cplat_trace_file_sink_write(
        handle, CPLAT_TRACE_LEVEL_DEBUG, nullptr,
        "debug line"); // [手順] - DEBUG レベルと "debug line" を指定して cplat_trace_file_sink_write() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_trace_file_sink_write() の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

// 明示タイムスタンプ指定時に内部の現在時刻取得を行わずに書き込むことの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_write_uses_explicit_timestamp_without_internal_clock)
{
    // Arrange
    cplat_trace_file_sink *handle =
        cplat_trace_file_sink_create("trace.log", 0, 0, 0); // [状態] - 初期化済みの file sink を用意する。
    cplat_timespec timestamp = make_fixed_timestamp();
    ASSERT_NE(nullptr,
              handle); // [状態確認] - cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_clock_get_realtime(_))
        .Times(
            0); // [Pre-Assert確認_正常系] - 明示タイムスタンプ指定時に cplat_clock_get_realtime() が呼び出されないこと。
    EXPECT_CALL(mock_cplat, cplat_file_write(_, _, _, _))
        .WillOnce(
            [](cplat_file *, const void *buf, size_t len, cplat_error *)
            {
                std::string actual((const char *)buf, len);
                EXPECT_EQ("2026-04-26T03:04:05.678+09:00 I explicit hello\n", actual);
                // [Pre-Assert確認_正常系] - actual が "2026-04-26T03:04:05.678+09:00 I explicit hello\n" であること。
                return 0;
            }); // [Pre-Assert確認_正常系] - cplat_file_write() が 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_file_write() から、書式化した行を確認して 0 を返す。

    // Act
    int result = cplat_trace_file_sink_write(
        handle, CPLAT_TRACE_LEVEL_INFO, &timestamp,
        "explicit hello"); // [手順] - 明示タイムスタンプと "explicit hello" を指定して cplat_trace_file_sink_write() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        result); // [確認_正常系] - 明示タイムスタンプ付きの cplat_trace_file_sink_write() の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

// ファイル書き込み失敗時に -1 が返ることの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_write_returns_minus_one_on_file_error)
{
    // Arrange
    cplat_trace_file_sink *handle =
        cplat_trace_file_sink_create("trace.log", 0, 0, 0); // [状態] - 初期化済みの file sink を用意する。
    ASSERT_NE(nullptr,
              handle); // [状態確認] - cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_file_write(_, _, _, _))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - cplat_file_write() が 1 回呼び出されること。
                               // [Pre-Assert手順] - cplat_file_write() から -1 を返す。

    // Act
    int result = cplat_trace_file_sink_write(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "write error"); // [手順] - cplat_file_write() が失敗する状態で、"write error" を指定して cplat_trace_file_sink_write() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - cplat_trace_file_sink_write() の戻り値が CPLAT_ERR_UNKNOWN であること。

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

// 不正な明示タイムスタンプ指定時に現在時刻へ代替して書き込みつつ -1 を返すことの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_write_falls_back_from_invalid_explicit_timestamp)
{
    // Arrange
    cplat_trace_file_sink *handle =
        cplat_trace_file_sink_create("trace.log", 0, 0, 0); // [状態] - 初期化済みの file sink を用意する。
    cplat_timespec invalid_timestamp = {1714100645LL, 1000000000};
    ASSERT_NE(nullptr,
              handle); // [状態確認] - cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_clock_get_realtime(_))
        .Times(
            1); // [Pre-Assert確認_異常系] - 不正な明示タイムスタンプのとき cplat_clock_get_realtime() が 1 回呼び出されること。(現在時刻へ代替したことの確認)
    EXPECT_CALL(mock_cplat, cplat_file_write(_, _, _, _))
        .WillOnce(
            [](cplat_file *, const void *buf, size_t len, cplat_error *)
            {
                std::string actual((const char *)buf, len);
                EXPECT_EQ("2026-04-26T03:04:05.678+09:00 I invalid\n", actual);
                // [Pre-Assert確認_正常系] - actual が "2026-04-26T03:04:05.678+09:00 I invalid\n" であること。
                return 0;
            }); // [Pre-Assert確認_異常系] - cplat_file_write() が 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_file_write() から、代替時刻の行を確認して 0 を返す。

    // Act
    int result = cplat_trace_file_sink_write(
        handle, CPLAT_TRACE_LEVEL_INFO, &invalid_timestamp,
        "invalid"); // [手順] - 不正なタイムスタンプと "invalid" を指定して cplat_trace_file_sink_write() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        result); // [確認_異常系] - 代替出力後の cplat_trace_file_sink_write() の戻り値が CPLAT_ERR_UNKNOWN であること。

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

// サイズ上限超過時にローテーションが実行されることの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_write_rotates_when_size_limit_is_reached)
{
    // Arrange

    // Pre-Assert
    InSequence seq;

    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_default(), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - create 時に cplat_file_open() が "trace.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_file_open() から 0 を返す。
    EXPECT_CALL(mock_cplat, cplat_file_get_size(_, _, _))
        .WillOnce(
            [](const cplat_file *, size_t *size_out, cplat_error *)
            {
                *size_out = 0;
                return 0;
            }); // [Pre-Assert確認_正常系] - create 時に cplat_file_get_size() が 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_file_get_size() から、size_out に 0 を設定して 0 を返す。

    // Arrange
    cplat_trace_file_sink *handle =
        cplat_trace_file_sink_create("trace.log", 1, 2, 0); // [状態] - ローテーション設定付きの file sink を用意する。
    ASSERT_NE(nullptr,
              handle); // [状態確認] - cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_file_write(_, _, _, _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - 元ファイルへの書き込みで cplat_file_write() が 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_file_write() から 0 を返す。
    EXPECT_CALL(mock_cplat, cplat_file_close(_, _)).Times(1); // ローテーション前の close
    // [Pre-Assert確認_正常系] - ローテーション前に cplat_file_close() が 1 回呼び出されること。
    EXPECT_CALL(mock_cplat, cplat_remove(StrEq("trace.log.2"), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - cplat_remove() が "trace.log.2" と等しい文字列を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_remove() から 0 を返す。
    EXPECT_CALL(mock_cplat, cplat_rename(StrEq("trace.log.1"), StrEq("trace.log.2"), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - cplat_rename() が "trace.log.1" と等しい文字列、"trace.log.2" と等しい文字列を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_rename() から 0 を返す。
    EXPECT_CALL(mock_cplat, cplat_rename(StrEq("trace.log"), StrEq("trace.log.1"), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - cplat_rename() が "trace.log" と等しい文字列、"trace.log.1" と等しい文字列を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_rename() から、現在ファイルに対して 0 を返す。
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_truncate(), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - ローテーション後の cplat_file_open() が "trace.log" と等しい文字列、open_flags_truncate() の戻り値を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_file_open() から、truncate 付きの open に対して 0 を返す。
    EXPECT_CALL(mock_cplat, cplat_file_close(_, _)).Times(1); // dispose 分
    // [Pre-Assert確認_正常系] - dispose 時に cplat_file_close() が 1 回呼び出されること。

    // Act
    int result = cplat_trace_file_sink_write(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "rotate me"); // [手順] - 上限 1 byte の sink と "rotate me" を指定して cplat_trace_file_sink_write() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        result); // [確認_正常系] - cplat_trace_file_sink_write() の戻り値が CPLAT_OK であること。(書き込み後にローテーションが完了したことの確認)

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

// 単一プロセス モードのローテーション後にファイル オープンが失敗した場合、次の書き込みで追記モードで開き直すことの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_write_reopens_after_rotation_open_failure)
{
    // Arrange

    // Pre-Assert
    {
        InSequence seq;

        EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_default(), _))
            .WillOnce(Return(
                0)); // [Pre-Assert確認_正常系] - create 時に cplat_file_open() が "trace.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。
                     // [Pre-Assert手順] - cplat_file_open() から 0 を返す。
        EXPECT_CALL(mock_cplat, cplat_file_get_size(_, _, _))
            .WillOnce(
                [](const cplat_file *, size_t *size_out, cplat_error *)
                {
                    *size_out = 0;
                    return 0;
                }); // [Pre-Assert確認_正常系] - create 時に cplat_file_get_size() が 1 回呼び出されること。
                    // [Pre-Assert手順] - cplat_file_get_size() から、size_out に 0 を設定して 0 を返す。
    }

    // Arrange
    const std::string first_line(30U, 'r');
    cplat_trace_file_sink *handle =
        cplat_trace_file_sink_create("trace.log", 60, 2, 0); // [状態] - ローテーション設定付きの file sink を用意する。
    ASSERT_NE(nullptr,
              handle); // [状態確認] - cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。

    // Pre-Assert
    {
        InSequence seq;
        EXPECT_CALL(mock_cplat, cplat_file_write(_, _, _, _))
            .WillOnce(
                Return(0)); // [Pre-Assert確認_異常系] - 1 回目の書き込みの cplat_file_write() が 1 回呼び出されること。
                            // [Pre-Assert手順] - cplat_file_write() から 0 を返す。
        EXPECT_CALL(mock_cplat, cplat_file_close(_, _)).Times(1); // ローテーション前の close
        // [Pre-Assert確認_正常系] - ローテーション前に cplat_file_close() が 1 回呼び出されること。
        EXPECT_CALL(mock_cplat, cplat_remove(StrEq("trace.log.2"), _))
            .WillOnce(Return(
                0)); // [Pre-Assert確認_異常系] - cplat_remove() が "trace.log.2" と等しい文字列を指定して 1 回呼び出されること。
                     // [Pre-Assert手順] - cplat_remove() から 0 を返す。
        EXPECT_CALL(mock_cplat, cplat_rename(StrEq("trace.log.1"), StrEq("trace.log.2"), _))
            .WillOnce(Return(
                0)); // [Pre-Assert確認_異常系] - cplat_rename() が "trace.log.1" と等しい文字列、"trace.log.2" と等しい文字列を指定して 1 回呼び出されること。
                     // [Pre-Assert手順] - cplat_rename() から、旧世代に対して 0 を返す。
        EXPECT_CALL(mock_cplat, cplat_rename(StrEq("trace.log"), StrEq("trace.log.1"), _))
            .WillOnce(Return(
                0)); // [Pre-Assert確認_異常系] - cplat_rename() が "trace.log" と等しい文字列、"trace.log.1" と等しい文字列を指定して 1 回呼び出されること。
                     // [Pre-Assert手順] - cplat_rename() から、現在ファイルに対して 0 を返す。
        EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_truncate(), _))
            .WillOnce(Return(
                -1)); // [Pre-Assert確認_異常系] - ローテーション後の cplat_file_open() が "trace.log" と等しい文字列、open_flags_truncate() の戻り値を指定して 1 回呼び出されること。
                      // [Pre-Assert手順] - cplat_file_open() から、truncate 付きの open に対して -1 を返す。
        EXPECT_CALL(mock_cplat, cplat_file_close(_, _))
            .Times(1); // [Pre-Assert確認_正常系] - 2 回目の開き直し前に cplat_file_close() が 1 回呼び出されること。
        EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_default(), _))
            .WillOnce(Return(
                0)); // [Pre-Assert確認_正常系] - 2 回目の書き込みで cplat_file_open() が "trace.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。(追記モードで開き直すことの確認)
                     // [Pre-Assert手順] - cplat_file_open() から 0 を返す。
        EXPECT_CALL(mock_cplat, cplat_file_get_size(_, _, _))
            .WillOnce(
                [](const cplat_file *, size_t *size_out, cplat_error *)
                {
                    *size_out = 0;
                    return 0;
                }); // [Pre-Assert確認_正常系] - 開き直し時に cplat_file_get_size() が 1 回呼び出されること。
                    // [Pre-Assert手順] - cplat_file_get_size() から、size_out に 0 を設定して 0 を返す。
        EXPECT_CALL(mock_cplat, cplat_file_write(_, _, _, _))
            .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 2 回目の書き込みの cplat_file_write() が 1 回呼び出されること。
                                  // [Pre-Assert手順] - cplat_file_write() から 0 を返す。
        EXPECT_CALL(mock_cplat, cplat_file_close(_, _)).Times(1); // dispose 分
        // [Pre-Assert確認_正常系] - dispose 時に cplat_file_close() が 1 回呼び出されること。
    }
    EXPECT_CALL(mock_cplat, cplat_sleep_ms(_))
        .Times(
            0); // [Pre-Assert確認_異常系] - ローテーション後のオープン失敗時および開き直し時に cplat_sleep_ms() が呼び出されないこと。

    // Act
    int actual_ret_first = cplat_trace_file_sink_write(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
        first_line.c_str()); // [手順] - ローテーション後の cplat_file_open() が失敗する状態で、上限 60 byte の sink へ 63 byte の行を書き込む。
    int actual_ret_second = cplat_trace_file_sink_write(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "reopen write"); // [手順] - 未オープン状態の sink に "reopen write" を指定して cplat_trace_file_sink_write() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_first); // [確認_異常系] - 1 回目の cplat_trace_file_sink_write() の戻り値が CPLAT_OK であること。(完了済みの書き込み結果であることの確認)
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_second); // [確認_正常系] - 2 回目の cplat_trace_file_sink_write() の戻り値が CPLAT_OK であること。(開き直し後に書き込めたことの確認)

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

// dispose が NULL ハンドルでも安全であることの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_dispose_with_null_handle_is_safe)
{
    // Arrange

    // Pre-Assert

    // Act
    cplat_trace_file_sink_dispose(
        nullptr); // [手順] - handle に nullptr を指定して cplat_trace_file_sink_dispose() を呼び出す。

    // Assert
    SUCCEED(); // [確認_正常系] - SUCCEED() に到達すること。(nullptr を渡した cplat_trace_file_sink_dispose() がクラッシュしないことの確認)
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

// パスに区切り文字が含まれる場合に makedirs が親ディレクトリ パスで呼ばれることの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_create_calls_makedirs_for_path_with_separator)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_makedirs(StrEq("sub"), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - cplat_makedirs() が "sub" と等しい文字列を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_makedirs() から 0 を返す。
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("sub/trace.log"), open_flags_default(), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - 親ディレクトリの作成後に cplat_file_open() が "sub/trace.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_file_open() から 0 を返す。

    // Act
    cplat_trace_file_sink *handle = cplat_trace_file_sink_create(
        "sub/trace.log", 0, 0,
        0); // [手順] - 区切り文字を含むパス "sub/trace.log" を指定して cplat_trace_file_sink_create() を呼び出す。

    // Assert
    EXPECT_NE(nullptr,
              handle); // [確認_正常系] - cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。

    // Cleanup
    EXPECT_CALL(mock_cplat, cplat_file_close(_, _)).Times(AtLeast(1)); // dispose 時の close を許容する。
    // [Pre-Assert確認_正常系] - cplat_file_close() が 1 回以上呼び出されること。
    cplat_trace_file_sink_dispose(handle);
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

#if defined(PLATFORM_WINDOWS)
// Windows スタイル区切りのパスでも makedirs が親ディレクトリ パスで呼ばれることの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_create_normalizes_windows_separator_for_parent_directory)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_makedirs(StrEq("sub/dir"), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - cplat_makedirs() が "sub/dir" と等しい文字列を指定して 1 回呼び出されること。(区切り文字を正規化した親ディレクトリであることの確認)
                 // [Pre-Assert手順] - cplat_makedirs() から 0 を返す。
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("sub\\dir\\trace.log"), open_flags_default(), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - cplat_file_open() が "sub\\dir\\trace.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。(呼び出し元のパス文字列のまま開くことの確認)
                 // [Pre-Assert手順] - cplat_file_open() から 0 を返す。

    // Act
    cplat_trace_file_sink *handle = cplat_trace_file_sink_create("sub\\dir\\trace.log", 0, 0, 0);

    // Assert
    EXPECT_NE(nullptr,
              handle); // [確認_正常系] - cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。

    // Cleanup
    EXPECT_CALL(mock_cplat, cplat_file_close(_, _)).Times(AtLeast(1)); // dispose 時の close を許容する。
    // [Pre-Assert確認_正常系] - cplat_file_close() が 1 回以上呼び出されること。
    cplat_trace_file_sink_dispose(handle);
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]
#endif /* PLATFORM_WINDOWS */

// 単一プロセス モード (flags 0) ではプロセス間ロックも同一性チェックも使わないことの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_single_mode_does_not_use_interprocess_lock_or_identity_check)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_interprocess_lock_open(_, _))
        .Times(0); // [Pre-Assert確認_正常系] - cplat_interprocess_lock_open() が呼び出されないこと。
    EXPECT_CALL(mock_cplat, cplat_file_get_path_id(_, _, _))
        .Times(
            0); // [Pre-Assert確認_正常系] - cplat_file_get_path_id() が呼び出されないこと。(パスの同一性を確認しないことの確認)
    EXPECT_CALL(mock_cplat, cplat_file_get_id(_, _, _))
        .Times(
            0); // [Pre-Assert確認_正常系] - cplat_file_get_id() が呼び出されないこと。(ハンドルの同一性を取得しないことの確認)

    // Act
    cplat_trace_file_sink *handle = cplat_trace_file_sink_create(
        "trace.log", 0, 0, 0); // [手順] - flags に 0 を指定して cplat_trace_file_sink_create() を呼び出す。
    int result = cplat_trace_file_sink_write(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "single"); // [手順] - "single" を指定して cplat_trace_file_sink_write() を呼び出す。

    // Assert
    EXPECT_NE(nullptr,
              handle); // [確認_正常系] - cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_trace_file_sink_write() の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

// 共有モードの create がロック ファイルを開くことの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_create_shared_opens_lock_file)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_default(), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - cplat_file_open() が "trace.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_file_open() から 0 を返す。
    EXPECT_CALL(mock_cplat, cplat_interprocess_lock_open(StrEq("trace.log.lock"), _))
        .Times(
            1); // [Pre-Assert確認_正常系] - cplat_interprocess_lock_open() が "trace.log.lock" と等しい文字列を指定して 1 回呼び出されること。

    // Act
    cplat_trace_file_sink *handle = cplat_trace_file_sink_create(
        "trace.log", 0, 0,
        CPLAT_TRACE_FILE_SINK_SHARED); // [手順] - 共有モードを指定して cplat_trace_file_sink_create() を呼び出す。

    // Assert
    EXPECT_NE(nullptr,
              handle); // [確認_正常系] - cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。

    // Cleanup
    EXPECT_CALL(mock_cplat, cplat_file_close(_, _)).Times(AtLeast(1)); // dispose 時の close を許容する。
    // [Pre-Assert確認_正常系] - cplat_file_close() が 1 回以上呼び出されること。
    EXPECT_CALL(mock_cplat, cplat_interprocess_lock_dispose(_)).Times(1); // dispose 時のプロセス間ロック破棄
    // [Pre-Assert確認_正常系] - cplat_interprocess_lock_dispose() が 1 回呼び出されること。
    cplat_trace_file_sink_dispose(handle);
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

// 共有モードでプロセス間ロックのオープンに失敗した場合に create が NULL を返すことの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_create_shared_returns_null_when_lock_open_fails)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_interprocess_lock_open(StrEq("trace.log.lock"), _))
        .WillOnce(Return(
            CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_異常系] - cplat_interprocess_lock_open() が "trace.log.lock" と等しい文字列を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_interprocess_lock_open() から CPLAT_ERR_UNKNOWN を返す。
    EXPECT_CALL(mock_cplat, cplat_file_close(_, _))
        .Times(AtLeast(
            1)); // [Pre-Assert確認_異常系] - cplat_file_close() が 1 回以上呼び出されること。(オープン済みファイルを閉じることの確認)

    // Act
    cplat_trace_file_sink *handle = cplat_trace_file_sink_create(
        "trace.log", 0, 0,
        CPLAT_TRACE_FILE_SINK_SHARED); // [手順] - 共有モードを指定して cplat_trace_file_sink_create() を呼び出す。

    // Assert
    EXPECT_EQ(nullptr,
              handle); // [確認_異常系] - cplat_trace_file_sink_create() の戻り値が nullptr であること。
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

// 負の flags では create が NULL を返すことの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_create_returns_null_for_negative_flags)
{
    // Arrange

    // Pre-Assert

    // Act
    cplat_trace_file_sink *handle = cplat_trace_file_sink_create(
        "trace.log", 0, 0, -1); // [手順] - flags に -1 を指定して cplat_trace_file_sink_create() を呼び出す。

    // Assert
    EXPECT_EQ(nullptr,
              handle); // [確認_異常系] - cplat_trace_file_sink_create() の戻り値が nullptr であること。
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

// 他プロセスのローテーションで path の実体が変わった場合に書き込み前に開き直すことの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_shared_write_reopens_after_external_rotation)
{
    // Arrange
    cplat_trace_file_sink *handle = cplat_trace_file_sink_create(
        "trace.log", 0, 0, CPLAT_TRACE_FILE_SINK_SHARED); // [状態] - 共有モードの file sink を用意する。
    ASSERT_NE(nullptr,
              handle); // [状態確認] - cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。

    // Pre-Assert
    InSequence seq;
    EXPECT_CALL(mock_cplat, cplat_file_get_path_id(StrEq("trace.log"), _, _))
        .WillOnce(
            [](const char *, cplat_file_id *id_out, cplat_error *)
            {
                set_file_id(id_out, kDefaultFileIndex + 1); // 別実体を示す
                return 0;
            }); // [Pre-Assert確認_正常系] - cplat_file_get_path_id() が "trace.log" と等しい文字列を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_file_get_path_id() から、別のファイル同一性を設定して 0 を返す。
    EXPECT_CALL(mock_cplat, cplat_file_close(_, _))
        .Times(1); // [Pre-Assert確認_正常系] - 旧ハンドルの cplat_file_close() が 1 回呼び出されること。
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_default(), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - 開き直しの cplat_file_open() が "trace.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_file_open() から、開き直しに対して 0 を返す。
    EXPECT_CALL(mock_cplat, cplat_file_write(_, _, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 開き直し後の cplat_file_write() が 1 回呼び出されること。
                              // [Pre-Assert手順] - cplat_file_write() から、開き直し後の呼び出しに対して 0 を返す。
    EXPECT_CALL(mock_cplat, cplat_file_close(_, _)).Times(1); // dispose 分
    // [Pre-Assert確認_正常系] - dispose 時に cplat_file_close() が 1 回呼び出されること。

    // Act
    int result = cplat_trace_file_sink_write(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "after rotate"); // [手順] - "after rotate" を指定して cplat_trace_file_sink_write() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_trace_file_sink_write() の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

// 共有モードの開き直しでファイル オープンに失敗した場合に再試行しないことの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_shared_write_reopen_does_not_retry_after_external_rotation)
{
    // Arrange
    cplat_trace_file_sink *handle = cplat_trace_file_sink_create(
        "trace.log", 0, 0, CPLAT_TRACE_FILE_SINK_SHARED); // [状態] - 共有モードの file sink を用意する。
    ASSERT_NE(nullptr,
              handle); // [状態確認] - cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。

    // Pre-Assert
    {
        InSequence seq;
        EXPECT_CALL(mock_cplat, cplat_file_get_path_id(StrEq("trace.log"), _, _))
            .WillOnce(
                [](const char *, cplat_file_id *id_out, cplat_error *)
                {
                    set_file_id(id_out, kDefaultFileIndex + 1);
                    return 0;
                }); // [Pre-Assert確認_異常系] - cplat_file_get_path_id() が "trace.log" と等しい文字列を指定して 1 回呼び出されること。
        // [Pre-Assert手順] - cplat_file_get_path_id() から、別のファイル同一性を設定して 0 を返す。
        EXPECT_CALL(mock_cplat, cplat_file_close(_, _))
            .Times(1); // [Pre-Assert確認_異常系] - 旧ハンドルの cplat_file_close() が 1 回呼び出されること。
        EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_default(), _))
            .WillOnce(Return(
                -1)); // [Pre-Assert確認_異常系] - 開き直しの cplat_file_open() が "trace.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。
                      // [Pre-Assert手順] - cplat_file_open() から -1 を返す。
        EXPECT_CALL(mock_cplat, cplat_file_close(_, _)).Times(1); // dispose 分
        // [Pre-Assert確認_正常系] - dispose 時に cplat_file_close() が 1 回呼び出されること。
    }
    EXPECT_CALL(mock_cplat, cplat_sleep_ms(_))
        .Times(0); // [Pre-Assert確認_異常系] - ファイル オープンの失敗後に cplat_sleep_ms() が呼び出されないこと。
    EXPECT_CALL(mock_cplat, cplat_file_write(_, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - ファイルを開き直せない場合に cplat_file_write() が呼び出されないこと。

    // Act
    int result = cplat_trace_file_sink_write(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "after rotate"); // [手順] - "after rotate" を指定して cplat_trace_file_sink_write() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        result); // [確認_異常系] - cplat_trace_file_sink_write() の戻り値が CPLAT_ERR_UNKNOWN であること。(再試行せず直ちに失敗したことの確認)

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

// 共有モードで実サイズがしきい値以上のときプロセス間ロック下でローテーションすることの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_shared_write_rotates_under_interprocess_lock)
{
    // Arrange

    // Pre-Assert
    InSequence seq;

    // create: オープン → サイズ取得 → 同一性キャッシュ → ロック ファイル オープン
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_default(), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - create 時に cplat_file_open() が "trace.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_file_open() から 0 を返す。
    EXPECT_CALL(mock_cplat, cplat_file_get_size(_, _, _))
        .WillOnce(
            [](const cplat_file *, size_t *size_out, cplat_error *)
            {
                *size_out = 0;
                return 0;
            }); // [Pre-Assert確認_正常系] - create 時に cplat_file_get_size() が 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_file_get_size() から、size_out に 0 を設定して 0 を返す。
    EXPECT_CALL(mock_cplat, cplat_file_get_id(_, _, _))
        .Times(1); // [Pre-Assert確認_正常系] - create 時に cplat_file_get_id() が 1 回呼び出されること。
    EXPECT_CALL(mock_cplat, cplat_interprocess_lock_open(StrEq("trace.log.lock"), _))
        .Times(
            1); // [Pre-Assert確認_正常系] - create 時に cplat_interprocess_lock_open() が "trace.log.lock" と等しい文字列を指定して 1 回呼び出されること。

    // Arrange
    cplat_trace_file_sink *handle = cplat_trace_file_sink_create(
        "trace.log", 1, 2, CPLAT_TRACE_FILE_SINK_SHARED); // [状態] - 共有モードの file sink を用意する。
    ASSERT_NE(nullptr,
              handle); // [状態確認] - cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。

    // Pre-Assert
    // write: 同一性チェック → 書き込み → 実サイズ超過
    EXPECT_CALL(mock_cplat, cplat_file_get_path_id(StrEq("trace.log"), _, _)).Times(1); // 書き込み前の同一性チェック
    // [Pre-Assert確認_正常系] - 書き込み前に cplat_file_get_path_id() が "trace.log" と等しい文字列を指定して 1 回呼び出されること。
    EXPECT_CALL(mock_cplat, cplat_file_write(_, _, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - cplat_file_write() が 1 回呼び出されること。
                              // [Pre-Assert手順] - cplat_file_write() から 0 を返す。
    EXPECT_CALL(mock_cplat, cplat_file_get_size(_, _, _))
        .WillOnce(
            [](const cplat_file *, size_t *size_out, cplat_error *)
            {
                *size_out = 10;
                return 0;
            }); // [Pre-Assert確認_正常系] - 書き込み後の cplat_file_get_size() が 1 回呼び出されること。(実サイズがしきい値以上であることの確認)
                // [Pre-Assert手順] - cplat_file_get_size() から、サイズに 10 を設定して 0 を返す。

    // プロセス間ロック下の再確認 → ローテーション
    EXPECT_CALL(mock_cplat, cplat_interprocess_lock_try_lock(_))
        .WillOnce(Return(
            CPLAT_OK)); // [Pre-Assert確認_正常系] - ローテーション前に cplat_interprocess_lock_try_lock() が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_interprocess_lock_try_lock() から CPLAT_OK を返す。
    EXPECT_CALL(mock_cplat, cplat_file_get_path_id(StrEq("trace.log"), _, _)).Times(1); // ロック下の同一性再確認
    // [Pre-Assert確認_正常系] - ロック下に cplat_file_get_path_id() が "trace.log" と等しい文字列を指定して 1 回呼び出されること。
    EXPECT_CALL(mock_cplat, cplat_file_get_size(_, _, _))
        .WillOnce(
            [](const cplat_file *, size_t *size_out, cplat_error *)
            {
                *size_out = 10;
                return 0;
            }); // [Pre-Assert確認_正常系] - ロック下の cplat_file_get_size() が 1 回呼び出されること。(実サイズを再確認したことの確認)
    // [Pre-Assert手順] - cplat_file_get_size() から、ロック下のサイズとして 10 を設定して 0 を返す。
    EXPECT_CALL(mock_cplat, cplat_file_close(_, _)).Times(1); // ローテーション前の close
    // [Pre-Assert確認_正常系] - ローテーション前に cplat_file_close() が 1 回呼び出されること。
    EXPECT_CALL(mock_cplat, cplat_remove(StrEq("trace.log.2"), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - cplat_remove() が "trace.log.2" と等しい文字列を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_remove() から 0 を返す。
    EXPECT_CALL(mock_cplat, cplat_rename(StrEq("trace.log.1"), StrEq("trace.log.2"), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - cplat_rename() が "trace.log.1" と等しい文字列、"trace.log.2" と等しい文字列を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_rename() から、旧世代に対して 0 を返す。
    EXPECT_CALL(mock_cplat, cplat_rename(StrEq("trace.log"), StrEq("trace.log.1"), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - cplat_rename() が "trace.log" と等しい文字列、"trace.log.1" と等しい文字列を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_rename() から、現在ファイルに対して 0 を返す。
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_default(), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - 開き直しの cplat_file_open() が "trace.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。(切り詰めずに追記モードで開き直すことの確認)
                 // [Pre-Assert手順] - cplat_file_open() から、追記モードの open に対して 0 を返す。
    EXPECT_CALL(mock_cplat, cplat_file_get_size(_, _, _))
        .WillOnce(
            [](const cplat_file *, size_t *size_out, cplat_error *)
            {
                *size_out = 0;
                return 0;
            }); // [Pre-Assert確認_正常系] - 開き直し時に cplat_file_get_size() が 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_file_get_size() から、size_out に 0 を設定して 0 を返す。
    EXPECT_CALL(mock_cplat, cplat_file_get_id(_, _, _))
        .Times(
            1); // [Pre-Assert確認_正常系] - 開き直し時に cplat_file_get_id() が 1 回呼び出されること。(開き直したファイルの同一性をキャッシュすることの確認)
    EXPECT_CALL(mock_cplat, cplat_interprocess_lock_unlock(_))
        .WillOnce(Return(
            CPLAT_OK)); // [Pre-Assert確認_正常系] - ローテーション後に cplat_interprocess_lock_unlock() が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_interprocess_lock_unlock() から CPLAT_OK を返す。
    EXPECT_CALL(mock_cplat, cplat_file_close(_, _)).Times(1); // dispose 分
    // [Pre-Assert確認_正常系] - dispose 時に cplat_file_close() が 1 回呼び出されること。

    // Act
    int result = cplat_trace_file_sink_write(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "rotate me"); // [手順] - 上限 1 byte の sink と "rotate me" を指定して cplat_trace_file_sink_write() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        result); // [確認_正常系] - cplat_trace_file_sink_write() の戻り値が CPLAT_OK であること。(書き込み後にローテーションが完了したことの確認)

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

// ロック下の再確認で他プロセスのローテーション済みを検知した場合に開き直すだけになることの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_shared_write_skips_rotate_when_other_process_already_rotated)
{
    // Arrange
    cplat_trace_file_sink *handle = cplat_trace_file_sink_create(
        "trace.log", 1, 2, CPLAT_TRACE_FILE_SINK_SHARED); // [状態] - 共有モードの file sink を用意する。
    ASSERT_NE(nullptr,
              handle); // [状態確認] - cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_rename(_, _, _))
        .Times(0); // [Pre-Assert確認_正常系] - cplat_rename() が呼び出されないこと。

    InSequence seq;
    EXPECT_CALL(mock_cplat, cplat_file_get_path_id(StrEq("trace.log"), _, _))
        .Times(
            1); // [Pre-Assert確認_正常系] - 書き込み前に cplat_file_get_path_id() が "trace.log" と等しい文字列を指定して 1 回呼び出されること。
    EXPECT_CALL(mock_cplat, cplat_file_write(_, _, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - cplat_file_write() が 1 回呼び出されること。
                              // [Pre-Assert手順] - cplat_file_write() から 0 を返す。
    EXPECT_CALL(mock_cplat, cplat_file_get_size(_, _, _))
        .WillOnce(
            [](const cplat_file *, size_t *size_out, cplat_error *)
            {
                *size_out = 10;
                return 0;
            }); // [Pre-Assert確認_正常系] - 書き込み後の cplat_file_get_size() が 1 回呼び出されること。(実サイズがしきい値以上であることの確認)
                // [Pre-Assert手順] - cplat_file_get_size() から、サイズに 10 を設定して 0 を返す。
    EXPECT_CALL(mock_cplat, cplat_interprocess_lock_try_lock(_))
        .WillOnce(
            Return(CPLAT_OK)); // [Pre-Assert確認_正常系] - cplat_interprocess_lock_try_lock() が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_interprocess_lock_try_lock() から CPLAT_OK を返す。
    EXPECT_CALL(mock_cplat, cplat_file_get_path_id(StrEq("trace.log"), _, _))
        .WillOnce(
            [](const char *, cplat_file_id *id_out, cplat_error *)
            {
                set_file_id(id_out, kDefaultFileIndex + 1); // 他プロセスがローテーション済み
                return 0;
            }); // [Pre-Assert確認_正常系] - ロック下の再確認で cplat_file_get_path_id() が "trace.log" と等しい文字列を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_file_get_path_id() から、別のファイル同一性を設定して 0 を返す。
    EXPECT_CALL(mock_cplat, cplat_file_close(_, _)).Times(1); // 開き直し前の close
    // [Pre-Assert確認_正常系] - 開き直し前に cplat_file_close() が 1 回呼び出されること。
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_default(), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - cplat_file_open() が "trace.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。(リネームせず開き直しだけ行うことの確認)
                 // [Pre-Assert手順] - cplat_file_open() から 0 を返す。
    EXPECT_CALL(mock_cplat, cplat_file_get_size(_, _, _))
        .WillOnce(
            [](const cplat_file *, size_t *size_out, cplat_error *)
            {
                *size_out = 0;
                return 0;
            }); // [Pre-Assert確認_正常系] - 開き直し時に cplat_file_get_size() が 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_file_get_size() から、size_out に 0 を設定して 0 を返す。
    EXPECT_CALL(mock_cplat, cplat_file_get_id(_, _, _))
        .Times(
            1); // [Pre-Assert確認_正常系] - 開き直し時に cplat_file_get_id() が 1 回呼び出されること。(開き直したファイルの同一性をキャッシュすることの確認)
    EXPECT_CALL(mock_cplat, cplat_interprocess_lock_unlock(_)).WillOnce(Return(CPLAT_OK)); // 開き直し後のロック解放
    // [Pre-Assert確認_正常系] - 開き直し後に cplat_interprocess_lock_unlock() が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_interprocess_lock_unlock() から CPLAT_OK を返す。
    EXPECT_CALL(mock_cplat, cplat_file_close(_, _)).Times(1); // dispose 分
    // [Pre-Assert確認_正常系] - dispose 時に cplat_file_close() が 1 回呼び出されること。

    // Act
    int result = cplat_trace_file_sink_write(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "already rotated"); // [手順] - "already rotated" を指定して cplat_trace_file_sink_write() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_trace_file_sink_write() の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

// プロセス間ロックがビジー状態の場合にローテーションを見送ることの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_shared_write_skips_rotate_when_lock_is_busy)
{
    // Arrange
    cplat_trace_file_sink *handle = cplat_trace_file_sink_create(
        "trace.log", 1, 2, CPLAT_TRACE_FILE_SINK_SHARED); // [状態] - 共有モードの file sink を用意する。
    ASSERT_NE(nullptr,
              handle); // [状態確認] - cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_file_get_size(_, _, _))
        .WillOnce(
            [](const cplat_file *, size_t *size_out, cplat_error *)
            {
                *size_out = 10;
                return 0;
            }); // [Pre-Assert確認_異常系] - cplat_file_get_size() が 1 回呼び出されること。(実サイズがしきい値以上であることの確認)
                // [Pre-Assert手順] - cplat_file_get_size() から、サイズに 10 を設定して 0 を返す。
    EXPECT_CALL(mock_cplat, cplat_interprocess_lock_try_lock(_))
        .WillOnce(Return(
            CPLAT_ERR_BUSY)); // [Pre-Assert確認_異常系] - cplat_interprocess_lock_try_lock() が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_interprocess_lock_try_lock() から CPLAT_ERR_BUSY を返す。
    EXPECT_CALL(mock_cplat, cplat_rename(_, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - cplat_rename() が呼び出されないこと。
    EXPECT_CALL(mock_cplat, cplat_interprocess_lock_unlock(_))
        .Times(0); // [Pre-Assert確認_異常系] - cplat_interprocess_lock_unlock() が呼び出されないこと。

    // Act
    int result = cplat_trace_file_sink_write(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "lock busy"); // [手順] - "lock busy" を指定して cplat_trace_file_sink_write() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        result); // [確認_異常系] - cplat_trace_file_sink_write() の戻り値が CPLAT_OK であること。(ローテーションを見送っても書き込みは成功であることの確認)

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

// 同一プロセス内で同一パスの create が同一ハンドルを共有することの確認 (プロセス内調停)
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_create_same_path_shares_handle_in_single_process)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_default(), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - cplat_file_open() が "trace.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。(同一パスの create を 2 回呼んでもオープンは 1 回であることの確認)
                 // [Pre-Assert手順] - cplat_file_open() から 0 を返す。

    // Act
    cplat_trace_file_sink *first = cplat_trace_file_sink_create(
        "trace.log", 0, 0, 0); // [手順] - パス "trace.log" を指定して cplat_trace_file_sink_create() を呼び出す。
    cplat_trace_file_sink *second = cplat_trace_file_sink_create(
        "trace.log", 0, 0, 0); // [手順] - 同一パス "trace.log" を指定して cplat_trace_file_sink_create() を呼び出す。

    // Assert
    ASSERT_NE(nullptr,
              first); // [確認_正常系] - 1 回目の cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。
    EXPECT_EQ(
        first,
        second); // [確認_正常系] - 2 回目の cplat_trace_file_sink_create() の戻り値が 1 回目の cplat_trace_file_sink_create() の戻り値と一致すること。

    // Cleanup
    EXPECT_CALL(mock_cplat, cplat_file_close(_, _)).Times(AtLeast(1)); // dispose 時の close を許容する。
    // [Pre-Assert確認_正常系] - cplat_file_close() が 1 回以上呼び出されること。
    cplat_trace_file_sink_dispose(second);
    cplat_trace_file_sink_dispose(first);
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

// 参照カウントにより最後の dispose までハンドルが有効であることの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_shared_handle_survives_until_last_dispose)
{
    // Arrange
    cplat_trace_file_sink *first =
        cplat_trace_file_sink_create("trace.log", 0, 0, 0); // [状態] - 同一パスの 1 つ目の file sink を用意する。
    cplat_trace_file_sink *second =
        cplat_trace_file_sink_create("trace.log", 0, 0, 0); // [状態] - 同一パスの 2 つ目の file sink を用意する。
    ASSERT_NE(nullptr,
              first); // [状態確認] - 1 つ目の cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。
    ASSERT_EQ(first, second); // [状態] - 同一 sink の参照カウントが 2 の状態とする。
    // [状態確認] - 2 つ目の cplat_trace_file_sink_create() の戻り値が 1 つ目の cplat_trace_file_sink_create() の戻り値と一致すること。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_file_write(_, _, _, _))
        .WillOnce(
            [](cplat_file *, const void *buf, size_t len, cplat_error *)
            {
                std::string actual((const char *)buf, len);
                EXPECT_NE(std::string::npos, actual.find("after first dispose"));
                // [Pre-Assert確認_正常系] - "after first dispose" を探した actual.find() の戻り値が std::string::npos 以外であること。
                return 0;
            }); // [Pre-Assert確認_正常系] - 1 回目の dispose の後に cplat_file_write() が 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_file_write() から、書き込み内容を確認して 0 を返す。

    // Act
    cplat_trace_file_sink_dispose(
        first); // [手順] - 1 人目の sink を指定して cplat_trace_file_sink_dispose() を呼び出す。
    int result = cplat_trace_file_sink_write(
        second, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "after first dispose"); // [手順] - 2 人目の sink と "after first dispose" を指定して cplat_trace_file_sink_write() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        result); // [確認_正常系] - 参照が残っている間の cplat_trace_file_sink_write() の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_trace_file_sink_dispose(second);
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

// 共有モード設定が一致しない同一パスの create が失敗することの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_create_same_path_with_mismatched_shared_flag_returns_null)
{
    // Arrange
    cplat_trace_file_sink *first =
        cplat_trace_file_sink_create("trace.log", 0, 0, 0); // [状態] - 占有モードの sink を用意する。
    ASSERT_NE(nullptr,
              first); // [状態確認] - cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。

    // Pre-Assert

    // Act
    cplat_trace_file_sink *second = cplat_trace_file_sink_create(
        "trace.log", 0, 0,
        CPLAT_TRACE_FILE_SINK_SHARED); // [手順] - 占有モードで開いた同一パスと共有モードを指定して cplat_trace_file_sink_create() を呼び出す。

    // Assert
    EXPECT_EQ(
        nullptr,
        second); // [確認_異常系] - 共有モードが一致しないときの cplat_trace_file_sink_create() の戻り値が nullptr であること。

    // Cleanup
    cplat_trace_file_sink_dispose(first);
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

// 異なるパスの create は独立したハンドルを生成することの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_create_different_paths_returns_distinct_handles)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("first.log"), open_flags_default(), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - cplat_file_open() が "first.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_file_open() から、1 つ目に対して 0 を返す。
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("second.log"), open_flags_default(), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - cplat_file_open() が "second.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_file_open() から、2 つ目に対して 0 を返す。

    // Act
    cplat_trace_file_sink *first = cplat_trace_file_sink_create(
        "first.log", 0, 0, 0); // [手順] - パス "first.log" を指定して cplat_trace_file_sink_create() を呼び出す。
    cplat_trace_file_sink *second = cplat_trace_file_sink_create(
        "second.log", 0, 0, 0); // [手順] - パス "second.log" を指定して cplat_trace_file_sink_create() を呼び出す。

    // Assert
    ASSERT_NE(nullptr,
              first); // [確認_正常系] - 1 つ目の cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。
    ASSERT_NE(nullptr,
              second); // [確認_正常系] - 2 つ目の cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。
    EXPECT_NE(
        first,
        second); // [確認_正常系] - 2 つ目の cplat_trace_file_sink_create() の戻り値が 1 つ目の cplat_trace_file_sink_create() の戻り値以外であること。(異なるパスでは別のハンドルであることの確認)

    // Cleanup
    EXPECT_CALL(mock_cplat, cplat_file_close(_, _)).Times(AtLeast(2)); // dispose 時の close を許容する。
    // [Pre-Assert確認_正常系] - cplat_file_close() が 2 回以上呼び出されること。
    cplat_trace_file_sink_dispose(first);
    cplat_trace_file_sink_dispose(second);
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

// 開き直しの失敗が続く場合、書き込みを行わず CPLAT_ERR_UNKNOWN を返し、次の成功時に書き込めることの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, write_retries_reopen_on_subsequent_write_after_reopen_failure)
{
    // Arrange
    const std::string first_line(30U, 'r');
    cplat_trace_file_sink *handle =
        cplat_trace_file_sink_create("trace.log", 60, 2, 0); // [状態] - ローテーション設定付きの file sink を用意する。
    ASSERT_NE(nullptr,
              handle); // [状態確認] - cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。

    // Pre-Assert
    {
        InSequence seq;
        EXPECT_CALL(mock_cplat, cplat_file_write(_, _, _, _))
            .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 1 回目の書き込みで cplat_file_write() が 1 回呼び出されること。
                                  // [Pre-Assert手順] - cplat_file_write() から 0 を返す。
        EXPECT_CALL(mock_cplat, cplat_file_close(_, _))
            .Times(1); // [Pre-Assert確認_正常系] - ローテーション前に cplat_file_close() が 1 回呼び出されること。
        EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_truncate(), _))
            .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - ローテーション後の cplat_file_open() が失敗すること。
                                   // [Pre-Assert手順] - cplat_file_open() から -1 を返す。
        EXPECT_CALL(mock_cplat, cplat_file_close(_, _))
            .Times(1); // [Pre-Assert確認_異常系] - 2 回目の開き直し前に cplat_file_close() が 1 回呼び出されること。
        EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_default(), _))
            .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - 2 回目の開き直しの cplat_file_open() が open_flags_default() で 1 回呼び出されること。
                                   // [Pre-Assert手順] - cplat_file_open() から -1 を返す。
        EXPECT_CALL(mock_cplat, cplat_file_close(_, _))
            .Times(1); // [Pre-Assert確認_正常系] - 3 回目の開き直し前に cplat_file_close() が 1 回呼び出されること。
        EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_default(), _))
            .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 3 回目の開き直しの cplat_file_open() が open_flags_default() で 1 回呼び出されること。
                                  // [Pre-Assert手順] - cplat_file_open() から 0 を返す。
        EXPECT_CALL(mock_cplat, cplat_file_write(_, _, _, _))
            .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 3 回目の書き込みで cplat_file_write() が 1 回呼び出されること。
                                  // [Pre-Assert手順] - cplat_file_write() から 0 を返す。
        EXPECT_CALL(mock_cplat, cplat_file_close(_, _))
            .Times(1); // [Pre-Assert確認_正常系] - dispose 時に cplat_file_close() が 1 回呼び出されること。
    }

    // Act
    int actual_ret_first = cplat_trace_file_sink_write(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
        first_line.c_str()); // [手順] - ローテーション後オープンが失敗する状態で 1 回目の書き込みを行う。
    int actual_ret_second = cplat_trace_file_sink_write(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "second"); // [手順] - 開き直しが失敗する状態で 2 回目の書き込みを行う。
    int actual_ret_third = cplat_trace_file_sink_write(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "third"); // [手順] - 開き直しが成功する状態で 3 回目の書き込みを行う。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret_first); // [確認_異常系] - 1 回目の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret_second); // [確認_異常系] - 2 回目の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(CPLAT_OK,
              actual_ret_third); // [確認_正常系] - 3 回目の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

// 単一プロセス モードのローテーションで削除・名前変更・パス生成失敗時に切り詰めず CPLAT_ERR_UNKNOWN を返すことの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, write_single_rotation_failures_do_not_truncate_and_return_unknown)
{
    // Arrange
    cplat_trace_file_sink *handle_remove_fail =
        cplat_trace_file_sink_create("remove-fail.log", 1, 2, 0); // [状態] - 削除失敗用の単一プロセス sink を用意する。
    cplat_trace_file_sink *handle_rename_fail =
        cplat_trace_file_sink_create("rename-fail.log", 1, 2, 0); // [状態] - 名前変更失敗用の単一プロセス sink を用意する。
    const std::string near_max_path(PLATFORM_PATH_UTF8_MAX - 7U, 'p');
    cplat_trace_file_sink *handle_path_fail =
        cplat_trace_file_sink_create(near_max_path.c_str(), 1, 100000,
                                     0); // [状態] - 世代パス生成で上限を超える sink を用意する。
    ASSERT_NE(nullptr, handle_remove_fail); // [状態確認] - handle_remove_fail が nullptr 以外であること。
    ASSERT_NE(nullptr, handle_rename_fail); // [状態確認] - handle_rename_fail が nullptr 以外であること。
    ASSERT_NE(nullptr, handle_path_fail);   // [状態確認] - handle_path_fail が nullptr 以外であること。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_remove(StrEq("remove-fail.log.2"), _))
        .WillOnce(
            [](const char *, cplat_error *detail_out)
            {
                if (detail_out != nullptr)
                {
                    detail_out->result = CPLAT_ERR_PERMISSION_DENIED;
                }
                return -1;
            }); // [Pre-Assert確認_異常系] - cplat_remove() が "remove-fail.log.2" で 1 回呼ばれること。
                // [Pre-Assert手順] - CPLAT_ERR_NOT_FOUND 以外の失敗を返す。
    EXPECT_CALL(mock_cplat, cplat_rename(StrEq("remove-fail.log.1"), _, _))
        .Times(0); // [Pre-Assert確認_異常系] - 削除失敗後に後続の世代移動を行わないこと。
    EXPECT_CALL(mock_cplat, cplat_rename(StrEq("remove-fail.log"), _, _))
        .Times(0); // [Pre-Assert確認_異常系] - 削除失敗後に現在ファイルの移動を行わないこと。
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("remove-fail.log"), open_flags_truncate(), _))
        .Times(0); // [Pre-Assert確認_異常系] - 削除失敗後に切り詰め open を呼ばないこと。

    EXPECT_CALL(mock_cplat, cplat_remove(StrEq("rename-fail.log.2"), _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - rename 失敗テストで最古世代削除の cplat_remove() が 1 回呼ばれること。
    EXPECT_CALL(mock_cplat, cplat_rename(StrEq("rename-fail.log.1"), StrEq("rename-fail.log.2"), _))
        .WillOnce(
            [](const char *, const char *, cplat_error *detail_out)
            {
                if (detail_out != nullptr)
                {
                    detail_out->result = CPLAT_ERR_PERMISSION_DENIED;
                }
                return -1;
            }); // [Pre-Assert確認_異常系] - cplat_rename() が "rename-fail.log.1" で 1 回呼ばれること。
                // [Pre-Assert手順] - CPLAT_ERR_NOT_FOUND 以外の失敗を返す。
    EXPECT_CALL(mock_cplat, cplat_rename(StrEq("rename-fail.log"), _, _))
        .Times(0); // [Pre-Assert確認_異常系] - 名前変更失敗後に後続の移動を行わないこと。
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("rename-fail.log"), open_flags_truncate(), _))
        .Times(0); // [Pre-Assert確認_異常系] - 名前変更失敗後に切り詰め open を呼ばないこと。

    EXPECT_CALL(mock_cplat, cplat_remove(StrEq((near_max_path + ".100000").c_str()), _))
        .Times(0); // [Pre-Assert確認_異常系] - 世代パス生成失敗時は削除を呼ばないこと。
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq(near_max_path.c_str()), open_flags_truncate(), _))
        .Times(0); // [Pre-Assert確認_異常系] - 世代パス生成失敗後に切り詰め open を呼ばないこと。

    // Act
    int actual_ret_remove = cplat_trace_file_sink_write(
        handle_remove_fail, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "remove fail"); // [手順] - 削除が失敗する状態で書き込む。
    int actual_ret_rename = cplat_trace_file_sink_write(
        handle_rename_fail, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "rename fail"); // [手順] - 名前変更が失敗する状態で書き込む。
    int actual_ret_path = cplat_trace_file_sink_write(
        handle_path_fail, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "path fail"); // [手順] - 世代パス生成が失敗する状態で書き込む。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret_remove); // [確認_異常系] - 削除失敗時の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret_rename); // [確認_異常系] - 名前変更失敗時の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret_path); // [確認_異常系] - 世代パス生成失敗時の戻り値が CPLAT_ERR_UNKNOWN であること。

    // Cleanup
    cplat_trace_file_sink_dispose(handle_path_fail);
    cplat_trace_file_sink_dispose(handle_rename_fail);
    cplat_trace_file_sink_dispose(handle_remove_fail);
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

// 単一プロセス モードのローテーションで CPLAT_ERR_NOT_FOUND を正常な不在として扱い、open 失敗単独なら CPLAT_OK を返すことの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, write_single_rotation_ignores_not_found_and_handles_open_failure)
{
    // Arrange
    cplat_trace_file_sink *handle =
        cplat_trace_file_sink_create("not-found-ok.log", 1, 2, 0); // [状態] - ローテーション設定付きの file sink を用意する。
    ASSERT_NE(nullptr, handle); // [状態確認] - cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。

    // Pre-Assert
    {
        InSequence seq;
        EXPECT_CALL(mock_cplat, cplat_file_write(_, _, _, _))
            .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 書き込み時に cplat_file_write() が 1 回呼ばれること。
                                  // [Pre-Assert手順] - 0 を返す。
        EXPECT_CALL(mock_cplat, cplat_file_close(_, _))
            .Times(1); // [Pre-Assert確認_正常系] - ローテーション前に cplat_file_close() が 1 回呼ばれること。
        EXPECT_CALL(mock_cplat, cplat_remove(StrEq("not-found-ok.log.2"), _))
            .WillOnce(
                [](const char *, cplat_error *detail_out)
                {
                    if (detail_out != nullptr)
                    {
                        detail_out->result = CPLAT_ERR_NOT_FOUND;
                    }
                    return -1;
                }); // [Pre-Assert確認_正常系] - 最古世代削除の cplat_remove() が呼ばれること。
                    // [Pre-Assert手順] - CPLAT_ERR_NOT_FOUND を設定して -1 を返す。
        EXPECT_CALL(mock_cplat, cplat_rename(StrEq("not-found-ok.log.1"), StrEq("not-found-ok.log.2"), _))
            .WillOnce(
                [](const char *, const char *, cplat_error *detail_out)
                {
                    if (detail_out != nullptr)
                    {
                        detail_out->result = CPLAT_ERR_NOT_FOUND;
                    }
                    return -1;
                }); // [Pre-Assert確認_正常系] - 旧世代移動の cplat_rename() が呼ばれること。
                    // [Pre-Assert手順] - CPLAT_ERR_NOT_FOUND を設定して -1 を返す。
        EXPECT_CALL(mock_cplat, cplat_rename(StrEq("not-found-ok.log"), StrEq("not-found-ok.log.1"), _))
            .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 現在ファイル移動の cplat_rename() が呼ばれること。
                                  // [Pre-Assert手順] - 0 を返す。
        EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("not-found-ok.log"), open_flags_truncate(), _))
            .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - 新規ファイルの cplat_file_open() が呼ばれること。
                                   // [Pre-Assert手順] - -1 を返す。
        EXPECT_CALL(mock_cplat, cplat_file_close(_, _))
            .Times(1); // [Pre-Assert確認_正常系] - dispose 時に cplat_file_close() が 1 回呼ばれること。
    }

    // Act
    int actual_ret = cplat_trace_file_sink_write(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "rotate with missing old"); // [手順] - 旧世代が不在で新規ファイル open だけが失敗する状態で書き込む。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret); // [確認_正常系] - 不在を正常扱いし open 失敗単独として CPLAT_OK を返すこと。

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

// 共有モードのローテーションにおける各種失敗が規定の戻り値を返すことの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, write_shared_rotation_failures_return_expected_results)
{
    // Arrange
    cplat_trace_file_sink *handle_lock_err = cplat_trace_file_sink_create(
        "shared-lock-err.log", 1, 2, CPLAT_TRACE_FILE_SINK_SHARED); // [状態] - ロック取得失敗用の共有 sink を用意する。
    cplat_trace_file_sink *handle_close_err = cplat_trace_file_sink_create(
        "shared-close-err.log", 1, 2, CPLAT_TRACE_FILE_SINK_SHARED); // [状態] - close 失敗用の共有 sink を用意する。
    cplat_trace_file_sink *handle_remove_err = cplat_trace_file_sink_create(
        "shared-remove-err.log", 1, 2, CPLAT_TRACE_FILE_SINK_SHARED); // [状態] - remove 失敗用の共有 sink を用意する。
    cplat_trace_file_sink *handle_rename_err = cplat_trace_file_sink_create(
        "shared-rename-err.log", 1, 2, CPLAT_TRACE_FILE_SINK_SHARED); // [状態] - rename 失敗用の共有 sink を用意する。
    const std::string near_max_path(PLATFORM_PATH_UTF8_MAX - 7U, 's');
    cplat_trace_file_sink *handle_path_err = cplat_trace_file_sink_create(
        near_max_path.c_str(), 1, 100000, CPLAT_TRACE_FILE_SINK_SHARED); // [状態] - 世代パス超過用の共有 sink を用意する。
    cplat_trace_file_sink *handle_open_err = cplat_trace_file_sink_create(
        "shared-open-err.log", 1, 2, CPLAT_TRACE_FILE_SINK_SHARED); // [状態] - 新規 open 失敗用の共有 sink を用意する。
    ASSERT_NE(nullptr, handle_lock_err);   // [状態確認] - handle_lock_err が nullptr 以外であること。
    ASSERT_NE(nullptr, handle_close_err);  // [状態確認] - handle_close_err が nullptr 以外であること。
    ASSERT_NE(nullptr, handle_remove_err); // [状態確認] - handle_remove_err が nullptr 以外であること。
    ASSERT_NE(nullptr, handle_rename_err); // [状態確認] - handle_rename_err が nullptr 以外であること。
    ASSERT_NE(nullptr, handle_path_err);   // [状態確認] - handle_path_err が nullptr 以外であること。
    ASSERT_NE(nullptr, handle_open_err);   // [状態確認] - handle_open_err が nullptr 以外であること。

    // Pre-Assert
    ON_CALL(mock_cplat, cplat_file_get_size(_, _, _))
        .WillByDefault(
            [](const cplat_file *, size_t *size_out, cplat_error *)
            {
                *size_out = 10;
                return CPLAT_OK;
            }); // [Pre-Assert手順] - 既定サイズを上限以上の 10 にする。

    EXPECT_CALL(mock_cplat, cplat_interprocess_lock_try_lock(_))
        .WillOnce(Return(CPLAT_ERR_PERMISSION_DENIED))
        .WillRepeatedly(Return(CPLAT_OK)); // [Pre-Assert確認_異常系] - ロック取得で 1 回目は BUSY 以外の失敗を返し、以降は成功すること。
    EXPECT_CALL(mock_cplat, cplat_file_close(_, _))
        .WillOnce(Return(CPLAT_ERR_UNKNOWN))
        .WillRepeatedly(Return(CPLAT_OK)); // [Pre-Assert確認_異常系] - ローテーション中の close で 1 回失敗させること。
    EXPECT_CALL(mock_cplat, cplat_remove(_, _))
        .Times(AnyNumber()); // [Pre-Assert確認_正常系] - 他の共有 sink の削除が既定動作で成功すること。
    EXPECT_CALL(mock_cplat, cplat_remove(StrEq("shared-remove-err.log.2"), _))
        .WillOnce(
            [](const char *, cplat_error *detail_out)
            {
                if (detail_out != nullptr)
                {
                    detail_out->result = CPLAT_ERR_PERMISSION_DENIED;
                }
                return -1;
            }); // [Pre-Assert確認_異常系] - remove を失敗させること。
    EXPECT_CALL(mock_cplat, cplat_rename(_, _, _))
        .Times(AnyNumber()); // [Pre-Assert確認_正常系] - 他の共有 sink の名前変更が既定動作で成功すること。
    EXPECT_CALL(mock_cplat, cplat_rename(StrEq("shared-rename-err.log.1"), StrEq("shared-rename-err.log.2"), _))
        .WillOnce(
            [](const char *, const char *, cplat_error *detail_out)
            {
                if (detail_out != nullptr)
                {
                    detail_out->result = CPLAT_ERR_PERMISSION_DENIED;
                }
                return -1;
            }); // [Pre-Assert確認_異常系] - rename を失敗させること。
    EXPECT_CALL(mock_cplat, cplat_file_open(_, _, _, _))
        .Times(AnyNumber()); // [Pre-Assert確認_正常系] - 他の共有 sink のファイル オープンが既定動作で成功すること。
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("shared-open-err.log"), open_flags_default(), _))
        .WillOnce(Return(-1))
        .WillOnce(Return(0))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - 新規ファイル open だけを失敗させ、次の書き込みの開き直しと再ローテーションは成功すること。

    // Act
    int actual_ret_lock = cplat_trace_file_sink_write(
        handle_lock_err, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "lock err"); // [手順] - ロック取得が BUSY 以外で失敗する状態で書き込む。
    int actual_ret_close = cplat_trace_file_sink_write(
        handle_close_err, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "close err"); // [手順] - close が失敗する状態で書き込む。
    int actual_ret_remove = cplat_trace_file_sink_write(
        handle_remove_err, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "remove err"); // [手順] - remove が失敗する状態で書き込む。
    int actual_ret_rename = cplat_trace_file_sink_write(
        handle_rename_err, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "rename err"); // [手順] - rename が失敗する状態で書き込む。
    int actual_ret_path = cplat_trace_file_sink_write(
        handle_path_err, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "path err"); // [手順] - 世代パス生成が失敗する状態で書き込む。
    int actual_ret_open = cplat_trace_file_sink_write(
        handle_open_err, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "open err"); // [手順] - 新規ファイル open だけが失敗する状態で書き込む。
    int actual_ret_open_next = cplat_trace_file_sink_write(
        handle_open_err, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "open next"); // [手順] - 未オープン状態の共有 sink で次の書き込みを行う。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret_lock);   // [確認_異常系] - ロック取得失敗で CPLAT_ERR_UNKNOWN を返すこと。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret_close);  // [確認_異常系] - close 失敗で CPLAT_ERR_UNKNOWN を返すこと。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret_remove); // [確認_異常系] - remove 失敗で CPLAT_ERR_UNKNOWN を返すこと。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret_rename); // [確認_異常系] - rename 失敗で CPLAT_ERR_UNKNOWN を返すこと。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret_path);   // [確認_異常系] - パス生成失敗で CPLAT_ERR_UNKNOWN を返すこと。
    EXPECT_EQ(CPLAT_OK,
              actual_ret_open); // [確認_異常系] - 新規ファイル open 失敗単独ならロック解放成功後に CPLAT_OK を返すこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_open_next); // [確認_正常系] - 次の書き込みで開き直して書き込めること。

    // Cleanup
    cplat_trace_file_sink_dispose(handle_open_err);
    cplat_trace_file_sink_dispose(handle_path_err);
    cplat_trace_file_sink_dispose(handle_rename_err);
    cplat_trace_file_sink_dispose(handle_remove_err);
    cplat_trace_file_sink_dispose(handle_close_err);
    cplat_trace_file_sink_dispose(handle_lock_err);
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

// 共有モードで新規ファイル open 失敗とロック解放失敗が同時に起きた場合に CPLAT_ERR_UNKNOWN が優先されることの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, write_shared_rotation_open_failure_and_unlock_failure_returns_unknown)
{
    // Arrange
    cplat_trace_file_sink *handle = cplat_trace_file_sink_create(
        "shared-open-unlock-err.log", 1, 2, CPLAT_TRACE_FILE_SINK_SHARED); // [状態] - 共有モードの sink を用意する。
    ASSERT_NE(nullptr, handle); // [状態確認] - cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_file_get_size(_, _, _))
        .WillOnce(
            [](const cplat_file *, size_t *size_out, cplat_error *)
            {
                *size_out = 10;
                return CPLAT_OK;
            })
        .WillOnce(
            [](const cplat_file *, size_t *size_out, cplat_error *)
            {
                *size_out = 10;
                return CPLAT_OK;
            }); // [Pre-Assert確認_正常系] - しきい値判定とロック内の再確認で cplat_file_get_size() が 2 回呼ばれること。
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("shared-open-unlock-err.log"), open_flags_default(), _))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - 新規ファイルの open が失敗すること。
                               // [Pre-Assert手順] - -1 を返す。
    EXPECT_CALL(mock_cplat, cplat_interprocess_lock_unlock(_))
        .WillOnce(Return(CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_異常系] - ロック解放が失敗すること。
                                              // [Pre-Assert手順] - CPLAT_ERR_UNKNOWN を返す。

    // Act
    int actual_ret = cplat_trace_file_sink_write(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "both fail"); // [手順] - 新規ファイル open とロック解放が同時に失敗する状態で書き込む。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - ロック解放失敗が優先され CPLAT_ERR_UNKNOWN を返すこと。

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

// 開き直した時点のサイズが上限以上の場合に書き込み直後に再びローテーションが起きることの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, write_reopen_rotates_immediately_when_size_is_at_or_above_limit)
{
    // Arrange
    cplat_trace_file_sink *handle =
        cplat_trace_file_sink_create("reopen-size-limit.log", 10, 2, 0); // [状態] - 上限 10 byte の単一プロセス sink を用意する。
    ASSERT_NE(nullptr, handle); // [状態確認] - cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。

    // Pre-Assert
    {
        InSequence seq;
        EXPECT_CALL(mock_cplat, cplat_file_write(_, _, _, _))
            .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 1 回目の書き込みを行うこと。
        EXPECT_CALL(mock_cplat, cplat_remove(StrEq("reopen-size-limit.log.2"), _))
            .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 1 回目のローテーションで cplat_remove() が呼ばれること。
        EXPECT_CALL(mock_cplat, cplat_rename(StrEq("reopen-size-limit.log.1"), StrEq("reopen-size-limit.log.2"), _))
            .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 1 回目のローテーションで cplat_rename() が呼ばれること。
        EXPECT_CALL(mock_cplat, cplat_rename(StrEq("reopen-size-limit.log"), StrEq("reopen-size-limit.log.1"), _))
            .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 1 回目のローテーションで現在ファイルの cplat_rename() が呼ばれること。
        EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("reopen-size-limit.log"), open_flags_truncate(), _))
            .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - 1 回目のローテーション後の open を失敗させること。
        EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("reopen-size-limit.log"), open_flags_default(), _))
            .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 2 回目の開き直しが成功すること。
        EXPECT_CALL(mock_cplat, cplat_file_get_size(_, _, _))
            .WillOnce(
                [](const cplat_file *, size_t *size_out, cplat_error *)
                {
                    *size_out = 10;
                    return CPLAT_OK;
                }); // [Pre-Assert確認_正常系] - 開き直し時のサイズ取得で上限値 10 を返すこと。
        EXPECT_CALL(mock_cplat, cplat_file_write(_, _, _, _))
            .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 2 回目の書き込みを行うこと。
        EXPECT_CALL(mock_cplat, cplat_remove(StrEq("reopen-size-limit.log.2"), _))
            .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 2 回目の書き込み直後に cplat_remove() が呼ばれること。
        EXPECT_CALL(mock_cplat, cplat_rename(StrEq("reopen-size-limit.log.1"), StrEq("reopen-size-limit.log.2"), _))
            .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 2 回目の書き込み直後に cplat_rename() が呼ばれること。
        EXPECT_CALL(mock_cplat, cplat_rename(StrEq("reopen-size-limit.log"), StrEq("reopen-size-limit.log.1"), _))
            .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 現在ファイルの cplat_rename() が呼ばれること。
        EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("reopen-size-limit.log"), open_flags_truncate(), _))
            .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 再ローテーション後の切り詰め open が呼ばれること。
    }

    // Act
    int actual_ret_first = cplat_trace_file_sink_write(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "rotate me!"); // [手順] - 上限を超える 1 回目の書き込みを行う。
    int actual_ret_second = cplat_trace_file_sink_write(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "a"); // [手順] - 開き直し後のサイズが上限以上の状態で短い 2 回目の書き込みを行う。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_first);  // [確認_異常系] - 1 回目の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_second); // [確認_正常系] - 2 回目の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

// 開き直しのサイズ取得に失敗した場合に書き込みが続き、current_bytes が 0 から数え直されることの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, write_reopen_resets_current_bytes_when_size_query_fails)
{
    // Arrange
    // "2026-04-26T03:04:05.678+09:00 I x\n" は 34 byte。上限 50 byte なら、0 起点で 1 回目は超えず、2 回目で超える。
    cplat_trace_file_sink *handle =
        cplat_trace_file_sink_create("reopen-size-fail.log", 50, 2, 0); // [状態] - 上限 50 byte の sink を用意する。
    ASSERT_NE(nullptr, handle); // [状態確認] - cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。
    const std::string long_line(60U, 'L');

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("reopen-size-fail.log"), open_flags_truncate(), _))
        .WillOnce(Return(-1))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - 1 回目のローテーション後 open は失敗し、2 回目のローテーション後 open は成功すること。
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("reopen-size-fail.log"), open_flags_default(), _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 2 回目の開き直し open が 1 回呼ばれること。
    EXPECT_CALL(mock_cplat, cplat_file_get_size(_, _, _))
        .WillOnce(Return(CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_異常系] - 開き直し時のサイズ取得を失敗させること。
    EXPECT_CALL(mock_cplat, cplat_remove(StrEq("reopen-size-fail.log.2"), _))
        .Times(2); // [Pre-Assert確認_正常系] - 1 回目 (long_line) と 3 回目 (34+34=68 >= 50) の 2 回だけローテーションが起きること。

    // Act
    int actual_ret_first = cplat_trace_file_sink_write(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
        long_line.c_str()); // [手順] - 1 回目のローテーションを起こす。
    int actual_ret_second = cplat_trace_file_sink_write(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "x"); // [手順] - 開き直しのサイズ取得失敗後に 34 byte の書き込みを行う。
    int actual_ret_third = cplat_trace_file_sink_write(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "y"); // [手順] - 累積 68 byte で再びローテーションを起こす書き込みを行う。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_first);  // [確認_異常系] - 1 回目の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_second); // [確認_正常系] - サイズ取得失敗後も 2 回目が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_third);  // [確認_正常系] - 3 回目が CPLAT_OK であること。

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]

// ローテーション後に新しいファイルが開かれている場合、次の書き込みで重ねて開かないことの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, write_does_not_reopen_when_file_is_already_open_after_rotation)
{
    // Arrange
    const std::string first_line(30U, 'r');
    cplat_trace_file_sink *handle =
        cplat_trace_file_sink_create("already-open.log", 60, 2, 0); // [状態] - 単一プロセス sink を用意する。
    ASSERT_NE(nullptr, handle); // [状態確認] - cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。

    // Pre-Assert
    {
        InSequence seq;
        EXPECT_CALL(mock_cplat, cplat_file_write(_, _, _, _))
            .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 1 回目の書き込みを行うこと。
        EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("already-open.log"), open_flags_truncate(), _))
            .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - ローテーション後の切り詰め open が成功すること。
        EXPECT_CALL(mock_cplat, cplat_file_write(_, _, _, _))
            .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 2 回目の書き込みで再オープンせずそのまま書き込むこと。
    }
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("already-open.log"), open_flags_default(), _))
        .Times(0); // [Pre-Assert確認_正常系] - 2 回目の書き込み前に追記 open が呼ばれないこと。

    // Act
    int actual_ret_first = cplat_trace_file_sink_write(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
        first_line.c_str()); // [手順] - ローテーション後の open が成功する 1 回目の書き込みを行う。
    int actual_ret_second = cplat_trace_file_sink_write(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "next write"); // [手順] - 続けて 2 回目の書き込みを行う。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_first);  // [確認_正常系] - 1 回目の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_second); // [確認_正常系] - 2 回目の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}
// [サブ手順参照 名前=traceFileRegistryTest.TearDown]
