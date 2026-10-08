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
        NULL,
        "FAKE_ID_0001",
        "first brief",
        "first details",
        NULL,
        {"first", NULL, NULL},
        {"", NULL, NULL},
    },
    {
        3,
        1,
        0,
        0,
        NULL,
        "FAKE_ID_0003",
        "second brief",
        "second details",
        NULL,
        {"second", NULL, NULL},
        {"", NULL, NULL},
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
static const cplat_string_catalog s_catalog_without_index = {s_entries, NULL, 2, 0};

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
        NULL,
        "OTHER_CATALOG_ID_0001",
        "other brief",
        "other details",
        NULL,
        {"other first", NULL, NULL},
        {"", NULL, NULL},
    },
};

/** @ref s_other_entries を参照するカタログです。 */
static const cplat_string_catalog s_other_catalog = {s_other_entries, NULL, 1, 0};

class stringCatalogCatalogTest : public Test
{
};

// 参照できないカタログが空のカタログとして振る舞うことの確認
TEST_F(stringCatalogCatalogTest, unusable_catalog_is_empty)
{
    // Arrange
    static const cplat_string_catalog null_entries = {NULL, NULL, 0, 0};
    static const cplat_string_catalog negative_count = {s_entries, NULL, -1, 0};
    static const cplat_string_catalog negative_index_count = {s_entries, s_key_index, 2, -1};

    // Pre-Assert

    // Act
    bool usable_null = cplat_internal_string_catalog_is_usable(NULL); // [手順] - NULL のカタログの使用可否を判定する。
    bool usable_null_entries =
        cplat_internal_string_catalog_is_usable(&null_entries); // [手順] - 配列が NULL のカタログの使用可否を判定する。
    bool usable_negative_count =
        cplat_internal_string_catalog_is_usable(&negative_count); // [手順] - 件数が負のカタログの使用可否を判定する。
    bool usable_negative_index = cplat_internal_string_catalog_is_usable(
        &negative_index_count); // [手順] - インデックス表の要素数が負のカタログの使用可否を判定する。
    bool usable_valid = cplat_internal_string_catalog_is_usable(
        &s_catalog_with_index); // [手順] - 妥当な構造のカタログの使用可否を判定する。

    int count_null = cplat_internal_string_catalog_entry_count(NULL); // [手順] - NULL の件数を取得する。
    const cplat_string_catalog_entry *find_null =
        cplat_internal_string_catalog_find_entry(NULL, 1); // [手順] - NULL からエントリを検索する。
    const cplat_string_catalog_entry *at_null =
        cplat_internal_string_catalog_entry_at(NULL, 0); // [手順] - NULL からインデックス指定で取得する。
    int count_null_entries =
        cplat_internal_string_catalog_entry_count(&null_entries); // [手順] - 配列が NULL の件数を取得する。

    // Assert
    EXPECT_FALSE(usable_null);           // [確認_異常系] - NULL は参照できないこと。
    EXPECT_FALSE(usable_null_entries);   // [確認_異常系] - 配列が NULL なら参照できないこと。
    EXPECT_FALSE(usable_negative_count); // [確認_異常系] - 件数が負なら参照できないこと。
    EXPECT_FALSE(usable_negative_index); // [確認_異常系] - インデックス表の要素数が負なら参照できないこと。
    EXPECT_TRUE(usable_valid);           // [確認_正常系] - 妥当な構造のカタログは参照できること。
    EXPECT_EQ(0, count_null);            // [確認_異常系] - NULL では件数が 0 であること。
    EXPECT_EQ(nullptr, find_null);       // [確認_異常系] - NULL では検索が NULL を返すこと。
    EXPECT_EQ(nullptr, at_null);         // [確認_異常系] - NULL ではインデックス指定取得が NULL を返すこと。
    EXPECT_EQ(0, count_null_entries);    // [確認_異常系] - 配列が NULL では件数が 0 であること。
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
    actual_count = cplat_internal_string_catalog_entry_count(&s_catalog_without_index); // [手順] - 件数を取得する。
    actual_entry_found = cplat_internal_string_catalog_find_entry(&s_catalog_without_index,
                                                                  3); // [手順] - 登録済みの文字列キーで検索する。
    actual_entry_unknown = cplat_internal_string_catalog_find_entry(&s_catalog_without_index,
                                                                    2); // [手順] - 未登録の文字列キーで検索する。

    // Assert
    EXPECT_EQ(2, actual_count);               // [確認_正常系] - カタログが持つ件数を返すこと。
    ASSERT_NE(nullptr, actual_entry_found);   // [確認_正常系] - カタログを取得できること。
    EXPECT_EQ(3, actual_entry_found->key);    // [確認_正常系] - 検索した文字列キーの定義であること。
    EXPECT_EQ(nullptr, actual_entry_unknown); // [確認_異常系] - 未登録の文字列キーでは NULL を返すこと。
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
        &s_catalog_with_index, 3); // [手順] - インデックス表に登録した文字列キーで検索する。
    actual_entry_absent =
        cplat_internal_string_catalog_find_entry(&s_catalog_with_index,
                                                 2); // [手順] - インデックス表が負の値を持つ文字列キーで検索する。
    actual_entry_over_index = cplat_internal_string_catalog_find_entry(
        &s_catalog_with_index, 9); // [手順] - インデックス表の範囲を超える文字列キーで検索する。
    actual_entry_negative_key =
        cplat_internal_string_catalog_find_entry(&s_catalog_with_index, -1); // [手順] - 負の文字列キーで検索する。

    // Assert
    ASSERT_NE(nullptr, actual_entry_found);  // [確認_正常系] - インデックス表からカタログを取得できること。
    EXPECT_EQ(3, actual_entry_found->key);   // [確認_正常系] - 検索した文字列キーの定義であること。
    EXPECT_EQ(nullptr, actual_entry_absent); // [確認_異常系] - インデックス表が負の値を持つ場合は NULL を返すこと。
    EXPECT_EQ(
        nullptr,
        actual_entry_over_index); // [確認_異常系] - インデックス表の範囲外では線形探索へフォールバックし、NULL を返すこと。
    EXPECT_EQ(nullptr, actual_entry_negative_key); // [確認_異常系] - 負の文字列キーでは NULL を返すこと。
}

// インデックス表が範囲外のインデックスを持つ場合に参照しないことの確認
TEST_F(stringCatalogCatalogTest, broken_key_index)
{
    // Arrange
    const cplat_string_catalog_entry *actual_entry;

    // Pre-Assert

    // Act
    actual_entry = cplat_internal_string_catalog_find_entry(
        &s_catalog_broken_index, 1); // [手順] - 範囲外のインデックスを持つインデックス表で検索する。

    // Assert
    EXPECT_EQ(nullptr, actual_entry); // [確認_異常系] - カタログを参照せずに NULL を返すこと。
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
    actual_entry = cplat_string_catalog_get_entry(&s_catalog_with_index,
                                                  1);         // [手順] - 登録済みの文字列キーで項目を取得する。
    actual_unknown = cplat_string_catalog_get_entry(NULL, 1); // [手順] - NULL のカタログで項目を取得する。
    actual_absent =
        cplat_string_catalog_get_entry(&s_catalog_with_index, 2); // [手順] - 未登録の文字列キーで項目を取得する。

    // Assert
    ASSERT_NE(nullptr, actual_entry);                 // [確認_正常系] - 登録済みの項目を取得できること。
    EXPECT_EQ(1, actual_entry->key);                  // [確認_正常系] - 指定した文字列キーの項目であること。
    EXPECT_STREQ("first brief", actual_entry->brief); // [確認_正常系] - メタデータを保持した項目を返すこと。
    EXPECT_EQ(nullptr, actual_unknown);               // [確認_異常系] - NULL のカタログでは NULL を返すこと。
    EXPECT_EQ(nullptr, actual_absent);                // [確認_異常系] - 未登録の文字列キーでは NULL を返すこと。
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
    actual_entry = cplat_internal_string_catalog_entry_at(&s_catalog_with_index,
                                                          1); // [手順] - インデックス指定で 2 件目を取得する。
    actual_entry_negative =
        cplat_internal_string_catalog_entry_at(&s_catalog_with_index, -1); // [手順] - 負のインデックスで取得する。
    actual_entry_over =
        cplat_internal_string_catalog_entry_at(&s_catalog_with_index,
                                               s_entry_count); // [手順] - 登録件数と同じインデックスで取得する。

    // Assert
    ASSERT_NE(nullptr, actual_entry);          // [確認_正常系] - カタログを取得できること。
    EXPECT_EQ(3, actual_entry->key);           // [確認_正常系] - インデックスに対応する文字列であること。
    EXPECT_EQ(nullptr, actual_entry_negative); // [確認_異常系] - 負のインデックスでは NULL を返すこと。
    EXPECT_EQ(nullptr, actual_entry_over);     // [確認_異常系] - 登録件数以上のインデックスでは NULL を返すこと。
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
    actual_entry_first =
        cplat_internal_string_catalog_find_entry(&s_catalog_with_index,
                                                 1); // [手順] - 1 つ目のカタログで文字列キー 1 を引く。
    actual_entry_other =
        cplat_internal_string_catalog_find_entry(&s_other_catalog,
                                                 1); // [手順] - 2 つ目のカタログで文字列キー 1 を引く。
    actual_entry_only_in_first = cplat_internal_string_catalog_find_entry(
        &s_other_catalog, 3); // [手順] - 1 つ目にだけある文字列キーを 2 つ目で引く。

    // Assert
    ASSERT_NE(nullptr, actual_entry_first); // [確認_正常系] - 1 つ目のカタログから取得できること。
    ASSERT_NE(nullptr, actual_entry_other); // [確認_正常系] - 2 つ目のカタログから取得できること。
    EXPECT_STREQ("FAKE_ID_0001",
                 actual_entry_first->id); // [確認_正常系] - 1 つ目の ID を返すこと。
    EXPECT_STREQ("OTHER_CATALOG_ID_0001",
                 actual_entry_other->id);       // [確認_正常系] - 2 つ目の ID を返すこと。
    EXPECT_EQ(3, actual_entry_first->category); // [確認_正常系] - 1 つ目の分類値を返すこと。
    EXPECT_EQ(7, actual_entry_other->category); // [確認_正常系] - 2 つ目の分類値を返すこと。
    EXPECT_EQ(nullptr,
              actual_entry_only_in_first); // [確認_異常系] - 一方にだけある文字列キーは他方から引けないこと。
}
