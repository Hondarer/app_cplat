#include <testfw.h>

#include <cplat/base/result.h>
#include <cplat/hashtable/hashtable.h>
#include <mock_cplat.h>

#include <cstring>
#include <vector>

namespace
{

void fill_config(cplat_hashtable_config *config, size_t capacity, unsigned char lifetime)
{
    *config = {};
    config->capacity = capacity;
    config->key_type = CPLAT_HASHTABLE_FIELD_FIXED_STRING;
    config->value_type = CPLAT_HASHTABLE_FIELD_FIXED_BINARY;
    config->timestamp_scope = CPLAT_HASHTABLE_TIMESTAMP_SCOPE_RECORD;
    config->key_size = 8;
    config->value_size = 8;
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

class hashtableUpsertTest : public Test
{
  protected:
    NiceMock<Mock_cplat> mock_cplat_;
};

// cplat_hashtable_upsert で未登録キーの新規追加と登録済みキーの値更新が正しく行われることの確認
TEST_F(hashtableUpsertTest, inserts_then_updates_the_same_key)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    std::vector<unsigned char> read_back(8, 0);
    int inserted_first = -1;
    int inserted_second = -1;
    size_t in_use = 0;
    size_t required = 0;

    fill_config(&config, 4, 5);
    fill_value(&value, "v1");

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    int actual_ret_first = cplat_hashtable_upsert(ht, "a", value.data(),
                                                  &inserted_first); // [手順] - 未登録のキーを upsert する。
    fill_value(&value, "v2");
    int actual_ret_second = cplat_hashtable_upsert(ht, "a", value.data(),
                                                   &inserted_second); // [手順] - 同じキーを再度 upsert する。
    int actual_ret_read = cplat_hashtable_find_value_copy(ht, "a", read_back.data(), read_back.size(), &required);
    int actual_ret_count = cplat_hashtable_count(ht, &in_use);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_first);  // [確認_正常系] - 未登録のキーの upsert が成功すること。
    EXPECT_EQ(1, inserted_first);           // [確認_正常系] - 未登録のキーは新規追加として報告されること。
    EXPECT_EQ(CPLAT_OK, actual_ret_second); // [確認_正常系] - 登録済みのキーの upsert が成功すること。
    EXPECT_EQ(0, inserted_second);          // [確認_正常系] - 登録済みのキーは既存更新として報告されること。
    EXPECT_EQ(CPLAT_OK, actual_ret_read);
    // [確認_正常系] - `cplat_hashtable_find_value_copy(ht, "a", read_back.data(), read_back.size(), &required)` の戻り値が `CPLAT_OK` であること。
    EXPECT_STREQ("v2", reinterpret_cast<const char *>(read_back.data())); // [確認_正常系] - 値が更新されていること。
    EXPECT_EQ(CPLAT_OK, actual_ret_count);
    // [確認_正常系] - `cplat_hashtable_count(ht, &in_use)` の戻り値が `CPLAT_OK` であること。
    EXPECT_EQ(1u, in_use); // [確認_正常系] - 更新では使用中件数が増えないこと。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// cplat_hashtable_upsert の inserted 出力ポインタに NULL を渡しても新規追加および更新が成功することの確認
TEST_F(hashtableUpsertTest, accepts_null_inserted_out)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);

    fill_config(&config, 4, 5);
    fill_value(&value, "v1");

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    int actual_ret_insert =
        cplat_hashtable_upsert(ht, "a", value.data(), NULL); // [手順] - 格納先を省いて upsert する。
    int actual_ret_update =
        cplat_hashtable_upsert(ht, "a", value.data(), NULL); // [手順] - 格納先を省いて再度 upsert する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_insert); // [確認_正常系] - 格納先が NULL でも新規追加できること。
    EXPECT_EQ(CPLAT_OK, actual_ret_update); // [確認_正常系] - 格納先が NULL でも既存更新できること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// cplat_hashtable_upsert で削除済みレコードのキーを指定した場合に指定した値で新規追加として復活することの確認
TEST_F(hashtableUpsertTest, revives_deleted_key_with_the_given_value)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    std::vector<unsigned char> read_back(8, 0);
    int inserted = -1;
    int status = -1;
    size_t required = 0;

    fill_config(&config, 4, CPLAT_HASHTABLE_LIFETIME_INFINITE);
    fill_value(&value, "v1");

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    (void)cplat_hashtable_add(ht, "a", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_delete(ht, "a"); // [手順] - キーを削除済みにする。
    fill_value(&value, "v2");
    int actual_ret_upsert =
        cplat_hashtable_upsert(ht, "a", value.data(), &inserted); // [手順] - 削除済みのキーを upsert する。
    int actual_ret_status = cplat_hashtable_get_status(ht, 1, &status);
    int actual_ret_read = cplat_hashtable_find_value_copy(ht, "a", read_back.data(), read_back.size(), &required);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_upsert); // [確認_正常系] - 削除済みのキーの upsert が成功すること。
    EXPECT_EQ(1, inserted);                 // [確認_正常系] - 削除済みからの復活は新規追加として報告されること。
    EXPECT_EQ(CPLAT_OK, actual_ret_status);
    // [確認_正常系] - `cplat_hashtable_get_status(ht, 1, &status)` の戻り値が `CPLAT_OK` であること。
    EXPECT_EQ(1, status); // [確認_正常系] - レコードが使用中に戻ること。
    EXPECT_EQ(CPLAT_OK, actual_ret_read);
    // [確認_正常系] - `cplat_hashtable_find_value_copy(ht, "a", read_back.data(), read_back.size(), &required)` の戻り値が `CPLAT_OK` であること。
    EXPECT_STREQ("v2", reinterpret_cast<const char *>(
                           read_back.data())); // [確認_正常系] - 削除前の値ではなく渡した値で復活すること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// ハッシュテーブル満杯時に cplat_hashtable_upsert で新規キーを追加すると LIMIT_EXCEEDED となり既存キーの更新は成功することの確認
TEST_F(hashtableUpsertTest, reports_limit_exceeded_when_table_is_full)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    int inserted = -1;

    fill_config(&config, 2, 5); // [状態] - レコード数 2 の設定を用意する。
    fill_value(&value, "v");

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    (void)cplat_hashtable_upsert(ht, "a", value.data(), NULL);
    (void)cplat_hashtable_upsert(ht, "b", value.data(), NULL);
    int actual_ret_full =
        cplat_hashtable_upsert(ht, "c", value.data(), &inserted); // [手順] - 満杯の状態で新しいキーを upsert する。
    int actual_ret_existing =
        cplat_hashtable_upsert(ht, "a", value.data(), &inserted); // [手順] - 満杯でも登録済みのキーを upsert する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_LIMIT_EXCEEDED,
              actual_ret_full);               // [確認_異常系] - 満杯での新規追加が LIMIT_EXCEEDED であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_existing); // [確認_正常系] - 満杯でも登録済みのキーは更新できること。
    EXPECT_EQ(0, inserted);                   // [確認_正常系] - 既存更新として報告されること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// cplat_hashtable_upsert に不正な引数や範囲外キーを渡した場合に適切なエラーコードが返ることの確認
TEST_F(hashtableUpsertTest, guards_reject_invalid_arguments)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    int inserted = -1;

    fill_config(&config, 4, 5);
    fill_value(&value, "v");

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    int actual_ret_null_ht = cplat_hashtable_upsert(NULL, "a", value.data(), &inserted);
    int actual_ret_null_key = cplat_hashtable_upsert(ht, NULL, value.data(), &inserted);
    int actual_ret_null_value = cplat_hashtable_upsert(ht, "a", NULL, &inserted);
    int actual_ret_long_key = cplat_hashtable_upsert(ht, "0123456789", value.data(),
                                                     &inserted); // [手順] - key_size に収まらないキーを渡す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_null_ht);    // [確認_異常系] - ht が NULL なら失敗すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_null_key);   // [確認_異常系] - キーが NULL なら失敗すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_null_value); // [確認_異常系] - 値が NULL なら失敗すること。
    EXPECT_EQ(CPLAT_ERR_OUT_OF_RANGE,
              actual_ret_long_key); // [確認_異常系] - key_size に収まらないキーは OUT_OF_RANGE であること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// cplat_hashtable_upsert で同一バケットの衝突チェイン先頭ではないキーでも正常に値が更新されることの確認
TEST_F(hashtableUpsertTest, updates_key_that_is_not_at_chain_head)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    std::vector<unsigned char> read_back(8, 0);
    const char *peer = nullptr;
    int inserted = -1;
    size_t required = 0;

    fill_config(&config, 4, 5);
    fill_value(&value, "v1");
    peer = find_colliding_key("a", 4); // [状態] - "a" と同じバケットの別キーを探す。

    // Pre-Assert
    ASSERT_NE(nullptr, peer); // [Pre-Assert確認_正常系] - 同一バケットの別キーが見つかること。

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    (void)cplat_hashtable_upsert(ht, "a", value.data(), NULL);
    (void)cplat_hashtable_upsert(ht, peer, value.data(),
                                 NULL); // [手順] - 同一バケットへ別キーを足してチェイン先頭をずらす。
    fill_value(&value, "v3");
    int actual_ret_upsert =
        cplat_hashtable_upsert(ht, "a", value.data(), &inserted); // [手順] - チェイン先頭でないキーを upsert する。
    int actual_ret_read = cplat_hashtable_find_value_copy(ht, "a", read_back.data(), read_back.size(), &required);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_upsert); // [確認_正常系] - チェイン先頭でないキーを更新できること。
    EXPECT_EQ(0, inserted);                 // [確認_正常系] - 既存更新として報告されること。
    EXPECT_EQ(CPLAT_OK, actual_ret_read);
    // [確認_正常系] - `cplat_hashtable_find_value_copy(ht, "a", read_back.data(), read_back.size(), &required)` の戻り値が `CPLAT_OK` であること。
    EXPECT_STREQ("v3", reinterpret_cast<const char *>(read_back.data())); // [確認_正常系] - 値が更新されていること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// 可変長文字列値のハッシュテーブルで cplat_hashtable_upsert 時にストレージ容量を超える値で更新した場合に STORAGE_FULL が返ることの確認
TEST_F(hashtableUpsertTest, reports_storage_full_when_variable_value_does_not_fit)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    int inserted = -1;

    *(&config) = {};
    config.capacity = 4;
    config.key_type = CPLAT_HASHTABLE_FIELD_FIXED_STRING;
    config.value_type = CPLAT_HASHTABLE_FIELD_VARIABLE_STRING;
    config.timestamp_scope = CPLAT_HASHTABLE_TIMESTAMP_SCOPE_RECORD;
    config.key_size = 8;
    config.value_storage_size = 8; // [状態] - 可変長値のストレージを 8 バイトだけにする。
    config.lifetime = 5;

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    int actual_ret_insert = cplat_hashtable_upsert(ht, "a", "short", &inserted); // [手順] - 収まる値で追加する。
    int actual_ret_update = cplat_hashtable_upsert(ht, "a", "far too long for the storage",
                                                   &inserted); // [手順] - 収まらない値で同じキーを更新する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_insert); // [確認_正常系] - 収まる値の追加が成功すること。
    EXPECT_EQ(CPLAT_ERR_STORAGE_FULL,
              actual_ret_update); // [確認_異常系] - 収まらない値での更新が STORAGE_FULL であること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}
