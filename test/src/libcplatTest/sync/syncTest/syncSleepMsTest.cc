#include <testfw.h>
#include "syncTestHelper.h"
#if defined(PLATFORM_LINUX)
    #include <mock_time.h>
#endif

// 待機時間 0 ms の指定で即時に戻ることの確認
TEST(syncSleepMsTest, zero_returns_immediately)
{
    // Arrange
    uint64_t before = test_monotonic_ms(); // [状態] - 呼び出し前の単調増加時刻を取得する。

    // Pre-Assert

    // Act
    cplat_sleep_ms(0);                 // [手順] - 待機時間 0 ms で呼び出す。
    uint64_t after = test_monotonic_ms(); // [手順] - 呼び出し後の単調増加時刻を取得する。

    // Assert
    EXPECT_LT(after - before, 100U); // [確認_正常系] - 0 ms 指定で即時に戻ること。
}

// 負の待機時間の指定で即時に戻ることの確認
TEST(syncSleepMsTest, negative_returns_immediately)
{
    // Arrange
    uint64_t before = test_monotonic_ms(); // [状態] - 呼び出し前の単調増加時刻を取得する。

    // Pre-Assert

    // Act
    cplat_sleep_ms(-1);                // [手順] - 負の待機時間で呼び出す。
    uint64_t after = test_monotonic_ms(); // [手順] - 呼び出し後の単調増加時刻を取得する。

    // Assert
    EXPECT_LT(after - before, 100U); // [確認_異常系] - 負値指定で即時に戻ること (no-op)。
}

// 指定した待機時間以上が経過することの確認
TEST(syncSleepMsTest, elapses_at_least_specified_duration)
{
    // Arrange
#if defined(PLATFORM_LINUX)
    testing::NiceMock<Mock_time> mock_time;
#endif                        /* PLATFORM_LINUX */
    const int target_ms = 50; // [状態] - 待機時間の指示値を 50 ms とする。

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_time, nanosleep(_, _, _, _, _))
        .WillOnce(
            [](const char *, int, const char *, const struct timespec *req, struct timespec *) -> int
            {
                EXPECT_EQ(0, req->tv_sec);
                // [確認_正常系] - `nanosleep` に渡す待機時間の秒部 が `0` であること。
                EXPECT_EQ(50000000L, req->tv_nsec);
                // [確認_正常系] - `nanosleep` に渡す待機時間のナノ秒部 が `50000000L` であること。
                return 0;
            }); // [Pre-Assert確認_正常系] - nanosleep が 50 ms 相当で 1 回呼び出されること。
                // [Pre-Assert手順] - nanosleep から 0 を返却する。
#endif          /* PLATFORM_LINUX */

    // Act
#if defined(PLATFORM_LINUX)
    cplat_sleep_ms(target_ms); // [手順] - 指定ミリ秒待機する。
#else
    uint64_t before = test_monotonic_ms(); // [状態] - 呼び出し前の単調増加時刻を取得する。
    cplat_sleep_ms(target_ms);          // [手順] - 指定ミリ秒待機する。
    uint64_t after = test_monotonic_ms();  // [手順] - 呼び出し後の単調増加時刻を取得する。
#endif /* PLATFORM_LINUX */

    // Assert
#if defined(PLATFORM_LINUX)
    SUCCEED(); // [確認_正常系] - nanosleep が 50 ms 相当で呼び出されること。
#else
    // Windows の GetTickCount64 は ~15 ms 精度のため余裕を持って判定する。
    EXPECT_GE(after - before, (uint64_t)target_ms - 15U); // [確認_正常系] - 指定ミリ秒以上経過していること。
#endif /* PLATFORM_LINUX */
}
