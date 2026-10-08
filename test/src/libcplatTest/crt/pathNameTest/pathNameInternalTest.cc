#include <testfw.h>
#include <cplat/base/result.h>
#include <cplat/crt/path.h>
#include <errno.h>
#include <string.h>

#include "path_name.inject.h"

class pathNameInternalTest : public Test
{
};

// path_out に NULL を渡した場合に EINVAL が返ることの確認
TEST_F(pathNameInternalTest, copy_path_name_text_returns_einval_for_null_path_out)
{
    // Arrange
    cplat_error err; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert

    // Act
    int actual_ret =
        test_copy_path_name_text(NULL, 16u, &err,
                                 "."); // [手順] - test_copy_path_name_text(NULL, 16, &err, ".") を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - copy_path_name_text の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(1, cplat_error_is(&err, CPLAT_CAUSE_INVALID_ARGUMENT)); // [確認_異常系] - EINVAL の要因であること。
}

// path_size に 0 を渡した場合に EINVAL が返ることの確認
TEST_F(pathNameInternalTest, copy_path_name_text_returns_einval_for_zero_path_size)
{
    // Arrange
    char actual[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。
    cplat_error err;                // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert

    // Act
    int actual_ret =
        test_copy_path_name_text(actual, 0u, &err,
                                 "."); // [手順] - test_copy_path_name_text(actual, 0, &err, ".") を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - copy_path_name_text の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(1, cplat_error_is(&err, CPLAT_CAUSE_INVALID_ARGUMENT)); // [確認_異常系] - EINVAL の要因であること。
}

// text に NULL を渡した場合に EINVAL が返ることの確認
TEST_F(pathNameInternalTest, copy_path_name_text_returns_einval_for_null_text)
{
    // Arrange
    char actual[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。
    cplat_error err;                // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert

    // Act
    int actual_ret =
        test_copy_path_name_text(actual, sizeof(actual), &err,
                                 NULL); // [手順] - test_copy_path_name_text(actual, size, &err, NULL) を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - copy_path_name_text の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(1, cplat_error_is(&err, CPLAT_CAUSE_INVALID_ARGUMENT)); // [確認_異常系] - EINVAL の要因であること。
}

// 正常な引数でテキストがコピーされることの確認
TEST_F(pathNameInternalTest, copy_path_name_text_copies_text_into_buffer)
{
    // Arrange
    char actual[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。
    cplat_error err;                // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert

    // Act
    int actual_ret =
        test_copy_path_name_text(actual, sizeof(actual), &err,
                                 "."); // [手順] - test_copy_path_name_text(actual, size, &err, ".") を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - copy_path_name_text の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(".", actual);       // [確認_正常系] - 出力バッファーに "." がコピーされること。
}
