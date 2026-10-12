#include <testfw.h>
/* テスト対象が呼び出す cplat_regex_* の mock。Windows で実装オブジェクトを取り込むため、ヘッダーを取り込む */
#include <mock_cplat.h>

#include "filterTestSupport.h"

/* パターンの定数と見出しを確かめ、書き換えるため、モジュール私有ヘッダーを取り込む */
#include "filter.h"

#include <cplat/base/result.h>

#include <cstdint>
#include <cstring>

using namespace filter_test;

namespace
{
/** 行レコードの見出しと全体のハッシュ値を計算し直し、ハッシュ値の確認では拒否されない状態にします。 */
void rehash(unsigned char *image, unsigned char *record)
{
    string_catalog_filter_record_header record_header;
    string_catalog_filter_image_header image_header;

    string_catalog_filter_read_record_header(record, &record_header);
    record_header.line_hash = string_catalog_filter_compute_line_hash(record, (uint32_t)kLineWidth);
    string_catalog_filter_write_record_header(record, &record_header);
    string_catalog_filter_read_image_header(image, &image_header);
    string_catalog_filter_update_content_hash(image, &image_header);
}
} // namespace

class stringCatalogFilterPatternCompileTest : public Test
{
  protected:
    unsigned char image_[kImageSize];

    // [サブ手順 名前=stringCatalogFilterPatternCompileTest.SetUp]
    void SetUp() override
    {
        std::memset(image_, 0, sizeof(image_));
    }
    // [サブ手順終了]

    /** 1 行をコンパイルし、無効になった場合の原因を返します。 */
    cplat_string_catalog_filter_line_error compile_error_of(const char *text)
    {
        cplat_string_catalog_filter_diagnostic diagnostic;
        std::size_t invalid_count = 0U;

        std::memset(&diagnostic, 0, sizeof(diagnostic));
        (void)compile_single_line(text, image_, kImageSize, kLineWidth, kLineCapacity, &diagnostic, 1U, &invalid_count);
        if (invalid_count == 0U)
        {
            return CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE;
        }
        return diagnostic.error;
    }
};

// matches と matches_i を ID と文字列の引数へ書け、パターンの定数と番号を記録することの確認
// [サブ手順参照 名前=stringCatalogFilterPatternCompileTest.SetUp]
TEST_F(stringCatalogFilterPatternCompileTest, pattern_constants_are_numbered_per_line)
{
    // Arrange
    string_catalog_filter_record_header header;
    string_catalog_filter_constant constant;
    const unsigned char *record;
    const unsigned char *constants;
    uint32_t offset = 0U;
    uint16_t actual_slots[4] = {0xFFFFU, 0xFFFFU, 0xFFFFU, 0xFFFFU};
    std::size_t actual_pattern_constants = 0U;

    // Pre-Assert

    // Act
    int actual_ret_compile = compile_single_line("id matches \"^FILTER\" && arg.job_name matches_i \"IMP\"",
                                                 image_); // [手順] - 2 つのパターンを含む条件式をコンパイルする。
    ASSERT_EQ(CPLAT_OK, actual_ret_compile); // [確認_正常系] - compile_single_line() の戻り値が CPLAT_OK であること。
    record = filter_test_record_address(image_, kLineWidth, 0U);
    constants = string_catalog_filter_record_constants(record, (uint32_t)kLineWidth);
    string_catalog_filter_read_record_header(record, &header);
    while (offset < header.constant_size)
    {
        int actual_ret_read = string_catalog_filter_read_constant(constants, header.constant_size, offset, &constant);
        ASSERT_EQ(
            CPLAT_OK,
            actual_ret_read); // [状態確認] - string_catalog_filter_read_constant() の戻り値が CPLAT_OK であること。
        if ((constant.header.kind == (uint8_t)STRING_CATALOG_FILTER_CONSTANT_KIND_PATTERN) &&
            (actual_pattern_constants < 4U))
        {
            actual_slots[actual_pattern_constants] = constant.header.slot;
            actual_pattern_constants++;
        }
        offset = constant.next_offset;
    }

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        cplat_string_catalog_filter_validate(
            image_,
            kImageSize)); // [確認_正常系] - cplat_string_catalog_filter_validate() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        2U,
        header
            .pattern_count); // [確認_正常系] - header.pattern_count が 2U であること。(2 つのパターンの数を記録したことの確認)
    EXPECT_EQ(
        2U,
        actual_pattern_constants); // [確認_正常系] - actual_pattern_constants が 2U であること。(パターンの種類の定数が 2 個あることの確認)
    EXPECT_EQ(
        0U, actual_slots
                [0]); // [確認_正常系] - actual_slots[0] が 0U であること。(1 個目のパターンの番号が 0 であることの確認)
    EXPECT_EQ(
        1U, actual_slots
                [1]); // [確認_正常系] - actual_slots[1] が 1U であること。(2 個目のパターンの番号が 1 であることの確認)
}

// パターンを含む条件式をデコンパイルし、再コンパイルすると元と同じ表現になることの確認
// [サブ手順参照 名前=stringCatalogFilterPatternCompileTest.SetUp]
TEST_F(stringCatalogFilterPatternCompileTest, decompiled_pattern_recompiles_to_same_bytes)
{
    // Arrange
    static unsigned char recompiled[kImageSize];
    char actual_text[kLineWidth];

    ASSERT_EQ(CPLAT_OK, compile_single_line("arg.job_name matches \"^imp\\\\.[0-9]+\\\"x\" || id matches_i \"0005$\"",
                                            image_)); // [状態] - エスケープを含むパターンをコンパイルする。
    // [状態確認] - エスケープを含むパターンをコンパイルした compile_single_line() の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int actual_ret_decompile = cplat_string_catalog_filter_decompile_line(
        image_, kImageSize, 0U, actual_text, sizeof(actual_text)); // [手順] - デコンパイルする。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_decompile); // [確認_正常系] - cplat_string_catalog_filter_decompile_line() の戻り値が CPLAT_OK であること。
    int actual_ret_recompile =
        compile_single_line(actual_text, recompiled); // [手順] - 復元した条件式を再コンパイルする。

    // Assert
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_recompile); // [確認_正常系] - 復元した条件式を再コンパイルした compile_single_line() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "arg.job_name matches \"^imp\\\\.[0-9]+\\\"x\" || id matches_i \"0005$\"",
        actual_text); // [確認_正常系] - actual_text が "arg.job_name matches \"^imp\\\\.[0-9]+\\\"x\" || id matches_i \"0005$\"" であること。(元の表記へ復元したことの確認)
    EXPECT_EQ(
        0,
        std::memcmp(
            filter_test_record_address(image_, kLineWidth, 0U), filter_test_record_address(recompiled, kLineWidth, 0U),
            CPLAT_STRING_CATALOG_FILTER_RECORD_SIZE(
                kLineWidth))); // [確認_正常系] - std::memcmp() の戻り値が 0 であること。(元と同じ表現であることの確認)
}

// 正規表現として誤ったパターンは、変換の時点で行を無効にし、原因と位置を通知することの確認
// [サブ手順参照 名前=stringCatalogFilterPatternCompileTest.SetUp]
TEST_F(stringCatalogFilterPatternCompileTest, invalid_pattern_is_diagnosed_at_compile)
{
    // Arrange
    cplat_string_catalog_filter_diagnostic diagnostic;
    std::size_t invalid_count = 0U;

    std::memset(&diagnostic, 0, sizeof(diagnostic));

    // Pre-Assert

    // Act
    (void)compile_single_line("arg.job_name matches \"(imp\"", image_, kImageSize, kLineWidth, kLineCapacity,
                              &diagnostic, 1U,
                              &invalid_count); // [手順] - 閉じていない括弧を含むパターンをコンパイルする。

    // Assert
    EXPECT_EQ(1U, invalid_count); // [確認_異常系] - invalid_count が 1U であること。
    EXPECT_EQ(
        CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_INVALID_PATTERN,
        diagnostic
            .error); // [確認_異常系] - diagnostic.error が CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_INVALID_PATTERN であること。
    EXPECT_EQ(
        21U,
        diagnostic
            .column); // [確認_異常系] - diagnostic.column が 21U であること。(パターンの字句の位置が誤りの位置であることの確認)
}

// パターンを書けないフィールドと定数を、型の誤りとして拒否することの確認
// [サブ手順参照 名前=stringCatalogFilterPatternCompileTest.SetUp]
TEST_F(stringCatalogFilterPatternCompileTest, pattern_requires_string_field_and_string_literal)
{
    // Arrange

    // Pre-Assert

    // Act
    const cplat_string_catalog_filter_line_error actual_key =
        compile_error_of("key matches \"1\""); // [手順] - 文字列キーへ書く。
    const cplat_string_catalog_filter_line_error actual_category =
        compile_error_of("category matches \"1\""); // [手順] - 分類値へ書く。
    const cplat_string_catalog_filter_line_error actual_number =
        compile_error_of("arg.job_name matches 1"); // [手順] - 数値をパターンにする。
    const cplat_string_catalog_filter_line_error actual_null =
        compile_error_of("arg.job_name matches null"); // [手順] - null をパターンにする。

    // Assert
    EXPECT_EQ(
        CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_TYPE_MISMATCH,
        actual_key); // [確認_異常系] - 文字列キーへ書いた compile_error_of() の戻り値が CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_TYPE_MISMATCH であること。
    EXPECT_EQ(
        CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_TYPE_MISMATCH,
        actual_category); // [確認_異常系] - 分類値へ書いた compile_error_of() の戻り値が CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_TYPE_MISMATCH であること。
    EXPECT_EQ(
        CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_TYPE_MISMATCH,
        actual_number); // [確認_異常系] - 数値をパターンにした compile_error_of() の戻り値が CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_TYPE_MISMATCH であること。
    EXPECT_EQ(
        CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_TYPE_MISMATCH,
        actual_null); // [確認_異常系] - null をパターンにした compile_error_of() の戻り値が CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_TYPE_MISMATCH であること。
}

// 1 行のパターンの数の上限を超えた場合は、上限の超過として行を無効にすることの確認
// [サブ手順参照 名前=stringCatalogFilterPatternCompileTest.SetUp]
TEST_F(stringCatalogFilterPatternCompileTest, pattern_count_is_limited_per_line)
{
    // Arrange

    // Pre-Assert

    // Act
    const cplat_string_catalog_filter_line_error actual_at_limit = compile_error_of(
        "id matches \"a\" || id matches \"b\" || id matches \"c\" || id matches \"d\""); // [手順] - 上限の 4 個を書く。
    const cplat_string_catalog_filter_line_error actual_over_limit =
        compile_error_of("id matches \"a\" || id matches \"b\" || id matches \"c\" || id matches \"d\" || id matches "
                         "\"e\""); // [手順] - 5 個を書く。

    // Assert
    EXPECT_EQ(
        CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE,
        actual_at_limit); // [確認_正常系] - 上限の 4 個に対する compile_error_of() の戻り値が CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE であること。
    EXPECT_EQ(
        CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_LIMIT_EXCEEDED,
        actual_over_limit); // [確認_異常系] - 5 個に対する compile_error_of() の戻り値が CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_LIMIT_EXCEEDED であること。
}

// パターンの番号が見出しのパターンの数を超える場合は、構造の検査で拒否することの確認
// [サブ手順参照 名前=stringCatalogFilterPatternCompileTest.SetUp]
TEST_F(stringCatalogFilterPatternCompileTest, pattern_slot_beyond_count_is_rejected)
{
    // Arrange
    string_catalog_filter_record_header header;
    unsigned char *record;
    int actual_ret;

    ASSERT_EQ(CPLAT_OK, compile_single_line("id matches \"0005$\"",
                                            image_)); // [状態] - パターンを 1 個含む条件式をコンパイルする。
    // [状態確認] - compile_single_line() の戻り値が CPLAT_OK であること。
    record = filter_test_record_address(image_, kLineWidth, 0U);

    // Pre-Assert

    // Act
    string_catalog_filter_read_record_header(record, &header);
    header.pattern_count = 0U;
    string_catalog_filter_write_record_header(record, &header);            // [手順] - 見出しのパターンの数を 0 にする。
    rehash(image_, record);                                                // [手順] - ハッシュ値を計算し直す。
    actual_ret = cplat_string_catalog_filter_validate(image_, kImageSize); // [手順] - 検証する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_CORRUPT_DESCRIPTOR,
        actual_ret); // [確認_異常系] - cplat_string_catalog_filter_validate() の戻り値が CPLAT_ERR_CORRUPT_DESCRIPTOR であること。
}
