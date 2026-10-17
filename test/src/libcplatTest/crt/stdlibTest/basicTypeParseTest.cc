#include <testfw.h>
#include <cplat/crt/stdlib.h>

#include <array>
#include <limits>

// cplat_parse_char が有効な表記と基本型の境界値を変換することの確認
TEST(basicTypeParseTest, char_valid_values)
{
    // Arrange
    struct valid_case
    {
        const char *text;
        int base;
        char expected;
    };
    const std::array<valid_case, 11> cases = {{
        {"0", 10, static_cast<char>(0)},
        {"-128", 10, std::numeric_limits<char>::min()},
        {"127", 10, std::numeric_limits<char>::max()},
        {"+7", 10, static_cast<char>(7)},
        {" \t7", 10, static_cast<char>(7)},
        {"7f", 16, static_cast<char>(127)},
        {"101", 2, static_cast<char>(5)},
        {"z", 36, static_cast<char>(35)},
        {"0x10", 0, static_cast<char>(16)},
        {"077", 0, static_cast<char>(63)},
        {"-0", 10, static_cast<char>(0)},
    }}; // [状態] - 表記、基数、基本型の境界値を用意する。
    std::array<char, 11> values = {};
    std::array<int, 11> actual_ret = {};

    // Pre-Assert

    // Act
    for (size_t i = 0; i < cases.size(); i++)
    {
        actual_ret[i] = cplat_parse_char(&values[i], cases[i].text,
                                         cases[i].base); // [手順] - 各入力を指定して cplat_parse_char() を呼び出す。
    }

    // Assert
    for (size_t i = 0; i < cases.size(); i++)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(CPLAT_OK, actual_ret[i]);      // [確認_正常系 回数=11] - 変換が成功すること。
        EXPECT_EQ(cases[i].expected, values[i]); // [確認_正常系 回数=11] - 出力が期待値と一致すること。
    }
}

// cplat_parse_char が不正入力を拒否し出力を保持することの確認
TEST(basicTypeParseTest, char_invalid_values_preserve_output)
{
    // Arrange
    struct invalid_case
    {
        const char *text;
        int base;
        int expected;
    };
    const std::array<invalid_case, 12> cases = {{
        {"", 10, CPLAT_ERR_INVALID_INTEGER},
        {" ", 10, CPLAT_ERR_INVALID_INTEGER},
        {"abc", 10, CPLAT_ERR_INVALID_INTEGER},
        {"12x", 10, CPLAT_ERR_INVALID_INTEGER},
        {"1 ", 10, CPLAT_ERR_INVALID_INTEGER},
        {"-129", 10, CPLAT_ERR_OUT_OF_RANGE},
        {"128", 10, CPLAT_ERR_OUT_OF_RANGE},
        {nullptr, 10, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", -1, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", 1, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", 37, CPLAT_ERR_INVALID_ARGUMENT},
        {"-x", 1, CPLAT_ERR_INVALID_ARGUMENT},
    }}; // [状態] - 範囲外、不正文字列、nullptr、無効基数を用意する。
    std::array<char, 12> values;
    values.fill(static_cast<char>(42)); // [状態] - 出力を 42 で初期化する。
    std::array<int, 12> actual_ret = {};

    // Pre-Assert

    // Act
    for (size_t i = 0; i < cases.size(); i++)
    {
        actual_ret[i] = cplat_parse_char(&values[i], cases[i].text,
                                         cases[i].base); // [手順] - 不正入力を指定して cplat_parse_char() を呼び出す。
    }

    // Assert
    for (size_t i = 0; i < cases.size(); i++)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(cases[i].expected, actual_ret[i]); // [確認_異常系 回数=12] - 対応するエラーを返すこと。
        EXPECT_EQ(static_cast<char>(42), values[i]); // [確認_異常系 回数=12] - 出力が変更されないこと。
    }
}

// cplat_parse_char が NULL 出力先を拒否することの確認
TEST(basicTypeParseTest, char_null_output)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_parse_char(nullptr, "42", 10); // [手順] - 出力先に nullptr を指定して cplat_parse_char() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret); // [確認_異常系] - 無効引数エラーを返すこと。
}

// cplat_parse_schar が有効な表記と基本型の境界値を変換することの確認
TEST(basicTypeParseTest, schar_valid_values)
{
    // Arrange
    struct valid_case
    {
        const char *text;
        int base;
        signed char expected;
    };
    const std::array<valid_case, 11> cases = {{
        {"0", 10, static_cast<signed char>(0)},
        {"-128", 10, std::numeric_limits<signed char>::min()},
        {"127", 10, std::numeric_limits<signed char>::max()},
        {"+7", 10, static_cast<signed char>(7)},
        {" \t7", 10, static_cast<signed char>(7)},
        {"7f", 16, static_cast<signed char>(127)},
        {"101", 2, static_cast<signed char>(5)},
        {"z", 36, static_cast<signed char>(35)},
        {"0x10", 0, static_cast<signed char>(16)},
        {"077", 0, static_cast<signed char>(63)},
        {"-0", 10, static_cast<signed char>(0)},
    }}; // [状態] - 表記、基数、基本型の境界値を用意する。
    std::array<signed char, 11> values = {};
    std::array<int, 11> actual_ret = {};

    // Pre-Assert

    // Act
    for (size_t i = 0; i < cases.size(); i++)
    {
        actual_ret[i] = cplat_parse_schar(&values[i], cases[i].text,
                                          cases[i].base); // [手順] - 各入力を指定して cplat_parse_schar() を呼び出す。
    }

    // Assert
    for (size_t i = 0; i < cases.size(); i++)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(CPLAT_OK, actual_ret[i]);      // [確認_正常系 回数=11] - 変換が成功すること。
        EXPECT_EQ(cases[i].expected, values[i]); // [確認_正常系 回数=11] - 出力が期待値と一致すること。
    }
}

// cplat_parse_schar が不正入力を拒否し出力を保持することの確認
TEST(basicTypeParseTest, schar_invalid_values_preserve_output)
{
    // Arrange
    struct invalid_case
    {
        const char *text;
        int base;
        int expected;
    };
    const std::array<invalid_case, 12> cases = {{
        {"", 10, CPLAT_ERR_INVALID_INTEGER},
        {" ", 10, CPLAT_ERR_INVALID_INTEGER},
        {"abc", 10, CPLAT_ERR_INVALID_INTEGER},
        {"12x", 10, CPLAT_ERR_INVALID_INTEGER},
        {"1 ", 10, CPLAT_ERR_INVALID_INTEGER},
        {"-129", 10, CPLAT_ERR_OUT_OF_RANGE},
        {"128", 10, CPLAT_ERR_OUT_OF_RANGE},
        {nullptr, 10, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", -1, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", 1, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", 37, CPLAT_ERR_INVALID_ARGUMENT},
        {"-x", 1, CPLAT_ERR_INVALID_ARGUMENT},
    }}; // [状態] - 範囲外、不正文字列、nullptr、無効基数を用意する。
    std::array<signed char, 12> values;
    values.fill(static_cast<signed char>(42)); // [状態] - 出力を 42 で初期化する。
    std::array<int, 12> actual_ret = {};

    // Pre-Assert

    // Act
    for (size_t i = 0; i < cases.size(); i++)
    {
        actual_ret[i] = cplat_parse_schar(
            &values[i], cases[i].text, cases[i].base); // [手順] - 不正入力を指定して cplat_parse_schar() を呼び出す。
    }

    // Assert
    for (size_t i = 0; i < cases.size(); i++)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(cases[i].expected, actual_ret[i]);        // [確認_異常系 回数=12] - 対応するエラーを返すこと。
        EXPECT_EQ(static_cast<signed char>(42), values[i]); // [確認_異常系 回数=12] - 出力が変更されないこと。
    }
}

// cplat_parse_schar が NULL 出力先を拒否することの確認
TEST(basicTypeParseTest, schar_null_output)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_parse_schar(nullptr, "42", 10); // [手順] - 出力先に nullptr を指定して cplat_parse_schar() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret); // [確認_異常系] - 無効引数エラーを返すこと。
}

// cplat_parse_uchar が有効な表記と基本型の境界値を変換することの確認
TEST(basicTypeParseTest, uchar_valid_values)
{
    // Arrange
    struct valid_case
    {
        const char *text;
        int base;
        unsigned char expected;
    };
    const std::array<valid_case, 10> cases = {{
        {"0", 10, static_cast<unsigned char>(0)},
        {"0", 10, std::numeric_limits<unsigned char>::min()},
        {"255", 10, std::numeric_limits<unsigned char>::max()},
        {"+7", 10, static_cast<unsigned char>(7)},
        {" \t7", 10, static_cast<unsigned char>(7)},
        {"7f", 16, static_cast<unsigned char>(127)},
        {"101", 2, static_cast<unsigned char>(5)},
        {"z", 36, static_cast<unsigned char>(35)},
        {"0x10", 0, static_cast<unsigned char>(16)},
        {"077", 0, static_cast<unsigned char>(63)},
    }}; // [状態] - 表記、基数、基本型の境界値を用意する。
    std::array<unsigned char, 10> values = {};
    std::array<int, 10> actual_ret = {};

    // Pre-Assert

    // Act
    for (size_t i = 0; i < cases.size(); i++)
    {
        actual_ret[i] = cplat_parse_uchar(&values[i], cases[i].text,
                                          cases[i].base); // [手順] - 各入力を指定して cplat_parse_uchar() を呼び出す。
    }

    // Assert
    for (size_t i = 0; i < cases.size(); i++)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(CPLAT_OK, actual_ret[i]);      // [確認_正常系 回数=10] - 変換が成功すること。
        EXPECT_EQ(cases[i].expected, values[i]); // [確認_正常系 回数=10] - 出力が期待値と一致すること。
    }
}

// cplat_parse_uchar が不正入力を拒否し出力を保持することの確認
TEST(basicTypeParseTest, uchar_invalid_values_preserve_output)
{
    // Arrange
    struct invalid_case
    {
        const char *text;
        int base;
        int expected;
    };
    const std::array<invalid_case, 15> cases = {{
        {"", 10, CPLAT_ERR_INVALID_INTEGER},
        {" ", 10, CPLAT_ERR_INVALID_INTEGER},
        {"abc", 10, CPLAT_ERR_INVALID_INTEGER},
        {"12x", 10, CPLAT_ERR_INVALID_INTEGER},
        {"1 ", 10, CPLAT_ERR_INVALID_INTEGER},
        {"-1", 10, CPLAT_ERR_OUT_OF_RANGE},
        {"256", 10, CPLAT_ERR_OUT_OF_RANGE},
        {nullptr, 10, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", -1, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", 1, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", 37, CPLAT_ERR_INVALID_ARGUMENT},
        {"-x", 1, CPLAT_ERR_INVALID_ARGUMENT},
        {"-0", 10, CPLAT_ERR_OUT_OF_RANGE},
        {" \t-1", 10, CPLAT_ERR_OUT_OF_RANGE},
        {"-x", 10, CPLAT_ERR_OUT_OF_RANGE},
    }}; // [状態] - 範囲外、不正文字列、nullptr、無効基数を用意する。
    std::array<unsigned char, 15> values;
    values.fill(static_cast<unsigned char>(42)); // [状態] - 出力を 42 で初期化する。
    std::array<int, 15> actual_ret = {};

    // Pre-Assert

    // Act
    for (size_t i = 0; i < cases.size(); i++)
    {
        actual_ret[i] = cplat_parse_uchar(
            &values[i], cases[i].text, cases[i].base); // [手順] - 不正入力を指定して cplat_parse_uchar() を呼び出す。
    }

    // Assert
    for (size_t i = 0; i < cases.size(); i++)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(cases[i].expected, actual_ret[i]);          // [確認_異常系 回数=15] - 対応するエラーを返すこと。
        EXPECT_EQ(static_cast<unsigned char>(42), values[i]); // [確認_異常系 回数=15] - 出力が変更されないこと。
    }
}

// cplat_parse_uchar が NULL 出力先を拒否することの確認
TEST(basicTypeParseTest, uchar_null_output)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_parse_uchar(nullptr, "42", 10); // [手順] - 出力先に nullptr を指定して cplat_parse_uchar() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret); // [確認_異常系] - 無効引数エラーを返すこと。
}

// cplat_parse_short が有効な表記と基本型の境界値を変換することの確認
TEST(basicTypeParseTest, short_valid_values)
{
    // Arrange
    struct valid_case
    {
        const char *text;
        int base;
        short expected;
    };
    const std::array<valid_case, 11> cases = {{
        {"0", 10, static_cast<short>(0)},
        {"-32768", 10, std::numeric_limits<short>::min()},
        {"32767", 10, std::numeric_limits<short>::max()},
        {"+7", 10, static_cast<short>(7)},
        {" \t7", 10, static_cast<short>(7)},
        {"7f", 16, static_cast<short>(127)},
        {"101", 2, static_cast<short>(5)},
        {"z", 36, static_cast<short>(35)},
        {"0x10", 0, static_cast<short>(16)},
        {"077", 0, static_cast<short>(63)},
        {"-0", 10, static_cast<short>(0)},
    }}; // [状態] - 表記、基数、基本型の境界値を用意する。
    std::array<short, 11> values = {};
    std::array<int, 11> actual_ret = {};

    // Pre-Assert

    // Act
    for (size_t i = 0; i < cases.size(); i++)
    {
        actual_ret[i] = cplat_parse_short(&values[i], cases[i].text,
                                          cases[i].base); // [手順] - 各入力を指定して cplat_parse_short() を呼び出す。
    }

    // Assert
    for (size_t i = 0; i < cases.size(); i++)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(CPLAT_OK, actual_ret[i]);      // [確認_正常系 回数=11] - 変換が成功すること。
        EXPECT_EQ(cases[i].expected, values[i]); // [確認_正常系 回数=11] - 出力が期待値と一致すること。
    }
}

// cplat_parse_short が不正入力を拒否し出力を保持することの確認
TEST(basicTypeParseTest, short_invalid_values_preserve_output)
{
    // Arrange
    struct invalid_case
    {
        const char *text;
        int base;
        int expected;
    };
    const std::array<invalid_case, 12> cases = {{
        {"", 10, CPLAT_ERR_INVALID_INTEGER},
        {" ", 10, CPLAT_ERR_INVALID_INTEGER},
        {"abc", 10, CPLAT_ERR_INVALID_INTEGER},
        {"12x", 10, CPLAT_ERR_INVALID_INTEGER},
        {"1 ", 10, CPLAT_ERR_INVALID_INTEGER},
        {"-32769", 10, CPLAT_ERR_OUT_OF_RANGE},
        {"32768", 10, CPLAT_ERR_OUT_OF_RANGE},
        {nullptr, 10, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", -1, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", 1, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", 37, CPLAT_ERR_INVALID_ARGUMENT},
        {"-x", 1, CPLAT_ERR_INVALID_ARGUMENT},
    }}; // [状態] - 範囲外、不正文字列、nullptr、無効基数を用意する。
    std::array<short, 12> values;
    values.fill(static_cast<short>(42)); // [状態] - 出力を 42 で初期化する。
    std::array<int, 12> actual_ret = {};

    // Pre-Assert

    // Act
    for (size_t i = 0; i < cases.size(); i++)
    {
        actual_ret[i] = cplat_parse_short(
            &values[i], cases[i].text, cases[i].base); // [手順] - 不正入力を指定して cplat_parse_short() を呼び出す。
    }

    // Assert
    for (size_t i = 0; i < cases.size(); i++)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(cases[i].expected, actual_ret[i]);  // [確認_異常系 回数=12] - 対応するエラーを返すこと。
        EXPECT_EQ(static_cast<short>(42), values[i]); // [確認_異常系 回数=12] - 出力が変更されないこと。
    }
}

// cplat_parse_short が NULL 出力先を拒否することの確認
TEST(basicTypeParseTest, short_null_output)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_parse_short(nullptr, "42", 10); // [手順] - 出力先に nullptr を指定して cplat_parse_short() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret); // [確認_異常系] - 無効引数エラーを返すこと。
}

// cplat_parse_ushort が有効な表記と基本型の境界値を変換することの確認
TEST(basicTypeParseTest, ushort_valid_values)
{
    // Arrange
    struct valid_case
    {
        const char *text;
        int base;
        unsigned short expected;
    };
    const std::array<valid_case, 10> cases = {{
        {"0", 10, static_cast<unsigned short>(0)},
        {"0", 10, std::numeric_limits<unsigned short>::min()},
        {"65535", 10, std::numeric_limits<unsigned short>::max()},
        {"+7", 10, static_cast<unsigned short>(7)},
        {" \t7", 10, static_cast<unsigned short>(7)},
        {"7f", 16, static_cast<unsigned short>(127)},
        {"101", 2, static_cast<unsigned short>(5)},
        {"z", 36, static_cast<unsigned short>(35)},
        {"0x10", 0, static_cast<unsigned short>(16)},
        {"077", 0, static_cast<unsigned short>(63)},
    }}; // [状態] - 表記、基数、基本型の境界値を用意する。
    std::array<unsigned short, 10> values = {};
    std::array<int, 10> actual_ret = {};

    // Pre-Assert

    // Act
    for (size_t i = 0; i < cases.size(); i++)
    {
        actual_ret[i] = cplat_parse_ushort(
            &values[i], cases[i].text, cases[i].base); // [手順] - 各入力を指定して cplat_parse_ushort() を呼び出す。
    }

    // Assert
    for (size_t i = 0; i < cases.size(); i++)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(CPLAT_OK, actual_ret[i]);      // [確認_正常系 回数=10] - 変換が成功すること。
        EXPECT_EQ(cases[i].expected, values[i]); // [確認_正常系 回数=10] - 出力が期待値と一致すること。
    }
}

// cplat_parse_ushort が不正入力を拒否し出力を保持することの確認
TEST(basicTypeParseTest, ushort_invalid_values_preserve_output)
{
    // Arrange
    struct invalid_case
    {
        const char *text;
        int base;
        int expected;
    };
    const std::array<invalid_case, 15> cases = {{
        {"", 10, CPLAT_ERR_INVALID_INTEGER},
        {" ", 10, CPLAT_ERR_INVALID_INTEGER},
        {"abc", 10, CPLAT_ERR_INVALID_INTEGER},
        {"12x", 10, CPLAT_ERR_INVALID_INTEGER},
        {"1 ", 10, CPLAT_ERR_INVALID_INTEGER},
        {"-1", 10, CPLAT_ERR_OUT_OF_RANGE},
        {"65536", 10, CPLAT_ERR_OUT_OF_RANGE},
        {nullptr, 10, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", -1, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", 1, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", 37, CPLAT_ERR_INVALID_ARGUMENT},
        {"-x", 1, CPLAT_ERR_INVALID_ARGUMENT},
        {"-0", 10, CPLAT_ERR_OUT_OF_RANGE},
        {" \t-1", 10, CPLAT_ERR_OUT_OF_RANGE},
        {"-x", 10, CPLAT_ERR_OUT_OF_RANGE},
    }}; // [状態] - 範囲外、不正文字列、nullptr、無効基数を用意する。
    std::array<unsigned short, 15> values;
    values.fill(static_cast<unsigned short>(42)); // [状態] - 出力を 42 で初期化する。
    std::array<int, 15> actual_ret = {};

    // Pre-Assert

    // Act
    for (size_t i = 0; i < cases.size(); i++)
    {
        actual_ret[i] = cplat_parse_ushort(
            &values[i], cases[i].text, cases[i].base); // [手順] - 不正入力を指定して cplat_parse_ushort() を呼び出す。
    }

    // Assert
    for (size_t i = 0; i < cases.size(); i++)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(cases[i].expected, actual_ret[i]);           // [確認_異常系 回数=15] - 対応するエラーを返すこと。
        EXPECT_EQ(static_cast<unsigned short>(42), values[i]); // [確認_異常系 回数=15] - 出力が変更されないこと。
    }
}

// cplat_parse_ushort が NULL 出力先を拒否することの確認
TEST(basicTypeParseTest, ushort_null_output)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_parse_ushort(nullptr, "42", 10); // [手順] - 出力先に nullptr を指定して cplat_parse_ushort() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret); // [確認_異常系] - 無効引数エラーを返すこと。
}

// cplat_parse_int が有効な表記と基本型の境界値を変換することの確認
TEST(basicTypeParseTest, int_valid_values)
{
    // Arrange
    struct valid_case
    {
        const char *text;
        int base;
        int expected;
    };
    const std::array<valid_case, 11> cases = {{
        {"0", 10, static_cast<int>(0)},
        {"-2147483648", 10, std::numeric_limits<int>::min()},
        {"2147483647", 10, std::numeric_limits<int>::max()},
        {"+7", 10, static_cast<int>(7)},
        {" \t7", 10, static_cast<int>(7)},
        {"80", 16, static_cast<int>(128)},
        {"101", 2, static_cast<int>(5)},
        {"z", 36, static_cast<int>(35)},
        {"0x10", 0, static_cast<int>(16)},
        {"077", 0, static_cast<int>(63)},
        {"-0", 10, static_cast<int>(0)},
    }}; // [状態] - 表記、基数、基本型の境界値を用意する。
    std::array<int, 11> values = {};
    std::array<int, 11> actual_ret = {};

    // Pre-Assert

    // Act
    for (size_t i = 0; i < cases.size(); i++)
    {
        actual_ret[i] = cplat_parse_int(&values[i], cases[i].text,
                                        cases[i].base); // [手順] - 各入力を指定して cplat_parse_int() を呼び出す。
    }

    // Assert
    for (size_t i = 0; i < cases.size(); i++)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(CPLAT_OK, actual_ret[i]);      // [確認_正常系 回数=11] - 変換が成功すること。
        EXPECT_EQ(cases[i].expected, values[i]); // [確認_正常系 回数=11] - 出力が期待値と一致すること。
    }
}

// cplat_parse_int が不正入力を拒否し出力を保持することの確認
TEST(basicTypeParseTest, int_invalid_values_preserve_output)
{
    // Arrange
    struct invalid_case
    {
        const char *text;
        int base;
        int expected;
    };
    const std::array<invalid_case, 12> cases = {{
        {"", 10, CPLAT_ERR_INVALID_INTEGER},
        {" ", 10, CPLAT_ERR_INVALID_INTEGER},
        {"abc", 10, CPLAT_ERR_INVALID_INTEGER},
        {"12x", 10, CPLAT_ERR_INVALID_INTEGER},
        {"1 ", 10, CPLAT_ERR_INVALID_INTEGER},
        {"-2147483649", 10, CPLAT_ERR_OUT_OF_RANGE},
        {"2147483648", 10, CPLAT_ERR_OUT_OF_RANGE},
        {nullptr, 10, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", -1, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", 1, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", 37, CPLAT_ERR_INVALID_ARGUMENT},
        {"-x", 1, CPLAT_ERR_INVALID_ARGUMENT},
    }}; // [状態] - 範囲外、不正文字列、nullptr、無効基数を用意する。
    std::array<int, 12> values;
    values.fill(static_cast<int>(42)); // [状態] - 出力を 42 で初期化する。
    std::array<int, 12> actual_ret = {};

    // Pre-Assert

    // Act
    for (size_t i = 0; i < cases.size(); i++)
    {
        actual_ret[i] = cplat_parse_int(&values[i], cases[i].text,
                                        cases[i].base); // [手順] - 不正入力を指定して cplat_parse_int() を呼び出す。
    }

    // Assert
    for (size_t i = 0; i < cases.size(); i++)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(cases[i].expected, actual_ret[i]); // [確認_異常系 回数=12] - 対応するエラーを返すこと。
        EXPECT_EQ(static_cast<int>(42), values[i]);  // [確認_異常系 回数=12] - 出力が変更されないこと。
    }
}

// cplat_parse_int が NULL 出力先を拒否することの確認
TEST(basicTypeParseTest, int_null_output)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_parse_int(nullptr, "42", 10); // [手順] - 出力先に nullptr を指定して cplat_parse_int() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret); // [確認_異常系] - 無効引数エラーを返すこと。
}

// cplat_parse_uint が有効な表記と基本型の境界値を変換することの確認
TEST(basicTypeParseTest, uint_valid_values)
{
    // Arrange
    struct valid_case
    {
        const char *text;
        int base;
        unsigned int expected;
    };
    const std::array<valid_case, 10> cases = {{
        {"0", 10, static_cast<unsigned int>(0)},
        {"0", 10, std::numeric_limits<unsigned int>::min()},
        {"4294967295", 10, std::numeric_limits<unsigned int>::max()},
        {"+7", 10, static_cast<unsigned int>(7)},
        {" \t7", 10, static_cast<unsigned int>(7)},
        {"7f", 16, static_cast<unsigned int>(127)},
        {"101", 2, static_cast<unsigned int>(5)},
        {"z", 36, static_cast<unsigned int>(35)},
        {"0x10", 0, static_cast<unsigned int>(16)},
        {"077", 0, static_cast<unsigned int>(63)},
    }}; // [状態] - 表記、基数、基本型の境界値を用意する。
    std::array<unsigned int, 10> values = {};
    std::array<int, 10> actual_ret = {};

    // Pre-Assert

    // Act
    for (size_t i = 0; i < cases.size(); i++)
    {
        actual_ret[i] = cplat_parse_uint(&values[i], cases[i].text,
                                         cases[i].base); // [手順] - 各入力を指定して cplat_parse_uint() を呼び出す。
    }

    // Assert
    for (size_t i = 0; i < cases.size(); i++)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(CPLAT_OK, actual_ret[i]);      // [確認_正常系 回数=10] - 変換が成功すること。
        EXPECT_EQ(cases[i].expected, values[i]); // [確認_正常系 回数=10] - 出力が期待値と一致すること。
    }
}

// cplat_parse_uint が不正入力を拒否し出力を保持することの確認
TEST(basicTypeParseTest, uint_invalid_values_preserve_output)
{
    // Arrange
    struct invalid_case
    {
        const char *text;
        int base;
        int expected;
    };
    const std::array<invalid_case, 15> cases = {{
        {"", 10, CPLAT_ERR_INVALID_INTEGER},
        {" ", 10, CPLAT_ERR_INVALID_INTEGER},
        {"abc", 10, CPLAT_ERR_INVALID_INTEGER},
        {"12x", 10, CPLAT_ERR_INVALID_INTEGER},
        {"1 ", 10, CPLAT_ERR_INVALID_INTEGER},
        {"-1", 10, CPLAT_ERR_OUT_OF_RANGE},
        {"4294967296", 10, CPLAT_ERR_OUT_OF_RANGE},
        {nullptr, 10, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", -1, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", 1, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", 37, CPLAT_ERR_INVALID_ARGUMENT},
        {"-x", 1, CPLAT_ERR_INVALID_ARGUMENT},
        {"-0", 10, CPLAT_ERR_OUT_OF_RANGE},
        {" \t-1", 10, CPLAT_ERR_OUT_OF_RANGE},
        {"-x", 10, CPLAT_ERR_OUT_OF_RANGE},
    }}; // [状態] - 範囲外、不正文字列、nullptr、無効基数を用意する。
    std::array<unsigned int, 15> values;
    values.fill(static_cast<unsigned int>(42)); // [状態] - 出力を 42 で初期化する。
    std::array<int, 15> actual_ret = {};

    // Pre-Assert

    // Act
    for (size_t i = 0; i < cases.size(); i++)
    {
        actual_ret[i] = cplat_parse_uint(&values[i], cases[i].text,
                                         cases[i].base); // [手順] - 不正入力を指定して cplat_parse_uint() を呼び出す。
    }

    // Assert
    for (size_t i = 0; i < cases.size(); i++)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(cases[i].expected, actual_ret[i]);         // [確認_異常系 回数=15] - 対応するエラーを返すこと。
        EXPECT_EQ(static_cast<unsigned int>(42), values[i]); // [確認_異常系 回数=15] - 出力が変更されないこと。
    }
}

// cplat_parse_uint が NULL 出力先を拒否することの確認
TEST(basicTypeParseTest, uint_null_output)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_parse_uint(nullptr, "42", 10); // [手順] - 出力先に nullptr を指定して cplat_parse_uint() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret); // [確認_異常系] - 無効引数エラーを返すこと。
}

// cplat_parse_longlong が有効な表記と基本型の境界値を変換することの確認
TEST(basicTypeParseTest, longlong_valid_values)
{
    // Arrange
    struct valid_case
    {
        const char *text;
        int base;
        long long expected;
    };
    const std::array<valid_case, 11> cases = {{
        {"0", 10, static_cast<long long>(0)},
        {"-9223372036854775808", 10, std::numeric_limits<long long>::min()},
        {"9223372036854775807", 10, std::numeric_limits<long long>::max()},
        {"+7", 10, static_cast<long long>(7)},
        {" \t7", 10, static_cast<long long>(7)},
        {"7f", 16, static_cast<long long>(127)},
        {"101", 2, static_cast<long long>(5)},
        {"z", 36, static_cast<long long>(35)},
        {"0x10", 0, static_cast<long long>(16)},
        {"077", 0, static_cast<long long>(63)},
        {"-0", 10, static_cast<long long>(0)},
    }}; // [状態] - 表記、基数、基本型の境界値を用意する。
    std::array<long long, 11> values = {};
    std::array<int, 11> actual_ret = {};

    // Pre-Assert

    // Act
    for (size_t i = 0; i < cases.size(); i++)
    {
        actual_ret[i] = cplat_parse_longlong(
            &values[i], cases[i].text, cases[i].base); // [手順] - 各入力を指定して cplat_parse_longlong() を呼び出す。
    }

    // Assert
    for (size_t i = 0; i < cases.size(); i++)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(CPLAT_OK, actual_ret[i]);      // [確認_正常系 回数=11] - 変換が成功すること。
        EXPECT_EQ(cases[i].expected, values[i]); // [確認_正常系 回数=11] - 出力が期待値と一致すること。
    }
}

// cplat_parse_longlong が不正入力を拒否し出力を保持することの確認
TEST(basicTypeParseTest, longlong_invalid_values_preserve_output)
{
    // Arrange
    struct invalid_case
    {
        const char *text;
        int base;
        int expected;
    };
    const std::array<invalid_case, 12> cases = {{
        {"", 10, CPLAT_ERR_INVALID_INTEGER},
        {" ", 10, CPLAT_ERR_INVALID_INTEGER},
        {"abc", 10, CPLAT_ERR_INVALID_INTEGER},
        {"12x", 10, CPLAT_ERR_INVALID_INTEGER},
        {"1 ", 10, CPLAT_ERR_INVALID_INTEGER},
        {"-9223372036854775809", 10, CPLAT_ERR_OUT_OF_RANGE},
        {"9223372036854775808", 10, CPLAT_ERR_OUT_OF_RANGE},
        {nullptr, 10, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", -1, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", 1, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", 37, CPLAT_ERR_INVALID_ARGUMENT},
        {"-x", 1, CPLAT_ERR_INVALID_ARGUMENT},
    }}; // [状態] - 範囲外、不正文字列、nullptr、無効基数を用意する。
    std::array<long long, 12> values;
    values.fill(static_cast<long long>(42)); // [状態] - 出力を 42 で初期化する。
    std::array<int, 12> actual_ret = {};

    // Pre-Assert

    // Act
    for (size_t i = 0; i < cases.size(); i++)
    {
        actual_ret[i] =
            cplat_parse_longlong(&values[i], cases[i].text,
                                 cases[i].base); // [手順] - 不正入力を指定して cplat_parse_longlong() を呼び出す。
    }

    // Assert
    for (size_t i = 0; i < cases.size(); i++)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(cases[i].expected, actual_ret[i]);      // [確認_異常系 回数=12] - 対応するエラーを返すこと。
        EXPECT_EQ(static_cast<long long>(42), values[i]); // [確認_異常系 回数=12] - 出力が変更されないこと。
    }
}

// cplat_parse_longlong が NULL 出力先を拒否することの確認
TEST(basicTypeParseTest, longlong_null_output)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret = cplat_parse_longlong(
        nullptr, "42", 10); // [手順] - 出力先に nullptr を指定して cplat_parse_longlong() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret); // [確認_異常系] - 無効引数エラーを返すこと。
}

// cplat_parse_ulonglong が有効な表記と基本型の境界値を変換することの確認
TEST(basicTypeParseTest, ulonglong_valid_values)
{
    // Arrange
    struct valid_case
    {
        const char *text;
        int base;
        unsigned long long expected;
    };
    const std::array<valid_case, 10> cases = {{
        {"0", 10, static_cast<unsigned long long>(0)},
        {"0", 10, std::numeric_limits<unsigned long long>::min()},
        {"18446744073709551615", 10, std::numeric_limits<unsigned long long>::max()},
        {"+7", 10, static_cast<unsigned long long>(7)},
        {" \t7", 10, static_cast<unsigned long long>(7)},
        {"7f", 16, static_cast<unsigned long long>(127)},
        {"101", 2, static_cast<unsigned long long>(5)},
        {"z", 36, static_cast<unsigned long long>(35)},
        {"0x10", 0, static_cast<unsigned long long>(16)},
        {"077", 0, static_cast<unsigned long long>(63)},
    }}; // [状態] - 表記、基数、基本型の境界値を用意する。
    std::array<unsigned long long, 10> values = {};
    std::array<int, 10> actual_ret = {};

    // Pre-Assert

    // Act
    for (size_t i = 0; i < cases.size(); i++)
    {
        actual_ret[i] = cplat_parse_ulonglong(
            &values[i], cases[i].text, cases[i].base); // [手順] - 各入力を指定して cplat_parse_ulonglong() を呼び出す。
    }

    // Assert
    for (size_t i = 0; i < cases.size(); i++)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(CPLAT_OK, actual_ret[i]);      // [確認_正常系 回数=10] - 変換が成功すること。
        EXPECT_EQ(cases[i].expected, values[i]); // [確認_正常系 回数=10] - 出力が期待値と一致すること。
    }
}

// cplat_parse_ulonglong が不正入力を拒否し出力を保持することの確認
TEST(basicTypeParseTest, ulonglong_invalid_values_preserve_output)
{
    // Arrange
    struct invalid_case
    {
        const char *text;
        int base;
        int expected;
    };
    const std::array<invalid_case, 15> cases = {{
        {"", 10, CPLAT_ERR_INVALID_INTEGER},
        {" ", 10, CPLAT_ERR_INVALID_INTEGER},
        {"abc", 10, CPLAT_ERR_INVALID_INTEGER},
        {"12x", 10, CPLAT_ERR_INVALID_INTEGER},
        {"1 ", 10, CPLAT_ERR_INVALID_INTEGER},
        {"-1", 10, CPLAT_ERR_OUT_OF_RANGE},
        {"18446744073709551616", 10, CPLAT_ERR_OUT_OF_RANGE},
        {nullptr, 10, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", -1, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", 1, CPLAT_ERR_INVALID_ARGUMENT},
        {"1", 37, CPLAT_ERR_INVALID_ARGUMENT},
        {"-x", 1, CPLAT_ERR_INVALID_ARGUMENT},
        {"-0", 10, CPLAT_ERR_OUT_OF_RANGE},
        {" \t-1", 10, CPLAT_ERR_OUT_OF_RANGE},
        {"-x", 10, CPLAT_ERR_OUT_OF_RANGE},
    }}; // [状態] - 範囲外、不正文字列、nullptr、無効基数を用意する。
    std::array<unsigned long long, 15> values;
    values.fill(static_cast<unsigned long long>(42)); // [状態] - 出力を 42 で初期化する。
    std::array<int, 15> actual_ret = {};

    // Pre-Assert

    // Act
    for (size_t i = 0; i < cases.size(); i++)
    {
        actual_ret[i] =
            cplat_parse_ulonglong(&values[i], cases[i].text,
                                  cases[i].base); // [手順] - 不正入力を指定して cplat_parse_ulonglong() を呼び出す。
    }

    // Assert
    for (size_t i = 0; i < cases.size(); i++)
    {
        SCOPED_TRACE(i);
        EXPECT_EQ(cases[i].expected, actual_ret[i]);               // [確認_異常系 回数=15] - 対応するエラーを返すこと。
        EXPECT_EQ(static_cast<unsigned long long>(42), values[i]); // [確認_異常系 回数=15] - 出力が変更されないこと。
    }
}

// cplat_parse_ulonglong が NULL 出力先を拒否することの確認
TEST(basicTypeParseTest, ulonglong_null_output)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret = cplat_parse_ulonglong(
        nullptr, "42", 10); // [手順] - 出力先に nullptr を指定して cplat_parse_ulonglong() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret); // [確認_異常系] - 無効引数エラーを返すこと。
}
