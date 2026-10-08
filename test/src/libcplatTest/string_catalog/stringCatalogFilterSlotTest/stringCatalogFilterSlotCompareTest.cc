#include <testfw.h>

#include "filterTestSupport.h"

#include "gen/filter_test_trace.h"

#include <cplat/base/result.h>

#include <cmath>
#include <cstdint>

using namespace filter_test;

class stringCatalogFilterSlotCompareTest : public Test
{
  protected:
    // [サブ手順 名前=stringCatalogFilterSlotCompareTest.SetUp]
    void SetUp() override
    {
        ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_create(
                                filter_test_trace_catalog(), filter_test_trace_key_names(),
                                filter_test_trace_key_name_count(), nullptr, kLineCapacity, kLineWidth, &slot_));
        // [状態確認] - `cplat_string_catalog_filter_slot_create( filter_test_trace_catalog(), filter_test_trace_key_names(), filter_test_trace_key_name_count(), nullptr, kLineCapacity, kLineWidth, &slot_)` の戻り値が `CPLAT_OK` であること。
    }
    // [サブ手順終了]

    // [サブ手順 名前=stringCatalogFilterSlotCompareTest.TearDown]
    void TearDown() override
    {
        cplat_string_catalog_filter_slot_dispose(&slot_);
    }
    // [サブ手順終了]

    /** 1 行の条件式をコンパイルし、スロットへ適用します。Arrange の共通処理です。 */
    // [サブ手順 名前=stringCatalogFilterSlotCompareTest.apply_filter]
    void apply_filter(const char *expr)
    {
        static unsigned char image[kImageSize];

        ASSERT_EQ(CPLAT_OK, compile_single_line(expr, image));
        // [状態確認] - `compile_single_line(expr, image)` の戻り値が `CPLAT_OK` であること。
        ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_apply(slot_, image, kImageSize, nullptr, 0U, nullptr));
        // [状態確認] - `cplat_string_catalog_filter_slot_apply(slot_, image, kImageSize, nullptr, 0U, nullptr)` の戻り値が `CPLAT_OK` であること。
    }
    // [サブ手順終了]

    cplat_string_catalog_filter_slot *slot_ = nullptr;
};

// INT32 の -1 と、符号なしの巨大な整数定数 4294967295 は、数学的な大小では等しくないことの確認
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.SetUp]
TEST_F(stringCatalogFilterSlotCompareTest, signed_int32_and_large_uint_constant_are_not_equal)
{
    // Arrange
    char dest[CPLAT_STRING_CATALOG_TEXT_MAX];
    int actual_matched = -1;
    int actual_ret;

    // [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.apply_filter]
    apply_filter(
        "arg.priority == 4294967295"); // [状態] - INT32 の引数を、範囲外の巨大な整数定数と比較する条件式を適用する。

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_filter_slot_format(
        slot_, dest, sizeof(dest), &actual_matched, FILTER_TEST_TRACE_KEY_JOB_RECEIVED, (uint32_t)1, (uint64_t)1, "job",
        (int32_t)-1,
        FILTER_TEST_CONTEXT_ARGS(7)); // [手順] - priority に -1 を渡して判定と書式展開を行う。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, actual_matched); // [確認_正常系] - -1 と 4294967295 は数学的な大小で等しくないため、一致しないこと。
}
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.TearDown]

// UINT64 の最大値どうしの比較が一致することの確認
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.SetUp]
TEST_F(stringCatalogFilterSlotCompareTest, uint64_max_value_matches_equal)
{
    // Arrange
    char dest[CPLAT_STRING_CATALOG_TEXT_MAX];
    int actual_matched = -1;
    int actual_ret;

    // [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.apply_filter]
    apply_filter("arg.processed_count == 18446744073709551615"); // [状態] - UINT64 の最大値と比較する条件式を適用する。

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_filter_slot_format(
        slot_, dest, sizeof(dest), &actual_matched, FILTER_TEST_TRACE_KEY_WORKER_STOPPED, (uint32_t)1,
        (uint64_t)0xFFFFFFFFFFFFFFFFULL,
        FILTER_TEST_CONTEXT_ARGS(7)); // [手順] - processed_count に UINT64 の最大値を渡す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_NE(0, actual_matched);    // [確認_正常系] - UINT64 の最大値どうしが一致すること。
}
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.TearDown]

// arg.ratio != 0.5 が、NaN のときに真となることの確認
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.SetUp]
TEST_F(stringCatalogFilterSlotCompareTest, nan_ratio_matches_not_equal)
{
    // Arrange
    char dest[CPLAT_STRING_CATALOG_TEXT_MAX];
    int actual_matched = -1;
    int actual_ret;

    // [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.apply_filter]
    apply_filter("arg.ratio != 0.5"); // [状態] - DOUBLE の引数を != で比較する条件式を適用する。

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_filter_slot_format(
        slot_, dest, sizeof(dest), &actual_matched, FILTER_TEST_TRACE_KEY_JOB_PROGRESS, (uint64_t)1, std::nan(""),
        FILTER_TEST_CONTEXT_ARGS(7)); // [手順] - ratio に NaN を渡して判定と書式展開を行う。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_NE(0, actual_matched);    // [確認_正常系] - NaN は != だけが真となること。
}
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.TearDown]

// arg.ratio < 1.0 が、NaN のときに偽となることの確認
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.SetUp]
TEST_F(stringCatalogFilterSlotCompareTest, nan_ratio_does_not_match_less_than)
{
    // Arrange
    char dest[CPLAT_STRING_CATALOG_TEXT_MAX];
    int actual_matched = -1;
    int actual_ret;

    // [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.apply_filter]
    apply_filter("arg.ratio < 1.0"); // [状態] - DOUBLE の引数を < で比較する条件式を適用する。

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_filter_slot_format(
        slot_, dest, sizeof(dest), &actual_matched, FILTER_TEST_TRACE_KEY_JOB_PROGRESS, (uint64_t)1, std::nan(""),
        FILTER_TEST_CONTEXT_ARGS(7)); // [手順] - ratio に NaN を渡して判定と書式展開を行う。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, actual_matched);    // [確認_正常系] - NaN では < が偽となること。
}
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.TearDown]

// starts_with_i が、大文字小文字を区別せず先頭一致することの確認
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.SetUp]
TEST_F(stringCatalogFilterSlotCompareTest, starts_with_i_ignores_case)
{
    // Arrange
    char dest[CPLAT_STRING_CATALOG_TEXT_MAX];
    int actual_matched = -1;
    int actual_ret;

    // [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.apply_filter]
    apply_filter("arg.job_name starts_with_i \"JOB\""); // [状態] - 大文字小文字を区別しない先頭一致の条件式を適用する。

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_filter_slot_format(
        slot_, dest, sizeof(dest), &actual_matched, FILTER_TEST_TRACE_KEY_JOB_RECEIVED, (uint32_t)1, (uint64_t)1,
        "job-42", (int32_t)0, FILTER_TEST_CONTEXT_ARGS(7)); // [手順] - 小文字表記の job_name を渡す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_NE(0, actual_matched);    // [確認_正常系] - 大文字小文字を無視して先頭一致すること。
}
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.TearDown]

// STRING 引数が NULL の場合、== null は真になることの確認
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.SetUp]
TEST_F(stringCatalogFilterSlotCompareTest, null_string_argument_matches_equal_null)
{
    // Arrange
    char dest[CPLAT_STRING_CATALOG_TEXT_MAX];
    int actual_matched = -1;
    int actual_ret;

    // [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.apply_filter]
    apply_filter("arg.job_name == null"); // [状態] - job_name を null と比較する条件式を適用する。

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_filter_slot_format(
        slot_, dest, sizeof(dest), &actual_matched, FILTER_TEST_TRACE_KEY_JOB_RECEIVED, (uint32_t)1, (uint64_t)1,
        (const char *)nullptr, (int32_t)0,
        FILTER_TEST_CONTEXT_ARGS(7)); // [手順] - job_name に NULL を渡す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_NE(0, actual_matched);    // [確認_正常系] - NULL の引数は == null で真になること。
}
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.TearDown]

// STRING 引数が NULL の場合、starts_with は偽になることの確認
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.SetUp]
TEST_F(stringCatalogFilterSlotCompareTest, null_string_argument_does_not_match_starts_with)
{
    // Arrange
    char dest[CPLAT_STRING_CATALOG_TEXT_MAX];
    int actual_matched = -1;
    int actual_ret;

    // [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.apply_filter]
    apply_filter("arg.job_name starts_with \"x\""); // [状態] - job_name の先頭一致を判定する条件式を適用する。

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_filter_slot_format(
        slot_, dest, sizeof(dest), &actual_matched, FILTER_TEST_TRACE_KEY_JOB_RECEIVED, (uint32_t)1, (uint64_t)1,
        (const char *)nullptr, (int32_t)0,
        FILTER_TEST_CONTEXT_ARGS(7)); // [手順] - job_name に NULL を渡す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, actual_matched);    // [確認_正常系] - NULL の引数は文字列判定演算がすべて偽になること。
}
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.TearDown]

// STRING 引数が NULL の場合、!= は真になることの確認
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.SetUp]
TEST_F(stringCatalogFilterSlotCompareTest, null_string_argument_matches_not_equal)
{
    // Arrange
    char dest[CPLAT_STRING_CATALOG_TEXT_MAX];
    int actual_matched = -1;
    int actual_ret;

    // [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.apply_filter]
    apply_filter("arg.job_name != \"x\""); // [状態] - job_name の不一致を判定する条件式を適用する。

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_filter_slot_format(
        slot_, dest, sizeof(dest), &actual_matched, FILTER_TEST_TRACE_KEY_JOB_RECEIVED, (uint32_t)1, (uint64_t)1,
        (const char *)nullptr, (int32_t)0,
        FILTER_TEST_CONTEXT_ARGS(7)); // [手順] - job_name に NULL を渡す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_NE(0, actual_matched);    // [確認_正常系] - NULL の引数は != で真になること。
}
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.TearDown]

// POINTER 引数の == null が、NULL と非 NULL を正しく判定することの確認
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.SetUp]
TEST_F(stringCatalogFilterSlotCompareTest, pointer_argument_equal_null_distinguishes_null_and_non_null)
{
    // Arrange
    char dest[CPLAT_STRING_CATALOG_TEXT_MAX];
    int dummy_target = 0;
    int actual_matched_null = -1;
    int actual_matched_non_null = -1;
    int actual_ret_null;
    int actual_ret_non_null;

    // [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.apply_filter]
    apply_filter("arg.buffer == null"); // [状態] - buffer を null と比較する条件式を適用する。

    // Pre-Assert

    // Act
    actual_ret_null = cplat_string_catalog_filter_slot_format(
        slot_, dest, sizeof(dest), &actual_matched_null, FILTER_TEST_TRACE_KEY_BUFFER_ALLOCATED, (const void *)nullptr,
        (size_t)0, FILTER_TEST_CONTEXT_ARGS(7)); // [手順] - buffer に NULL を渡す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_null); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_NE(0, actual_matched_null);    // [確認_正常系] - NULL のポインターは == null で真になること。

    // Act_2
    actual_ret_non_null = cplat_string_catalog_filter_slot_format(
        slot_, dest, sizeof(dest), &actual_matched_non_null, FILTER_TEST_TRACE_KEY_BUFFER_ALLOCATED,
        (const void *)&dummy_target, (size_t)4,
        FILTER_TEST_CONTEXT_ARGS(7)); // [手順] - buffer に非 NULL を渡す。

    // Assert_2
    EXPECT_EQ(CPLAT_OK, actual_ret_non_null); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, actual_matched_non_null);    // [確認_正常系] - 非 NULL のポインターは == null で偽になること。
}
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.TearDown]

// CHAR 引数が、文字定数と比較して一致することの確認
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.SetUp]
TEST_F(stringCatalogFilterSlotCompareTest, char_argument_matches_character_literal)
{
    // Arrange
    char dest[CPLAT_STRING_CATALOG_TEXT_MAX];
    int actual_matched = -1;
    int actual_ret;

    // [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.apply_filter]
    apply_filter("arg.command == 's'"); // [状態] - command を文字定数 's' と比較する条件式を適用する。

    // Pre-Assert

    // Act
    actual_ret =
        cplat_string_catalog_filter_slot_format(slot_, dest, sizeof(dest), &actual_matched,
                                                FILTER_TEST_TRACE_KEY_COMMAND_RECEIVED, (int)'s', (int)0, (int64_t)0,
                                                FILTER_TEST_CONTEXT_ARGS(7)); // [手順] - command に 's' を渡す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_NE(0, actual_matched);    // [確認_正常系] - 文字定数と一致すること。
}
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.TearDown]

// HEX8 引数が、16 進定数と比較して一致することの確認
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.SetUp]
TEST_F(stringCatalogFilterSlotCompareTest, hex8_argument_matches_hexadecimal_literal)
{
    // Arrange
    char dest[CPLAT_STRING_CATALOG_TEXT_MAX];
    int actual_matched = -1;
    int actual_ret;

    // [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.apply_filter]
    apply_filter("arg.status == 0xFF"); // [状態] - status を 16 進定数 0xFF と比較する条件式を適用する。

    // Pre-Assert

    // Act
    actual_ret =
        cplat_string_catalog_filter_slot_format(slot_, dest, sizeof(dest), &actual_matched,
                                                FILTER_TEST_TRACE_KEY_COMMAND_RECEIVED, (int)'a', (int)0xFF, (int64_t)0,
                                                FILTER_TEST_CONTEXT_ARGS(7)); // [手順] - status に 0xFF を渡す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_NE(0, actual_matched);    // [確認_正常系] - 16 進定数と一致すること。
}
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.TearDown]

// between の境界値 (下限、上限) が一致し、境界の外は一致しないことの確認
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.SetUp]
TEST_F(stringCatalogFilterSlotCompareTest, between_boundary_values_are_inclusive)
{
    // Arrange
    char dest[CPLAT_STRING_CATALOG_TEXT_MAX];
    int actual_matched_lower = -1;
    int actual_matched_upper = -1;
    int actual_matched_below = -1;
    int actual_matched_above = -1;

    // [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.apply_filter]
    apply_filter("arg.priority between 1 and 10"); // [状態] - priority が 1 以上 10 以下かを判定する条件式を適用する。

    // Pre-Assert

    // Act
    ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_format(
                            slot_, dest, sizeof(dest), &actual_matched_lower, FILTER_TEST_TRACE_KEY_JOB_RECEIVED,
                            (uint32_t)1, (uint64_t)1, "job", (int32_t)1,
                            FILTER_TEST_CONTEXT_ARGS(7))); // [手順] - priority に下限の 1 を渡す。
    // [確認_正常系] - `cplat_string_catalog_filter_slot_format( slot_, dest, sizeof(dest), &actual_matched_lower, FILTER_TEST_TRACE_KEY_JOB_RECEIVED, (uint32_t)1, (uint64_t)1, "job", (int32_t)1, FILTER_TEST_CONTEXT_ARGS(7))` の戻り値が `CPLAT_OK` であること。

    // Assert
    EXPECT_NE(0, actual_matched_lower); // [確認_正常系] - 下限の 1 が一致すること。

    // Act_2
    ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_format(
                            slot_, dest, sizeof(dest), &actual_matched_upper, FILTER_TEST_TRACE_KEY_JOB_RECEIVED,
                            (uint32_t)1, (uint64_t)1, "job", (int32_t)10,
                            FILTER_TEST_CONTEXT_ARGS(7))); // [手順] - priority に上限の 10 を渡す。
    // [確認_正常系] - `cplat_string_catalog_filter_slot_format( slot_, dest, sizeof(dest), &actual_matched_upper, FILTER_TEST_TRACE_KEY_JOB_RECEIVED, (uint32_t)1, (uint64_t)1, "job", (int32_t)10, FILTER_TEST_CONTEXT_ARGS(7))` の戻り値が `CPLAT_OK` であること。

    // Assert_2
    EXPECT_NE(0, actual_matched_upper); // [確認_正常系] - 上限の 10 が一致すること。

    // Act_3
    ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_format(
                            slot_, dest, sizeof(dest), &actual_matched_below, FILTER_TEST_TRACE_KEY_JOB_RECEIVED,
                            (uint32_t)1, (uint64_t)1, "job", (int32_t)0,
                            FILTER_TEST_CONTEXT_ARGS(7))); // [手順] - priority に下限未満の 0 を渡す。
    // [確認_正常系] - `cplat_string_catalog_filter_slot_format( slot_, dest, sizeof(dest), &actual_matched_below, FILTER_TEST_TRACE_KEY_JOB_RECEIVED, (uint32_t)1, (uint64_t)1, "job", (int32_t)0, FILTER_TEST_CONTEXT_ARGS(7))` の戻り値が `CPLAT_OK` であること。

    // Assert_3
    EXPECT_EQ(0, actual_matched_below); // [確認_正常系] - 下限未満の 0 は一致しないこと。

    // Act_4
    ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_format(
                            slot_, dest, sizeof(dest), &actual_matched_above, FILTER_TEST_TRACE_KEY_JOB_RECEIVED,
                            (uint32_t)1, (uint64_t)1, "job", (int32_t)11,
                            FILTER_TEST_CONTEXT_ARGS(7))); // [手順] - priority に上限超過の 11 を渡す。
    // [確認_正常系] - `cplat_string_catalog_filter_slot_format( slot_, dest, sizeof(dest), &actual_matched_above, FILTER_TEST_TRACE_KEY_JOB_RECEIVED, (uint32_t)1, (uint64_t)1, "job", (int32_t)11, FILTER_TEST_CONTEXT_ARGS(7))` の戻り値が `CPLAT_OK` であること。

    // Assert_4
    EXPECT_EQ(0, actual_matched_above); // [確認_正常系] - 上限超過の 11 は一致しないこと。
}
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.TearDown]

// in が、列挙した値のいずれかと一致することの確認
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.SetUp]
TEST_F(stringCatalogFilterSlotCompareTest, in_operator_matches_any_listed_value)
{
    // Arrange
    char dest[CPLAT_STRING_CATALOG_TEXT_MAX];
    int actual_matched_listed = -1;
    int actual_matched_unlisted = -1;

    // [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.apply_filter]
    apply_filter("arg.priority in [1, 5, 9]"); // [状態] - priority が列挙値のいずれかかを判定する条件式を適用する。

    // Pre-Assert

    // Act
    ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_format(
                            slot_, dest, sizeof(dest), &actual_matched_listed, FILTER_TEST_TRACE_KEY_JOB_RECEIVED,
                            (uint32_t)1, (uint64_t)1, "job", (int32_t)5,
                            FILTER_TEST_CONTEXT_ARGS(7))); // [手順] - priority に列挙値の 5 を渡す。
    // [確認_正常系] - `cplat_string_catalog_filter_slot_format( slot_, dest, sizeof(dest), &actual_matched_listed, FILTER_TEST_TRACE_KEY_JOB_RECEIVED, (uint32_t)1, (uint64_t)1, "job", (int32_t)5, FILTER_TEST_CONTEXT_ARGS(7))` の戻り値が `CPLAT_OK` であること。

    // Assert
    EXPECT_NE(0, actual_matched_listed); // [確認_正常系] - 列挙値のいずれかと一致すること。

    // Act_2
    ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_format(
                            slot_, dest, sizeof(dest), &actual_matched_unlisted, FILTER_TEST_TRACE_KEY_JOB_RECEIVED,
                            (uint32_t)1, (uint64_t)1, "job", (int32_t)6,
                            FILTER_TEST_CONTEXT_ARGS(7))); // [手順] - priority に列挙にない 6 を渡す。
    // [確認_正常系] - `cplat_string_catalog_filter_slot_format( slot_, dest, sizeof(dest), &actual_matched_unlisted, FILTER_TEST_TRACE_KEY_JOB_RECEIVED, (uint32_t)1, (uint64_t)1, "job", (int32_t)6, FILTER_TEST_CONTEXT_ARGS(7))` の戻り値が `CPLAT_OK` であること。

    // Assert_2
    EXPECT_EQ(0, actual_matched_unlisted); // [確認_正常系] - 列挙値のいずれにも一致しないこと。
}
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.TearDown]

// has(arg.<name>) が、引数を持つ項目でだけ真になることの確認
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.SetUp]
TEST_F(stringCatalogFilterSlotCompareTest, has_argument_name_is_true_only_for_entries_with_that_argument)
{
    // Arrange
    char dest[CPLAT_STRING_CATALOG_TEXT_MAX];
    int actual_matched_with_argument = -1;
    int actual_matched_without_argument = -1;

    // [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.apply_filter]
    apply_filter("has(arg.buffer)"); // [状態] - buffer を持つかどうかを判定する条件式を適用する。

    // Pre-Assert

    // Act
    ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_format(
                            slot_, dest, sizeof(dest), &actual_matched_with_argument,
                            FILTER_TEST_TRACE_KEY_BUFFER_ALLOCATED, (const void *)nullptr, (size_t)0,
                            FILTER_TEST_CONTEXT_ARGS(7))); // [手順] - buffer を持つ BUFFER_ALLOCATED を判定する。
    // [確認_正常系] - `cplat_string_catalog_filter_slot_format( slot_, dest, sizeof(dest), &actual_matched_with_argument, FILTER_TEST_TRACE_KEY_BUFFER_ALLOCATED, (const void *)nullptr, (size_t)0, FILTER_TEST_CONTEXT_ARGS(7))` の戻り値が `CPLAT_OK` であること。

    // Assert
    EXPECT_NE(0, actual_matched_with_argument); // [確認_正常系] - buffer を持つ項目は、値によらず真になること。

    // Act_2
    ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_format(
                            slot_, dest, sizeof(dest), &actual_matched_without_argument,
                            FILTER_TEST_TRACE_KEY_WORKER_STARTED, (uint32_t)1,
                            FILTER_TEST_CONTEXT_ARGS(7))); // [手順] - buffer を持たない WORKER_STARTED を判定する。
    // [確認_正常系] - `cplat_string_catalog_filter_slot_format( slot_, dest, sizeof(dest), &actual_matched_without_argument, FILTER_TEST_TRACE_KEY_WORKER_STARTED, (uint32_t)1, FILTER_TEST_CONTEXT_ARGS(7))` の戻り値が `CPLAT_OK` であること。

    // Assert_2
    EXPECT_EQ(0, actual_matched_without_argument); // [確認_正常系] - buffer を持たない項目は偽になること。
}
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.TearDown]

// arg[<n>] が、その項目に定義がない (UNUSED) 添字を指す場合は常に偽であることの確認
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.SetUp]
TEST_F(stringCatalogFilterSlotCompareTest, unused_argument_index_never_matches)
{
    // Arrange
    char dest[CPLAT_STRING_CATALOG_TEXT_MAX];
    int actual_matched = -1;
    int actual_ret;

    // [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.apply_filter]
    apply_filter("arg[3] == 1"); // [状態] - WORKER_STARTED では未使用の添字 3 と比較する条件式を適用する。

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_filter_slot_format(
        slot_, dest, sizeof(dest), &actual_matched, FILTER_TEST_TRACE_KEY_WORKER_STARTED, (uint32_t)1,
        FILTER_TEST_CONTEXT_ARGS(7)); // [手順] - WORKER_STARTED を判定する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, actual_matched);    // [確認_正常系] - 未使用の添字との比較は常に偽であること。
}
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.TearDown]

// 生成器が付与する文脈引数 (source_file_name、function_name) を条件式から照合できることの確認
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.SetUp]
TEST_F(stringCatalogFilterSlotCompareTest, generated_context_arguments_are_comparable)
{
    // Arrange
    char dest[CPLAT_STRING_CATALOG_TEXT_MAX];
    int actual_matched = -1;
    int actual_ret;

    // [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.apply_filter]
    apply_filter(
        "arg.source_file_name ends_with \".c\" && arg.function_name == \"fn\""); // [状態] - 文脈引数を対象とする条件式を適用する。

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_filter_slot_format(
        slot_, dest, sizeof(dest), &actual_matched, FILTER_TEST_TRACE_KEY_JOB_RECEIVED, (uint32_t)1, (uint64_t)1, "job",
        (int32_t)0,
        FILTER_TEST_CONTEXT_ARGS(7)); // [手順] - source_file_name="f.c"、function_name="fn" を渡す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_NE(0, actual_matched);    // [確認_正常系] - 文脈引数どうしの比較が成立すること。
}
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.TearDown]

// app が定義するコンテキスト引数 arg.sequence_number / arg[46] を、条件式から照合できることの確認
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.SetUp]
TEST_F(stringCatalogFilterSlotCompareTest, app_defined_sequence_number_context_argument_is_comparable)
{
    // Arrange
    char dest[CPLAT_STRING_CATALOG_TEXT_MAX];
    int actual_matched_equal = -1;
    int actual_matched_between = -1;
    int actual_matched_index_below = -1;
    int actual_matched_index_above = -1;

    // Pre-Assert

    // Act
    // [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.apply_filter]
    apply_filter("arg.sequence_number == 5"); // [手順] - sequence_number == 5 を適用する。
    ASSERT_EQ(CPLAT_OK,
              cplat_string_catalog_filter_slot_format(
                  slot_, dest, sizeof(dest), &actual_matched_equal, FILTER_TEST_TRACE_KEY_WORKER_STARTED, (uint32_t)1,
                  FILTER_TEST_CONTEXT_ARGS(5))); // [手順] - sequence_number に 5 を渡す。
    // [確認_正常系] - `cplat_string_catalog_filter_slot_format( slot_, dest, sizeof(dest), &actual_matched_equal, FILTER_TEST_TRACE_KEY_WORKER_STARTED, (uint32_t)1, FILTER_TEST_CONTEXT_ARGS(5))` の戻り値が `CPLAT_OK` であること。

    // Assert
    EXPECT_NE(0, actual_matched_equal); // [確認_正常系] - 一致する値を渡すと真になること。

    // Act_2
    // [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.apply_filter]
    apply_filter("arg.sequence_number between 90 and 99"); // [手順] - sequence_number の上限付近の範囲を適用する。
    ASSERT_EQ(CPLAT_OK,
              cplat_string_catalog_filter_slot_format(
                  slot_, dest, sizeof(dest), &actual_matched_between, FILTER_TEST_TRACE_KEY_WORKER_STARTED, (uint32_t)1,
                  FILTER_TEST_CONTEXT_ARGS(95))); // [手順] - sequence_number に 95 を渡す。
    // [確認_正常系] - `cplat_string_catalog_filter_slot_format( slot_, dest, sizeof(dest), &actual_matched_between, FILTER_TEST_TRACE_KEY_WORKER_STARTED, (uint32_t)1, FILTER_TEST_CONTEXT_ARGS(95))` の戻り値が `CPLAT_OK` であること。

    // Assert_2
    EXPECT_NE(0, actual_matched_between); // [確認_正常系] - 範囲内の値で真になること。

    // Act_3
    // [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.apply_filter]
    apply_filter("arg[46] < 10"); // [手順] - 添字指定で sequence_number を判定する条件式を適用する。
    ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_format(
                            slot_, dest, sizeof(dest), &actual_matched_index_below,
                            FILTER_TEST_TRACE_KEY_WORKER_STARTED, (uint32_t)1,
                            FILTER_TEST_CONTEXT_ARGS(5))); // [手順] - sequence_number に 5 を渡す。
    // [確認_正常系] - `cplat_string_catalog_filter_slot_format( slot_, dest, sizeof(dest), &actual_matched_index_below, FILTER_TEST_TRACE_KEY_WORKER_STARTED, (uint32_t)1, FILTER_TEST_CONTEXT_ARGS(5))` の戻り値が `CPLAT_OK` であること。

    // Assert_3
    EXPECT_NE(0, actual_matched_index_below); // [確認_正常系] - 添字指定でも、10 未満の値で真になること。

    // Act_4
    ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_format(
                            slot_, dest, sizeof(dest), &actual_matched_index_above,
                            FILTER_TEST_TRACE_KEY_WORKER_STARTED, (uint32_t)1,
                            FILTER_TEST_CONTEXT_ARGS(15))); // [手順] - sequence_number に 15 を渡す。
    // [確認_正常系] - `cplat_string_catalog_filter_slot_format( slot_, dest, sizeof(dest), &actual_matched_index_above, FILTER_TEST_TRACE_KEY_WORKER_STARTED, (uint32_t)1, FILTER_TEST_CONTEXT_ARGS(15))` の戻り値が `CPLAT_OK` であること。

    // Assert_4
    EXPECT_EQ(0, actual_matched_index_above); // [確認_正常系] - 添字指定でも、10 以上の値では偽になること。
}
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.TearDown]

// すべての項目が sequence_number を持つため、has(arg.sequence_number) が全キーで常に一致になることの確認
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.SetUp]
TEST_F(stringCatalogFilterSlotCompareTest, has_sequence_number_is_always_match_for_every_key)
{
    // Arrange
    static const int keys[] = {
        FILTER_TEST_TRACE_KEY_WORKER_STARTED, FILTER_TEST_TRACE_KEY_JOB_RECEIVED,
        FILTER_TEST_TRACE_KEY_JOB_PROGRESS,   FILTER_TEST_TRACE_KEY_BUFFER_ALLOCATED,
        FILTER_TEST_TRACE_KEY_JOB_FAILED,     FILTER_TEST_TRACE_KEY_COMMAND_RECEIVED,
        FILTER_TEST_TRACE_KEY_WORKER_STOPPED,
    };
    cplat_string_catalog_filter_state actual_states[7];
    bool actual_all_always_match = true;

    // [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.apply_filter]
    apply_filter("has(arg.sequence_number)"); // [状態] - sequence_number の有無を判定する条件式を適用する。

    // Pre-Assert

    // Act
    for (std::size_t index = 0; index < 7U; index++)
    {
        ASSERT_EQ(CPLAT_OK,
                  cplat_string_catalog_filter_slot_test(
                      slot_, keys[index], &actual_states[index])); // [手順] - 各キーの事前計算状態を取得する。
        // [確認_正常系 回数=7] - `cplat_string_catalog_filter_slot_test( slot_, keys[index], &actual_states[index])` の戻り値が `CPLAT_OK` であること。
        if (actual_states[index] != CPLAT_STRING_CATALOG_FILTER_STATE_ALWAYS_MATCH)
        {
            actual_all_always_match = false;
        }
    }

    // Assert
    EXPECT_TRUE(actual_all_always_match); // [確認_正常系] - 全キーが常に一致 (ALWAYS_MATCH) であること。
}
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.TearDown]

// arg.job_name (STRING) を整数定数と比較する型不一致の条件式が、常に偽であることの確認
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.SetUp]
TEST_F(stringCatalogFilterSlotCompareTest, type_mismatched_predicate_is_always_false)
{
    // Arrange
    char dest[CPLAT_STRING_CATALOG_TEXT_MAX];
    cplat_string_catalog_filter_state actual_state;
    int actual_matched = -1;
    int actual_ret;

    // [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.apply_filter]
    apply_filter("arg.job_name == 1"); // [状態] - STRING の引数を整数定数と比較する条件式を適用する。

    // Pre-Assert

    // Act
    (void)cplat_string_catalog_filter_slot_test(slot_, FILTER_TEST_TRACE_KEY_JOB_RECEIVED,
                                                &actual_state); // [手順] - 事前計算状態を取得する。
    actual_ret = cplat_string_catalog_filter_slot_format(
        slot_, dest, sizeof(dest), &actual_matched, FILTER_TEST_TRACE_KEY_JOB_RECEIVED, (uint32_t)1, (uint64_t)1, "job",
        (int32_t)0, FILTER_TEST_CONTEXT_ARGS(7)); // [手順] - 判定と書式展開を行う。

    // Assert
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_STATE_NEVER_MATCH,
              actual_state);         // [確認_正常系] - 型が一致しないため、事前計算で常に不一致となること。
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, actual_matched);    // [確認_正常系] - 判定結果が偽であること。
}
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.TearDown]

// 型が合わない != の比較要素も偽となり、どの項目でも成立しない行として無効になることの確認
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.SetUp]
TEST_F(stringCatalogFilterSlotCompareTest, type_mismatched_not_equal_predicate_is_false)
{
    // Arrange
    std::size_t actual_invalid = 0U;
    cplat_string_catalog_filter_line_error actual_error = CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE;
    static unsigned char image[kImageSize];

    ASSERT_EQ(CPLAT_OK,
              compile_single_line("arg.job_name != 1",
                                  image)); // [状態] - STRING の引数を整数定数と != で比較する条件式をコンパイルする。
    // [状態確認] - `compile_single_line("arg.job_name != 1", image)` の戻り値が `CPLAT_OK` であること。

    // Pre-Assert

    // Act
    ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_apply(slot_, image, kImageSize, nullptr, 0U,
                                                               &actual_invalid)); // [手順] - 条件式を適用する。
    // [確認_正常系] - `cplat_string_catalog_filter_slot_apply(slot_, image, kImageSize, nullptr, 0U, &actual_invalid)` の戻り値が `CPLAT_OK` であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_string_catalog_filter_slot_get_line_error(slot_, 0U,
                                                              &actual_error)); // [手順] - 行の状態を取得する。
    // [確認_正常系] - `cplat_string_catalog_filter_slot_get_line_error(slot_, 0U, &actual_error)` の戻り値が `CPLAT_OK` であること。

    // Assert
    EXPECT_EQ(1U, actual_invalid); // [確認_正常系] - != でも型が合わないため偽となり、行を無効にすること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NEVER_SATISFIABLE,
              actual_error); // [確認_正常系] - 原因が成立し得ない条件であること。
}
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.TearDown]

// 型が合わない比較要素は偽となり、否定すると真になることの確認
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.SetUp]
TEST_F(stringCatalogFilterSlotCompareTest, negated_type_mismatched_predicate_is_true)
{
    // Arrange
    char dest[CPLAT_STRING_CATALOG_TEXT_MAX];
    cplat_string_catalog_filter_state actual_state;
    int actual_matched = -1;
    int actual_ret;

    // [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.apply_filter]
    apply_filter("!(arg.job_name == 1)"); // [状態] - 型が合わない比較要素を否定する条件式を適用する。

    // Pre-Assert

    // Act
    (void)cplat_string_catalog_filter_slot_test(slot_, FILTER_TEST_TRACE_KEY_JOB_RECEIVED,
                                                &actual_state); // [手順] - 事前計算状態を取得する。
    actual_ret = cplat_string_catalog_filter_slot_format(
        slot_, dest, sizeof(dest), &actual_matched, FILTER_TEST_TRACE_KEY_JOB_RECEIVED, (uint32_t)1, (uint64_t)1, "job",
        (int32_t)0, FILTER_TEST_CONTEXT_ARGS(7)); // [手順] - 判定と書式展開を行う。

    // Assert
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_STATE_ALWAYS_MATCH,
              actual_state);         // [確認_正常系] - 偽の比較要素の否定により、常に一致となること。
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_NE(0, actual_matched);    // [確認_正常系] - 判定結果が真であること。
}
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.TearDown]

// 項目に無い引数を参照する比較要素は != でも偽となり、否定すると真になることの確認
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.SetUp]
TEST_F(stringCatalogFilterSlotCompareTest, missing_argument_predicate_is_false_and_negation_is_true)
{
    // Arrange
    cplat_string_catalog_filter_state actual_not_equal_state;
    cplat_string_catalog_filter_state actual_negated_state;

    // Pre-Assert

    // Act
    // [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.apply_filter]
    apply_filter("arg.job_name != \"x\""); // [手順] - job_name を != で比較する条件式を適用する。
    (void)cplat_string_catalog_filter_slot_test(
        slot_, FILTER_TEST_TRACE_KEY_WORKER_STARTED,
        &actual_not_equal_state);             // [手順] - job_name の無い項目の状態を取得する。
    // [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.apply_filter]
    apply_filter("!(arg.job_name == \"x\")"); // [手順] - job_name の比較を否定する条件式を適用する。
    (void)cplat_string_catalog_filter_slot_test(
        slot_, FILTER_TEST_TRACE_KEY_WORKER_STARTED,
        &actual_negated_state); // [手順] - job_name の無い項目の状態を取得する。

    // Assert
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_STATE_NEVER_MATCH,
              actual_not_equal_state); // [確認_正常系] - 引数が無い項目では != の比較要素も偽となること。
    EXPECT_EQ(CPLAT_STRING_CATALOG_FILTER_STATE_ALWAYS_MATCH,
              actual_negated_state); // [確認_正常系] - 偽の比較要素の否定により、常に一致となること。
}
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.TearDown]

// 一致の有無にかかわらず dest へ文字列が組み立てられ、戻り値が CPLAT_OK であることの確認
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.SetUp]
TEST_F(stringCatalogFilterSlotCompareTest, destination_is_formatted_regardless_of_match_result)
{
    // Arrange
    char actual_dest[CPLAT_STRING_CATALOG_TEXT_MAX];
    char expected_dest[CPLAT_STRING_CATALOG_TEXT_MAX];
    const int32_t sequence_number = 42;
    int actual_matched = -1;
    int actual_ret;
    int expected_ret;

    // [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.apply_filter]
    apply_filter("key == 999999"); // [状態] - どの項目のキーとも一致しない条件式を適用する。

    // Pre-Assert

    // Act
    actual_ret = cplat_string_catalog_filter_slot_format(
        slot_, actual_dest, sizeof(actual_dest), &actual_matched, FILTER_TEST_TRACE_KEY_WORKER_STARTED, (uint32_t)7,
        FILTER_TEST_CONTEXT_ARGS(sequence_number)); // [手順] - フィルターを通して文字列を組み立てる。
    expected_ret = cplat_string_catalog_format(
        filter_test_trace_catalog(), expected_dest, sizeof(expected_dest), FILTER_TEST_TRACE_KEY_WORKER_STARTED,
        (uint32_t)7,
        FILTER_TEST_CONTEXT_ARGS(sequence_number)); // [手順] - フィルターを介さず、同じ引数で直接組み立てる。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret);   // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, actual_matched);      // [確認_正常系] - どの行にも一致しないこと。
    EXPECT_EQ(CPLAT_OK, expected_ret); // [確認_正常系] - 比較対象の直接呼び出しも成功すること。
    EXPECT_STREQ(expected_dest,
                 actual_dest); // [確認_正常系] - 一致しない場合でも、直接呼び出しと同じ文字列が組み立てられること。
}
// [サブ手順参照 名前=stringCatalogFilterSlotCompareTest.TearDown]
