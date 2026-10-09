#include <testfw.h>

#include "filterTestSupport.h"

#include "gen/filter_test_trace.h"

#include <cplat/base/result.h>
#include <cplat/string_catalog/string_catalog.h>

#include <cstdint>
#include <cstring>
#include <string>

using namespace filter_test;

class stringCatalogFilterPatternTest : public Test
{
  protected:
    unsigned char image_[kImageSize];
    /** 組み立てた文字列の格納先です。照合の上限を超える引数を含めて組み立てられる大きさにします。 */
    char dest_[CPLAT_STRING_CATALOG_FILTER_PATTERN_SUBJECT_MAX * 4U];
    cplat_string_catalog_filter_slot *slot_ = nullptr;
    cplat_string_catalog_language saved_language_ = CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL;
    unsigned int pad_ = 0U; /**< 明示的アラインメントです。 */

    // [サブ手順 名前=stringCatalogFilterPatternTest.SetUp]
    void SetUp() override
    {
        saved_language_ = cplat_string_catalog_get_language();
        std::memset(image_, 0, sizeof(image_));
        ASSERT_EQ(CPLAT_OK, filter_test_trace_create_filter(nullptr, kLineCapacity, kLineWidth, &slot_));
        // [状態確認] - `filter_test_trace_create_filter(nullptr, kLineCapacity, kLineWidth, &slot_)` の戻り値が `CPLAT_OK` であること。
    }
    // [サブ手順終了]

    // [サブ手順 名前=stringCatalogFilterPatternTest.TearDown]
    void TearDown() override
    {
        cplat_string_catalog_filter_slot_dispose(&slot_);
        (void)cplat_string_catalog_set_language(saved_language_);
    }
    // [サブ手順終了]

    /** 条件式 1 行をコンパイルしてスロットへ適用し、無効にした行の数を返します。 */
    // [サブ手順 名前=stringCatalogFilterPatternTest.apply_line]
    std::size_t apply_line(const char *text)
    {
        std::size_t invalid_count = 99U;

        EXPECT_EQ(CPLAT_OK, compile_single_line(text, image_));
        // [確認_正常系] - `compile_single_line(text, image_)` の戻り値が `CPLAT_OK` であること。
        EXPECT_EQ(CPLAT_OK,
                  cplat_string_catalog_filter_slot_apply(slot_, image_, sizeof(image_), nullptr, 0U, &invalid_count));
        // [確認_正常系] - `cplat_string_catalog_filter_slot_apply(slot_, image_, sizeof(image_), nullptr, 0U, &invalid_count)` の戻り値が `CPLAT_OK` であること。
        return invalid_count;
    }
    // [サブ手順終了]

    /** ジョブ名を指定して JOB_RECEIVED を判定付きで組み立て、一致結果を返します。 */
    // [サブ手順 名前=stringCatalogFilterPatternTest.job_received_matched]
    int job_received_matched(const char *job_name)
    {
        int matched = -1;

        EXPECT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_format(
                                slot_, dest_, sizeof(dest_), &matched, FILTER_TEST_TRACE_KEY_JOB_RECEIVED, (uint32_t)0U,
                                (uint64_t)1U, job_name, (int32_t)3, FILTER_TEST_CONTEXT_ARGS(7)));
        // [確認_正常系] - `cplat_string_catalog_filter_slot_format( slot_, dest_, sizeof(dest_), &matched, FILTER_TEST_TRACE_KEY_JOB_RECEIVED, (uint32_t)0U, (uint64_t)1U, job_name, (int32_t)3, FILTER_TEST_CONTEXT_ARGS(7))` の戻り値が `CPLAT_OK` であること。
        return matched;
    }
    // [サブ手順終了]

    // [サブ手順 名前=stringCatalogFilterPatternTest.state_of]
    cplat_string_catalog_filter_state state_of(const int string_key)
    {
        cplat_string_catalog_filter_state state = CPLAT_STRING_CATALOG_FILTER_STATE_NEVER_MATCH;

        EXPECT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_test(slot_, string_key, &state));
        // [確認_正常系] - `cplat_string_catalog_filter_slot_test(slot_, string_key, &state)` の戻り値が `CPLAT_OK` であること。
        return state;
    }
    // [サブ手順終了]
};

// matches は文字列の一部に一致すれば成立し、^ と $ で位置を固定できることの確認
// [サブ手順参照 名前=stringCatalogFilterPatternTest.SetUp]
TEST_F(stringCatalogFilterPatternTest, matches_is_partial_match)
{
    // Arrange

    // Pre-Assert

    // Act
    // [サブ手順参照 名前=stringCatalogFilterPatternTest.apply_line]
    ASSERT_EQ(0U, apply_line("arg.job_name matches \"port\""));          // [手順] - 部分に一致するパターンを適用する。
    // 条件式のコンパイルと適用が成功すること。
    // [確認_正常系] - `apply_line("arg.job_name matches \"port\"")` の戻り値が `0U` であること。
    // [サブ手順参照 名前=stringCatalogFilterPatternTest.job_received_matched]
    const int actual_import = job_received_matched("import-1");          // [手順] - 途中に port を含む名前で判定する。
    // JOB_RECEIVED の組み立てが成功すること。
    // [サブ手順参照 名前=stringCatalogFilterPatternTest.job_received_matched]
    const int actual_build = job_received_matched("build-1");            // [手順] - port を含まない名前で判定する。
    // JOB_RECEIVED の組み立てが成功すること。
    // [サブ手順参照 名前=stringCatalogFilterPatternTest.apply_line]
    ASSERT_EQ(0U, apply_line("arg.job_name matches \"^imp.*-[0-9]$\"")); // [手順] - 位置を固定したパターンを適用する。
    // 条件式のコンパイルと適用が成功すること。
    // [確認_正常系] - `apply_line("arg.job_name matches \"^imp.*-[0-9]$\"")` の戻り値が `0U` であること。
    // [サブ手順参照 名前=stringCatalogFilterPatternTest.job_received_matched]
    const int actual_anchored = job_received_matched("import-1");        // [手順] - 全体が一致する名前で判定する。
    // JOB_RECEIVED の組み立てが成功すること。
    // [サブ手順参照 名前=stringCatalogFilterPatternTest.job_received_matched]
    const int actual_prefixed = job_received_matched("reimport-1");      // [手順] - 先頭が異なる名前で判定する。
    // JOB_RECEIVED の組み立てが成功すること。

    // Assert
    EXPECT_NE(0, actual_import);   // [確認_正常系] - 一部に一致すれば成立すること。
    EXPECT_EQ(0, actual_build);    // [確認_正常系] - 一致する部分がなければ成立しないこと。
    EXPECT_NE(0, actual_anchored); // [確認_正常系] - 固定した位置で一致すれば成立すること。
    EXPECT_EQ(0, actual_prefixed); // [確認_正常系] - ^ で先頭に固定できること。
}
// [サブ手順参照 名前=stringCatalogFilterPatternTest.TearDown]

// matches_i は ASCII の大文字と小文字を区別せず、matches は区別することの確認
// [サブ手順参照 名前=stringCatalogFilterPatternTest.SetUp]
TEST_F(stringCatalogFilterPatternTest, matches_i_ignores_ascii_case)
{
    // Arrange

    // Pre-Assert

    // Act
    // [サブ手順参照 名前=stringCatalogFilterPatternTest.apply_line]
    ASSERT_EQ(0U, apply_line("arg.job_name matches \"IMPORT\"")); // [手順] - 大文字のパターンを matches で適用する。
    // 条件式のコンパイルと適用が成功すること。
    // [確認_正常系] - `apply_line("arg.job_name matches \"IMPORT\"")` の戻り値が `0U` であること。
    // [サブ手順参照 名前=stringCatalogFilterPatternTest.job_received_matched]
    const int actual_sensitive = job_received_matched("import-1");
    // JOB_RECEIVED の組み立てが成功すること。
    // [サブ手順参照 名前=stringCatalogFilterPatternTest.apply_line]
    ASSERT_EQ(0U, apply_line("arg.job_name matches_i \"IMPORT\"")); // [手順] - 同じパターンを matches_i で適用する。
    // 条件式のコンパイルと適用が成功すること。
    // [確認_正常系] - `apply_line("arg.job_name matches_i \"IMPORT\"")` の戻り値が `0U` であること。
    // [サブ手順参照 名前=stringCatalogFilterPatternTest.job_received_matched]
    const int actual_insensitive = job_received_matched("import-1");
    // JOB_RECEIVED の組み立てが成功すること。

    // Assert
    EXPECT_EQ(0, actual_sensitive);   // [確認_正常系] - matches は大文字と小文字を区別すること。
    EXPECT_NE(0, actual_insensitive); // [確認_正常系] - matches_i は大文字と小文字を区別しないこと。
}
// [サブ手順参照 名前=stringCatalogFilterPatternTest.TearDown]

// ID へのパターンは適用の時点で項目ごとに一致が確定することの確認
// [サブ手順参照 名前=stringCatalogFilterPatternTest.SetUp]
TEST_F(stringCatalogFilterPatternTest, id_pattern_is_resolved_at_apply)
{
    // Arrange

    // Pre-Assert

    // Act
    // [サブ手順参照 名前=stringCatalogFilterPatternTest.apply_line]
    ASSERT_EQ(0U, apply_line("id matches \"0005$\"")); // [手順] - ID の末尾に一致するパターンを適用する。
    // 条件式のコンパイルと適用が成功すること。
    // [確認_正常系] - `apply_line("id matches \"0005$\"")` の戻り値が `0U` であること。

    // Assert
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_STATE_ALWAYS_MATCH,
              // [サブ手順参照 名前=stringCatalogFilterPatternTest.state_of]
              state_of(FILTER_TEST_TRACE_KEY_JOB_FAILED)); // [確認_正常系] - 一致する項目が常に一致になること。
    // [確認_正常系] - フィルターの判定状態を取得できること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_STATE_NEVER_MATCH,
              // [サブ手順参照 名前=stringCatalogFilterPatternTest.state_of]
              state_of(FILTER_TEST_TRACE_KEY_JOB_RECEIVED)); // [確認_正常系] - 一致しない項目が常に不一致になること。
    // [確認_正常系] - フィルターの判定状態を取得できること。
}
// [サブ手順参照 名前=stringCatalogFilterPatternTest.TearDown]

// 照合する文字列のバイト数の上限を超える場合は照合せず、不一致とすることの確認
// [サブ手順参照 名前=stringCatalogFilterPatternTest.SetUp]
TEST_F(stringCatalogFilterPatternTest, subject_longer_than_limit_is_not_matched)
{
    // Arrange
    const std::string at_limit =
        std::string("x") + std::string(CPLAT_STRING_CATALOG_FILTER_PATTERN_SUBJECT_MAX - 1U, 'a');
    const std::string over_limit = at_limit + "a";
    // [サブ手順参照 名前=stringCatalogFilterPatternTest.apply_line]
    ASSERT_EQ(0U, apply_line("arg.job_name matches \"^x\"")); // [状態] - 先頭に一致するパターンを適用する。
    // 条件式のコンパイルと適用が成功すること。
    // [状態確認] - `apply_line("arg.job_name matches \"^x\"")` の戻り値が `0U` であること。

    // Pre-Assert

    // Act
    // [サブ手順参照 名前=stringCatalogFilterPatternTest.job_received_matched]
    const int actual_at_limit = job_received_matched(at_limit.c_str()); // [手順] - 上限ちょうどの長さで判定する。
    // JOB_RECEIVED の組み立てが成功すること。
    const int actual_over_limit =
        // [サブ手順参照 名前=stringCatalogFilterPatternTest.job_received_matched]
        job_received_matched(over_limit.c_str()); // [手順] - 上限を 1 バイト超える長さで判定する。
    // JOB_RECEIVED の組み立てが成功すること。

    // Assert
    EXPECT_NE(0, actual_at_limit);   // [確認_正常系] - 上限ちょうどは照合すること。
    EXPECT_EQ(0, actual_over_limit); // [確認_異常系] - 上限を超える文字列は不一致とすること。
}
// [サブ手順参照 名前=stringCatalogFilterPatternTest.TearDown]

// NULL の文字列の引数は、パターンに一致しないことの確認
// [サブ手順参照 名前=stringCatalogFilterPatternTest.SetUp]
TEST_F(stringCatalogFilterPatternTest, null_subject_is_not_matched)
{
    // Arrange
    // [サブ手順参照 名前=stringCatalogFilterPatternTest.apply_line]
    ASSERT_EQ(0U, apply_line("arg.job_name matches \"\"")); // [状態] - すべての文字列に一致する空のパターンを適用する。
    // 条件式のコンパイルと適用が成功すること。
    // [状態確認] - `apply_line("arg.job_name matches \"\"")` の戻り値が `0U` であること。

    // Pre-Assert

    // Act
    // [サブ手順参照 名前=stringCatalogFilterPatternTest.job_received_matched]
    const int actual_empty_name = job_received_matched("");     // [手順] - 空の名前で判定する。
    // JOB_RECEIVED の組み立てが成功すること。
    // [サブ手順参照 名前=stringCatalogFilterPatternTest.job_received_matched]
    const int actual_null_name = job_received_matched(nullptr); // [手順] - NULL の名前で判定する。
    // JOB_RECEIVED の組み立てが成功すること。

    // Assert
    EXPECT_NE(0, actual_empty_name); // [確認_正常系] - 空のパターンは空の文字列にも一致すること。
    EXPECT_EQ(0, actual_null_name);  // [確認_異常系] - NULL はパターンに一致しないこと。
}
// [サブ手順参照 名前=stringCatalogFilterPatternTest.TearDown]

// 同じ条件式を適用し直しても、再利用した行のパターンで判定できることの確認
// [サブ手順参照 名前=stringCatalogFilterPatternTest.SetUp]
TEST_F(stringCatalogFilterPatternTest, reapplied_line_keeps_pattern)
{
    // Arrange
    // [サブ手順参照 名前=stringCatalogFilterPatternTest.apply_line]
    ASSERT_EQ(0U, apply_line("arg.job_name matches \"port\"")); // [状態] - パターンを適用する。
    // 条件式のコンパイルと適用が成功すること。
    // [状態確認] - `apply_line("arg.job_name matches \"port\"")` の戻り値が `0U` であること。

    // Pre-Assert

    // Act
    // [サブ手順参照 名前=stringCatalogFilterPatternTest.apply_line]
    ASSERT_EQ(0U, apply_line("arg.job_name matches \"port\"")); // [手順] - 同じ条件式を適用し直す (2 面目)。
    // 条件式のコンパイルと適用が成功すること。
    // [確認_正常系] - `apply_line("arg.job_name matches \"port\"")` の戻り値が `0U` であること。
    // [サブ手順参照 名前=stringCatalogFilterPatternTest.job_received_matched]
    const int actual_second = job_received_matched("import-1");
    // JOB_RECEIVED の組み立てが成功すること。
    // [サブ手順参照 名前=stringCatalogFilterPatternTest.apply_line]
    ASSERT_EQ(0U, apply_line("arg.job_name matches \"port\"")); // [手順] - もう一度適用し直す (1 面目へ戻る)。
    // 条件式のコンパイルと適用が成功すること。
    // [確認_正常系] - `apply_line("arg.job_name matches \"port\"")` の戻り値が `0U` であること。
    // [サブ手順参照 名前=stringCatalogFilterPatternTest.job_received_matched]
    const int actual_third = job_received_matched("import-1");
    // JOB_RECEIVED の組み立てが成功すること。

    // Assert
    EXPECT_NE(0, actual_second); // [確認_正常系] - 2 面目でもパターンで判定すること。
    EXPECT_NE(0, actual_third);  // [確認_正常系] - 1 面目へ戻ってもパターンで判定すること。
}
// [サブ手順参照 名前=stringCatalogFilterPatternTest.TearDown]

// 文字列でない引数へのパターンは、どの項目でも成立しないため、成立し得ない行として無効にすることの確認
// [サブ手順参照 名前=stringCatalogFilterPatternTest.SetUp]
TEST_F(stringCatalogFilterPatternTest, pattern_on_non_string_argument_is_never_satisfiable)
{
    // Arrange
    cplat_string_catalog_filter_line_error actual_error = CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE;

    // Pre-Assert

    // Act
    const std::size_t actual_invalid =
        // [サブ手順参照 名前=stringCatalogFilterPatternTest.apply_line]
        apply_line("arg.priority matches \"3\""); // [手順] - 整数の引数へパターンを書く。
    // 条件式のコンパイルと適用が成功すること。
    ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_get_line_error(
                            slot_, 0U, &actual_error)); // [手順] - 行の状態を取得する。
    // [確認_正常系] - `cplat_string_catalog_filter_slot_get_line_error( slot_, 0U, &actual_error)` の戻り値が `CPLAT_OK` であること。

    // Assert
    EXPECT_EQ(1U, actual_invalid); // [確認_異常系] - 行を無効にすること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NEVER_SATISFIABLE,
              actual_error); // [確認_異常系] - 原因が成立し得ない条件であること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_STATE_NEVER_MATCH,
              // [サブ手順参照 名前=stringCatalogFilterPatternTest.state_of]
              state_of(FILTER_TEST_TRACE_KEY_JOB_RECEIVED)); // [確認_異常系] - どの項目にも一致しないこと。
    // [確認_正常系] - フィルターの判定状態を取得できること。
}
// [サブ手順参照 名前=stringCatalogFilterPatternTest.TearDown]

// パターンの条件を、日本語とニュートラル言語の文で説明することの確認
// [サブ手順参照 名前=stringCatalogFilterPatternTest.SetUp]
TEST_F(stringCatalogFilterPatternTest, pattern_is_described_in_both_languages)
{
    // Arrange
    char actual_japanese[512];
    char actual_neutral[512];
    // [サブ手順参照 名前=stringCatalogFilterPatternTest.apply_line]
    ASSERT_EQ(0U, apply_line("arg.job_name matches_i \"^imp\"")); // [状態] - パターンを適用する。
    // 条件式のコンパイルと適用が成功すること。
    // [状態確認] - `apply_line("arg.job_name matches_i \"^imp\"")` の戻り値が `0U` であること。

    // Pre-Assert

    // Act
    (void)cplat_string_catalog_set_language(CPLAT_STRING_CATALOG_LANGUAGE_JAPANESE);
    ASSERT_EQ(CPLAT_OK,
              cplat_string_catalog_filter_slot_describe_line(slot_, 0U, actual_japanese,
                                                             sizeof(actual_japanese))); // [手順] - 日本語で説明する。
    // [確認_正常系] - `cplat_string_catalog_filter_slot_describe_line(slot_, 0U, actual_japanese, sizeof(actual_japanese))` の戻り値が `CPLAT_OK` であること。
    (void)cplat_string_catalog_set_language(CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL);
    ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_describe_line(
                            slot_, 0U, actual_neutral,
                            sizeof(actual_neutral))); // [手順] - ニュートラル言語で説明する。
    // [確認_正常系] - `cplat_string_catalog_filter_slot_describe_line( slot_, 0U, actual_neutral, sizeof(actual_neutral))` の戻り値が `CPLAT_OK` であること。

    // Assert
    EXPECT_NE(nullptr,
              std::strstr(actual_japanese, "が 正規表現 \"^imp\" に一致する部分を含む (大文字と小文字を区別しない)"))
        << actual_japanese; // [確認_正常系] - 日本語の文型で説明すること。
    EXPECT_NE(nullptr,
              std::strstr(actual_neutral, " contains a match for the regular expression \"^imp\" (case-insensitive)"))
        << actual_neutral; // [確認_正常系] - ニュートラル言語の文型で説明すること。
}
// [サブ手順参照 名前=stringCatalogFilterPatternTest.TearDown]
