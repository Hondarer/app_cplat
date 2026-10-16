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

class trace_fileTest : public Test
{
  protected:
    NiceMock<Mock_cplat> mock_cplat;

    // [サブ手順 名前=trace_fileTest.SetUp]
    void SetUp() override
    {
        ON_CALL(mock_cplat, cplat_clock_get_realtime_deadline_ms(_, _))
            .WillByDefault([](uint64_t, struct timespec *abs_timeout) { set_valid_deadline(abs_timeout); });
        // [状態] - cplat_clock_get_realtime_deadline_ms() の既定動作を設定する。
        ON_CALL(mock_cplat, cplat_clock_get_realtime(_))
            .WillByDefault([](cplat_timespec *ts) { set_fixed_realtime(ts); });
        // [状態] - cplat_clock_get_realtime() の既定動作を設定する。
        ON_CALL(mock_cplat, cplat_clock_format_realtime_iso8601_local(_, _, _))
            .WillByDefault(
                [](char *buf, size_t buf_size, const cplat_timespec *)
                {
                    snprintf(buf, buf_size, "%s", "2026-04-26T03:04:05.678+09:00");
                    return 0;
                });
        // [状態] - cplat_clock_format_realtime_iso8601_local() の既定動作を設定する。
        ON_CALL(mock_cplat, cplat_file_open(_, _, _, _)).WillByDefault(Return(CPLAT_OK));
        // [状態] - cplat_file_open() の既定動作を設定する。
        ON_CALL(mock_cplat, cplat_file_get_size(_, _, _))
            .WillByDefault(
                [](const cplat_file *, size_t *size_out, cplat_error *)
                {
                    *size_out = 0;
                    return 0;
                });
        // [状態] - cplat_file_get_size() の既定動作を設定する。
        ON_CALL(mock_cplat, cplat_file_write(_, _, _, _)).WillByDefault(Return(CPLAT_OK));
        // [状態] - cplat_file_write() の既定動作を設定する。
        ON_CALL(mock_cplat, cplat_file_close(_, _)).WillByDefault(Return(CPLAT_OK));
        // [状態] - cplat_file_close() の既定動作を設定する。
        ON_CALL(mock_cplat, cplat_remove(_, _)).WillByDefault(Return(0));
        // [状態] - cplat_remove() の既定動作を設定する。
        ON_CALL(mock_cplat, cplat_rename(_, _, _)).WillByDefault(Return(0));
        // [状態] - cplat_rename() の既定動作を設定する。

        // 共有モード用の既定動作: ハンドルとパスの同一性は常に一致させる
        ON_CALL(mock_cplat, cplat_file_get_id(_, _, _))
            .WillByDefault(
                [](const cplat_file *, cplat_file_id *id_out, cplat_error *)
                {
                    set_file_id(id_out, kDefaultFileIndex);
                    return 0;
                });
        // [状態] - cplat_file_get_id() の既定動作を設定する。
        ON_CALL(mock_cplat, cplat_file_get_path_id(_, _, _))
            .WillByDefault(
                [](const char *, cplat_file_id *id_out, cplat_error *)
                {
                    set_file_id(id_out, kDefaultFileIndex);
                    return 0;
                });
        // [状態] - cplat_file_get_path_id() の既定動作を設定する。

        // プロセス間ロックはダミー ハンドルで成功させる (実体は作らない)
        ON_CALL(mock_cplat, cplat_interprocess_lock_open(_, _))
            .WillByDefault(
                [](const char *, cplat_interprocess_lock **lock)
                {
                    static int dummy_lock = 0;
                    *lock = (cplat_interprocess_lock *)&dummy_lock;
                    return CPLAT_OK;
                });
        // [状態] - cplat_interprocess_lock_open() の既定動作を設定する。
        ON_CALL(mock_cplat, cplat_interprocess_lock_try_lock(_)).WillByDefault(Return(CPLAT_OK));
        // [状態] - cplat_interprocess_lock_try_lock() の既定動作を設定する。
        ON_CALL(mock_cplat, cplat_interprocess_lock_unlock(_)).WillByDefault(Return(CPLAT_OK));
        // [状態] - cplat_interprocess_lock_unlock() の既定動作を設定する。
        ON_CALL(mock_cplat, cplat_interprocess_lock_dispose(_)).WillByDefault(Return());
        // [状態] - cplat_interprocess_lock_dispose() の既定動作を設定する。
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
    cplat_trace_file_sink *handle =
        cplat_trace_file_sink_create(nullptr, 0, 0, 0); // [手順] - NULL path で create を呼ぶ。

    // Assert
    EXPECT_EQ(nullptr,
              handle); // [確認_異常系] - cplat_trace_file_sink_create() の戻り値が nullptr であること。
}

// create が既定 open flags でファイルを開くことの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_create_opens_file_with_default_flags)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_default(), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - cplat_file_open() が "trace.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_file_open() から 0 を返却する。
    EXPECT_CALL(mock_cplat, cplat_file_get_size(_, _, _))
        .WillOnce(
            [](const cplat_file *, size_t *size_out, cplat_error *)
            {
                *size_out = 123;
                return 0;
            }); // [Pre-Assert確認_正常系] - cplat_file_get_size() が 1 回呼び出されること。
                // [Pre-Assert手順] - サイズ 123 を返却する。

    // Act
    cplat_trace_file_sink *handle =
        cplat_trace_file_sink_create("trace.log", 0, 0, 0); // [手順] - 既定値で create を呼ぶ。

    // Assert
    EXPECT_NE(nullptr,
              handle); // [確認_正常系] - cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。

    // Cleanup
    EXPECT_CALL(mock_cplat, cplat_file_close(_, _)).Times(AtLeast(1)); // dispose 時の close を許容する。
    // [Pre-Assert確認_正常系] - cplat_file_close() が 1 回以上呼び出されること。
    cplat_trace_file_sink_dispose(handle);
}

// OS バッファー指定時に write-through を付けずにファイルを開くことの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_create_opens_buffered_file_without_write_through)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("buffered.log"), open_flags_buffered(), _))
        .WillOnce(Return(
            CPLAT_OK)); // [Pre-Assert確認_正常系] - cplat_file_open() が "buffered.log" と等しい文字列、open_flags_buffered() の戻り値を指定して 1 回呼び出されること。(write-through を含まない flags で開いたことの確認)
                        // [Pre-Assert手順] - cplat_file_open() から CPLAT_OK を返却する。

    // Act
    cplat_trace_file_sink *handle = cplat_trace_file_sink_create(
        "buffered.log", 0, 0,
        CPLAT_TRACE_FILE_SINK_OS_BUFFERED); // [手順] - OS バッファー指定で sink を生成する。

    // Assert
    ASSERT_NE(nullptr,
              handle); // [確認_正常系] - cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}

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
                  // [Pre-Assert手順] - cplat_file_open() から -1 を返却する。
    EXPECT_CALL(mock_cplat, cplat_sleep_ms(3000))
        .WillOnce(Return()); // [Pre-Assert確認_正常系] - cplat_sleep_ms() が 3000 を指定して 1 回呼び出されること。
                             // [Pre-Assert手順] - 3 秒相当の待機を行う。
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_default(), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - リトライの cplat_file_open() が "trace.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_file_open() から 0 を返却する。
    EXPECT_CALL(mock_cplat, cplat_file_get_size(_, _, _))
        .WillOnce(
            [](const cplat_file *, size_t *size_out, cplat_error *)
            {
                *size_out = 0;
                return 0;
            }); // [Pre-Assert確認_正常系] - リトライ成功後の cplat_file_get_size() が 1 回呼び出されること。
                // [Pre-Assert手順] - サイズ 0 を返却する。

    // Act
    cplat_trace_file_sink *handle =
        cplat_trace_file_sink_create("trace.log", 0, 0, 0); // [手順] - 初回 open 失敗後に create を継続する。

    // Assert
    EXPECT_NE(nullptr,
              handle); // [確認_正常系] - cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。

    // Cleanup
    EXPECT_CALL(mock_cplat, cplat_file_close(_, _)).Times(AtLeast(1)); // dispose 時の close を許容する。
    // [Pre-Assert確認_異常系] - cplat_file_close() が 1 回以上呼び出されること。
    cplat_trace_file_sink_dispose(handle);
}

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
                  // [Pre-Assert手順] - cplat_file_open() から -1 を返却する。
    EXPECT_CALL(mock_cplat, cplat_sleep_ms(3000))
        .WillOnce(
            Return()); // [Pre-Assert確認_異常系] - 1 回目のリトライ前に cplat_sleep_ms() が 3000 を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - 3 秒相当の待機を行う。
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_default(), _))
        .WillOnce(Return(
            -1)); // [Pre-Assert確認_異常系] - 1 回目のリトライの cplat_file_open() が "trace.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。
                  // [Pre-Assert手順] - cplat_file_open() から -1 を返却する。
    EXPECT_CALL(mock_cplat, cplat_sleep_ms(3000))
        .WillOnce(
            Return()); // [Pre-Assert確認_異常系] - 2 回目のリトライ前に cplat_sleep_ms() が 3000 を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - 3 秒相当の待機を行う。
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_default(), _))
        .WillOnce(Return(
            -1)); // [Pre-Assert確認_異常系] - 2 回目のリトライの cplat_file_open() が "trace.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。
                  // [Pre-Assert手順] - cplat_file_open() から -1 を返却する。
    EXPECT_CALL(mock_cplat, cplat_sleep_ms(3000))
        .WillOnce(
            Return()); // [Pre-Assert確認_異常系] - 3 回目のリトライ前に cplat_sleep_ms() が 3000 を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - 3 秒相当の待機を行う。
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_default(), _))
        .WillOnce(Return(
            -1)); // [Pre-Assert確認_異常系] - 3 回目のリトライの cplat_file_open() が "trace.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。
                  // [Pre-Assert手順] - cplat_file_open() から -1 を返却する。
    EXPECT_CALL(mock_cplat, cplat_file_close(_, _))
        .Times(AtLeast(
            1)); // [Pre-Assert確認_異常系] - cplat_file_close() が 1 回以上呼び出されること。(失敗経路で開いたファイルを閉じることの確認)

    // Act
    cplat_trace_file_sink *handle =
        cplat_trace_file_sink_create("trace.log", 0, 0, 0); // [手順] - open 失敗を継続させる。

    // Assert
    EXPECT_EQ(nullptr,
              handle); // [確認_異常系] - cplat_trace_file_sink_create() の戻り値が nullptr であること。
}

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
                // [Pre-Assert手順] - 書式化した行を確認し、0 を返却する。

    // Act
    int result = cplat_trace_file_sink_write(handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
                                             "hello"); // [手順] - INFO 行を書き込む。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_trace_file_sink_write() の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}

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
                // [Pre-Assert手順] - D marker の行を確認し、0 を返却する。

    // Act
    int result = cplat_trace_file_sink_write(handle, CPLAT_TRACE_LEVEL_DEBUG, nullptr,
                                             "debug line"); // [手順] - DEBUG 行を書き込む。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_trace_file_sink_write() の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}

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
                // [Pre-Assert手順] - 書式化した行を確認し、0 を返却する。

    // Act
    int result = cplat_trace_file_sink_write(handle, CPLAT_TRACE_LEVEL_INFO, &timestamp,
                                             "explicit hello"); // [手順] - 明示タイムスタンプ付きで書き込む。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        result); // [確認_正常系] - 明示タイムスタンプ付きの cplat_trace_file_sink_write() の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}

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
                               // [Pre-Assert手順] - cplat_file_write() から -1 を返却する。

    // Act
    int result = cplat_trace_file_sink_write(handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
                                             "write error"); // [手順] - 書き込み失敗を発生させる。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - cplat_trace_file_sink_write() の戻り値が CPLAT_ERR_UNKNOWN であること。

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}

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
                // [Pre-Assert手順] - 代替時刻の行を確認し、0 を返却する。

    // Act
    int result = cplat_trace_file_sink_write(handle, CPLAT_TRACE_LEVEL_INFO, &invalid_timestamp,
                                             "invalid"); // [手順] - 不正タイムスタンプで書き込む。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        result); // [確認_異常系] - 代替出力後の cplat_trace_file_sink_write() の戻り値が CPLAT_ERR_UNKNOWN であること。

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}

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
                 // [Pre-Assert手順] - cplat_file_open() から 0 を返却する。
    EXPECT_CALL(mock_cplat, cplat_file_get_size(_, _, _))
        .WillOnce(
            [](const cplat_file *, size_t *size_out, cplat_error *)
            {
                *size_out = 0;
                return 0;
            }); // [Pre-Assert確認_正常系] - create 時に cplat_file_get_size() が 1 回呼び出されること。
                // [Pre-Assert手順] - size_out に 0 を設定し、0 を返却する。

    // Arrange
    cplat_trace_file_sink *handle =
        cplat_trace_file_sink_create("trace.log", 1, 2, 0); // [状態] - ローテーション設定付きの file sink を用意する。
    ASSERT_NE(nullptr,
              handle); // [状態確認] - cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_file_write(_, _, _, _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - 元ファイルへの書き込みで cplat_file_write() が 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_file_write() から 0 を返却する。
    EXPECT_CALL(mock_cplat, cplat_file_close(_, _)).Times(1); // ローテーション前の close
    // [Pre-Assert確認_正常系] - ローテーション前に cplat_file_close() が 1 回呼び出されること。
    EXPECT_CALL(mock_cplat, cplat_remove(StrEq("trace.log.2"), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - cplat_remove() が "trace.log.2" と等しい文字列を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_remove() から 0 を返却する。
    EXPECT_CALL(mock_cplat, cplat_rename(StrEq("trace.log.1"), StrEq("trace.log.2"), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - cplat_rename() が "trace.log.1" と等しい文字列、"trace.log.2" と等しい文字列を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_rename() から 0 を返却する。
    EXPECT_CALL(mock_cplat, cplat_rename(StrEq("trace.log"), StrEq("trace.log.1"), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - cplat_rename() が "trace.log" と等しい文字列、"trace.log.1" と等しい文字列を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - 現在ファイルの rename から 0 を返却する。
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_truncate(), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - ローテーション後の cplat_file_open() が "trace.log" と等しい文字列、open_flags_truncate() の戻り値を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - truncate 付き open から 0 を返却する。
    EXPECT_CALL(mock_cplat, cplat_file_close(_, _)).Times(1); // dispose 分
    // [Pre-Assert確認_正常系] - dispose 時に cplat_file_close() が 1 回呼び出されること。

    // Act
    int result = cplat_trace_file_sink_write(handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
                                             "rotate me"); // [手順] - 上限 1 byte のファイルへ 1 行書き込む。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        result); // [確認_正常系] - cplat_trace_file_sink_write() の戻り値が CPLAT_OK であること。(書き込み後にローテーションが完了したことの確認)

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}

// 単一プロセス モードのローテーション後にファイル オープンが失敗しても再試行しないことの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_write_does_not_retry_open_after_rotation)
{
    // Arrange

    // Pre-Assert
    {
        InSequence seq;

        EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_default(), _))
            .WillOnce(Return(
                0)); // [Pre-Assert確認_正常系] - create 時に cplat_file_open() が "trace.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。
                     // [Pre-Assert手順] - cplat_file_open() から 0 を返却する。
        EXPECT_CALL(mock_cplat, cplat_file_get_size(_, _, _))
            .WillOnce(
                [](const cplat_file *, size_t *size_out, cplat_error *)
                {
                    *size_out = 0;
                    return 0;
                }); // [Pre-Assert確認_正常系] - create 時に cplat_file_get_size() が 1 回呼び出されること。
                    // [Pre-Assert手順] - size_out に 0 を設定し、0 を返却する。
    }

    // Arrange
    cplat_trace_file_sink *handle =
        cplat_trace_file_sink_create("trace.log", 1, 2, 0); // [状態] - ローテーション設定付きの file sink を用意する。
    ASSERT_NE(nullptr,
              handle); // [状態確認] - cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。

    // Pre-Assert
    {
        InSequence seq;
        EXPECT_CALL(mock_cplat, cplat_file_write(_, _, _, _))
            .WillOnce(
                Return(0)); // [Pre-Assert確認_異常系] - ローテーション前の cplat_file_write() が 1 回呼び出されること。
                            // [Pre-Assert手順] - cplat_file_write() から 0 を返却する。
        EXPECT_CALL(mock_cplat, cplat_file_close(_, _)).Times(1); // ローテーション前の close
        // [Pre-Assert確認_正常系] - ローテーション前に cplat_file_close() が 1 回呼び出されること。
        EXPECT_CALL(mock_cplat, cplat_remove(StrEq("trace.log.2"), _))
            .WillOnce(Return(
                0)); // [Pre-Assert確認_異常系] - cplat_remove() が "trace.log.2" と等しい文字列を指定して 1 回呼び出されること。
                     // [Pre-Assert手順] - cplat_remove() から 0 を返却する。
        EXPECT_CALL(mock_cplat, cplat_rename(StrEq("trace.log.1"), StrEq("trace.log.2"), _))
            .WillOnce(Return(
                0)); // [Pre-Assert確認_異常系] - cplat_rename() が "trace.log.1" と等しい文字列、"trace.log.2" と等しい文字列を指定して 1 回呼び出されること。
                     // [Pre-Assert手順] - 旧世代の rename から 0 を返却する。
        EXPECT_CALL(mock_cplat, cplat_rename(StrEq("trace.log"), StrEq("trace.log.1"), _))
            .WillOnce(Return(
                0)); // [Pre-Assert確認_異常系] - cplat_rename() が "trace.log" と等しい文字列、"trace.log.1" と等しい文字列を指定して 1 回呼び出されること。
                     // [Pre-Assert手順] - 現在ファイルの rename から 0 を返却する。
        EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_truncate(), _))
            .WillOnce(Return(
                -1)); // [Pre-Assert確認_異常系] - ローテーション後の cplat_file_open() が "trace.log" と等しい文字列、open_flags_truncate() の戻り値を指定して 1 回呼び出されること。
                      // [Pre-Assert手順] - truncate 付き open から -1 を返却する。
        EXPECT_CALL(mock_cplat, cplat_file_close(_, _)).Times(1); // dispose 分
        // [Pre-Assert確認_正常系] - dispose 時に cplat_file_close() が 1 回呼び出されること。
    }
    EXPECT_CALL(mock_cplat, cplat_sleep_ms(_))
        .Times(
            0); // [Pre-Assert確認_異常系] - ローテーション後のオープン失敗時に cplat_sleep_ms() が呼び出されないこと。

    // Act
    int result = cplat_trace_file_sink_write(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "rotate once"); // [手順] - 上限 1 byte のファイルへ書き込み、ローテーション後の open を失敗させる。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        result); // [確認_異常系] - cplat_trace_file_sink_write() の戻り値が CPLAT_OK であること。(完了済みの書き込み結果であることの確認)

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}

// dispose が NULL ハンドルでも安全であることの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_dispose_with_null_handle_is_safe)
{
    // Arrange

    // Pre-Assert

    // Act
    cplat_trace_file_sink_dispose(nullptr); // [手順] - NULL ハンドルで dispose を呼び出す。

    // Assert
    SUCCEED(); // [確認_正常系] - SUCCEED() に到達すること。(NULL を渡した cplat_trace_file_sink_dispose() がクラッシュしないことの確認)
}

// パスに区切り文字が含まれる場合に makedirs が親ディレクトリ パスで呼ばれることの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_create_calls_makedirs_for_path_with_separator)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_makedirs(StrEq("sub"), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - cplat_makedirs() が "sub" と等しい文字列を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_makedirs() から 0 を返却する。
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("sub/trace.log"), open_flags_default(), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - 親ディレクトリの作成後に cplat_file_open() が "sub/trace.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_file_open() から 0 を返却する。

    // Act
    cplat_trace_file_sink *handle =
        cplat_trace_file_sink_create("sub/trace.log", 0, 0, 0); // [手順] - 区切り文字を含むパスで create を呼ぶ。

    // Assert
    EXPECT_NE(nullptr,
              handle); // [確認_正常系] - cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。

    // Cleanup
    EXPECT_CALL(mock_cplat, cplat_file_close(_, _)).Times(AtLeast(1)); // dispose 時の close を許容する。
    // [Pre-Assert確認_正常系] - cplat_file_close() が 1 回以上呼び出されること。
    cplat_trace_file_sink_dispose(handle);
}

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
                 // [Pre-Assert手順] - cplat_makedirs() から 0 を返却する。
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("sub\\dir\\trace.log"), open_flags_default(), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - cplat_file_open() が "sub\\dir\\trace.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。(呼び出し元のパス文字列のまま開くことの確認)
                 // [Pre-Assert手順] - cplat_file_open() から 0 を返却する。

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
    cplat_trace_file_sink *handle =
        cplat_trace_file_sink_create("trace.log", 0, 0, 0); // [手順] - flags なしの create を呼ぶ。
    int result = cplat_trace_file_sink_write(handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
                                             "single"); // [手順] - 1 行書き込む。

    // Assert
    EXPECT_NE(nullptr,
              handle); // [確認_正常系] - cplat_trace_file_sink_create() の戻り値が nullptr 以外であること。
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_trace_file_sink_write() の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}

// 共有モードの create がロック ファイルを開くことの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_create_shared_opens_lock_file)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_default(), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - cplat_file_open() が "trace.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_file_open() から 0 を返却する。
    EXPECT_CALL(mock_cplat, cplat_interprocess_lock_open(StrEq("trace.log.lock"), _))
        .Times(
            1); // [Pre-Assert確認_正常系] - cplat_interprocess_lock_open() が "trace.log.lock" と等しい文字列を指定して 1 回呼び出されること。

    // Act
    cplat_trace_file_sink *handle = cplat_trace_file_sink_create(
        "trace.log", 0, 0, CPLAT_TRACE_FILE_SINK_SHARED); // [手順] - 共有モードで create を呼ぶ。

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

// 共有モードでプロセス間ロックのオープンに失敗した場合に create が NULL を返すことの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_create_shared_returns_null_when_lock_open_fails)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_interprocess_lock_open(StrEq("trace.log.lock"), _))
        .WillOnce(Return(
            CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_異常系] - cplat_interprocess_lock_open() が "trace.log.lock" と等しい文字列を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_interprocess_lock_open() から CPLAT_ERR_UNKNOWN を返却する。
    EXPECT_CALL(mock_cplat, cplat_file_close(_, _))
        .Times(AtLeast(
            1)); // [Pre-Assert確認_異常系] - cplat_file_close() が 1 回以上呼び出されること。(オープン済みファイルを閉じることの確認)

    // Act
    cplat_trace_file_sink *handle = cplat_trace_file_sink_create(
        "trace.log", 0, 0, CPLAT_TRACE_FILE_SINK_SHARED); // [手順] - 共有モードで create を呼ぶ。

    // Assert
    EXPECT_EQ(nullptr,
              handle); // [確認_異常系] - cplat_trace_file_sink_create() の戻り値が nullptr であること。
}

// 負の flags では create が NULL を返すことの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_create_returns_null_for_negative_flags)
{
    // Arrange

    // Pre-Assert

    // Act
    cplat_trace_file_sink *handle =
        cplat_trace_file_sink_create("trace.log", 0, 0, -1); // [手順] - 負の flags で create を呼ぶ。

    // Assert
    EXPECT_EQ(nullptr,
              handle); // [確認_異常系] - cplat_trace_file_sink_create() の戻り値が nullptr であること。
}

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
                // [Pre-Assert手順] - 別実体のファイル同一性を返却する。
    EXPECT_CALL(mock_cplat, cplat_file_close(_, _))
        .Times(1); // [Pre-Assert確認_正常系] - 旧ハンドルの cplat_file_close() が 1 回呼び出されること。
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_default(), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - 開き直しの cplat_file_open() が "trace.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - 開き直しの cplat_file_open() から 0 を返却する。
    EXPECT_CALL(mock_cplat, cplat_file_write(_, _, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 開き直し後の cplat_file_write() が 1 回呼び出されること。
                              // [Pre-Assert手順] - 開き直し後の cplat_file_write() から 0 を返却する。
    EXPECT_CALL(mock_cplat, cplat_file_close(_, _)).Times(1); // dispose 分
    // [Pre-Assert確認_正常系] - dispose 時に cplat_file_close() が 1 回呼び出されること。

    // Act
    int result = cplat_trace_file_sink_write(handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
                                             "after rotate"); // [手順] - 1 行書き込む。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_trace_file_sink_write() の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}

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
        // [Pre-Assert手順] - 別実体のファイル同一性を返却する。
        EXPECT_CALL(mock_cplat, cplat_file_close(_, _))
            .Times(1); // [Pre-Assert確認_異常系] - 旧ハンドルの cplat_file_close() が 1 回呼び出されること。
        EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_default(), _))
            .WillOnce(Return(
                -1)); // [Pre-Assert確認_異常系] - 開き直しの cplat_file_open() が "trace.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。
                      // [Pre-Assert手順] - cplat_file_open() から -1 を返却する。
        EXPECT_CALL(mock_cplat, cplat_file_close(_, _)).Times(1); // dispose 分
        // [Pre-Assert確認_正常系] - dispose 時に cplat_file_close() が 1 回呼び出されること。
    }
    EXPECT_CALL(mock_cplat, cplat_sleep_ms(_))
        .Times(0); // [Pre-Assert確認_異常系] - ファイル オープンの失敗後に cplat_sleep_ms() が呼び出されないこと。
    EXPECT_CALL(mock_cplat, cplat_file_write(_, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - ファイルを開き直せない場合に cplat_file_write() が呼び出されないこと。

    // Act
    int result = cplat_trace_file_sink_write(handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
                                             "after rotate"); // [手順] - 1 行書き込む。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        result); // [確認_異常系] - cplat_trace_file_sink_write() の戻り値が CPLAT_ERR_UNKNOWN であること。(再試行せず直ちに失敗したことの確認)

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}

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
                 // [Pre-Assert手順] - cplat_file_open() から 0 を返却する。
    EXPECT_CALL(mock_cplat, cplat_file_get_size(_, _, _))
        .WillOnce(
            [](const cplat_file *, size_t *size_out, cplat_error *)
            {
                *size_out = 0;
                return 0;
            }); // [Pre-Assert確認_正常系] - create 時に cplat_file_get_size() が 1 回呼び出されること。
                // [Pre-Assert手順] - size_out に 0 を設定し、0 を返却する。
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
                              // [Pre-Assert手順] - cplat_file_write() から 0 を返却する。
    EXPECT_CALL(mock_cplat, cplat_file_get_size(_, _, _))
        .WillOnce(
            [](const cplat_file *, size_t *size_out, cplat_error *)
            {
                *size_out = 10;
                return 0;
            }); // [Pre-Assert確認_正常系] - 書き込み後の cplat_file_get_size() が 1 回呼び出されること。(実サイズがしきい値以上であることの確認)
                // [Pre-Assert手順] - サイズ 10 を返却する。

    // プロセス間ロック下の再確認 → ローテーション
    EXPECT_CALL(mock_cplat, cplat_interprocess_lock_try_lock(_))
        .WillOnce(Return(
            CPLAT_OK)); // [Pre-Assert確認_正常系] - ローテーション前に cplat_interprocess_lock_try_lock() が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_interprocess_lock_try_lock() から CPLAT_OK を返却する。
    EXPECT_CALL(mock_cplat, cplat_file_get_path_id(StrEq("trace.log"), _, _)).Times(1); // ロック下の同一性再確認
    // [Pre-Assert確認_正常系] - ロック下に cplat_file_get_path_id() が "trace.log" と等しい文字列を指定して 1 回呼び出されること。
    EXPECT_CALL(mock_cplat, cplat_file_get_size(_, _, _))
        .WillOnce(
            [](const cplat_file *, size_t *size_out, cplat_error *)
            {
                *size_out = 10;
                return 0;
            }); // [Pre-Assert確認_正常系] - ロック下の cplat_file_get_size() が 1 回呼び出されること。(実サイズを再確認したことの確認)
    // [Pre-Assert手順] - ロック下のサイズ 10 を返却する。
    EXPECT_CALL(mock_cplat, cplat_file_close(_, _)).Times(1); // ローテーション前の close
    // [Pre-Assert確認_正常系] - ローテーション前に cplat_file_close() が 1 回呼び出されること。
    EXPECT_CALL(mock_cplat, cplat_remove(StrEq("trace.log.2"), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - cplat_remove() が "trace.log.2" と等しい文字列を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_remove() から 0 を返却する。
    EXPECT_CALL(mock_cplat, cplat_rename(StrEq("trace.log.1"), StrEq("trace.log.2"), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - cplat_rename() が "trace.log.1" と等しい文字列、"trace.log.2" と等しい文字列を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - 旧世代の rename から 0 を返却する。
    EXPECT_CALL(mock_cplat, cplat_rename(StrEq("trace.log"), StrEq("trace.log.1"), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - cplat_rename() が "trace.log" と等しい文字列、"trace.log.1" と等しい文字列を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - 現在ファイルの rename から 0 を返却する。
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_default(), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - 開き直しの cplat_file_open() が "trace.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。(切り詰めずに追記モードで開き直すことの確認)
                 // [Pre-Assert手順] - 追記モードの open から 0 を返却する。
    EXPECT_CALL(mock_cplat, cplat_file_get_size(_, _, _))
        .WillOnce(
            [](const cplat_file *, size_t *size_out, cplat_error *)
            {
                *size_out = 0;
                return 0;
            }); // [Pre-Assert確認_正常系] - 開き直し時に cplat_file_get_size() が 1 回呼び出されること。
                // [Pre-Assert手順] - size_out に 0 を設定し、0 を返却する。
    EXPECT_CALL(mock_cplat, cplat_file_get_id(_, _, _))
        .Times(
            1); // [Pre-Assert確認_正常系] - 開き直し時に cplat_file_get_id() が 1 回呼び出されること。(開き直したファイルの同一性をキャッシュすることの確認)
    EXPECT_CALL(mock_cplat, cplat_interprocess_lock_unlock(_))
        .WillOnce(Return(
            CPLAT_OK)); // [Pre-Assert確認_正常系] - ローテーション後に cplat_interprocess_lock_unlock() が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_interprocess_lock_unlock() から CPLAT_OK を返却する。
    EXPECT_CALL(mock_cplat, cplat_file_close(_, _)).Times(1); // dispose 分
    // [Pre-Assert確認_正常系] - dispose 時に cplat_file_close() が 1 回呼び出されること。

    // Act
    int result = cplat_trace_file_sink_write(handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
                                             "rotate me"); // [手順] - 上限 1 byte のファイルへ 1 行書き込む。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        result); // [確認_正常系] - cplat_trace_file_sink_write() の戻り値が CPLAT_OK であること。(書き込み後にローテーションが完了したことの確認)

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}

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
                              // [Pre-Assert手順] - cplat_file_write() から 0 を返却する。
    EXPECT_CALL(mock_cplat, cplat_file_get_size(_, _, _))
        .WillOnce(
            [](const cplat_file *, size_t *size_out, cplat_error *)
            {
                *size_out = 10;
                return 0;
            }); // [Pre-Assert確認_正常系] - 書き込み後の cplat_file_get_size() が 1 回呼び出されること。(実サイズがしきい値以上であることの確認)
                // [Pre-Assert手順] - サイズ 10 を返却する。
    EXPECT_CALL(mock_cplat, cplat_interprocess_lock_try_lock(_))
        .WillOnce(
            Return(CPLAT_OK)); // [Pre-Assert確認_正常系] - cplat_interprocess_lock_try_lock() が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_interprocess_lock_try_lock() から CPLAT_OK を返却する。
    EXPECT_CALL(mock_cplat, cplat_file_get_path_id(StrEq("trace.log"), _, _))
        .WillOnce(
            [](const char *, cplat_file_id *id_out, cplat_error *)
            {
                set_file_id(id_out, kDefaultFileIndex + 1); // 他プロセスがローテーション済み
                return 0;
            }); // [Pre-Assert確認_正常系] - ロック下の再確認で cplat_file_get_path_id() が "trace.log" と等しい文字列を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - 別実体のファイル同一性を返却する。
    EXPECT_CALL(mock_cplat, cplat_file_close(_, _)).Times(1); // 開き直し前の close
    // [Pre-Assert確認_正常系] - 開き直し前に cplat_file_close() が 1 回呼び出されること。
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_default(), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - cplat_file_open() が "trace.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。(リネームせず開き直しだけ行うことの確認)
                 // [Pre-Assert手順] - cplat_file_open() から 0 を返却する。
    EXPECT_CALL(mock_cplat, cplat_file_get_size(_, _, _))
        .WillOnce(
            [](const cplat_file *, size_t *size_out, cplat_error *)
            {
                *size_out = 0;
                return 0;
            }); // [Pre-Assert確認_正常系] - 開き直し時に cplat_file_get_size() が 1 回呼び出されること。
                // [Pre-Assert手順] - size_out に 0 を設定し、0 を返却する。
    EXPECT_CALL(mock_cplat, cplat_file_get_id(_, _, _))
        .Times(
            1); // [Pre-Assert確認_正常系] - 開き直し時に cplat_file_get_id() が 1 回呼び出されること。(開き直したファイルの同一性をキャッシュすることの確認)
    EXPECT_CALL(mock_cplat, cplat_interprocess_lock_unlock(_)).WillOnce(Return(CPLAT_OK)); // 開き直し後のロック解放
    // [Pre-Assert確認_正常系] - 開き直し後に cplat_interprocess_lock_unlock() が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_interprocess_lock_unlock() から CPLAT_OK を返却する。
    EXPECT_CALL(mock_cplat, cplat_file_close(_, _)).Times(1); // dispose 分
    // [Pre-Assert確認_正常系] - dispose 時に cplat_file_close() が 1 回呼び出されること。

    // Act
    int result = cplat_trace_file_sink_write(handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
                                             "already rotated"); // [手順] - 1 行書き込む。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_trace_file_sink_write() の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}

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
                // [Pre-Assert手順] - サイズ 10 を返却する。
    EXPECT_CALL(mock_cplat, cplat_interprocess_lock_try_lock(_))
        .WillOnce(Return(
            CPLAT_ERR_BUSY)); // [Pre-Assert確認_異常系] - cplat_interprocess_lock_try_lock() が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_interprocess_lock_try_lock() から CPLAT_ERR_BUSY を返却する。
    EXPECT_CALL(mock_cplat, cplat_rename(_, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - cplat_rename() が呼び出されないこと。
    EXPECT_CALL(mock_cplat, cplat_interprocess_lock_unlock(_))
        .Times(0); // [Pre-Assert確認_異常系] - cplat_interprocess_lock_unlock() が呼び出されないこと。

    // Act
    int result = cplat_trace_file_sink_write(handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
                                             "lock busy"); // [手順] - 1 行書き込む。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        result); // [確認_異常系] - cplat_trace_file_sink_write() の戻り値が CPLAT_OK であること。(ローテーションを見送っても書き込みは成功であることの確認)

    // Cleanup
    cplat_trace_file_sink_dispose(handle);
}

// 同一プロセス内で同一パスの create が同一ハンドルを共有することの確認 (プロセス内調停)
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_create_same_path_shares_handle_in_single_process)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("trace.log"), open_flags_default(), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - cplat_file_open() が "trace.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。(同一パスの create を 2 回呼んでもオープンは 1 回であることの確認)
                 // [Pre-Assert手順] - cplat_file_open() から 0 を返却する。

    // Act
    cplat_trace_file_sink *first =
        cplat_trace_file_sink_create("trace.log", 0, 0, 0); // [手順] - 1 回目の create を呼ぶ。
    cplat_trace_file_sink *second =
        cplat_trace_file_sink_create("trace.log", 0, 0, 0); // [手順] - 同一パスで 2 回目の create を呼ぶ。

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
                // [Pre-Assert手順] - 書き込み内容を確認し、0 を返却する。

    // Act
    cplat_trace_file_sink_dispose(first); // [手順] - 1 人目の利用者が解放する。
    int result = cplat_trace_file_sink_write(second, CPLAT_TRACE_LEVEL_INFO, nullptr,
                                             "after first dispose"); // [手順] - 2 人目の利用者が書き込む。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        result); // [確認_正常系] - 参照が残っている間の cplat_trace_file_sink_write() の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_trace_file_sink_dispose(second);
}

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
        "trace.log", 0, 0, CPLAT_TRACE_FILE_SINK_SHARED); // [手順] - 同一パスを共有モードで create する。

    // Assert
    EXPECT_EQ(
        nullptr,
        second); // [確認_異常系] - 共有モードが一致しないときの cplat_trace_file_sink_create() の戻り値が nullptr であること。

    // Cleanup
    cplat_trace_file_sink_dispose(first);
}

// 異なるパスの create は独立したハンドルを生成することの確認
// [サブ手順参照 名前=trace_fileTest.SetUp]
TEST_F(trace_fileTest, test_create_different_paths_returns_distinct_handles)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("first.log"), open_flags_default(), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - cplat_file_open() が "first.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - 1 つ目の cplat_file_open() から 0 を返却する。
    EXPECT_CALL(mock_cplat, cplat_file_open(_, StrEq("second.log"), open_flags_default(), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - cplat_file_open() が "second.log" と等しい文字列、open_flags_default() の戻り値を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - 2 つ目の cplat_file_open() から 0 を返却する。

    // Act
    cplat_trace_file_sink *first =
        cplat_trace_file_sink_create("first.log", 0, 0, 0); // [手順] - 1 つ目のパスで create を呼ぶ。
    cplat_trace_file_sink *second =
        cplat_trace_file_sink_create("second.log", 0, 0, 0); // [手順] - 2 つ目のパスで create を呼ぶ。

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
