#include <testfw.h>
#include <mock_cplat.h>

#include <cplat/base/result.h>
#include <cplat/locale/ui_language.h>
#include <cplat/string_catalog/language_internal.h>
#include <cplat/string_catalog/string_catalog.h>

#include <stddef.h>
#include <string.h>

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;

namespace
{

/**
 *  @brief          表示言語の取得が、指定した言語タグを返すようにします。
 *  @param[in,out]  mock  設定する mock。
 *  @param[in]      tag   返す言語タグ。
 */
// [サブ手順 名前=stringCatalogLanguageTest.expect_ui_language_tag]
void expect_ui_language_tag(NiceMock<Mock_cplat> &mock, const char *const tag)
{
    EXPECT_CALL(mock, cplat_ui_language_get_tag(_, _))
        .WillRepeatedly(Invoke(
            [tag](char *tag_out, size_t tag_size)
            {
                const size_t length = strlen(tag);

                if (tag_out == nullptr || tag_size <= length)
                {
                    return CPLAT_ERR_BUFFER_TOO_SMALL;
                }
                memcpy(tag_out, tag, length + 1U);
                return CPLAT_OK;
            }));
    // [Pre-Assert手順] - 指定した表示言語タグを出力バッファーへコピーする。
    // [Pre-Assert確認_正常系] - 表示言語の取得で cplat_ui_language_get_tag() が呼び出されること。
}
// [サブ手順終了]

} // namespace

// 出力言語はプロセス グローバルな状態のため、テストごとに決定前の状態へ戻す
class stringCatalogLanguageTest : public Test
{
  protected:
    // [サブ手順 名前=stringCatalogLanguageTest.SetUp]
    void SetUp() override
    {
        cplat_internal_string_catalog_language_reset_for_test();
    }
    // [サブ手順終了]

    // [サブ手順 名前=stringCatalogLanguageTest.TearDown]
    void TearDown() override
    {
        cplat_internal_string_catalog_language_reset_for_test();
    }
    // [サブ手順終了]
};

// 設定していないプロセスが、実行環境の表示言語を出力言語とすることの確認
// [サブ手順参照 名前=stringCatalogLanguageTest.SetUp]
TEST_F(stringCatalogLanguageTest, decides_from_environment)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_string_catalog_language actual_language;

    // Pre-Assert
    // [サブ手順参照 名前=stringCatalogLanguageTest.expect_ui_language_tag]
    expect_ui_language_tag(mock_cplat, "ja-JP"); // [Pre-Assert手順] - 表示言語として ja-JP を返す状態にする。
    // 表示言語を取得する呼び出し期待を満たすこと。

    // Act
    actual_language = cplat_string_catalog_get_language(); // [手順] - 設定を行わずに現在の言語を取得する。

    // Assert
    EXPECT_EQ(CPLAT_STRING_CATALOG_LANGUAGE_JAPANESE,
              actual_language); // [確認_正常系] - 実行環境の表示言語に対応する日本語となること。
}
// [サブ手順参照 名前=stringCatalogLanguageTest.TearDown]

// 対応する言語が存在しない表示言語で、ニュートラル言語となることの確認
// [サブ手順参照 名前=stringCatalogLanguageTest.SetUp]
TEST_F(stringCatalogLanguageTest, decides_neutral_for_unknown_tag)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_string_catalog_language actual_language;

    // Pre-Assert
    // [サブ手順参照 名前=stringCatalogLanguageTest.expect_ui_language_tag]
    expect_ui_language_tag(mock_cplat, "fr-FR"); // [Pre-Assert手順] - 扱わない言語の表示言語を返す状態にする。
    // 表示言語を取得する呼び出し期待を満たすこと。

    // Act
    actual_language = cplat_string_catalog_get_language(); // [手順] - 設定を行わずに現在の言語を取得する。

    // Assert
    EXPECT_EQ(CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL,
              actual_language); // [確認_正常系] - 対応する言語が存在しない場合にニュートラル言語となること。
}
// [サブ手順参照 名前=stringCatalogLanguageTest.TearDown]

// 表示言語がニュートラルの場合に、ニュートラル言語となることの確認
// [サブ手順参照 名前=stringCatalogLanguageTest.SetUp]
TEST_F(stringCatalogLanguageTest, decides_neutral_for_neutral_tag)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_string_catalog_language actual_language;

    // Pre-Assert
    // [サブ手順参照 名前=stringCatalogLanguageTest.expect_ui_language_tag]
    expect_ui_language_tag(mock_cplat, ""); // [Pre-Assert手順] - 表示言語としてニュートラルを返す状態にする。
    // 表示言語を取得する呼び出し期待を満たすこと。

    // Act
    actual_language = cplat_string_catalog_get_language(); // [手順] - 設定を行わずに現在の言語を取得する。

    // Assert
    EXPECT_EQ(CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL,
              actual_language); // [確認_正常系] - ニュートラル言語となること。
}
// [サブ手順参照 名前=stringCatalogLanguageTest.TearDown]

// 実行環境からの決定が 1 回だけ行われることの確認
// [サブ手順参照 名前=stringCatalogLanguageTest.SetUp]
TEST_F(stringCatalogLanguageTest, decides_environment_only_once)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_string_catalog_language actual_language_first;
    cplat_string_catalog_language actual_language_second;

    // Pre-Assert
    // [サブ手順参照 名前=stringCatalogLanguageTest.expect_ui_language_tag]
    expect_ui_language_tag(mock_cplat, "ja"); // [Pre-Assert手順] - 表示言語として ja を返す状態にする。
    // 表示言語を取得する呼び出し期待を満たすこと。

    // Act
    actual_language_first = cplat_string_catalog_get_language();  // [手順] - 現在の言語を取得する。
    // [サブ手順参照 名前=stringCatalogLanguageTest.expect_ui_language_tag]
    expect_ui_language_tag(mock_cplat, "en");                     // [手順] - 表示言語として en を返す状態へ変更する。
    // 表示言語を取得する呼び出し期待を満たすこと。
    actual_language_second = cplat_string_catalog_get_language(); // [手順] - 現在の言語を再度取得する。

    // Assert
    EXPECT_EQ(CPLAT_STRING_CATALOG_LANGUAGE_JAPANESE,
              actual_language_first); // [確認_正常系] - 最初の取得で日本語となること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_LANGUAGE_JAPANESE,
              actual_language_second); // [確認_正常系] - 表示言語の変更後も決定済みの日本語を返すこと。
}
// [サブ手順参照 名前=stringCatalogLanguageTest.TearDown]

// 明示的な設定が実行環境より優先されることの確認
// [サブ手順参照 名前=stringCatalogLanguageTest.SetUp]
TEST_F(stringCatalogLanguageTest, explicit_setting_precedes_environment)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    int actual_ret;
    cplat_string_catalog_language actual_language;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_ui_language_get_tag(_, _))
        .Times(0); // [Pre-Assert確認_正常系] - 表示言語の取得が呼び出されないこと。

    // Act
    actual_ret = cplat_string_catalog_set_language(
        CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL);            // [手順] - ニュートラル言語を明示的に設定する。
    actual_language = cplat_string_catalog_get_language(); // [手順] - 現在の言語を取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - ニュートラル言語の設定が成功すること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL,
              actual_language); // [確認_正常系] - 明示的に設定したニュートラル言語が実行環境で上書きされないこと。
}
// [サブ手順参照 名前=stringCatalogLanguageTest.TearDown]

// 設定した言語を取得できることの確認
// [サブ手順参照 名前=stringCatalogLanguageTest.SetUp]
TEST_F(stringCatalogLanguageTest, set_and_get)
{
    // Arrange
    int actual_ret_japanese;
    int actual_ret_english;
    cplat_string_catalog_language actual_language_japanese;
    cplat_string_catalog_language actual_language_english;

    // Pre-Assert

    // Act
    actual_ret_japanese =
        cplat_string_catalog_set_language(CPLAT_STRING_CATALOG_LANGUAGE_JAPANESE); // [手順] - 日本語を設定する。
    actual_language_japanese = cplat_string_catalog_get_language();                // [手順] - 現在の言語を取得する。
    actual_ret_english =
        cplat_string_catalog_set_language(CPLAT_STRING_CATALOG_LANGUAGE_ENGLISH); // [手順] - 英語を設定する。
    actual_language_english = cplat_string_catalog_get_language();                // [手順] - 現在の言語を取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_japanese); // [確認_正常系] - 日本語の設定が成功すること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_LANGUAGE_JAPANESE,
              actual_language_japanese);     // [確認_正常系] - 設定した日本語を取得できること。
    EXPECT_EQ(CPLAT_OK, actual_ret_english); // [確認_正常系] - 英語の設定が成功すること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_LANGUAGE_ENGLISH,
              actual_language_english); // [確認_正常系] - 設定した英語を取得できること。
}
// [サブ手順参照 名前=stringCatalogLanguageTest.TearDown]

// 範囲外の言語を設定できないことの確認
// [サブ手順参照 名前=stringCatalogLanguageTest.SetUp]
TEST_F(stringCatalogLanguageTest, invalid_language)
{
    // Arrange
    int actual_ret;
    cplat_string_catalog_language actual_language;

    cplat_string_catalog_set_language(CPLAT_STRING_CATALOG_LANGUAGE_JAPANESE);

    // Pre-Assert

    // Act
    actual_ret =
        cplat_string_catalog_set_language(CPLAT_STRING_CATALOG_LANGUAGE_COUNT); // [手順] - 言語ではない値を設定する。
    actual_language = cplat_string_catalog_get_language();                      // [手順] - 現在の言語を取得する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - 戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_LANGUAGE_JAPANESE, actual_language); // [確認_異常系] - 設定が変更されないこと。
}
// [サブ手順参照 名前=stringCatalogLanguageTest.TearDown]

// 言語タグから扱う言語へ対応付けられることの確認
// [サブ手順参照 名前=stringCatalogLanguageTest.SetUp]
TEST_F(stringCatalogLanguageTest, language_from_tag)
{
    // Arrange
    int actual_ret_japanese;
    int actual_ret_english;
    int actual_ret_upper;
    cplat_string_catalog_language actual_language_japanese = CPLAT_STRING_CATALOG_LANGUAGE_COUNT;
    cplat_string_catalog_language actual_language_english = CPLAT_STRING_CATALOG_LANGUAGE_COUNT;
    cplat_string_catalog_language actual_language_upper = CPLAT_STRING_CATALOG_LANGUAGE_COUNT;

    // Pre-Assert

    // Act
    actual_ret_japanese = cplat_string_catalog_language_from_tag(
        "ja-JP", &actual_language_japanese); // [手順] - 地域を含む日本語の言語タグを対応付ける。
    actual_ret_english = cplat_string_catalog_language_from_tag(
        "en", &actual_language_english); // [手順] - 言語だけの英語の言語タグを対応付ける。
    actual_ret_upper =
        cplat_string_catalog_language_from_tag("JA", &actual_language_upper); // [手順] - 大文字の言語タグを対応付ける。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_japanese); // [確認_正常系] - 日本語の言語タグで戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_LANGUAGE_JAPANESE,
              actual_language_japanese);     // [確認_正常系] - 地域を含む言語タグが日本語へ対応付けられること。
    EXPECT_EQ(CPLAT_OK, actual_ret_english); // [確認_正常系] - 英語の言語タグで戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_LANGUAGE_ENGLISH,
              actual_language_english);    // [確認_正常系] - 言語だけの言語タグが英語へ対応付けられること。
    EXPECT_EQ(CPLAT_OK, actual_ret_upper); // [確認_正常系] - 大文字の言語タグで戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_LANGUAGE_JAPANESE,
              actual_language_upper); // [確認_正常系] - 大文字の言語タグが日本語へ対応付けられること。
}
// [サブ手順参照 名前=stringCatalogLanguageTest.TearDown]

// 対応する言語が存在しない言語タグの扱いの確認
// [サブ手順参照 名前=stringCatalogLanguageTest.SetUp]
TEST_F(stringCatalogLanguageTest, language_from_tag_without_match)
{
    // Arrange
    int actual_ret_unknown;
    int actual_ret_neutral;
    cplat_string_catalog_language actual_language_unknown = CPLAT_STRING_CATALOG_LANGUAGE_COUNT;
    cplat_string_catalog_language actual_language_neutral = CPLAT_STRING_CATALOG_LANGUAGE_COUNT;

    // Pre-Assert

    // Act
    actual_ret_unknown = cplat_string_catalog_language_from_tag(
        "fr-FR", &actual_language_unknown); // [手順] - 扱わない言語の言語タグを対応付ける。
    actual_ret_neutral = cplat_string_catalog_language_from_tag(
        "", &actual_language_neutral); // [手順] - ニュートラルを表す空文字列を対応付ける。

    // Assert
    EXPECT_EQ(CPLAT_ERR_NOT_FOUND,
              actual_ret_unknown); // [確認_異常系] - 扱わない言語の戻り値が CPLAT_ERR_NOT_FOUND であること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL,
              actual_language_unknown);      // [確認_異常系] - 扱わない言語でニュートラル言語が格納されること。
    EXPECT_EQ(CPLAT_OK, actual_ret_neutral); // [確認_正常系] - 空文字列の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL,
              actual_language_neutral); // [確認_正常系] - 空文字列でニュートラル言語が格納されること。
}
// [サブ手順参照 名前=stringCatalogLanguageTest.TearDown]

// 言語タグの対応付けが不正な引数を拒否することの確認
// [サブ手順参照 名前=stringCatalogLanguageTest.SetUp]
TEST_F(stringCatalogLanguageTest, language_from_tag_invalid_argument)
{
    // Arrange
    int actual_ret_null_tag;
    int actual_ret_null_language;
    cplat_string_catalog_language actual_language = CPLAT_STRING_CATALOG_LANGUAGE_COUNT;

    // Pre-Assert

    // Act
    actual_ret_null_tag =
        cplat_string_catalog_language_from_tag(NULL, &actual_language);            // [手順] - 言語タグに NULL を渡す。
    actual_ret_null_language = cplat_string_catalog_language_from_tag("ja", NULL); // [手順] - 格納先に NULL を渡す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_tag); // [確認_異常系] - 言語タグが NULL の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_language); // [確認_異常系] - 格納先が NULL の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_LANGUAGE_COUNT,
              actual_language); // [確認_異常系] - 言語タグが NULL の場合に格納先を変更しないこと。
}
// [サブ手順参照 名前=stringCatalogLanguageTest.TearDown]
