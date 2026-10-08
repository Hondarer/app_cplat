#include <testfw.h>

#include <cplat/base/result.h>
#include <cplat/hashtable/hashtable.h>
#include <mock_cplat.h>

#include <cstring>
#include <string>
#include <vector>

namespace
{

cplat_timespec k_insert_timestamp = {1, 0};

void fill_config(cplat_hashtable_config *config, size_t capacity, size_t key_size, size_t value_size,
                 unsigned char lifetime, cplat_hashtable_key_type key_type)
{
    *config = {};
    config->capacity = capacity;
    config->key_type = key_type;
    config->timestamp_scope = CPLAT_HASHTABLE_TIMESTAMP_SCOPE_RECORD;
    config->key_size = key_size;
    config->value_size = value_size;
    config->lifetime = lifetime;
}

void fill_value(std::vector<unsigned char> *buf, const char *text)
{
    buf->assign(buf->size(), 0);
    if (text != nullptr)
    {
        std::memcpy(buf->data(), text, std::strlen(text));
    }
}

/* hashtable.c の hash_key と同じ djb2。同一バケットの別キーを用意するために使う。 */
size_t hash_string_mod(const char *key, size_t capacity)
{
    uint64_t hash = 5381;
    const unsigned char *p = reinterpret_cast<const unsigned char *>(key);
    int c = 0;

    while ((c = *p++) != 0)
    {
        hash = ((hash << 5) + hash) + static_cast<uint64_t>(c);
    }
    return static_cast<size_t>(hash) % capacity;
}

const char *find_colliding_key(const char *base, size_t capacity)
{
    static const char *const candidates[] = {"b", "c",  "d",  "e",  "f",  "g",  "h",  "i",  "j",
                                             "k", "aa", "ab", "ac", "ad", "ba", "bb", "bc", nullptr};
    const size_t target = hash_string_mod(base, capacity);
    size_t i = 0;

    for (i = 0; candidates[i] != nullptr; ++i)
    {
        if ((std::strcmp(candidates[i], base) != 0) && (hash_string_mod(candidates[i], capacity) == target))
        {
            return candidates[i];
        }
    }
    return nullptr;
}

} // namespace

class hashtableInsertDirectTest : public Test
{
  protected:
    NiceMock<Mock_cplat> mock_cplat_;
};

// 指定したレコード番号へ直接レコードを挿入でき、検索や状態取得、衝突キー配置が正しく動作することの確認
TEST_F(hashtableInsertDirectTest, places_in_use_at_requested_record)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    uint64_t rec = 0;
    int status = -1;
    size_t in_use = 0;
    size_t deleted = 0;
    size_t empty = 0;
    const void *found = nullptr;
    const char *peer = nullptr;

    fill_config(&config, 4, 16, 8, 5,
                CPLAT_HASHTABLE_KEY_STRING); // [状態] - capacity 4 の文字列テーブル設定を用意する。
    fill_value(&value, "keep");              // [状態] - 直接書き込む値を用意する。
    peer = find_colliding_key("c", 4);

    // Pre-Assert
    ASSERT_NE(nullptr, peer); // [Pre-Assert確認_正常系] - 同一バケットの別キーが見つかること。

    // Act
    int actual_ret_create = cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [手順] - テーブルを構築する。
    int actual_ret_direct = cplat_hashtable_insert_direct(ht, 3, "c", 1, value.data(), &k_insert_timestamp,
                                                          3); // [手順] - レコード 3 へ実装中で直接書き込む。
    int actual_ret_find = cplat_hashtable_find_value_ref(ht, "c", &found); // [手順] - キーで検索する。
    int actual_ret_rec = cplat_hashtable_find_recno(ht, "c", &rec);        // [手順] - レコード番号を取得する。
    int actual_ret_status = cplat_hashtable_get_status(ht, 3, &status);    // [手順] - レコード 3 の状態を取得する。
    fill_value(&value, "peer");
    int actual_ret_peer = cplat_hashtable_insert_direct(ht, 4, peer, 1, value.data(), &k_insert_timestamp,
                                                        4); // [手順] - 同一バケットの別キーをレコード 4 へ置く。
    fill_value(&value, "first");
    int actual_ret_add = cplat_hashtable_add(
        ht, "a", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 先頭空きへ通常追加する。
    int actual_ret_counts = cplat_hashtable_count_status(ht, &in_use, &deleted, &empty); // [手順] - 件数を取得する。
    int actual_ret_validate = cplat_hashtable_validate(ht);                              // [手順] - 整合性を検証する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_create);                 // [確認_正常系] - create が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_direct);                 // [確認_正常系] - insert_direct が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_find);                   // [確認_正常系] - 直接書き込んだキーが見つかること。
    EXPECT_STREQ("keep", static_cast<const char *>(found)); // [確認_正常系] - 値が一致すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_rec);                    // [確認_正常系] - find_recno が成功すること。
    EXPECT_EQ(3u, rec);                                     // [確認_正常系] - 指定したレコード番号が保たれること。
    EXPECT_EQ(CPLAT_OK, actual_ret_status);                 // [確認_正常系] - get_status が成功すること。
    EXPECT_EQ(1, status);                                   // [確認_正常系] - 状態が実装中であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_peer);                   // [確認_正常系] - 同一バケットの別キーを置けること。
    EXPECT_EQ(CPLAT_OK, actual_ret_add);                    // [確認_正常系] - 先頭空きへの add が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_counts);                 // [確認_正常系] - count_status が成功すること。
    EXPECT_EQ(3u, in_use);                                  // [確認_正常系] - 実装中が 3 件であること。
    EXPECT_EQ(0u, deleted);                                 // [確認_正常系] - 削除済みが 0 件であること。
    EXPECT_EQ(1u, empty);                                   // [確認_正常系] - 空が 1 件であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_validate);               // [確認_正常系] - validate が成功すること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - テーブルを破棄する。
}

// 指定レコード番号へ寿命内の削除済み状態として直接配置でき、検索対象外かつキー再配置が拒否されることの確認
TEST_F(hashtableInsertDirectTest, places_deleted_without_resurrecting_key)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    int status = -1;
    size_t deleted = 0;
    const void *found = nullptr;
    const void *key_out = nullptr;

    fill_config(&config, 4, 16, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - lifetime 5 の設定を用意する。
    fill_value(&value, "old");                                     // [状態] - 削除済みとして残す値を用意する。

    // Pre-Assert

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [手順] - テーブルを構築する。
    int actual_ret_direct = cplat_hashtable_insert_direct(ht, 2, "gone", 3, value.data(), &k_insert_timestamp,
                                                          2); // [手順] - 寿命内の削除済みをレコード 2 へ置く。
    int actual_ret_find = cplat_hashtable_find_value_ref(ht, "gone", &found); // [手順] - 削除済みキーを検索する。
    int actual_ret_status = cplat_hashtable_get_status(ht, 2, &status);       // [手順] - レコード 2 の状態を取得する。
    int actual_ret_key = cplat_hashtable_get_key_ref(ht, 2, &key_out);        // [手順] - レコード 2 のキーを読む。
    int actual_ret_deleted = cplat_hashtable_deleted_count(ht, &deleted);     // [手順] - 削除済み件数を取得する。
    int actual_ret_again = cplat_hashtable_insert_direct(ht, 3, "gone", 1, value.data(), &k_insert_timestamp,
                                                         3); // [手順] - 同じキーを別レコードへ置く。
    int actual_ret_validate = cplat_hashtable_validate(ht);  // [手順] - 整合性を検証する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_direct); // [確認_正常系] - 寿命内の削除済みを置けること。
    EXPECT_EQ(CPLAT_ERR_NOT_FOUND,
              actual_ret_find);                               // [確認_正常系] - 削除済みキーは検索対象にならないこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_status);                   // [確認_正常系] - get_status が成功すること。
    EXPECT_EQ(3, status);                                     // [確認_正常系] - 書き込んだ加齢値が保たれること。
    EXPECT_EQ(CPLAT_OK, actual_ret_key);                      // [確認_正常系] - 削除済みでもキーを読めること。
    EXPECT_STREQ("gone", static_cast<const char *>(key_out)); // [確認_正常系] - キーが一致すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_deleted);                  // [確認_正常系] - deleted_count が成功すること。
    EXPECT_EQ(1u, deleted);                                   // [確認_正常系] - 削除済みが 1 件であること。
    EXPECT_EQ(CPLAT_ERR_DUPLICATE_KEY,
              actual_ret_again); // [確認_異常系] - 削除済みキーの再配置が DUPLICATE_KEY であること。
    EXPECT_EQ(CPLAT_OK,
              actual_ret_validate); // [確認_正常系] - 削除済みをチェインへ載せたあと validate が成功すること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - テーブルを破棄する。
}

// 寿命設定上成立し得ない無効なステータス値の直接挿入がスキップされテーブルが変更されないことの確認
TEST_F(hashtableInsertDirectTest, skips_status_that_cannot_exist)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 7);
    int status = -1;
    size_t empty = 0;

    fill_config(&config, 2, 16, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - lifetime 5 の設定を用意する。

    // Pre-Assert

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [手順] - テーブルを構築する。
    int actual_ret_eq = cplat_hashtable_insert_direct(ht, 1, "a", 5, value.data(), &k_insert_timestamp,
                                                      1); // [手順] - status が lifetime と等しい値を置く。
    int actual_ret_over =
        cplat_hashtable_insert_direct(ht, 1, "a", CPLAT_HASHTABLE_LIFETIME_INFINITE, value.data(), &k_insert_timestamp,
                                      1); // [手順] - lifetime を超える加齢値を置く。
    fill_config(&config, 2, 16, 8, 2, CPLAT_HASHTABLE_KEY_STRING);
    cplat_hashtable_dispose(ht);
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [手順] - lifetime 2 のテーブルを構築する。
    int actual_ret_life2 = cplat_hashtable_insert_direct(ht, 1, "a", 2, value.data(), &k_insert_timestamp,
                                                         1); // [手順] - lifetime 2 では成立しない status 2 を置く。
    (void)cplat_hashtable_get_status(ht, 1, &status);
    (void)cplat_hashtable_empty_count(ht, &empty);

    // Assert
    EXPECT_EQ(CPLAT_SKIPPED, actual_ret_eq);    // [確認_正常系] - status == lifetime が SKIPPED であること。
    EXPECT_EQ(CPLAT_SKIPPED, actual_ret_over);  // [確認_正常系] - lifetime 超過が SKIPPED であること。
    EXPECT_EQ(CPLAT_SKIPPED, actual_ret_life2); // [確認_正常系] - lifetime 2 の status 2 が SKIPPED であること。
    EXPECT_EQ(0, status);                       // [確認_正常系] - SKIPPED 後もスロットが空のままであること。
    EXPECT_EQ(2u, empty);                       // [確認_正常系] - テーブルを変更していないこと。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - テーブルを破棄する。
}

// 直接挿入関数にNULLポインタ、無効なステータス、範囲外レコード番号、重複キー等を渡した場合にエラーとなることの確認
TEST_F(hashtableInsertDirectTest, rejects_invalid_arguments_and_collisions)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 3);
    char too_long[9];

    fill_config(&config, 4, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - key_size 8 の設定を用意する。
    std::memset(too_long, 'x', sizeof(too_long));                 // [状態] - NUL が無いキーを用意する。

    // Pre-Assert

    // Act
    int actual_ret_null_ht = cplat_hashtable_insert_direct(NULL, 1, "a", 1, value.data(), &k_insert_timestamp,
                                                           1); // [手順] - ht に NULL を渡す。
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    int actual_ret_null_key = cplat_hashtable_insert_direct(ht, 1, NULL, 1, value.data(), &k_insert_timestamp,
                                                            1); // [手順] - key に NULL を渡す。
    int actual_ret_null_value =
        cplat_hashtable_insert_direct(ht, 1, "a", 1, NULL, &k_insert_timestamp, 1); // [手順] - value に NULL を渡す。
    int actual_ret_null_timestamp =
        cplat_hashtable_insert_direct(ht, 1, "a", 1, value.data(), NULL, 1); // [手順] - timestamp に NULL を渡す。
    int actual_ret_status0 = cplat_hashtable_insert_direct(ht, 1, "a", 0, value.data(), &k_insert_timestamp,
                                                           1); // [手順] - status 0 を渡す。
    int actual_ret_status256 = cplat_hashtable_insert_direct(ht, 1, "a", 256, value.data(), &k_insert_timestamp,
                                                             1); // [手順] - status 256 を渡す。
    int actual_ret_rec0 = cplat_hashtable_insert_direct(ht, 0, "a", 5, value.data(), &k_insert_timestamp,
                                                        1); // [手順] - レコード番号 0 を渡す。
    int actual_ret_rec_hi = cplat_hashtable_insert_direct(ht, 5, "a", 5, value.data(), &k_insert_timestamp,
                                                          5); // [手順] - capacity 超のレコード番号を渡す。
    int actual_ret_long = cplat_hashtable_insert_direct(ht, 1, too_long, 1, value.data(), &k_insert_timestamp,
                                                        1); // [手順] - 長すぎるキーを渡す。
    int actual_ret_ok = cplat_hashtable_insert_direct(ht, 1, "a", 1, value.data(), &k_insert_timestamp,
                                                      1); // [手順] - レコード 1 へ実装中で置く。
    int actual_ret_occupied = cplat_hashtable_insert_direct(ht, 1, "b", 1, value.data(), &k_insert_timestamp,
                                                            1); // [手順] - 使用中のレコードへ別キーを置く。
    int actual_ret_dup_key = cplat_hashtable_insert_direct(ht, 2, "a", 1, value.data(), &k_insert_timestamp,
                                                           2); // [手順] - 既存キーを別レコードへ置く。
    (void)cplat_hashtable_delete(ht, "a");
    int actual_ret_deleted_slot = cplat_hashtable_insert_direct(ht, 1, "b", 1, value.data(), &k_insert_timestamp,
                                                                1); // [手順] - 削除済みスロットへ別キーを置く。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_null_ht); // [確認_異常系] - NULL ht が INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_null_key); // [確認_異常系] - NULL key が INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_null_value); // [確認_異常系] - NULL value が INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_null_timestamp); // [確認_異常系] - NULL timestamp が INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_status0); // [確認_異常系] - status 0 が INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_status256); // [確認_異常系] - status 256 が INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_rec0); // [確認_異常系] - レコード番号 0 が INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_rec_hi); // [確認_異常系] - 範囲外レコードが INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_OUT_OF_RANGE, actual_ret_long); // [確認_異常系] - 長すぎるキーが OUT_OF_RANGE であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_ok);                 // [確認_正常系] - 最初の直接書き込みが成功すること。
    EXPECT_EQ(CPLAT_ERR_DUPLICATE_DEFINITION,
              actual_ret_occupied); // [確認_異常系] - 使用中レコードが DUPLICATE_DEFINITION であること。
    EXPECT_EQ(CPLAT_ERR_DUPLICATE_KEY,
              actual_ret_dup_key); // [確認_異常系] - 既存キーが DUPLICATE_KEY であること。
    EXPECT_EQ(CPLAT_ERR_DUPLICATE_DEFINITION,
              actual_ret_deleted_slot); // [確認_異常系] - 削除済みスロットが DUPLICATE_DEFINITION であること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - テーブルを破棄する。
}

// 占有済みスロットへの挿入時でも成立しない寿命値のスキップ判定がスロット占有エラーより優先されることの確認
TEST_F(hashtableInsertDirectTest, skip_checked_before_occupancy)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    const void *found = nullptr;

    fill_config(&config, 2, 16, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - lifetime 5 の設定を用意する。
    fill_value(&value, "keep");                                    // [状態] - 既存値を用意する。

    // Pre-Assert

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    (void)cplat_hashtable_add(ht, "keep", value.data(),
                              CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - レコード 1 を占有する。
    fill_value(&value, "skip");
    int actual_ret_skip = cplat_hashtable_insert_direct(ht, 1, "other", 5, value.data(), &k_insert_timestamp,
                                                        1); // [手順] - 占有スロットへ成立しない寿命値を置く。
    int actual_ret_find = cplat_hashtable_find_value_ref(ht, "keep", &found); // [手順] - 既存キーを検索する。

    // Assert
    EXPECT_EQ(CPLAT_SKIPPED, actual_ret_skip);              // [確認_正常系] - 占有より先に SKIPPED になること。
    EXPECT_EQ(CPLAT_OK, actual_ret_find);                   // [確認_正常系] - 既存キーが残ること。
    EXPECT_STREQ("keep", static_cast<const char *>(found)); // [確認_正常系] - 既存値が変わらないこと。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - テーブルを破棄する。
}
