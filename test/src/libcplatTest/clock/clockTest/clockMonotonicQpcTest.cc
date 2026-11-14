#include <testfw.h>
#include <mock_cplat.h>
#include <cplat/clock/clock.h>
#if defined(PLATFORM_LINUX)
    #include <mock_time.h>
    #include <errno.h>
#elif defined(PLATFORM_WINDOWS)
    #include <mock_windows.h>
#endif

class clockMonotonicQpcTest : public Test
{
};

// 単調増加クロックの連続した取得値が前の値より小さくならないことを確認します。
// cplat-req: id=CPLAT-CLOCK-FUNC-007; uuid=32a417f6-af82-4a99-967b-a76105d8b945
TEST_F(clockMonotonicQpcTest, consecutive_values_do_not_decrease)
{
    // Arrange
    cplat_timespec before = {};
    cplat_timespec after = {};

    // Pre-Assert

    // Act
    int before_ret = cplat_clock_get_monotonic(&before);
    int after_ret = cplat_clock_get_monotonic(&after);

    // Assert
    ASSERT_EQ(CPLAT_OK, before_ret); // [確認_正常系] - 1 回目の取得が CPLAT_OK を返すこと。
    ASSERT_EQ(CPLAT_OK, after_ret); // [確認_正常系] - 2 回目の取得が CPLAT_OK を返すこと。
    EXPECT_TRUE(after.tv_sec > before.tv_sec ||
                (after.tv_sec == before.tv_sec && after.tv_nsec >= before.tv_nsec));
    // [確認_正常系] - 2 回目の取得値が 1 回目より小さくならないこと。
}

TEST_F(clockMonotonicQpcTest, null_output_is_rejected)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret = cplat_clock_get_monotonic(nullptr);

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret); // [確認_異常系] - NULL 出力先を拒否すること。
}

#if defined(PLATFORM_LINUX)
TEST_F(clockMonotonicQpcTest, failed_clock_gettime_preserves_output)
{
    // Arrange
    Mock_time mock_time;
    cplat_timespec actual = {99, 98};

    // Pre-Assert
    EXPECT_CALL(mock_time, clock_gettime(_, _, _, CLOCK_MONOTONIC, _))
        .WillOnce(DoAll(Assign(&errno, EIO), Return(-1)));
    // [Pre-Assert確認_異常系] - CLOCK_MONOTONIC の取得を 1 回試みること。
    // [Pre-Assert手順] - clock_gettime() から EIO を設定して -1 を返す。

    // Act
    int actual_ret = cplat_clock_get_monotonic(&actual);

    // Assert
    EXPECT_NE(CPLAT_OK, actual_ret); // [確認_異常系] - OS API の失敗を通知すること。
    EXPECT_EQ(99, actual.tv_sec); // [確認_異常系] - tv_sec を保持すること。
    EXPECT_EQ(98, actual.tv_nsec); // [確認_異常系] - tv_nsec を保持すること。
}
#elif defined(PLATFORM_WINDOWS)
TEST_F(clockMonotonicQpcTest, failed_frequency_preserves_output)
{
    // Arrange
    Mock_windows mock_windows;
    cplat_timespec actual = {99, 98};

    // Pre-Assert
    EXPECT_CALL(mock_windows, QueryPerformanceFrequency(_, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, LARGE_INTEGER *frequency)
            {
                frequency->QuadPart = 10000000;
                return FALSE;
            });
    // [Pre-Assert確認_異常系] - QueryPerformanceFrequency() が 1 回呼び出されること。
    // [Pre-Assert手順] - 周波数を設定して FALSE を返す。
    EXPECT_CALL(mock_windows, QueryPerformanceCounter(_, _, _, _)).Times(0);
    // [Pre-Assert確認_異常系] - 周波数取得失敗時に QueryPerformanceCounter() を呼ばないこと。

    // Act
    int actual_ret = cplat_clock_get_monotonic(&actual);

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret); // [確認_異常系] - 周波数取得失敗を通知すること。
    EXPECT_EQ(99, actual.tv_sec); // [確認_異常系] - tv_sec を保持すること。
    EXPECT_EQ(98, actual.tv_nsec); // [確認_異常系] - tv_nsec を保持すること。
}

TEST_F(clockMonotonicQpcTest, milliseconds_wrapper_propagates_frequency_failure)
{
    // Arrange
    Mock_windows mock_windows;
    uint64_t actual_ms = 99;

    // Pre-Assert
    EXPECT_CALL(mock_windows, QueryPerformanceFrequency(_, _, _, _)).WillOnce(Return(FALSE));
    // [Pre-Assert確認_異常系] - QueryPerformanceFrequency() の失敗を確認すること。
    EXPECT_CALL(mock_windows, QueryPerformanceCounter(_, _, _, _)).Times(0);
    // [Pre-Assert確認_異常系] - 周波数取得失敗時に QueryPerformanceCounter() を呼ばないこと。

    // Act
    int actual_ret = cplat_clock_get_monotonic_ms(&actual_ms);

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret); // [確認_異常系] - 時計取得失敗を伝播すること。
    EXPECT_EQ(99U, actual_ms); // [確認_異常系] - 失敗時にミリ秒出力を保持すること。
}

TEST_F(clockMonotonicQpcTest, nonpositive_frequency_preserves_output)
{
    // Arrange
    Mock_windows mock_windows;
    cplat_timespec actual = {99, 98};

    // Pre-Assert
    EXPECT_CALL(mock_windows, QueryPerformanceFrequency(_, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, LARGE_INTEGER *frequency)
            {
                frequency->QuadPart = 0;
                return TRUE;
            });
    // [Pre-Assert確認_異常系] - QueryPerformanceFrequency() が 1 回呼び出されること。
    // [Pre-Assert手順] - 周波数 0 を設定して TRUE を返す。
    EXPECT_CALL(mock_windows, QueryPerformanceCounter(_, _, _, _)).Times(0);
    // [Pre-Assert確認_異常系] - 周波数が 0 以下の場合に QueryPerformanceCounter() を呼ばないこと。

    // Act
    int actual_ret = cplat_clock_get_monotonic(&actual);

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret); // [確認_異常系] - 不正な周波数を拒否すること。
    EXPECT_EQ(99, actual.tv_sec); // [確認_異常系] - tv_sec を保持すること。
    EXPECT_EQ(98, actual.tv_nsec); // [確認_異常系] - tv_nsec を保持すること。
}

TEST_F(clockMonotonicQpcTest, failed_counter_preserves_output)
{
    // Arrange
    Mock_windows mock_windows;
    cplat_timespec actual = {99, 98};

    // Pre-Assert
    EXPECT_CALL(mock_windows, QueryPerformanceFrequency(_, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, LARGE_INTEGER *frequency)
            {
                frequency->QuadPart = 10000000;
                return TRUE;
            });
    // [Pre-Assert確認_正常系] - 有効な周波数を取得すること。
    EXPECT_CALL(mock_windows, QueryPerformanceCounter(_, _, _, _)).WillOnce(Return(FALSE));
    // [Pre-Assert確認_異常系] - QueryPerformanceCounter() の失敗を確認すること。

    // Act
    int actual_ret = cplat_clock_get_monotonic(&actual);

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret); // [確認_異常系] - カウンター取得失敗を通知すること。
    EXPECT_EQ(99, actual.tv_sec); // [確認_異常系] - tv_sec を保持すること。
    EXPECT_EQ(98, actual.tv_nsec); // [確認_異常系] - tv_nsec を保持すること。
}

TEST_F(clockMonotonicQpcTest, negative_counter_preserves_output)
{
    // Arrange
    Mock_windows mock_windows;
    cplat_timespec actual = {99, 98};

    // Pre-Assert
    EXPECT_CALL(mock_windows, QueryPerformanceFrequency(_, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, LARGE_INTEGER *frequency)
            {
                frequency->QuadPart = 10000000;
                return TRUE;
            });
    // [Pre-Assert確認_正常系] - 有効な周波数を取得すること。
    EXPECT_CALL(mock_windows, QueryPerformanceCounter(_, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, LARGE_INTEGER *counter)
            {
                counter->QuadPart = -1;
                return TRUE;
            });
    // [Pre-Assert確認_異常系] - 負のカウンター値を確認すること。

    // Act
    int actual_ret = cplat_clock_get_monotonic(&actual);

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret); // [確認_異常系] - 負のカウンター値を拒否すること。
    EXPECT_EQ(99, actual.tv_sec); // [確認_異常系] - tv_sec を保持すること。
    EXPECT_EQ(98, actual.tv_nsec); // [確認_異常系] - tv_nsec を保持すること。
}

TEST_F(clockMonotonicQpcTest, counter_is_converted_to_seconds_and_nanoseconds)
{
    // Arrange
    Mock_windows mock_windows;
    cplat_timespec actual = {-1, -1};

    // Pre-Assert
    EXPECT_CALL(mock_windows, QueryPerformanceFrequency(_, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, LARGE_INTEGER *frequency)
            {
                frequency->QuadPart = 10000000;
                return TRUE;
            });
    // [Pre-Assert確認_正常系] - QPF から 10000000 を取得すること。
    EXPECT_CALL(mock_windows, QueryPerformanceCounter(_, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, LARGE_INTEGER *counter)
            {
                counter->QuadPart = 12345678901LL;
                return TRUE;
            });
    // [Pre-Assert確認_正常系] - QPC から 12345678901 を取得すること。

    // Act
    int actual_ret = cplat_clock_get_monotonic(&actual);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 時刻取得が成功すること。
    EXPECT_EQ(1234, actual.tv_sec); // [確認_正常系] - 秒部を換算すること。
    EXPECT_EQ(567890100, actual.tv_nsec); // [確認_正常系] - 秒未満をナノ秒へ換算すること。
}

TEST_F(clockMonotonicQpcTest, large_counter_avoids_multiplication_overflow)
{
    // Arrange
    Mock_windows mock_windows;
    cplat_timespec actual = {-1, -1};

    // Pre-Assert
    EXPECT_CALL(mock_windows, QueryPerformanceFrequency(_, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, LARGE_INTEGER *frequency)
            {
                frequency->QuadPart = 10000000;
                return TRUE;
            });
    EXPECT_CALL(mock_windows, QueryPerformanceCounter(_, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, LARGE_INTEGER *counter)
            {
                counter->QuadPart = 123456789012345LL;
                return TRUE;
            });
    // [Pre-Assert確認_正常系] - QPF と QPC がそれぞれ 1 回呼び出されること。

    // Act
    int actual_ret = cplat_clock_get_monotonic(&actual);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 時刻取得が成功すること。
    EXPECT_EQ(12345678, actual.tv_sec); // [確認_正常系] - 大きなカウンターから秒部を換算すること。
    EXPECT_EQ(901234500, actual.tv_nsec); // [確認_正常系] - 大きなカウンターの秒未満を換算すること。
}

TEST_F(clockMonotonicQpcTest, fractional_nanoseconds_are_truncated)
{
    // Arrange
    Mock_windows mock_windows;
    cplat_timespec actual = {-1, -1};

    // Pre-Assert
    EXPECT_CALL(mock_windows, QueryPerformanceFrequency(_, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, LARGE_INTEGER *frequency)
            {
                frequency->QuadPart = 3;
                return TRUE;
            });
    EXPECT_CALL(mock_windows, QueryPerformanceCounter(_, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, LARGE_INTEGER *counter)
            {
                counter->QuadPart = 1;
                return TRUE;
            });
    // [Pre-Assert確認_正常系] - QPF と QPC がそれぞれ 1 回呼び出されること。

    // Act
    int actual_ret = cplat_clock_get_monotonic(&actual);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 時刻取得が成功すること。
    EXPECT_EQ(0, actual.tv_sec); // [確認_正常系] - 秒部が 0 であること。
    EXPECT_EQ(333333333, actual.tv_nsec); // [確認_正常系] - ナノ秒未満を切り捨てること。
}
#endif
