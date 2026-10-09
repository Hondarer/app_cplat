#include <testfw.h>
#include <mock_stdio.h>
#include <cplat/crt/stdio.h>
#include <cerrno>
#include <cstring>

using testing::_;
using testing::NiceMock;
using testing::Return;

namespace
{
FILE *const kStream = reinterpret_cast<FILE *>(static_cast<uintptr_t>(0x70));

char *copy_fgets(char *dest, int n, const char *src)
{
    size_t max = static_cast<size_t>(n - 1);
    size_t len = strlen(src);

    if (len > max)
    {
        len = max;
    }
    memcpy(dest, src, len);
    dest[len] = '\0';
    return dest;
}
} // namespace

class fgetsTest : public testing::Test
{
  protected:
    NiceMock<Mock_stdio> mock_stdio;

    // [サブ手順 名前=fgetsTest.SetUp]
    void SetUp() override
    {
        ON_CALL(mock_stdio, feof(_, _, _, _)).WillByDefault(Return(0));
        // [状態] - `feof` の既定動作を設定する。
        ON_CALL(mock_stdio, ferror(_, _, _, _)).WillByDefault(Return(0));
        // [状態] - `ferror` の既定動作を設定する。
    }
    // [サブ手順終了]
};

// LF で終わる行が改行を除去して取得されることの確認
// [サブ手順参照 名前=fgetsTest.SetUp]
TEST_F(fgetsTest, reads_line_terminated_by_lf)
{
    // Arrange
    char buf[16]; // [状態] - 16 バイトの格納先バッファーを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_stdio, fgets(_, _, _, buf, static_cast<int>(sizeof(buf)), kStream))
        .WillOnce(
            [](const char *, int, const char *, char *dest, int n, FILE *)
            { return copy_fgets(dest, n, "abc\n"); }); // [Pre-Assert確認_正常系] - fgets が 1 回呼び出されること。
                                                       // [Pre-Assert手順] - "abc\n" を格納して dest を返却する。

    // Act
    int actual_ret = cplat_fgets(buf, sizeof(buf), kStream, NULL); // [手順] - 1 行目を cplat_fgets で読み取る。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_fgets の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("abc", buf);    // [確認_正常系] - 末尾の LF を除いた "abc" が格納されること。
}

// CRLF で終わる行が CR と LF の双方を除去して取得されることの確認
// [サブ手順参照 名前=fgetsTest.SetUp]
TEST_F(fgetsTest, strips_crlf)
{
    // Arrange
    char buf[16]; // [状態] - 16 バイトの格納先バッファーを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_stdio, fgets(_, _, _, buf, static_cast<int>(sizeof(buf)), kStream))
        .WillOnce(
            [](const char *, int, const char *, char *dest, int n, FILE *)
            { return copy_fgets(dest, n, "abc\r\n"); }); // [Pre-Assert確認_正常系] - fgets が 1 回呼び出されること。
                                                         // [Pre-Assert手順] - "abc\r\n" を格納して dest を返却する。

    // Act
    int actual_ret = cplat_fgets(buf, sizeof(buf), kStream, NULL); // [手順] - CRLF で終わる行を cplat_fgets で読み取る。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_fgets の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("abc", buf);    // [確認_正常系] - CR と LF を除いた "abc" が格納されること。
}

// 改行で終わらない最終行が取得できることの確認
// [サブ手順参照 名前=fgetsTest.SetUp]
TEST_F(fgetsTest, reads_last_line_without_newline)
{
    // Arrange
    char buf[16]; // [状態] - 16 バイトの格納先バッファーを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_stdio, fgets(_, _, _, buf, static_cast<int>(sizeof(buf)), kStream))
        .WillOnce([](const char *, int, const char *, char *dest, int n, FILE *)
                  { return copy_fgets(dest, n, "abc"); }); // [Pre-Assert確認_正常系] - fgets が 1 回呼び出されること。
    // [Pre-Assert手順] - 改行なしの "abc" を格納して dest を返却する。

    // Act
    int actual_ret =
        cplat_fgets(buf, sizeof(buf), kStream, NULL); // [手順] - 改行のない最終行を cplat_fgets で読み取る。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_fgets の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("abc", buf);    // [確認_正常系] - "abc" が格納されること。
}

// 空行が空文字列として取得されることの確認
// [サブ手順参照 名前=fgetsTest.SetUp]
TEST_F(fgetsTest, reads_empty_line)
{
    // Arrange
    char buf[16]; // [状態] - 16 バイトの格納先バッファーを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_stdio, fgets(_, _, _, buf, static_cast<int>(sizeof(buf)), kStream))
        .WillOnce([](const char *, int, const char *, char *dest, int n, FILE *)
                  { return copy_fgets(dest, n, "\n"); }); // [Pre-Assert確認_正常系] - fgets が 1 回呼び出されること。
                                                          // [Pre-Assert手順] - "\n" を格納して dest を返却する。

    // Act
    int actual_ret =
        cplat_fgets(buf, sizeof(buf), kStream, NULL); // [手順] - 空行である 1 行目を cplat_fgets で読み取る。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_fgets の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("", buf);       // [確認_正常系] - 空文字列が格納されること。
}

// 読み取る行がない場合に EOF を返すことの確認
// [サブ手順参照 名前=fgetsTest.SetUp]
TEST_F(fgetsTest, returns_eof_at_end_of_stream)
{
    // Arrange
    char buf[16]; // [状態] - 16 バイトの格納先バッファーを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_stdio, fgets(_, _, _, buf, static_cast<int>(sizeof(buf)), kStream))
        .WillOnce([](const char *, int, const char *, char *dest, int n, FILE *)
                  { return copy_fgets(dest, n, "abc\n"); })
        .WillOnce(Return(static_cast<char *>(NULL))); // [Pre-Assert確認_正常系] - fgets が 2 回呼び出されること。
                                                      // [Pre-Assert手順] - 1 回目は "abc\n"、2 回目は NULL を返却する。

    // Act
    int first_ret = cplat_fgets(buf, sizeof(buf), kStream, NULL); // [手順] - 1 回目の呼び出しで唯一の行を読み取る。
    int second_ret =
        cplat_fgets(buf, sizeof(buf), kStream, NULL); // [手順] - 2 回目の呼び出しを行末尾の状態で実行する。

    // Assert
    EXPECT_EQ(CPLAT_OK, first_ret); // [確認_正常系] - 1 回目の cplat_fgets の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_ERR_EOF,
              second_ret); // [確認_正常系] - 2 回目の cplat_fgets の戻り値が CPLAT_ERR_EOF であること。
}

// 行がバッファーに収まらない場合にバッファー不足を返すことの確認
// [サブ手順参照 名前=fgetsTest.SetUp]
TEST_F(fgetsTest, returns_buffer_too_small_for_long_line)
{
    // Arrange
    char buf[4]; // [状態] - 4 バイトの格納先バッファーを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_stdio, fgets(_, _, _, buf, 4, kStream))
        .WillOnce(
            [](const char *, int, const char *, char *dest, int n, FILE *)
            {
                return copy_fgets(dest, n, "abcdefgh\n");
            }); // [Pre-Assert確認_異常系] - fgets がバッファー サイズ 4 で 1 回呼び出されること。
                // [Pre-Assert手順] - 先頭 3 文字を格納して dest を返却する。

    // Act
    int actual_ret = cplat_fgets(buf, sizeof(buf), kStream, NULL); // [手順] - 4 バイトのバッファーで 8 文字の行を読み取る。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              actual_ret);        // [確認_異常系] - cplat_fgets の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_STREQ("", buf); // [確認_異常系] - 途中までの内容を残さずバッファーが空文字列になること。
}

// バッファーに収まらない行の残りが次の呼び出しで取得されることの確認
// [サブ手順参照 名前=fgetsTest.SetUp]
TEST_F(fgetsTest, continues_reading_remainder_after_buffer_too_small)
{
    // Arrange
    char buf[5]; // [状態] - 5 バイトの格納先バッファーを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_stdio, fgets(_, _, _, buf, 5, kStream))
        .WillOnce([](const char *, int, const char *, char *dest, int n, FILE *)
                  { return copy_fgets(dest, n, "abcdef\n"); })
        .WillOnce([](const char *, int, const char *, char *dest, int n, FILE *)
                  { return copy_fgets(dest, n, "ef\n"); }); // [Pre-Assert確認_正常系] - fgets が 2 回呼び出されること。
    // [Pre-Assert手順] - 1 回目は先頭 4 文字、2 回目は残り "ef\n" を返却する。

    // Act
    int first_ret =
        cplat_fgets(buf, sizeof(buf), kStream, NULL); // [手順] - 1 回目の呼び出しで先頭 4 文字分を読み取らせる。
    int second_ret = cplat_fgets(buf, sizeof(buf), kStream, NULL); // [手順] - 2 回目の呼び出しで行の残りを読み取る。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_BUFFER_TOO_SMALL,
        first_ret); // [確認_異常系] - 1 回目の cplat_fgets の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_EQ(CPLAT_OK,
              second_ret);   // [確認_正常系] - 2 回目の cplat_fgets の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("ef", buf); // [確認_正常系] - 2 回目の呼び出しで行の残り "ef" が格納されること。
}

// バッファー サイズが 1 の場合にバッファー不足を返すことの確認
// [サブ手順参照 名前=fgetsTest.SetUp]
TEST_F(fgetsTest, buffer_size_one_returns_buffer_too_small)
{
    // Arrange
    char buf[1]; // [状態] - 終端の 1 バイトしか置けないバッファーを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_stdio, fgets(_, _, _, buf, 1, kStream))
        .WillOnce(
            [](const char *, int, const char *, char *dest, int n, FILE *)
            {
                return copy_fgets(dest, n, "abc\n");
            }); // [Pre-Assert確認_異常系] - fgets がバッファー サイズ 1 で 1 回呼び出されること。
                // [Pre-Assert手順] - 空文字列を格納して dest を返却する。

    // Act
    int actual_ret =
        cplat_fgets(buf, sizeof(buf), kStream, NULL); // [手順] - 終端の 1 バイトしか置けないバッファーで読み取る。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              actual_ret);        // [確認_異常系] - cplat_fgets の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_STREQ("", buf); // [確認_異常系] - バッファーが空文字列になること。
}

// 格納先が NULL の場合に引数エラーになることの確認
// [サブ手順参照 名前=fgetsTest.SetUp]
TEST_F(fgetsTest, null_dest_returns_invalid_argument)
{
    // Arrange
    cplat_error detail = {}; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_fgets(NULL, 16u, kStream, &detail); // [手順] - 格納先に NULL を渡して cplat_fgets を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_fgets の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(EINVAL, cplat_error_get_errno(&detail)); // [確認_異常系] - 詳細エラーへ EINVAL が記録されること。
}

// ストリームが NULL の場合に引数エラーになることの確認
// [サブ手順参照 名前=fgetsTest.SetUp]
TEST_F(fgetsTest, null_stream_returns_invalid_argument)
{
    // Arrange
    char buf[16]; // [状態] - 16 バイトの格納先バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_fgets(buf, sizeof(buf), NULL, NULL); // [手順] - ストリームに NULL を渡して cplat_fgets を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_fgets の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// 成功時に詳細エラーがクリアされることの確認
// [サブ手順参照 名前=fgetsTest.SetUp]
TEST_F(fgetsTest, clears_detail_on_success)
{
    // Arrange
    cplat_error detail = {CPLAT_ERROR_DOMAIN_ERRNO, CPLAT_ERR_NOT_FOUND,
                             ENOENT}; // [状態] - 詳細エラーへあらかじめ ENOENT を設定する。
    char buf[16];
    ASSERT_NE(0, cplat_error_is_set(&detail)); // [状態確認] - 呼び出し前の詳細エラーが設定済みであること。

    // Pre-Assert
    EXPECT_CALL(mock_stdio, fgets(_, _, _, buf, static_cast<int>(sizeof(buf)), kStream))
        .WillOnce(
            [](const char *, int, const char *, char *dest, int n, FILE *)
            { return copy_fgets(dest, n, "abc\n"); }); // [Pre-Assert確認_正常系] - fgets が 1 回呼び出されること。
                                                       // [Pre-Assert手順] - "abc\n" を格納して dest を返却する。

    // Act
    int actual_ret = cplat_fgets(buf, sizeof(buf), kStream, &detail); // [手順] - 詳細エラー出力を指定して 1 行を読み取る。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret);                  // [確認_正常系] - cplat_fgets の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, cplat_error_is_set(&detail)); // [確認_正常系] - 成功時に詳細エラーがクリアされること。
}
