#include <testfw.h>
#include <mock_cplat.h>

#include <cplat/crt/path_format.h>

#include <errno.h>
#include <stdarg.h>

using namespace testing;

static int call_vformat_path(char *path, size_t path_size, int *error_out, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    int actual_ret = cplat_internal_vformat_path(path, path_size, format, args, error_out);
    va_end(args);

    return actual_ret;
}

// path が NULL の場合に引数不正を報告することの確認
TEST(pathFormatTest, rejects_null_path)
{
    // Arrange
    int error_out = 0;

    // Pre-Assert

    // Act
    int actual_ret =
        call_vformat_path(NULL, 8, &error_out, "%s",
                          "value"); // [手順] - path に NULL を指定して cplat_internal_vformat_path を呼び出す。

    // Assert
    EXPECT_EQ(-1, actual_ret);    // [確認_異常系] - cplat_internal_vformat_path の戻り値が -1 であること。
    EXPECT_EQ(EINVAL, error_out); // [確認_異常系] - error_out に EINVAL が格納されること。
}

// path_size が 0 の場合に引数不正を報告することの確認
TEST(pathFormatTest, rejects_zero_path_size)
{
    // Arrange
    char path[8] = "before";
    int error_out = 0;

    // Pre-Assert

    // Act
    int actual_ret =
        call_vformat_path(path, 0, &error_out, "%s",
                          "value"); // [手順] - path_size に 0 を指定して cplat_internal_vformat_path を呼び出す。

    // Assert
    EXPECT_EQ(-1, actual_ret);    // [確認_異常系] - cplat_internal_vformat_path の戻り値が -1 であること。
    EXPECT_EQ(EINVAL, error_out); // [確認_異常系] - error_out に EINVAL が格納されること。
}

// format が NULL の場合に引数不正を報告することの確認
TEST(pathFormatTest, rejects_null_format)
{
    // Arrange
    char path[8] = "before";
    int error_out = 0;

    // Pre-Assert

    // Act
    int actual_ret =
        call_vformat_path(path, sizeof(path), &error_out,
                          NULL); // [手順] - format に NULL を指定して cplat_internal_vformat_path を呼び出す。

    // Assert
    EXPECT_EQ(-1, actual_ret);    // [確認_異常系] - cplat_internal_vformat_path の戻り値が -1 であること。
    EXPECT_EQ(EINVAL, error_out); // [確認_異常系] - error_out に EINVAL が格納されること。
}

// 引数不正時に error_out へ NULL を指定できることの確認
TEST(pathFormatTest, allows_null_error_out_for_invalid_argument)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret = call_vformat_path(
        NULL, 8, NULL, "%s",
        "value"); // [手順] - path と error_out に NULL を指定して cplat_internal_vformat_path を呼び出す。

    // Assert
    EXPECT_EQ(-1, actual_ret); // [確認_異常系] - cplat_internal_vformat_path の戻り値が -1 であること。
}

// 書式化の失敗時に EINVAL を報告することの確認
TEST(pathFormatTest, reports_vsnprintf_failure)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    char path[16] = "before";
    int error_out = 0;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_vsnprintf(path, sizeof(path), StrEq("value-7")))
        .WillOnce(Return(
            CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_異常系] - cplat_vsnprintf が展開済み文字列 "value-7" で 1 回呼び出されること。
                                 // [Pre-Assert手順] - cplat_vsnprintf から CPLAT_ERR_UNKNOWN を返却する。

    // Act
    int actual_ret = call_vformat_path(path, sizeof(path), &error_out, "value-%d",
                                       7); // [手順] - 書式化が失敗する条件で cplat_internal_vformat_path を呼び出す。

    // Assert
    EXPECT_EQ(-1, actual_ret);    // [確認_異常系] - cplat_internal_vformat_path の戻り値が -1 であること。
    EXPECT_EQ(EINVAL, error_out); // [確認_異常系] - error_out に EINVAL が格納されること。
}

// 書式化の失敗時に error_out へ NULL を指定できることの確認
TEST(pathFormatTest, allows_null_error_out_for_vsnprintf_failure)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    char path[16] = "before";

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_vsnprintf(path, sizeof(path), StrEq("value")))
        .WillOnce(Return(
            CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_異常系] - cplat_vsnprintf が展開済み文字列 "value" で 1 回呼び出されること。
                                 // [Pre-Assert手順] - cplat_vsnprintf から CPLAT_ERR_UNKNOWN を返却する。

    // Act
    int actual_ret = call_vformat_path(
        path, sizeof(path), NULL, "%s",
        "value"); // [手順] - 書式化が失敗する条件で error_out に NULL を指定して cplat_internal_vformat_path を呼び出す。

    // Assert
    EXPECT_EQ(-1, actual_ret); // [確認_異常系] - cplat_internal_vformat_path の戻り値が -1 であること。
}

// バッファー不足時に ENAMETOOLONG を報告することの確認
TEST(pathFormatTest, reports_buffer_too_small_from_wrapper)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    char path[16] = "before";
    int error_out = 0;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_vsnprintf(path, sizeof(path), StrEq("value")))
        .WillOnce(Return(
            CPLAT_ERR_BUFFER_TOO_SMALL)); // [Pre-Assert確認_異常系] - cplat_vsnprintf が展開済み文字列 "value" で 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_vsnprintf から CPLAT_ERR_BUFFER_TOO_SMALL を返却する。

    // Act
    int actual_ret = call_vformat_path(path, sizeof(path), &error_out, "%s",
                                       "value"); // [手順] - 書式化がバッファー不足を返す条件で呼び出す。

    // Assert
    EXPECT_EQ(-1, actual_ret);          // [確認_異常系] - cplat_internal_vformat_path の戻り値が -1 であること。
    EXPECT_EQ(ENAMETOOLONG, error_out); // [確認_異常系] - error_out に ENAMETOOLONG が格納されること。
}

// 出力が切り詰められる場合に ENAMETOOLONG を報告することの確認
TEST(pathFormatTest, reports_truncation)
{
    // Arrange
    char path[4] = "";
    int error_out = 0;

    // Pre-Assert

    // Act
    int actual_ret = call_vformat_path(
        path, sizeof(path), &error_out, "%s",
        "abcd"); // [手順] - 出力長が path_size 以上になる条件で cplat_internal_vformat_path を呼び出す。

    // Assert
    EXPECT_EQ(-1, actual_ret);          // [確認_異常系] - cplat_internal_vformat_path の戻り値が -1 であること。
    EXPECT_EQ(ENAMETOOLONG, error_out); // [確認_異常系] - error_out に ENAMETOOLONG が格納されること。
}

// 出力切り詰め時に error_out へ NULL を指定できることの確認
TEST(pathFormatTest, allows_null_error_out_for_truncation)
{
    // Arrange
    char path[4] = "";

    // Pre-Assert

    // Act
    int actual_ret = call_vformat_path(
        path, sizeof(path), NULL, "%s",
        "abcd"); // [手順] - 出力が切り詰められる条件で error_out に NULL を指定して cplat_internal_vformat_path を呼び出す。

    // Assert
    EXPECT_EQ(-1, actual_ret); // [確認_異常系] - cplat_internal_vformat_path の戻り値が -1 であること。
}

// 書式展開に成功し error_out を変更しないことの確認
TEST(pathFormatTest, formats_path_and_preserves_error_out)
{
    // Arrange
    char path[16] = "";
    int error_out = 12345;

    // Pre-Assert

    // Act
    int actual_ret = call_vformat_path(path, sizeof(path), &error_out, "%s-%d", "value",
                                       7); // [手順] - 出力が収まる条件で cplat_internal_vformat_path を呼び出す。

    // Assert
    EXPECT_EQ(0, actual_ret);      // [確認_正常系] - cplat_internal_vformat_path の戻り値が 0 であること。
    EXPECT_STREQ("value-7", path); // [確認_正常系] - path に書式展開後の文字列が格納されること。
    EXPECT_EQ(12345, error_out);   // [確認_正常系] - 成功時に error_out が変更されないこと。
}

// 書式展開成功時に error_out へ NULL を指定できることの確認
TEST(pathFormatTest, allows_null_error_out_on_success)
{
    // Arrange
    char path[16] = "";

    // Pre-Assert

    // Act
    int actual_ret =
        call_vformat_path(path, sizeof(path), NULL, "%s",
                          "value"); // [手順] - error_out に NULL を指定して cplat_internal_vformat_path を呼び出す。

    // Assert
    EXPECT_EQ(0, actual_ret);    // [確認_正常系] - cplat_internal_vformat_path の戻り値が 0 であること。
    EXPECT_STREQ("value", path); // [確認_正常系] - path に書式展開後の文字列が格納されること。
}
