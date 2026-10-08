#include <testfw.h>
#include <cplat/base/result.h>
#include <cplat/crt/path.h>
#include <errno.h>
#include <string.h>

class pathDirnameTest : public Test
{
};

// 複数階層パスから親ディレクトリが取り出せることの確認
TEST_F(pathDirnameTest, returns_parent_of_nested_path)
{
    // Arrange
    char actual[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_path_dirname(actual, sizeof(actual), NULL,
                           "a/b/c"); // [手順] - cplat_path_dirname(actual, size, NULL, "a/b/c") を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_path_dirname の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("a/b", actual);     // [確認_正常系] - 親ディレクトリが "a/b" であること。
}

// セパレータを含まないパスで "." が返ることの確認
TEST_F(pathDirnameTest, returns_dot_when_no_separator)
{
    // Arrange
    char actual[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_dirname(actual, sizeof(actual), NULL,
                                        "c"); // [手順] - cplat_path_dirname(actual, size, NULL, "c") を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_path_dirname の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(".", actual);       // [確認_正常系] - "." が返ること。
}

// ルートのみのパスでルートが返ることの確認
TEST_F(pathDirnameTest, returns_root_for_root_path)
{
    // Arrange
    char actual[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_dirname(actual, sizeof(actual), NULL,
                                        "/"); // [手順] - cplat_path_dirname(actual, size, NULL, "/") を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_path_dirname の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("/", actual);       // [確認_正常系] - "/" が返ること。
}

// ルート直下のパスでルートが返ることの確認
TEST_F(pathDirnameTest, returns_root_for_top_level_path)
{
    // Arrange
    char actual[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_path_dirname(actual, sizeof(actual), NULL,
                           "/name"); // [手順] - cplat_path_dirname(actual, size, NULL, "/name") を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_path_dirname の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("/", actual);       // [確認_正常系] - "/" が返ること。
}

// 末尾セパレータが除去されて親ディレクトリが取れることの確認
TEST_F(pathDirnameTest, strips_trailing_separator_before_computing_parent)
{
    // Arrange
    char actual[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_dirname(actual, sizeof(actual), NULL,
                                        "a/b/"); // [手順] - cplat_path_dirname(actual, size, NULL, "a/b/") を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_path_dirname の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("a", actual);       // [確認_正常系] - "a" が返ること。
}

// '\\' 区切りの入力が '/' 正規化されて返ることの確認
TEST_F(pathDirnameTest, normalizes_backslash_separator_in_output)
{
    // Arrange
    char actual[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_path_dirname(actual, sizeof(actual), NULL,
                           "a\\b\\c"); // [手順] - cplat_path_dirname(actual, size, NULL, "a\\b\\c") を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_path_dirname の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("a/b", actual);     // [確認_正常系] - '/' 正規化された "a/b" が返ること。
}

// NULL path_out で EINVAL が返ることの確認
TEST_F(pathDirnameTest, returns_einval_for_null_path_out)
{
    // Arrange
    cplat_error err; // [状態] - 詳細エラーの格納先を用意する。
    cplat_error last_error;

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_dirname(NULL, 16, &err,
                                        "a/b"); // [手順] - cplat_path_dirname(NULL, 16, &err, "a/b") を呼び出す。
    cplat_error_get_last(&last_error);          // [手順] - TLS に記録された詳細エラーを取得する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_path_dirname の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(1, cplat_error_is(&err, CPLAT_CAUSE_INVALID_ARGUMENT)); // [確認_異常系] - EINVAL の要因であること。
    EXPECT_EQ(1, cplat_error_is_set(&last_error)); // [確認_異常系] - TLS に詳細エラーが記録されること。
}

// path_size が 0 の場合に EINVAL が返ることの確認
TEST_F(pathDirnameTest, returns_einval_for_zero_path_size)
{
    // Arrange
    char actual[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。
    cplat_error err;                // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_dirname(actual, 0u, &err,
                                        "a/b"); // [手順] - cplat_path_dirname(actual, 0, &err, "a/b") を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_path_dirname の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(1, cplat_error_is(&err, CPLAT_CAUSE_INVALID_ARGUMENT)); // [確認_異常系] - EINVAL の要因であること。
}

// NULL path で EINVAL が返ることの確認
TEST_F(pathDirnameTest, returns_einval_for_null_path)
{
    // Arrange
    char actual[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。
    cplat_error err;                // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_dirname(actual, sizeof(actual), &err,
                                        NULL); // [手順] - path に NULL を指定して cplat_path_dirname を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret); // [確認_異常系] - path が NULL の cplat_path_dirname の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(1, cplat_error_is(&err, CPLAT_CAUSE_INVALID_ARGUMENT)); // [確認_異常系] - EINVAL の要因であること。
}

// 空文字列パスで EINVAL が返ることの確認
TEST_F(pathDirnameTest, returns_einval_for_empty_path)
{
    // Arrange
    char actual[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。
    cplat_error err;                // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_dirname(actual, sizeof(actual), &err,
                                        ""); // [手順] - cplat_path_dirname(actual, size, &err, "") を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_path_dirname の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(1, cplat_error_is(&err, CPLAT_CAUSE_INVALID_ARGUMENT)); // [確認_異常系] - EINVAL の要因であること。
}

// バッファー不足で ENAMETOOLONG が返ることの確認
TEST_F(pathDirnameTest, returns_enametoolong_when_buffer_too_small)
{
    // Arrange
    char actual[2];  // [状態] - "a/b" の親 "a" (2 バイト必要) に対し 2 バイトの出力バッファーを用意する。
    cplat_error err; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_dirname(actual, sizeof(actual), &err,
                                        "ab/c"); // [手順] - cplat_path_dirname(actual, 2, &err, "ab/c") を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              actual_ret); // [確認_異常系] - cplat_path_dirname の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_EQ(1, cplat_error_is(&err, CPLAT_CAUSE_NAME_TOO_LONG)); // [確認_異常系] - ENAMETOOLONG の要因であること。
}

// "." の書き込みがバッファー不足になる場合に ENAMETOOLONG が返ることの確認
TEST_F(pathDirnameTest, returns_enametoolong_when_dot_does_not_fit)
{
    // Arrange
    char actual[1];  // [状態] - "." (終端込みで 2 バイト必要) に対し 1 バイトの出力バッファーを用意する。
    cplat_error err; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_dirname(actual, sizeof(actual), &err,
                                        "c"); // [手順] - cplat_path_dirname(actual, 1, &err, "c") を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              actual_ret);    // [確認_異常系] - cplat_path_dirname の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_STREQ("", actual); // [確認_異常系] - 出力バッファーが空文字列に初期化されること。
    EXPECT_EQ(1, cplat_error_is(&err, CPLAT_CAUSE_NAME_TOO_LONG)); // [確認_異常系] - ENAMETOOLONG の要因であること。
}

// ルートの書き込みがバッファー不足になる場合に ENAMETOOLONG が返ることの確認
TEST_F(pathDirnameTest, returns_enametoolong_when_root_does_not_fit)
{
    // Arrange
    char actual[1];  // [状態] - ルート 1 文字 (終端込みで 2 バイト必要) に対し 1 バイトの出力バッファーを用意する。
    cplat_error err; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_dirname(actual, sizeof(actual), &err,
                                        "/name"); // [手順] - cplat_path_dirname(actual, 1, &err, "/name") を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              actual_ret);    // [確認_異常系] - cplat_path_dirname の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_STREQ("", actual); // [確認_異常系] - 出力バッファーが空文字列に初期化されること。
    EXPECT_EQ(1, cplat_error_is(&err, CPLAT_CAUSE_NAME_TOO_LONG)); // [確認_異常系] - ENAMETOOLONG の要因であること。
}

// detail_out に NULL を渡しても動作することの確認
TEST_F(pathDirnameTest, allows_null_detail_out)
{
    // Arrange
    char actual[1]; // [状態] - 意図的に不足するバッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_dirname(actual, sizeof(actual), NULL,
                                        "a/b"); // [手順] - cplat_path_dirname(actual, 1, NULL, "a/b") を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_BUFFER_TOO_SMALL,
        actual_ret); // [確認_異常系] - cplat_path_dirname の戻り値として、detail_out が NULL でもクラッシュせず CPLAT_ERR_BUFFER_TOO_SMALL が返ること。
}
