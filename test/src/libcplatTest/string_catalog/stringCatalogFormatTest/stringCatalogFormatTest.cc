#include <testfw.h>
#include <mock_cplat.h>

#include "fake_catalog.h"

#include <cplat/base/result.h>
#include <cplat/string_catalog/string_catalog.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/**
 *  可変長引数を組み立てて、cplat_string_catalog_vformat へ中継します。
 *
 *  `va_start` の直前の名前付き引数を持たせるため、メンバー関数ではなく通常の関数とします。
 */
static int call_vformat(char *dest, size_t dest_size, int string_key, ...)
{
    va_list args;
    int ret;

    va_start(args, string_key);
    ret = cplat_string_catalog_vformat(fake_catalog(), dest, dest_size, string_key, args);
    va_end(args);

    return ret;
}

class stringCatalogFormatTest : public Test
{
  protected:
    /** 組み立てた文字列の格納先です。 */
    char dest[64];

    // [サブ手順 名前=stringCatalogFormatTest.SetUp]
    void SetUp() override
    {
        memset(dest, 0, sizeof(dest));
        fake_catalog_reset();
        cplat_string_catalog_set_language(CPLAT_STRING_CATALOG_LANGUAGE_JAPANESE);
    }
    // [サブ手順終了]
};

// 引数を取らない文字列を組み立てられることの確認
// [サブ手順参照 名前=stringCatalogFormatTest.SetUp]
TEST_F(stringCatalogFormatTest, no_argument)
{
    // Arrange
    int actual_ret;

    // Pre-Assert

    // Act
    actual_ret =
        cplat_string_catalog_format(fake_catalog(), dest, sizeof(dest),
                                    FAKE_CATALOG_KEY_NO_ARGUMENT); // [手順] - 引数を取らない文字列を組み立てる。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret);      // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_STREQ("開始しました。", dest); // [確認_正常系] - 現在の言語の書式がそのまま組み立てられること。
}

// 言語設定の変更が語順だけを変えることの確認
// [サブ手順参照 名前=stringCatalogFormatTest.SetUp]
TEST_F(stringCatalogFormatTest, language_changes_order_only)
{
    // Arrange
    int actual_ret_japanese;
    int actual_ret_english;
    char english_dest[64];

    memset(english_dest, 0, sizeof(english_dest));

    // Pre-Assert

    // Act
    actual_ret_japanese =
        cplat_string_catalog_format(fake_catalog(), dest, sizeof(dest), FAKE_CATALOG_KEY_TWO_ARGUMENTS, "config.json",
                                    INT32_C(2)); // [手順] - 日本語のまま 2 引数の文字列を組み立てる。

    cplat_string_catalog_set_language(CPLAT_STRING_CATALOG_LANGUAGE_ENGLISH);
    actual_ret_english = cplat_string_catalog_format(fake_catalog(), english_dest, sizeof(english_dest),
                                                     FAKE_CATALOG_KEY_TWO_ARGUMENTS, "config.json",
                                                     INT32_C(2)); // [手順] - 言語を英語へ変更し、同じ引数で組み立てる。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret_japanese);                    // [確認_正常系] - 日本語の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_english);           // [確認_正常系] - 英語の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("ファイル config.json 番号 2", dest); // [確認_正常系] - 日本語では書式の語順で並ぶこと。
    EXPECT_STREQ("number 2 of config.json",
                 english_dest); // [確認_正常系] - 英語では引数順を変えずに語順だけが変わること。
}

// 言語を設定していないプロセスがニュートラル言語を使用することの確認
// [サブ手順参照 名前=stringCatalogFormatTest.SetUp]
TEST_F(stringCatalogFormatTest, neutral_language)
{
    // Arrange
    int actual_ret;
    cplat_string_catalog_set_language(CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL);

    // Pre-Assert

    // Act
    actual_ret =
        cplat_string_catalog_format(fake_catalog(), dest, sizeof(dest), FAKE_CATALOG_KEY_TWO_ARGUMENTS, "config.json",
                                    INT32_C(2)); // [手順] - ニュートラル言語で文字列を組み立てる。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret);                 // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_STREQ("file config.json number 2", dest); // [確認_正常系] - ニュートラル言語の書式で組み立てられること。
}

// va_list を受け取る API でも同じ結果になることの確認
// [サブ手順参照 名前=stringCatalogFormatTest.SetUp]
TEST_F(stringCatalogFormatTest, vformat_accepts_argument_list)
{
    // Arrange
    int actual_ret;

    // Pre-Assert

    // Act
    actual_ret = call_vformat(dest, sizeof(dest), FAKE_CATALOG_KEY_ONE_ARGUMENT,
                              (size_t)4096U); // [手順] - va_list を渡して文字列を組み立てる。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_STREQ("上限 4096", dest); // [確認_正常系] - 可変長引数と同じ結果になること。
}

// 書き込み先が NULL の場合に引数不正となることの確認
// [サブ手順参照 名前=stringCatalogFormatTest.SetUp]
TEST_F(stringCatalogFormatTest, null_dest)
{
    // Arrange
    int actual_ret;

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_format(fake_catalog(), NULL, sizeof(dest),
                                             FAKE_CATALOG_KEY_NO_ARGUMENT); // [手順] - 書き込み先へ NULL を渡す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - 戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// 書き込み先の容量が 0 の場合に引数不正となることの確認
// [サブ手順参照 名前=stringCatalogFormatTest.SetUp]
TEST_F(stringCatalogFormatTest, zero_dest_size)
{
    // Arrange
    int actual_ret;

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_format(fake_catalog(), dest, 0U,
                                             FAKE_CATALOG_KEY_NO_ARGUMENT); // [手順] - 書き込み先の容量へ 0 を渡す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - 戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// カタログに無い文字列キーを指定した場合に未検出となることの確認
// [サブ手順参照 名前=stringCatalogFormatTest.SetUp]
TEST_F(stringCatalogFormatTest, unknown_string_key)
{
    // Arrange
    int actual_ret;

    // Pre-Assert

    // Act
    actual_ret =
        cplat_string_catalog_format(fake_catalog(), dest, sizeof(dest),
                                    FAKE_CATALOG_KEY_UNKNOWN); // [手順] - カタログに登録していない文字列キーを渡す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_NOT_FOUND,
              actual_ret);  // [確認_異常系] - 戻り値が CPLAT_ERR_NOT_FOUND であること。
    EXPECT_STREQ("", dest); // [確認_異常系] - 書き込み先が NUL 終端されること。
}

// 引数個数が上限を超える定義が定義エラーになることの確認
// [サブ手順参照 名前=stringCatalogFormatTest.SetUp]
TEST_F(stringCatalogFormatTest, argument_count_over_max)
{
    // Arrange
    int actual_ret;
    fake_catalog_set_argument_count(FAKE_CATALOG_INDEX_NO_ARGUMENT, CPLAT_STRING_CATALOG_ARGUMENT_MAX + 1);

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_format(
        fake_catalog(), dest, sizeof(dest),
        FAKE_CATALOG_KEY_NO_ARGUMENT); // [手順] - 引数個数が上限を超える定義で文字列を組み立てる。

    // Assert
    EXPECT_EQ(CPLAT_ERR_MALFORMED_DEFINITION,
              actual_ret); // [確認_異常系] - 戻り値が CPLAT_ERR_MALFORMED_DEFINITION であること。
}

// 引数個数が負の定義が定義エラーになることの確認
// [サブ手順参照 名前=stringCatalogFormatTest.SetUp]
TEST_F(stringCatalogFormatTest, argument_count_negative)
{
    // Arrange
    int actual_ret;
    fake_catalog_set_argument_count(FAKE_CATALOG_INDEX_NO_ARGUMENT, -1);

    // Pre-Assert

    // Act
    actual_ret =
        cplat_string_catalog_format(fake_catalog(), dest, sizeof(dest),
                                    FAKE_CATALOG_KEY_NO_ARGUMENT); // [手順] - 引数個数が負の定義で文字列を組み立てる。

    // Assert
    EXPECT_EQ(CPLAT_ERR_MALFORMED_DEFINITION,
              actual_ret); // [確認_異常系] - 戻り値が CPLAT_ERR_MALFORMED_DEFINITION であること。
}

// 現在の言語のリソースが無い場合にニュートラル言語へフォールバックすることの確認
// [サブ手順参照 名前=stringCatalogFormatTest.SetUp]
TEST_F(stringCatalogFormatTest, falls_back_to_neutral_text)
{
    // Arrange
    int actual_ret;
    fake_catalog_set_text(FAKE_CATALOG_INDEX_NO_ARGUMENT, CPLAT_STRING_CATALOG_LANGUAGE_JAPANESE, NULL);

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_format(
        fake_catalog(), dest, sizeof(dest),
        FAKE_CATALOG_KEY_NO_ARGUMENT); // [手順] - 日本語のリソースが無い文字列を日本語で組み立てる。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_STREQ("started", dest);   // [確認_正常系] - ニュートラル言語の書式で組み立てられること。
}

// ニュートラル言語のリソースも無い場合に定義エラーになることの確認
// [サブ手順参照 名前=stringCatalogFormatTest.SetUp]
TEST_F(stringCatalogFormatTest, missing_neutral_text)
{
    // Arrange
    int actual_ret;
    fake_catalog_set_text(FAKE_CATALOG_INDEX_NO_ARGUMENT, CPLAT_STRING_CATALOG_LANGUAGE_JAPANESE, NULL);
    fake_catalog_set_text(FAKE_CATALOG_INDEX_NO_ARGUMENT, CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL, NULL);

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_format(
        fake_catalog(), dest, sizeof(dest),
        FAKE_CATALOG_KEY_NO_ARGUMENT); // [手順] - どの言語にもリソースが無い文字列を組み立てる。

    // Assert
    EXPECT_EQ(CPLAT_ERR_MALFORMED_DEFINITION,
              actual_ret); // [確認_異常系] - 戻り値が CPLAT_ERR_MALFORMED_DEFINITION であること。
}

// 備考を現在の言語で参照し、無い場合はニュートラル言語へフォールバックすることの確認
// [サブ手順参照 名前=stringCatalogFormatTest.SetUp]
TEST_F(stringCatalogFormatTest, metadata)
{
    // Arrange
    const char *actual_id;
    const char *actual_note_japanese;
    const char *actual_note_fallback;
    const char *actual_unknown_note;
    const char *actual_unknown_id;
    const cplat_string_catalog_entry *actual_entry;
    int actual_category;
    int actual_unknown_category;

    // Pre-Assert

    // Act
    actual_id = cplat_string_catalog_get_id(fake_catalog(),
                                            FAKE_CATALOG_KEY_NO_ARGUMENT); // [手順] - ID を取得する。
    actual_note_japanese = cplat_string_catalog_get_note(
        fake_catalog(), FAKE_CATALOG_KEY_NO_ARGUMENT); // [手順] - 日本語の備考を取得する。

    cplat_string_catalog_set_language(CPLAT_STRING_CATALOG_LANGUAGE_ENGLISH);
    actual_note_fallback = cplat_string_catalog_get_note(
        fake_catalog(), FAKE_CATALOG_KEY_NO_ARGUMENT); // [手順] - 英語の備考が無い状態で備考を取得する。
    actual_category =
        cplat_string_catalog_get_category(fake_catalog(), FAKE_CATALOG_KEY_NO_ARGUMENT); // [手順] - 分類値を取得する。
    actual_unknown_category = cplat_string_catalog_get_category(
        fake_catalog(), FAKE_CATALOG_KEY_UNKNOWN); // [手順] - 未登録の文字列キーで分類値を取得する。
    actual_unknown_id = cplat_string_catalog_get_id(
        fake_catalog(), FAKE_CATALOG_KEY_UNKNOWN); // [手順] - 未登録の文字列キーで ID を取得する。
    actual_unknown_note = cplat_string_catalog_get_note(
        fake_catalog(), FAKE_CATALOG_KEY_UNKNOWN); // [手順] - 未登録の文字列キーで備考を取得する。
    actual_entry = cplat_string_catalog_get_entry(
        fake_catalog(), FAKE_CATALOG_KEY_TWO_ARGUMENTS); // [手順] - 文字列キーの項目メタデータを取得する。

    // Assert
    ASSERT_NE(nullptr, actual_id);           // [確認_正常系] - ID を取得できること。
    EXPECT_STREQ("FAKE_ID_0001", actual_id); // [確認_正常系] - ID が一致すること。
    EXPECT_EQ(3, actual_category);           // [確認_正常系] - カタログの分類値をそのまま返すこと。
    EXPECT_EQ(0, actual_unknown_category);   // [確認_異常系] - 未登録の文字列キーでは 0 を返すこと。
    EXPECT_STREQ("引数を取らない文字列です。",
                 actual_note_japanese); // [確認_正常系] - 現在の言語の備考を返すこと。
    EXPECT_STREQ("no argument",
                 actual_note_fallback); // [確認_正常系] - 備考が無い言語ではニュートラル言語の備考を返すこと。
    EXPECT_EQ(nullptr,
              actual_unknown_id);                // [確認_異常系] - 未登録の文字列キーでは ID が NULL であること。
    EXPECT_EQ(nullptr, actual_unknown_note);     // [確認_異常系] - 未登録の文字列キーでは NULL を返すこと。
    ASSERT_NE(nullptr, actual_entry);            // [確認_正常系] - 項目メタデータを取得できること。
    EXPECT_STREQ("2 引数", actual_entry->brief); // [確認_正常系] - 短い説明を保持すること。
    EXPECT_STREQ("2 つの引数を取る文字列です。", actual_entry->details); // [確認_正常系] - 詳細説明を保持すること。
    ASSERT_NE(nullptr, actual_entry->arguments);                         // [確認_正常系] - 引数定義を取得できること。
    EXPECT_STREQ("path", actual_entry->arguments[0].name);               // [確認_正常系] - 引数名を保持すること。
    EXPECT_STREQ("ファイルのパス。",
                 actual_entry->arguments[0].description); // [確認_正常系] - 引数説明を保持すること。
}

// 列挙に無い引数種別を持つ定義が定義エラーになることの確認
// [サブ手順参照 名前=stringCatalogFormatTest.SetUp]
TEST_F(stringCatalogFormatTest, unknown_argument_kind)
{
    // Arrange
    int actual_ret;
    fake_catalog_set_argument_kind(FAKE_CATALOG_INDEX_ONE_ARGUMENT, 0, (cplat_string_catalog_argument_kind)20);

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_format(fake_catalog(), dest, sizeof(dest), FAKE_CATALOG_KEY_ONE_ARGUMENT,
                                             (size_t)1U); // [手順] - 列挙に無い引数種別を持つ定義で文字列を組み立てる。

    // Assert
    EXPECT_EQ(CPLAT_ERR_MALFORMED_DEFINITION,
              actual_ret); // [確認_異常系] - 戻り値が CPLAT_ERR_MALFORMED_DEFINITION であること。
}

// 書き込み先に収まらない場合に切り詰めを報告することの確認
// [サブ手順参照 名前=stringCatalogFormatTest.SetUp]
TEST_F(stringCatalogFormatTest, truncated_output)
{
    // Arrange
    char small_dest[8];
    int actual_ret;

    memset(small_dest, 0, sizeof(small_dest));
    cplat_string_catalog_set_language(CPLAT_STRING_CATALOG_LANGUAGE_ENGLISH);

    // Pre-Assert

    // Act
    actual_ret =
        cplat_string_catalog_format(fake_catalog(), small_dest, sizeof(small_dest), FAKE_CATALOG_KEY_ONE_ARGUMENT,
                                    (size_t)4096U); // [手順] - 結果が収まらない容量で文字列を組み立てる。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              actual_ret);               // [確認_異常系] - 戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_STREQ("limit 4", small_dest); // [確認_異常系] - 容量まで書き込んで NUL 終端すること。
}
