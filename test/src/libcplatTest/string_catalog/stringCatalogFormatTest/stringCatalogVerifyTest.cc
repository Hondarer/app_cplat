#include <testfw.h>

#include "fake_catalog.h"

#include <cplat/base/result.h>
#include <cplat/string_catalog/string_catalog.h>

class stringCatalogVerifyTest : public Test
{
  protected:
    /** 不正を検出した文字列キーの受け取り先です。 */
    int string_key;

    /** 不正を検出した言語の受け取り先です。 */
    cplat_string_catalog_language language;

    // [サブ手順 名前=stringCatalogVerifyTest.SetUp]
    void SetUp() override
    {
        string_key = FAKE_CATALOG_KEY_UNKNOWN;
        language = CPLAT_STRING_CATALOG_LANGUAGE_ENGLISH;
        fake_catalog_reset();
    }
    // [サブ手順終了]
};

// 整合したカタログが受理されることの確認
// [サブ手順参照 名前=stringCatalogVerifyTest.SetUp]
TEST_F(stringCatalogVerifyTest, valid_catalog)
{
    // Arrange
    int actual_ret;

    // Pre-Assert

    // Act
    actual_ret =
        cplat_string_catalog_verify(fake_catalog(), &string_key, &language); // [手順] - 既定のカタログを確認する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
}

// 必須の項目メタデータが未設定のカタログが拒否されることの確認
// [サブ手順参照 名前=stringCatalogVerifyTest.SetUp]
TEST_F(stringCatalogVerifyTest, missing_entry_metadata)
{
    // Arrange
    int actual_ret;
    fake_catalog_set_brief(FAKE_CATALOG_INDEX_TWO_ARGUMENTS, NULL);

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_verify(fake_catalog(), &string_key,
                                             &language); // [手順] - 短い説明が未設定のカタログを確認する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_MALFORMED_DEFINITION,
              actual_ret); // [確認_異常系] - 必須メタデータの欠落を定義エラーとして通知すること。
    EXPECT_EQ(FAKE_CATALOG_KEY_TWO_ARGUMENTS,
              string_key); // [確認_異常系] - 不正を検出した文字列キーを報告すること。
}

// 詳細説明が未設定でもカタログが受理されることの確認
// [サブ手順参照 名前=stringCatalogVerifyTest.SetUp]
TEST_F(stringCatalogVerifyTest, missing_details_is_allowed)
{
    // Arrange
    int actual_ret;
    fake_catalog_set_details(FAKE_CATALOG_INDEX_TWO_ARGUMENTS, NULL);

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_verify(fake_catalog(), &string_key,
                                             &language); // [手順] - 詳細説明が未設定のカタログを確認する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 詳細説明の欠落を許容すること。
    EXPECT_EQ(FAKE_CATALOG_KEY_UNKNOWN,
              string_key); // [確認_正常系] - 不正がないため文字列キーを変更しないこと。
}

// ID が未設定のカタログを許容することの確認
// [サブ手順参照 名前=stringCatalogVerifyTest.SetUp]
TEST_F(stringCatalogVerifyTest, missing_id_is_allowed)
{
    // Arrange
    int actual_ret;
    fake_catalog_set_id(FAKE_CATALOG_INDEX_TWO_ARGUMENTS, NULL);

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_verify(fake_catalog(), &string_key,
                                             &language); // [手順] - ID が未設定のカタログを確認する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - ID の欠落を許容すること。
    EXPECT_EQ(FAKE_CATALOG_KEY_UNKNOWN,
              string_key); // [確認_正常系] - 不正がないため文字列キーを変更しないこと。
}

// 引数定義のメタデータが未設定のカタログが拒否されることの確認
// [サブ手順参照 名前=stringCatalogVerifyTest.SetUp]
TEST_F(stringCatalogVerifyTest, missing_argument_metadata)
{
    // Arrange
    int actual_ret;
    fake_catalog_set_argument_name(FAKE_CATALOG_INDEX_TWO_ARGUMENTS, 0, NULL);

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_verify(fake_catalog(), &string_key,
                                             &language); // [手順] - 引数名が未設定のカタログを確認する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_MALFORMED_DEFINITION,
              actual_ret); // [確認_異常系] - 引数メタデータの欠落を定義エラーとして通知すること。
    EXPECT_EQ(FAKE_CATALOG_KEY_TWO_ARGUMENTS,
              string_key); // [確認_異常系] - 不正を検出した文字列キーを報告すること。
}

// 引数説明が未設定のカタログが拒否されることの確認
// [サブ手順参照 名前=stringCatalogVerifyTest.SetUp]
TEST_F(stringCatalogVerifyTest, missing_argument_description)
{
    // Arrange
    int actual_ret;
    fake_catalog_set_argument_description(FAKE_CATALOG_INDEX_TWO_ARGUMENTS, 0, NULL);

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_verify(fake_catalog(), &string_key,
                                             &language); // [手順] - 引数説明が未設定のカタログを確認する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_MALFORMED_DEFINITION,
              actual_ret); // [確認_異常系] - 引数説明の欠落を定義エラーとして通知すること。
    EXPECT_EQ(FAKE_CATALOG_KEY_TWO_ARGUMENTS,
              string_key); // [確認_異常系] - 不正を検出した文字列キーを報告すること。
}

// 引数定義配列が未設定のカタログが拒否されることの確認
// [サブ手順参照 名前=stringCatalogVerifyTest.SetUp]
TEST_F(stringCatalogVerifyTest, missing_argument_array)
{
    // Arrange
    int actual_ret;
    fake_catalog_set_arguments(FAKE_CATALOG_INDEX_TWO_ARGUMENTS, NULL);

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_verify(fake_catalog(), &string_key,
                                             &language); // [手順] - 引数定義配列が未設定のカタログを確認する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_MALFORMED_DEFINITION,
              actual_ret); // [確認_異常系] - 引数定義配列の欠落を定義エラーとして通知すること。
    EXPECT_EQ(FAKE_CATALOG_KEY_TWO_ARGUMENTS,
              string_key); // [確認_異常系] - 不正を検出した文字列キーを報告すること。
}

// 書式の構文が不正なカタログが拒否されることの確認
// [サブ手順参照 名前=stringCatalogVerifyTest.SetUp]
TEST_F(stringCatalogVerifyTest, invalid_format)
{
    // Arrange
    int actual_ret;
    fake_catalog_set_text(FAKE_CATALOG_INDEX_ONE_ARGUMENT, CPLAT_STRING_CATALOG_LANGUAGE_ENGLISH, "limit {2}");

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_verify(fake_catalog(), &string_key,
                                             &language); // [手順] - 引数個数を超える位置指定を持つカタログを確認する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_MALFORMED_DEFINITION,
              actual_ret); // [確認_異常系] - 戻り値が CPLAT_ERR_MALFORMED_DEFINITION であること。
    EXPECT_EQ(FAKE_CATALOG_KEY_ONE_ARGUMENT,
              string_key); // [確認_異常系] - 不正を検出した文字列キーを報告すること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_LANGUAGE_ENGLISH, language); // [確認_異常系] - 不正を検出した言語を報告すること。
}

// 引数を割り当てないインデックスを参照する書式が拒否されることの確認
// [サブ手順参照 名前=stringCatalogVerifyTest.SetUp]
TEST_F(stringCatalogVerifyTest, unused_argument_index_in_format)
{
    // Arrange
    int actual_ret;
    fake_catalog_set_argument_kind(FAKE_CATALOG_INDEX_ONE_ARGUMENT, 0, CPLAT_STRING_CATALOG_ARGUMENT_KIND_UNUSED);

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_verify(fake_catalog(), &string_key,
                                             &language); // [手順] - 未使用のインデックスを参照するカタログを確認する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_MALFORMED_DEFINITION,
              actual_ret); // [確認_異常系] - 戻り値が CPLAT_ERR_MALFORMED_DEFINITION であること。
    EXPECT_EQ(FAKE_CATALOG_KEY_ONE_ARGUMENT,
              string_key); // [確認_異常系] - 不正を検出した文字列キーを報告すること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL, language); // [確認_異常系] - 不正を検出した言語を報告すること。
}

// 値を受け取らない引数に名前と説明が無くても受理されることの確認
// [サブ手順参照 名前=stringCatalogVerifyTest.SetUp]
TEST_F(stringCatalogVerifyTest, unused_argument_needs_no_metadata)
{
    // Arrange
    int actual_ret;
    fake_catalog_set_argument_kind(FAKE_CATALOG_INDEX_TWO_ARGUMENTS, 1, CPLAT_STRING_CATALOG_ARGUMENT_KIND_UNUSED);
    fake_catalog_set_argument_name(FAKE_CATALOG_INDEX_TWO_ARGUMENTS, 1, NULL);
    fake_catalog_set_argument_description(FAKE_CATALOG_INDEX_TWO_ARGUMENTS, 1, NULL);
    fake_catalog_set_text(FAKE_CATALOG_INDEX_TWO_ARGUMENTS, CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL, "path {0}");
    fake_catalog_set_text(FAKE_CATALOG_INDEX_TWO_ARGUMENTS, CPLAT_STRING_CATALOG_LANGUAGE_JAPANESE, "パス {0}");
    fake_catalog_set_text(FAKE_CATALOG_INDEX_TWO_ARGUMENTS, CPLAT_STRING_CATALOG_LANGUAGE_ENGLISH, "path {0}");

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_verify(
        fake_catalog(), &string_key,
        &language); // [手順] - 名前と説明が未設定の、値を受け取らない引数を持つカタログを確認する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 値を受け取らない引数には名前と説明を求めないこと。
}

// 引数を割り当てないインデックスを参照しない書式が受理されることの確認
// [サブ手順参照 名前=stringCatalogVerifyTest.SetUp]
TEST_F(stringCatalogVerifyTest, unused_argument_kind_is_allowed)
{
    // Arrange
    int actual_ret;
    fake_catalog_set_argument_kind(FAKE_CATALOG_INDEX_TWO_ARGUMENTS, 1, CPLAT_STRING_CATALOG_ARGUMENT_KIND_UNUSED);
    fake_catalog_set_text(FAKE_CATALOG_INDEX_TWO_ARGUMENTS, CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL, "path {0}");
    fake_catalog_set_text(FAKE_CATALOG_INDEX_TWO_ARGUMENTS, CPLAT_STRING_CATALOG_LANGUAGE_ENGLISH, "path {0}");
    fake_catalog_set_text(FAKE_CATALOG_INDEX_TWO_ARGUMENTS, CPLAT_STRING_CATALOG_LANGUAGE_JAPANESE, "パス {0}");

    // Pre-Assert

    // Act
    actual_ret =
        cplat_string_catalog_verify(fake_catalog(), &string_key,
                                    &language); // [手順] - 未使用のインデックスを参照しないカタログを確認する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 未使用のインデックスがあっても受理すること。
}

// ニュートラル言語以外のリソースが未定義であっても受理されることの確認
// [サブ手順参照 名前=stringCatalogVerifyTest.SetUp]
TEST_F(stringCatalogVerifyTest, missing_localized_text_is_allowed)
{
    // Arrange
    int actual_ret;
    fake_catalog_set_text(FAKE_CATALOG_INDEX_TWO_ARGUMENTS, CPLAT_STRING_CATALOG_LANGUAGE_JAPANESE, NULL);

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_verify(fake_catalog(), &string_key,
                                             &language); // [手順] - 日本語のリソースが未設定のカタログを確認する。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret); // [確認_正常系] - ニュートラル言語へフォールバックされるため、不正としないこと。
}

// ニュートラル言語の書式が未設定のカタログが拒否されることの確認
// [サブ手順参照 名前=stringCatalogVerifyTest.SetUp]
TEST_F(stringCatalogVerifyTest, missing_neutral_text)
{
    // Arrange
    int actual_ret;
    fake_catalog_set_text(FAKE_CATALOG_INDEX_TWO_ARGUMENTS, CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL, NULL);

    // Pre-Assert

    // Act
    actual_ret =
        cplat_string_catalog_verify(fake_catalog(), &string_key,
                                    &language); // [手順] - ニュートラル言語の書式が未設定のカタログを確認する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_MALFORMED_DEFINITION,
              actual_ret); // [確認_異常系] - 戻り値が CPLAT_ERR_MALFORMED_DEFINITION であること。
    EXPECT_EQ(FAKE_CATALOG_KEY_TWO_ARGUMENTS,
              string_key); // [確認_異常系] - 不正を検出した文字列キーを報告すること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL, language); // [確認_異常系] - 不正を検出した言語を報告すること。
}

// ニュートラル言語の備考が未設定のカタログが拒否されることの確認
// [サブ手順参照 名前=stringCatalogVerifyTest.SetUp]
TEST_F(stringCatalogVerifyTest, missing_neutral_note)
{
    // Arrange
    int actual_ret;
    fake_catalog_set_note(FAKE_CATALOG_INDEX_TWO_ARGUMENTS, CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL, NULL);

    // Pre-Assert

    // Act
    actual_ret =
        cplat_string_catalog_verify(fake_catalog(), &string_key,
                                    &language); // [手順] - ニュートラル言語の備考が未設定のカタログを確認する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_MALFORMED_DEFINITION,
              actual_ret); // [確認_異常系] - 戻り値が CPLAT_ERR_MALFORMED_DEFINITION であること。
    EXPECT_EQ(FAKE_CATALOG_KEY_TWO_ARGUMENTS,
              string_key); // [確認_異常系] - 不正を検出した文字列キーを報告すること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL, language); // [確認_異常系] - 不正を検出した言語を報告すること。
}

// 引数個数が上限を超えたカタログが拒否されることの確認
// [サブ手順参照 名前=stringCatalogVerifyTest.SetUp]
TEST_F(stringCatalogVerifyTest, argument_count_over_max)
{
    // Arrange
    int actual_ret;
    fake_catalog_set_argument_count(FAKE_CATALOG_INDEX_TWO_ARGUMENTS, CPLAT_STRING_CATALOG_ARGUMENT_MAX + 1);

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_verify(fake_catalog(), &string_key,
                                             &language); // [手順] - 引数個数が上限を超えるカタログを確認する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_MALFORMED_DEFINITION,
              actual_ret); // [確認_異常系] - 戻り値が CPLAT_ERR_MALFORMED_DEFINITION であること。
    EXPECT_EQ(FAKE_CATALOG_KEY_TWO_ARGUMENTS,
              string_key); // [確認_異常系] - 不正を検出した文字列キーを報告すること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_LANGUAGE_COUNT,
              language); // [確認_異常系] - 言語に依らない不正として、言語ではない値を報告すること。
}

// 文字列キーが重複したカタログが拒否されることの確認
// [サブ手順参照 名前=stringCatalogVerifyTest.SetUp]
TEST_F(stringCatalogVerifyTest, duplicated_string_key)
{
    // Arrange
    int actual_ret;
    fake_catalog_set_key(FAKE_CATALOG_INDEX_TWO_ARGUMENTS, FAKE_CATALOG_KEY_NO_ARGUMENT);

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_verify(fake_catalog(), &string_key,
                                             &language); // [手順] - 文字列キーが重複したカタログを確認する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_MALFORMED_DEFINITION,
              actual_ret); // [確認_異常系] - 検索が自分自身へ到達しないため、定義エラーを返すこと。
    EXPECT_EQ(FAKE_CATALOG_KEY_NO_ARGUMENT, string_key); // [確認_異常系] - 不正を検出した文字列キーを報告すること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_LANGUAGE_COUNT,
              language); // [確認_異常系] - 言語に依らない不正として、言語ではない値を報告すること。
}

// ID が重複したカタログを許容することの確認
// [サブ手順参照 名前=stringCatalogVerifyTest.SetUp]
TEST_F(stringCatalogVerifyTest, duplicated_id_is_allowed)
{
    // Arrange
    int actual_ret;
    fake_catalog_set_id(FAKE_CATALOG_INDEX_TWO_ARGUMENTS, "FAKE_ID_0001");

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_verify(fake_catalog(), &string_key,
                                             &language); // [手順] - ID が重複したカタログを確認する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - ID の重複を許容すること。
    EXPECT_EQ(FAKE_CATALOG_KEY_UNKNOWN,
              string_key); // [確認_正常系] - 不正がないため文字列キーを変更しないこと。
}

// 引数個数が負のカタログが拒否されることの確認
// [サブ手順参照 名前=stringCatalogVerifyTest.SetUp]
TEST_F(stringCatalogVerifyTest, argument_count_negative)
{
    // Arrange
    int actual_ret;
    fake_catalog_set_argument_count(FAKE_CATALOG_INDEX_TWO_ARGUMENTS, -1);

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_verify(fake_catalog(), &string_key,
                                             &language); // [手順] - 引数個数が負のカタログを確認する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_MALFORMED_DEFINITION,
              actual_ret); // [確認_異常系] - 戻り値が CPLAT_ERR_MALFORMED_DEFINITION であること。
}

// 出力引数を省略しても結果コードを返すことの確認
// [サブ手順参照 名前=stringCatalogVerifyTest.SetUp]
TEST_F(stringCatalogVerifyTest, omitted_output_arguments)
{
    // Arrange
    int actual_ret_count;
    int actual_ret_text;

    // Pre-Assert

    // Act
    fake_catalog_set_argument_count(FAKE_CATALOG_INDEX_TWO_ARGUMENTS, CPLAT_STRING_CATALOG_ARGUMENT_MAX + 1);
    actual_ret_count =
        cplat_string_catalog_verify(fake_catalog(), NULL,
                                    NULL); // [手順] - 出力引数を省略して、引数個数が不正なカタログを確認する。

    fake_catalog_reset();
    fake_catalog_set_text(FAKE_CATALOG_INDEX_TWO_ARGUMENTS, CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL, NULL);
    actual_ret_text = cplat_string_catalog_verify(
        fake_catalog(), NULL,
        NULL); // [手順] - 出力引数を省略して、ニュートラル言語の書式が未設定のカタログを確認する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_MALFORMED_DEFINITION,
              actual_ret_count); // [確認_異常系] - 引数個数の不正を報告すること。
    EXPECT_EQ(CPLAT_ERR_MALFORMED_DEFINITION,
              actual_ret_text); // [確認_異常系] - 書式の欠落を報告すること。
}
