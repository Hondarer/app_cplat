#include <testfw.h>

#include "filterTestSupport.h"

#include "gen/filter_test_trace.h"

#include <cplat/base/result.h>

#include <cstdint>

using namespace filter_test;

namespace
{
/** 分類値をトレース レベルとして扱う場合の名前です。 */
const char *const s_level_names[] = {"CRITICAL", "ERROR", "WARNING", "INFO", "VERBOSE", "DEBUG", "NONE"};

const cplat_string_catalog_filter_category_names s_level_category_names = {
    s_level_names,
    sizeof(s_level_names) / sizeof(s_level_names[0]),
    "レベル",
    "the level",
};
} // namespace

class stringCatalogFilterCategoryTest : public Test
{
  protected:
    // [サブ手順 名前=stringCatalogFilterCategoryTest.SetUp]
    void SetUp() override
    {
        ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_create(
                                filter_test_trace_catalog(), filter_test_trace_key_names(),
                                filter_test_trace_key_name_count(), nullptr, kLineCapacity, kLineWidth, &slot_));
        // [状態確認] - `cplat_string_catalog_filter_slot_create( filter_test_trace_catalog(), filter_test_trace_key_names(), filter_test_trace_key_name_count(), nullptr, kLineCapacity, kLineWidth, &slot_)` の戻り値が `CPLAT_OK` であること。
    }
    // [サブ手順終了]

    // [サブ手順 名前=stringCatalogFilterCategoryTest.TearDown]
    void TearDown() override
    {
        cplat_string_catalog_filter_slot_dispose(&slot_);
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

    /** 1 行の条件式を適用し、無効にした原因を返します。有効な場合は CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE です。 */
    cplat_string_catalog_filter_line_error apply_line(const char *expression)
    {
        static unsigned char image[kImageSize];
        cplat_string_catalog_filter_diagnostic diagnostic = {};
        std::size_t invalid_count = 0U;

        if (compile_single_line(expression, image) != CPLAT_OK)
        {
            return CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_SYNTAX;
        }
        if (cplat_string_catalog_filter_slot_apply(slot_, image, kImageSize, &diagnostic, 1U, &invalid_count) !=
            CPLAT_OK)
        {
            return CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_SYNTAX;
        }
        if (invalid_count == 0U)
        {
            return CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE;
        }
        return diagnostic.error;
    }

    cplat_string_catalog_filter_state state_of(const int string_key)
    {
        cplat_string_catalog_filter_state state = CPLAT_STRING_CATALOG_FILTER_STATE_NEVER_MATCH;

        (void)cplat_string_catalog_filter_slot_test(slot_, string_key, &state);
        return state;
    }

    cplat_string_catalog_filter_slot *slot_ = nullptr;
};

// 分類値の名前を設定すると、レベル名と数値のどちらでも同じ判定になることの確認
// [サブ手順参照 名前=stringCatalogFilterCategoryTest.SetUp]
TEST_F(stringCatalogFilterCategoryTest, level_name_and_number_give_same_result)
{
    // Arrange
    cplat_string_catalog_filter_line_error actual_error_name;
    cplat_string_catalog_filter_state actual_failed_name;
    cplat_string_catalog_filter_state actual_started_name;
    cplat_string_catalog_filter_line_error actual_error_number;
    cplat_string_catalog_filter_state actual_failed_number;
    cplat_string_catalog_filter_state actual_started_number;

    ASSERT_EQ(CPLAT_OK,
              recreate_slot_with_category_names(&s_level_category_names)); // [状態] - レベルの名前を設定する。
    // [状態確認] - `recreate_slot_with_category_names(&s_level_category_names)` の戻り値が `CPLAT_OK` であること。

    // Pre-Assert

    // Act_1
    actual_error_name = apply_line("category <= WARNING"); // [手順] - レベル名で書いた条件式を適用する。
    actual_failed_name = state_of(FILTER_TEST_TRACE_KEY_JOB_FAILED);
    actual_started_name = state_of(FILTER_TEST_TRACE_KEY_WORKER_STARTED);

    // Assert_1
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE,
              actual_error_name); // [確認_正常系] - レベル名の行が有効であること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_STATE_ALWAYS_MATCH,
              actual_failed_name); // [確認_正常系] - WARNING の JOB_FAILED が一致すること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_STATE_NEVER_MATCH,
              actual_started_name); // [確認_正常系] - INFO の WORKER_STARTED が一致しないこと。

    // Act_2
    actual_error_number = apply_line("category <= 2"); // [手順] - 数値で書いた条件式を適用する。
    actual_failed_number = state_of(FILTER_TEST_TRACE_KEY_JOB_FAILED);
    actual_started_number = state_of(FILTER_TEST_TRACE_KEY_WORKER_STARTED);

    // Assert_2
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE,
              actual_error_number);                        // [確認_正常系] - 数値の行が有効であること。
    EXPECT_EQ(actual_failed_name, actual_failed_number);   // [確認_正常系] - JOB_FAILED の判定が同じであること。
    EXPECT_EQ(actual_started_name, actual_started_number); // [確認_正常系] - WORKER_STARTED の判定が同じであること。
}
// [サブ手順参照 名前=stringCatalogFilterCategoryTest.TearDown]

// 分類値の名前の範囲外の定数を、行を無効にして拒否することの確認
// [サブ手順参照 名前=stringCatalogFilterCategoryTest.SetUp]
TEST_F(stringCatalogFilterCategoryTest, out_of_range_values_are_rejected)
{
    // Arrange
    ASSERT_EQ(CPLAT_OK,
              recreate_slot_with_category_names(&s_level_category_names)); // [状態] - レベルの名前を設定する。
    // [状態確認] - `recreate_slot_with_category_names(&s_level_category_names)` の戻り値が `CPLAT_OK` であること。

    // Pre-Assert

    // Act
    cplat_string_catalog_filter_line_error actual_upper =
        apply_line("category <= 7"); // [手順] - 上限を超える値を適用する。
    cplat_string_catalog_filter_line_error actual_negative =
        apply_line("category == -1"); // [手順] - 負の値を適用する。
    cplat_string_catalog_filter_line_error actual_fraction =
        apply_line("category between 1 and 2.5"); // [手順] - 整数でない値を適用する。
    cplat_string_catalog_filter_line_error actual_list =
        apply_line("category in [0, 9]"); // [手順] - 列挙の一部が範囲外の値を適用する。
    cplat_string_catalog_filter_line_error actual_boundary =
        apply_line("category in [0, 6, 2.0]"); // [手順] - 範囲の両端と整数値の実数を適用する。

    // Assert
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_CATEGORY_OUT_OF_RANGE,
              actual_upper); // [確認_異常系] - 上限超過が拒否されること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_CATEGORY_OUT_OF_RANGE,
              actual_negative); // [確認_異常系] - 負の値が拒否されること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_CATEGORY_OUT_OF_RANGE,
              actual_fraction); // [確認_異常系] - 整数でない値が拒否されること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_CATEGORY_OUT_OF_RANGE,
              actual_list); // [確認_異常系] - 列挙の範囲外が拒否されること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE,
              actual_boundary); // [確認_正常系] - 範囲内の値は受け入れられること。
}
// [サブ手順参照 名前=stringCatalogFilterCategoryTest.TearDown]

// 分類値の名前にない識別子を、行を無効にして拒否することの確認
// [サブ手順参照 名前=stringCatalogFilterCategoryTest.SetUp]
TEST_F(stringCatalogFilterCategoryTest, unknown_level_name_is_rejected)
{
    // Arrange
    ASSERT_EQ(CPLAT_OK,
              recreate_slot_with_category_names(&s_level_category_names)); // [状態] - レベルの名前を設定する。
    // [状態確認] - `recreate_slot_with_category_names(&s_level_category_names)` の戻り値が `CPLAT_OK` であること。

    // Pre-Assert

    // Act
    cplat_string_catalog_filter_line_error actual_error =
        apply_line("category in [INFO, FOO]"); // [手順] - 未知のレベル名を含む条件式を適用する。

    // Assert
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_UNRESOLVED_CATEGORY_NAME,
              actual_error); // [確認_異常系] - 未知のレベル名として拒否されること。
}
// [サブ手順参照 名前=stringCatalogFilterCategoryTest.TearDown]

// 分類値の名前を設定しない場合は、分類値を匿名のまま扱うことの確認
// [サブ手順参照 名前=stringCatalogFilterCategoryTest.SetUp]
TEST_F(stringCatalogFilterCategoryTest, without_category_names_category_is_anonymous)
{
    // Arrange

    // Pre-Assert

    // Act
    cplat_string_catalog_filter_line_error actual_large = apply_line("category <= 99"); // [手順] - 大きな値を適用する。
    cplat_string_catalog_filter_line_error actual_name =
        apply_line("category <= WARNING"); // [手順] - レベル名を適用する。

    // Assert
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE,
              actual_large); // [確認_正常系] - 範囲を確かめないため受け入れられること。
    EXPECT_EQ(
        CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_UNRESOLVED_KEY_NAME,
        actual_name); // [確認_異常系] - 識別子は従来どおり文字列キーの名前として解決され、WARNING は見つからないこと。
}
// [サブ手順参照 名前=stringCatalogFilterCategoryTest.TearDown]
