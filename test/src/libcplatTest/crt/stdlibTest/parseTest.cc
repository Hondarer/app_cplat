#include <testfw.h>
#include <cplat/crt/stdlib.h>
#include <climits>
#include <cstdint>

class parseTest : public Test
{
};

// 10 進数の文字列が int64_t へ変換されることの確認
TEST_F(parseTest, parse_int64_decimal)
{
    // Arrange
    int64_t value = 0; // [状態] - 変換結果の格納先を 0 で初期化する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_parse_int64(&value, "1234", 10); // [手順] - "1234" を基数 10 で cplat_parse_int64 に渡す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_parse_int64 の戻り値が CPLAT_OK であること。
    EXPECT_EQ((int64_t)1234, value); // [確認_正常系] - 変換結果が 1234 であること。
}

// 負の値が符号付きで変換されることの確認
TEST_F(parseTest, parse_int64_negative)
{
    // Arrange
    int64_t value = 0; // [状態] - 変換結果の格納先を 0 で初期化する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_parse_int64(&value, "-42", 10); // [手順] - "-42" を基数 10 で cplat_parse_int64 に渡す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_parse_int64 の戻り値が CPLAT_OK であること。
    EXPECT_EQ((int64_t)-42, value);  // [確認_正常系] - 変換結果が -42 であること。
}

// 先頭の空白が読み飛ばされることの確認
TEST_F(parseTest, parse_int64_leading_space)
{
    // Arrange
    int64_t value = 0; // [状態] - 変換結果の格納先を 0 で初期化する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_parse_int64(&value, "   7", 10); // [手順] - 先頭に空白を持つ "   7" を渡す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_parse_int64 の戻り値が CPLAT_OK であること。
    EXPECT_EQ((int64_t)7, value);    // [確認_正常系] - 変換結果が 7 であること。
}

// 基数 16 の指定で 16 進数として変換されることの確認
TEST_F(parseTest, parse_int64_base16)
{
    // Arrange
    int64_t value = 0; // [状態] - 変換結果の格納先を 0 で初期化する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_parse_int64(&value, "ff", 16); // [手順] - "ff" を基数 16 で cplat_parse_int64 に渡す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_parse_int64 の戻り値が CPLAT_OK であること。
    EXPECT_EQ((int64_t)255, value);  // [確認_正常系] - 変換結果が 255 であること。
}

// 基数 0 の指定で接頭辞から基数が自動判別されることの確認
TEST_F(parseTest, parse_int64_base_auto_detect)
{
    // Arrange
    int64_t value = 0; // [状態] - 変換結果の格納先を 0 で初期化する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_parse_int64(&value, "0x10", 0); // [手順] - "0x10" を基数 0 で cplat_parse_int64 に渡す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_parse_int64 の戻り値が CPLAT_OK であること。
    EXPECT_EQ((int64_t)16, value);   // [確認_正常系] - 0x10 が 16 として変換されること。
}

// int64_t の上限と下限が変換できることの確認
TEST_F(parseTest, parse_int64_boundary_values)
{
    // Arrange
    int64_t max_value = 0;
    int64_t min_value = 0; // [状態] - 上限用と下限用の格納先をそれぞれ 0 で初期化する。

    // Pre-Assert

    // Act
    int max_ret =
        cplat_parse_int64(&max_value, "9223372036854775807", 10); // [手順] - INT64_MAX の 10 進表記を変換する。
    int min_ret =
        cplat_parse_int64(&min_value, "-9223372036854775808", 10); // [手順] - INT64_MIN の 10 進表記を変換する。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              max_ret); // [確認_正常系] - INT64_MAX を渡した cplat_parse_int64 の戻り値が CPLAT_OK であること。
    EXPECT_EQ(INT64_MAX, max_value); // [確認_正常系] - 変換結果が INT64_MAX であること。
    EXPECT_EQ(CPLAT_OK,
              min_ret); // [確認_正常系] - INT64_MIN を渡した cplat_parse_int64 の戻り値が CPLAT_OK であること。
    EXPECT_EQ(INT64_MIN, min_value); // [確認_正常系] - 変換結果が INT64_MIN であること。
}

// 空文字列が変換エラーになることの確認
TEST_F(parseTest, parse_int64_empty_text)
{
    // Arrange
    int64_t value = 0; // [状態] - 変換結果の格納先を 0 で初期化する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_parse_int64(&value, "", 10); // [手順] - 空文字列を cplat_parse_int64 に渡す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_INTEGER,
              actual_ret); // [確認_異常系] - cplat_parse_int64 の戻り値が CPLAT_ERR_INVALID_INTEGER であること。
}

// 末尾に余分な文字が残る場合に変換エラーになることの確認
TEST_F(parseTest, parse_int64_trailing_garbage)
{
    // Arrange
    int64_t value = 0; // [状態] - 変換結果の格納先を 0 で初期化する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_parse_int64(&value, "12abc", 10); // [手順] - 末尾に文字が続く "12abc" を渡す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_INTEGER,
              actual_ret); // [確認_異常系] - cplat_parse_int64 の戻り値が CPLAT_ERR_INVALID_INTEGER であること。
}

// int64_t の範囲を超える入力が範囲外エラーになることの確認
TEST_F(parseTest, parse_int64_out_of_range)
{
    // Arrange
    int64_t value = 0; // [状態] - 変換結果の格納先を 0 で初期化する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_parse_int64(&value, "99999999999999999999",
                                       10); // [手順] - INT64_MAX を超える 20 桁の数値を渡す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_RANGE,
              actual_ret); // [確認_異常系] - cplat_parse_int64 の戻り値が CPLAT_ERR_OUT_OF_RANGE であること。
}

// 格納先が NULL の場合に引数エラーになることの確認
TEST_F(parseTest, parse_int64_null_value_out)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret = cplat_parse_int64(NULL, "1", 10); // [手順] - 格納先に NULL を渡して cplat_parse_int64 を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_parse_int64 の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// 変換元が NULL の場合に引数エラーになることの確認
TEST_F(parseTest, parse_int64_null_text)
{
    // Arrange
    int64_t value = 0; // [状態] - 変換結果の格納先を 0 で初期化する。

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_parse_int64(&value, NULL, 10); // [手順] - 変換元に NULL を渡して cplat_parse_int64 を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_parse_int64 の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// 範囲外の基数が引数エラーになることの確認
TEST_F(parseTest, parse_int64_invalid_base)
{
    // Arrange
    int64_t value = 0; // [状態] - 変換結果の格納先を 0 で初期化する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_parse_int64(&value, "1", 37); // [手順] - 上限 36 を超える基数 37 を指定して呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_parse_int64 の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// 符号なし整数が変換されることの確認
TEST_F(parseTest, parse_uint64_decimal)
{
    // Arrange
    uint64_t value = 0u; // [状態] - 変換結果の格納先を 0 で初期化する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_parse_uint64(&value, "18446744073709551615",
                                        10); // [手順] - UINT64_MAX の 10 進表記を cplat_parse_uint64 に渡す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_parse_uint64 の戻り値が CPLAT_OK であること。
    EXPECT_EQ(UINT64_MAX, value);    // [確認_正常系] - 変換結果が UINT64_MAX であること。
}

// 負値の入力が折り返されずに範囲外エラーになることの確認
TEST_F(parseTest, parse_uint64_rejects_negative)
{
    // Arrange
    uint64_t value = 0u; // [状態] - 変換結果の格納先を 0 で初期化する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_parse_uint64(&value, "-1", 10); // [手順] - "-1" を cplat_parse_uint64 に渡す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_RANGE,
              actual_ret); // [確認_異常系] - cplat_parse_uint64 の戻り値が CPLAT_ERR_OUT_OF_RANGE であること。
}

// 先頭に空白を伴う負値も範囲外エラーになることの確認
TEST_F(parseTest, parse_uint64_rejects_negative_with_leading_space)
{
    // Arrange
    uint64_t value = 0u; // [状態] - 変換結果の格納先を 0 で初期化する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_parse_uint64(&value, "  -1", 10); // [手順] - 先頭に空白を持つ "  -1" を渡す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_RANGE,
              actual_ret); // [確認_異常系] - cplat_parse_uint64 の戻り値が CPLAT_ERR_OUT_OF_RANGE であること。
}

// int の範囲に収まる値が変換されることの確認
TEST_F(parseTest, parse_int_boundary_values)
{
    // Arrange
    int max_value = 0;
    int min_value = 0; // [状態] - 上限用と下限用の格納先をそれぞれ 0 で初期化する。
    char max_text[32];
    char min_text[32];

    snprintf(max_text, sizeof(max_text), "%d", INT_MAX);
    snprintf(min_text, sizeof(min_text), "%d", INT_MIN);

    // Pre-Assert

    // Act
    int max_ret = cplat_parse_int(&max_value, max_text, 10); // [手順] - INT_MAX の 10 進表記を変換する。
    int min_ret = cplat_parse_int(&min_value, min_text, 10); // [手順] - INT_MIN の 10 進表記を変換する。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              max_ret);            // [確認_正常系] - INT_MAX を渡した cplat_parse_int の戻り値が CPLAT_OK であること。
    EXPECT_EQ(INT_MAX, max_value); // [確認_正常系] - 変換結果が INT_MAX であること。
    EXPECT_EQ(CPLAT_OK,
              min_ret);            // [確認_正常系] - INT_MIN を渡した cplat_parse_int の戻り値が CPLAT_OK であること。
    EXPECT_EQ(INT_MIN, min_value); // [確認_正常系] - 変換結果が INT_MIN であること。
}

// int の範囲を超える値が範囲外エラーになることの確認
TEST_F(parseTest, parse_int_out_of_range)
{
    // Arrange
    int upper_value = 0;
    int lower_value = 0; // [状態] - 上限超過用と下限超過用の格納先を 0 で初期化する。

    // Pre-Assert

    // Act
    int upper_ret = cplat_parse_int(&upper_value, "2147483648", 10); // [手順] - INT_MAX を 1 超える 2147483648 を渡す。
    int lower_ret =
        cplat_parse_int(&lower_value, "-2147483649", 10); // [手順] - INT_MIN を 1 下回る -2147483649 を渡す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_OUT_OF_RANGE,
        upper_ret); // [確認_異常系] - INT_MAX を超える値を渡した cplat_parse_int の戻り値が CPLAT_ERR_OUT_OF_RANGE であること。
    EXPECT_EQ(
        CPLAT_ERR_OUT_OF_RANGE,
        lower_ret); // [確認_異常系] - INT_MIN を下回る値を渡した cplat_parse_int の戻り値が CPLAT_ERR_OUT_OF_RANGE であること。
}

// 浮動小数の文字列が変換されることの確認
TEST_F(parseTest, parse_double_decimal)
{
    // Arrange
    double value = 0.0; // [状態] - 変換結果の格納先を 0.0 で初期化する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_parse_double(&value, "1.5"); // [手順] - "1.5" を cplat_parse_double に渡す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_parse_double の戻り値が CPLAT_OK であること。
    EXPECT_DOUBLE_EQ(1.5, value);    // [確認_正常系] - 変換結果が 1.5 であること。
}

// 指数表記の文字列が変換されることの確認
TEST_F(parseTest, parse_double_exponent)
{
    // Arrange
    double value = 0.0; // [状態] - 変換結果の格納先を 0.0 で初期化する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_parse_double(&value, "2e3"); // [手順] - 指数表記の "2e3" を cplat_parse_double に渡す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_parse_double の戻り値が CPLAT_OK であること。
    EXPECT_DOUBLE_EQ(2000.0, value); // [確認_正常系] - 変換結果が 2000.0 であること。
}

// 数値として解釈できない文字列が変換エラーになることの確認
TEST_F(parseTest, parse_double_invalid_text)
{
    // Arrange
    double value = 0.0; // [状態] - 変換結果の格納先を 0.0 で初期化する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_parse_double(&value, "abc"); // [手順] - 数値でない "abc" を cplat_parse_double に渡す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_INTEGER,
              actual_ret); // [確認_異常系] - cplat_parse_double の戻り値が CPLAT_ERR_INVALID_INTEGER であること。
}

// 空文字列が浮動小数として変換されないことの確認
TEST_F(parseTest, parse_double_rejects_empty_text)
{
    // Arrange
    double value = 0.0; // [状態] - 変換結果の格納先を 0.0 で初期化する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_parse_double(&value, ""); // [手順] - 空文字列を cplat_parse_double に渡す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_INTEGER,
        actual_ret); // [確認_異常系] - 空文字列を渡した cplat_parse_double の戻り値が CPLAT_ERR_INVALID_INTEGER であること。
}

// 数値の後ろに文字が残る浮動小数文字列が拒否されることの確認
TEST_F(parseTest, parse_double_rejects_trailing_garbage)
{
    // Arrange
    double value = 0.0; // [状態] - 変換結果の格納先を 0.0 で初期化する。

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_parse_double(&value,
                           "1.5x"); // [手順] - 数値の後ろに文字が残る "1.5x" を cplat_parse_double に渡す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_INTEGER,
        actual_ret); // [確認_異常系] - 末尾に文字が残る入力を渡した cplat_parse_double の戻り値が CPLAT_ERR_INVALID_INTEGER であること。
}

// double の範囲を超える入力が範囲外エラーになることの確認
TEST_F(parseTest, parse_double_out_of_range)
{
    // Arrange
    double value = 0.0; // [状態] - 変換結果の格納先を 0.0 で初期化する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_parse_double(&value, "1e400"); // [手順] - double の上限を超える "1e400" を渡す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_RANGE,
              actual_ret); // [確認_異常系] - cplat_parse_double の戻り値が CPLAT_ERR_OUT_OF_RANGE であること。
}

// cplat_parse_uint64 が不正な引数を拒否することの確認
TEST_F(parseTest, parse_uint64_rejects_invalid_arguments)
{
    // Arrange
    uint64_t value = 0u; // [状態] - 変換結果の格納先を 0 で初期化する。

    // Pre-Assert

    // Act
    int actual_ret_null_value_out =
        cplat_parse_uint64(NULL, "1", 10);                            // [手順] - value_out に NULL を指定して呼び出す。
    int actual_ret_null_text = cplat_parse_uint64(&value, NULL, 10);  // [手順] - text に NULL を指定して呼び出す。
    int actual_ret_invalid_base = cplat_parse_uint64(&value, "1", 1); // [手順] - base に 1 を指定して呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_value_out); // [確認_異常系] - value_out が NULL のとき cplat_parse_uint64 の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_text); // [確認_異常系] - text が NULL のとき cplat_parse_uint64 の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_invalid_base); // [確認_異常系] - base が 1 のとき cplat_parse_uint64 の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// cplat_parse_uint64 が末尾に余分な文字を含む入力を拒否することの確認
TEST_F(parseTest, parse_uint64_trailing_garbage)
{
    // Arrange
    uint64_t value = 0u; // [状態] - 変換結果の格納先を 0 で初期化する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_parse_uint64(&value, "12x", 10); // [手順] - 末尾に 'x' を含む "12x" を渡す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_INTEGER,
              actual_ret); // [確認_異常系] - cplat_parse_uint64 の戻り値が CPLAT_ERR_INVALID_INTEGER であること。
}

// 空文字列が符号なし整数として変換されないことの確認
TEST_F(parseTest, parse_uint64_rejects_empty_text)
{
    // Arrange
    uint64_t value = 0u; // [状態] - 変換結果の格納先を 0 で初期化する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_parse_uint64(&value, "", 10); // [手順] - 空文字列を cplat_parse_uint64 に渡す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_INTEGER,
        actual_ret); // [確認_異常系] - 空文字列を渡した cplat_parse_uint64 の戻り値が CPLAT_ERR_INVALID_INTEGER であること。
}

// cplat_parse_uint64 が uint64_t の範囲を超える入力を拒否することの確認
TEST_F(parseTest, parse_uint64_out_of_range)
{
    // Arrange
    uint64_t value = 0u; // [状態] - 変換結果の格納先を 0 で初期化する。

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_parse_uint64(&value, "99999999999999999999999999", 10); // [手順] - uint64_t の上限を超える値を渡す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_RANGE,
              actual_ret); // [確認_異常系] - cplat_parse_uint64 の戻り値が CPLAT_ERR_OUT_OF_RANGE であること。
}

// cplat_parse_int が value_out に NULL を渡された場合に拒否することの確認
TEST_F(parseTest, parse_int_rejects_null_value_out)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_parse_int(NULL, "1", 10); // [手順] - value_out に NULL を指定して cplat_parse_int を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_parse_int の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// cplat_parse_int が cplat_parse_int64 の失敗をそのまま返すことの確認
TEST_F(parseTest, parse_int_propagates_parse_int64_error)
{
    // Arrange
    int value = 0; // [状態] - 変換結果の格納先を 0 で初期化する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_parse_int(&value, "abc", 10); // [手順] - 数値でない "abc" を cplat_parse_int に渡す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_INTEGER,
        actual_ret); // [確認_異常系] - cplat_parse_int64 が返した CPLAT_ERR_INVALID_INTEGER が cplat_parse_int の戻り値になること。
}

// cplat_parse_double が不正な引数を拒否することの確認
TEST_F(parseTest, parse_double_rejects_invalid_arguments)
{
    // Arrange
    double value = 0.0; // [状態] - 変換結果の格納先を 0.0 で初期化する。

    // Pre-Assert

    // Act
    int actual_ret_null_value_out = cplat_parse_double(NULL, "1.0"); // [手順] - value_out に NULL を指定して呼び出す。
    int actual_ret_null_text = cplat_parse_double(&value, NULL);     // [手順] - text に NULL を指定して呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_value_out); // [確認_異常系] - value_out が NULL のとき cplat_parse_double の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_text); // [確認_異常系] - text が NULL のとき cplat_parse_double の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}
