#include <testfw.h>

#include "filterTestSupport.h"

#include "gen/filter_test_mixed.h"

#include <cplat/base/result.h>
#include <cplat/string_catalog/string_catalog.h>

#include <cstring>

using namespace filter_test;

class stringCatalogFilterCheckTest : public Test
{
  protected:
    void SetUp() override
    {
        saved_language_ = cplat_string_catalog_get_language();
        ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_create(
                                filter_test_mixed_catalog(), filter_test_mixed_key_names(),
                                filter_test_mixed_key_name_count(), nullptr, kLineCapacity, kLineWidth, &slot_));
        // [状態確認] - `cplat_string_catalog_filter_slot_create( filter_test_mixed_catalog(), filter_test_mixed_key_names(), filter_test_mixed_key_name_count(), nullptr, kLineCapacity, kLineWidth, &slot_)` の戻り値が `CPLAT_OK` であること。
    }

    void TearDown() override
    {
        cplat_string_catalog_filter_slot_dispose(&slot_);
        (void)cplat_string_catalog_set_language(saved_language_);
    }

    /** 1 行の条件式をコンパイルし、確認関数で警告を得ます。Arrange と Act の共通処理です。 */
    int check(const char *expression)
    {
        static unsigned char image[kImageSize];

        if (compile_single_line(expression, image) != CPLAT_OK)
        {
            return CPLAT_ERR_UNKNOWN;
        }
        return cplat_string_catalog_filter_slot_check(slot_, image, kImageSize, diagnostics_, 4U, &invalid_count_,
                                                      warnings_, 8U, &warning_count_);
    }

    /** 1 行の条件式を適用し、その行の説明文を得ます。 */
    int describe(const char *expression, const cplat_string_catalog_language language)
    {
        static unsigned char image[kImageSize];

        if ((compile_single_line(expression, image) != CPLAT_OK) ||
            (cplat_string_catalog_filter_slot_apply(slot_, image, kImageSize, nullptr, 0U, nullptr) != CPLAT_OK))
        {
            return CPLAT_ERR_UNKNOWN;
        }
        (void)cplat_string_catalog_set_language(language);
        return cplat_string_catalog_filter_slot_describe_line(slot_, 0U, description_, sizeof(description_));
    }

    cplat_string_catalog_filter_slot *slot_ = nullptr;
    cplat_string_catalog_filter_diagnostic diagnostics_[4] = {};
    cplat_string_catalog_filter_warning warnings_[8] = {};
    std::size_t invalid_count_ = 0U;
    std::size_t warning_count_ = 0U;
    cplat_string_catalog_language saved_language_ = CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL;
    int pad_ = 0; /**< 明示的アラインメントです。 */
    char description_[1024] = {0};
};

// 数値と比較する引数が、文字列の項目と数値の項目に分かれる場合、混在と型の不一致を警告することの確認
TEST_F(stringCatalogFilterCheckTest, numeric_comparison_warns_mixed_and_type_mismatch)
{
    // Arrange
    int actual_ret;

    // Pre-Assert

    // Act
    actual_ret = check("arg.value == 5"); // [手順] - 文字列、整数、ポインターに分かれる value を数値と比較する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret);  // [確認_正常系] - 警告だけなら CPLAT_OK を返すこと。
    EXPECT_EQ(0U, invalid_count_);    // [確認_正常系] - 行を無効にしないこと。
    ASSERT_EQ(2U, warning_count_);    // [確認_正常系] - 混在と型の不一致の 2 件であること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_WARNING_MIXED_ARGUMENT_TYPES,
              warnings_[0].kind);     // [確認_正常系] - 1 件目が型区分の混在であること。
    EXPECT_EQ(0U, warnings_[0].line_index);      // [確認_正常系] - 行の位置が 0 であること。
    EXPECT_EQ(0U, warnings_[0].predicate_index); // [確認_正常系] - 比較要素の位置が 0 であること。
    EXPECT_EQ(FILTER_TEST_MIXED_KEY_TEXT_VALUE,
              warnings_[0].string_key); // [確認_正常系] - 文字列の代表が TEXT_VALUE であること。
    EXPECT_EQ(0, warnings_[0].argument_index); // [確認_正常系] - 文字列の代表の引数の位置が 0 であること。
    EXPECT_EQ(FILTER_TEST_MIXED_KEY_NUMBER_VALUE,
              warnings_[0].other_string_key); // [確認_正常系] - 文字列以外の代表が NUMBER_VALUE であること。
    EXPECT_EQ(0, warnings_[0].other_argument_index); // [確認_正常系] - 文字列以外の代表の引数の位置が 0 であること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_WARNING_TYPE_MISMATCH,
              warnings_[1].kind); // [確認_正常系] - 2 件目が型の不一致であること。
    EXPECT_EQ(FILTER_TEST_MIXED_KEY_TEXT_VALUE,
              warnings_[1].string_key); // [確認_正常系] - 型が合わないのは文字列の項目だけであること。
    EXPECT_EQ(0, warnings_[1].argument_index);        // [確認_正常系] - 引数の位置が 0 であること。
    EXPECT_EQ(-1, warnings_[1].other_argument_index); // [確認_正常系] - もう一方の引数は使用しないこと。
}

// null との比較では、ポインターと文字列は比較でき、整数の項目だけが型の不一致になることの確認
TEST_F(stringCatalogFilterCheckTest, null_comparison_warns_only_integer_entry)
{
    // Arrange
    int actual_ret;

    // Pre-Assert

    // Act
    actual_ret = check("arg.value == null"); // [手順] - value を null と比較する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - CPLAT_OK を返すこと。
    ASSERT_EQ(2U, warning_count_);   // [確認_正常系] - 混在と型の不一致の 2 件であること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_WARNING_MIXED_ARGUMENT_TYPES,
              warnings_[0].kind); // [確認_正常系] - 1 件目が型区分の混在であること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_WARNING_TYPE_MISMATCH,
              warnings_[1].kind); // [確認_正常系] - 2 件目が型の不一致であること。
    EXPECT_EQ(FILTER_TEST_MIXED_KEY_NUMBER_VALUE,
              warnings_[1].string_key); // [確認_正常系] - 型が合わないのは整数の項目だけであること。
}

// 同じ引数名を複数の比較要素で参照しても、混在の警告は最初の比較要素で 1 回だけであることの確認
TEST_F(stringCatalogFilterCheckTest, mixed_warning_is_reported_once_per_name)
{
    // Arrange
    int actual_ret;

    // Pre-Assert

    // Act
    actual_ret = check("arg.value == 5 || arg.value == \"x\""); // [手順] - value を数値と文字列の両方と比較する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - CPLAT_OK を返すこと。
    ASSERT_EQ(4U, warning_count_);   // [確認_正常系] - 混在 1 件と型の不一致 3 件であること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_WARNING_MIXED_ARGUMENT_TYPES,
              warnings_[0].kind); // [確認_正常系] - 混在の警告が先頭にあること。
    EXPECT_EQ(0U, warnings_[0].predicate_index); // [確認_正常系] - 混在は最初の比較要素で警告すること。
    EXPECT_EQ(FILTER_TEST_MIXED_KEY_TEXT_VALUE,
              warnings_[1].string_key); // [確認_正常系] - 数値との比較では文字列の項目が合わないこと。
    EXPECT_EQ(0U, warnings_[1].predicate_index); // [確認_正常系] - 1 つ目の比較要素の警告であること。
    EXPECT_EQ(FILTER_TEST_MIXED_KEY_NUMBER_VALUE,
              warnings_[2].string_key); // [確認_正常系] - 文字列との比較では整数の項目が合わないこと。
    EXPECT_EQ(1U, warnings_[2].predicate_index); // [確認_正常系] - 2 つ目の比較要素の警告であること。
    EXPECT_EQ(FILTER_TEST_MIXED_KEY_POINTER_VALUE,
              warnings_[3].string_key); // [確認_正常系] - 文字列との比較ではポインターの項目も合わないこと。
    EXPECT_EQ(1U, warnings_[3].predicate_index); // [確認_正常系] - 2 つ目の比較要素の警告であること。
}

// 成立し得ない行は無効にする診断を返し、その原因の型の不一致も警告することの確認
TEST_F(stringCatalogFilterCheckTest, never_satisfiable_line_is_diagnosed_and_warned)
{
    // Arrange
    int actual_ret;

    // Pre-Assert

    // Act
    actual_ret = check("arg.count starts_with \"1\""); // [手順] - 整数だけの count を文字列で判定する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - CPLAT_OK を返すこと。
    ASSERT_EQ(1U, invalid_count_);   // [確認_正常系] - 行を無効にすること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NEVER_SATISFIABLE,
              diagnostics_[0].error); // [確認_正常系] - 原因が成立し得ない条件であること。
    ASSERT_EQ(2U, warning_count_);   // [確認_正常系] - count を持つ 2 項目の型の不一致であること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_WARNING_TYPE_MISMATCH,
              warnings_[0].kind); // [確認_正常系] - 混在ではなく型の不一致であること。
    EXPECT_EQ(FILTER_TEST_MIXED_KEY_TEXT_VALUE, warnings_[0].string_key);    // [確認_正常系] - 1 項目目であること。
    EXPECT_EQ(1, warnings_[0].argument_index);                              // [確認_正常系] - count の位置であること。
    EXPECT_EQ(FILTER_TEST_MIXED_KEY_NUMBER_VALUE, warnings_[1].string_key); // [確認_正常系] - 2 項目目であること。
}

// has と、インデックスで指定した引数は、混在の警告の対象にならないことの確認
TEST_F(stringCatalogFilterCheckTest, has_and_index_reference_are_not_warned_as_mixed)
{
    // Arrange
    int actual_has_ret;
    std::size_t actual_has_count;
    int actual_index_ret;

    // Pre-Assert

    // Act
    actual_has_ret = check("has(arg.value)"); // [手順] - value の有無だけを判定する。
    actual_has_count = warning_count_;
    actual_index_ret = check("arg[0] == 5"); // [手順] - 0 番目の引数を数値と比較する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_has_ret);  // [確認_正常系] - CPLAT_OK を返すこと。
    EXPECT_EQ(0U, actual_has_count);      // [確認_正常系] - has は警告しないこと。
    EXPECT_EQ(CPLAT_OK, actual_index_ret); // [確認_正常系] - CPLAT_OK を返すこと。
    ASSERT_EQ(1U, warning_count_);         // [確認_正常系] - 型の不一致の 1 件だけであること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_WARNING_TYPE_MISMATCH,
              warnings_[0].kind); // [確認_正常系] - インデックスの参照は混在として警告しないこと。
    EXPECT_EQ(FILTER_TEST_MIXED_KEY_TEXT_VALUE, warnings_[0].string_key); // [確認_正常系] - 文字列の項目であること。
}

// 警告の格納先の容量を超える場合も総数を返し、容量までを格納することの確認
TEST_F(stringCatalogFilterCheckTest, warning_count_exceeding_capacity_is_reported)
{
    // Arrange
    static unsigned char image[kImageSize];
    cplat_string_catalog_filter_warning actual_warnings[2] = {};
    std::size_t actual_count = 0U;
    int actual_ret;

    ASSERT_EQ(CPLAT_OK, compile_single_line("arg.value == 5 || arg.value == \"x\"",
                                            image)); // [状態] - 警告が 4 件になる条件式をコンパイルする。
    // [状態確認] - `compile_single_line("arg.value == 5 || arg.value == \"x\"", image)` の戻り値が `CPLAT_OK` であること。
    actual_warnings[1].kind = CPLAT_STRING_CATALOG_FILTER_WARNING_NONE;

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_filter_slot_check(slot_, image, kImageSize, nullptr, 0U, nullptr,
                                                        actual_warnings, 1U,
                                                        &actual_count); // [手順] - 容量 1 で確認する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - CPLAT_OK を返すこと。
    EXPECT_EQ(4U, actual_count);     // [確認_正常系] - 容量にかかわらず総数を返すこと。
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_WARNING_MIXED_ARGUMENT_TYPES,
              actual_warnings[0].kind); // [確認_正常系] - 容量までを格納すること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_WARNING_NONE,
              actual_warnings[1].kind); // [確認_正常系] - 容量を超えて書き込まないこと。
}

// 確認は適用中の条件を変えないことの確認
TEST_F(stringCatalogFilterCheckTest, check_keeps_applied_conditions)
{
    // Arrange
    static unsigned char applied[kImageSize];
    cplat_string_catalog_filter_state actual_number_state;
    cplat_string_catalog_filter_state actual_text_state;
    int actual_ret;

    ASSERT_EQ(CPLAT_OK, compile_single_line("key == FILTER_TEST_MIXED_KEY_NUMBER_VALUE",
                                            applied)); // [状態] - NUMBER_VALUE だけに一致する条件式をコンパイルする。
    // [状態確認] - `compile_single_line("key == FILTER_TEST_MIXED_KEY_NUMBER_VALUE", applied)` の戻り値が `CPLAT_OK` であること。
    ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_apply(slot_, applied, kImageSize, nullptr, 0U,
                                                               nullptr)); // [状態] - 条件式を適用する。
    // [状態確認] - `cplat_string_catalog_filter_slot_apply(slot_, applied, kImageSize, nullptr, 0U, nullptr)` の戻り値が `CPLAT_OK` であること。

    // Pre-Assert

    // Act
    actual_ret = check("key == FILTER_TEST_MIXED_KEY_TEXT_VALUE"); // [手順] - 別の条件式を確認する。
    (void)cplat_string_catalog_filter_slot_test(slot_, FILTER_TEST_MIXED_KEY_NUMBER_VALUE,
                                                &actual_number_state); // [手順] - 適用中の状態を取得する。
    (void)cplat_string_catalog_filter_slot_test(slot_, FILTER_TEST_MIXED_KEY_TEXT_VALUE,
                                                &actual_text_state); // [手順] - 適用中の状態を取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - CPLAT_OK を返すこと。
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_STATE_ALWAYS_MATCH,
              actual_number_state); // [確認_正常系] - 適用中の条件が維持されること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_STATE_NEVER_MATCH,
              actual_text_state); // [確認_正常系] - 確認した条件は判定に使われないこと。
}

// 壊れたフィルター オブジェクトは適用と同じ結果コードを返し、警告の総数を書き換えないことの確認
TEST_F(stringCatalogFilterCheckTest, corrupt_image_is_rejected)
{
    // Arrange
    static unsigned char image[kImageSize];
    std::size_t actual_count = 99U;
    int actual_ret;

    std::memset(image, 0, sizeof(image)); // [状態] - フィルター オブジェクトではない領域を用意する。

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_filter_slot_check(slot_, image, kImageSize, nullptr, 0U, nullptr, nullptr, 0U,
                                                        &actual_count); // [手順] - 壊れた領域を確認する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR, actual_ret); // [確認_異常系] - 検証の失敗を返すこと。
    EXPECT_EQ(99U, actual_count);                        // [確認_異常系] - 総数を書き換えないこと。
}

// NULL の引数を拒否することの確認
TEST_F(stringCatalogFilterCheckTest, null_arguments_are_rejected)
{
    // Arrange
    static unsigned char image[kImageSize];

    ASSERT_EQ(CPLAT_OK, compile_single_line("arg.value == 5", image)); // [状態] - 条件式をコンパイルする。
    // [状態確認] - `compile_single_line("arg.value == 5", image)` の戻り値が `CPLAT_OK` であること。

    // Pre-Assert

    // Act & Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              cplat_string_catalog_filter_slot_check(nullptr, image, kImageSize, nullptr, 0U, nullptr, nullptr, 0U,
                                                     nullptr)); // [確認_異常系] - スロットが NULL なら拒否すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              cplat_string_catalog_filter_slot_check(slot_, nullptr, kImageSize, nullptr, 0U, nullptr, nullptr, 0U,
                                                     nullptr)); // [確認_異常系] - 領域が NULL なら拒否すること。
}

// 型区分が混在する引数は、日本語の説明文の末尾に注記されることの確認
TEST_F(stringCatalogFilterCheckTest, describe_notes_mixed_argument_in_japanese)
{
    // Arrange
    int actual_ret;

    // Pre-Assert

    // Act
    actual_ret = describe("arg.value == 5 && arg.value != 6",
                          CPLAT_STRING_CATALOG_LANGUAGE_JAPANESE); // [手順] - value を 2 回比較する条件式を説明する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - CPLAT_OK を返すこと。
    EXPECT_STREQ("引数 value が 5 である かつ 引数 value が 6 ではない"
                 " (注意: 引数 value はカタログ内に文字列型と数値型が混在しているため、"
                 "意図した判定結果にならない可能性があります)",
                 description_); // [確認_正常系] - 同じ名前の注記は 1 回だけ末尾に付くこと。
}

// 型区分が混在する引数は、ニュートラル言語の説明文の末尾に注記されることの確認
TEST_F(stringCatalogFilterCheckTest, describe_notes_mixed_argument_in_neutral)
{
    // Arrange
    int actual_ret;

    // Pre-Assert

    // Act
    actual_ret = describe("arg.value == null", CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL); // [手順] - 条件式を説明する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - CPLAT_OK を返すこと。
    EXPECT_NE(nullptr, std::strstr(description_, " (Note: argument value has both string and numeric types in the "
                                                 "catalog; the result may not be as intended.)"))
        << description_; // [確認_正常系] - ニュートラル言語の注記が付くこと。
}

// 型区分が混在しない引数と、has だけで参照する引数には注記しないことの確認
TEST_F(stringCatalogFilterCheckTest, describe_does_not_note_unmixed_or_has_only_argument)
{
    // Arrange
    int actual_count_ret;
    bool actual_count_noted;
    int actual_has_ret;

    // Pre-Assert

    // Act
    actual_count_ret = describe("arg.count == 1", CPLAT_STRING_CATALOG_LANGUAGE_JAPANESE); // [手順] - count を説明する。
    actual_count_noted = (std::strstr(description_, "注意") != nullptr);
    actual_has_ret = describe("has(arg.value)", CPLAT_STRING_CATALOG_LANGUAGE_JAPANESE); // [手順] - has を説明する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_count_ret); // [確認_正常系] - CPLAT_OK を返すこと。
    EXPECT_FALSE(actual_count_noted);      // [確認_正常系] - 整数だけの count には注記しないこと。
    EXPECT_EQ(CPLAT_OK, actual_has_ret);   // [確認_正常系] - CPLAT_OK を返すこと。
    EXPECT_EQ(nullptr, std::strstr(description_, "注意"))
        << description_; // [確認_正常系] - has だけで参照する value には注記しないこと。
}
