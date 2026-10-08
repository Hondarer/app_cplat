#include <testfw.h>

#include "filterTestSupport.h"

#include <cplat/base/result.h>

#include <cstring>

using namespace filter_test;

class stringCatalogFilterValidateTest : public Test
{
};

// 正しくコンパイルしたフィルター オブジェクトが検証に成功することの確認
TEST_F(stringCatalogFilterValidateTest, valid_image_passes_validation)
{
    // Arrange
    static unsigned char image[kImageSize];
    int actual_compile_ret;
    int actual_validate_ret;

    // Pre-Assert

    // Act
    actual_compile_ret = compile_single_line("key == 1", image); // [手順] - 有効な条件式をコンパイルする。
    actual_validate_ret =
        cplat_string_catalog_filter_validate(image, kImageSize); // [手順] - フィルター オブジェクトを検証する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_compile_ret);  // [確認_正常系] - コンパイルが成功すること。
    EXPECT_EQ(CPLAT_OK, actual_validate_ret); // [確認_正常系] - 検証が成功すること。
}

// 署名が破損している場合に CPLAT_ERR_CORRUPT_DESCRIPTOR を返すことの確認
TEST_F(stringCatalogFilterValidateTest, corrupted_signature_returns_corrupt_descriptor)
{
    // Arrange
    static unsigned char image[kImageSize];
    int actual_validate_ret;

    ASSERT_EQ(CPLAT_OK, compile_single_line("key == 1", image)); // [状態] - 有効な条件式をコンパイルする。
    // [状態確認] - `compile_single_line("key == 1", image)` の戻り値が `CPLAT_OK` であること。
    image[0] = (unsigned char)(image[0] ^ 0xFFU);                // [状態] - 署名の先頭バイトを破壊する。

    // Pre-Assert

    // Act
    actual_validate_ret =
        cplat_string_catalog_filter_validate(image, kImageSize); // [手順] - フィルター オブジェクトを検証する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR, actual_validate_ret); // [確認_異常系] - 署名の破損を検出すること。
}

// 内容の 1 バイト改変がハッシュ値の不一致として検出されることの確認
TEST_F(stringCatalogFilterValidateTest, single_byte_content_change_is_detected_by_hash)
{
    // Arrange
    static unsigned char image[kImageSize];
    unsigned char *record;
    int actual_validate_ret_before;
    int actual_validate_ret_after;

    ASSERT_EQ(CPLAT_OK, compile_single_line("key == 1", image)); // [状態] - 有効な条件式をコンパイルする。
    // [状態確認] - `compile_single_line("key == 1", image)` の戻り値が `CPLAT_OK` であること。
    actual_validate_ret_before = cplat_string_catalog_filter_validate(image, kImageSize);
    ASSERT_EQ(CPLAT_OK, actual_validate_ret_before); // [状態確認] - 改変前は検証に成功すること。

    // Pre-Assert

    // Act
    /* 命令数・定数サイズの使用域より後方 (レコード末尾の未使用パディング) を 1 バイト改変する。
     * 行のハッシュ値は使用域だけを対象とするため check_record は通過するが、
     * イメージ全体のハッシュ値はレコードの全バイトを対象とするため不一致を検出する。 */
    record = filter_test_record_address(image, kLineWidth, 0U);
    record[CPLAT_STRING_CATALOG_FILTER_RECORD_SIZE(kLineWidth) - 1U] ^=
        0xFFU; // [手順] - 先頭行レコードの末尾 1 バイトを反転する。
    actual_validate_ret_after =
        cplat_string_catalog_filter_validate(image, kImageSize); // [手順] - フィルター オブジェクトを再検証する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR,
              actual_validate_ret_after); // [確認_異常系] - 内容のハッシュ値不一致を検出すること。
}

// image_size が不足している場合に CPLAT_ERR_CORRUPT_DESCRIPTOR を返すことの確認
TEST_F(stringCatalogFilterValidateTest, insufficient_image_size_returns_corrupt_descriptor)
{
    // Arrange
    static unsigned char image[kImageSize];
    int actual_ret;

    ASSERT_EQ(CPLAT_OK, compile_single_line("key == 1", image)); // [状態] - 有効な条件式をコンパイルする。
    // [状態確認] - `compile_single_line("key == 1", image)` の戻り値が `CPLAT_OK` であること。

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_filter_validate(
        image, CPLAT_STRING_CATALOG_FILTER_HEADER_SIZE); // [手順] - ヘッダー長だけの image_size で検証する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR,
              actual_ret); // [確認_異常系] - 宣言されている image_size に満たない場合は破損として扱うこと。
}
