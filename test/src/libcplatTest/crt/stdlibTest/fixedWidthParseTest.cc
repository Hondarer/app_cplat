#include <testfw.h>
#include <cplat/crt/stdlib.h>

#include <array>
#include <cstdint>

// cplat_parse_int8 が有効な表記と型の境界値を変換することの確認
TEST(fixedWidthParseTest, int8_valid_values)
{
    // Arrange
    struct valid_case
    {
        const char *text;
        int base;
        int8_t expected;
    };
    const std::array<valid_case, 12> cases = {{
        {"0", 10, static_cast<int8_t>(0)},
        {"127", 10, static_cast<int8_t>(127)},
        {"+7", 10, static_cast<int8_t>(7)},
        {" \t7", 10, static_cast<int8_t>(7)},
        {"7f", 16, static_cast<int8_t>(127)},
        {"101", 2, static_cast<int8_t>(5)},
        {"z", 36, static_cast<int8_t>(35)},
        {"0x10", 0, static_cast<int8_t>(16)},
        {"077", 0, static_cast<int8_t>(63)},
        {"-128", 10, static_cast<int8_t>(-128)},
        {"-0", 10, static_cast<int8_t>(0)},
        {"-7", 10, static_cast<int8_t>(-7)},
    }}; // [状態] - 有効な表記、基数、型の境界値を用意する。
    std::array<int8_t, 12> values = {};
    std::array<int, 12> actual_ret = {};

    // Pre-Assert

    // Act
    for (size_t i = 0; i < cases.size(); i++)
    {
        actual_ret[i] =
            cplat_parse_int8(&values[i], cases[i].text,
                             cases[i].base); // [手順] - 各入力と基数を指定して cplat_parse_int8() を呼び出す。
    }

    // Assert
    for (size_t i = 0; i < cases.size(); i++)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(CPLAT_OK, actual_ret[i]);      // [確認_正常系 回数=12] - 各入力の変換が成功すること。
        EXPECT_EQ(cases[i].expected, values[i]); // [確認_正常系 回数=12] - 出力が各入力の期待値と一致すること。
    }
}

// cplat_parse_int8 が不正入力を拒否し出力先を変更しないことの確認
TEST(fixedWidthParseTest, int8_invalid_values_preserve_output)
{
    // Arrange
    struct invalid_case
    {
        const char *text;
        int base;
        int expected;
    };
    const std::array<invalid_case, 13> cases = {{
        {"", 10, CPLAT_ERR_INVALID_INTEGER},
        {"  ", 10, CPLAT_ERR_INVALID_INTEGER},
        {"abc", 10, CPLAT_ERR_INVALID_INTEGER},
        {"12x", 10, CPLAT_ERR_INVALID_INTEGER},
        {"1 ", 10, CPLAT_ERR_INVALID_INTEGER},
        {"128", 10, CPLAT_ERR_OUT_OF_RANGE},
        {"18446744073709551616", 10, CPLAT_ERR_OUT_OF_RANGE},
        {nullptr, 10, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", -1, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", 1, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", 37, CPLAT_ERR_INVALID_ARGUMENT},
        {"-x", 1, CPLAT_ERR_INVALID_ARGUMENT},
        {"-129", 10, CPLAT_ERR_OUT_OF_RANGE},
    }}; // [状態] - 不正な文字列、値域、基数、nullptr 文字列を用意する。
    std::array<int8_t, 13> values;
    values.fill(static_cast<int8_t>(42)); // [状態] - 出力先を 42 で初期化する。
    std::array<int, 13> actual_ret = {};

    // Pre-Assert

    // Act
    for (size_t i = 0; i < cases.size(); i++)
    {
        actual_ret[i] = cplat_parse_int8(&values[i], cases[i].text,
                                         cases[i].base); // [手順] - 不正入力を指定して cplat_parse_int8() を呼び出す。
    }

    // Assert
    for (size_t i = 0; i < cases.size(); i++)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(cases[i].expected, actual_ret[i]);   // [確認_異常系 回数=13] - エラーが各入力の期待値と一致すること。
        EXPECT_EQ(static_cast<int8_t>(42), values[i]); // [確認_異常系 回数=13] - 失敗時の出力先が 42 のままであること。
    }
}

// cplat_parse_int8 が NULL 出力先を拒否することの確認
TEST(fixedWidthParseTest, int8_null_output)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_parse_int8(nullptr, "1", 10); // [手順] - 出力先に nullptr を指定して cplat_parse_int8() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret); // [確認_異常系] - nullptr 出力先が無効引数として拒否されること。
}

// cplat_parse_int16 が有効な表記と型の境界値を変換することの確認
TEST(fixedWidthParseTest, int16_valid_values)
{
    // Arrange
    struct valid_case
    {
        const char *text;
        int base;
        int16_t expected;
    };
    const std::array<valid_case, 12> cases = {{
        {"0", 10, static_cast<int16_t>(0)},
        {"32767", 10, static_cast<int16_t>(32767)},
        {"+7", 10, static_cast<int16_t>(7)},
        {" \t7", 10, static_cast<int16_t>(7)},
        {"7f", 16, static_cast<int16_t>(127)},
        {"101", 2, static_cast<int16_t>(5)},
        {"z", 36, static_cast<int16_t>(35)},
        {"0x10", 0, static_cast<int16_t>(16)},
        {"077", 0, static_cast<int16_t>(63)},
        {"-32768", 10, static_cast<int16_t>(-32768)},
        {"-0", 10, static_cast<int16_t>(0)},
        {"-7", 10, static_cast<int16_t>(-7)},
    }}; // [状態] - 有効な表記、基数、型の境界値を用意する。
    std::array<int16_t, 12> values = {};
    std::array<int, 12> actual_ret = {};

    // Pre-Assert

    // Act
    for (size_t i = 0; i < cases.size(); i++)
    {
        actual_ret[i] =
            cplat_parse_int16(&values[i], cases[i].text,
                              cases[i].base); // [手順] - 各入力と基数を指定して cplat_parse_int16() を呼び出す。
    }

    // Assert
    for (size_t i = 0; i < cases.size(); i++)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(CPLAT_OK, actual_ret[i]);      // [確認_正常系 回数=12] - 各入力の変換が成功すること。
        EXPECT_EQ(cases[i].expected, values[i]); // [確認_正常系 回数=12] - 出力が各入力の期待値と一致すること。
    }
}

// cplat_parse_int16 が不正入力を拒否し出力先を変更しないことの確認
TEST(fixedWidthParseTest, int16_invalid_values_preserve_output)
{
    // Arrange
    struct invalid_case
    {
        const char *text;
        int base;
        int expected;
    };
    const std::array<invalid_case, 13> cases = {{
        {"", 10, CPLAT_ERR_INVALID_INTEGER},
        {"  ", 10, CPLAT_ERR_INVALID_INTEGER},
        {"abc", 10, CPLAT_ERR_INVALID_INTEGER},
        {"12x", 10, CPLAT_ERR_INVALID_INTEGER},
        {"1 ", 10, CPLAT_ERR_INVALID_INTEGER},
        {"32768", 10, CPLAT_ERR_OUT_OF_RANGE},
        {"18446744073709551616", 10, CPLAT_ERR_OUT_OF_RANGE},
        {nullptr, 10, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", -1, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", 1, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", 37, CPLAT_ERR_INVALID_ARGUMENT},
        {"-x", 1, CPLAT_ERR_INVALID_ARGUMENT},
        {"-32769", 10, CPLAT_ERR_OUT_OF_RANGE},
    }}; // [状態] - 不正な文字列、値域、基数、nullptr 文字列を用意する。
    std::array<int16_t, 13> values;
    values.fill(static_cast<int16_t>(42)); // [状態] - 出力先を 42 で初期化する。
    std::array<int, 13> actual_ret = {};

    // Pre-Assert

    // Act
    for (size_t i = 0; i < cases.size(); i++)
    {
        actual_ret[i] = cplat_parse_int16(
            &values[i], cases[i].text, cases[i].base); // [手順] - 不正入力を指定して cplat_parse_int16() を呼び出す。
    }

    // Assert
    for (size_t i = 0; i < cases.size(); i++)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(cases[i].expected, actual_ret[i]); // [確認_異常系 回数=13] - エラーが各入力の期待値と一致すること。
        EXPECT_EQ(static_cast<int16_t>(42),
                  values[i]); // [確認_異常系 回数=13] - 失敗時の出力先が 42 のままであること。
    }
}

// cplat_parse_int16 が NULL 出力先を拒否することの確認
TEST(fixedWidthParseTest, int16_null_output)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_parse_int16(nullptr, "1", 10); // [手順] - 出力先に nullptr を指定して cplat_parse_int16() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret); // [確認_異常系] - nullptr 出力先が無効引数として拒否されること。
}

// cplat_parse_int32 が有効な表記と型の境界値を変換することの確認
TEST(fixedWidthParseTest, int32_valid_values)
{
    // Arrange
    struct valid_case
    {
        const char *text;
        int base;
        int32_t expected;
    };
    const std::array<valid_case, 12> cases = {{
        {"0", 10, static_cast<int32_t>(0)},
        {"2147483647", 10, static_cast<int32_t>(2147483647)},
        {"+7", 10, static_cast<int32_t>(7)},
        {" \t7", 10, static_cast<int32_t>(7)},
        {"7f", 16, static_cast<int32_t>(127)},
        {"101", 2, static_cast<int32_t>(5)},
        {"z", 36, static_cast<int32_t>(35)},
        {"0x10", 0, static_cast<int32_t>(16)},
        {"077", 0, static_cast<int32_t>(63)},
        {"-2147483648", 10, INT32_MIN},
        {"-0", 10, static_cast<int32_t>(0)},
        {"-7", 10, static_cast<int32_t>(-7)},
    }}; // [状態] - 有効な表記、基数、型の境界値を用意する。
    std::array<int32_t, 12> values = {};
    std::array<int, 12> actual_ret = {};

    // Pre-Assert

    // Act
    for (size_t i = 0; i < cases.size(); i++)
    {
        actual_ret[i] =
            cplat_parse_int32(&values[i], cases[i].text,
                              cases[i].base); // [手順] - 各入力と基数を指定して cplat_parse_int32() を呼び出す。
    }

    // Assert
    for (size_t i = 0; i < cases.size(); i++)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(CPLAT_OK, actual_ret[i]);      // [確認_正常系 回数=12] - 各入力の変換が成功すること。
        EXPECT_EQ(cases[i].expected, values[i]); // [確認_正常系 回数=12] - 出力が各入力の期待値と一致すること。
    }
}

// cplat_parse_int32 が不正入力を拒否し出力先を変更しないことの確認
TEST(fixedWidthParseTest, int32_invalid_values_preserve_output)
{
    // Arrange
    struct invalid_case
    {
        const char *text;
        int base;
        int expected;
    };
    const std::array<invalid_case, 13> cases = {{
        {"", 10, CPLAT_ERR_INVALID_INTEGER},
        {"  ", 10, CPLAT_ERR_INVALID_INTEGER},
        {"abc", 10, CPLAT_ERR_INVALID_INTEGER},
        {"12x", 10, CPLAT_ERR_INVALID_INTEGER},
        {"1 ", 10, CPLAT_ERR_INVALID_INTEGER},
        {"2147483648", 10, CPLAT_ERR_OUT_OF_RANGE},
        {"18446744073709551616", 10, CPLAT_ERR_OUT_OF_RANGE},
        {nullptr, 10, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", -1, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", 1, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", 37, CPLAT_ERR_INVALID_ARGUMENT},
        {"-x", 1, CPLAT_ERR_INVALID_ARGUMENT},
        {"-2147483649", 10, CPLAT_ERR_OUT_OF_RANGE},
    }}; // [状態] - 不正な文字列、値域、基数、nullptr 文字列を用意する。
    std::array<int32_t, 13> values;
    values.fill(static_cast<int32_t>(42)); // [状態] - 出力先を 42 で初期化する。
    std::array<int, 13> actual_ret = {};

    // Pre-Assert

    // Act
    for (size_t i = 0; i < cases.size(); i++)
    {
        actual_ret[i] = cplat_parse_int32(
            &values[i], cases[i].text, cases[i].base); // [手順] - 不正入力を指定して cplat_parse_int32() を呼び出す。
    }

    // Assert
    for (size_t i = 0; i < cases.size(); i++)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(cases[i].expected, actual_ret[i]); // [確認_異常系 回数=13] - エラーが各入力の期待値と一致すること。
        EXPECT_EQ(static_cast<int32_t>(42),
                  values[i]); // [確認_異常系 回数=13] - 失敗時の出力先が 42 のままであること。
    }
}

// cplat_parse_int32 が NULL 出力先を拒否することの確認
TEST(fixedWidthParseTest, int32_null_output)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_parse_int32(nullptr, "1", 10); // [手順] - 出力先に nullptr を指定して cplat_parse_int32() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret); // [確認_異常系] - nullptr 出力先が無効引数として拒否されること。
}

// cplat_parse_uint8 が有効な表記と型の境界値を変換することの確認
TEST(fixedWidthParseTest, uint8_valid_values)
{
    // Arrange
    struct valid_case
    {
        const char *text;
        int base;
        uint8_t expected;
    };
    const std::array<valid_case, 9> cases = {{
        {"0", 10, static_cast<uint8_t>(0)},
        {"255", 10, static_cast<uint8_t>(255)},
        {"+7", 10, static_cast<uint8_t>(7)},
        {" \t7", 10, static_cast<uint8_t>(7)},
        {"7f", 16, static_cast<uint8_t>(127)},
        {"101", 2, static_cast<uint8_t>(5)},
        {"z", 36, static_cast<uint8_t>(35)},
        {"0x10", 0, static_cast<uint8_t>(16)},
        {"077", 0, static_cast<uint8_t>(63)},
    }}; // [状態] - 有効な表記、基数、型の境界値を用意する。
    std::array<uint8_t, 9> values = {};
    std::array<int, 9> actual_ret = {};

    // Pre-Assert

    // Act
    for (size_t i = 0; i < cases.size(); i++)
    {
        actual_ret[i] =
            cplat_parse_uint8(&values[i], cases[i].text,
                              cases[i].base); // [手順] - 各入力と基数を指定して cplat_parse_uint8() を呼び出す。
    }

    // Assert
    for (size_t i = 0; i < cases.size(); i++)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(CPLAT_OK, actual_ret[i]);      // [確認_正常系 回数=9] - 各入力の変換が成功すること。
        EXPECT_EQ(cases[i].expected, values[i]); // [確認_正常系 回数=9] - 出力が各入力の期待値と一致すること。
    }
}

// cplat_parse_uint8 が不正入力を拒否し出力先を変更しないことの確認
TEST(fixedWidthParseTest, uint8_invalid_values_preserve_output)
{
    // Arrange
    struct invalid_case
    {
        const char *text;
        int base;
        int expected;
    };
    const std::array<invalid_case, 16> cases = {{
        {"", 10, CPLAT_ERR_INVALID_INTEGER},
        {"  ", 10, CPLAT_ERR_INVALID_INTEGER},
        {"abc", 10, CPLAT_ERR_INVALID_INTEGER},
        {"12x", 10, CPLAT_ERR_INVALID_INTEGER},
        {"1 ", 10, CPLAT_ERR_INVALID_INTEGER},
        {"256", 10, CPLAT_ERR_OUT_OF_RANGE},
        {"18446744073709551616", 10, CPLAT_ERR_OUT_OF_RANGE},
        {nullptr, 10, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", -1, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", 1, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", 37, CPLAT_ERR_INVALID_ARGUMENT},
        {"-x", 1, CPLAT_ERR_INVALID_ARGUMENT},
        {"-1", 10, CPLAT_ERR_OUT_OF_RANGE},
        {"-0", 10, CPLAT_ERR_OUT_OF_RANGE},
        {" \t-0", 10, CPLAT_ERR_OUT_OF_RANGE},
        {"-x", 10, CPLAT_ERR_OUT_OF_RANGE},
    }}; // [状態] - 不正な文字列、値域、基数、nullptr 文字列を用意する。
    std::array<uint8_t, 16> values;
    values.fill(static_cast<uint8_t>(42)); // [状態] - 出力先を 42 で初期化する。
    std::array<int, 16> actual_ret = {};

    // Pre-Assert

    // Act
    for (size_t i = 0; i < cases.size(); i++)
    {
        actual_ret[i] = cplat_parse_uint8(
            &values[i], cases[i].text, cases[i].base); // [手順] - 不正入力を指定して cplat_parse_uint8() を呼び出す。
    }

    // Assert
    for (size_t i = 0; i < cases.size(); i++)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(cases[i].expected, actual_ret[i]); // [確認_異常系 回数=16] - エラーが各入力の期待値と一致すること。
        EXPECT_EQ(static_cast<uint8_t>(42),
                  values[i]); // [確認_異常系 回数=16] - 失敗時の出力先が 42 のままであること。
    }
}

// cplat_parse_uint8 が NULL 出力先を拒否することの確認
TEST(fixedWidthParseTest, uint8_null_output)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_parse_uint8(nullptr, "1", 10); // [手順] - 出力先に nullptr を指定して cplat_parse_uint8() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret); // [確認_異常系] - nullptr 出力先が無効引数として拒否されること。
}

// cplat_parse_uint16 が有効な表記と型の境界値を変換することの確認
TEST(fixedWidthParseTest, uint16_valid_values)
{
    // Arrange
    struct valid_case
    {
        const char *text;
        int base;
        uint16_t expected;
    };
    const std::array<valid_case, 9> cases = {{
        {"0", 10, static_cast<uint16_t>(0)},
        {"65535", 10, static_cast<uint16_t>(65535)},
        {"+7", 10, static_cast<uint16_t>(7)},
        {" \t7", 10, static_cast<uint16_t>(7)},
        {"7f", 16, static_cast<uint16_t>(127)},
        {"101", 2, static_cast<uint16_t>(5)},
        {"z", 36, static_cast<uint16_t>(35)},
        {"0x10", 0, static_cast<uint16_t>(16)},
        {"077", 0, static_cast<uint16_t>(63)},
    }}; // [状態] - 有効な表記、基数、型の境界値を用意する。
    std::array<uint16_t, 9> values = {};
    std::array<int, 9> actual_ret = {};

    // Pre-Assert

    // Act
    for (size_t i = 0; i < cases.size(); i++)
    {
        actual_ret[i] =
            cplat_parse_uint16(&values[i], cases[i].text,
                               cases[i].base); // [手順] - 各入力と基数を指定して cplat_parse_uint16() を呼び出す。
    }

    // Assert
    for (size_t i = 0; i < cases.size(); i++)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(CPLAT_OK, actual_ret[i]);      // [確認_正常系 回数=9] - 各入力の変換が成功すること。
        EXPECT_EQ(cases[i].expected, values[i]); // [確認_正常系 回数=9] - 出力が各入力の期待値と一致すること。
    }
}

// cplat_parse_uint16 が不正入力を拒否し出力先を変更しないことの確認
TEST(fixedWidthParseTest, uint16_invalid_values_preserve_output)
{
    // Arrange
    struct invalid_case
    {
        const char *text;
        int base;
        int expected;
    };
    const std::array<invalid_case, 16> cases = {{
        {"", 10, CPLAT_ERR_INVALID_INTEGER},
        {"  ", 10, CPLAT_ERR_INVALID_INTEGER},
        {"abc", 10, CPLAT_ERR_INVALID_INTEGER},
        {"12x", 10, CPLAT_ERR_INVALID_INTEGER},
        {"1 ", 10, CPLAT_ERR_INVALID_INTEGER},
        {"65536", 10, CPLAT_ERR_OUT_OF_RANGE},
        {"18446744073709551616", 10, CPLAT_ERR_OUT_OF_RANGE},
        {nullptr, 10, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", -1, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", 1, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", 37, CPLAT_ERR_INVALID_ARGUMENT},
        {"-x", 1, CPLAT_ERR_INVALID_ARGUMENT},
        {"-1", 10, CPLAT_ERR_OUT_OF_RANGE},
        {"-0", 10, CPLAT_ERR_OUT_OF_RANGE},
        {" \t-0", 10, CPLAT_ERR_OUT_OF_RANGE},
        {"-x", 10, CPLAT_ERR_OUT_OF_RANGE},
    }}; // [状態] - 不正な文字列、値域、基数、nullptr 文字列を用意する。
    std::array<uint16_t, 16> values;
    values.fill(static_cast<uint16_t>(42)); // [状態] - 出力先を 42 で初期化する。
    std::array<int, 16> actual_ret = {};

    // Pre-Assert

    // Act
    for (size_t i = 0; i < cases.size(); i++)
    {
        actual_ret[i] = cplat_parse_uint16(
            &values[i], cases[i].text, cases[i].base); // [手順] - 不正入力を指定して cplat_parse_uint16() を呼び出す。
    }

    // Assert
    for (size_t i = 0; i < cases.size(); i++)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(cases[i].expected, actual_ret[i]); // [確認_異常系 回数=16] - エラーが各入力の期待値と一致すること。
        EXPECT_EQ(static_cast<uint16_t>(42),
                  values[i]); // [確認_異常系 回数=16] - 失敗時の出力先が 42 のままであること。
    }
}

// cplat_parse_uint16 が NULL 出力先を拒否することの確認
TEST(fixedWidthParseTest, uint16_null_output)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_parse_uint16(nullptr, "1", 10); // [手順] - 出力先に nullptr を指定して cplat_parse_uint16() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret); // [確認_異常系] - nullptr 出力先が無効引数として拒否されること。
}

// cplat_parse_uint32 が有効な表記と型の境界値を変換することの確認
TEST(fixedWidthParseTest, uint32_valid_values)
{
    // Arrange
    struct valid_case
    {
        const char *text;
        int base;
        uint32_t expected;
    };
    const std::array<valid_case, 9> cases = {{
        {"0", 10, static_cast<uint32_t>(0)},
        {"4294967295", 10, static_cast<uint32_t>(4294967295)},
        {"+7", 10, static_cast<uint32_t>(7)},
        {" \t7", 10, static_cast<uint32_t>(7)},
        {"7f", 16, static_cast<uint32_t>(127)},
        {"101", 2, static_cast<uint32_t>(5)},
        {"z", 36, static_cast<uint32_t>(35)},
        {"0x10", 0, static_cast<uint32_t>(16)},
        {"077", 0, static_cast<uint32_t>(63)},
    }}; // [状態] - 有効な表記、基数、型の境界値を用意する。
    std::array<uint32_t, 9> values = {};
    std::array<int, 9> actual_ret = {};

    // Pre-Assert

    // Act
    for (size_t i = 0; i < cases.size(); i++)
    {
        actual_ret[i] =
            cplat_parse_uint32(&values[i], cases[i].text,
                               cases[i].base); // [手順] - 各入力と基数を指定して cplat_parse_uint32() を呼び出す。
    }

    // Assert
    for (size_t i = 0; i < cases.size(); i++)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(CPLAT_OK, actual_ret[i]);      // [確認_正常系 回数=9] - 各入力の変換が成功すること。
        EXPECT_EQ(cases[i].expected, values[i]); // [確認_正常系 回数=9] - 出力が各入力の期待値と一致すること。
    }
}

// cplat_parse_uint32 が不正入力を拒否し出力先を変更しないことの確認
TEST(fixedWidthParseTest, uint32_invalid_values_preserve_output)
{
    // Arrange
    struct invalid_case
    {
        const char *text;
        int base;
        int expected;
    };
    const std::array<invalid_case, 16> cases = {{
        {"", 10, CPLAT_ERR_INVALID_INTEGER},
        {"  ", 10, CPLAT_ERR_INVALID_INTEGER},
        {"abc", 10, CPLAT_ERR_INVALID_INTEGER},
        {"12x", 10, CPLAT_ERR_INVALID_INTEGER},
        {"1 ", 10, CPLAT_ERR_INVALID_INTEGER},
        {"4294967296", 10, CPLAT_ERR_OUT_OF_RANGE},
        {"18446744073709551616", 10, CPLAT_ERR_OUT_OF_RANGE},
        {nullptr, 10, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", -1, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", 1, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", 37, CPLAT_ERR_INVALID_ARGUMENT},
        {"-x", 1, CPLAT_ERR_INVALID_ARGUMENT},
        {"-1", 10, CPLAT_ERR_OUT_OF_RANGE},
        {"-0", 10, CPLAT_ERR_OUT_OF_RANGE},
        {" \t-0", 10, CPLAT_ERR_OUT_OF_RANGE},
        {"-x", 10, CPLAT_ERR_OUT_OF_RANGE},
    }}; // [状態] - 不正な文字列、値域、基数、nullptr 文字列を用意する。
    std::array<uint32_t, 16> values;
    values.fill(static_cast<uint32_t>(42)); // [状態] - 出力先を 42 で初期化する。
    std::array<int, 16> actual_ret = {};

    // Pre-Assert

    // Act
    for (size_t i = 0; i < cases.size(); i++)
    {
        actual_ret[i] = cplat_parse_uint32(
            &values[i], cases[i].text, cases[i].base); // [手順] - 不正入力を指定して cplat_parse_uint32() を呼び出す。
    }

    // Assert
    for (size_t i = 0; i < cases.size(); i++)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(cases[i].expected, actual_ret[i]); // [確認_異常系 回数=16] - エラーが各入力の期待値と一致すること。
        EXPECT_EQ(static_cast<uint32_t>(42),
                  values[i]); // [確認_異常系 回数=16] - 失敗時の出力先が 42 のままであること。
    }
}

// cplat_parse_uint32 が NULL 出力先を拒否することの確認
TEST(fixedWidthParseTest, uint32_null_output)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_parse_uint32(nullptr, "1", 10); // [手順] - 出力先に nullptr を指定して cplat_parse_uint32() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret); // [確認_異常系] - nullptr 出力先が無効引数として拒否されること。
}
