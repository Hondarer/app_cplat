#include <testfw.h>

#include "filterTestSupport.h"

/* 命令形式を書き換えるため、モジュール私有ヘッダーを取り込む */
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

/** 行レコードの見出しを読み取ります。 */
string_catalog_filter_record_header read_record_header(const unsigned char *record)
{
    string_catalog_filter_record_header header;

    string_catalog_filter_read_record_header(record, &header);
    return header;
}

/** 命令を 1 個読み取ります。 */
string_catalog_filter_instruction read_instruction(const unsigned char *record, uint32_t index)
{
    string_catalog_filter_instruction instruction;

    string_catalog_filter_read_instruction(record, index, &instruction);
    return instruction;
}

/** 指定した種類の命令の位置を返します。見つからない場合は UINT32_MAX を返します。 */
uint32_t find_instruction(const unsigned char *record, uint8_t opcode)
{
    const string_catalog_filter_record_header header = read_record_header(record);

    for (uint32_t index = 0; index < header.instruction_count; index++)
    {
        if (read_instruction(record, index).opcode == opcode)
        {
            return index;
        }
    }
    return UINT32_MAX;
}
} // namespace

class stringCatalogFilterStructureTest : public Test
{
  protected:
    /** 書き換えの対象とするフィルター オブジェクトです。 */
    unsigned char image[kImageSize];

    /** 先頭行の行レコードです。 */
    unsigned char *record = nullptr;

    /** 1 行の条件式をコンパイルし、検証に成功することを確かめたうえで先頭行の行レコードを得ます。 */
    void prepare(const char *text)
    {
        std::memset(image, 0, sizeof(image));
        ASSERT_EQ(CPLAT_OK, compile_single_line(text, image));
        // [状態確認] - `compile_single_line(text, image)` の戻り値が `CPLAT_OK` であること。
        ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_validate(image, kImageSize));
        // [状態確認] - `cplat_string_catalog_filter_validate(image, kImageSize)` の戻り値が `CPLAT_OK` であること。
        record = filter_test_record_address(image, kLineWidth, 0U);
    }

    /** 命令を 1 個書き換えます。 */
    void write_instruction(uint32_t index, const string_catalog_filter_instruction &instruction)
    {
        string_catalog_filter_write_instruction(record, index, &instruction);
    }
};

// 書き換えずにハッシュ値だけを計算し直した場合は検証に成功し、計算し直しが正しいことの確認
// 以降のテストが、ハッシュ値の不一致ではなく構造の検査で拒否されることの前提とする
TEST_F(stringCatalogFilterStructureTest, rehash_without_change_keeps_image_valid)
{
    // Arrange
    int actual_ret;

    prepare("key == 1 || !(id == \"X\")"); // [状態] - ジャンプ、否定、文字列定数を含む条件式をコンパイルする。

    // Pre-Assert

    // Act
    rehash(image, record);                                                // [手順] - ハッシュ値を計算し直す。
    actual_ret = cplat_string_catalog_filter_validate(image, kImageSize); // [手順] - 検証する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 検証に成功すること。
}

// ジャンプ先が自身より前の命令を指す場合は、構造の検査で拒否することの確認
TEST_F(stringCatalogFilterStructureTest, backward_jump_is_rejected)
{
    // Arrange
    uint32_t jump_index;
    string_catalog_filter_instruction instruction;
    int actual_ret;

    prepare("key == 1 || key == 2"); // [状態] - 短絡評価のジャンプを含む条件式をコンパイルする。
    jump_index = find_instruction(record, (uint8_t)STRING_CATALOG_FILTER_OPCODE_JUMP_IF_TRUE);
    ASSERT_NE(UINT32_MAX, jump_index); // [状態確認] - ジャンプ命令があること。

    // Pre-Assert

    // Act
    instruction = read_instruction(record, jump_index);
    instruction.operand = 0U;
    write_instruction(jump_index, instruction); // [手順] - ジャンプ先を先頭の命令へ書き換える。
    rehash(image, record);                      // [手順] - ハッシュ値を計算し直す。
    actual_ret = cplat_string_catalog_filter_validate(image, kImageSize); // [手順] - 検証する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR, actual_ret); // [確認_異常系] - 後方へのジャンプを拒否すること。
}

// ジャンプ先が命令列の末尾より後ろを指す場合は、構造の検査で拒否することの確認
TEST_F(stringCatalogFilterStructureTest, jump_beyond_end_is_rejected)
{
    // Arrange
    uint32_t jump_index;
    string_catalog_filter_instruction instruction;
    int actual_ret;

    prepare("key == 1 || key == 2"); // [状態] - 短絡評価のジャンプを含む条件式をコンパイルする。
    jump_index = find_instruction(record, (uint8_t)STRING_CATALOG_FILTER_OPCODE_JUMP_IF_TRUE);
    ASSERT_NE(UINT32_MAX, jump_index); // [状態確認] - ジャンプ命令があること。

    // Pre-Assert

    // Act
    instruction = read_instruction(record, jump_index);
    instruction.operand = (uint16_t)(read_record_header(record).instruction_count + 1U);
    write_instruction(jump_index, instruction); // [手順] - ジャンプ先を命令列の末尾の次の次へ書き換える。
    rehash(image, record);                      // [手順] - ハッシュ値を計算し直す。
    actual_ret = cplat_string_catalog_filter_validate(image, kImageSize); // [手順] - 検証する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR, actual_ret); // [確認_異常系] - 範囲外へのジャンプを拒否すること。
}

// 存在しない命令の種類は、構造の検査で拒否することの確認
TEST_F(stringCatalogFilterStructureTest, unknown_opcode_is_rejected)
{
    // Arrange
    string_catalog_filter_instruction instruction;
    int actual_ret;

    prepare("key == 1"); // [状態] - 判定要素 1 個の条件式をコンパイルする。

    // Pre-Assert

    // Act
    instruction = read_instruction(record, 0U);
    instruction.opcode = 0x7FU;
    write_instruction(0U, instruction); // [手順] - 先頭の命令の種類を存在しない値へ書き換える。
    rehash(image, record);              // [手順] - ハッシュ値を計算し直す。
    actual_ret = cplat_string_catalog_filter_validate(image, kImageSize); // [手順] - 検証する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR, actual_ret); // [確認_異常系] - 存在しない命令を拒否すること。
}

// 論理積の時点でスタックに値が 2 個ない場合は、構造の検査で拒否することの確認
TEST_F(stringCatalogFilterStructureTest, stack_underflow_is_rejected)
{
    // Arrange
    string_catalog_filter_instruction instruction;
    int actual_ret;

    prepare("key == 1"); // [状態] - 判定要素 1 個の条件式をコンパイルする。

    // Pre-Assert

    // Act
    std::memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (uint8_t)STRING_CATALOG_FILTER_OPCODE_AND;
    write_instruction(0U, instruction); // [手順] - 先頭の判定要素を、値を 2 個消費する論理積へ書き換える。
    rehash(image, record);              // [手順] - ハッシュ値を計算し直す。
    actual_ret = cplat_string_catalog_filter_validate(image, kImageSize); // [手順] - 検証する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR, actual_ret); // [確認_異常系] - スタックの不足を拒否すること。
}

// 判定要素以外の命令がフィールドを持つ場合は、構造の検査で拒否することの確認
TEST_F(stringCatalogFilterStructureTest, logical_instruction_with_field_is_rejected)
{
    // Arrange
    uint32_t not_index;
    string_catalog_filter_instruction instruction;
    int actual_ret;

    prepare("!(key == 1)"); // [状態] - 否定を含む条件式をコンパイルする。
    not_index = find_instruction(record, (uint8_t)STRING_CATALOG_FILTER_OPCODE_NOT);
    ASSERT_NE(UINT32_MAX, not_index); // [状態確認] - 否定の命令があること。

    // Pre-Assert

    // Act
    instruction = read_instruction(record, not_index);
    instruction.field = (uint8_t)STRING_CATALOG_FILTER_FIELD_KEY;
    write_instruction(not_index, instruction);                            // [手順] - 否定の命令へフィールドを書き込む。
    rehash(image, record);                                                // [手順] - ハッシュ値を計算し直す。
    actual_ret = cplat_string_catalog_filter_validate(image, kImageSize); // [手順] - 検証する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR, actual_ret); // [確認_異常系] - 余分なフィールドを拒否すること。
}

// 見出しのスタックの深さが命令列と一致しない場合は、構造の検査で拒否することの確認
TEST_F(stringCatalogFilterStructureTest, stack_depth_mismatch_is_rejected)
{
    // Arrange
    string_catalog_filter_record_header header;
    int actual_ret;

    prepare("key == 1 && key == 2"); // [状態] - 判定要素 2 個の条件式をコンパイルする。

    // Pre-Assert

    // Act
    header = read_record_header(record);
    header.stack_depth = (uint8_t)(header.stack_depth + 1U);
    string_catalog_filter_write_record_header(record, &header);           // [手順] - スタックの深さを 1 増やす。
    rehash(image, record);                                                // [手順] - ハッシュ値を計算し直す。
    actual_ret = cplat_string_catalog_filter_validate(image, kImageSize); // [手順] - 検証する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR, actual_ret); // [確認_異常系] - 深さの不一致を拒否すること。
}

// 命令数が 0 の行は、構造の検査で拒否することの確認
TEST_F(stringCatalogFilterStructureTest, empty_instruction_list_is_rejected)
{
    // Arrange
    string_catalog_filter_record_header header;
    int actual_ret;

    prepare("key == 1"); // [状態] - 判定要素 1 個の条件式をコンパイルする。

    // Pre-Assert

    // Act
    header = read_record_header(record);
    header.instruction_count = 0U;
    string_catalog_filter_write_record_header(record, &header);           // [手順] - 命令数を 0 にする。
    rehash(image, record);                                                // [手順] - ハッシュ値を計算し直す。
    actual_ret = cplat_string_catalog_filter_validate(image, kImageSize); // [手順] - 検証する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR, actual_ret); // [確認_異常系] - 命令のない行を拒否すること。
}

// 定数の参照位置が定数領域の外を指す場合は、構造の検査で拒否することの確認
TEST_F(stringCatalogFilterStructureTest, constant_offset_out_of_range_is_rejected)
{
    // Arrange
    string_catalog_filter_instruction instruction;
    int actual_ret;

    prepare("key == 1"); // [状態] - 定数を 1 個参照する条件式をコンパイルする。

    // Pre-Assert

    // Act
    instruction = read_instruction(record, 0U);
    instruction.operand = (uint16_t)(read_record_header(record).constant_size + 8U);
    write_instruction(0U, instruction); // [手順] - 定数の参照位置を、使用している定数領域の外へ書き換える。
    rehash(image, record);              // [手順] - ハッシュ値を計算し直す。
    actual_ret = cplat_string_catalog_filter_validate(image, kImageSize); // [手順] - 検証する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR, actual_ret); // [確認_異常系] - 範囲外の定数参照を拒否すること。
}

// フィールド、演算子、定数の種類の組み合わせが成り立たない場合は、構造の検査で拒否することの確認
TEST_F(stringCatalogFilterStructureTest, operator_and_constant_mismatch_is_rejected)
{
    // Arrange
    string_catalog_filter_instruction instruction;
    int actual_ret;

    prepare("id == \"X\""); // [状態] - 文字列の定数と比較する条件式をコンパイルする。

    // Pre-Assert

    // Act
    instruction = read_instruction(record, 0U);
    instruction.operator_kind = (uint8_t)STRING_CATALOG_FILTER_OPERATOR_LESS;
    write_instruction(0U, instruction); // [手順] - 演算子を、文字列には使えない大小比較へ書き換える。
    rehash(image, record);              // [手順] - ハッシュ値を計算し直す。
    actual_ret = cplat_string_catalog_filter_validate(image, kImageSize); // [手順] - 検証する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR, actual_ret); // [確認_異常系] - 成り立たない組み合わせを拒否すること。
}

// 存在しない種類の定数は、構造の検査で拒否することの確認
TEST_F(stringCatalogFilterStructureTest, unknown_constant_kind_is_rejected)
{
    // Arrange
    unsigned char *constants;
    int actual_ret;

    prepare("key == 1"); // [状態] - 定数を 1 個参照する条件式をコンパイルする。

    // Pre-Assert

    // Act
    constants = record + (string_catalog_filter_record_constants(record, (uint32_t)kLineWidth) - record);
    constants[read_instruction(record, 0U).operand] = 0x7FU; // [手順] - 参照先の定数の種類を存在しない値へ書き換える。
    rehash(image, record);                                   // [手順] - ハッシュ値を計算し直す。
    actual_ret = cplat_string_catalog_filter_validate(image, kImageSize); // [手順] - 検証する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR, actual_ret); // [確認_異常系] - 存在しない定数の種類を拒否すること。
}

// 形式版が異なるフィルター オブジェクトは、変換せずに拒否することの確認
TEST_F(stringCatalogFilterStructureTest, different_format_version_is_rejected)
{
    // Arrange
    string_catalog_filter_image_header header;
    int actual_ret;

    prepare("key == 1"); // [状態] - 条件式をコンパイルする。

    // Pre-Assert

    // Act
    string_catalog_filter_read_image_header(image, &header);
    header.format_version = (uint16_t)(STRING_CATALOG_FILTER_FORMAT_VERSION + 1U);
    string_catalog_filter_write_image_header(image, &header);             // [手順] - 形式版を 1 つ進める。
    string_catalog_filter_update_content_hash(image, &header);            // [手順] - 全体のハッシュ値を計算し直す。
    actual_ret = cplat_string_catalog_filter_validate(image, kImageSize); // [手順] - 検証する。

    cplat_string_catalog_filter_info actual_info;
    char actual_text[64];
    int actual_info_ret =
        cplat_string_catalog_filter_get_info(image, kImageSize, &actual_info);              // [手順] - 情報を取得する。
    int actual_remove_ret = cplat_string_catalog_filter_remove_line(image, kImageSize, 0U); // [手順] - 行を削除する。
    int actual_compile_ret = cplat_string_catalog_filter_compile_line(image, kImageSize, 0U, "key == 1",
                                                                      nullptr); // [手順] - 行を書き換える。
    int actual_decompile_ret = cplat_string_catalog_filter_decompile_line(
        image, kImageSize, 0U, actual_text, sizeof(actual_text)); // [手順] - 行を逆コンパイルする。

    // Assert
    EXPECT_EQ(CPLAT_ERR_VERSION_MISMATCH, actual_ret);        // [確認_異常系] - 異なる形式版を拒否すること。
    EXPECT_EQ(CPLAT_ERR_VERSION_MISMATCH, actual_info_ret);   // [確認_異常系] - 情報取得でも版の不一致を維持すること。
    EXPECT_EQ(CPLAT_ERR_VERSION_MISMATCH, actual_remove_ret); // [確認_異常系] - 削除でも版の不一致を維持すること。
    EXPECT_EQ(CPLAT_ERR_VERSION_MISMATCH,
              actual_compile_ret); // [確認_異常系] - コンパイルでも版の不一致を維持すること。
    EXPECT_EQ(CPLAT_ERR_VERSION_MISMATCH,
              actual_decompile_ret); // [確認_異常系] - 逆コンパイルでも版の不一致を維持すること。
}

// バイト順序の目印が異なるフィルター オブジェクトは、変換せずに拒否することの確認
TEST_F(stringCatalogFilterStructureTest, different_byte_order_is_rejected)
{
    // Arrange
    string_catalog_filter_image_header header;
    int actual_ret;

    prepare("key == 1"); // [状態] - 条件式をコンパイルする。

    // Pre-Assert

    // Act
    string_catalog_filter_read_image_header(image, &header);
    header.byte_order_mark = 0x0201U;
    string_catalog_filter_write_image_header(image, &header);             // [手順] - バイト順序の目印を逆順の値にする。
    string_catalog_filter_update_content_hash(image, &header);            // [手順] - 全体のハッシュ値を計算し直す。
    actual_ret = cplat_string_catalog_filter_validate(image, kImageSize); // [手順] - 検証する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR, actual_ret); // [確認_異常系] - 異なるバイト順序を拒否すること。
}
