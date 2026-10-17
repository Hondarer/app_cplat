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

const unsigned char k_short_key[] = {0x01, 0x02};
const unsigned char k_short_key_with_zero[] = {0x01, 0x02, 0x00};
const unsigned char k_full_key[] = {0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

void fill_binary_config(cplat_hashtable_config *config, cplat_hashtable_timestamp_scope timestamp_scope)
{
    *config = {};
    config->capacity = 4;
    config->key_type = CPLAT_HASHTABLE_FIELD_BINARY;
    config->value_type = CPLAT_HASHTABLE_FIELD_BINARY;
    config->timestamp_scope = timestamp_scope;
    config->key_size = sizeof(k_full_key);
    config->value_size = sizeof(int);
    config->lifetime = 5;
}

int read_int_value(const cplat_hashtable *ht, const void *key, size_t key_length)
{
    int value = -1;
    size_t required = 0;

    (void)cplat_hashtable_find_value_copy_with_key_length(ht, key, key_length, &value, sizeof(value), &required);
    return value;
}

} // namespace

class hashtableKeyLengthTest : public Test
{
  protected:
    NiceMock<Mock_cplat> mock_cplat_;
};

// 有効長の異なるバイナリ キーが、0 埋め後の内容が同じでも別のキーとして扱われることの確認
TEST_F(hashtableKeyLengthTest, keys_with_different_lengths_are_distinct)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    const int value_short = 1;
    const int value_with_zero = 2;
    const int value_full = 3;
    size_t in_use = 0;

    fill_binary_config(&config, CPLAT_HASHTABLE_TIMESTAMP_SCOPE_TABLE);
    (void)cplat_hashtable_create(&config, nullptr, 0, nullptr, 0,
                                 &ht); // [状態] - バイナリ キーのテーブルを構築しておく。

    // Act
    int actual_ret_short = cplat_hashtable_add_with_key_length(
        ht, k_short_key, sizeof(k_short_key), &value_short,
        CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 有効長 2 のキーを指定して cplat_hashtable_add_with_key_length() を呼び出す。
    int actual_ret_with_zero = cplat_hashtable_add_with_key_length(
        ht, k_short_key_with_zero, sizeof(k_short_key_with_zero), &value_with_zero,
        CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 末尾に 0 を足した有効長 3 のキーを指定して cplat_hashtable_add_with_key_length() を呼び出す。
    int actual_ret_full = cplat_hashtable_add(
        ht, k_full_key, &value_full,
        CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 0 埋めした key_size のキーを指定して cplat_hashtable_add() を呼び出す。
    int actual_ret_count = cplat_hashtable_count(ht, &in_use);
    int actual_value_short = read_int_value(ht, k_short_key, sizeof(k_short_key));
    int actual_value_with_zero = read_int_value(ht, k_short_key_with_zero, sizeof(k_short_key_with_zero));
    int actual_value_full = read_int_value(ht, k_full_key, sizeof(k_full_key));
    int actual_ret_validate = cplat_hashtable_validate(ht);

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_short); // [確認_正常系] - 有効長 2 のキーを渡した cplat_hashtable_add_with_key_length() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_with_zero); // [確認_正常系] - 有効長 3 のキーを渡した cplat_hashtable_add_with_key_length() の戻り値が CPLAT_OK であること。(末尾の 0 だけが異なるキーが重複と判定されないことの確認)
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_full); // [確認_正常系] - 0 埋めした key_size のキーを渡した cplat_hashtable_add() の戻り値が CPLAT_OK であること。(有効長が key_size のキーが別のキーとして扱われることの確認)
    EXPECT_EQ(CPLAT_OK, actual_ret_count); // [確認_正常系] - cplat_hashtable_count() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(3u, in_use); // [確認_正常系] - in_use が 3 であること。(3 つのキーがすべて登録されたことの確認)
    EXPECT_EQ(value_short,
              actual_value_short); // [確認_正常系] - 有効長 2 のキーで読んだ値が value_short であること。
    EXPECT_EQ(value_with_zero,
              actual_value_with_zero); // [確認_正常系] - 有効長 3 のキーで読んだ値が value_with_zero であること。
    EXPECT_EQ(value_full, actual_value_full); // [確認_正常系] - 有効長 8 のキーで読んだ値が value_full であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_validate); // [確認_正常系] - cplat_hashtable_validate() の戻り値が CPLAT_OK であること。(有効長を記録したテーブルが整合していることの確認)

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// 有効長版の更新・検索・削除の各関数が、有効長で指定したキーへ作用することの確認
TEST_F(hashtableKeyLengthTest, key_length_variants_operate_on_the_given_length)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    const int value_first = 10;
    const int value_second = 20;
    const int value_third = 30;
    int inserted_first = -1;
    int inserted_second = -1;
    const void *value_ref = nullptr;
    uint64_t record = 0;
    const cplat_timespec *timestamp_ref = nullptr;
    cplat_timespec timestamp_val = {};
    uint64_t generation = 0;
    uint64_t record_after_delete = 0;

    fill_binary_config(&config, CPLAT_HASHTABLE_TIMESTAMP_SCOPE_RECORD);
    (void)cplat_hashtable_create(&config, nullptr, 0, nullptr, 0,
                                 &ht); // [状態] - レコード時刻を持つテーブルを構築しておく。

    // Act
    int actual_ret_upsert_insert = cplat_hashtable_upsert_with_key_length(
        ht, k_short_key, sizeof(k_short_key), &value_first,
        &inserted_first); // [手順] - 未登録の短いキーを指定して cplat_hashtable_upsert_with_key_length() を呼び出す。
    int actual_ret_upsert_update = cplat_hashtable_upsert_with_key_length(
        ht, k_short_key, sizeof(k_short_key), &value_second,
        &inserted_second); // [手順] - 同じ短いキーを指定して cplat_hashtable_upsert_with_key_length() を呼び出す。
    int actual_ret_update = cplat_hashtable_update_with_key_length(
        ht, k_short_key, sizeof(k_short_key),
        &value_third); // [手順] - 短いキーと新しい値を指定して cplat_hashtable_update_with_key_length() を呼び出す。
    int actual_ret_value_ref = cplat_hashtable_find_value_ref_with_key_length(
        ht, k_short_key, sizeof(k_short_key),
        &value_ref); // [手順] - 短いキーを指定して cplat_hashtable_find_value_ref_with_key_length() を呼び出す。
    int actual_ret_recno = cplat_hashtable_find_recno_with_key_length(
        ht, k_short_key, sizeof(k_short_key),
        &record); // [手順] - 短いキーを指定して cplat_hashtable_find_recno_with_key_length() を呼び出す。
    int actual_ret_timestamp_ref = cplat_hashtable_find_timestamp_ref_with_key_length(
        ht, k_short_key, sizeof(k_short_key),
        &timestamp_ref); // [手順] - 短いキーを指定して cplat_hashtable_find_timestamp_ref_with_key_length() を呼び出す。
    int actual_ret_timestamp_val = cplat_hashtable_find_timestamp_val_with_key_length(
        ht, k_short_key, sizeof(k_short_key),
        &timestamp_val); // [手順] - 短いキーを指定して cplat_hashtable_find_timestamp_val_with_key_length() を呼び出す。
    int actual_timestamp_compare = (timestamp_ref != nullptr)
                                       ? std::memcmp(timestamp_ref, &timestamp_val, sizeof(timestamp_val))
                                       : -1; // [手順] - 削除で 0 埋めされる前に、参照先と複製した変更時刻を比べる。
    int actual_ret_generation = cplat_hashtable_find_generation_with_key_length(
        ht, k_short_key, sizeof(k_short_key),
        &generation); // [手順] - 短いキーを指定して cplat_hashtable_find_generation_with_key_length() を呼び出す。
    int actual_ret_full_key = cplat_hashtable_find_recno(
        ht, k_full_key,
        &record_after_delete); // [手順] - 0 埋めした key_size のキーを指定して cplat_hashtable_find_recno() を呼び出す。
    int actual_ret_delete = cplat_hashtable_delete_with_key_length(
        ht, k_short_key,
        sizeof(k_short_key)); // [手順] - 短いキーを指定して cplat_hashtable_delete_with_key_length() を呼び出す。
    int actual_ret_after_delete = cplat_hashtable_find_recno_with_key_length(
        ht, k_short_key, sizeof(k_short_key),
        &record_after_delete); // [手順] - 削除後に短いキーを指定して cplat_hashtable_find_recno_with_key_length() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_upsert_insert); // [確認_正常系] - 未登録のキーに対する cplat_hashtable_upsert_with_key_length() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1,
              inserted_first); // [確認_正常系] - inserted_first が 1 であること。(新規追加として報告されたことの確認)
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_upsert_update); // [確認_正常系] - 登録済みのキーに対する cplat_hashtable_upsert_with_key_length() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0,
              inserted_second); // [確認_正常系] - inserted_second が 0 であること。(既存更新として報告されたことの確認)
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_update); // [確認_正常系] - cplat_hashtable_update_with_key_length() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_value_ref); // [確認_正常系] - cplat_hashtable_find_value_ref_with_key_length() の戻り値が CPLAT_OK であること。
    ASSERT_NE(nullptr, value_ref); // [確認_正常系] - value_ref が nullptr 以外であること。
    int actual_value = 0;
    std::memcpy(&actual_value, value_ref, sizeof(actual_value));
    EXPECT_EQ(value_third,
              actual_value); // [確認_正常系] - 参照先の値が value_third であること。(update が反映されたことの確認)
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_recno); // [確認_正常系] - cplat_hashtable_find_recno_with_key_length() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1u, record); // [確認_正常系] - record が 1 であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_timestamp_ref); // [確認_正常系] - cplat_hashtable_find_timestamp_ref_with_key_length() の戻り値が CPLAT_OK であること。
    ASSERT_NE(nullptr, timestamp_ref); // [確認_正常系] - timestamp_ref が nullptr 以外であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_timestamp_val); // [確認_正常系] - cplat_hashtable_find_timestamp_val_with_key_length() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, actual_timestamp_compare); // [確認_正常系] - 複製した変更時刻が参照先と一致すること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_generation); // [確認_正常系] - cplat_hashtable_find_generation_with_key_length() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        3u,
        generation); // [確認_正常系] - generation が 3 であること。(upsert 2 回と update 1 回で世代が進んだことの確認)
    EXPECT_EQ(
        CPLAT_ERR_NOT_FOUND,
        actual_ret_full_key); // [確認_正常系] - 0 埋めした key_size のキーを渡した cplat_hashtable_find_recno() の戻り値が CPLAT_ERR_NOT_FOUND であること。(有効長の異なるキーと一致しないことの確認)
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_delete); // [確認_正常系] - cplat_hashtable_delete_with_key_length() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_NOT_FOUND,
        actual_ret_after_delete); // [確認_正常系] - 削除後の cplat_hashtable_find_recno_with_key_length() の戻り値が CPLAT_ERR_NOT_FOUND であること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// 有効長版の insert_direct と find_value_copy が、有効長で指定したキーを扱うことの確認
TEST_F(hashtableKeyLengthTest, insert_direct_and_copy_with_key_length)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    const int value = 42;
    int copied = 0;
    size_t required = 0;

    fill_binary_config(&config, CPLAT_HASHTABLE_TIMESTAMP_SCOPE_TABLE);
    (void)cplat_hashtable_create(&config, nullptr, 0, nullptr, 0,
                                 &ht); // [状態] - バイナリ キーのテーブルを構築しておく。

    // Act
    int actual_ret_insert = cplat_hashtable_insert_direct_with_key_length(
        ht, 3, k_short_key, sizeof(k_short_key), 1, &value, nullptr,
        0); // [手順] - レコード 3 と短いキーを指定して cplat_hashtable_insert_direct_with_key_length() を呼び出す。
    int actual_ret_copy = cplat_hashtable_find_value_copy_with_key_length(
        ht, k_short_key, sizeof(k_short_key), &copied, sizeof(copied),
        &required); // [手順] - 短いキーを指定して cplat_hashtable_find_value_copy_with_key_length() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_insert); // [確認_正常系] - cplat_hashtable_insert_direct_with_key_length() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_copy); // [確認_正常系] - cplat_hashtable_find_value_copy_with_key_length() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(sizeof(int), required); // [確認_正常系] - required が sizeof(int) であること。
    EXPECT_EQ(value, copied);         // [確認_正常系] - copied が value であること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// 格納済みキーの有効長を get_key_ref と get_key_copy で取得でき、有効長の外が 0 埋めされることの確認
TEST_F(hashtableKeyLengthTest, get_key_returns_effective_length)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    const int value = 1;
    const void *key_ref = nullptr;
    size_t key_length = 0;
    std::vector<unsigned char> copied(sizeof(k_full_key), 0xFF);
    size_t required = 0;
    uint64_t record = 0;

    fill_binary_config(&config, CPLAT_HASHTABLE_TIMESTAMP_SCOPE_TABLE);
    (void)cplat_hashtable_create(&config, nullptr, 0, nullptr, 0,
                                 &ht); // [状態] - バイナリ キーのテーブルを構築しておく。
    (void)cplat_hashtable_add_with_key_length(
        ht, k_short_key, sizeof(k_short_key), &value,
        CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [状態] - 短いキーを追加しておく。
    (void)cplat_hashtable_find_recno_with_key_length(ht, k_short_key, sizeof(k_short_key),
                                                     &record); // [状態] - レコード番号を得ておく。

    // Act
    int actual_ret_ref = cplat_hashtable_get_key_ref(
        ht, record, &key_ref,
        &key_length); // [手順] - レコード番号を指定して cplat_hashtable_get_key_ref() を呼び出す。
    int actual_ret_copy = cplat_hashtable_get_key_copy(
        ht, record, copied.data(), copied.size(),
        &required); // [手順] - キーの複製先を指定して cplat_hashtable_get_key_copy() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret_ref); // [確認_正常系] - cplat_hashtable_get_key_ref() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(sizeof(k_short_key), key_length); // [確認_正常系] - key_length が 2 であること。
    ASSERT_NE(nullptr, key_ref);                // [確認_正常系] - key_ref が nullptr 以外であること。
    EXPECT_EQ(
        0,
        std::memcmp(
            k_full_key, key_ref,
            sizeof(
                k_full_key))); // [確認_正常系] - 参照先の key_size バイトが k_full_key と一致すること。(有効長の外が 0 埋めされたことの確認)
    EXPECT_EQ(CPLAT_OK,
              actual_ret_copy); // [確認_正常系] - cplat_hashtable_get_key_copy() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(sizeof(k_short_key), required); // [確認_正常系] - required が有効長の 2 であること。
    EXPECT_EQ(
        std::vector<unsigned char>({0x01, 0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}),
        copied); // [確認_正常系] - copied が {0x01, 0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF} と一致すること。(有効長だけを複製したことの確認)

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// 文字列キーのテーブルでは、get_key_ref が NUL を含む長さを返し、有効長版の関数を拒否することの確認
TEST_F(hashtableKeyLengthTest, string_key_table_rejects_key_length_variants)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    const int value = 1;
    const void *key_ref = nullptr;
    size_t key_length = 0;
    uint64_t record = 0;

    fill_binary_config(&config, CPLAT_HASHTABLE_TIMESTAMP_SCOPE_TABLE);
    config.key_type = CPLAT_HASHTABLE_FIELD_FIXED_STRING;
    (void)cplat_hashtable_create(&config, nullptr, 0, nullptr, 0,
                                 &ht); // [状態] - 固定長文字列キーのテーブルを構築しておく。
    (void)cplat_hashtable_add(ht, "abc", &value,
                              CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [状態] - キーを追加しておく。

    // Act
    int actual_ret_ref = cplat_hashtable_get_key_ref(
        ht, 1, &key_ref, &key_length); // [手順] - レコード 1 を指定して cplat_hashtable_get_key_ref() を呼び出す。
    int actual_ret_add = cplat_hashtable_add_with_key_length(
        ht, "xyz", 3, &value,
        CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 有効長 3 のキー "xyz" を指定して cplat_hashtable_add_with_key_length() を呼び出す。
    int actual_ret_find = cplat_hashtable_find_recno_with_key_length(
        ht, "abc", 3,
        &record); // [手順] - 有効長 3 のキー "abc" を指定して cplat_hashtable_find_recno_with_key_length() を呼び出す。
    int actual_ret_delete = cplat_hashtable_delete_with_key_length(
        ht, "abc",
        3); // [手順] - 有効長 3 のキー "abc" を指定して cplat_hashtable_delete_with_key_length() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret_ref); // [確認_正常系] - cplat_hashtable_get_key_ref() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(4u,
              key_length); // [確認_正常系] - key_length が 4 であること。(文字列長と終端 NUL の合計であることの確認)
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_add); // [確認_異常系] - 文字列キーのテーブルへの cplat_hashtable_add_with_key_length() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_find); // [確認_異常系] - 文字列キーのテーブルへの cplat_hashtable_find_recno_with_key_length() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_delete); // [確認_異常系] - 文字列キーのテーブルへの cplat_hashtable_delete_with_key_length() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// 有効長が key_size を超える場合と、ht に NULL を渡した場合を拒否することの確認
TEST_F(hashtableKeyLengthTest, rejects_invalid_key_length_arguments)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    const int value = 1;
    unsigned char long_key[sizeof(k_full_key) + 1] = {};
    uint64_t record = 0;

    fill_binary_config(&config, CPLAT_HASHTABLE_TIMESTAMP_SCOPE_TABLE);
    (void)cplat_hashtable_create(&config, nullptr, 0, nullptr, 0,
                                 &ht); // [状態] - バイナリ キーのテーブルを構築しておく。

    // Act
    int actual_ret_add_long = cplat_hashtable_add_with_key_length(
        ht, long_key, sizeof(long_key), &value,
        CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - key_size を超える有効長を指定して cplat_hashtable_add_with_key_length() を呼び出す。
    int actual_ret_find_long = cplat_hashtable_find_recno_with_key_length(
        ht, long_key, sizeof(long_key),
        &record); // [手順] - key_size を超える有効長を指定して cplat_hashtable_find_recno_with_key_length() を呼び出す。
    int actual_ret_null_ht = cplat_hashtable_add_with_key_length(
        nullptr, k_short_key, sizeof(k_short_key), &value,
        CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - ht に nullptr を指定して cplat_hashtable_add_with_key_length() を呼び出す。
    int actual_ret_null_key = cplat_hashtable_find_recno_with_key_length(
        ht, nullptr, 0,
        &record); // [手順] - key に nullptr を指定して cplat_hashtable_find_recno_with_key_length() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_OUT_OF_RANGE,
        actual_ret_add_long); // [確認_異常系] - key_size を超える有効長を渡した cplat_hashtable_add_with_key_length() の戻り値が CPLAT_ERR_OUT_OF_RANGE であること。
    EXPECT_EQ(
        CPLAT_ERR_OUT_OF_RANGE,
        actual_ret_find_long); // [確認_異常系] - key_size を超える有効長を渡した cplat_hashtable_find_recno_with_key_length() の戻り値が CPLAT_ERR_OUT_OF_RANGE であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_ht); // [確認_異常系] - ht に nullptr を渡した cplat_hashtable_add_with_key_length() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_key); // [確認_異常系] - key に nullptr を渡した cplat_hashtable_find_recno_with_key_length() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// 有効長 0 のキーを登録でき、すべて 0 の key_size のキーとは別のキーとして扱われることの確認
TEST_F(hashtableKeyLengthTest, accepts_zero_length_key)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    const int value_empty = 1;
    const int value_zeros = 2;
    const unsigned char zeros[sizeof(k_full_key)] = {};

    fill_binary_config(&config, CPLAT_HASHTABLE_TIMESTAMP_SCOPE_TABLE);
    (void)cplat_hashtable_create(&config, nullptr, 0, nullptr, 0,
                                 &ht); // [状態] - バイナリ キーのテーブルを構築しておく。

    // Act
    int actual_ret_empty = cplat_hashtable_add_with_key_length(
        ht, zeros, 0, &value_empty,
        CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 有効長 0 のキーを指定して cplat_hashtable_add_with_key_length() を呼び出す。
    int actual_ret_zeros = cplat_hashtable_add(
        ht, zeros, &value_zeros,
        CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - すべて 0 のキーを指定して cplat_hashtable_add() を呼び出す。
    int actual_value_empty = read_int_value(ht, zeros, 0);
    int actual_value_zeros = read_int_value(ht, zeros, sizeof(zeros));

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_empty); // [確認_正常系] - 有効長 0 を渡した cplat_hashtable_add_with_key_length() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_zeros); // [確認_正常系] - すべて 0 のキーを渡した cplat_hashtable_add() の戻り値が CPLAT_OK であること。(有効長 0 のキーと重複しないことの確認)
    EXPECT_EQ(value_empty, actual_value_empty); // [確認_正常系] - 有効長 0 のキーで読んだ値が value_empty であること。
    EXPECT_EQ(value_zeros, actual_value_zeros); // [確認_正常系] - すべて 0 のキーで読んだ値が value_zeros であること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// 有効長を記録したテーブルを再接続と拡大の後も同じ有効長で検索できることの確認
TEST_F(hashtableKeyLengthTest, key_length_survives_attach_and_resize)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable_config grown = {};
    cplat_hashtable *ht = nullptr;
    cplat_hashtable *attached = nullptr;
    const int value = 7;
    size_t mgmt_size = 0;
    size_t data_size = 0;
    const void *mgmt = nullptr;
    const void *data = nullptr;

    fill_binary_config(&config, CPLAT_HASHTABLE_TIMESTAMP_SCOPE_TABLE);
    (void)cplat_hashtable_create(&config, nullptr, 0, nullptr, 0,
                                 &ht); // [状態] - バイナリ キーのテーブルを構築しておく。
    (void)cplat_hashtable_add_with_key_length(
        ht, k_short_key, sizeof(k_short_key), &value,
        CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [状態] - 短いキーを追加しておく。
    (void)cplat_hashtable_buffer_size(ht, &mgmt_size, &data_size);
    (void)cplat_hashtable_buffer_ref(ht, &mgmt, &data);
    std::vector<uint64_t> mgmt_copy((mgmt_size + sizeof(uint64_t) - 1) / sizeof(uint64_t));
    std::vector<uint64_t> data_copy((data_size + sizeof(uint64_t) - 1) / sizeof(uint64_t) + 1);
    std::memcpy(mgmt_copy.data(), mgmt, mgmt_size);
    std::memcpy(data_copy.data(), data, data_size);
    grown = config;
    grown.capacity = 16;

    // Act
    int actual_ret_attach =
        cplat_hashtable_attach(mgmt_copy.data(), mgmt_size, data_copy.data(), data_size,
                               &attached); // [手順] - 複製した領域を指定して cplat_hashtable_attach() を呼び出す。
    int actual_value_attached = read_int_value(attached, k_short_key, sizeof(k_short_key));
    int actual_ret_resize = cplat_hashtable_resize(ht, &grown); // [手順] - cplat_hashtable_resize() を呼び出す。
    int actual_value_resized = read_int_value(ht, k_short_key, sizeof(k_short_key));
    int actual_ret_validate = cplat_hashtable_validate(ht);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_attach); // [確認_正常系] - cplat_hashtable_attach() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(value,
              actual_value_attached); // [確認_正常系] - 再接続したテーブルを短いキーで読んだ値が value であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_resize); // [確認_正常系] - cplat_hashtable_resize() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        value,
        actual_value_resized); // [確認_正常系] - 拡大したテーブルを短いキーで読んだ値が value であること。(移行で有効長が引き継がれたことの確認)
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_validate); // [確認_正常系] - 拡大後の cplat_hashtable_validate() の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_hashtable_dispose(attached);
    cplat_hashtable_dispose(ht);
}

// 格納済みの有効長が key_size を超える破損と、有効長の外が 0 でない破損を検出することの確認
TEST_F(hashtableKeyLengthTest, validate_detects_broken_key_length)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht_long = nullptr;
    cplat_hashtable *ht_padding = nullptr;
    const int value = 1;

    fill_binary_config(&config, CPLAT_HASHTABLE_TIMESTAMP_SCOPE_TABLE);
    (void)cplat_hashtable_create(&config, nullptr, 0, nullptr, 0,
                                 &ht_long); // [状態] - 有効長を壊すテーブルを構築しておく。
    (void)cplat_hashtable_create(&config, nullptr, 0, nullptr, 0,
                                 &ht_padding); // [状態] - 0 埋めを壊すテーブルを構築しておく。
    (void)cplat_hashtable_add_with_key_length(
        ht_long, k_short_key, sizeof(k_short_key), &value,
        CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [状態] - 短いキーを追加しておく。
    (void)cplat_hashtable_add_with_key_length(
        ht_padding, k_short_key, sizeof(k_short_key), &value,
        CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [状態] - 短いキーを追加しておく。
    *test_hashtable_entry_key_length(ht_long, 0) =
        static_cast<uint32_t>(sizeof(k_full_key) + 1); // [状態] - 有効長へ key_size を超える値を書く。
    test_hashtable_entry_key(ht_padding, 0)[sizeof(k_short_key)] = 0x7F; // [状態] - 有効長の直後へ 0 以外を書く。

    // Act
    int actual_ret_long = cplat_hashtable_validate(
        ht_long); // [手順] - 有効長を壊したテーブルを指定して cplat_hashtable_validate() を呼び出す。
    int actual_ret_padding = cplat_hashtable_validate(
        ht_padding); // [手順] - 0 埋めを壊したテーブルを指定して cplat_hashtable_validate() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_CORRUPT_DESCRIPTOR,
        actual_ret_long); // [確認_異常系] - 有効長が key_size を超えるときの cplat_hashtable_validate() の戻り値が CPLAT_ERR_CORRUPT_DESCRIPTOR であること。
    EXPECT_EQ(
        CPLAT_ERR_CORRUPT_DESCRIPTOR,
        actual_ret_padding); // [確認_異常系] - 有効長の外が 0 でないときの cplat_hashtable_validate() の戻り値が CPLAT_ERR_CORRUPT_DESCRIPTOR であること。

    // Cleanup
    cplat_hashtable_dispose(ht_long);
    cplat_hashtable_dispose(ht_padding);
}

// key_size が UINT32_MAX を超えるバイナリ キーの設定を拒否することの確認
TEST_F(hashtableKeyLengthTest, rejects_binary_key_size_over_uint32_max)
{
    // Arrange
    cplat_hashtable_config config = {};
    size_t mgmt_size = 0;
    size_t data_size = 0;

    fill_binary_config(&config, CPLAT_HASHTABLE_TIMESTAMP_SCOPE_TABLE);
    config.key_size = static_cast<size_t>(UINT32_MAX) + 1u; // [状態] - UINT32_MAX を超える key_size を設定する。

    // Act
    int actual_ret = cplat_hashtable_required_size(
        &config, &mgmt_size, &data_size); // [手順] - 設定を指定して cplat_hashtable_required_size() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret); // [確認_異常系] - key_size が UINT32_MAX を超える設定を渡した cplat_hashtable_required_size() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}
