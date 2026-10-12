#include <testfw.h>
#include <cplat/crt/string.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static int call_cplat_vsscanf(const char *buffer, const char *format, ...)
{
    va_list args;
    int result;

    va_start(args, format);
    result = cplat_vsscanf(buffer, format, args);
    va_end(args);
    return result;
}

class stringTest : public Test
{
};

// 文字列がコピーされることの確認
TEST_F(stringTest, strcpy_success)
{
    // Arrange
    char buf[16]; // [状態] - 16 バイトのコピー先バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_strcpy(buf, sizeof(buf), "abc"); // [手順] - "abc" を指定して cplat_strcpy を呼び出す。

    // Assert
    EXPECT_EQ(0, actual_ret); // [確認_正常系] - cplat_strcpy() の戻り値が 0 であること。
    EXPECT_STREQ("abc", buf); // [確認_正常系] - buf が "abc" であること。
}

// コピー元が NULL の場合に EINVAL を返すことの確認
TEST_F(stringTest, strcpy_null_argument)
{
    // Arrange
    char buf[16]; // [状態] - 16 バイトのコピー先バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_strcpy(buf, sizeof(buf), NULL); // [手順] - コピー元に NULL を渡して cplat_strcpy を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_strcpy() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// コピー先が NULL またはサイズ 0 の場合に引数エラーになることの確認
TEST_F(stringTest, strcpy_rejects_null_destination_and_zero_size)
{
    // Arrange
    char buf[4] = "old"; // [状態] - 内容を保持した 4 バイトのバッファーを用意する。

    // Pre-Assert

    // Act
    int null_destination_result = cplat_strcpy(NULL, sizeof(buf), "new"); // [手順] - コピー先に NULL を渡す。
    int zero_size_result = cplat_strcpy(buf, 0, "new");                   // [手順] - コピー先サイズに 0 を渡す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_destination_result); // [確認_異常系] - コピー先が NULL の cplat_strcpy() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        zero_size_result); // [確認_異常系] - コピー先サイズが 0 の cplat_strcpy() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_STREQ(
        "old", buf); // [確認_異常系] - buf が "old" であること。(サイズ 0 の呼び出しでコピー先が変更されないことの確認)
}

// バッファー不足の場合に ERANGE を返しバッファーが空になることの確認
TEST_F(stringTest, strcpy_buffer_shortage)
{
    // Arrange
    char buf[3] = "xx"; // [状態] - コピー元より小さい 3 バイトのバッファーを "xx" で初期化する。

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_strcpy(buf, sizeof(buf), "abcd"); // [手順] - 4 文字の "abcd" を指定して cplat_strcpy を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              actual_ret); // [確認_異常系] - cplat_strcpy() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_STREQ("", buf); // [確認_異常系] - buf が "" であること。(バッファーが空文字列にクリアされることの確認)
}

// count 指定で文字列が切り詰めてコピーされることの確認
TEST_F(stringTest, strncpy_truncates_to_count)
{
    // Arrange
    char buf[8]; // [状態] - 8 バイトのコピー先バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_strncpy(buf, sizeof(buf), "abcdef",
                                   3); // [手順] - "abcdef" と count=3 を指定して cplat_strncpy を呼び出す。

    // Assert
    EXPECT_EQ(0, actual_ret); // [確認_正常系] - cplat_strncpy() の戻り値が 0 であること。
    EXPECT_STREQ("abc", buf); // [確認_正常系] - buf が "abc" であること。(先頭 3 文字だけがコピーされることの確認)
}

// コピー先容量と NULL 引数が cplat_strncpy で検証されることの確認
TEST_F(stringTest, strncpy_rejects_null_and_limits_destination)
{
    // Arrange
    char buf[4] = "old"; // [状態] - 4 バイトのコピー先を用意する。

    // Pre-Assert

    // Act
    int null_destination_result = cplat_strncpy(NULL, sizeof(buf), "abc", 3u); // [手順] - コピー先に NULL を渡す。
    int zero_size_result = cplat_strncpy(buf, 0u, "abc", 3u);                  // [手順] - コピー先サイズに 0 を渡す。
    int null_source_result = cplat_strncpy(buf, sizeof(buf), NULL, 3u);        // [手順] - コピー元に NULL を渡す。
    int limited_destination_result =
        cplat_strncpy(buf, sizeof(buf), "abcdef", 6u); // [手順] - コピー先容量を超える文字列を指定する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_destination_result); // [確認_異常系] - コピー先が NULL の cplat_strncpy() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        zero_size_result); // [確認_異常系] - コピー先サイズが 0 の cplat_strncpy() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_source_result); // [確認_異常系] - コピー元が NULL の cplat_strncpy() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_OK,
        limited_destination_result); // [確認_正常系] - コピー先容量に合わせて切り詰める cplat_strncpy() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("abc",
                 buf); // [確認_正常系] - buf が "abc" であること。(コピー先容量に合わせて "abc" が格納されることの確認)
}

// 文字列が連結されることの確認
TEST_F(stringTest, strcat_success)
{
    // Arrange
    char buf[16] = "abc"; // [状態] - 連結先バッファーを "abc" で初期化する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_strcat(buf, sizeof(buf), "def"); // [手順] - "def" を指定して cplat_strcat を呼び出す。

    // Assert
    EXPECT_EQ(0, actual_ret);    // [確認_正常系] - cplat_strcat() の戻り値が 0 であること。
    EXPECT_STREQ("abcdef", buf); // [確認_正常系] - buf が "abcdef" であること。
}

// count 指定で連結元が切り詰められることの確認
TEST_F(stringTest, strncat_truncates_to_count)
{
    // Arrange
    char buf[16] = "abc"; // [状態] - 連結先バッファーを "abc" で初期化する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_strncat(buf, sizeof(buf), "defgh", 2u); // [手順] - "defgh" の先頭 2 文字を連結する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_strncat() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("abcde",
                 buf); // [確認_正常系] - buf が "abcde" であること。(連結元の先頭 2 文字だけが連結されることの確認)
}

// count が連結元の長さを超える場合に全体が連結されることの確認
TEST_F(stringTest, strncat_copies_whole_source_when_count_exceeds_length)
{
    // Arrange
    char buf[16] = "abc"; // [状態] - 連結先バッファーを "abc" で初期化する。

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_strncat(buf, sizeof(buf), "de", 10u); // [手順] - 長さ 2 の "de" に count 10 を指定して連結する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_strncat() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "abcde",
        buf); // [確認_正常系] - buf が "abcde" であること。(count が長さを超えても連結元全体が連結されることの確認)
}

// 連結結果がちょうど収まる場合に成功することの確認
TEST_F(stringTest, strncat_exact_fit_succeeds)
{
    // Arrange
    char buf[6] = "abc"; // [状態] - 終端を含めて "abcde" がちょうど収まる 6 バイトのバッファーを "abc" で初期化する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_strncat(buf, sizeof(buf), "de", 2u); // [手順] - 残り容量ちょうどの "de" を連結する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_strncat() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("abcde", buf);      // [確認_正常系] - buf が "abcde" であること。(連結結果がちょうど収まることの確認)
}

// 連結結果が収まらない場合に連結先が変更されないことの確認
TEST_F(stringTest, strncat_buffer_shortage_keeps_dest)
{
    // Arrange
    char buf[6] = "abc"; // [状態] - 6 バイトのバッファーを "abc" で初期化する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_strncat(buf, sizeof(buf), "defg", 4u); // [手順] - 残り容量を超える 4 文字を連結させる。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              actual_ret);    // [確認_異常系] - cplat_strncat() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_STREQ("abc", buf); // [確認_異常系] - buf が "abc" であること。(連結先が "abc" のまま変更されないことの確認)
}

// 連結先が null 終端していない場合にバッファー不足を返すことの確認
TEST_F(stringTest, strncat_unterminated_dest_returns_buffer_too_small)
{
    // Arrange
    char buf[4] = {'a', 'b', 'c', 'd'}; // [状態] - null 終端を持たない 4 バイトのバッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_strncat(buf, sizeof(buf), "e", 1u); // [手順] - 非終端のバッファーへ "e" を連結させる。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              actual_ret); // [確認_異常系] - cplat_strncat() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_STREQ("", buf); // [確認_異常系] - buf が "" であること。(バッファーが空文字列にクリアされることの確認)
}

// 連結元が NULL の場合に引数エラーになることの確認
TEST_F(stringTest, strncat_null_argument)
{
    // Arrange
    char buf[16] = "abc"; // [状態] - 連結先バッファーを "abc" で初期化する。

    // Pre-Assert

    // Act
    int null_destination_result = cplat_strncat(NULL, sizeof(buf), "def", 1u); // [手順] - 連結先に NULL を渡す。
    int zero_size_result = cplat_strncat(buf, 0u, "def", 1u);                  // [手順] - 連結先サイズに 0 を渡す。
    int actual_ret =
        cplat_strncat(buf, sizeof(buf), NULL, 1u); // [手順] - 連結元に NULL を渡して cplat_strncat を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret); // [確認_異常系] - 連結元が NULL の cplat_strncat() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_destination_result); // [確認_異常系] - 連結先が NULL の cplat_strncat() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        zero_size_result); // [確認_異常系] - 連結先サイズが 0 の cplat_strncat() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// strcat が NULL 引数または終端のないコピー先を拒否することの確認
TEST_F(stringTest, strcat_rejects_invalid_arguments)
{
    // Arrange
    char terminated[8] = "abc";                  // [状態] - 終端済みのコピー先を用意する。
    char unterminated[4] = {'a', 'b', 'c', 'd'}; // [状態] - NULL 終端のないコピー先を用意する。

    // Pre-Assert

    // Act
    int null_destination_result = cplat_strcat(NULL, sizeof(terminated), "new"); // [手順] - コピー先に NULL を渡す。
    int zero_size_result = cplat_strcat(terminated, 0u, "new");                  // [手順] - コピー先サイズに 0 を渡す。
    int null_source_result = cplat_strcat(terminated, sizeof(terminated), NULL); // [手順] - コピー元に NULL を渡す。
    int unterminated_result =
        cplat_strcat(unterminated, sizeof(unterminated), "e"); // [手順] - 終端のないコピー先へ連結する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_source_result); // [確認_異常系] - コピー元が NULL の cplat_strcat() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_destination_result); // [確認_異常系] - コピー先が NULL の cplat_strcat() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        zero_size_result); // [確認_異常系] - コピー先サイズが 0 の cplat_strcat() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_BUFFER_TOO_SMALL,
        unterminated_result); // [確認_異常系] - 終端のないコピー先の cplat_strcat() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_EQ(
        '\0',
        unterminated
            [0]); // [確認_異常系] - unterminated[0] が '\0' であること。(終端のないコピー先が空文字列へクリアされることの確認)
}

// 連結結果が収まらない場合にコピー先を変更しないことの確認
TEST_F(stringTest, strcat_returns_buffer_too_small_when_result_does_not_fit)
{
    // Arrange
    char buf[6] = "abc"; // [状態] - 連結先を用意する。

    // Pre-Assert

    // Act
    int result = cplat_strcat(buf, sizeof(buf), "def"); // [手順] - 終端を含めて容量を超える文字列を連結する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_BUFFER_TOO_SMALL,
        result); // [確認_異常系] - 結果が収まらない cplat_strcat() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_STREQ("abc", buf); // [確認_異常系] - buf が "abc" であること。(容量不足時にコピー先が変更されないことの確認)
}

// 文字列がトークンへ分割されることの確認
TEST_F(stringTest, strtok_r_splits_tokens)
{
    // Arrange
    char text[] = "a,b,c"; // [状態] - カンマ区切りの文字列 "a,b,c" を用意する。
    char *saveptr = NULL;
    char *first;
    char *second;
    char *third;
    char *fourth;

    // Pre-Assert

    // Act
    first = cplat_strtok_r(text, ",", &saveptr);  // [手順] - 1 回目は対象文字列を指定して呼び出す。
    second = cplat_strtok_r(NULL, ",", &saveptr); // [手順] - 2 回目は NULL を指定して呼び出す。
    third = cplat_strtok_r(NULL, ",", &saveptr);  // [手順] - 3 回目は NULL を指定して呼び出す。
    fourth = cplat_strtok_r(NULL, ",", &saveptr); // [手順] - 4 回目は NULL を指定して呼び出す。

    // Assert
    EXPECT_STREQ("a", first);  // [確認_正常系] - 1 回目の cplat_strtok_r() の戻り値が "a" であること。
    EXPECT_STREQ("b", second); // [確認_正常系] - 2 回目の cplat_strtok_r() の戻り値が "b" であること。
    EXPECT_STREQ("c", third);  // [確認_正常系] - 3 回目の cplat_strtok_r() の戻り値が "c" であること。
    EXPECT_EQ(
        (char *)NULL,
        fourth); // [確認_正常系] - 4 回目の cplat_strtok_r() の戻り値が (char *)NULL であること。(これ以上トークンがないことの確認)
}

// 区切り文字が NULL の場合に NULL を返すことの確認
TEST_F(stringTest, strtok_r_null_delimiter_returns_null)
{
    // Arrange
    char text[] = "a,b"; // [状態] - カンマ区切りの文字列 "a,b" を用意する。
    char *saveptr = NULL;

    // Pre-Assert

    // Act
    char *token = cplat_strtok_r(text, NULL, &saveptr); // [手順] - 区切り文字に NULL を渡して呼び出す。

    // Assert
    EXPECT_EQ(
        (char *)NULL,
        token); // [確認_異常系] - 区切り文字が NULL の cplat_strtok_r() の戻り値が (char *)NULL であること。(トークンを返さないことの確認)
}

// saveptr が NULL の場合に NULL を返すことの確認
TEST_F(stringTest, strtok_r_null_saveptr_returns_null)
{
    // Arrange
    char text[] = "a,b"; // [状態] - カンマ区切りの文字列 "a,b" を用意する。

    // Pre-Assert

    // Act
    char *token = cplat_strtok_r(text, ",", NULL); // [手順] - saveptr に NULL を指定して呼び出す。

    // Assert
    EXPECT_EQ(
        (char *)NULL,
        token); // [確認_異常系] - saveptr が NULL の cplat_strtok_r() の戻り値が (char *)NULL であること。(トークンを返さないことの確認)
}

// 3 つのトークンが読み取られることの確認
TEST_F(stringTest, sscanf_reads_three_tokens)
{
    // Arrange
    char token1[8];
    char token2[8];
    char token3[8]; // [状態] - トークンの受け取り先バッファーを 3 つ用意する。

    // Pre-Assert

    // Act
    int count = cplat_sscanf("one two three", "%7s %7s %7s", token1, token2,
                             token3); // [手順] - 入力 "one two three" を %7s x 3 で解析する。

    // Assert
    EXPECT_EQ(3,
              count); // [確認_正常系] - cplat_sscanf() の戻り値が 3 であること。(3 つのトークンを読み取ったことの確認)
    EXPECT_STREQ("one", token1);   // [確認_正常系] - token1 が "one" であること。
    EXPECT_STREQ("two", token2);   // [確認_正常系] - token2 が "two" であること。
    EXPECT_STREQ("three", token3); // [確認_正常系] - token3 が "three" であること。
}

// 入力が不足している場合に読み取れた数だけが返ることの確認
TEST_F(stringTest, sscanf_returns_partial_count_for_short_input)
{
    // Arrange
    char token1[8];
    char token2[8];
    char token3[8]; // [状態] - トークンの受け取り先バッファーを 3 つ用意する。

    // Pre-Assert

    // Act
    int count = cplat_sscanf("one two", "%7s %7s %7s", token1, token2,
                             token3); // [手順] - トークンが 2 つの入力 "one two" を %7s x 3 で解析する。

    // Assert
    EXPECT_EQ(
        2,
        count); // [確認_正常系] - cplat_sscanf() の戻り値が 2 であること。(入力が不足し読み取れた数が 2 であることの確認)
}

// 連続する空白やタブが読み飛ばされることの確認
TEST_F(stringTest, sscanf_skips_repeated_spaces)
{
    // Arrange
    char token1[8];
    char token2[8];
    char token3[8]; // [状態] - トークンの受け取り先バッファーを 3 つ用意する。

    // Pre-Assert

    // Act
    int count = cplat_sscanf("  one   two\tthree", "%7s %7s %7s", token1, token2,
                             token3); // [手順] - 連続空白とタブを含む入力を %7s x 3 で解析する。

    // Assert
    EXPECT_EQ(
        3,
        count); // [確認_正常系] - cplat_sscanf() の戻り値が 3 であること。(連続する空白やタブを読み飛ばして 3 件読み取ったことの確認)
    EXPECT_STREQ("one", token1);   // [確認_正常系] - token1 が "one" であること。
    EXPECT_STREQ("two", token2);   // [確認_正常系] - token2 が "two" であること。
    EXPECT_STREQ("three", token3); // [確認_正常系] - token3 が "three" であること。
}

// 幅指定を超える入力が切り詰められることの確認
TEST_F(stringTest, sscanf_respects_width_limit)
{
    // Arrange
    char token[4]; // [状態] - 4 バイトのトークン受け取り先バッファーを用意する。

    // Pre-Assert

    // Act
    int count = cplat_sscanf("abcdef", "%3s", token); // [手順] - 6 文字の入力 "abcdef" を %3s で解析する。

    // Assert
    EXPECT_EQ(1, count); // [確認_正常系] - cplat_sscanf() の戻り値が 1 であること。(幅指定で 1 件読み取ったことの確認)
    EXPECT_STREQ("abc", token); // [確認_正常系] - token が "abc" であること。(幅 3 で切り詰められたことの確認)
}

// vsscanf 版でも幅指定した文字列が読み取られることの確認
TEST_F(stringTest, vsscanf_reads_width_limited_token)
{
    // Arrange
    char token[4]; // [状態] - 4 バイトのトークン受け取り先バッファーを用意する。

    // Pre-Assert

    // Act
    int count = call_cplat_vsscanf("abcdef", "%3s", token); // [手順] - "abcdef" を %3s で解析する。

    // Assert
    EXPECT_EQ(
        1, count); // [確認_正常系] - call_cplat_vsscanf() の戻り値が 1 であること。(幅指定で 1 件読み取ったことの確認)
    EXPECT_STREQ("abc", token); // [確認_正常系] - token が "abc" であること。(先頭 3 文字が読み取られることの確認)
}

// cplat_strdup が文字列を複製することの確認
TEST_F(stringTest, strdup_duplicates_string)
{
    // Arrange
    const char *src = "hello"; // [状態] - 複製元の文字列を "hello" とする。

    // Pre-Assert

    // Act
    char *dup = cplat_strdup(src); // [手順] - "hello" を複製する。

    // Assert
    ASSERT_NE(nullptr, dup);    // [確認_正常系] - cplat_strdup() の戻り値が nullptr 以外であること。
    EXPECT_STREQ("hello", dup); // [確認_正常系] - cplat_strdup() の戻り値が "hello" であること。
    EXPECT_NE(src, dup); // [確認_正常系] - cplat_strdup() の戻り値が src 以外であること。(複製領域が別であることの確認)

    // Cleanup
    free(dup);
}

// cplat_strdup が NULL に対して NULL を返すことの確認
TEST_F(stringTest, strdup_null_returns_null)
{
    // Arrange

    // Pre-Assert

    // Act
    char *dup = cplat_strdup(NULL); // [手順] - 複製元に NULL を指定して呼び出す。

    // Assert
    EXPECT_EQ(
        nullptr,
        dup); // [確認_異常系] - cplat_strdup() の戻り値が nullptr であること。(複製元が NULL のとき NULL を返すことの確認)
}

// ワイド文字列がコピーされることの確認
TEST_F(stringTest, wcscpy_success)
{
    // Arrange
    wchar_t buf[8]; // [状態] - 8 要素のワイド文字列コピー先を用意する。

    // Pre-Assert

    // Act
    int result =
        cplat_wcscpy(buf, sizeof(buf) / sizeof(buf[0]), L"abc"); // [手順] - L"abc" をワイド文字列としてコピーする。

    // Assert
    EXPECT_EQ(CPLAT_OK, result); // [確認_正常系] - cplat_wcscpy() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(L"abc", buf);   // [確認_正常系] - buf が L"abc" であること。
}

// ワイド文字列の NULL 引数と容量不足が拒否されることの確認
TEST_F(stringTest, wcscpy_rejects_invalid_arguments)
{
    // Arrange
    wchar_t buf[4] = L"old"; // [状態] - 内容を保持した 4 要素のワイド文字列バッファーを用意する。

    // Pre-Assert

    // Act
    int null_destination_result = cplat_wcscpy(NULL, 4u, L"abc"); // [手順] - ワイド文字列のコピー先に NULL を渡す。
    int zero_size_result = cplat_wcscpy(buf, 0u, L"abc");         // [手順] - ワイド文字列のコピー先サイズに 0 を渡す。
    int null_source_result = cplat_wcscpy(buf, 4, NULL);          // [手順] - ワイド文字列のコピー元に NULL を渡す。
    int short_buffer_result = cplat_wcscpy(buf, 4, L"abcd");      // [手順] - 終端を含めて容量を超える文字列を渡す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_source_result); // [確認_異常系] - コピー元が NULL の cplat_wcscpy() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_destination_result); // [確認_異常系] - コピー先が NULL の cplat_wcscpy() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        zero_size_result); // [確認_異常系] - コピー先サイズが 0 の cplat_wcscpy() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_BUFFER_TOO_SMALL,
        short_buffer_result); // [確認_異常系] - 容量不足の cplat_wcscpy() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_EQ(
        L'\0',
        buf[0]); // [確認_異常系] - buf[0] が L'\0' であること。(容量不足時にコピー先が空文字列へクリアされることの確認)
}

// ASCII の大文字小文字だけが異なる文字列を一致と判定することの確認
TEST_F(stringTest, strcasecmp_ignores_ascii_case)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret = cplat_strcasecmp("AbC", "aBc"); // [手順] - ASCII の大小だけが異なる文字列を比較する。

    // Assert
    EXPECT_EQ(
        0,
        actual_ret); // [確認_正常系] - cplat_strcasecmp() の戻り値が 0 であること。(ASCII の大文字小文字を無視して等しいことの確認)
}

// 大文字小文字を無視しても異なる文字列を順序付きで判定することの確認
TEST_F(stringTest, strcasecmp_orders_unequal_strings)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret_less = cplat_strcasecmp("abc", "abd");    // [手順] - 小さい文字列と比較する。
    int actual_ret_greater = cplat_strcasecmp("abd", "abc"); // [手順] - 大きい文字列と比較する。

    // Assert
    EXPECT_EQ(
        -1,
        actual_ret_less); // [確認_正常系] - 小さい側の cplat_strcasecmp() の戻り値が -1 であること。(左辺が右辺より小さいことの確認)
    EXPECT_EQ(
        1,
        actual_ret_greater); // [確認_正常系] - 大きい側の cplat_strcasecmp() の戻り値が 1 であること。(左辺が右辺より大きいことの確認)
}

// 一方が他方の先頭部分である文字列を、短い側が小さいと判定することの確認
TEST_F(stringTest, strcasecmp_orders_prefix_strings)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret_shorter = cplat_strcasecmp("abc", "ABCD"); // [手順] - 短い側を左辺にして比較する。
    int actual_ret_longer = cplat_strcasecmp("ABCD", "abc");  // [手順] - 長い側を左辺にして比較する。

    // Assert
    EXPECT_EQ(
        -1,
        actual_ret_shorter); // [確認_正常系] - 短い側を左辺にした cplat_strcasecmp() の戻り値が -1 であること。(短い側が小さいことの確認)
    EXPECT_EQ(
        1,
        actual_ret_longer); // [確認_正常系] - 長い側を左辺にした cplat_strcasecmp() の戻り値が 1 であること。(長い側が大きいことの確認)
}

// 空文字列どうしを一致と判定することの確認
TEST_F(stringTest, strcasecmp_empty_strings_are_equal)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret = cplat_strcasecmp("", ""); // [手順] - 空文字列どうしを比較する。

    // Assert
    EXPECT_EQ(
        0,
        actual_ret); // [確認_正常系] - cplat_strcasecmp() の戻り値が 0 であること。(空文字列どうしが等しいことの確認)
}

// 非 ASCII バイトは畳まず符号なしバイトとして比較することの確認
TEST_F(stringTest, strcasecmp_does_not_fold_non_ascii_bytes)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret = cplat_strcasecmp("\xC1", "\xE1"); // [手順] - ASCII 外の 2 バイトを比較する。

    // Assert
    EXPECT_EQ(
        -1,
        actual_ret); // [確認_正常系] - cplat_strcasecmp() の戻り値が -1 であること。(非 ASCII バイトを畳まず 0xC1 が 0xE1 より小さいことの確認)
}

// NULL 引数を全順序で扱うことの確認
TEST_F(stringTest, strcasecmp_orders_null_arguments)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret_both_null = cplat_strcasecmp(NULL, NULL); // [手順] - 両方 NULL で比較する。
    int actual_ret_lhs_null = cplat_strcasecmp(NULL, "a");   // [手順] - 左辺だけ NULL で比較する。
    int actual_ret_rhs_null = cplat_strcasecmp("a", NULL);   // [手順] - 右辺だけ NULL で比較する。

    // Assert
    EXPECT_EQ(
        0,
        actual_ret_both_null); // [確認_正常系] - 両方 NULL の cplat_strcasecmp() の戻り値が 0 であること。(NULL どうしを等しいと扱うことの確認)
    EXPECT_EQ(
        -1,
        actual_ret_lhs_null); // [確認_正常系] - 左辺だけ NULL の cplat_strcasecmp() の戻り値が -1 であること。(NULL を非 NULL より小さいと扱うことの確認)
    EXPECT_EQ(
        1,
        actual_ret_rhs_null); // [確認_正常系] - 右辺だけ NULL の cplat_strcasecmp() の戻り値が 1 であること。(非 NULL を NULL より大きいと扱うことの確認)
}

// 指定バイト数までの大小無視比較が一致することの確認
TEST_F(stringTest, strncasecmp_compares_limited_prefix)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret_prefix = cplat_strncasecmp("File", "file.txt", 4u); // [手順] - 先頭 4 バイトだけ比較する。
    int actual_ret_full = cplat_strncasecmp("File", "file.txt", 5u);   // [手順] - 5 バイト目まで比較する。

    // Assert
    EXPECT_EQ(
        0,
        actual_ret_prefix); // [確認_正常系] - 先頭 4 バイトの cplat_strncasecmp() の戻り値が 0 であること。(先頭 4 バイトが大文字小文字を無視して等しいことの確認)
    EXPECT_EQ(
        -1,
        actual_ret_full); // [確認_正常系] - 5 バイト目までの cplat_strncasecmp() の戻り値が -1 であること。(終端が '.' より小さいことの確認)
}

// 比較バイト数が 0 のとき一致と判定することの確認
TEST_F(stringTest, strncasecmp_zero_count_is_equal)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret = cplat_strncasecmp(NULL, "abc", 0u); // [手順] - count 0 で NULL と文字列を比較する。

    // Assert
    EXPECT_EQ(
        0,
        actual_ret); // [確認_正常系] - count が 0 の cplat_strncasecmp() の戻り値が 0 であること。(比較バイト数 0 を等しいと扱うことの確認)
}

// strncasecmp が NULL 引数を全順序で扱うことの確認
TEST_F(stringTest, strncasecmp_orders_null_arguments)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret_both_null = cplat_strncasecmp(NULL, NULL, 1u); // [手順] - count 1 で両方 NULL を比較する。
    int actual_ret_lhs_null = cplat_strncasecmp(NULL, "a", 1u);   // [手順] - count 1 で左辺だけ NULL を比較する。
    int actual_ret_rhs_null = cplat_strncasecmp("a", NULL, 1u);   // [手順] - count 1 で右辺だけ NULL を比較する。

    // Assert
    EXPECT_EQ(
        0,
        actual_ret_both_null); // [確認_正常系] - 両方 NULL の cplat_strncasecmp() の戻り値が 0 であること。(NULL どうしを等しいと扱うことの確認)
    EXPECT_EQ(
        -1,
        actual_ret_lhs_null); // [確認_正常系] - 左辺だけ NULL の cplat_strncasecmp() の戻り値が -1 であること。(NULL を非 NULL より小さいと扱うことの確認)
    EXPECT_EQ(
        1,
        actual_ret_rhs_null); // [確認_正常系] - 右辺だけ NULL の cplat_strncasecmp() の戻り値が 1 であること。(非 NULL を NULL より大きいと扱うことの確認)
}
