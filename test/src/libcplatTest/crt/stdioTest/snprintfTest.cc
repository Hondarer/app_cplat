#include <testfw.h>
#include <cplat/crt/stdio.h>
#include <cstdarg>

static int call_cplat_vsnprintf(char *dest, size_t dest_size, const char *format, ...)
{
    va_list args;
    int actual_ret;

    va_start(args, format);
    actual_ret = cplat_vsnprintf(dest, dest_size, format, args);
    va_end(args);
    return actual_ret;
}

class snprintfTest : public Test
{
};

// 書式化結果がバッファーへ書き込まれることの確認
TEST_F(snprintfTest, formats_into_buffer)
{
    // Arrange
    char buf[32]; // [状態] - 32 バイトの書き込み先バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_snprintf(buf, sizeof(buf), "%s=%d", "count",
                                    7); // [手順] - "%s=%d" に "count" と 7 を与えて cplat_snprintf を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_snprintf の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("count=7", buf);    // [確認_正常系] - バッファーに "count=7" が書き込まれること。
}

// 出力がバッファーにちょうど収まる場合に成功することの確認
TEST_F(snprintfTest, exact_fit_succeeds)
{
    // Arrange
    char buf[4]; // [状態] - 終端を含めて "abc" がちょうど収まる 4 バイトのバッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_snprintf(buf, sizeof(buf), "abc"); // [手順] - 3 文字の "abc" を書式として渡す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_snprintf の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("abc", buf);        // [確認_正常系] - バッファーに "abc" が書き込まれること。
}

// 1 バイト不足の場合にバッファー不足を返しバッファーが空になることの確認
TEST_F(snprintfTest, truncation_clears_buffer)
{
    // Arrange
    char buf[4] = "zzz"; // [状態] - 4 バイトのバッファーを "zzz" で初期化する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_snprintf(buf, sizeof(buf), "abcd"); // [手順] - 終端を含めて 5 バイトを要する "abcd" を渡す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              actual_ret); // [確認_異常系] - cplat_snprintf の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_STREQ("", buf); // [確認_異常系] - 切り詰めた結果を残さずバッファーが空文字列になること。
}

// 書き込み先が NULL の場合に引数エラーになることの確認
TEST_F(snprintfTest, null_dest_returns_invalid_argument)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_snprintf(NULL, 16u, "abc"); // [手順] - 書き込み先に NULL を渡して cplat_snprintf を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_snprintf の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// バッファー サイズが 0 の場合に引数エラーになることの確認
TEST_F(snprintfTest, zero_dest_size_returns_invalid_argument)
{
    // Arrange
    char buf[4]; // [状態] - 4 バイトのバッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_snprintf(buf, 0u, "abc"); // [手順] - バッファー サイズに 0 を渡して cplat_snprintf を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_snprintf の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// 書式が NULL の場合に引数エラーになることの確認
TEST_F(snprintfTest, null_format_returns_invalid_argument)
{
    // Arrange
    char buf[16]; // [状態] - 16 バイトのバッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_snprintf(buf, sizeof(buf), NULL); // [手順] - 書式に NULL を渡して cplat_snprintf を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_snprintf の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// va_list 版でも書式化結果が書き込まれることの確認
TEST_F(snprintfTest, vsnprintf_formats_into_buffer)
{
    // Arrange
    char buf[32]; // [状態] - 32 バイトの書き込み先バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = call_cplat_vsnprintf(buf, sizeof(buf), "%s-%s", "a",
                                          "b"); // [手順] - va_list 経由で "%s-%s" に "a" と "b" を与えて呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_vsnprintf の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("a-b", buf);        // [確認_正常系] - バッファーに "a-b" が書き込まれること。
}

// va_list 版でも切り詰め時にバッファーが空になることの確認
TEST_F(snprintfTest, vsnprintf_truncation_clears_buffer)
{
    // Arrange
    char buf[3] = "yy"; // [状態] - 3 バイトのバッファーを "yy" で初期化する。

    // Pre-Assert

    // Act
    int actual_ret = call_cplat_vsnprintf(buf, sizeof(buf), "%s",
                                          "abcd"); // [手順] - va_list 経由で収まらない "abcd" を書き込ませる。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              actual_ret); // [確認_異常系] - cplat_vsnprintf の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_STREQ("", buf); // [確認_異常系] - 切り詰めた結果を残さずバッファーが空文字列になること。
}
