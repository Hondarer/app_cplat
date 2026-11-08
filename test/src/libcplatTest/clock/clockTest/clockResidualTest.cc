#include <testfw.h>
#include <mock_cplat.h>
#include <cplat/clock/clock.h>
#if defined(PLATFORM_LINUX)
    #include <mock_time.h>
    #include <errno.h>
#endif

// 必須の時刻出力が NULL の場合に失敗し、ほかの出力も保持することの確認
TEST(clockResidualTest, null_outputs_are_rejected_without_partial_updates)
{
    // Arrange
    struct tm calendar = {};
    calendar.tm_year = 42;
    int32_t nanos = 123;

    // Pre-Assert

    // Act
    int milliseconds = cplat_clock_get_monotonic_ms(nullptr);         // [手順] - ミリ秒出力を省略する。
    int monotonic = cplat_clock_get_monotonic(nullptr);               // [手順] - 単調時刻の出力を省略する。
    int realtime = cplat_clock_get_realtime(nullptr);                 // [手順] - 実時刻の出力を省略する。
    int utc_calendar = cplat_clock_get_realtime_utc(nullptr, &nanos); // [手順] - UTC 分解の出力を省略する。
    int utc_nanos = cplat_clock_get_realtime_utc(&calendar, nullptr); // [手順] - ナノ秒出力を省略する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, milliseconds); // [確認_異常系] - ミリ秒出力の NULL を拒否すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, monotonic);    // [確認_異常系] - 単調時刻出力の NULL を拒否すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, realtime);     // [確認_異常系] - 実時刻出力の NULL を拒否すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, utc_calendar); // [確認_異常系] - UTC 分解出力の NULL を拒否すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, utc_nanos);    // [確認_異常系] - ナノ秒出力の NULL を拒否すること。
    EXPECT_EQ(42, calendar.tm_year);                     // [確認_異常系] - 有効な出力先も変更しないこと。
    EXPECT_EQ(123, nanos);                               // [確認_異常系] - 有効なナノ秒出力も変更しないこと。
}

#if defined(PLATFORM_LINUX)
// OS の時刻取得失敗を通知し、すべての出力を変更しないことの確認
// cplat-req: id=CPLAT-CLOCK-FUNC-006; uuid=4b028e9a-42d5-47ef-b568-c89d969b494c
TEST(clockResidualTest, failed_os_clock_preserves_all_outputs)
{
    // Arrange
    NiceMock<Mock_time> mock;
    uint64_t milliseconds = 99;
    cplat_timespec mono = {11, 22};
    cplat_timespec real = {33, 44};
    struct tm calendar = {};
    calendar.tm_year = 42;
    int32_t nanos = 123;
    struct timespec deadline = {55, 66};

    // Pre-Assert
    EXPECT_CALL(mock, clock_gettime(_, _, _, _, _))
        .Times(5)
        .WillRepeatedly(testing::DoAll(testing::Assign(&errno, EIO), Return(-1)));

    // [Pre-Assert確認_異常系] - clock_gettime() が 5 回呼び出されること。
    // [Pre-Assert手順] - 出力せず errno に EIO を設定して -1 を返す。

    // Act
    int ms_ret = cplat_clock_get_monotonic_ms(&milliseconds);      // [手順] - ミリ秒の取得を失敗させる。
    int mono_ret = cplat_clock_get_monotonic(&mono);               // [手順] - 単調時刻の取得を失敗させる。
    int real_ret = cplat_clock_get_realtime(&real);                // [手順] - 実時刻の取得を失敗させる。
    int utc_ret = cplat_clock_get_realtime_utc(&calendar, &nanos); // [手順] - UTC 分解前の時刻取得を失敗させる。
    int deadline_ret =
        cplat_clock_get_realtime_deadline_ms(1, &deadline); // [手順] - 期限生成前の時刻取得を失敗させる。

    // Assert
    EXPECT_NE(CPLAT_OK, ms_ret);       // [確認_異常系] - ミリ秒取得を失敗として返すこと。
    EXPECT_NE(CPLAT_OK, mono_ret);     // [確認_異常系] - 単調時刻取得を失敗として返すこと。
    EXPECT_NE(CPLAT_OK, real_ret);     // [確認_異常系] - 実時刻取得を失敗として返すこと。
    EXPECT_NE(CPLAT_OK, utc_ret);      // [確認_異常系] - UTC の取得を失敗として返すこと。
    EXPECT_NE(CPLAT_OK, deadline_ret); // [確認_異常系] - 期限生成を失敗として返すこと。
    EXPECT_EQ(99U, milliseconds);      // [確認_異常系] - ミリ秒出力が 99 のままであること。
    EXPECT_EQ(11, mono.tv_sec);        // [確認_異常系] - 単調時刻の秒部を保持すること。
    EXPECT_EQ(22, mono.tv_nsec);       // [確認_異常系] - 単調時刻のナノ秒部を保持すること。
    EXPECT_EQ(33, real.tv_sec);        // [確認_異常系] - 実時刻の秒部を保持すること。
    EXPECT_EQ(44, real.tv_nsec);       // [確認_異常系] - 実時刻のナノ秒部を保持すること。
    EXPECT_EQ(42, calendar.tm_year);   // [確認_異常系] - UTC 分解出力を保持すること。
    EXPECT_EQ(123, nanos);             // [確認_異常系] - UTC ナノ秒出力を保持すること。
    EXPECT_EQ(55, deadline.tv_sec);    // [確認_異常系] - 期限の秒部を保持すること。
    EXPECT_EQ(66, deadline.tv_nsec);   // [確認_異常系] - 期限のナノ秒部を保持すること。
}
#endif
