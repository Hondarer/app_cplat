#include <testfw.h>

#include "filterTestSupport.h"

#include <cplat/base/result.h>

#include <cstring>

using namespace filter_test;

class stringCatalogFilterEditTest : public Test
{
};

// 先頭と末尾への挿入が、デコンパイルの結果で確認できることの確認
TEST_F(stringCatalogFilterEditTest, insert_line_at_head_and_tail_are_reflected)
{
    // Arrange
    static unsigned char image[kImageSize];
    char actual_line0[kLineWidth];
    char actual_line1[kLineWidth];
    char actual_line2[kLineWidth];
    int actual_insert_head_ret;
    int actual_insert_tail_ret;

    ASSERT_EQ(CPLAT_OK, compile_single_line("key == 1", image)); // [状態] - 1 行の条件式をコンパイルする。
    // [状態確認] - `compile_single_line("key == 1", image)` の戻り値が `CPLAT_OK` であること。

    // Pre-Assert

    // Act
    actual_insert_head_ret = cplat_string_catalog_filter_insert_line(image, kImageSize, 0U, "key == 2",
                                                                     nullptr); // [手順] - 先頭 (行 0) へ挿入する。

    // Assert
    ASSERT_EQ(CPLAT_OK, actual_insert_head_ret); // [確認_正常系] - 先頭への挿入が成功すること。

    // Act_2
    actual_insert_tail_ret = cplat_string_catalog_filter_insert_line(
        image, kImageSize, 2U, "key == 3", nullptr); // [手順] - 末尾 (行数と同じ index=2) へ挿入する。
    (void)cplat_string_catalog_filter_decompile_line(image, kImageSize, 0U, actual_line0,
                                                     sizeof(actual_line0)); // [手順] - 行 0 を復元する。
    (void)cplat_string_catalog_filter_decompile_line(image, kImageSize, 1U, actual_line1,
                                                     sizeof(actual_line1)); // [手順] - 行 1 を復元する。
    (void)cplat_string_catalog_filter_decompile_line(image, kImageSize, 2U, actual_line2,
                                                     sizeof(actual_line2)); // [手順] - 行 2 を復元する。

    // Assert_2
    ASSERT_EQ(CPLAT_OK, actual_insert_tail_ret); // [確認_正常系] - 末尾への挿入が成功すること。
    EXPECT_STREQ("key == 2", actual_line0);      // [確認_正常系] - 行 0 が先頭に挿入した内容であること。
    EXPECT_STREQ("key == 1", actual_line1);      // [確認_正常系] - 行 1 が元の行であること。
    EXPECT_STREQ("key == 3", actual_line2);      // [確認_正常系] - 行 2 が末尾に挿入した内容であること。
}

// 行の置き換えが、対象の行だけへ反映されることの確認
TEST_F(stringCatalogFilterEditTest, compile_line_replaces_only_target_line)
{
    // Arrange
    static unsigned char image[kImageSize];
    const char *lines[] = {"key == 1", "key == 2"};
    char actual_line0[kLineWidth];
    char actual_line1[kLineWidth];
    int actual_ret;

    ASSERT_EQ(CPLAT_OK, compile_lines(lines, 2U, image)); // [状態] - 2 行の条件式をコンパイルする。
    // [状態確認] - `compile_lines(lines, 2U, image)` の戻り値が `CPLAT_OK` であること。

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_filter_compile_line(image, kImageSize, 1U, "key == 5",
                                                          nullptr); // [手順] - 行 1 を置き換える。
    (void)cplat_string_catalog_filter_decompile_line(image, kImageSize, 0U, actual_line0,
                                                     sizeof(actual_line0)); // [手順] - 行 0 を復元する。
    (void)cplat_string_catalog_filter_decompile_line(image, kImageSize, 1U, actual_line1,
                                                     sizeof(actual_line1)); // [手順] - 行 1 を復元する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret);        // [確認_正常系] - 置き換えが成功すること。
    EXPECT_STREQ("key == 1", actual_line0); // [確認_正常系] - 行 0 は変更されないこと。
    EXPECT_STREQ("key == 5", actual_line1); // [確認_正常系] - 行 1 が置き換えた内容であること。
}

// 行の削除で、後続の行が 1 つ前へ詰まることの確認
TEST_F(stringCatalogFilterEditTest, remove_line_shifts_following_lines)
{
    // Arrange
    static unsigned char image[kImageSize];
    const char *lines[] = {"key == 1", "key == 2", "key == 3"};
    cplat_string_catalog_filter_info actual_info;
    char actual_line0[kLineWidth];
    char actual_line1[kLineWidth];
    int actual_remove_ret;

    ASSERT_EQ(CPLAT_OK, compile_lines(lines, 3U, image)); // [状態] - 3 行の条件式をコンパイルする。
    // [状態確認] - `compile_lines(lines, 3U, image)` の戻り値が `CPLAT_OK` であること。

    // Pre-Assert

    // Act
    actual_remove_ret =
        cplat_string_catalog_filter_remove_line(image, kImageSize, 1U); // [手順] - 行 1 ("key == 2") を削除する。
    (void)cplat_string_catalog_filter_get_info(image, kImageSize, &actual_info); // [手順] - ヘッダー情報を取得する。
    (void)cplat_string_catalog_filter_decompile_line(image, kImageSize, 0U, actual_line0,
                                                     sizeof(actual_line0)); // [手順] - 行 0 を復元する。
    (void)cplat_string_catalog_filter_decompile_line(image, kImageSize, 1U, actual_line1,
                                                     sizeof(actual_line1)); // [手順] - 行 1 を復元する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_remove_ret); // [確認_正常系] - 削除が成功すること。
    EXPECT_EQ(2U, actual_info.line_count);  // [確認_正常系] - 格納数が 1 件減ること。
    EXPECT_STREQ("key == 1", actual_line0); // [確認_正常系] - 行 0 は変更されないこと。
    EXPECT_STREQ("key == 3", actual_line1); // [確認_正常系] - 削除した行の後続 ("key == 3") が 1 つ前へ詰まること。
}

// 不正な条件式での compile_line が CPLAT_ERR_MALFORMED_DEFINITION を返し、イメージを変更しないことの確認
TEST_F(stringCatalogFilterEditTest, malformed_compile_line_leaves_image_unchanged)
{
    // Arrange
    static unsigned char image[kImageSize];
    unsigned char snapshot[kImageSize];
    cplat_string_catalog_filter_diagnostic diagnostic;
    int actual_ret;

    ASSERT_EQ(CPLAT_OK, compile_single_line("key == 1", image)); // [状態] - 1 行の条件式をコンパイルする。
    // [状態確認] - `compile_single_line("key == 1", image)` の戻り値が `CPLAT_OK` であること。
    std::memcpy(snapshot, image, kImageSize);                    // [状態] - 変更前のバイト列を保存する。

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_filter_compile_line(
        image, kImageSize, 0U, "key ==", &diagnostic); // [手順] - 不正な条件式で行 0 を置き換えようとする。

    // Assert
    EXPECT_EQ(CPLAT_ERR_MALFORMED_DEFINITION, actual_ret); // [確認_異常系] - 構文エラーとして失敗すること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_SYNTAX,
              diagnostic.error);                            // [確認_異常系] - 診断情報の原因が構文の誤りであること。
    EXPECT_EQ(0, std::memcmp(snapshot, image, kImageSize)); // [確認_異常系] - イメージが変更されていないこと。
}

// 不正な条件式での insert_line が CPLAT_ERR_MALFORMED_DEFINITION を返し、イメージを変更しないことの確認
TEST_F(stringCatalogFilterEditTest, malformed_insert_line_leaves_image_unchanged)
{
    // Arrange
    static unsigned char image[kImageSize];
    unsigned char snapshot[kImageSize];
    cplat_string_catalog_filter_diagnostic diagnostic;
    int actual_ret;

    ASSERT_EQ(CPLAT_OK, compile_single_line("key == 1", image)); // [状態] - 1 行の条件式をコンパイルする。
    // [状態確認] - `compile_single_line("key == 1", image)` の戻り値が `CPLAT_OK` であること。
    std::memcpy(snapshot, image, kImageSize);                    // [状態] - 変更前のバイト列を保存する。

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_filter_insert_line(
        image, kImageSize, 1U, "x.y", &diagnostic); // [手順] - 不正な条件式を末尾へ挿入しようとする。

    // Assert
    EXPECT_EQ(CPLAT_ERR_MALFORMED_DEFINITION, actual_ret); // [確認_異常系] - 構文エラーとして失敗すること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_SYNTAX,
              diagnostic.error);                            // [確認_異常系] - 診断情報の原因が構文の誤りであること。
    EXPECT_EQ(0, std::memcmp(snapshot, image, kImageSize)); // [確認_異常系] - イメージが変更されていないこと。
}

// 行数の上限に達している場合、insert_line が CPLAT_ERR_STORAGE_FULL を返すことの確認
TEST_F(stringCatalogFilterEditTest, insert_line_at_full_capacity_returns_storage_full)
{
    // Arrange
    static unsigned char image[CPLAT_STRING_CATALOG_FILTER_IMAGE_SIZE(1U, kLineWidth)];
    int actual_ret;

    ASSERT_EQ(CPLAT_OK,
              compile_single_line("key == 1", image, CPLAT_STRING_CATALOG_FILTER_IMAGE_SIZE(1U, kLineWidth), kLineWidth,
                                  1U)); // [状態] - 行数の上限 1 いっぱいまで条件式を格納する。
    // [状態確認] - `compile_single_line("key == 1", image, CPLAT_STRING_CATALOG_FILTER_IMAGE_SIZE(1U, kLineWidth), kLineWidth, 1U)` の戻り値が `CPLAT_OK` であること。

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_filter_insert_line(
        image, CPLAT_STRING_CATALOG_FILTER_IMAGE_SIZE(1U, kLineWidth), 1U, "key == 2",
        nullptr); // [手順] - 上限に達した状態で末尾へ挿入しようとする。

    // Assert
    EXPECT_EQ(CPLAT_ERR_STORAGE_FULL, actual_ret); // [確認_異常系] - CPLAT_ERR_STORAGE_FULL を返すこと。
}
