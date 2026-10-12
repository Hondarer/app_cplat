#include <testfw.h>
#include <cplat/base/result.h>
#include <cplat/crt/path.h>
#include <errno.h>
#include <string.h>

class pathJoinTest : public Test
{
};

// セパレータのない 2 断片がセパレータ補完で結合されることの確認
TEST_F(pathJoinTest, inserts_separator_between_plain_fragments)
{
    // Arrange
    char actual[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_join(actual, sizeof(actual), NULL, "a",
                                     "b"); // [手順] - cplat_path_join(actual, size, NULL, "a", "b") を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_path_join() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("a/b", actual);     // [確認_正常系] - actual が "a/b" であること。
}

// 両断片の境界にセパレータが重複する場合に 1 つへ畳まれることの確認
TEST_F(pathJoinTest, collapses_duplicate_separators_at_boundary)
{
    // Arrange
    char actual[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_join(actual, sizeof(actual), NULL, "a/",
                                     "/b"); // [手順] - cplat_path_join(actual, size, NULL, "a/", "/b") を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_path_join() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("a/b", actual);     // [確認_正常系] - actual が "a/b" であること。
}

// 前の断片だけがセパレータで終わる場合に補完せず結合されることの確認
TEST_F(pathJoinTest, preserves_trailing_separator_at_boundary)
{
    // Arrange
    char actual[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_join(actual, sizeof(actual), NULL, "a/",
                                     "b"); // [手順] - 末尾だけにセパレータを持つ断片を cplat_path_join で結合する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_path_join() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("a/b", actual);     // [確認_正常系] - actual が "a/b" であること。
}

// 後ろの断片だけがセパレータで始まる場合に補完せず結合されることの確認
TEST_F(pathJoinTest, preserves_leading_separator_at_boundary)
{
    // Arrange
    char actual[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_join(actual, sizeof(actual), NULL, "a",
                                     "/b"); // [手順] - 先頭だけにセパレータを持つ断片を cplat_path_join で結合する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_path_join() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("a/b", actual);     // [確認_正常系] - actual が "a/b" であること。
}

// 先頭断片の絶対パス性が保持されることの確認
TEST_F(pathJoinTest, preserves_absolute_first_fragment)
{
    // Arrange
    char actual[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_join(actual, sizeof(actual), NULL, "/abs",
                                     "b"); // [手順] - cplat_path_join(actual, size, NULL, "/abs", "b") を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_path_join() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("/abs/b", actual);  // [確認_正常系] - actual が "/abs/b" であること。
}

// 空文字列の断片がスキップされることの確認
TEST_F(pathJoinTest, skips_empty_fragments)
{
    // Arrange
    char actual[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_join(actual, sizeof(actual), NULL, "a", "",
                                     "b"); // [手順] - cplat_path_join(actual, size, NULL, "a", "", "b") を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_path_join() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("a/b", actual);     // [確認_正常系] - actual が "a/b" であること。
}

// すべての断片が空の場合に空文字列で成功することの確認
TEST_F(pathJoinTest, succeeds_with_empty_result_when_all_fragments_empty)
{
    // Arrange
    char actual[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_join(actual, sizeof(actual), NULL, "",
                                     ""); // [手順] - cplat_path_join(actual, size, NULL, "", "") を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_path_join() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("", actual);        // [確認_正常系] - actual が "" であること。
}

// 単一断片がそのまま返ることの確認
TEST_F(pathJoinTest, returns_single_fragment_as_is)
{
    // Arrange
    char actual[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_join(actual, sizeof(actual), NULL,
                                     "only"); // [手順] - cplat_path_join(actual, size, NULL, "only") を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_path_join() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("only", actual);    // [確認_正常系] - actual が "only" であること。
}

// 16 断片 (上限) を結合できることの確認
TEST_F(pathJoinTest, joins_maximum_sixteen_fragments)
{
    // Arrange
    char actual[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_join(actual, sizeof(actual), NULL, "1", "2", "3", "4", "5", "6", "7", "8", "9", "10",
                                     "11", "12", "13", "14", "15",
                                     "16"); // [手順] - 16 個の断片で cplat_path_join を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_path_join() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("1/2/3/4/5/6/7/8/9/10/11/12/13/14/15/16",
                 actual); // [確認_正常系] - actual が "1/2/3/4/5/6/7/8/9/10/11/12/13/14/15/16" であること。
}

// NULL 断片で EINVAL が返ることの確認
TEST_F(pathJoinTest, returns_einval_for_null_fragment)
{
    // Arrange
    char actual[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。
    cplat_error err;                // [状態] - 詳細エラーの格納先を用意する。
    cplat_error last_error;

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_join_n(actual, sizeof(actual), &err, 2, "a",
                                       (const char *)NULL); // [手順] - 2 番目の断片に NULL を渡して呼び出す。
    cplat_error_get_last(&last_error);                      // [手順] - TLS に記録された詳細エラーを取得する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_path_join_n() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
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

// part_count が 0 の場合に EINVAL が返ることの確認
TEST_F(pathJoinTest, returns_einval_for_zero_part_count)
{
    // Arrange
    char actual[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。
    cplat_error err;                // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_join_n(actual, sizeof(actual), &err, 0); // [手順] - part_count に 0 を渡して呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_path_join_n() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        1,
        cplat_error_is(
            &err,
            CPLAT_CAUSE_INVALID_ARGUMENT)); // [確認_異常系] - cplat_error_is() の戻り値が 1 であること。(err の要因が CPLAT_CAUSE_INVALID_ARGUMENT であることの確認)
}

// path_size が 0 の場合に EINVAL が返ることの確認
TEST_F(pathJoinTest, returns_einval_for_zero_path_size)
{
    // Arrange
    char actual[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。
    cplat_error err;                // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_join(actual, 0u, &err, "a",
                                     "b"); // [手順] - cplat_path_join(actual, 0, &err, "a", "b") を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_path_join() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        1,
        cplat_error_is(
            &err,
            CPLAT_CAUSE_INVALID_ARGUMENT)); // [確認_異常系] - cplat_error_is() の戻り値が 1 であること。(err の要因が CPLAT_CAUSE_INVALID_ARGUMENT であることの確認)
}

// セパレータ補完を含めてちょうど収まる場合に成功することの確認
TEST_F(pathJoinTest, succeeds_when_result_exactly_fits_with_separator)
{
    // Arrange
    char actual[4]; // [状態] - "a/b" (3 バイト) + null 終端 (1 バイト) = 4 バイトの出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_join(actual, sizeof(actual), NULL, "a",
                                     "b"); // [手順] - cplat_path_join(actual, 4, NULL, "a", "b") を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_path_join() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("a/b", actual);     // [確認_正常系] - actual が "a/b" であること。
}

// セパレータ補完込みで 1 バイト不足する場合に ENAMETOOLONG が返ることの確認
TEST_F(pathJoinTest, returns_enametoolong_when_separator_insertion_overflows)
{
    // Arrange
    char actual[3];  // [状態] - "a/b" (3 バイト) + null 終端に対し 1 バイト不足するバッファーを用意する。
    cplat_error err; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_join(actual, sizeof(actual), &err, "a",
                                     "b"); // [手順] - cplat_path_join(actual, 3, &err, "a", "b") を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              actual_ret); // [確認_異常系] - cplat_path_join() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_EQ(
        1,
        cplat_error_is(
            &err,
            CPLAT_CAUSE_NAME_TOO_LONG)); // [確認_異常系] - cplat_error_is() の戻り値が 1 であること。(err の要因が CPLAT_CAUSE_NAME_TOO_LONG であることの確認)
}

// セパレータ補完の余地がちょうどない境界でバッファーをあふれさせずに ENAMETOOLONG が返ることの確認
TEST_F(pathJoinTest, returns_enametoolong_without_overflow_when_no_room_for_separator)
{
    // Arrange
    char actual[2] = {
        'X', 'X'};   // [状態] - "a" 単独ならちょうど収まるが結合には不足するバッファーをセンチネル付きで用意する。
    cplat_error err; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_join(actual, sizeof(actual), &err, "a",
                                     "b"); // [手順] - cplat_path_join(actual, 2, &err, "a", "b") を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              actual_ret); // [確認_異常系] - cplat_path_join() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_EQ(
        1,
        cplat_error_is(
            &err,
            CPLAT_CAUSE_NAME_TOO_LONG)); // [確認_異常系] - cplat_error_is() の戻り値が 1 であること。(err の要因が CPLAT_CAUSE_NAME_TOO_LONG であることの確認)
}

// 断片が単独でバッファーに収まらない場合に ENAMETOOLONG が返ることの確認
TEST_F(pathJoinTest, returns_enametoolong_when_single_fragment_does_not_fit)
{
    // Arrange
    char actual[3];  // [状態] - "abcde" に対し 3 バイトの出力バッファーを用意する。
    cplat_error err; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_join(actual, sizeof(actual), &err,
                                     "abcde"); // [手順] - cplat_path_join(actual, 3, &err, "abcde") を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              actual_ret); // [確認_異常系] - cplat_path_join() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_EQ(
        1,
        cplat_error_is(
            &err,
            CPLAT_CAUSE_NAME_TOO_LONG)); // [確認_異常系] - cplat_error_is() の戻り値が 1 であること。(err の要因が CPLAT_CAUSE_NAME_TOO_LONG であることの確認)
}

// NULL path_out で EINVAL が返ることの確認
TEST_F(pathJoinTest, returns_einval_for_null_path_out)
{
    // Arrange
    cplat_error err; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_path_join(NULL, 16, &err, "a",
                                     "b"); // [手順] - cplat_path_join(NULL, 16, &err, "a", "b") を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_path_join() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        1,
        cplat_error_is(
            &err,
            CPLAT_CAUSE_INVALID_ARGUMENT)); // [確認_異常系] - cplat_error_is() の戻り値が 1 であること。(err の要因が CPLAT_CAUSE_INVALID_ARGUMENT であることの確認)
}
