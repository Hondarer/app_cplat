#include <testfw.h>
#include <mock_cplat.h>

#include <cplat/base/result.h>

#include "string_catalog.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

class stringCatalogRenderTest : public Test
{
  protected:
    /** 組み立てた文字列の格納先です。 */
    char dest[64];

    /** 展開に使用する値の配列です。 */
    cplat_internal_string_catalog_argument_value values[CPLAT_STRING_CATALOG_ARGUMENT_MAX];

    // [サブ手順 名前=stringCatalogRenderTest.SetUp]
    void SetUp() override
    {
        memset(dest, 0, sizeof(dest));
        memset(values, 0, sizeof(values));
    }
    // [サブ手順終了]

    /**
     *  指定した位置へ 32 bit 符号付き整数の値を設定します。
     */
    void set_int32(const int index, const int32_t value)
    {
        values[index].kind = CPLAT_STRING_CATALOG_ARGUMENT_KIND_INT32;
        values[index].value.int32_value = value;
    }

    /**
     *  指定した位置へ文字列の値を設定します。
     */
    void set_string(const int index, const char *value)
    {
        values[index].kind = CPLAT_STRING_CATALOG_ARGUMENT_KIND_STRING;
        values[index].value.string_value = value;
    }
};

// 位置指定を持たない書式が、そのまま書き込まれることの確認
// [サブ手順参照 名前=stringCatalogRenderTest.SetUp]
TEST_F(stringCatalogRenderTest, plain_text)
{
    // Arrange
    int actual_ret;

    // Pre-Assert

    // Act
    actual_ret = cplat_internal_string_catalog_render_text(dest, sizeof(dest), "開始しました。", values,
                                                           0); // [手順] - 位置指定を含まない書式を展開する。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - cplat_internal_string_catalog_render_text() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "開始しました。",
        dest); // [確認_正常系] - dest が "開始しました。" であること。(位置指定を持たない書式がそのまま書き込まれたことの確認)
}

// 位置指定が値へ置き換わることの確認
// [サブ手順参照 名前=stringCatalogRenderTest.SetUp]
TEST_F(stringCatalogRenderTest, single_placeholder)
{
    // Arrange
    int actual_ret;
    set_int32(0, 42);

    // Pre-Assert

    // Act
    actual_ret = cplat_internal_string_catalog_render_text(dest, sizeof(dest), "値は {0} です。", values,
                                                           1); // [手順] - 位置指定を 1 個含む書式を展開する。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - cplat_internal_string_catalog_render_text() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("値は 42 です。",
                 dest); // [確認_正常系] - dest が "値は 42 です。" であること。(位置指定が値へ置き換わったことの確認)
}

// 位置指定の順序を入れ替えても、値の対応が変わらないことの確認
// [サブ手順参照 名前=stringCatalogRenderTest.SetUp]
TEST_F(stringCatalogRenderTest, reordered_placeholder)
{
    // Arrange
    int actual_ret;
    set_string(0, "config.json");
    set_int32(1, 2);

    // Pre-Assert

    // Act
    actual_ret = cplat_internal_string_catalog_render_text(dest, sizeof(dest), "{1} / {0}", values,
                                                           2); // [手順] - 位置指定を逆順に並べた書式を展開する。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - cplat_internal_string_catalog_render_text() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("2 / config.json",
                 dest); // [確認_正常系] - dest が "2 / config.json" であること。(書式の順序で値が並んだことの確認)
}

// 同じ位置指定を複数回参照できることの確認
// [サブ手順参照 名前=stringCatalogRenderTest.SetUp]
TEST_F(stringCatalogRenderTest, repeated_placeholder)
{
    // Arrange
    int actual_ret;
    set_int32(0, 7);

    // Pre-Assert

    // Act
    actual_ret = cplat_internal_string_catalog_render_text(dest, sizeof(dest), "{0}-{0}", values,
                                                           1); // [手順] - 同じ位置指定を 2 回含む書式を展開する。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - cplat_internal_string_catalog_render_text() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("7-7", dest); // [確認_正常系] - dest が "7-7" であること。(同じ値が 2 回展開されたことの確認)
}

// 波括弧のエスケープが 1 文字へ戻ることの確認
// [サブ手順参照 名前=stringCatalogRenderTest.SetUp]
TEST_F(stringCatalogRenderTest, escaped_braces)
{
    // Arrange
    int actual_ret;
    set_int32(0, 123);

    // Pre-Assert

    // Act
    actual_ret = cplat_internal_string_catalog_render_text(dest, sizeof(dest), "value={{ {0} }}", values,
                                                           1); // [手順] - エスケープと位置指定を含む書式を展開する。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - cplat_internal_string_catalog_render_text() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "value={ 123 }",
        dest); // [確認_正常系] - dest が "value={ 123 }" であること。({{ と }} が 1 文字の波括弧になったことの確認)
}

// 引数種別ごとの文字列表現の確認
// [サブ手順参照 名前=stringCatalogRenderTest.SetUp]
TEST_F(stringCatalogRenderTest, argument_kind_text)
{
    // Arrange
    static const char sample_object[] = "x";
    int actual_ret;

    // Pre-Assert

    // Act
    values[0].kind = CPLAT_STRING_CATALOG_ARGUMENT_KIND_STRING;
    values[0].value.string_value = nullptr;
    actual_ret = cplat_internal_string_catalog_render_text(dest, sizeof(dest), "{0}", values,
                                                           1); // [手順] - NULL の文字列引数を展開する。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - NULL の文字列引数を展開した cplat_internal_string_catalog_render_text() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "(null)",
        dest); // [確認_正常系] - NULL の文字列引数を展開した dest が "(null)" であること。(NULL が "(null)" と表現されたことの確認)

    // Act_2
    values[0].kind = CPLAT_STRING_CATALOG_ARGUMENT_KIND_CHAR;
    values[0].value.char_value = 'A';
    actual_ret = cplat_internal_string_catalog_render_text(dest, sizeof(dest), "{0}", values,
                                                           1); // [手順] - 印字できる文字を展開する。

    // Assert_2
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - 印字できる文字を展開した cplat_internal_string_catalog_render_text() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "'A'",
        dest); // [確認_正常系] - 印字できる文字を展開した dest が "'A'" であること。(単引用符で囲んだ 1 文字になったことの確認)

    // Act_3
    values[0].kind = CPLAT_STRING_CATALOG_ARGUMENT_KIND_CHAR;
    values[0].value.char_value = (char)(unsigned char)0x8A;
    actual_ret = cplat_internal_string_catalog_render_text(dest, sizeof(dest), "{0}", values,
                                                           1); // [手順] - 印字できない文字を展開する。

    // Assert_3
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - 印字できない文字を展開した cplat_internal_string_catalog_render_text() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "138 (0x8a)",
        dest); // [確認_正常系] - 印字できない文字を展開した dest が "138 (0x8a)" であること。(10 進数と 16 進数が併記されたことの確認)

    // Act_4
    values[0].kind = CPLAT_STRING_CATALOG_ARGUMENT_KIND_INT8;
    values[0].value.int8_value = INT8_C(-12);
    actual_ret = cplat_internal_string_catalog_render_text(dest, sizeof(dest), "{0}", values,
                                                           1); // [手順] - 符号付き 8 bit 整数を展開する。

    // Assert_4
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - 符号付き 8 bit 整数を展開した cplat_internal_string_catalog_render_text() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "-12",
        dest); // [確認_正常系] - 符号付き 8 bit 整数を展開した dest が "-12" であること。(10 進数で表現されたことの確認)

    // Act_5
    values[0].kind = CPLAT_STRING_CATALOG_ARGUMENT_KIND_UINT8;
    values[0].value.uint8_value = UINT8_C(200);
    actual_ret = cplat_internal_string_catalog_render_text(dest, sizeof(dest), "{0}", values,
                                                           1); // [手順] - 符号なし 8 bit 整数を展開する。

    // Assert_5
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - 符号なし 8 bit 整数を展開した cplat_internal_string_catalog_render_text() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "200",
        dest); // [確認_正常系] - 符号なし 8 bit 整数を展開した dest が "200" であること。(10 進数で表現されたことの確認)

    // Act_6
    values[0].kind = CPLAT_STRING_CATALOG_ARGUMENT_KIND_INT16;
    values[0].value.int16_value = INT16_C(-1200);
    actual_ret = cplat_internal_string_catalog_render_text(dest, sizeof(dest), "{0}", values,
                                                           1); // [手順] - 符号付き 16 bit 整数を展開する。

    // Assert_6
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - 符号付き 16 bit 整数を展開した cplat_internal_string_catalog_render_text() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "-1200",
        dest); // [確認_正常系] - 符号付き 16 bit 整数を展開した dest が "-1200" であること。(10 進数で表現されたことの確認)

    // Act_7
    values[0].kind = CPLAT_STRING_CATALOG_ARGUMENT_KIND_UINT16;
    values[0].value.uint16_value = UINT16_C(48000);
    actual_ret = cplat_internal_string_catalog_render_text(dest, sizeof(dest), "{0}", values,
                                                           1); // [手順] - 符号なし 16 bit 整数を展開する。

    // Assert_7
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - 符号なし 16 bit 整数を展開した cplat_internal_string_catalog_render_text() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "48000",
        dest); // [確認_正常系] - 符号なし 16 bit 整数を展開した dest が "48000" であること。(10 進数で表現されたことの確認)

    // Act_8
    values[0].kind = CPLAT_STRING_CATALOG_ARGUMENT_KIND_UINT32;
    values[0].value.uint32_value = 12U;
    actual_ret = cplat_internal_string_catalog_render_text(dest, sizeof(dest), "{0}", values,
                                                           1); // [手順] - 符号なし 32 bit 整数を展開する。

    // Assert_8
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - 符号なし 32 bit 整数を展開した cplat_internal_string_catalog_render_text() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "12",
        dest); // [確認_正常系] - 符号なし 32 bit 整数を展開した dest が "12" であること。(10 進数で表現されたことの確認)

    // Act_9
    values[0].kind = CPLAT_STRING_CATALOG_ARGUMENT_KIND_INT64;
    values[0].value.int64_value = INT64_C(-4294967296);
    actual_ret = cplat_internal_string_catalog_render_text(dest, sizeof(dest), "{0}", values,
                                                           1); // [手順] - 符号付き 64 bit 整数を展開する。

    // Assert_9
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - 符号付き 64 bit 整数を展開した cplat_internal_string_catalog_render_text() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "-4294967296",
        dest); // [確認_正常系] - 符号付き 64 bit 整数を展開した dest が "-4294967296" であること。(10 進数で表現されたことの確認)

    // Act_10
    values[0].kind = CPLAT_STRING_CATALOG_ARGUMENT_KIND_UINT64;
    values[0].value.uint64_value = UINT64_C(4294967296);
    actual_ret = cplat_internal_string_catalog_render_text(dest, sizeof(dest), "{0}", values,
                                                           1); // [手順] - 符号なし 64 bit 整数を展開する。

    // Assert_10
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - 符号なし 64 bit 整数を展開した cplat_internal_string_catalog_render_text() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "4294967296",
        dest); // [確認_正常系] - 符号なし 64 bit 整数を展開した dest が "4294967296" であること。(10 進数で表現されたことの確認)

    // Act_11
    values[0].kind = CPLAT_STRING_CATALOG_ARGUMENT_KIND_HEX8;
    values[0].value.uint8_value = UINT8_C(0x8A);
    actual_ret = cplat_internal_string_catalog_render_text(dest, sizeof(dest), "{0}", values,
                                                           1); // [手順] - 8 bit の 16 進数を展開する。

    // Assert_11
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - 8 bit の 16 進数を展開した cplat_internal_string_catalog_render_text() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "0x8a",
        dest); // [確認_正常系] - 8 bit の 16 進数を展開した dest が "0x8a" であること。(2 桁の英小文字 16 進数で表現されたことの確認)

    // Act_12
    values[0].kind = CPLAT_STRING_CATALOG_ARGUMENT_KIND_HEX16;
    values[0].value.uint16_value = UINT16_C(0xBEEF);
    actual_ret = cplat_internal_string_catalog_render_text(dest, sizeof(dest), "{0}", values,
                                                           1); // [手順] - 16 bit の 16 進数を展開する。

    // Assert_12
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - 16 bit の 16 進数を展開した cplat_internal_string_catalog_render_text() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "0xbeef",
        dest); // [確認_正常系] - 16 bit の 16 進数を展開した dest が "0xbeef" であること。(4 桁の英小文字 16 進数で表現されたことの確認)

    // Act_13
    values[0].kind = CPLAT_STRING_CATALOG_ARGUMENT_KIND_HEX32;
    values[0].value.uint32_value = 0x1234ABCDU;
    actual_ret = cplat_internal_string_catalog_render_text(dest, sizeof(dest), "{0}", values,
                                                           1); // [手順] - 32 bit の 16 進数を展開する。

    // Assert_13
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - 32 bit の 16 進数を展開した cplat_internal_string_catalog_render_text() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "0x1234abcd",
        dest); // [確認_正常系] - 32 bit の 16 進数を展開した dest が "0x1234abcd" であること。(8 桁の英小文字 16 進数で表現されたことの確認)

    // Act_14
    values[0].kind = CPLAT_STRING_CATALOG_ARGUMENT_KIND_HEX64;
    values[0].value.uint64_value = UINT64_C(0xDEADBEEF);
    actual_ret = cplat_internal_string_catalog_render_text(dest, sizeof(dest), "{0}", values,
                                                           1); // [手順] - 64 bit の 16 進数を展開する。

    // Assert_14
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - 64 bit の 16 進数を展開した cplat_internal_string_catalog_render_text() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "0x00000000deadbeef",
        dest); // [確認_正常系] - 64 bit の 16 進数を展開した dest が "0x00000000deadbeef" であること。(16 桁の英小文字 16 進数で表現されたことの確認)

    // Act_15
    values[0].kind = CPLAT_STRING_CATALOG_ARGUMENT_KIND_SIZE;
    values[0].value.size_value = (size_t)4096U;
    actual_ret = cplat_internal_string_catalog_render_text(dest, sizeof(dest), "{0}", values,
                                                           1); // [手順] - バイト数を展開する。

    // Assert_15
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - バイト数を展開した cplat_internal_string_catalog_render_text() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("4096",
                 dest); // [確認_正常系] - バイト数を展開した dest が "4096" であること。(10 進数で表現されたことの確認)

    // Act_16
    values[0].kind = CPLAT_STRING_CATALOG_ARGUMENT_KIND_SSIZE;
    values[0].value.int64_value = INT64_C(-1);
    actual_ret = cplat_internal_string_catalog_render_text(dest, sizeof(dest), "{0}", values,
                                                           1); // [手順] - 符号付きのバイト数を展開する。

    // Assert_16
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - 符号付きのバイト数を展開した cplat_internal_string_catalog_render_text() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "-1",
        dest); // [確認_正常系] - 符号付きのバイト数を展開した dest が "-1" であること。(10 進数で表現されたことの確認)

    // Act_17
    values[0].kind = CPLAT_STRING_CATALOG_ARGUMENT_KIND_POINTER;
    values[0].value.pointer_value = sample_object;
    actual_ret = cplat_internal_string_catalog_render_text(dest, sizeof(dest), "{0}", values,
                                                           1); // [手順] - ポインターを展開する。

    // Assert_17
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - ポインターを展開した cplat_internal_string_catalog_render_text() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        18U,
        strlen(
            dest)); // [確認_正常系] - strlen() の戻り値が 18U であること。(ポインターが 0x と 16 桁の 16 進数で表現されたことの確認)
    EXPECT_EQ(
        0,
        strncmp(
            dest, "0x",
            2)); // [確認_正常系] - dest と "0x" を 2 文字比較した strncmp() の戻り値が 0 であること。(ポインター表現の先頭が 0x であることの確認)

    // Act_18
    values[0].kind = CPLAT_STRING_CATALOG_ARGUMENT_KIND_DOUBLE;
    values[0].value.double_value = 12.5;
    actual_ret = cplat_internal_string_catalog_render_text(dest, sizeof(dest), "{0}", values,
                                                           1); // [手順] - 倍精度浮動小数点数を展開する。

    // Assert_18
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - 倍精度浮動小数点数を展開した cplat_internal_string_catalog_render_text() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "12.5",
        dest); // [確認_正常系] - 倍精度浮動小数点数を展開した dest が "12.5" であること。(有効桁を保つ短い表現になったことの確認)

    // Act_19
    values[0].kind = CPLAT_STRING_CATALOG_ARGUMENT_KIND_ERROR_CODE;
    values[0].value.error_code_value = 2;
    actual_ret = cplat_internal_string_catalog_render_text(dest, sizeof(dest), "{0}", values,
                                                           1); // [手順] - エラー コードを展開する。

    // Assert_19
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - エラー コードを展開した cplat_internal_string_catalog_render_text() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "2 (0x00000002)",
        dest); // [確認_正常系] - エラー コードを展開した dest が "2 (0x00000002)" であること。(10 進数と 16 進数が併記されたことの確認)
}

// 未知の引数種別が定義エラーになることの確認
// [サブ手順参照 名前=stringCatalogRenderTest.SetUp]
TEST_F(stringCatalogRenderTest, unknown_argument_kind)
{
    // Arrange
    int actual_ret;
    values[0].kind = (cplat_string_catalog_argument_kind)20;

    // Pre-Assert

    // Act
    actual_ret = cplat_internal_string_catalog_render_text(dest, sizeof(dest), "{0}", values,
                                                           1); // [手順] - 列挙に無い引数種別を持つ値を展開する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_MALFORMED_DEFINITION,
        actual_ret); // [確認_異常系] - cplat_internal_string_catalog_render_text() の戻り値が CPLAT_ERR_MALFORMED_DEFINITION であること。
}

// 容量が不足した場合に切り詰めて報告することの確認
// [サブ手順参照 名前=stringCatalogRenderTest.SetUp]
TEST_F(stringCatalogRenderTest, truncated_output)
{
    // Arrange
    char small_dest[6];
    int actual_ret;
    set_string(0, "0123456789");

    // Pre-Assert

    // Act
    actual_ret = cplat_internal_string_catalog_render_text(small_dest, sizeof(small_dest), "{0}", values,
                                                           1); // [手順] - 容量に収まらない値を展開する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_BUFFER_TOO_SMALL,
        actual_ret); // [確認_異常系] - cplat_internal_string_catalog_render_text() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_STREQ(
        "01234",
        small_dest); // [確認_異常系] - small_dest が "01234" であること。(容量まで書き込んで NUL 終端したことの確認)
}

// 容量を使い切った後の追加でも切り詰めを報告することの確認
// [サブ手順参照 名前=stringCatalogRenderTest.SetUp]
TEST_F(stringCatalogRenderTest, truncated_after_full)
{
    // Arrange
    char small_dest[4];
    int actual_ret;
    set_string(0, "abc");

    // Pre-Assert

    // Act
    actual_ret =
        cplat_internal_string_catalog_render_text(small_dest, sizeof(small_dest), "{0}def", values,
                                                  1); // [手順] - 容量を使い切った後にさらに追加される書式を展開する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_BUFFER_TOO_SMALL,
        actual_ret); // [確認_異常系] - cplat_internal_string_catalog_render_text() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_STREQ(
        "abc",
        small_dest); // [確認_異常系] - small_dest が "abc" であること。(容量まで書き込んで NUL 終端したことの確認)
}

// 容量にちょうど収まる場合に切り詰めを報告しないことの確認
// [サブ手順参照 名前=stringCatalogRenderTest.SetUp]
TEST_F(stringCatalogRenderTest, exact_fit)
{
    // Arrange
    char small_dest[4];
    int actual_ret;
    set_string(0, "abc");

    // Pre-Assert

    // Act
    actual_ret = cplat_internal_string_catalog_render_text(small_dest, sizeof(small_dest), "{0}", values,
                                                           1); // [手順] - 容量にちょうど収まる値を展開する。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - cplat_internal_string_catalog_render_text() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("abc",
                 small_dest); // [確認_正常系] - small_dest が "abc" であること。(値がすべて書き込まれたことの確認)
}

// 構文が不正な書式が定義エラーになることの確認
// [サブ手順参照 名前=stringCatalogRenderTest.SetUp]
TEST_F(stringCatalogRenderTest, invalid_format)
{
    // Arrange
    set_int32(0, 1);

    // Pre-Assert

    // Act
    int ret_closing_brace =
        cplat_internal_string_catalog_render_text(dest, sizeof(dest), "a}b", values,
                                                  1); // [手順] - 対を成さない } を含む書式を展開する。
    int ret_open_brace = cplat_internal_string_catalog_render_text(dest, sizeof(dest), "a{", values,
                                                                   1); // [手順] - { で終わる書式を展開する。
    int ret_non_numeric =
        cplat_internal_string_catalog_render_text(dest, sizeof(dest), "{abc}", values,
                                                  1); // [手順] - 数値以外のインデックスを含む書式を展開する。
    int ret_symbol =
        cplat_internal_string_catalog_render_text(dest, sizeof(dest), "{~}", values,
                                                  1); // [手順] - 数字以外の文字をインデックスに指定した書式を展開する。
    int ret_leading_zero = cplat_internal_string_catalog_render_text(
        dest, sizeof(dest), "{01}", values,
        1); // [手順] - 先行ゼロを含む 2 桁のインデックスを持つ書式を展開する。
    int ret_out_of_bounds =
        cplat_internal_string_catalog_render_text(dest, sizeof(dest), "{1}", values,
                                                  1); // [手順] - 引数個数を超えるインデックスを持つ書式を展開する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_MALFORMED_DEFINITION,
        ret_closing_brace); // [確認_異常系] - 対を成さない } を含む書式を展開した cplat_internal_string_catalog_render_text() の戻り値が CPLAT_ERR_MALFORMED_DEFINITION であること。
    EXPECT_EQ(
        CPLAT_ERR_MALFORMED_DEFINITION,
        ret_open_brace); // [確認_異常系] - { で終わる書式を展開した cplat_internal_string_catalog_render_text() の戻り値が CPLAT_ERR_MALFORMED_DEFINITION であること。
    EXPECT_EQ(
        CPLAT_ERR_MALFORMED_DEFINITION,
        ret_non_numeric); // [確認_異常系] - 数値以外のインデックスを含む書式を展開した cplat_internal_string_catalog_render_text() の戻り値が CPLAT_ERR_MALFORMED_DEFINITION であること。
    EXPECT_EQ(
        CPLAT_ERR_MALFORMED_DEFINITION,
        ret_symbol); // [確認_異常系] - 数字以外の文字をインデックスに指定した書式を展開した cplat_internal_string_catalog_render_text() の戻り値が CPLAT_ERR_MALFORMED_DEFINITION であること。
    EXPECT_EQ(
        CPLAT_ERR_MALFORMED_DEFINITION,
        ret_leading_zero); // [確認_異常系] - 先行ゼロを含む 2 桁のインデックスを持つ書式を展開した cplat_internal_string_catalog_render_text() の戻り値が CPLAT_ERR_MALFORMED_DEFINITION であること。
    EXPECT_EQ(
        CPLAT_ERR_MALFORMED_DEFINITION,
        ret_out_of_bounds); // [確認_異常系] - 引数個数を超えるインデックスを持つ書式を展開した cplat_internal_string_catalog_render_text() の戻り値が CPLAT_ERR_MALFORMED_DEFINITION であること。
}

// 2 桁のインデックスを展開できることの確認
// [サブ手順参照 名前=stringCatalogRenderTest.SetUp]
TEST_F(stringCatalogRenderTest, two_digit_placeholder)
{
    // Arrange
    char wide_dest[128];
    int index;
    int actual_ret_first;
    int actual_ret_last;

    memset(wide_dest, 0, sizeof(wide_dest));
    for (index = 0; index < CPLAT_STRING_CATALOG_ARGUMENT_MAX; index++)
    {
        set_int32(index, index);
    }

    // Pre-Assert

    // Act
    actual_ret_first = cplat_internal_string_catalog_render_text(
        wide_dest, sizeof(wide_dest), "{0}/{9}/{10}", values,
        CPLAT_STRING_CATALOG_ARGUMENT_MAX); // [手順] - 1 桁と 2 桁を混ぜて展開する。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_first); // [確認_正常系] - 1 桁と 2 桁を混ぜて展開した cplat_internal_string_catalog_render_text() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "0/9/10",
        wide_dest); // [確認_正常系] - 1 桁と 2 桁を混ぜて展開した wide_dest が "0/9/10" であること。(2 桁のインデックスが対応する値へ展開されたことの確認)

    // Act_2
    actual_ret_last = cplat_internal_string_catalog_render_text(
        wide_dest, sizeof(wide_dest), "{31}", values,
        CPLAT_STRING_CATALOG_ARGUMENT_MAX); // [手順] - 最大インデックスを展開する。

    // Assert_2
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_last); // [確認_正常系] - 最大インデックスを展開した cplat_internal_string_catalog_render_text() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "31",
        wide_dest); // [確認_正常系] - 最大インデックスを展開した wide_dest が "31" であること。(インデックス 31 の値へ展開されたことの確認)
}

// 2 桁を超えるインデックスおよび範囲外のインデックスが定義エラーになることの確認
// [サブ手順参照 名前=stringCatalogRenderTest.SetUp]
TEST_F(stringCatalogRenderTest, invalid_two_digit_placeholder)
{
    // Arrange
    int index;
    int actual_ret_three_digits;
    int actual_ret_over_count;
    int actual_ret_leading_zero;

    for (index = 0; index < CPLAT_STRING_CATALOG_ARGUMENT_MAX; index++)
    {
        set_int32(index, index);
    }

    // Pre-Assert

    // Act
    actual_ret_three_digits = cplat_internal_string_catalog_render_text(
        dest, sizeof(dest), "{100}", values,
        CPLAT_STRING_CATALOG_ARGUMENT_MAX); // [手順] - 3 桁のインデックスを展開する。
    actual_ret_over_count = cplat_internal_string_catalog_render_text(
        dest, sizeof(dest), "{50}", values,
        CPLAT_STRING_CATALOG_ARGUMENT_MAX); // [手順] - 引数個数と同じインデックスを展開する。
    actual_ret_leading_zero = cplat_internal_string_catalog_render_text(
        dest, sizeof(dest), "{09}", values,
        CPLAT_STRING_CATALOG_ARGUMENT_MAX); // [手順] - 先行ゼロを含む 2 桁のインデックスを展開する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_MALFORMED_DEFINITION,
        actual_ret_three_digits); // [確認_異常系] - 3 桁のインデックスを展開した cplat_internal_string_catalog_render_text() の戻り値が CPLAT_ERR_MALFORMED_DEFINITION であること。
    EXPECT_EQ(
        CPLAT_ERR_MALFORMED_DEFINITION,
        actual_ret_over_count); // [確認_異常系] - 引数個数と同じインデックスを展開した cplat_internal_string_catalog_render_text() の戻り値が CPLAT_ERR_MALFORMED_DEFINITION であること。
    EXPECT_EQ(
        CPLAT_ERR_MALFORMED_DEFINITION,
        actual_ret_leading_zero); // [確認_異常系] - 先行ゼロを含む 2 桁のインデックスを展開した cplat_internal_string_catalog_render_text() の戻り値が CPLAT_ERR_MALFORMED_DEFINITION であること。
}

// 自前の整数変換が、標準ライブラリの書式と境界値で一致することの確認
// [サブ手順参照 名前=stringCatalogRenderTest.SetUp]
TEST_F(stringCatalogRenderTest, integer_conversion_matches_standard_library)
{
    // Arrange
    static const int64_t signed_values[] = {
        0, 1, -1, 9, -9, 10, -10, 99, -100, 2147483647, -2147483647 - 1, INT64_MAX, INT64_MIN};
    static const uint64_t unsigned_values[] = {0U, 1U, 9U, 10U, 4294967295U, 4294967296U, UINT64_MAX};
    static const int error_values[] = {0, 2, -1, 2147483647, -2147483647 - 1};
    char actual_signed[13][sizeof(dest)];
    char expected_signed[13][128];
    char actual_unsigned[7][sizeof(dest)];
    char expected_unsigned[7][128];
    char actual_hex[7][sizeof(dest)];
    char expected_hex[7][128];
    char actual_error[5][sizeof(dest)];
    char expected_error[5][128];
    size_t index;

    // Pre-Assert

    // Act
    for (index = 0; index < (sizeof(signed_values) / sizeof(signed_values[0])); index++)
    {
        values[0].kind = CPLAT_STRING_CATALOG_ARGUMENT_KIND_INT64;
        values[0].value.int64_value = signed_values[index];
        (void)cplat_internal_string_catalog_render_text(dest, sizeof(dest), "{0}", values,
                                                        1); // [手順] - 符号付き 64 bit 整数を展開する。
        snprintf(expected_signed[index], sizeof(expected_signed[index]), "%" PRId64, signed_values[index]);

        memcpy(actual_signed[index], dest, sizeof(dest));
    }

    for (index = 0; index < (sizeof(unsigned_values) / sizeof(unsigned_values[0])); index++)
    {
        values[0].kind = CPLAT_STRING_CATALOG_ARGUMENT_KIND_UINT64;
        values[0].value.uint64_value = unsigned_values[index];
        (void)cplat_internal_string_catalog_render_text(dest, sizeof(dest), "{0}", values,
                                                        1); // [手順] - 符号なし 64 bit 整数を展開する。
        snprintf(expected_unsigned[index], sizeof(expected_unsigned[index]), "%" PRIu64, unsigned_values[index]);
        memcpy(actual_unsigned[index], dest, sizeof(dest));

        values[0].kind = CPLAT_STRING_CATALOG_ARGUMENT_KIND_HEX64;
        (void)cplat_internal_string_catalog_render_text(dest, sizeof(dest), "{0}", values,
                                                        1); // [手順] - 64 bit の 16 進数を展開する。
        snprintf(expected_hex[index], sizeof(expected_hex[index]), "0x%016" PRIx64, unsigned_values[index]);
        memcpy(actual_hex[index], dest, sizeof(dest));
    }

    for (index = 0; index < (sizeof(error_values) / sizeof(error_values[0])); index++)
    {
        values[0].kind = CPLAT_STRING_CATALOG_ARGUMENT_KIND_ERROR_CODE;
        values[0].value.error_code_value = error_values[index];
        (void)cplat_internal_string_catalog_render_text(dest, sizeof(dest), "{0}", values,
                                                        1); // [手順] - エラー コードを展開する。
        snprintf(expected_error[index], sizeof(expected_error[index]), "%d (0x%08x)", error_values[index],
                 (unsigned int)error_values[index]);
        memcpy(actual_error[index], dest, sizeof(dest));
    }

    // Assert
    for (index = 0; index < (sizeof(signed_values) / sizeof(signed_values[0])); index++)
    {
        EXPECT_STREQ(
            expected_signed[index],
            actual_signed
                [index]); // [確認_正常系 回数=13] - 符号付き 64 bit 整数を展開した actual_signed[index] が expected_signed[index] と一致すること。(自前の整数変換が標準ライブラリの 10 進表現と一致することの確認)
    }

    for (index = 0; index < (sizeof(unsigned_values) / sizeof(unsigned_values[0])); index++)
    {
        EXPECT_STREQ(
            expected_unsigned[index],
            actual_unsigned
                [index]); // [確認_正常系 回数=7] - 符号なし 64 bit 整数を展開した actual_unsigned[index] が expected_unsigned[index] と一致すること。(自前の整数変換が標準ライブラリの 10 進表現と一致することの確認)
        EXPECT_STREQ(
            expected_hex[index],
            actual_hex
                [index]); // [確認_正常系 回数=7] - 64 bit の 16 進数を展開した actual_hex[index] が expected_hex[index] と一致すること。(自前の整数変換が標準ライブラリの 16 進表現と一致することの確認)
    }

    for (index = 0; index < (sizeof(error_values) / sizeof(error_values[0])); index++)
    {
        EXPECT_STREQ(
            expected_error[index],
            actual_error
                [index]); // [確認_正常系 回数=5] - エラー コードを展開した actual_error[index] が expected_error[index] と一致すること。(10 進数と 16 進数の併記が標準ライブラリの書式と一致することの確認)
    }
}

// 値を割り当てないインデックスを参照する書式が定義エラーになることの確認
// [サブ手順参照 名前=stringCatalogRenderTest.SetUp]
TEST_F(stringCatalogRenderTest, unused_index_reference)
{
    // Arrange
    int actual_ret;

    set_int32(0, INT32_C(1));

    // Pre-Assert

    // Act
    actual_ret =
        cplat_internal_string_catalog_render_text(dest, sizeof(dest), "{0} と {1}", values,
                                                  2); // [手順] - 値を割り当てないインデックスを参照する書式を展開する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_MALFORMED_DEFINITION,
        actual_ret); // [確認_異常系] - cplat_internal_string_catalog_render_text() の戻り値が CPLAT_ERR_MALFORMED_DEFINITION であること。
}
