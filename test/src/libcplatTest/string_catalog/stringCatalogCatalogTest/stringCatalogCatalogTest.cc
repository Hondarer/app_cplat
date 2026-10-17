#include <testfw.h>

#include <cplat/string_catalog/catalog_internal.h>
#include <cplat/string_catalog/string_catalog.h>
#include <stddef.h>

/** 参照するカタログです。内容は参照の確認だけに使用します。第 2 要素は分類値です。 */
static const cplat_string_catalog_entry s_entries[] = {
    {
        1,
        3,
        0,
        0,
        nullptr,
        "FAKE_ID_0001",
        "first brief",
        "first details",
        nullptr,
        {"first", nullptr, nullptr},
        {"", nullptr, nullptr},
    },
    {
        3,
        1,
        0,
        0,
        nullptr,
        "FAKE_ID_0003",
        "second brief",
        "second details",
        nullptr,
        {"second", nullptr, nullptr},
        {"", nullptr, nullptr},
    },
};

/** @ref s_entries の要素数です。 */
static const int s_entry_count = (int)(sizeof(s_entries) / sizeof(s_entries[0]));

/** 文字列キーをインデックスとして @ref s_entries のインデックスを参照する表です。 */
static const int s_key_index[] = {-1, 0, -1, 1};

/** @ref s_key_index の要素数です。 */
static const int s_key_index_count = (int)(sizeof(s_key_index) / sizeof(s_key_index[0]));

/** 範囲外のインデックスを格納した、不正なインデックス表です。 */
static const int s_broken_key_index[] = {-1, 99};

/** インデックス表を持たないカタログです。検索は線形探索の経路を通ります。 */
static const cplat_string_catalog s_catalog_without_index = {s_entries, nullptr, 2, 0};

/** インデックス表を持つカタログです。 */
static const cplat_string_catalog s_catalog_with_index = {s_entries, s_key_index, 2, 4};

/** 不正なインデックス表を持つカタログです。 */
static const cplat_string_catalog s_catalog_broken_index = {s_entries, s_broken_key_index, 2, 2};

/** 同じ文字列キーに別の内容を持つ、2 つ目のカタログです。 */
static const cplat_string_catalog_entry s_other_entries[] = {
    {
        1,
        7,
        0,
        0,
        nullptr,
        "OTHER_CATALOG_ID_0001",
        "other brief",
        "other details",
        nullptr,
        {"other first", nullptr, nullptr},
        {"", nullptr, nullptr},
    },
};

/** @ref s_other_entries を参照するカタログです。 */
static const cplat_string_catalog s_other_catalog = {s_other_entries, nullptr, 1, 0};

class stringCatalogCatalogTest : public Test
{
};

// 参照できないカタログが空のカタログとして振る舞うことの確認
TEST_F(stringCatalogCatalogTest, unusable_catalog_is_empty)
{
    // Arrange
    static const cplat_string_catalog null_entries = {nullptr, nullptr, 0, 0};
    static const cplat_string_catalog negative_count = {s_entries, nullptr, -1, 0};
    static const cplat_string_catalog negative_index_count = {s_entries, s_key_index, 2, -1};

    // Pre-Assert

    // Act
    bool usable_null = cplat_internal_string_catalog_is_usable(
        nullptr); // [手順] - nullptr のカタログを指定して cplat_internal_string_catalog_is_usable() を呼び出す。
    bool usable_null_entries = cplat_internal_string_catalog_is_usable(
        &null_entries); // [手順] - 配列が nullptr のカタログを指定して cplat_internal_string_catalog_is_usable() を呼び出す。
    bool usable_negative_count = cplat_internal_string_catalog_is_usable(
        &negative_count); // [手順] - 件数が負のカタログを指定して cplat_internal_string_catalog_is_usable() を呼び出す。
    bool usable_negative_index = cplat_internal_string_catalog_is_usable(
        &negative_index_count); // [手順] - インデックス表の要素数が負のカタログを指定して cplat_internal_string_catalog_is_usable() を呼び出す。
    bool usable_valid = cplat_internal_string_catalog_is_usable(
        &s_catalog_with_index); // [手順] - 妥当な構造のカタログを指定して cplat_internal_string_catalog_is_usable() を呼び出す。

    int count_null = cplat_internal_string_catalog_entry_count(
        nullptr); // [手順] - nullptr を指定して cplat_internal_string_catalog_entry_count() を呼び出す。
    const cplat_string_catalog_entry *find_null = cplat_internal_string_catalog_find_entry(
        nullptr, 1); // [手順] - nullptr を指定して cplat_internal_string_catalog_find_entry() を呼び出す。
    const cplat_string_catalog_entry *at_null = cplat_internal_string_catalog_entry_at(
        nullptr, 0); // [手順] - nullptr を指定して cplat_internal_string_catalog_entry_at() を呼び出す。
    int count_null_entries = cplat_internal_string_catalog_entry_count(
        &null_entries); // [手順] - 配列が nullptr のカタログを指定して cplat_internal_string_catalog_entry_count() を呼び出す。

    // Assert
    EXPECT_FALSE(
        usable_null); // [確認_異常系] - nullptr のカタログに対する cplat_internal_string_catalog_is_usable() の戻り値が false であること。
    EXPECT_FALSE(
        usable_null_entries); // [確認_異常系] - 配列が nullptr のカタログに対する cplat_internal_string_catalog_is_usable() の戻り値が false であること。
    EXPECT_FALSE(
        usable_negative_count); // [確認_異常系] - 件数が負のカタログに対する cplat_internal_string_catalog_is_usable() の戻り値が false であること。
    EXPECT_FALSE(
        usable_negative_index); // [確認_異常系] - インデックス表の要素数が負のカタログに対する cplat_internal_string_catalog_is_usable() の戻り値が false であること。
    EXPECT_TRUE(
        usable_valid); // [確認_正常系] - 妥当な構造のカタログに対する cplat_internal_string_catalog_is_usable() の戻り値が true であること。
    EXPECT_EQ(
        0,
        count_null); // [確認_異常系] - nullptr のカタログに対する cplat_internal_string_catalog_entry_count() の戻り値が 0 であること。
    EXPECT_EQ(
        nullptr,
        find_null); // [確認_異常系] - nullptr のカタログに対する cplat_internal_string_catalog_find_entry() の戻り値が nullptr であること。
    EXPECT_EQ(
        nullptr,
        at_null); // [確認_異常系] - nullptr のカタログに対する cplat_internal_string_catalog_entry_at() の戻り値が nullptr であること。
    EXPECT_EQ(
        0,
        count_null_entries); // [確認_異常系] - 配列が nullptr のカタログに対する cplat_internal_string_catalog_entry_count() の戻り値が 0 であること。
}

// インデックス表を持たないカタログを線形探索で参照できることの確認
TEST_F(stringCatalogCatalogTest, find_without_key_index)
{
    // Arrange
    int actual_count;
    const cplat_string_catalog_entry *actual_entry_found;
    const cplat_string_catalog_entry *actual_entry_unknown;

    // Pre-Assert

    // Act
    actual_count = cplat_internal_string_catalog_entry_count(
        &s_catalog_without_index); // [手順] - インデックス表のないカタログを指定して cplat_internal_string_catalog_entry_count() を呼び出す。
    actual_entry_found = cplat_internal_string_catalog_find_entry(
        &s_catalog_without_index,
        3); // [手順] - 登録済みの文字列キーを指定して cplat_internal_string_catalog_find_entry() を呼び出す。
    actual_entry_unknown = cplat_internal_string_catalog_find_entry(
        &s_catalog_without_index,
        2); // [手順] - 未登録の文字列キーを指定して cplat_internal_string_catalog_find_entry() を呼び出す。

    // Assert
    EXPECT_EQ(2, actual_count); // [確認_正常系] - cplat_internal_string_catalog_entry_count() の戻り値が 2 であること。
    ASSERT_NE(
        nullptr,
        actual_entry_found); // [確認_正常系] - 登録済みの文字列キーに対する cplat_internal_string_catalog_find_entry() の戻り値が nullptr 以外であること。
    EXPECT_EQ(3, actual_entry_found->key); // [確認_正常系] - actual_entry_found->key が 3 であること。
    EXPECT_EQ(
        nullptr,
        actual_entry_unknown); // [確認_異常系] - 未登録の文字列キーに対する cplat_internal_string_catalog_find_entry() の戻り値が nullptr であること。
}

// インデックス表でカタログを参照できることの確認
TEST_F(stringCatalogCatalogTest, find_with_key_index)
{
    // Arrange
    const cplat_string_catalog_entry *actual_entry_found;
    const cplat_string_catalog_entry *actual_entry_absent;
    const cplat_string_catalog_entry *actual_entry_over_index;
    const cplat_string_catalog_entry *actual_entry_negative_key;

    // Pre-Assert

    // Act
    actual_entry_found = cplat_internal_string_catalog_find_entry(
        &s_catalog_with_index,
        3); // [手順] - インデックス表に登録した文字列キーを指定して cplat_internal_string_catalog_find_entry() を呼び出す。
    actual_entry_absent = cplat_internal_string_catalog_find_entry(
        &s_catalog_with_index,
        2); // [手順] - インデックス表が負の値を持つ文字列キーを指定して cplat_internal_string_catalog_find_entry() を呼び出す。
    actual_entry_over_index = cplat_internal_string_catalog_find_entry(
        &s_catalog_with_index,
        9); // [手順] - インデックス表の範囲を超える文字列キーを指定して cplat_internal_string_catalog_find_entry() を呼び出す。
    actual_entry_negative_key = cplat_internal_string_catalog_find_entry(
        &s_catalog_with_index,
        -1); // [手順] - 負の文字列キーを指定して cplat_internal_string_catalog_find_entry() を呼び出す。

    // Assert
    ASSERT_NE(
        nullptr,
        actual_entry_found); // [確認_正常系] - 登録済みの文字列キーに対する cplat_internal_string_catalog_find_entry() の戻り値が nullptr 以外であること。
    EXPECT_EQ(3, actual_entry_found->key); // [確認_正常系] - actual_entry_found->key が 3 であること。
    EXPECT_EQ(
        nullptr,
        actual_entry_absent); // [確認_異常系] - インデックス表が負の値を持つ文字列キーに対する cplat_internal_string_catalog_find_entry() の戻り値が nullptr であること。
    EXPECT_EQ(
        nullptr,
        actual_entry_over_index); // [確認_異常系] - インデックス表の範囲外の文字列キーに対する cplat_internal_string_catalog_find_entry() の戻り値が nullptr であること。
    EXPECT_EQ(
        nullptr,
        actual_entry_negative_key); // [確認_異常系] - 負の文字列キーに対する cplat_internal_string_catalog_find_entry() の戻り値が nullptr であること。
}

// インデックス表が範囲外のインデックスを持つ場合に参照しないことの確認
TEST_F(stringCatalogCatalogTest, broken_key_index)
{
    // Arrange
    const cplat_string_catalog_entry *actual_entry;

    // Pre-Assert

    // Act
    actual_entry = cplat_internal_string_catalog_find_entry(
        &s_catalog_broken_index,
        1); // [手順] - 範囲外のインデックスを持つインデックス表を指定して cplat_internal_string_catalog_find_entry() を呼び出す。

    // Assert
    EXPECT_EQ(
        nullptr,
        actual_entry); // [確認_異常系] - 不正なインデックス表に対する cplat_internal_string_catalog_find_entry() の戻り値が nullptr であること。
}

// 公開 API から文字列キーに対応するカタログ項目を取得できることの確認
TEST_F(stringCatalogCatalogTest, get_entry)
{
    // Arrange
    const cplat_string_catalog_entry *actual_entry;
    const cplat_string_catalog_entry *actual_unknown;
    const cplat_string_catalog_entry *actual_absent;

    // Pre-Assert

    // Act
    actual_entry = cplat_string_catalog_get_entry(
        &s_catalog_with_index,
        1); // [手順] - 登録済みの文字列キーを指定して cplat_string_catalog_get_entry() を呼び出す。
    actual_unknown = cplat_string_catalog_get_entry(
        nullptr, 1); // [手順] - nullptr のカタログを指定して cplat_string_catalog_get_entry() を呼び出す。
    actual_absent = cplat_string_catalog_get_entry(
        &s_catalog_with_index,
        2); // [手順] - 未登録の文字列キーを指定して cplat_string_catalog_get_entry() を呼び出す。

    // Assert
    ASSERT_NE(
        nullptr,
        actual_entry); // [確認_正常系] - 登録済みの文字列キーに対する cplat_string_catalog_get_entry() の戻り値が nullptr 以外であること。
    EXPECT_EQ(1, actual_entry->key); // [確認_正常系] - actual_entry->key が 1 であること。
    EXPECT_STREQ("first brief",
                 actual_entry->brief); // [確認_正常系] - actual_entry->brief が "first brief" であること。
    EXPECT_EQ(
        nullptr,
        actual_unknown); // [確認_異常系] - nullptr のカタログに対する cplat_string_catalog_get_entry() の戻り値が nullptr であること。
    EXPECT_EQ(
        nullptr,
        actual_absent); // [確認_異常系] - 未登録の文字列キーに対する cplat_string_catalog_get_entry() の戻り値が nullptr であること。
}

// インデックス指定でカタログを取得できることの確認
TEST_F(stringCatalogCatalogTest, entry_at)
{
    // Arrange
    const cplat_string_catalog_entry *actual_entry;
    const cplat_string_catalog_entry *actual_entry_negative;
    const cplat_string_catalog_entry *actual_entry_over;

    // Pre-Assert

    // Act
    actual_entry = cplat_internal_string_catalog_entry_at(
        &s_catalog_with_index,
        1); // [手順] - インデックス 1 (2 件目) を指定して cplat_internal_string_catalog_entry_at() を呼び出す。
    actual_entry_negative = cplat_internal_string_catalog_entry_at(
        &s_catalog_with_index,
        -1); // [手順] - 負のインデックスを指定して cplat_internal_string_catalog_entry_at() を呼び出す。
    actual_entry_over = cplat_internal_string_catalog_entry_at(
        &s_catalog_with_index,
        s_entry_count); // [手順] - 登録件数と同じインデックスを指定して cplat_internal_string_catalog_entry_at() を呼び出す。

    // Assert
    ASSERT_NE(
        nullptr,
        actual_entry); // [確認_正常系] - インデックス 1 に対する cplat_internal_string_catalog_entry_at() の戻り値が nullptr 以外であること。
    EXPECT_EQ(3, actual_entry->key); // [確認_正常系] - actual_entry->key が 3 であること。
    EXPECT_EQ(
        nullptr,
        actual_entry_negative); // [確認_異常系] - 負のインデックスに対する cplat_internal_string_catalog_entry_at() の戻り値が nullptr であること。
    EXPECT_EQ(
        nullptr,
        actual_entry_over); // [確認_異常系] - 登録件数と同じインデックスに対する cplat_internal_string_catalog_entry_at() の戻り値が nullptr であること。
}

// 複数のカタログが互いに影響しないことの確認
TEST_F(stringCatalogCatalogTest, multiple_catalogs_are_independent)
{
    // Arrange
    const cplat_string_catalog_entry *actual_entry_first;
    const cplat_string_catalog_entry *actual_entry_other;
    const cplat_string_catalog_entry *actual_entry_only_in_first;

    // Pre-Assert

    // Act
    actual_entry_first = cplat_internal_string_catalog_find_entry(
        &s_catalog_with_index,
        1); // [手順] - 1 つ目のカタログと文字列キー 1 を指定して cplat_internal_string_catalog_find_entry() を呼び出す。
    actual_entry_other = cplat_internal_string_catalog_find_entry(
        &s_other_catalog,
        1); // [手順] - 2 つ目のカタログと文字列キー 1 を指定して cplat_internal_string_catalog_find_entry() を呼び出す。
    actual_entry_only_in_first = cplat_internal_string_catalog_find_entry(
        &s_other_catalog,
        3); // [手順] - 2 つ目のカタログと、1 つ目にだけある文字列キーを指定して cplat_internal_string_catalog_find_entry() を呼び出す。

    // Assert
    ASSERT_NE(
        nullptr,
        actual_entry_first); // [確認_正常系] - 1 つ目のカタログに対する cplat_internal_string_catalog_find_entry() の戻り値が nullptr 以外であること。
    ASSERT_NE(
        nullptr,
        actual_entry_other); // [確認_正常系] - 2 つ目のカタログに対する cplat_internal_string_catalog_find_entry() の戻り値が nullptr 以外であること。
    EXPECT_STREQ("FAKE_ID_0001",
                 actual_entry_first->id); // [確認_正常系] - actual_entry_first->id が "FAKE_ID_0001" であること。
    EXPECT_STREQ(
        "OTHER_CATALOG_ID_0001",
        actual_entry_other->id); // [確認_正常系] - actual_entry_other->id が "OTHER_CATALOG_ID_0001" であること。
    EXPECT_EQ(3, actual_entry_first->category); // [確認_正常系] - actual_entry_first->category が 3 であること。
    EXPECT_EQ(7, actual_entry_other->category); // [確認_正常系] - actual_entry_other->category が 7 であること。
    EXPECT_EQ(
        nullptr,
        actual_entry_only_in_first); // [確認_異常系] - 1 つ目にだけある文字列キーに対する cplat_internal_string_catalog_find_entry() の戻り値が nullptr であること。
}
