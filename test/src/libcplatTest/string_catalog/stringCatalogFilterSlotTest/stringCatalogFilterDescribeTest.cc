#include <testfw.h>

#include "filterTestSupport.h"

#include "gen/filter_test_trace.h"

#include <cplat/base/result.h>
#include <cplat/string_catalog/string_catalog.h>

#include <cstring>

using namespace filter_test;

class stringCatalogFilterDescribeTest : public Test
{
  protected:
    // [サブ手順 名前=stringCatalogFilterDescribeTest.SetUp]
    void SetUp() override
    {
        saved_language_ = cplat_string_catalog_get_language();
        ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_create(
                                filter_test_trace_catalog(), filter_test_trace_key_names(),
                                filter_test_trace_key_name_count(), nullptr, kLineCapacity, kLineWidth, &slot_));
        // [状態確認] - `cplat_string_catalog_filter_slot_create( filter_test_trace_catalog(), filter_test_trace_key_names(), filter_test_trace_key_name_count(), nullptr, kLineCapacity, kLineWidth, &slot_)` の戻り値が `CPLAT_OK` であること。
    }
    // [サブ手順終了]

    // [サブ手順 名前=stringCatalogFilterDescribeTest.TearDown]
    void TearDown() override
    {
        cplat_string_catalog_filter_slot_dispose(&slot_);
        (void)cplat_string_catalog_set_language(saved_language_);
    }
    // [サブ手順終了]

    /** 分類値の名前を指定して、スロットを作り直します。分類値の名前は作成時にだけ指定できるためです。 */
    int recreate_slot_with_category_names(const cplat_string_catalog_filter_category_names *category_names)
    {
        cplat_string_catalog_filter_slot_dispose(&slot_);
        return cplat_string_catalog_filter_slot_create(filter_test_trace_catalog(), filter_test_trace_key_names(),
                                                       filter_test_trace_key_name_count(), category_names,
                                                       kLineCapacity, kLineWidth, &slot_);
    }

    /** 1 行の条件式を適用し、その行の説明文を得ます。 */
    int describe(const char *expression, const cplat_string_catalog_language language)
    {
        static unsigned char image[kImageSize];

        if (compile_single_line(expression, image) != CPLAT_OK)
        {
            return CPLAT_ERR_UNKNOWN;
        }
        if (cplat_string_catalog_filter_slot_apply(slot_, image, kImageSize, nullptr, 0U, nullptr) != CPLAT_OK)
        {
            return CPLAT_ERR_UNKNOWN;
        }
        (void)cplat_string_catalog_set_language(language);
        return cplat_string_catalog_filter_slot_describe_line(slot_, 0U, description_, sizeof(description_));
    }

    cplat_string_catalog_filter_slot *slot_ = nullptr;
    cplat_string_catalog_language saved_language_ = CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL;
    int pad_ = 0; /**< 明示的アラインメントです。 */
    char description_[1024] = {0};
};

// 文字列キーの一致が、項目の brief と ID で表されることの確認
// [サブ手順参照 名前=stringCatalogFilterDescribeTest.SetUp]
TEST_F(stringCatalogFilterDescribeTest, key_equal_is_described_with_brief_and_id)
{
    // Arrange
    int actual_ret;

    // Pre-Assert

    // Act
    actual_ret = describe("key == FILTER_TEST_TRACE_KEY_JOB_PROGRESS",
                          CPLAT_STRING_CATALOG_LANGUAGE_JAPANESE); // [手順] - 文字列キーの一致を日本語で説明する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 説明文を得られること。
    EXPECT_STREQ("「ジョブの進捗」(FILTER_TEST_TRACE_ID_0003)",
                 description_); // [確認_正常系] - brief の句点を除き、ID を添えた表現になること。
}
// [サブ手順参照 名前=stringCatalogFilterDescribeTest.TearDown]

// 行が 1 つの項目に限定される場合、その項目の引数の説明が使われることの確認
// [サブ手順参照 名前=stringCatalogFilterDescribeTest.SetUp]
TEST_F(stringCatalogFilterDescribeTest, single_entry_line_uses_argument_description)
{
    // Arrange
    int actual_ret;

    // Pre-Assert

    // Act
    actual_ret =
        describe("arg.buffer == null",
                 CPLAT_STRING_CATALOG_LANGUAGE_JAPANESE); // [手順] - BUFFER_ALLOCATED だけが持つ引数の条件を説明する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 説明文を得られること。
    EXPECT_STREQ("引数 buffer (確保したバッファーのアドレス。確保に失敗した場合は NULL です) が NULL である",
                 description_); // [確認_正常系] - 引数の説明と null の文型で表されること。
}
// [サブ手順参照 名前=stringCatalogFilterDescribeTest.TearDown]

// 複数の項目が対象でも、説明が一致する引数は説明付きで表されることの確認
// [サブ手順参照 名前=stringCatalogFilterDescribeTest.SetUp]
TEST_F(stringCatalogFilterDescribeTest, shared_argument_description_is_used_for_multiple_entries)
{
    // Arrange
    int actual_ret;

    // Pre-Assert

    // Act
    actual_ret = describe(
        "arg[46] between 90 and 99",
        CPLAT_STRING_CATALOG_LANGUAGE_JAPANESE); // [手順] - 全項目が持つ app 定義のコンテキスト引数を説明する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 説明文を得られること。
    EXPECT_STREQ("引数 {46} sequence_number (出力ごとに 1 から順に増加し、上限に達すると 1 へ戻るラウンド トリップ ID) "
                 "が 90 以上 99 以下",
                 description_); // [確認_正常系] - インデックス、共通の名前、共通の説明で表されること。
}
// [サブ手順参照 名前=stringCatalogFilterDescribeTest.TearDown]

// 否定と、論理積の中の論理和に括弧が付くことの確認
// [サブ手順参照 名前=stringCatalogFilterDescribeTest.SetUp]
TEST_F(stringCatalogFilterDescribeTest, not_and_nested_or_are_parenthesized)
{
    // Arrange
    int actual_ret;

    // Pre-Assert

    // Act
    actual_ret =
        describe("!(category <= 2) && (key == 99 || id ends_with \"0002\")",
                 CPLAT_STRING_CATALOG_LANGUAGE_JAPANESE); // [手順] - 否定と、括弧で囲んだ論理和を含む条件を説明する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 説明文を得られること。
    EXPECT_STREQ("(分類値が 2 以下) ではない かつ (文字列キー 99、または ID が \"0002\" で終わる)",
                 description_); // [確認_正常系] - 否定の文型と、優先順位を保つ括弧で表されること。
}
// [サブ手順参照 名前=stringCatalogFilterDescribeTest.TearDown]

// 日本語以外の出力言語では、ニュートラル言語の文型が使われることの確認
// [サブ手順参照 名前=stringCatalogFilterDescribeTest.SetUp]
TEST_F(stringCatalogFilterDescribeTest, neutral_language_uses_neutral_phrases)
{
    // Arrange
    int actual_ret_neutral;
    char actual_neutral[1024];
    int actual_ret_english;

    // Pre-Assert

    // Act
    actual_ret_neutral = describe("category <= 2 || arg.job_name contains_i \"exp\"",
                                  CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL); // [手順] - ニュートラル言語で説明する。
    std::memcpy(actual_neutral, description_, std::strlen(description_) + 1U);
    actual_ret_english = describe("category <= 2 || arg.job_name contains_i \"exp\"",
                                  CPLAT_STRING_CATALOG_LANGUAGE_ENGLISH); // [手順] - 英語で説明する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_neutral); // [確認_正常系] - ニュートラル言語で説明文を得られること。
    EXPECT_STREQ("the category is less than or equal to 2 or argument job_name (ジョブの名前) contains \"exp\" "
                 "(case-insensitive)",
                 actual_neutral);               // [確認_正常系] - ニュートラル言語の文型で表されること。
    EXPECT_EQ(CPLAT_OK, actual_ret_english);    // [確認_正常系] - 英語で説明文を得られること。
    EXPECT_STREQ(actual_neutral, description_); // [確認_正常系] - 英語はニュートラル言語の文型を使うこと。
}
// [サブ手順参照 名前=stringCatalogFilterDescribeTest.TearDown]

// 名前を解決できずに無効とした行は、説明文を作らないことの確認
// [サブ手順参照 名前=stringCatalogFilterDescribeTest.SetUp]
TEST_F(stringCatalogFilterDescribeTest, disabled_line_is_not_described)
{
    // Arrange
    int actual_ret;

    // Pre-Assert

    // Act
    actual_ret = describe("key == NO_SUCH_KEY",
                          CPLAT_STRING_CATALOG_LANGUAGE_JAPANESE); // [手順] - 未解決のキー名を含む行を説明する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_MALFORMED_DEFINITION, actual_ret); // [確認_異常系] - 無効な行として扱われること。
    EXPECT_STREQ("", description_);                        // [確認_異常系] - 出力先が空文字列であること。
}
// [サブ手順参照 名前=stringCatalogFilterDescribeTest.TearDown]

// 出力先が小さい場合は、切り詰めて NUL 終端することの確認
// [サブ手順参照 名前=stringCatalogFilterDescribeTest.SetUp]
TEST_F(stringCatalogFilterDescribeTest, small_buffer_is_truncated)
{
    // Arrange
    static unsigned char image[kImageSize];
    char actual_text[8];
    int actual_ret;

    ASSERT_EQ(CPLAT_OK, compile_single_line("category <= 2", image)); // [状態] - 条件式をコンパイルする。
    // [状態確認] - `compile_single_line("category <= 2", image)` の戻り値が `CPLAT_OK` であること。
    ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_apply(slot_, image, kImageSize, nullptr, 0U,
                                                               nullptr)); // [状態] - 適用する。
    // [状態確認] - `cplat_string_catalog_filter_slot_apply(slot_, image, kImageSize, nullptr, 0U, nullptr)` の戻り値が `CPLAT_OK` であること。
    (void)cplat_string_catalog_set_language(CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL);

    // Pre-Assert

    // Act
    actual_ret =
        cplat_string_catalog_filter_slot_describe_line(slot_, 0U, actual_text,
                                                       sizeof(actual_text)); // [手順] - 8 バイトの出力先で説明する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL, actual_ret); // [確認_異常系] - 切り詰めを報告すること。
    EXPECT_STREQ("the cat", actual_text);              // [確認_異常系] - 7 バイトで切り詰め、NUL 終端すること。
}
// [サブ手順参照 名前=stringCatalogFilterDescribeTest.TearDown]

namespace
{
/** 分類値をトレース レベルとして扱う場合の名前です。 */
const char *const s_test_level_names[] = {"CRITICAL", "ERROR", "WARNING", "INFO", "VERBOSE", "DEBUG", "NONE"};

const cplat_string_catalog_filter_category_names s_test_category_names = {
    s_test_level_names,
    sizeof(s_test_level_names) / sizeof(s_test_level_names[0]),
    "レベル",
    "the level",
};
} // namespace

// 分類値の名前を設定した場合、条件を満たす名前を列挙して表すことの確認
// [サブ手順参照 名前=stringCatalogFilterDescribeTest.SetUp]
TEST_F(stringCatalogFilterDescribeTest, category_names_list_matching_names)
{
    // Arrange
    int actual_ret;

    ASSERT_EQ(CPLAT_OK, recreate_slot_with_category_names(&s_test_category_names)); // [状態] - レベルの名前を設定する。
    // [状態確認] - `recreate_slot_with_category_names(&s_test_category_names)` の戻り値が `CPLAT_OK` であること。

    // Pre-Assert

    // Act
    actual_ret =
        describe("category <= 2", CPLAT_STRING_CATALOG_LANGUAGE_JAPANESE); // [手順] - 分類値の比較を日本語で説明する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 説明文を得られること。
    EXPECT_STREQ("レベルが CRITICAL、ERROR、WARNING のいずれか",
                 description_); // [確認_正常系] - 条件を満たすレベルの名前が列挙されること。
}
// [サブ手順参照 名前=stringCatalogFilterDescribeTest.TearDown]

// 満たさない名前のほうが少ない場合は、補集合を「以外」で表すことの確認
// [サブ手順参照 名前=stringCatalogFilterDescribeTest.SetUp]
TEST_F(stringCatalogFilterDescribeTest, category_names_use_complement_when_shorter)
{
    // Arrange
    int actual_ret_japanese;
    char actual_japanese[1024];
    int actual_ret_neutral;

    ASSERT_EQ(CPLAT_OK, recreate_slot_with_category_names(&s_test_category_names)); // [状態] - レベルの名前を設定する。
    // [状態確認] - `recreate_slot_with_category_names(&s_test_category_names)` の戻り値が `CPLAT_OK` であること。

    // Pre-Assert

    // Act
    actual_ret_japanese =
        describe("category != 4", CPLAT_STRING_CATALOG_LANGUAGE_JAPANESE); // [手順] - 日本語で説明する。
    std::memcpy(actual_japanese, description_, std::strlen(description_) + 1U);
    actual_ret_neutral =
        describe("category != 4", CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL); // [手順] - ニュートラル言語で説明する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_japanese);               // [確認_正常系] - 日本語の説明文を得られること。
    EXPECT_STREQ("レベルが VERBOSE 以外", actual_japanese); // [確認_正常系] - 補集合の VERBOSE を「以外」で表すこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_neutral);                // [確認_正常系] - ニュートラル言語の説明文を得られること。
    EXPECT_STREQ("the level is other than VERBOSE",
                 description_); // [確認_正常系] - ニュートラル言語でも補集合で表すこと。
}
// [サブ手順参照 名前=stringCatalogFilterDescribeTest.TearDown]

// 1 つだけ、すべて、該当なしの場合にそれぞれ適切に表現されることの確認
// [サブ手順参照 名前=stringCatalogFilterDescribeTest.SetUp]
TEST_F(stringCatalogFilterDescribeTest, category_names_describe_single_any_and_none)
{
    // Arrange
    int actual_ret_single;
    char actual_single[1024];
    int actual_ret_any;
    char actual_any[1024];
    int actual_ret_none;

    ASSERT_EQ(CPLAT_OK, recreate_slot_with_category_names(&s_test_category_names)); // [状態] - レベルの名前を設定する。
    // [状態確認] - `recreate_slot_with_category_names(&s_test_category_names)` の戻り値が `CPLAT_OK` であること。

    // Pre-Assert

    // Act
    actual_ret_single = describe(
        "category == 3", CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL); // [手順] - 1 つの名前だけが該当する条件を説明する。
    std::memcpy(actual_single, description_, std::strlen(description_) + 1U);
    actual_ret_any = describe(
        "category >= 0", CPLAT_STRING_CATALOG_LANGUAGE_JAPANESE); // [手順] - すべての名前が該当する条件を説明する。
    std::memcpy(actual_any, description_, std::strlen(description_) + 1U);
    actual_ret_none =
        describe("category < 0 || key == FILTER_TEST_TRACE_KEY_JOB_FAILED",
                 CPLAT_STRING_CATALOG_LANGUAGE_JAPANESE); // [手順] - どの名前も該当しない判定要素を含む条件を説明する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_single);           // [確認_正常系] - 説明文を得られること。
    EXPECT_STREQ("the level is INFO", actual_single); // [確認_正常系] - 1 つの名前で表すこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_any);              // [確認_正常系] - 説明文を得られること。
    EXPECT_STREQ("レベルを問わない", actual_any);     // [確認_正常系] - すべての名前が該当することを表すこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_none);             // [確認_正常系] - 説明文を得られること。
    EXPECT_NE(nullptr, std::strstr(description_, "レベルがいずれにも該当しない"))
        << description_; // [確認_正常系] - 該当なしを表すこと。
}
// [サブ手順参照 名前=stringCatalogFilterDescribeTest.TearDown]

// 分類値の名前を指定せずに作成すると数値で表し、不正な名前の設定では作成できないことの確認
// [サブ手順参照 名前=stringCatalogFilterDescribeTest.SetUp]
TEST_F(stringCatalogFilterDescribeTest, category_names_absent_use_numbers_and_reject_invalid_settings)
{
    // Arrange
    const char *const names_with_null[] = {"CRITICAL", nullptr};
    cplat_string_catalog_filter_category_names invalid_names = {names_with_null, 2U, "レベル", "the level"};
    int actual_ret_invalid;
    int actual_ret_describe;

    // Pre-Assert

    // Act_1
    actual_ret_invalid =
        recreate_slot_with_category_names(&invalid_names); // [手順] - NULL の名前を含む設定でスロットを作り直す。

    // Assert_1
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_invalid); // [確認_異常系] - 不正な設定が拒否されること。
    EXPECT_EQ(nullptr, slot_);                                 // [確認_異常系] - スロットが作成されないこと。

    // Arrange_2
    ASSERT_EQ(CPLAT_OK,
              recreate_slot_with_category_names(nullptr)); // [状態] - 分類値の名前を指定せずにスロットを作り直す。
    // [状態確認] - `recreate_slot_with_category_names(nullptr)` の戻り値が `CPLAT_OK` であること。

    // Act_2
    actual_ret_describe =
        describe("category <= 2", CPLAT_STRING_CATALOG_LANGUAGE_JAPANESE); // [手順] - 分類値の比較を説明する。

    // Assert_2
    EXPECT_EQ(CPLAT_OK, actual_ret_describe);      // [確認_正常系] - 説明文を得られること。
    EXPECT_STREQ("分類値が 2 以下", description_); // [確認_正常系] - 名前の設定がなければ数値で表すこと。
}
// [サブ手順参照 名前=stringCatalogFilterDescribeTest.TearDown]
