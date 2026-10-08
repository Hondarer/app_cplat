#include <testfw.h>

#include <cplat/base/result.h>
#include <cplat/hashtable/hashtable.h>
#include <mock_cplat.h>

#include "hashtable_inject.h"

#include <cstdint>
#include <cstring>
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

/* hashtable.c の hash_key と同じ djb2。バケットが変わる/一致するキーを探すために使う。 */
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
    static const char *const candidates[] = {"b", "c", "d", "e", "f", "g", "h", "i", "j", "k", nullptr};
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

class hashtableValidateTest : public Test
{
  protected:
    NiceMock<Mock_cplat> mock_cplat_;
};

// cplat_hashtable_validate が NULL ハンドルに対して INVALID_ARGUMENT を返すことの確認
TEST_F(hashtableValidateTest, rejects_null_handle)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret = cplat_hashtable_validate(NULL); // [手順] - NULL で validate を呼ぶ。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret); // [確認_異常系] - NULL ht が INVALID_ARGUMENT であること。
}

// バケット先頭リンクが capacity を超えている破損を CORRUPT_DESCRIPTOR として検出することの確認
TEST_F(hashtableValidateTest, detects_bucket_link_out_of_range)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;

    fill_config(&config, 2, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - capacity 2 の設定を用意する。
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [状態] - テーブルを構築しておく。
    test_hashtable_bucket_head(ht)[0] = 5; // [状態] - バケット先頭リンクへ capacity 超の値を書く。

    // Pre-Assert

    // Act
    int actual_ret = cplat_hashtable_validate(ht); // [手順] - 整合性を検証する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR,
              actual_ret); // [確認_異常系] - capacity を超えるバケット リンクが CORRUPT_DESCRIPTOR であること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュテーブルを破棄する。
}

// エントリチェインの循環破損を CORRUPT_DESCRIPTOR として検出することの確認
TEST_F(hashtableValidateTest, detects_link_cycle)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    const char *peer = nullptr;

    fill_config(&config, 2, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - capacity 2 の設定を用意する。
    peer = find_colliding_key("a", 2); // [状態] - "a" と同じバケットに割り当てられるキーを探す。

    // Pre-Assert
    ASSERT_NE(nullptr, peer); // [Pre-Assert確認_正常系] - 同一バケットの別キーが見つかること。

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    (void)cplat_hashtable_insert_direct(ht, 1, "a", 1, value.data(), &k_insert_timestamp,
                                        1); // [手順] - レコード 1 を使用中にする。
    (void)cplat_hashtable_insert_direct(ht, 2, peer, 1, value.data(), &k_insert_timestamp,
                                        2); // [手順] - 同一バケットのレコード 2 を使用中にする。
    *test_hashtable_entry_next(ht, 0) = 2;  // [手順] - レコード 1 の次リンクをレコード 2 へ向け、2 件の循環にする。
    int actual_ret = cplat_hashtable_validate(ht); // [手順] - 整合性を検証する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR,
              actual_ret); // [確認_異常系] - チェインの循環が CORRUPT_DESCRIPTOR であること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュテーブルを破棄する。
}

// 同一レコードへの重複リンク破損を CORRUPT_DESCRIPTOR として検出することの確認
TEST_F(hashtableValidateTest, detects_duplicate_visit)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);

    fill_config(&config, 2, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - capacity 2 の設定を用意する。
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [状態] - テーブルを構築しておく。
    (void)cplat_hashtable_add(ht, "a", value.data(),
                              CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [状態] - レコード 1 を使用中にする。
    test_hashtable_bucket_head(ht)[0] = 1; // [状態] - バケット 0 からもレコード 1 を指させる。
    test_hashtable_bucket_head(ht)[1] = 1; // [状態] - バケット 1 からもレコード 1 を指させる。

    // Pre-Assert

    // Act
    int actual_ret = cplat_hashtable_validate(ht); // [手順] - 整合性を検証する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR,
              actual_ret); // [確認_異常系] - 同じレコードへの二重リンクが CORRUPT_DESCRIPTOR であること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュテーブルを破棄する。
}

// 空スロットへのリンク破損を CORRUPT_DESCRIPTOR として検出することの確認
TEST_F(hashtableValidateTest, detects_linked_empty_slot)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;

    fill_config(&config, 2, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - capacity 2 の設定を用意する。
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [状態] - テーブルを構築しておく。
    test_hashtable_bucket_head(ht)[0] = 1; // [状態] - 空のレコード 1 をバケット 0 から直接リンクする。

    // Pre-Assert

    // Act
    int actual_ret = cplat_hashtable_validate(ht); // [手順] - 整合性を検証する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR,
              actual_ret); // [確認_異常系] - 空スロットへのリンクが CORRUPT_DESCRIPTOR であること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュテーブルを破棄する。
}

// 終端 NUL のないキー破損を CORRUPT_DESCRIPTOR として検出することの確認
TEST_F(hashtableValidateTest, detects_key_without_terminator)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);

    fill_config(&config, 2, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - key_size 8 の設定を用意する。
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [状態] - テーブルを構築しておく。
    (void)cplat_hashtable_add(ht, "a", value.data(),
                              CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [状態] - レコード 1 に文字列キーを格納する。
    std::memset(test_hashtable_entry_key(ht, 0), 'x', 8);             // [状態] - 格納キーを NUL 無しで埋め尽くす。

    // Pre-Assert

    // Act
    int actual_ret = cplat_hashtable_validate(ht); // [手順] - 整合性を検証する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR,
              actual_ret); // [確認_異常系] - NUL の無いキーが CORRUPT_DESCRIPTOR であること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュテーブルを破棄する。
}

// 格納キーとバケットのハッシュ不一致破損を CORRUPT_DESCRIPTOR として検出することの確認
TEST_F(hashtableValidateTest, detects_hash_mismatch)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    char mismatched_key[2] = {0, 0};
    size_t idx_a = 0;
    int i = 0;

    fill_config(&config, 2, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - capacity 2 の設定を用意する。
    idx_a = hash_string_mod("a", 2);
    for (i = 0; i < 26; i++) // [状態] - "a" と異なるバケットに割り当てられる 1 文字キーを探す。
    {
        char candidate_key[2] = {static_cast<char>('b' + i), 0};

        if (hash_string_mod(candidate_key, 2) != idx_a)
        {
            mismatched_key[0] = candidate_key[0];
            break;
        }
    }

    // Pre-Assert
    ASSERT_NE(0, mismatched_key[0]); // [Pre-Assert確認_正常系] - 異なるバケットに割り当てられるキーが見つかること。

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    (void)cplat_hashtable_add(ht, "a", value.data(),
                              CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - レコード 1 に "a" を格納する。
    std::memset(test_hashtable_entry_key(ht, 0), 0, 8);
    std::memcpy(test_hashtable_entry_key(ht, 0), mismatched_key,
                std::strlen(mismatched_key) + 1);  // [手順] - バケットは変えず格納キーだけを差し替える。
    int actual_ret = cplat_hashtable_validate(ht); // [手順] - 整合性を検証する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR,
              actual_ret); // [確認_異常系] - バケットと再計算ハッシュの不一致が CORRUPT_DESCRIPTOR であること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュテーブルを破棄する。
}

// 次リンクが capacity を超えている破損を CORRUPT_DESCRIPTOR として検出することの確認
TEST_F(hashtableValidateTest, detects_next_link_out_of_range)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);

    fill_config(&config, 2, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - capacity 2 の設定を用意する。
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [状態] - テーブルを構築しておく。
    (void)cplat_hashtable_add(ht, "a", value.data(),
                              CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [状態] - レコード 1 を使用中にする。
    *test_hashtable_entry_next(ht, 0) = 99; // [状態] - 次リンクへ capacity を超える値を書き込む。

    // Pre-Assert

    // Act
    int actual_ret = cplat_hashtable_validate(ht); // [手順] - 整合性を検証する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR,
              actual_ret); // [確認_異常系] - capacity を超える次リンクが CORRUPT_DESCRIPTOR であること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュテーブルを破棄する。
}

// どのバケットからもリンクされていない使用中レコードの破損を CORRUPT_DESCRIPTOR として検出することの確認
TEST_F(hashtableValidateTest, detects_unlinked_in_use_record)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;

    fill_config(&config, 2, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - capacity 2 の設定を用意する。
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [状態] - テーブルを構築しておく。
    *test_hashtable_entry_status(ht, 0) = 1; // [状態] - どのバケットからもリンクせずレコード 1 を使用中にする。

    // Pre-Assert

    // Act
    int actual_ret = cplat_hashtable_validate(ht); // [手順] - 整合性を検証する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR,
              actual_ret); // [確認_異常系] - 未リンクの使用中レコードが CORRUPT_DESCRIPTOR であること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュテーブルを破棄する。
}

// next_empty 管理変数が実態と一致しない破損を CORRUPT_DESCRIPTOR として検出することの確認
TEST_F(hashtableValidateTest, detects_next_empty_mismatch)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;

    fill_config(&config, 2, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - capacity 2 の設定を用意する。
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0,
                                 &ht);    // [状態] - 何も追加せずテーブルを構築する(next_empty は 1)。
    test_hashtable_set_next_empty(ht, 2); // [状態] - next_empty を実際の最小空きスロットと異なる値へ書き換える。

    // Pre-Assert

    // Act
    int actual_ret = cplat_hashtable_validate(ht); // [手順] - 整合性を検証する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR,
              actual_ret); // [確認_異常系] - next_empty の不一致が CORRUPT_DESCRIPTOR であること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュテーブルを破棄する。
}

// 使用中レコード件数 (in_use_count) の不一致破損を CORRUPT_DESCRIPTOR として検出することの確認
TEST_F(hashtableValidateTest, detects_in_use_count_mismatch)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 1);

    fill_config(&config, 2, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - capacity 2 の設定を用意する。
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [状態] - テーブルを構築しておく。
    (void)cplat_hashtable_add(
        ht, "a", value.data(),
        CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [状態] - 1 件追加する(in_use_count は 1 になる)。
    test_hashtable_set_counts(ht, 2, 0);        // [状態] - in_use_count を実際のスロット状態と異なる値へ書き換える。

    // Pre-Assert

    // Act
    int actual_ret = cplat_hashtable_validate(ht); // [手順] - 整合性を検証する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR,
              actual_ret); // [確認_異常系] - 実装中件数の不一致が CORRUPT_DESCRIPTOR であること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュテーブルを破棄する。
}

// 削除済みレコード件数 (deleted_count) の不一致破損を CORRUPT_DESCRIPTOR として検出することの確認
TEST_F(hashtableValidateTest, detects_deleted_count_mismatch)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 1);

    fill_config(&config, 2, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - capacity 2 の設定を用意する。
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [状態] - テーブルを構築しておく。
    (void)cplat_hashtable_add(ht, "a", value.data(),
                              CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [状態] - 1 件追加する。
    (void)cplat_hashtable_delete(ht, "a"); // [状態] - 削除する(in_use_count は 0、deleted_count は 1 になる)。
    test_hashtable_set_counts(ht, 0, 2); // [状態] - in_use_count は正しいまま、deleted_count だけ異なる値へ書き換える。

    // Pre-Assert

    // Act
    int actual_ret = cplat_hashtable_validate(ht); // [手順] - 整合性を検証する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR,
              actual_ret); // [確認_異常系] - 削除済み件数の不一致が CORRUPT_DESCRIPTOR であること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュテーブルを破棄する。
}

/*
 *  次は可変長値の空きリストに関する検査です。
 *  空きリストは、使用中ブロックと合わせてストレージ全体を過不足なく分割し、
 *  オフセット昇順かつ隣接する空きブロックが結合済みである必要があります。
 */

namespace
{

/** 固定長キーと可変長値を持つ設定を作ります。 */
cplat_hashtable_config variable_value_config(size_t capacity, size_t value_storage_size)
{
    cplat_hashtable_config config = {};

    config.capacity = capacity;
    config.key_type = CPLAT_HASHTABLE_FIELD_FIXED_STRING;
    config.value_type = CPLAT_HASHTABLE_FIELD_VARIABLE_STRING;
    config.key_size = 8;
    config.value_storage_size = value_storage_size;
    config.lifetime = 5;
    return config;
}

/** 空きリストの要素です。hashtable.c の struct hashtable_free_block と同じ配置です。 */
struct free_block_entry
{
    uint64_t offset;
    uint64_t length;
};

} // namespace

// 使用中ブロックと重複する空きブロックの破損を CORRUPT_DESCRIPTOR として検出することの確認
TEST_F(hashtableValidateTest, detects_free_block_overlapping_a_live_block)
{
    // Arrange
    cplat_hashtable_config config =
        variable_value_config(4, 32); // [状態] - capacity 4、値ストレージ 32 バイトの設定を用意する。
    cplat_hashtable *ht = nullptr;

    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [状態] - テーブルを構築しておく。
    (void)cplat_hashtable_add(ht, "a", "1111",
                              CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [状態] - 5 バイトの値を 1 件追加する。
    free_block_entry *free_blocks = static_cast<free_block_entry *>(test_hashtable_value_free_list(ht));
    free_blocks[0].offset = 0; // [状態] - 末尾の空きブロックの先頭を 0 へ書き換え、使用中ブロックと重ねる。

    // Pre-Assert

    // Act
    int actual_ret = cplat_hashtable_validate(ht); // [手順] - 整合性を検証する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR,
              actual_ret); // [確認_異常系] - 使用中ブロックと重なる空きブロックが CORRUPT_DESCRIPTOR であること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュテーブルを破棄する。
}

// 空きブロックの合計長が実態と食い違う破損を CORRUPT_DESCRIPTOR として検出することの確認
TEST_F(hashtableValidateTest, detects_free_list_total_mismatch)
{
    // Arrange
    cplat_hashtable_config config =
        variable_value_config(4, 32); // [状態] - capacity 4、値ストレージ 32 バイトの設定を用意する。
    cplat_hashtable *ht = nullptr;

    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [状態] - テーブルを構築しておく。
    (void)cplat_hashtable_add(ht, "a", "1111",
                              CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [状態] - 5 バイトの値を 1 件追加する。
    free_block_entry *free_blocks = static_cast<free_block_entry *>(test_hashtable_value_free_list(ht));
    free_blocks[0].length -= 1u; // [状態] - 末尾の空きブロックを 1 バイト縮め、空きの合計を実態と食い違わせる。

    // Pre-Assert

    // Act
    int actual_ret = cplat_hashtable_validate(ht); // [手順] - 整合性を検証する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR,
              actual_ret); // [確認_異常系] - 空きの合計の不一致が CORRUPT_DESCRIPTOR であること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュテーブルを破棄する。
}

// 未結合の隣接空きブロックが存在する破損を CORRUPT_DESCRIPTOR として検出することの確認
TEST_F(hashtableValidateTest, detects_adjacent_free_blocks_left_unmerged)
{
    // Arrange
    cplat_hashtable_config config =
        variable_value_config(4, 32); // [状態] - capacity 4、値ストレージ 32 バイトの設定を用意する。
    cplat_hashtable *ht = nullptr;

    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [状態] - テーブルを構築しておく。
    (void)cplat_hashtable_add(ht, "a", "1111",
                              CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [状態] - 5 バイトの値を 1 件追加する。
    free_block_entry *free_blocks = static_cast<free_block_entry *>(test_hashtable_value_free_list(ht));
    free_blocks[0].offset = 5;
    free_blocks[0].length = 10;
    free_blocks[1].offset = 15;
    free_blocks[1].length = 17; // [状態] - 末尾の空きブロックを、隣接したまま 2 個へ分割する。
    test_hashtable_set_value_free_count(ht, 2);

    // Pre-Assert

    // Act
    int actual_ret = cplat_hashtable_validate(ht); // [手順] - 整合性を検証する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR,
              actual_ret); // [確認_異常系] - 結合されていない隣接した空きブロックが CORRUPT_DESCRIPTOR であること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュテーブルを破棄する。
}

// 空きブロック個数が上限を超過している破損を CORRUPT_DESCRIPTOR として検出することの確認
TEST_F(hashtableValidateTest, detects_free_count_beyond_upper_bound)
{
    // Arrange
    cplat_hashtable_config config =
        variable_value_config(4, 32); // [状態] - capacity 4、値ストレージ 32 バイトの設定を用意する。
    cplat_hashtable *ht = nullptr;

    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [状態] - テーブルを構築しておく。
    test_hashtable_set_value_free_count(ht,
                                        6); // [状態] - 空きブロックの個数を上限 capacity + 1 より大きい値へ書き換える。

    // Pre-Assert

    // Act
    int actual_ret = cplat_hashtable_validate(ht); // [手順] - 整合性を検証する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR,
              actual_ret); // [確認_異常系] - 上限を超える空きブロックの個数が CORRUPT_DESCRIPTOR であること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュテーブルを破棄する。
}

// cplat_hashtable_attach で空きブロック個数が上限を超過している領域への接続が CORRUPT_DESCRIPTOR で拒否されることの確認
TEST_F(hashtableValidateTest, attach_rejects_free_count_beyond_upper_bound)
{
    // Arrange
    cplat_hashtable_config config =
        variable_value_config(4, 32); // [状態] - capacity 4、値ストレージ 32 バイトの設定を用意する。
    cplat_hashtable *ht = nullptr;
    cplat_hashtable *attached = nullptr;
    const void *mgmt = nullptr;
    const void *data = nullptr;
    size_t mgmt_size = 0;
    size_t data_size = 0;

    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [状態] - テーブルを構築しておく。
    test_hashtable_set_value_free_count(ht,
                                        6); // [状態] - 空きブロックの個数を上限 capacity + 1 より大きい値へ書き換える。
    (void)cplat_hashtable_buffer_size(ht, &mgmt_size, &data_size);
    (void)cplat_hashtable_buffer_ref(ht, &mgmt, &data);
    std::vector<unsigned char> mgmt_copy(static_cast<const unsigned char *>(mgmt),
                                         static_cast<const unsigned char *>(mgmt) + mgmt_size);
    std::vector<unsigned char> data_copy(static_cast<const unsigned char *>(data),
                                         static_cast<const unsigned char *>(data) + data_size);

    // Pre-Assert

    // Act
    int actual_ret = cplat_hashtable_attach(mgmt_copy.data(), mgmt_copy.size(), data_copy.data(), data_copy.size(),
                                            &attached); // [手順] - 壊れた領域へ再接続する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_CORRUPT_DESCRIPTOR,
        actual_ret); // [確認_異常系] - 上限を超える空きブロックの個数での attach が CORRUPT_DESCRIPTOR であること。
    EXPECT_EQ(nullptr, attached); // [確認_異常系] - 失敗後の ht_out が NULL であること。

    // Cleanup
    cplat_hashtable_dispose(attached);
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュテーブルを破棄する。
}
