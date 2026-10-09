#include <testfw.h>
#include <cplat/base/result.h>
#include <cplat/clock/clock.h>
#include <mock_cplat.h>
#include <mock_time.h>
#if defined(PLATFORM_WINDOWS)
    #include <mock_windows.h>
#endif
#include <stdint.h>

class clockTest : public Test
{
};

// [サブ手順 名前=clockTest.expect_tm_equal]
static void expect_tm_equal(const struct tm *actual, const struct tm *expected)
{
    EXPECT_EQ(expected->tm_year, actual->tm_year);
    // [確認_正常系] - `actual->tm_year` の値が `expected->tm_year` であること。
    EXPECT_EQ(expected->tm_mon, actual->tm_mon);
    // [確認_正常系] - `actual->tm_mon` の値が `expected->tm_mon` であること。
    EXPECT_EQ(expected->tm_mday, actual->tm_mday);
    // [確認_正常系] - `actual->tm_mday` の値が `expected->tm_mday` であること。
    EXPECT_EQ(expected->tm_hour, actual->tm_hour);
    // [確認_正常系] - `actual->tm_hour` の値が `expected->tm_hour` であること。
    EXPECT_EQ(expected->tm_min, actual->tm_min);
    // [確認_正常系] - `actual->tm_min` の値が `expected->tm_min` であること。
    EXPECT_EQ(expected->tm_sec, actual->tm_sec);
    // [確認_正常系] - `actual->tm_sec` の値が `expected->tm_sec` であること。
}
// [サブ手順終了]

static void set_tm(struct tm *tm_value, int year, int month, int day, int hour, int min, int sec)
{
    memset(tm_value, 0, sizeof(*tm_value));
    tm_value->tm_year = year - 1900;
    tm_value->tm_mon = month - 1;
    tm_value->tm_mday = day;
    tm_value->tm_hour = hour;
    tm_value->tm_min = min;
    tm_value->tm_sec = sec;
}

#if defined(PLATFORM_WINDOWS)
static FILETIME to_filetime(int64_t tv_sec, int32_t tv_nsec)
{
    const uint64_t filetime_units_per_sec = 10000000ULL;
    const int64_t filetime_epoch_offset_sec = 11644473600LL;
    ULARGE_INTEGER uli;
    FILETIME file_time;

    uli.QuadPart = (uint64_t)(tv_sec + filetime_epoch_offset_sec) * filetime_units_per_sec + (uint64_t)(tv_nsec / 100);
    file_time.dwLowDateTime = uli.LowPart;
    file_time.dwHighDateTime = uli.HighPart;

    return file_time;
}
#endif

// 単調増加クロックのミリ秒値がプラットフォーム値から正しく変換されることの確認
TEST_F(clockTest, monotonic_ms_converts_platform_value)
{
    // Arrange

    // Pre-Assert
    // 単調増加クロック取得 API が 1 回呼び出されること。
    // [Pre-Assert手順] - 単調増加クロック取得 API にて 12345 ミリ秒相当の値を返す。
#if defined(PLATFORM_LINUX)
    Mock_time mock_time;

    EXPECT_CALL(mock_time, clock_gettime(_, _, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, clockid_t clk_id, struct timespec *ts)
            {
                EXPECT_EQ(CLOCK_MONOTONIC, clk_id);
                // [確認_正常系] - `clock_gettime` に渡すクロック ID が `CLOCK_MONOTONIC` であること。
                ts->tv_sec = 12;
                ts->tv_nsec = 345678901L;
                return 0;
            });
    // [Pre-Assert確認_正常系] - mock_time の clock_gettime(_, _, _, _, _) が登録した呼び出し期待を満たすこと。
#elif defined(PLATFORM_WINDOWS)
    Mock_windows mock_windows;

    EXPECT_CALL(mock_windows, GetTickCount64(_, _, _)).WillOnce(Return(12345ULL));
    // [Pre-Assert確認_正常系] - mock_windows の GetTickCount64(_, _, _) が登録した呼び出し期待を満たすこと。
#endif

    // Act
    uint64_t actual_ms = cplat_clock_get_monotonic_ms(); // [手順] - cplat_clock_get_monotonic_ms() を呼び出す。

    // Assert
    EXPECT_EQ(12345U, actual_ms); // [確認_正常系] - 単調増加クロックのミリ秒値が 12345 であること。
}

// 単調増加クロックの秒部とナノ秒部がプラットフォーム値から正しく分解されることの確認
TEST_F(clockTest, monotonic_returns_split_platform_value)
{
    // Arrange
    cplat_timespec actual_ts = {-1, -1}; // [状態] - 出力先を未更新値 {-1, -1} で初期化する。

    // Pre-Assert
    // 単調増加クロック取得 API が 1 回呼び出されること。
    // [Pre-Assert手順] - 単調増加クロック取得 API にて 12 秒相当の時刻を返す。
#if defined(PLATFORM_LINUX)
    Mock_time mock_time;

    EXPECT_CALL(mock_time, clock_gettime(_, _, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, clockid_t clk_id, struct timespec *ts)
            {
                EXPECT_EQ(CLOCK_MONOTONIC, clk_id);
                // [確認_正常系] - `clock_gettime` に渡すクロック ID が `CLOCK_MONOTONIC` であること。
                ts->tv_sec = 12;
                ts->tv_nsec = 345678901L;
                return 0;
            });
    // [Pre-Assert確認_正常系] - mock_time の clock_gettime(_, _, _, _, _) が登録した呼び出し期待を満たすこと。
#elif defined(PLATFORM_WINDOWS)
    Mock_windows mock_windows;

    EXPECT_CALL(mock_windows, GetTickCount64(_, _, _)).WillOnce(Return(12345ULL));
    // [Pre-Assert確認_正常系] - mock_windows の GetTickCount64(_, _, _) が登録した呼び出し期待を満たすこと。
#endif

    // Act
    cplat_clock_get_monotonic(&actual_ts); // [手順] - cplat_clock_get_monotonic(&actual_ts) を呼び出す。

    // Assert
    EXPECT_EQ(12, actual_ts.tv_sec); // [確認_正常系] - 秒部が 12 であること。
    // ナノ秒部がプラットフォームごとの期待値 (Linux: 345678901, Windows: 345000000) と一致すること。
#if defined(PLATFORM_LINUX)
    EXPECT_EQ(345678901, actual_ts.tv_nsec);
    // [確認_正常系] - タイムスタンプのナノ秒部 `actual_ts.tv_nsec` が `345678901` であること。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_EQ(345000000, actual_ts.tv_nsec);
    // [確認_正常系] - タイムスタンプのナノ秒部 `actual_ts.tv_nsec` が `345000000` であること。
#endif
}

// 実時刻クロックの秒部とナノ秒部がプラットフォーム値から正しく分解されることの確認
TEST_F(clockTest, realtime_returns_split_platform_value)
{
    // Arrange
    const int64_t expected_sec = 1712345678LL; // [状態] - 実時刻の秒部期待値を 1712345678 とする。
    cplat_timespec actual_ts = {-1, -1};    // [状態] - 出力先を未更新値 {-1, -1} で初期化する。
    // [状態] - プラットフォームに応じたナノ秒部期待値を expected_nsec に設定する。

    // Pre-Assert
    // 実時刻取得 API が 1 回呼び出されること。
    // [Pre-Assert手順] - 実時刻取得 API にて期待する秒部とナノ秒部を返す。
#if defined(PLATFORM_LINUX)
    const int32_t expected_nsec = 987654321;
    Mock_time mock_time;

    EXPECT_CALL(mock_time, clock_gettime(_, _, _, _, _))
        .WillOnce(
            [&](const char *, const int, const char *, clockid_t clk_id, struct timespec *ts)
            {
                EXPECT_EQ(CLOCK_REALTIME, clk_id);
                // [確認_正常系] - `clock_gettime` に渡すクロック ID が `CLOCK_REALTIME` であること。
                ts->tv_sec = expected_sec;
                ts->tv_nsec = expected_nsec;
                return 0;
            });
    // [Pre-Assert確認_正常系] - mock_time の clock_gettime(_, _, _, _, _) が登録した呼び出し期待を満たすこと。
#elif defined(PLATFORM_WINDOWS)
    const int32_t expected_nsec = 987654300;
    Mock_windows mock_windows;

    EXPECT_CALL(mock_windows, GetSystemTimeAsFileTime(_, _, _, _))
        .WillOnce([&](const char *, const int, const char *, LPFILETIME file_time)
                  { *file_time = to_filetime(expected_sec, expected_nsec); });
    // [Pre-Assert確認_正常系] - mock_windows の GetSystemTimeAsFileTime(_, _, _, _) が登録した呼び出し期待を満たすこと。
#endif

    // Act
    cplat_clock_get_realtime(&actual_ts); // [手順] - cplat_clock_get_realtime(&actual_ts) を呼び出す。

    // Assert
    EXPECT_EQ(expected_sec, actual_ts.tv_sec); // [確認_正常系] - 秒部が期待値と一致すること。
    EXPECT_EQ(expected_nsec,
              actual_ts.tv_nsec); // [確認_正常系] - ナノ秒部がプラットフォームに応じた期待値と一致すること。
}

// 実時刻が UTC 分解結果とナノ秒部へ正しく変換されることの確認
TEST_F(clockTest, realtime_utc_uses_platform_conversion_result)
{
    // Arrange
    const int64_t expected_sec = 1712345678LL; // [状態] - 実時刻の秒部期待値を 1712345678 とする。
    const int32_t expected_nsec = 246800000;   // [状態] - 実時刻ナノ秒部として 246800000 を返す前提とする。
    struct tm expected_tm = {};                // [状態] - UTC 変換後の期待値を格納する構造体を 0 初期化する。
    struct tm actual_tm = {};                  // [状態] - 実際の UTC 変換結果を受け取る構造体を 0 初期化する。
    int32_t actual_nsec = -1;                  // [状態] - ナノ秒部の出力先を未更新値 -1 で初期化する。
    Mock_cplat mock_cplat;

    // Pre-Assert
    // 実時刻取得 API が 1 回呼び出されること。
    // [Pre-Assert手順] - 実時刻取得 API にて期待する秒部とナノ秒部を返す。
#if defined(PLATFORM_LINUX)
    Mock_time mock_time;

    EXPECT_CALL(mock_time, clock_gettime(_, _, _, _, _))
        .WillOnce(
            [&](const char *, const int, const char *, clockid_t clk_id, struct timespec *ts)
            {
                EXPECT_EQ(CLOCK_REALTIME, clk_id);
                // [確認_正常系] - `clock_gettime` に渡すクロック ID が `CLOCK_REALTIME` であること。
                ts->tv_sec = expected_sec;
                ts->tv_nsec = expected_nsec;
                return 0;
            });
    // [Pre-Assert確認_正常系] - mock_time の clock_gettime(_, _, _, _, _) が登録した呼び出し期待を満たすこと。
#elif defined(PLATFORM_WINDOWS)
    Mock_windows mock_windows;

    EXPECT_CALL(mock_windows, GetSystemTimeAsFileTime(_, _, _, _))
        .WillOnce([&](const char *, const int, const char *, LPFILETIME file_time)
                  { *file_time = to_filetime(expected_sec, expected_nsec); });
    // [Pre-Assert確認_正常系] - mock_windows の GetSystemTimeAsFileTime(_, _, _, _) が登録した呼び出し期待を満たすこと。
#endif

    EXPECT_CALL(mock_cplat, cplat_gmtime(_, _))
        .WillOnce(
            [&](struct tm *utc_tm, const time_t *timep)
            {
                EXPECT_EQ((time_t)expected_sec, *timep);
                // [確認_正常系] - `cplat_gmtime` に渡す時刻の秒数が `(time_t)expected_sec` であること。
                expected_tm.tm_year = 124;
                expected_tm.tm_mon = 3;
                expected_tm.tm_mday = 5;
                expected_tm.tm_hour = 6;
                expected_tm.tm_min = 7;
                expected_tm.tm_sec = 8;
                *utc_tm = expected_tm;
                return 0;
            }); // [Pre-Assert確認_正常系] - cplat_gmtime(&actual_tm, &realtime_time) が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_gmtime() にて UTC 分解結果として 2024-04-05 06:07:08 相当を設定し、成功を返す。

    // Act
    cplat_clock_get_realtime_utc(
        &actual_tm, &actual_nsec); // [手順] - cplat_clock_get_realtime_utc(&actual_tm, &actual_nsec) を呼び出す。

    // Assert
    // [サブ手順参照 名前=clockTest.expect_tm_equal]
    expect_tm_equal(&actual_tm,
                    &expected_tm);         // UTC 分解結果が cplat_gmtime() の設定値と一致すること。
    EXPECT_EQ(expected_nsec, actual_nsec); // [確認_正常系] - ナノ秒部が 246800000 のまま返ること。
}

// UTC 変換が失敗した場合に struct tm が 0 初期化されることの確認
TEST_F(clockTest, realtime_utc_zeroes_tm_when_cplat_gmtime_fails)
{
    // Arrange
    const int64_t expected_sec = 1712345678LL; // [状態] - 実時刻の秒部期待値を 1712345678 とする。
    const int32_t expected_nsec = 246800000;   // [状態] - 実時刻ナノ秒部として 246800000 を返す前提とする。
    struct tm actual_tm = {};                  // [状態] - 実際の UTC 変換結果を受け取る構造体を初期化する。
    struct tm expected_tm = {};                // [状態] - 失敗時に 0 初期化される期待値を用意する。
    int32_t actual_nsec = -1;                  // [状態] - ナノ秒部の出力先を未更新値 -1 で初期化する。
    Mock_cplat mock_cplat;

    actual_tm.tm_year = 1; // [状態] - 失敗時に上書きされたことが分かるよう、year を 1 に設定する。
    actual_tm.tm_mon = 2;  // [状態] - 失敗時に上書きされたことが分かるよう、mon を 2 に設定する。
    actual_tm.tm_mday = 3; // [状態] - 失敗時に上書きされたことが分かるよう、mday を 3 に設定する。
    actual_tm.tm_hour = 4; // [状態] - 失敗時に上書きされたことが分かるよう、hour を 4 に設定する。
    actual_tm.tm_min = 5;  // [状態] - 失敗時に上書きされたことが分かるよう、min を 5 に設定する。
    actual_tm.tm_sec = 6;  // [状態] - 失敗時に上書きされたことが分かるよう、sec を 6 に設定する。

    // Pre-Assert
    // 実時刻取得 API が 1 回呼び出されること。
    // [Pre-Assert手順] - 実時刻取得 API にて期待する秒部とナノ秒部を返す。
#if defined(PLATFORM_LINUX)
    Mock_time mock_time;

    EXPECT_CALL(mock_time, clock_gettime(_, _, _, _, _))
        .WillOnce(
            [&](const char *, const int, const char *, clockid_t clk_id, struct timespec *ts)
            {
                EXPECT_EQ(CLOCK_REALTIME, clk_id);
                // [確認_正常系] - `clock_gettime` に渡すクロック ID が `CLOCK_REALTIME` であること。
                ts->tv_sec = expected_sec;
                ts->tv_nsec = expected_nsec;
                return 0;
            });
    // [Pre-Assert確認_異常系] - mock_time の clock_gettime(_, _, _, _, _) が登録した呼び出し期待を満たすこと。
#elif defined(PLATFORM_WINDOWS)
    Mock_windows mock_windows;

    EXPECT_CALL(mock_windows, GetSystemTimeAsFileTime(_, _, _, _))
        .WillOnce([&](const char *, const int, const char *, LPFILETIME file_time)
                  { *file_time = to_filetime(expected_sec, expected_nsec); });
    // [Pre-Assert確認_異常系] - mock_windows の GetSystemTimeAsFileTime(_, _, _, _) が登録した呼び出し期待を満たすこと。
#endif

    EXPECT_CALL(mock_cplat, cplat_gmtime(_, _))
        .WillOnce(
            [&](struct tm *utc_tm, const time_t *timep)
            {
                EXPECT_EQ((time_t)expected_sec, *timep);
                // [確認_正常系] - `cplat_gmtime` に渡す時刻の秒数が `(time_t)expected_sec` であること。
                EXPECT_EQ(&actual_tm, utc_tm);
                // [確認_正常系] - `cplat_gmtime` に渡す格納先が `actual_tm` のアドレスであること。
                return -1;
            }); // [Pre-Assert確認_異常系] - cplat_gmtime(&actual_tm, &realtime_time) が 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_gmtime() にて失敗を返し、clock.c 側の 0 初期化処理へ進ませる。

    // Act
    cplat_clock_get_realtime_utc(
        &actual_tm, &actual_nsec); // [手順] - cplat_clock_get_realtime_utc(&actual_tm, &actual_nsec) を呼び出す。

    // Assert
    // [サブ手順参照 名前=clockTest.expect_tm_equal 区分=異常系]
    expect_tm_equal(&actual_tm, &expected_tm); // UTC 分解結果がすべて 0 に初期化されること。
    EXPECT_EQ(expected_nsec, actual_nsec);     // [確認_異常系] - ナノ秒部は取得済みの値 246800000 を保持すること。
}

// ローカル時刻の ISO 8601 文字列に UTC オフセットとミリ秒が出力されることの確認
TEST_F(clockTest, format_realtime_iso8601_local_outputs_offset_and_milliseconds)
{
    // Arrange
    const cplat_timespec timestamp = {1712297228LL,
                                         246800000LL}; // [状態] - 変換対象を {1712297228, 246800000} とする。
    char actual[CPLAT_CLOCK_ISO8601_LOCAL_MSEC_LEN + 1];
    struct tm local_tm;
    struct tm utc_tm;
    Mock_cplat mock_cplat;

    set_tm(&local_tm, 2024, 4, 5, 15, 7, 8); // [状態] - ローカル時刻を 2024-04-05 15:07:08 とする。
    set_tm(&utc_tm, 2024, 4, 5, 6, 7, 8);    // [状態] - UTC を 2024-04-05 06:07:08 (オフセット +09:00 相当) とする。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_localtime(_, _))
        .WillOnce(
            [&](struct tm *tm_value, const time_t *timep)
            {
                EXPECT_EQ(timestamp.tv_sec, *timep);
                // [確認_正常系] - `cplat_localtime` に渡す時刻の秒数が `timestamp.tv_sec` であること。
                *tm_value = local_tm;
                return 0;
            }); // [Pre-Assert確認_正常系] - cplat_localtime が対象秒で 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_localtime からローカル時刻の分解値を返却する。
    EXPECT_CALL(mock_cplat, cplat_gmtime(_, _))
        .WillOnce(
            [&](struct tm *tm_value, const time_t *timep)
            {
                EXPECT_EQ(timestamp.tv_sec, *timep);
                // [確認_正常系] - `cplat_gmtime` に渡す時刻の秒数が `timestamp.tv_sec` であること。
                *tm_value = utc_tm;
                return 0;
            }); // [Pre-Assert確認_正常系] - cplat_gmtime が対象秒で 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_gmtime から UTC の分解値を返却する。

    // Act
    int actual_ret_format_realtime_iso8601_local = cplat_clock_format_realtime_iso8601_local(
        actual, sizeof(actual), &timestamp); // [手順] - cplat_clock_format_realtime_iso8601_local を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_format_realtime_iso8601_local); // [確認_正常系] - cplat_clock_format_realtime_iso8601_local の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("2024-04-05T15:07:08.246+09:00",
                 actual); // [確認_正常系] - オフセット +09:00 とミリ秒 246 を含む文字列になること。
}

// 負の UTC オフセットが正しく出力されることの確認
TEST_F(clockTest, format_realtime_iso8601_local_supports_negative_offset)
{
    // Arrange
    const cplat_timespec timestamp = {1712297228LL,
                                         135000000LL}; // [状態] - 変換対象を {1712297228, 135000000} とする。
    char actual[CPLAT_CLOCK_ISO8601_LOCAL_MSEC_LEN + 1];
    struct tm local_tm;
    struct tm utc_tm;
    Mock_cplat mock_cplat;

    set_tm(&local_tm, 2024, 4, 5, 0, 37, 8); // [状態] - ローカル時刻を 2024-04-05 00:37:08 とする。
    set_tm(&utc_tm, 2024, 4, 5, 6, 7, 8);    // [状態] - UTC を 2024-04-05 06:07:08 (オフセット -05:30 相当) とする。

    // Pre-Assert
    // [Pre-Assert手順] - cplat_localtime と cplat_gmtime からそれぞれの分解値を返却する。
    EXPECT_CALL(mock_cplat, cplat_localtime(_, _))
        .WillOnce(
            [&](struct tm *tm_value, const time_t *)
            {
                *tm_value = local_tm;
                return 0;
            });
    // [Pre-Assert確認_正常系] - mock_cplat の cplat_localtime(_, _) が登録した呼び出し期待を満たすこと。
    EXPECT_CALL(mock_cplat, cplat_gmtime(_, _))
        .WillOnce(
            [&](struct tm *tm_value, const time_t *)
            {
                *tm_value = utc_tm;
                return 0;
            }); // [Pre-Assert確認_正常系] - cplat_gmtime が対象秒で 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_gmtime から UTC の分解値を返却する。

    // Act
    int actual_ret_format_realtime_iso8601_local = cplat_clock_format_realtime_iso8601_local(
        actual, sizeof(actual), &timestamp); // [手順] - cplat_clock_format_realtime_iso8601_local を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_format_realtime_iso8601_local); // [確認_正常系] - cplat_clock_format_realtime_iso8601_local の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("2024-04-05T00:37:08.135-05:30",
                 actual); // [確認_正常系] - 負のオフセット -05:30 を含む文字列になること。
}

// UTC の ISO 8601 文字列に "Z" サフィックスが出力されることの確認
TEST_F(clockTest, format_realtime_iso8601_utc_outputs_z_suffix)
{
    // Arrange
    const cplat_timespec timestamp = {1712297228LL,
                                         987000000LL}; // [状態] - 変換対象を {1712297228, 987000000} とする。
    char actual[CPLAT_CLOCK_ISO8601_UTC_MSEC_LEN + 1];
    struct tm utc_tm;
    Mock_cplat mock_cplat;

    set_tm(&utc_tm, 2024, 4, 5, 6, 7, 8); // [状態] - UTC を 2024-04-05 06:07:08 とする。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_gmtime(_, _))
        .WillOnce(
            [&](struct tm *tm_value, const time_t *timep)
            {
                EXPECT_EQ(timestamp.tv_sec, *timep);
                // [確認_正常系] - `cplat_gmtime` に渡す時刻の秒数が `timestamp.tv_sec` であること。
                *tm_value = utc_tm;
                return 0;
            }); // [Pre-Assert確認_正常系] - cplat_gmtime が対象秒で 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_gmtime から UTC の分解値を返却する。

    // Act
    int actual_ret_format_realtime_iso8601_utc = cplat_clock_format_realtime_iso8601_utc(
        actual, sizeof(actual), &timestamp); // [手順] - cplat_clock_format_realtime_iso8601_utc を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_format_realtime_iso8601_utc); // [確認_正常系] - cplat_clock_format_realtime_iso8601_utc の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("2024-04-05T06:07:08.987Z", actual); // [確認_正常系] - "Z" サフィックス付きの UTC 文字列になること。
}

// nsec が不正な場合に CPLAT_ERR_INVALID_ARGUMENT を返しゼロ埋め文字列へフォールバックすることの確認
TEST_F(clockTest, format_realtime_iso8601_local_falls_back_when_nsec_is_invalid)
{
    // Arrange
    const cplat_timespec invalid_timestamp = {
        0, 1000000000LL}; // [状態] - nsec が 10 億の不正なタイムスタンプを用意する。
    char actual[CPLAT_CLOCK_ISO8601_LOCAL_MSEC_LEN + 1];

    // Pre-Assert

    // Act
    int actual_ret_format_realtime_iso8601_local = cplat_clock_format_realtime_iso8601_local(
        actual, sizeof(actual),
        &invalid_timestamp); // [手順] - 不正なタイムスタンプで cplat_clock_format_realtime_iso8601_local を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_format_realtime_iso8601_local); // [確認_異常系] - cplat_clock_format_realtime_iso8601_local の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_STREQ("0000-00-00T00:00:00.000+00:00", actual); // [確認_異常系] - ゼロ埋めのフォールバック文字列になること。
}

// 不正な nsec と NULL バッファーを local formatter が拒否することの確認
TEST_F(clockTest, format_realtime_iso8601_local_rejects_invalid_arguments)
{
    // Arrange
    const cplat_timespec invalid_timestamp = {0, -1};

    // Pre-Assert

    // Act
    int result = cplat_clock_format_realtime_iso8601_local(
        NULL, 0U, &invalid_timestamp); // [手順] - NULL バッファーと不正な nsec で local formatter を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              result); // [確認_異常系] - 不正な引数が INVALID_ARGUMENT として通知されること。
}

// NULL タイムスタンプとサイズ 0 のバッファーを local formatter が拒否することの確認
TEST_F(clockTest, format_realtime_iso8601_local_rejects_null_timestamp)
{
    // Arrange
    char actual = 'x'; // [状態] - サイズ 0 の出力先が変更されないことを確認するため 'x' で初期化する。

    // Pre-Assert

    // Act
    int result = cplat_clock_format_realtime_iso8601_local(
        &actual, 0u, NULL); // [手順] - NULL タイムスタンプとサイズ 0 のバッファーで local formatter を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        result); // [確認_異常系] - NULL タイムスタンプを渡した cplat_clock_format_realtime_iso8601_local の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ('x', actual); // [確認_異常系] - サイズ 0 の出力先が変更されないこと。
}

// UTC formatter が不正な nsec に対してフォールバックすることの確認
TEST_F(clockTest, format_realtime_iso8601_utc_falls_back_when_nsec_is_invalid)
{
    // Arrange
    const cplat_timespec invalid_timestamp = {0, 1000000000LL};
    char actual[CPLAT_CLOCK_ISO8601_UTC_MSEC_LEN + 1];

    // Pre-Assert

    // Act
    int result = cplat_clock_format_realtime_iso8601_utc(
        actual, sizeof(actual), &invalid_timestamp); // [手順] - 不正な nsec で UTC formatter を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              result); // [確認_異常系] - 不正な nsec が INVALID_ARGUMENT として通知されること。
    EXPECT_STREQ("0000-00-00T00:00:00.000Z",
                 actual); // [確認_異常系] - UTC のフォールバック文字列になること。
}

// UTC formatter が NULL タイムスタンプと負のナノ秒を拒否することの確認
TEST_F(clockTest, format_realtime_iso8601_utc_rejects_invalid_timestamp_fields)
{
    // Arrange
    const cplat_timespec negative_nsec = {0, -1}; // [状態] - ナノ秒部が -1 のタイムスタンプを用意する。
    char null_actual[CPLAT_CLOCK_ISO8601_UTC_MSEC_LEN + 1];
    char negative_actual[CPLAT_CLOCK_ISO8601_UTC_MSEC_LEN + 1];

    // Pre-Assert

    // Act
    int null_result = cplat_clock_format_realtime_iso8601_utc(
        null_actual, sizeof(null_actual), NULL); // [手順] - NULL タイムスタンプで UTC formatter を呼び出す。
    int negative_result = cplat_clock_format_realtime_iso8601_utc(
        negative_actual, sizeof(negative_actual),
        &negative_nsec); // [手順] - 負のナノ秒を指定して UTC formatter を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_result); // [確認_異常系] - NULL タイムスタンプを渡した cplat_clock_format_realtime_iso8601_utc の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_STREQ("0000-00-00T00:00:00.000Z",
                 null_actual); // [確認_異常系] - NULL タイムスタンプに対して UTC のフォールバック文字列が返ること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        negative_result); // [確認_異常系] - 負のナノ秒を渡した cplat_clock_format_realtime_iso8601_utc の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_STREQ("0000-00-00T00:00:00.000Z",
                 negative_actual); // [確認_異常系] - 負のナノ秒に対して UTC のフォールバック文字列が返ること。
}

// localtime が失敗した場合に local formatter がフォールバックすることの確認
TEST_F(clockTest, format_realtime_iso8601_local_falls_back_when_localtime_fails)
{
    // Arrange
    const cplat_timespec timestamp = {1712297228LL, 123000000LL};
    char actual[CPLAT_CLOCK_ISO8601_LOCAL_MSEC_LEN + 1];
    Mock_cplat mock_cplat;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_localtime(_, _))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - cplat_localtime が 1 回呼び出されること。
                               // [Pre-Assert手順] - cplat_localtime から -1 を返却する。

    // Act
    int result = cplat_clock_format_realtime_iso8601_local(
        actual, sizeof(actual), &timestamp); // [手順] - localtime が失敗する状態で local formatter を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        result); // [確認_異常系] - localtime 失敗時の cplat_clock_format_realtime_iso8601_local の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_STREQ("0000-00-00T00:00:00.000+00:00",
                 actual); // [確認_異常系] - localtime 失敗時に local のフォールバック文字列が返ること。
}

// gmtime が失敗した場合に local formatter がフォールバックすることの確認
TEST_F(clockTest, format_realtime_iso8601_local_falls_back_when_gmtime_fails)
{
    // Arrange
    const cplat_timespec timestamp = {1712297228LL, 123000000LL};
    char actual[CPLAT_CLOCK_ISO8601_LOCAL_MSEC_LEN + 1];
    struct tm local_tm;
    Mock_cplat mock_cplat;

    set_tm(&local_tm, 2024, 4, 5, 15, 7, 8); // [状態] - localtime が返す分解時刻を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_localtime(_, _))
        .WillOnce(
            [&](struct tm *tm_value, const time_t *)
            {
                *tm_value = local_tm;
                return 0;
            }); // [Pre-Assert確認_正常系] - cplat_localtime が 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_localtime から用意した分解時刻を返却する。
    EXPECT_CALL(mock_cplat, cplat_gmtime(_, _))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - cplat_gmtime が 1 回呼び出されること。
                               // [Pre-Assert手順] - cplat_gmtime から -1 を返却する。

    // Act
    int result = cplat_clock_format_realtime_iso8601_local(
        actual, sizeof(actual), &timestamp); // [手順] - gmtime が失敗する状態で local formatter を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        result); // [確認_異常系] - gmtime 失敗時の cplat_clock_format_realtime_iso8601_local の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_STREQ("0000-00-00T00:00:00.000+00:00",
                 actual); // [確認_異常系] - gmtime 失敗時に local のフォールバック文字列が返ること。
}

// gmtime が失敗した場合に CPLAT_ERR_UNKNOWN を返しゼロ埋め文字列へフォールバックすることの確認
TEST_F(clockTest, format_realtime_iso8601_utc_falls_back_when_gmtime_fails)
{
    // Arrange
    const cplat_timespec timestamp = {1712297228LL,
                                         123000000LL}; // [状態] - 変換対象を {1712297228, 123000000} とする。
    char actual[CPLAT_CLOCK_ISO8601_UTC_MSEC_LEN + 1];
    Mock_cplat mock_cplat;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_gmtime(_, _))
        .WillOnce(
            [&](struct tm *tm_value, const time_t *timep)
            {
                EXPECT_EQ(timestamp.tv_sec, *timep);
                // [確認_正常系] - `cplat_gmtime` に渡す時刻の秒数が `timestamp.tv_sec` であること。
                EXPECT_NE((struct tm *)NULL, tm_value);
                // [確認_正常系] - `(struct tm *)NULL` と `tm_value` が異なること。
                return -1;
            }); // [Pre-Assert確認_異常系] - cplat_gmtime が対象秒で 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_gmtime から -1 を返却する。

    // Act
    int actual_ret_format_realtime_iso8601_utc = cplat_clock_format_realtime_iso8601_utc(
        actual, sizeof(actual), &timestamp); // [手順] - cplat_clock_format_realtime_iso8601_utc を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_format_realtime_iso8601_utc); // [確認_異常系] - cplat_clock_format_realtime_iso8601_utc の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_STREQ("0000-00-00T00:00:00.000Z", actual); // [確認_異常系] - ゼロ埋めのフォールバック文字列になること。
}

// ISO 8601 local formatter が小さいバッファーを検出してフォールバックすることの確認
TEST_F(clockTest, format_realtime_iso8601_local_rejects_small_buffer)
{
    // Arrange
    const cplat_timespec timestamp = {1712297228LL, 123000000LL};
    char actual[1] = {'x'};
    struct tm local_tm;
    struct tm utc_tm;
    Mock_cplat mock_cplat;
    set_tm(&local_tm, 2024, 4, 5, 15, 7, 8);
    set_tm(&utc_tm, 2024, 4, 5, 6, 7, 8);

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_localtime(_, _))
        .WillOnce(
            [&](struct tm *tm_value, const time_t *)
            {
                *tm_value = local_tm;
                return 0;
            }); // [Pre-Assert確認_正常系] - cplat_localtime が 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_localtime から用意した分解時刻を返却する。
    EXPECT_CALL(mock_cplat, cplat_gmtime(_, _))
        .WillOnce(
            [&](struct tm *tm_value, const time_t *)
            {
                *tm_value = utc_tm;
                return 0;
            }); // [Pre-Assert確認_正常系] - cplat_gmtime が 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_gmtime から用意した分解時刻を返却する。

    // Act
    int result = cplat_clock_format_realtime_iso8601_local(
        actual, sizeof(actual), &timestamp); // [手順] - 小さいバッファーで local formatter を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - 小さいバッファーが UNKNOWN として通知されること。
    EXPECT_EQ('\0', actual[0]); // [確認_異常系] - フォールバック文字列の先頭だけが格納されること。
}

// ISO 8601 UTC formatter が小さいバッファーを検出してフォールバックすることの確認
TEST_F(clockTest, format_realtime_iso8601_utc_rejects_small_buffer)
{
    // Arrange
    const cplat_timespec timestamp = {1712297228LL, 123000000LL};
    char actual[1] = {'x'};
    struct tm utc_tm;
    Mock_cplat mock_cplat;
    set_tm(&utc_tm, 2024, 4, 5, 6, 7, 8);

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_gmtime(_, _))
        .WillOnce(
            [&](struct tm *tm_value, const time_t *)
            {
                *tm_value = utc_tm;
                return 0;
            }); // [Pre-Assert確認_正常系] - cplat_gmtime が 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_gmtime から用意した分解時刻を返却する。

    // Act
    int result = cplat_clock_format_realtime_iso8601_utc(
        actual, sizeof(actual), &timestamp); // [手順] - 小さいバッファーで UTC formatter を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - 小さいバッファーが UNKNOWN として通知されること。
    EXPECT_EQ('\0', actual[0]); // [確認_異常系] - フォールバック文字列の先頭だけが格納されること。
}

// 有効なタイムスタンプでも NULL バッファーを local formatter が拒否することの確認
TEST_F(clockTest, format_realtime_iso8601_local_rejects_null_buffer)
{
    // Arrange
    const cplat_timespec timestamp = {1712297228LL, 123000000LL};
    struct tm local_tm;
    struct tm utc_tm;
    Mock_cplat mock_cplat;

    set_tm(&local_tm, 2024, 4, 5, 15, 7, 8); // [状態] - localtime が返す分解時刻を用意する。
    set_tm(&utc_tm, 2024, 4, 5, 6, 7, 8);    // [状態] - gmtime が返す分解時刻を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_localtime(_, _))
        .WillOnce(
            [&](struct tm *tm_value, const time_t *)
            {
                *tm_value = local_tm;
                return 0;
            }); // [Pre-Assert確認_正常系] - cplat_localtime が 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_localtime から用意した分解時刻を返却する。
    EXPECT_CALL(mock_cplat, cplat_gmtime(_, _))
        .WillOnce(
            [&](struct tm *tm_value, const time_t *)
            {
                *tm_value = utc_tm;
                return 0;
            }); // [Pre-Assert確認_正常系] - cplat_gmtime が 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_gmtime から用意した分解時刻を返却する。

    // Act
    int result =
        cplat_clock_format_realtime_iso8601_local(NULL, CPLAT_CLOCK_ISO8601_LOCAL_MSEC_LEN + 1u,
                                               &timestamp); // [手順] - NULL バッファーで local formatter を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        result); // [確認_異常系] - NULL バッファーを渡した cplat_clock_format_realtime_iso8601_local の戻り値が CPLAT_ERR_UNKNOWN であること。
}

// 有効なタイムスタンプでも NULL バッファーを UTC formatter が拒否することの確認
TEST_F(clockTest, format_realtime_iso8601_utc_rejects_null_buffer)
{
    // Arrange
    const cplat_timespec timestamp = {1712297228LL, 123000000LL};
    struct tm utc_tm;
    Mock_cplat mock_cplat;

    set_tm(&utc_tm, 2024, 4, 5, 6, 7, 8); // [状態] - gmtime が返す分解時刻を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_gmtime(_, _))
        .WillOnce(
            [&](struct tm *tm_value, const time_t *)
            {
                *tm_value = utc_tm;
                return 0;
            }); // [Pre-Assert確認_正常系] - cplat_gmtime が 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_gmtime から用意した分解時刻を返却する。

    // Act
    int result =
        cplat_clock_format_realtime_iso8601_utc(NULL, CPLAT_CLOCK_ISO8601_UTC_MSEC_LEN + 1u,
                                             &timestamp); // [手順] - NULL バッファーで UTC formatter を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        result); // [確認_異常系] - NULL バッファーを渡した cplat_clock_format_realtime_iso8601_utc の戻り値が CPLAT_ERR_UNKNOWN であること。
}

// 年初および紀元前相当の時刻を local formatter が処理できることの確認
TEST_F(clockTest, format_realtime_iso8601_local_supports_early_date)
{
    // Arrange
    const cplat_timespec timestamp = {0, 0};
    char actual[CPLAT_CLOCK_ISO8601_LOCAL_MSEC_LEN + 1];
    struct tm local_tm;
    struct tm utc_tm;
    Mock_cplat mock_cplat;
    set_tm(&local_tm, -1, 1, 1, 0, 0, 0);
    set_tm(&utc_tm, -1, 1, 1, 0, 0, 0);

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_localtime(_, _))
        .WillOnce(
            [&](struct tm *tm_value, const time_t *)
            {
                *tm_value = local_tm;
                return 0;
            }); // [Pre-Assert確認_正常系] - cplat_localtime が 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_localtime から用意した分解時刻を返却する。
    EXPECT_CALL(mock_cplat, cplat_gmtime(_, _))
        .WillOnce(
            [&](struct tm *tm_value, const time_t *)
            {
                *tm_value = utc_tm;
                return 0;
            }); // [Pre-Assert確認_正常系] - cplat_gmtime が 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_gmtime から用意した分解時刻を返却する。

    // Act
    int result = cplat_clock_format_realtime_iso8601_local(
        actual, sizeof(actual), &timestamp); // [手順] - 年初の早い日付で local formatter を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - 年初の早い日付を正常に整形できること。
}

// 実時刻 deadline 計算でナノ秒 overflow がない場合にそのまま加算されることの確認
TEST_F(clockTest, realtime_deadline_ms_adds_timeout_without_nsec_carry)
{
    // Arrange
    const uint64_t timeout_ms = 250;      // [状態] - deadline に加算するタイムアウトを 250 ミリ秒とする。
    const time_t expected_sec = 100;      // [状態] - deadline の秒部期待値を 100 とする。
    const long expected_nsec = 350000000; // [状態] - deadline のナノ秒部期待値を 350000000 とする。
    struct timespec abs_timeout = {};     // [状態] - 計算結果の格納先を 0 初期化する。

    // Pre-Assert
    // 実時刻取得 API が 1 回呼び出されること。
    // [Pre-Assert手順] - 実時刻取得 API にて 100 秒台の時刻を返す。
#if defined(PLATFORM_LINUX)
    Mock_time mock_time;

    EXPECT_CALL(mock_time, clock_gettime(_, _, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, clockid_t clk_id, struct timespec *ts)
            {
                EXPECT_EQ(CLOCK_REALTIME, clk_id);
                // [確認_正常系] - `clock_gettime` に渡すクロック ID が `CLOCK_REALTIME` であること。
                ts->tv_sec = 100;
                ts->tv_nsec = 100000000L;
                return 0;
            });
    // [Pre-Assert確認_正常系] - mock_time の clock_gettime(_, _, _, _, _) が登録した呼び出し期待を満たすこと。
#elif defined(PLATFORM_WINDOWS)
    Mock_windows mock_windows;

    EXPECT_CALL(mock_windows, GetSystemTimeAsFileTime(_, _, _, _))
        .WillOnce([](const char *, const int, const char *, LPFILETIME file_time)
                  { *file_time = to_filetime(100, 100000000); });
    // [Pre-Assert確認_正常系] - mock_windows の GetSystemTimeAsFileTime(_, _, _, _) が登録した呼び出し期待を満たすこと。
#endif

    // Act
    cplat_clock_get_realtime_deadline_ms(
        timeout_ms, &abs_timeout); // [手順] - cplat_clock_get_realtime_deadline_ms(timeout_ms, &abs_timeout) を呼び出す。

    // Assert
    EXPECT_EQ(expected_sec, abs_timeout.tv_sec);   // [確認_正常系] - 秒繰り上がりなしで秒部が 100 のままであること。
    EXPECT_EQ(expected_nsec, abs_timeout.tv_nsec); // [確認_正常系] - ナノ秒部が 350000000 になること。
}

// 実時刻 deadline 計算でナノ秒 overflow が発生した場合に秒繰り上がりすることの確認
TEST_F(clockTest, realtime_deadline_ms_carries_nsec_overflow)
{
    // Arrange
    const uint64_t timeout_ms = 250;     // [状態] - deadline に加算するタイムアウトを 250 ミリ秒とする。
    const time_t expected_sec = 101;     // [状態] - overflow 後の秒部期待値を 101 とする。
    const long expected_nsec = 50000000; // [状態] - overflow 後のナノ秒部期待値を 50000000 とする。
    struct timespec abs_timeout = {};    // [状態] - 計算結果の格納先を 0 初期化する。

    // Pre-Assert
    // 実時刻取得 API が 1 回呼び出されること。
    // [Pre-Assert手順] - 実時刻取得 API にて 100 秒台の時刻を返す。
#if defined(PLATFORM_LINUX)
    Mock_time mock_time;

    EXPECT_CALL(mock_time, clock_gettime(_, _, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, clockid_t clk_id, struct timespec *ts)
            {
                EXPECT_EQ(CLOCK_REALTIME, clk_id);
                // [確認_正常系] - `clock_gettime` に渡すクロック ID が `CLOCK_REALTIME` であること。
                ts->tv_sec = 100;
                ts->tv_nsec = 800000000L;
                return 0;
            });
    // [Pre-Assert確認_異常系] - mock_time の clock_gettime(_, _, _, _, _) が登録した呼び出し期待を満たすこと。
#elif defined(PLATFORM_WINDOWS)
    Mock_windows mock_windows;

    EXPECT_CALL(mock_windows, GetSystemTimeAsFileTime(_, _, _, _))
        .WillOnce([](const char *, const int, const char *, LPFILETIME file_time)
                  { *file_time = to_filetime(100, 800000000); });
    // [Pre-Assert確認_異常系] - mock_windows の GetSystemTimeAsFileTime(_, _, _, _) が登録した呼び出し期待を満たすこと。
#endif

    // Act
    cplat_clock_get_realtime_deadline_ms(
        timeout_ms, &abs_timeout); // [手順] - cplat_clock_get_realtime_deadline_ms(timeout_ms, &abs_timeout) を呼び出す。

    // Assert
    EXPECT_EQ(expected_sec, abs_timeout.tv_sec); // [確認_正常系] - ナノ秒 overflow により秒部が 101 に繰り上がること。
    EXPECT_EQ(expected_nsec, abs_timeout.tv_nsec); // [確認_正常系] - ナノ秒部が 50000000 に正規化されること。
}
