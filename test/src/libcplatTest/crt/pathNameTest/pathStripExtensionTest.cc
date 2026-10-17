#include <testfw.h>
#include <cplat/base/result.h>
#include <cplat/crt/path.h>
#include <errno.h>
#include <string.h>

class pathStripExtensionTest : public Test
{
};

// パスから拡張子を除いたパスがコピーされることの確認
TEST_F(pathStripExtensionTest, removes_extension)
{
    // Arrange
    char actual[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_strip_extension(actual, sizeof(actual), nullptr,
                                                "a/b.txt"); // [手順] - cplat_path_strip_extension() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_path_strip_extension() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("a/b", actual);     // [確認_正常系] - actual が "a/b" であること。
}

// 拡張子がない場合そのままコピーされることの確認
TEST_F(pathStripExtensionTest, copies_as_is_when_no_extension)
{
    // Arrange
    char actual[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_strip_extension(actual, sizeof(actual), nullptr,
                                                "noext"); // [手順] - cplat_path_strip_extension() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_path_strip_extension() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("noext", actual);   // [確認_正常系] - actual が "noext" であること。
}

// NULL path_out で EINVAL が返ることの確認
TEST_F(pathStripExtensionTest, returns_einval_for_null_path_out)
{
    // Arrange
    cplat_error err; // [状態] - 詳細エラーの格納先を用意する。
    cplat_error last_error;

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_strip_extension(nullptr, 16, &err,
                                                "a.txt"); // [手順] - cplat_path_strip_extension() を呼び出す。
    cplat_error_get_last(
        &last_error); // [手順] - TLS に記録された詳細エラーの格納先として last_error を指定して cplat_error_get_last() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret); // [確認_異常系] - cplat_path_strip_extension() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        1,
        cplat_error_is(
            &err,
            CPLAT_CAUSE_INVALID_ARGUMENT)); // [確認_異常系] - cplat_error_is() の戻り値が 1 であること。(err の要因が CPLAT_CAUSE_INVALID_ARGUMENT であることの確認)
    EXPECT_EQ(
        1,
        cplat_error_is_set(
            &last_error)); // [確認_異常系] - cplat_error_is_set() の戻り値が 1 であること。(TLS に詳細エラーが記録されたことの確認)
}

// path_size が 0 の場合に EINVAL が返ることの確認
TEST_F(pathStripExtensionTest, returns_einval_for_zero_path_size)
{
    // Arrange
    char actual[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。
    cplat_error err;                // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_strip_extension(
        actual, 0u, &err,
        "a.txt"); // [手順] - path_size に 0 を指定して cplat_path_strip_extension() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret); // [確認_異常系] - path_size が 0 の cplat_path_strip_extension() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        1,
        cplat_error_is(
            &err,
            CPLAT_CAUSE_INVALID_ARGUMENT)); // [確認_異常系] - cplat_error_is() の戻り値が 1 であること。(err の要因が CPLAT_CAUSE_INVALID_ARGUMENT であることの確認)
}

// NULL path で EINVAL が返ることの確認
TEST_F(pathStripExtensionTest, returns_einval_for_null_path)
{
    // Arrange
    char actual[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。
    cplat_error err;                // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_strip_extension(actual, sizeof(actual), &err,
                                                nullptr); // [手順] - cplat_path_strip_extension() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret); // [確認_異常系] - cplat_path_strip_extension() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        1,
        cplat_error_is(
            &err,
            CPLAT_CAUSE_INVALID_ARGUMENT)); // [確認_異常系] - cplat_error_is() の戻り値が 1 であること。(err の要因が CPLAT_CAUSE_INVALID_ARGUMENT であることの確認)
}

// 空文字列パスで EINVAL が返ることの確認
TEST_F(pathStripExtensionTest, returns_einval_for_empty_path)
{
    // Arrange
    char actual[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。
    cplat_error err;                // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_strip_extension(actual, sizeof(actual), &err,
                                                ""); // [手順] - cplat_path_strip_extension() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret); // [確認_異常系] - cplat_path_strip_extension() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        1,
        cplat_error_is(
            &err,
            CPLAT_CAUSE_INVALID_ARGUMENT)); // [確認_異常系] - cplat_error_is() の戻り値が 1 であること。(err の要因が CPLAT_CAUSE_INVALID_ARGUMENT であることの確認)
}

// バッファー不足で ENAMETOOLONG が返ることの確認
TEST_F(pathStripExtensionTest, returns_enametoolong_when_buffer_too_small)
{
    // Arrange
    char actual[2];  // [状態] - "abc" (3 バイト必要) に対し 2 バイトの出力バッファーを用意する。
    cplat_error err; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_strip_extension(actual, sizeof(actual), &err,
                                                "abc.txt"); // [手順] - cplat_path_strip_extension() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_BUFFER_TOO_SMALL,
        actual_ret); // [確認_異常系] - cplat_path_strip_extension() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_EQ(
        1,
        cplat_error_is(
            &err,
            CPLAT_CAUSE_NAME_TOO_LONG)); // [確認_異常系] - cplat_error_is() の戻り値が 1 であること。(err の要因が CPLAT_CAUSE_NAME_TOO_LONG であることの確認)
}
