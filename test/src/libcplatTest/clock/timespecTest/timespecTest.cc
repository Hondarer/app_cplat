#include <testfw.h>
#include <cplat/clock/timespec.h>
#include <stddef.h>
#include <stdint.h>

class timespecTest : public Test
{
};

// cplat_timespec が Linux x86-64 の struct timespec と同一レイアウトであることの確認
TEST_F(timespecTest, layout_is_16_bytes_with_expected_offsets)
{
    // Arrange

    // Pre-Assert

    // Act

    // Assert
    EXPECT_EQ(16U, sizeof(cplat_timespec)); // [確認_正常系] - sizeof(cplat_timespec) が 16U であること。
    EXPECT_EQ(0U,
              offsetof(cplat_timespec, tv_sec)); // [確認_正常系] - offsetof(cplat_timespec, tv_sec) が 0U であること。
    EXPECT_EQ(
        8U, offsetof(cplat_timespec, tv_nsec)); // [確認_正常系] - offsetof(cplat_timespec, tv_nsec) が 8U であること。
}

// ナノ秒部が上限を超えた時刻値が秒繰り上げで正規化されることの確認
TEST_F(timespecTest, normalize_carries_nsec_overflow_into_sec)
{
    // Arrange
    cplat_timespec ts = {1, 2500000000LL}; // [状態] - tv_nsec が 2.5 秒相当の未正規化値を用意する。

    // Pre-Assert

    // Act
    cplat_timespec_normalize(&ts); // [手順] - cplat_timespec_normalize() を呼び出す。

    // Assert
    EXPECT_EQ(3, ts.tv_sec); // [確認_正常系] - ts.tv_sec が 3 であること。(ナノ秒部から 2 秒繰り上がったことの確認)
    EXPECT_EQ(500000000LL, ts.tv_nsec); // [確認_正常系] - ts.tv_nsec が 500000000LL であること。
}

// 負のナノ秒部が秒繰り下げで正規化されることの確認
TEST_F(timespecTest, normalize_borrows_sec_for_negative_nsec)
{
    // Arrange
    cplat_timespec ts = {1, -1}; // [状態] - tv_nsec が負 (-1) の未正規化値を用意する。

    // Pre-Assert

    // Act
    cplat_timespec_normalize(&ts); // [手順] - cplat_timespec_normalize() を呼び出す。

    // Assert
    EXPECT_EQ(0,
              ts.tv_sec); // [確認_正常系] - ts.tv_sec が 0 であること。(負のナノ秒部により 1 秒繰り下がったことの確認)
    EXPECT_EQ(999999999LL, ts.tv_nsec); // [確認_正常系] - ts.tv_nsec が 999999999LL であること。
}

// 正規化済みの時刻値が正規化で変化しないことの確認
TEST_F(timespecTest, normalize_keeps_already_normalized_value)
{
    // Arrange
    cplat_timespec ts = {5, 999999999LL}; // [状態] - 正規化済みの境界値 {5, 999999999} を用意する。

    // Pre-Assert

    // Act
    cplat_timespec_normalize(&ts); // [手順] - cplat_timespec_normalize() を呼び出す。

    // Assert
    EXPECT_EQ(5, ts.tv_sec);            // [確認_正常系] - ts.tv_sec が 5 であること。
    EXPECT_EQ(999999999LL, ts.tv_nsec); // [確認_正常系] - ts.tv_nsec が 999999999LL であること。
}

// NULL を渡した正規化が無処理で戻ることの確認
TEST_F(timespecTest, normalize_ignores_null)
{
    // Arrange

    // Pre-Assert

    // Act
    cplat_timespec_normalize(nullptr); // [手順] - cplat_timespec_normalize() を呼び出す。

    // Assert
    SUCCEED(); // [確認_異常系] - SUCCEED() に到達すること。(nullptr を渡した cplat_timespec_normalize() がクラッシュしないことの確認)
}

// 加算でナノ秒部の繰り上げが行われることの確認
TEST_F(timespecTest, add_carries_nsec_into_sec)
{
    // Arrange
    const cplat_timespec a = {1, 600000000LL}; // [状態] - 加算元を {1, 600000000} とする。
    const cplat_timespec b = {2, 700000000LL}; // [状態] - 加算値を {2, 700000000} とする。
    cplat_timespec result = {-1, -1};          // [状態] - 結果の格納先を未更新値で初期化する。

    // Pre-Assert

    // Act
    cplat_timespec_add(&a, &b, &result); // [手順] - 加算元 a と加算値 b を指定して cplat_timespec_add() を呼び出す。

    // Assert
    EXPECT_EQ(
        4, result.tv_sec); // [確認_正常系] - result.tv_sec が 4 であること。(ナノ秒部から 1 秒繰り上がったことの確認)
    EXPECT_EQ(300000000LL, result.tv_nsec); // [確認_正常系] - result.tv_nsec が 300000000LL であること。
}

// 加算の結果格納先に入力と同一の変数を指定できることの確認
TEST_F(timespecTest, add_accepts_result_aliasing_input)
{
    // Arrange
    cplat_timespec a = {1, 100000000LL};       // [状態] - 加算元 (兼結果格納先) を {1, 100000000} とする。
    const cplat_timespec b = {2, 200000000LL}; // [状態] - 加算値を {2, 200000000} とする。

    // Pre-Assert

    // Act
    cplat_timespec_add(&a, &b,
                       &a); // [手順] - 結果格納先に加算元と同一の &a を指定して cplat_timespec_add() を呼び出す。

    // Assert
    EXPECT_EQ(3, a.tv_sec);            // [確認_正常系] - a.tv_sec が 3 であること。
    EXPECT_EQ(300000000LL, a.tv_nsec); // [確認_正常系] - a.tv_nsec が 300000000LL であること。
}

// 減算でナノ秒部の繰り下げが行われることの確認
TEST_F(timespecTest, sub_borrows_sec_for_nsec)
{
    // Arrange
    const cplat_timespec a = {3, 100000000LL}; // [状態] - 被減算値を {3, 100000000} とする。
    const cplat_timespec b = {1, 600000000LL}; // [状態] - 減算値を {1, 600000000} とする。
    cplat_timespec result = {-1, -1};          // [状態] - 結果の格納先を未更新値で初期化する。

    // Pre-Assert

    // Act
    cplat_timespec_sub(&a, &b, &result); // [手順] - 被減算値 a と減算値 b を指定して cplat_timespec_sub() を呼び出す。

    // Assert
    EXPECT_EQ(
        1,
        result
            .tv_sec); // [確認_正常系] - result.tv_sec が 1 であること。(ナノ秒部の不足により 1 秒繰り下がったことの確認)
    EXPECT_EQ(500000000LL, result.tv_nsec); // [確認_正常系] - result.tv_nsec が 500000000LL であること。
}

// 過去方向の減算結果が負の秒部と正規化済みナノ秒部になることの確認
TEST_F(timespecTest, sub_returns_negative_sec_when_a_is_older)
{
    // Arrange
    const cplat_timespec a = {1, 100000000LL}; // [状態] - 被減算値を {1, 100000000} とする。
    const cplat_timespec b = {2, 600000000LL}; // [状態] - 減算値を未来の {2, 600000000} とする。
    cplat_timespec result = {0, 0};            // [状態] - 結果の格納先を 0 初期化する。

    // Pre-Assert

    // Act
    cplat_timespec_sub(&a, &b, &result); // [手順] - 被減算値 a と減算値 b を指定して cplat_timespec_sub() を呼び出す。

    // Assert
    EXPECT_EQ(
        -2,
        result
            .tv_sec); // [確認_正常系] - result.tv_sec が -2 であること。(-1.5 秒の差分を {-2, 500000000} に正規化したことの確認)
    EXPECT_EQ(500000000LL, result.tv_nsec); // [確認_正常系] - result.tv_nsec が 500000000LL であること。
}

// 比較が秒部優先で行われることの確認
TEST_F(timespecTest, cmp_compares_sec_first_then_nsec)
{
    // Arrange
    const cplat_timespec base = {2, 500000000LL};       // [状態] - 基準値を {2, 500000000} とする。
    const cplat_timespec same = {2, 500000000LL};       // [状態] - 基準値と等しい値を用意する。
    const cplat_timespec older_sec = {1, 900000000LL};  // [状態] - 秒部が過去でナノ秒部が大きい値を用意する。
    const cplat_timespec newer_nsec = {2, 500000001LL}; // [状態] - 秒部が同じでナノ秒部が未来の値を用意する。

    // Pre-Assert

    // Act
    int actual_ret_same = cplat_timespec_cmp(&base, &same);
    int actual_ret_older_sec = cplat_timespec_cmp(&base, &older_sec);
    int actual_ret_reverse = cplat_timespec_cmp(&older_sec, &base);
    int actual_ret_newer_nsec = cplat_timespec_cmp(&base, &newer_nsec);
    int actual_ret_older_nsec = cplat_timespec_cmp(&newer_nsec, &base);

    // Assert
    EXPECT_EQ(0, actual_ret_same); // [確認_正常系] - 等しい値に対する cplat_timespec_cmp() の戻り値が 0 であること。
    EXPECT_EQ(
        1,
        actual_ret_older_sec); // [確認_正常系] - 秒部が過去の右辺に対する cplat_timespec_cmp() の戻り値が 1 であること。
    EXPECT_EQ(
        -1,
        actual_ret_reverse); // [確認_正常系] - 秒部が未来の右辺に対する cplat_timespec_cmp() の戻り値が -1 であること。
    EXPECT_EQ(
        -1,
        actual_ret_newer_nsec); // [確認_正常系] - 同秒でナノ秒部が未来の右辺に対する cplat_timespec_cmp() の戻り値が -1 であること。
    EXPECT_EQ(
        1,
        actual_ret_older_nsec); // [確認_正常系] - 同秒でナノ秒部が過去の右辺に対する cplat_timespec_cmp() の戻り値が 1 であること。
}

// ミリ秒加算で deadline が生成されることの確認
TEST_F(timespecTest, add_ms_creates_deadline_with_carry)
{
    // Arrange
    const cplat_timespec ts = {100, 800000000LL}; // [状態] - 加算元を {100, 800000000} とする。
    cplat_timespec result = {-1, -1};             // [状態] - 結果の格納先を未更新値で初期化する。

    // Pre-Assert

    // Act
    cplat_timespec_add_ms(&ts, 1250,
                          &result); // [手順] - 加算元 ts と 1250 ミリ秒を指定して cplat_timespec_add_ms() を呼び出す。

    // Assert
    EXPECT_EQ(
        102,
        result
            .tv_sec); // [確認_正常系] - result.tv_sec が 102 であること。(1250 ミリ秒の加算で 2 秒繰り上がったことの確認)
    EXPECT_EQ(50000000LL, result.tv_nsec); // [確認_正常系] - result.tv_nsec が 50000000LL であること。
}

// ミリ秒差分が切り捨てで計算されることの確認
TEST_F(timespecTest, diff_ms_truncates_sub_millisecond_part)
{
    // Arrange
    const cplat_timespec start = {10, 100000000LL}; // [状態] - 起点を {10, 100000000} とする。
    const cplat_timespec end = {12, 350999999LL};   // [状態] - 終点を 2250.999999 ミリ秒後とする。

    // Pre-Assert

    // Act
    int64_t elapsed_ms = cplat_timespec_diff_ms(
        &end, &start); // [手順] - 終点 end と起点 start を指定して cplat_timespec_diff_ms() を呼び出す。

    // Assert
    EXPECT_EQ(
        2250,
        elapsed_ms); // [確認_正常系] - cplat_timespec_diff_ms() の戻り値が 2250 であること。(ミリ秒未満が切り捨てられたことの確認)
}

// 過去方向のミリ秒差分が負値になることの確認
TEST_F(timespecTest, diff_ms_returns_negative_when_end_is_older)
{
    // Arrange
    const cplat_timespec start = {10, 500000000LL}; // [状態] - 起点を {10, 500000000} とする。
    const cplat_timespec end = {10, 499000000LL};   // [状態] - 終点を起点の 1 ミリ秒前とする。

    // Pre-Assert

    // Act
    int64_t elapsed_ms = cplat_timespec_diff_ms(
        &end, &start); // [手順] - 終点 end と起点 start を指定して cplat_timespec_diff_ms() を呼び出す。

    // Assert
    EXPECT_EQ(-1, elapsed_ms); // [確認_正常系] - cplat_timespec_diff_ms() の戻り値が -1 であること。
}

// NULL を渡した演算・変換が無処理または 0 で戻ることの確認
TEST_F(timespecTest, operations_ignore_null_arguments)
{
    // Arrange
    const cplat_timespec value = {1, 2}; // [状態] - nullptr 以外の側の引数として {1, 2} を用意する。
    cplat_timespec result = {3, 4};      // [状態] - 結果格納先を {3, 4} で初期化する。
    struct timespec native = {5, 6};     // [状態] - ネイティブ変換の格納先を {5, 6} で初期化する。

    // Pre-Assert

    // Act
    cplat_timespec_add(nullptr, &value,
                       &result); // [手順] - 加算元に nullptr を指定して cplat_timespec_add() を呼び出す。
    cplat_timespec_add(&value, nullptr, &result);
    cplat_timespec_add(&value, &value, nullptr);
    cplat_timespec_sub(nullptr, &value,
                       &result); // [手順] - 被減算値に nullptr を指定して cplat_timespec_sub() を呼び出す。
    cplat_timespec_sub(&value, nullptr, &result);
    cplat_timespec_sub(&value, &value, nullptr);
    cplat_timespec_add_ms(nullptr, 100,
                          &result); // [手順] - 加算元に nullptr を指定して cplat_timespec_add_ms() を呼び出す。
    cplat_timespec_add_ms(&value, 100, nullptr);
    cplat_timespec_to_native(nullptr,
                             &native); // [手順] - 変換元に nullptr を指定して cplat_timespec_to_native() を呼び出す。
    cplat_timespec_to_native(&value, nullptr);
    cplat_timespec_from_native(
        nullptr, &result); // [手順] - 変換元に nullptr を指定して cplat_timespec_from_native() を呼び出す。
    cplat_timespec_from_native(&native, nullptr);
    int cmp_null_left =
        cplat_timespec_cmp(nullptr, &value); // [手順] - 左辺に nullptr を指定して cplat_timespec_cmp() を呼び出す。
    int cmp_null_right =
        cplat_timespec_cmp(&value, nullptr); // [手順] - 右辺に nullptr を指定して cplat_timespec_cmp() を呼び出す。
    int64_t diff_null_end = cplat_timespec_diff_ms(
        nullptr, &value); // [手順] - 終点に nullptr を指定して cplat_timespec_diff_ms() を呼び出す。
    int64_t diff_null_start = cplat_timespec_diff_ms(
        &value, nullptr); // [手順] - 起点に nullptr を指定して cplat_timespec_diff_ms() を呼び出す。

    // Assert
    EXPECT_EQ(3, result.tv_sec);  // [確認_正常系] - result.tv_sec が 3 であること。
    EXPECT_EQ(4, result.tv_nsec); // [確認_正常系] - result.tv_nsec が 4 であること。
    EXPECT_EQ(5, native.tv_sec);  // [確認_正常系] - native.tv_sec が 5 であること。
    EXPECT_EQ(6, native.tv_nsec); // [確認_正常系] - native.tv_nsec が 6 であること。
    EXPECT_EQ(0, cmp_null_left); // [確認_正常系] - 左辺 nullptr に対する cplat_timespec_cmp() の戻り値が 0 であること。
    EXPECT_EQ(0,
              cmp_null_right); // [確認_正常系] - 右辺 nullptr に対する cplat_timespec_cmp() の戻り値が 0 であること。
    EXPECT_EQ(
        0,
        diff_null_end); // [確認_正常系] - 終点 nullptr に対する cplat_timespec_diff_ms() の戻り値が 0 であること。
    EXPECT_EQ(
        0,
        diff_null_start); // [確認_正常系] - 起点 nullptr に対する cplat_timespec_diff_ms() の戻り値が 0 であること。
}

// ネイティブ struct timespec との相互変換で値が保たれることの確認
TEST_F(timespecTest, native_conversion_round_trips_value)
{
    // Arrange
    const cplat_timespec ts = {1712345678, 987654321LL}; // [状態] - 変換元を {1712345678, 987654321} とする。
    struct timespec native = {};                         // [状態] - ネイティブ変換結果を 0 初期化する。
    cplat_timespec restored = {-1, -1};                  // [状態] - 逆変換結果の格納先を未更新値で初期化する。

    // Pre-Assert

    // Act
    cplat_timespec_to_native(&ts, &native); // [手順] - 変換元 ts を指定して cplat_timespec_to_native() を呼び出す。
    cplat_timespec_from_native(
        &native,
        &restored); // [手順] - 変換元 native を指定して cplat_timespec_from_native() を呼び出す。

    // Assert
    EXPECT_EQ((time_t)1712345678, native.tv_sec); // [確認_正常系] - native.tv_sec が 1712345678 であること。
    EXPECT_EQ(987654321L, native.tv_nsec);        // [確認_正常系] - native.tv_nsec が 987654321L であること。
    EXPECT_EQ(ts.tv_sec, restored.tv_sec);        // [確認_正常系] - restored.tv_sec が ts.tv_sec と一致すること。
    EXPECT_EQ(ts.tv_nsec, restored.tv_nsec);      // [確認_正常系] - restored.tv_nsec が ts.tv_nsec と一致すること。
}
