#include <testfw.h>
#include <mock_stdio.h>
#include <cplat/crt/stdio.h>

#include <cstdarg>
#include <cstring>

using testing::_;
using testing::NiceMock;
using testing::StrEq;

namespace
{
FILE *const kStream = reinterpret_cast<FILE *>(static_cast<uintptr_t>(0x70));

int fill_token_and_int(va_list args, const char *token, int value)
{
    char *token_out = va_arg(args, char *);
    int *value_out = va_arg(args, int *);

    std::memcpy(token_out, token, std::strlen(token) + 1U);
    *value_out = value;
    return 2;
}

int call_cplat_vscanf(const char *format, ...)
{
    va_list args;
    int result;

    va_start(args, format);
    result = cplat_vscanf(format, args);
    va_end(args);
    return result;
}

int call_cplat_vfscanf(FILE *stream, const char *format, ...)
{
    va_list args;
    int result;

    va_start(args, format);
    result = cplat_vfscanf(stream, format, args);
    va_end(args);
    return result;
}
} // namespace

class scanfTest : public testing::Test
{
  protected:
    NiceMock<Mock_stdio> mock_stdio;
};

// scanf が標準入力から幅指定した文字列と数値を読み取ることの確認
TEST_F(scanfTest, scanf_reads_width_limited_token_and_integer)
{
    // Arrange
    char token[8] = {};
    int value = 0; // [状態] - 文字列と数値の格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_stdio, vscanf(_, _, _, StrEq("%7s %d"), _))
        .WillOnce(
            [](const char *, int, const char *, const char *, va_list args)
            {
                return fill_token_and_int(args, "alpha", 42);
            }); // [Pre-Assert確認_正常系] - vscanf が "%7s %d" で 1 回呼び出されること。
                // [Pre-Assert手順] - 文字列に "alpha"、数値に 42 を格納し、2 を返却する。

    // Act
    int count = cplat_scanf("%7s %d", token, &value); // [手順] - 幅 7 の文字列と整数を cplat_scanf で読み取る。

    // Assert
    EXPECT_EQ(2, count);          // [確認_正常系] - cplat_scanf の戻り値が 2 であること。
    EXPECT_STREQ("alpha", token); // [確認_正常系] - 文字列が "alpha" であること。
    EXPECT_EQ(42, value);         // [確認_正常系] - 数値が 42 であること。
}

// vscanf が標準入力から幅指定した文字列と数値を読み取ることの確認
TEST_F(scanfTest, vscanf_reads_width_limited_token_and_integer)
{
    // Arrange
    char token[8] = {};
    int value = 0; // [状態] - 文字列と数値の格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_stdio, vscanf(_, _, _, StrEq("%7s %d"), _))
        .WillOnce(
            [](const char *, int, const char *, const char *, va_list args)
            {
                return fill_token_and_int(args, "bravo", 24);
            }); // [Pre-Assert確認_正常系] - vscanf が "%7s %d" で 1 回呼び出されること。
                // [Pre-Assert手順] - 文字列に "bravo"、数値に 24 を格納し、2 を返却する。

    // Act
    int count = call_cplat_vscanf("%7s %d", token, &value); // [手順] - 幅 7 の文字列と整数を cplat_vscanf で読み取る。

    // Assert
    EXPECT_EQ(2, count);          // [確認_正常系] - cplat_vscanf の戻り値が 2 であること。
    EXPECT_STREQ("bravo", token); // [確認_正常系] - 文字列が "bravo" であること。
    EXPECT_EQ(24, value);         // [確認_正常系] - 数値が 24 であること。
}

// fscanf がストリームから幅指定した文字列と数値を読み取ることの確認
TEST_F(scanfTest, fscanf_reads_width_limited_token_and_integer)
{
    // Arrange
    char token[8] = {};
    int value = 0; // [状態] - 番兵ストリームと、文字列と数値の格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_stdio, vfscanf(_, _, _, kStream, StrEq("%7s %d"), _))
        .WillOnce(
            [](const char *, int, const char *, FILE *, const char *, va_list args)
            {
                return fill_token_and_int(args, "charlie", 17);
            }); // [Pre-Assert確認_正常系] - vfscanf が番兵ストリームと "%7s %d" で 1 回呼び出されること。
                // [Pre-Assert手順] - 文字列に "charlie"、数値に 17 を格納し、2 を返却する。

    // Act
    int count = cplat_fscanf(kStream, "%7s %d", token,
                             &value); // [手順] - 幅 7 の文字列と整数を cplat_fscanf で読み取る。

    // Assert
    EXPECT_EQ(2, count);            // [確認_正常系] - cplat_fscanf の戻り値が 2 であること。
    EXPECT_STREQ("charlie", token); // [確認_正常系] - 文字列が "charlie" であること。
    EXPECT_EQ(17, value);           // [確認_正常系] - 数値が 17 であること。
}

// vfscanf がストリームから幅指定した文字列と数値を読み取ることの確認
TEST_F(scanfTest, vfscanf_reads_width_limited_token_and_integer)
{
    // Arrange
    char token[8] = {};
    int value = 0; // [状態] - 番兵ストリームと、文字列と数値の格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_stdio, vfscanf(_, _, _, kStream, StrEq("%7s %d"), _))
        .WillOnce(
            [](const char *, int, const char *, FILE *, const char *, va_list args)
            {
                return fill_token_and_int(args, "delta", 71);
            }); // [Pre-Assert確認_正常系] - vfscanf が番兵ストリームと "%7s %d" で 1 回呼び出されること。
                // [Pre-Assert手順] - 文字列に "delta"、数値に 71 を格納し、2 を返却する。

    // Act
    int count = call_cplat_vfscanf(kStream, "%7s %d", token,
                                   &value); // [手順] - 幅 7 の文字列と整数を cplat_vfscanf で読み取る。

    // Assert
    EXPECT_EQ(2, count);          // [確認_正常系] - cplat_vfscanf の戻り値が 2 であること。
    EXPECT_STREQ("delta", token); // [確認_正常系] - 文字列が "delta" であること。
    EXPECT_EQ(71, value);         // [確認_正常系] - 数値が 71 であること。
}
