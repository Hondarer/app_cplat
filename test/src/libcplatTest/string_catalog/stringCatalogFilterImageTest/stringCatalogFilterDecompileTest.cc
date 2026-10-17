#include <testfw.h>

#include "filterTestSupport.h"

#include <cplat/base/result.h>

#include <cstring>

using namespace filter_test;

class stringCatalogFilterDecompileTest : public Test
{
};

namespace
{
/**
     *  @brief          1 行をデコンパイルし、その結果を再コンパイルした行レコードが元のバイト列と一致するかを返します。
     */
bool decompiled_text_recompiles_to_same_bytes(const void *original_image, std::size_t line_index, char *decoded)
{
    static unsigned char recompiled_image[kImageSize];
    const unsigned char *original_record;
    const unsigned char *recompiled_record;
    int decompile_ret;
    int recompile_ret;

    decompile_ret =
        cplat_string_catalog_filter_decompile_line(original_image, kImageSize, line_index, decoded, kLineWidth);
    if (decompile_ret != CPLAT_OK)
    {
        return false;
    }
    recompile_ret = compile_single_line(decoded, recompiled_image);
    if (recompile_ret != CPLAT_OK)
    {
        return false;
    }

    original_record = filter_test_record_address(original_image, kLineWidth, line_index);
    recompiled_record = filter_test_record_address(recompiled_image, kLineWidth, 0U);
    return std::memcmp(original_record, recompiled_record, CPLAT_STRING_CATALOG_FILTER_RECORD_SIZE(kLineWidth)) == 0;
}
} // namespace

// 各種判定要素を含む条件式をデコンパイルして再コンパイルすると、元のフィルター オブジェクトとバイト単位で一致することの確認
TEST_F(stringCatalogFilterDecompileTest, decompile_then_recompile_matches_original_bytes)
{
    // Arrange
    static unsigned char image[kImageSize];
    const char *lines[] = {
        "key in [1, 2, 3]",   "arg.priority between -5 and 10", "category == 0x03", "arg.ratio == 1.5",
        "arg.command == 's'", "arg.job_name == \"a\\\"b\\nc\"", "has(arg[45])",
    };
    char decoded[kLineWidth];
    int actual_compile_ret;
    bool actual_matches_in;
    bool actual_matches_between;
    bool actual_matches_hex;
    bool actual_matches_float;
    bool actual_matches_char;
    bool actual_matches_escaped_string;
    bool actual_matches_has;

    // Pre-Assert

    // Act
    actual_compile_ret = compile_lines(
        lines, 7U, image); // [手順] - 7 種類の判定要素を含む条件式リストを指定して compile_lines() を呼び出す。
    actual_matches_in = decompiled_text_recompiles_to_same_bytes(
        image, 0U, decoded); // [手順] - in の行を指定して decompiled_text_recompiles_to_same_bytes() を呼び出す。
    actual_matches_between = decompiled_text_recompiles_to_same_bytes(
        image, 1U, decoded); // [手順] - between の行を指定して decompiled_text_recompiles_to_same_bytes() を呼び出す。
    actual_matches_hex = decompiled_text_recompiles_to_same_bytes(
        image, 2U, decoded); // [手順] - 16 進数の行を指定して decompiled_text_recompiles_to_same_bytes() を呼び出す。
    actual_matches_float = decompiled_text_recompiles_to_same_bytes(
        image, 3U,
        decoded); // [手順] - 浮動小数点数の行を指定して decompiled_text_recompiles_to_same_bytes() を呼び出す。
    actual_matches_char = decompiled_text_recompiles_to_same_bytes(
        image, 4U, decoded); // [手順] - 文字定数の行を指定して decompiled_text_recompiles_to_same_bytes() を呼び出す。
    actual_matches_escaped_string = decompiled_text_recompiles_to_same_bytes(
        image, 5U,
        decoded); // [手順] - エスケープを含む文字列の行を指定して decompiled_text_recompiles_to_same_bytes() を呼び出す。
    actual_matches_has = decompiled_text_recompiles_to_same_bytes(
        image, 6U, decoded); // [手順] - has の行を指定して decompiled_text_recompiles_to_same_bytes() を呼び出す。

    // Assert
    ASSERT_EQ(CPLAT_OK, actual_compile_ret); // [確認_正常系] - compile_lines() の戻り値が CPLAT_OK であること。
    EXPECT_TRUE(
        actual_matches_in); // [確認_正常系] - in の行に対する decompiled_text_recompiles_to_same_bytes() の戻り値が true であること。
    EXPECT_TRUE(
        actual_matches_between); // [確認_正常系] - between の行に対する decompiled_text_recompiles_to_same_bytes() の戻り値が true であること。
    EXPECT_TRUE(
        actual_matches_hex); // [確認_正常系] - 16 進数の行に対する decompiled_text_recompiles_to_same_bytes() の戻り値が true であること。
    EXPECT_TRUE(
        actual_matches_float); // [確認_正常系] - 浮動小数点数の行に対する decompiled_text_recompiles_to_same_bytes() の戻り値が true であること。
    EXPECT_TRUE(
        actual_matches_char); // [確認_正常系] - 文字定数の行に対する decompiled_text_recompiles_to_same_bytes() の戻り値が true であること。
    EXPECT_TRUE(
        actual_matches_escaped_string); // [確認_正常系] - エスケープを含む文字列の行に対する decompiled_text_recompiles_to_same_bytes() の戻り値が true であること。
    EXPECT_TRUE(
        actual_matches_has); // [確認_正常系] - has の行に対する decompiled_text_recompiles_to_same_bytes() の戻り値が true であること。
}

// 否定の対象が判定要素の場合に、括弧付きで復元されることの確認
TEST_F(stringCatalogFilterDecompileTest, not_of_predicate_is_decompiled_with_parenthesis)
{
    // Arrange
    static unsigned char image[kImageSize];
    char actual_text[kLineWidth];
    int actual_compile_ret;
    int actual_decompile_ret;

    // Pre-Assert

    // Act
    actual_compile_ret = compile_single_line(
        "!(key == 1)", image); // [手順] - "!(key == 1)" を指定して compile_single_line() を呼び出す。
    actual_decompile_ret = cplat_string_catalog_filter_decompile_line(
        image, kImageSize, 0U, actual_text,
        sizeof(actual_text)); // [手順] - 行 0 を指定して cplat_string_catalog_filter_decompile_line() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_compile_ret); // [確認_正常系] - compile_single_line() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_decompile_ret); // [確認_正常系] - cplat_string_catalog_filter_decompile_line() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("!(key == 1)", actual_text); // [確認_正常系] - actual_text が "!(key == 1)" であること。
}

// a && (b && c) の形が、右結合を保つために括弧付きで復元されることの確認
TEST_F(stringCatalogFilterDecompileTest, right_grouped_and_chain_keeps_parenthesis)
{
    // Arrange
    static unsigned char image[kImageSize];
    char actual_text[kLineWidth];
    int actual_compile_ret;
    int actual_decompile_ret;

    // Pre-Assert

    // Act
    actual_compile_ret = compile_single_line(
        "key == 1 && (key == 2 && key == 3)",
        image); // [手順] - 右側を括弧でまとめた "&&" の連結を指定して compile_single_line() を呼び出す。
    actual_decompile_ret = cplat_string_catalog_filter_decompile_line(
        image, kImageSize, 0U, actual_text,
        sizeof(actual_text)); // [手順] - 行 0 を指定して cplat_string_catalog_filter_decompile_line() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_compile_ret); // [確認_正常系] - compile_single_line() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_decompile_ret); // [確認_正常系] - cplat_string_catalog_filter_decompile_line() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("key == 1 && (key == 2 && key == 3)",
                 actual_text); // [確認_正常系] - actual_text が "key == 1 && (key == 2 && key == 3)" であること。
}

// 出力先が小さい場合に CPLAT_ERR_BUFFER_TOO_SMALL を返し、切り詰めたうえで NUL 終端することの確認
TEST_F(stringCatalogFilterDecompileTest, small_destination_is_truncated_and_null_terminated)
{
    // Arrange
    static unsigned char image[kImageSize];
    char actual_text[3];
    int actual_compile_ret;
    int actual_decompile_ret;

    // Pre-Assert

    // Act
    actual_compile_ret =
        compile_single_line("key == 1", image); // [手順] - "key == 1" を指定して compile_single_line() を呼び出す。
    actual_decompile_ret = cplat_string_catalog_filter_decompile_line(
        image, kImageSize, 0U, actual_text,
        sizeof(
            actual_text)); // [手順] - 3 バイトの出力先を指定して cplat_string_catalog_filter_decompile_line() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_compile_ret); // [確認_正常系] - compile_single_line() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_BUFFER_TOO_SMALL,
        actual_decompile_ret); // [確認_異常系] - cplat_string_catalog_filter_decompile_line() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_EQ(
        '\0',
        actual_text[std::strlen(
            actual_text)]); // [確認_異常系] - actual_text[std::strlen(actual_text)] が '\0' であること。(切り詰め後も NUL 終端していることの確認)
    EXPECT_LT(std::strlen(actual_text),
              sizeof(actual_text)); // [確認_異常系] - std::strlen() の戻り値が sizeof(actual_text) より小さいこと。
}
