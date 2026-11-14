#include <testfw.h>
#include <mock_cplat.h>
#include <cplat/clock/clock.h>
#if defined(PLATFORM_LINUX)
    #include <mock_time.h>
    #include <errno.h>
#elif defined(PLATFORM_WINDOWS)
    #include <mock_windows.h>
#endif

// 高分解能の単調増加時刻が後退しないことの確認
// cplat-req: id=CPLAT-CLOCK-FUNC-007; uuid=32a417f6-af82-4a99-967b-a76105d8b945
TEST(clockHighResolutionTest, consecutive_values_do_not_decrease)
{
    // Arrange
    uint64_t before = 0;
    uint64_t after = 0;

    // Pre-Assert

    // Act
    int actual_ret_before = cplat_clock_get_monotonic_high_resolution_ns(&before);
    // [手順] - 高分解能の単調増加時刻を取得する。
    int actual_ret_after = cplat_clock_get_monotonic_high_resolution_ns(&after);
    // [手順] - 高分解能の単調増加時刻を再度取得する。

    // Assert
    ASSERT_EQ(CPLAT_OK, actual_ret_before);
    // [確認_正常系] - 1 回目の cplat_clock_get_monotonic_high_resolution_ns() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, actual_ret_after);
    // [確認_正常系] - 2 回目の cplat_clock_get_monotonic_high_resolution_ns() の戻り値が CPLAT_OK であること。
    EXPECT_GE(after, before);
    // [確認_正常系] - after が before 以上であること。
}

// NULL の出力先を拒否することの確認
TEST(clockHighResolutionTest, null_output_is_rejected)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret = cplat_clock_get_monotonic_high_resolution_ns(nullptr);
    // [手順] - 出力先に nullptr を指定する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret);
    // [確認_異常系] - cplat_clock_get_monotonic_high_resolution_ns() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

#if defined(PLATFORM_LINUX)
// OS の時刻取得に失敗した場合に出力を保持することの確認
TEST(clockHighResolutionTest, failed_clock_gettime_preserves_output)
{
    // Arrange
    Mock_time mock_time;
    uint64_t nanoseconds = 99;

    // Pre-Assert
    EXPECT_CALL(mock_time, clock_gettime(_, _, _, CLOCK_MONOTONIC, _))
        .WillOnce(DoAll(Assign(&errno, EIO), Return(-1)));
    // [Pre-Assert確認_異常系] - clock_gettime() が CLOCK_MONOTONIC を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - clock_gettime() から、errno に EIO を設定して -1 を返す。

    // Act
    int actual_ret = cplat_clock_get_monotonic_high_resolution_ns(&nanoseconds);
    // [手順] - 高分解能の単調増加時刻を取得する。

    // Assert
    EXPECT_NE(CPLAT_OK, actual_ret);
    // [確認_異常系] - cplat_clock_get_monotonic_high_resolution_ns() の戻り値が CPLAT_OK 以外であること。
    EXPECT_EQ(99ULL, nanoseconds);
    // [確認_異常系] - nanoseconds が 99 であること。
}

// 秒部とナノ秒部をナノ秒単位の値に換算することの確認
TEST(clockHighResolutionTest, linux_value_is_converted_to_nanoseconds)
{
    // Arrange
    Mock_time mock_time;
    uint64_t nanoseconds = 0;

    // Pre-Assert
    EXPECT_CALL(mock_time, clock_gettime(_, _, _, CLOCK_MONOTONIC, _))
        .WillOnce(
            [](const char *, const int, const char *, clockid_t, struct timespec *timestamp)
            {
                timestamp->tv_sec = 12;
                timestamp->tv_nsec = 345678901L;
                return 0;
            });
    // [Pre-Assert確認_正常系] - clock_gettime() が CLOCK_MONOTONIC を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - clock_gettime() から、12 秒 345678901 ナノ秒を設定して 0 を返す。

    // Act
    int actual_ret = cplat_clock_get_monotonic_high_resolution_ns(&nanoseconds);
    // [手順] - 高分解能の単調増加時刻を取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret);
    // [確認_正常系] - cplat_clock_get_monotonic_high_resolution_ns() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(12345678901ULL, nanoseconds);
    // [確認_正常系] - nanoseconds が 12345678901 であること。
}
#elif defined(PLATFORM_WINDOWS)
// 周波数の取得失敗の場合に出力を保持することの確認
TEST(clockHighResolutionTest, failed_frequency_preserves_output)
{
    // Arrange
    Mock_windows mock_windows;
    uint64_t nanoseconds = 99;

    // Pre-Assert
    EXPECT_CALL(mock_windows, QueryPerformanceFrequency(_, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, LARGE_INTEGER *frequency)
            {
                frequency->QuadPart = 10000000;
                return FALSE;
            });
    // [Pre-Assert確認_異常系] - QueryPerformanceFrequency() が 1 回呼び出されること。
    // [Pre-Assert手順] - QueryPerformanceFrequency() から、周波数を 10000000 に設定して FALSE を返す。
    EXPECT_CALL(mock_windows, QueryPerformanceCounter(_, _, _, _)).Times(0);
    // [Pre-Assert確認_異常系] - QueryPerformanceCounter() が呼び出されないこと。

    // Act
    int actual_ret = cplat_clock_get_monotonic_high_resolution_ns(&nanoseconds);
    // [手順] - 高分解能の単調増加時刻を取得する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret);
    // [確認_異常系] - cplat_clock_get_monotonic_high_resolution_ns() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(99ULL, nanoseconds);
    // [確認_異常系] - nanoseconds が 99 であること。
}

// 周波数が 0 の場合に出力を保持することの確認
TEST(clockHighResolutionTest, zero_frequency_preserves_output)
{
    // Arrange
    Mock_windows mock_windows;
    uint64_t nanoseconds = 99;

    // Pre-Assert
    EXPECT_CALL(mock_windows, QueryPerformanceFrequency(_, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, LARGE_INTEGER *frequency)
            {
                frequency->QuadPart = 0;
                return TRUE;
            });
    // [Pre-Assert確認_異常系] - QueryPerformanceFrequency() が 1 回呼び出されること。
    // [Pre-Assert手順] - QueryPerformanceFrequency() から、周波数を 0 に設定して TRUE を返す。
    EXPECT_CALL(mock_windows, QueryPerformanceCounter(_, _, _, _)).Times(0);
    // [Pre-Assert確認_異常系] - QueryPerformanceCounter() が呼び出されないこと。

    // Act
    int actual_ret = cplat_clock_get_monotonic_high_resolution_ns(&nanoseconds);
    // [手順] - 高分解能の単調増加時刻を取得する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret);
    // [確認_異常系] - cplat_clock_get_monotonic_high_resolution_ns() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(99ULL, nanoseconds);
    // [確認_異常系] - nanoseconds が 99 であること。
}

// 周波数が負の場合に出力を保持することの確認
TEST(clockHighResolutionTest, negative_frequency_preserves_output)
{
    // Arrange
    Mock_windows mock_windows;
    uint64_t nanoseconds = 99;

    // Pre-Assert
    EXPECT_CALL(mock_windows, QueryPerformanceFrequency(_, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, LARGE_INTEGER *frequency)
            {
                frequency->QuadPart = -1;
                return TRUE;
            });
    // [Pre-Assert確認_異常系] - QueryPerformanceFrequency() が 1 回呼び出されること。
    // [Pre-Assert手順] - QueryPerformanceFrequency() から、周波数を -1 に設定して TRUE を返す。
    EXPECT_CALL(mock_windows, QueryPerformanceCounter(_, _, _, _)).Times(0);
    // [Pre-Assert確認_異常系] - QueryPerformanceCounter() が呼び出されないこと。

    // Act
    int actual_ret = cplat_clock_get_monotonic_high_resolution_ns(&nanoseconds);
    // [手順] - 高分解能の単調増加時刻を取得する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret);
    // [確認_異常系] - cplat_clock_get_monotonic_high_resolution_ns() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(99ULL, nanoseconds);
    // [確認_異常系] - nanoseconds が 99 であること。
}

// カウンターの取得失敗の場合に出力を保持することの確認
TEST(clockHighResolutionTest, failed_counter_preserves_output)
{
    // Arrange
    Mock_windows mock_windows;
    uint64_t nanoseconds = 99;

    // Pre-Assert
    EXPECT_CALL(mock_windows, QueryPerformanceFrequency(_, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, LARGE_INTEGER *frequency)
            {
                frequency->QuadPart = 10000000;
                return TRUE;
            });
    // [Pre-Assert確認_異常系] - QueryPerformanceFrequency() が 1 回呼び出されること。
    // [Pre-Assert手順] - QueryPerformanceFrequency() から、周波数を 10000000 に設定して TRUE を返す。
    EXPECT_CALL(mock_windows, QueryPerformanceCounter(_, _, _, _)).WillOnce(Return(FALSE));
    // [Pre-Assert確認_異常系] - QueryPerformanceCounter() が 1 回呼び出されること。
    // [Pre-Assert手順] - QueryPerformanceCounter() から FALSE を返す。

    // Act
    int actual_ret = cplat_clock_get_monotonic_high_resolution_ns(&nanoseconds);
    // [手順] - 高分解能の単調増加時刻を取得する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret);
    // [確認_異常系] - cplat_clock_get_monotonic_high_resolution_ns() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(99ULL, nanoseconds);
    // [確認_異常系] - nanoseconds が 99 であること。
}

// カウンターをナノ秒に換算することの確認
TEST(clockHighResolutionTest, counter_is_converted_to_nanoseconds)
{
    // Arrange
    Mock_windows mock_windows;
    uint64_t nanoseconds = 0;

    // Pre-Assert
    EXPECT_CALL(mock_windows, QueryPerformanceFrequency(_, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, LARGE_INTEGER *frequency)
            {
                frequency->QuadPart = 10000000;
                return TRUE;
            });
    // [Pre-Assert確認_正常系] - QueryPerformanceFrequency() が 1 回呼び出されること。
    // [Pre-Assert手順] - QueryPerformanceFrequency() から、周波数を 10000000 に設定して TRUE を返す。
    EXPECT_CALL(mock_windows, QueryPerformanceCounter(_, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, LARGE_INTEGER *counter)
            {
                counter->QuadPart = 12345678901LL;
                return TRUE;
            });
    // [Pre-Assert確認_正常系] - QueryPerformanceCounter() が 1 回呼び出されること。
    // [Pre-Assert手順] - QueryPerformanceCounter() から、カウンターを 12345678901 に設定して TRUE を返す。

    // Act
    int actual_ret = cplat_clock_get_monotonic_high_resolution_ns(&nanoseconds);
    // [手順] - 高分解能の単調増加時刻を取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret);
    // [確認_正常系] - cplat_clock_get_monotonic_high_resolution_ns() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1234567890100ULL, nanoseconds);
    // [確認_正常系] - nanoseconds が 1234567890100 であること。
}

// カウンター全体に 10 億を掛けると桁あふれする値も正しく換算することの確認
TEST(clockHighResolutionTest, large_counter_avoids_multiplication_overflow)
{
    // Arrange
    Mock_windows mock_windows;
    uint64_t nanoseconds = 0;

    // Pre-Assert
    EXPECT_CALL(mock_windows, QueryPerformanceFrequency(_, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, LARGE_INTEGER *frequency)
            {
                frequency->QuadPart = 10000000;
                return TRUE;
            });
    // [Pre-Assert確認_正常系] - QueryPerformanceFrequency() が 1 回呼び出されること。
    // [Pre-Assert手順] - QueryPerformanceFrequency() から、周波数を 10000000 に設定して TRUE を返す。
    EXPECT_CALL(mock_windows, QueryPerformanceCounter(_, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, LARGE_INTEGER *counter)
            {
                counter->QuadPart = 123456789012345LL;
                return TRUE;
            });
    // [Pre-Assert確認_正常系] - QueryPerformanceCounter() が 1 回呼び出されること。
    // [Pre-Assert手順] - QueryPerformanceCounter() から、カウンターを 123456789012345 に設定して TRUE を返す。

    // Act
    int actual_ret = cplat_clock_get_monotonic_high_resolution_ns(&nanoseconds);
    // [手順] - 高分解能の単調増加時刻を取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret);
    // [確認_正常系] - cplat_clock_get_monotonic_high_resolution_ns() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(12345678901234500ULL, nanoseconds);
    // [確認_正常系] - nanoseconds が 12345678901234500 であること。
}

// 秒未満の部分を換算することの確認
TEST(clockHighResolutionTest, fractional_nanoseconds_are_truncated)
{
    // Arrange
    Mock_windows mock_windows;
    uint64_t nanoseconds = 0;

    // Pre-Assert
    EXPECT_CALL(mock_windows, QueryPerformanceFrequency(_, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, LARGE_INTEGER *frequency)
            {
                frequency->QuadPart = 3;
                return TRUE;
            });
    // [Pre-Assert確認_正常系] - QueryPerformanceFrequency() が 1 回呼び出されること。
    // [Pre-Assert手順] - QueryPerformanceFrequency() から、周波数を 3 に設定して TRUE を返す。
    EXPECT_CALL(mock_windows, QueryPerformanceCounter(_, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, LARGE_INTEGER *counter)
            {
                counter->QuadPart = 1LL;
                return TRUE;
            });
    // [Pre-Assert確認_正常系] - QueryPerformanceCounter() が 1 回呼び出されること。
    // [Pre-Assert手順] - QueryPerformanceCounter() から、カウンターを 1 に設定して TRUE を返す。

    // Act
    int actual_ret = cplat_clock_get_monotonic_high_resolution_ns(&nanoseconds);
    // [手順] - 高分解能の単調増加時刻を取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret);
    // [確認_正常系] - cplat_clock_get_monotonic_high_resolution_ns() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(333333333ULL, nanoseconds);
    // [確認_正常系] - nanoseconds が 333333333 であること。
}

#endif
