#include <cplat/base/platform.h>
#if defined(PLATFORM_WINDOWS)
    #define _HAS_STD_BYTE 0
#endif
#include <testfw.h>
#include <mock_cplat.h>
#include "bench_timer.inject.h"

// 時刻差分の失敗とナノ秒換算の上限超過を通知することの確認
TEST(benchTimerTest, rejects_unrepresentable_elapsed_time)
{
    // Arrange
    const cplat_timespec start = {};
    const cplat_timespec end = {INT64_MAX, 0};
    uint64_t output = 42; // [状態] - ナノ秒へ換算できない時刻差を用意する。

    // Pre-Assert

    // Act
    int actual = test_bench_elapsed_ns(&start, &end, &output); // [手順] - 差分のナノ秒換算を行う。

    // Assert
    EXPECT_EQ(-1, actual);  // [確認_異常系] - 換算不能を失敗として返すこと。
    EXPECT_EQ(42U, output); // [確認_異常系] - 出力を保持すること。
}

// 減算の失敗を計測値として扱わないことの確認
TEST(benchTimerTest, propagates_subtraction_failure)
{
    // Arrange
    NiceMock<Mock_cplat> mock;
    const cplat_timespec start = {};
    const cplat_timespec end = {};
    uint64_t output = 42;

    // Pre-Assert
    EXPECT_CALL(mock, cplat_timespec_sub(_, _, _)).WillOnce(Return(CPLAT_ERR_OUT_OF_RANGE));
    // [Pre-Assert確認_異常系] - 時刻の減算を 1 回試すこと。
    // [Pre-Assert手順] - 減算の表現範囲外を返す。

    // Act
    int actual = test_bench_elapsed_ns(&start, &end, &output); // [手順] - 減算失敗を処理する。

    // Assert
    EXPECT_EQ(-1, actual); // [確認_異常系] - 失敗を返すこと。
    EXPECT_EQ(42U, output);
}
