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
    config->value_type = CPLAT_HASHTABLE_FIELD_BINARY;
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
    (void)cplat_hashtable_create(&config, nullptr, 0, nullptr, 0, &ht);
    int actual_ret_first = cplat_hashtable_upsert(
        ht, "a", value.data(),
        &inserted_first); // [手順] - 未登録のキーを指定して cplat_hashtable_upsert() を呼び出す。
    fill_value(&value, "v2");
    int actual_ret_second =
        cplat_hashtable_upsert(ht, "a", value.data(),
                               &inserted_second); // [手順] - 同じキーを指定して cplat_hashtable_upsert() を呼び出す。
    int actual_ret_read = cplat_hashtable_find_value_copy(ht, "a", read_back.data(), read_back.size(), &required);
    int actual_ret_count = cplat_hashtable_count(ht, &in_use);

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_first); // [確認_正常系] - 未登録のキーに対する cplat_hashtable_upsert() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        1,
        inserted_first); // [確認_正常系] - inserted_first が 1 であること。(未登録のキーは新規追加として報告されたことの確認)
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_second); // [確認_正常系] - 登録済みのキーに対する cplat_hashtable_upsert() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        0,
        inserted_second); // [確認_正常系] - inserted_second が 0 であること。(登録済みのキーは既存更新として報告されたことの確認)
    EXPECT_EQ(CPLAT_OK, actual_ret_read);
    // [確認_正常系] - cplat_hashtable_find_value_copy() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "v2",
        reinterpret_cast<const char *>(
            read_back
                .data())); // [確認_正常系] - read_back.data() の戻り値が "v2" であること。(値が更新されたことの確認)
    EXPECT_EQ(CPLAT_OK, actual_ret_count);
    // [確認_正常系] - cplat_hashtable_count() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1u, in_use); // [確認_正常系] - in_use が 1 であること。(更新では使用中件数が増えないことの確認)

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// cplat_hashtable_upsert の inserted 出力ポインターに NULL を渡しても新規追加および更新が成功することの確認
TEST_F(hashtableUpsertTest, accepts_null_inserted_out)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);

    fill_config(&config, 4, 5);
    fill_value(&value, "v1");

    // Act
    (void)cplat_hashtable_create(&config, nullptr, 0, nullptr, 0, &ht);
    int actual_ret_insert = cplat_hashtable_upsert(
        ht, "a", value.data(), nullptr); // [手順] - 格納先を省略して cplat_hashtable_upsert() を呼び出す。
    int actual_ret_update = cplat_hashtable_upsert(
        ht, "a", value.data(), nullptr); // [手順] - 格納先を省略して cplat_hashtable_upsert() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_insert); // [確認_正常系] - 格納先に nullptr を渡した新規追加の cplat_hashtable_upsert() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_update); // [確認_正常系] - 格納先に nullptr を渡した既存更新の cplat_hashtable_upsert() の戻り値が CPLAT_OK であること。

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
    (void)cplat_hashtable_create(&config, nullptr, 0, nullptr, 0, &ht);
    (void)cplat_hashtable_add(ht, "a", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_delete(ht, "a"); // [手順] - キーを指定して cplat_hashtable_delete() を呼び出す。
    fill_value(&value, "v2");
    int actual_ret_upsert = cplat_hashtable_upsert(
        ht, "a", value.data(), &inserted); // [手順] - 削除済みのキーを指定して cplat_hashtable_upsert() を呼び出す。
    int actual_ret_status = cplat_hashtable_get_status(ht, 1, &status);
    int actual_ret_read = cplat_hashtable_find_value_copy(ht, "a", read_back.data(), read_back.size(), &required);

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_upsert); // [確認_正常系] - 削除済みのキーに対する cplat_hashtable_upsert() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        1,
        inserted); // [確認_正常系] - inserted が 1 であること。(削除済みからの復活は新規追加として報告されたことの確認)
    EXPECT_EQ(CPLAT_OK, actual_ret_status);
    // [確認_正常系] - cplat_hashtable_get_status() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1, status); // [確認_正常系] - status が 1 であること。(レコードが使用中に戻ったことの確認)
    EXPECT_EQ(CPLAT_OK, actual_ret_read);
    // [確認_正常系] - cplat_hashtable_find_value_copy() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "v2",
        reinterpret_cast<const char *>(
            read_back
                .data())); // [確認_正常系] - read_back.data() の戻り値が "v2" であること。(削除前の値ではなく渡した値で復活したことの確認)

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// ハッシュ テーブル満杯時に cplat_hashtable_upsert で新規キーを追加すると LIMIT_EXCEEDED となり既存キーの更新は成功することの確認
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
    (void)cplat_hashtable_create(&config, nullptr, 0, nullptr, 0, &ht);
    (void)cplat_hashtable_upsert(ht, "a", value.data(), nullptr);
    (void)cplat_hashtable_upsert(ht, "b", value.data(), nullptr);
    int actual_ret_full = cplat_hashtable_upsert(
        ht, "c", value.data(),
        &inserted); // [手順] - 満杯の状態で新しいキーを指定して cplat_hashtable_upsert() を呼び出す。
    int actual_ret_existing = cplat_hashtable_upsert(
        ht, "a", value.data(),
        &inserted); // [手順] - 満杯の状態で登録済みのキーを指定して cplat_hashtable_upsert() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_LIMIT_EXCEEDED,
        actual_ret_full); // [確認_異常系] - 満杯での新規追加に対する cplat_hashtable_upsert() の戻り値が CPLAT_ERR_LIMIT_EXCEEDED であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_existing); // [確認_正常系] - 満杯での登録済みのキーに対する cplat_hashtable_upsert() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, inserted); // [確認_正常系] - inserted が 0 であること。(既存更新として報告されたことの確認)

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// cplat_hashtable_upsert に不正な引数や範囲外キーを渡した場合に適切なエラー コードが返ることの確認
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
    (void)cplat_hashtable_create(&config, nullptr, 0, nullptr, 0, &ht);
    int actual_ret_null_ht = cplat_hashtable_upsert(nullptr, "a", value.data(), &inserted);
    int actual_ret_null_key = cplat_hashtable_upsert(ht, nullptr, value.data(), &inserted);
    int actual_ret_null_value = cplat_hashtable_upsert(ht, "a", nullptr, &inserted);
    int actual_ret_long_key = cplat_hashtable_upsert(
        ht, "0123456789", value.data(),
        &inserted); // [手順] - key_size に収まらないキーを指定して cplat_hashtable_upsert() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_ht); // [確認_異常系] - ht に nullptr を渡した cplat_hashtable_upsert() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_key); // [確認_異常系] - キーに nullptr を渡した cplat_hashtable_upsert() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_value); // [確認_異常系] - 値に nullptr を渡した cplat_hashtable_upsert() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_OUT_OF_RANGE,
        actual_ret_long_key); // [確認_異常系] - key_size に収まらないキーを渡した cplat_hashtable_upsert() の戻り値が CPLAT_ERR_OUT_OF_RANGE であること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// cplat_hashtable_upsert で同一バケットの衝突チェーン先頭ではないキーでも正常に値が更新されることの確認
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

    ASSERT_NE(nullptr, peer); // [状態確認] - find_colliding_key() の戻り値が nullptr 以外であること。

    // Pre-Assert

    // Act
    (void)cplat_hashtable_create(&config, nullptr, 0, nullptr, 0, &ht);
    (void)cplat_hashtable_upsert(ht, "a", value.data(), nullptr);
    (void)cplat_hashtable_upsert(
        ht, peer, value.data(),
        nullptr); // [手順] - 同一バケットの別キーを指定して cplat_hashtable_upsert() を呼び出す。
    fill_value(&value, "v3");
    int actual_ret_upsert = cplat_hashtable_upsert(
        ht, "a", value.data(),
        &inserted); // [手順] - チェーン先頭ではないキーを指定して cplat_hashtable_upsert() を呼び出す。
    int actual_ret_read = cplat_hashtable_find_value_copy(ht, "a", read_back.data(), read_back.size(), &required);

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_upsert); // [確認_正常系] - チェーン先頭でないキーに対する cplat_hashtable_upsert() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, inserted); // [確認_正常系] - inserted が 0 であること。(既存更新として報告されたことの確認)
    EXPECT_EQ(CPLAT_OK, actual_ret_read);
    // [確認_正常系] - cplat_hashtable_find_value_copy() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "v3",
        reinterpret_cast<const char *>(
            read_back
                .data())); // [確認_正常系] - read_back.data() の戻り値が "v3" であること。(値が更新されたことの確認)

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// 可変長文字列値のハッシュ テーブルで cplat_hashtable_upsert 時にストレージ容量を超える値で更新した場合に STORAGE_FULL が返ることの確認
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
    (void)cplat_hashtable_create(&config, nullptr, 0, nullptr, 0, &ht);
    int actual_ret_insert = cplat_hashtable_upsert(
        ht, "a", "short", &inserted); // [手順] - 収まる値を指定して cplat_hashtable_upsert() を呼び出す。
    int actual_ret_update = cplat_hashtable_upsert(
        ht, "a", "far too long for the storage",
        &inserted); // [手順] - 収まらない値で同じキーを指定して cplat_hashtable_upsert() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_insert); // [確認_正常系] - 収まる値で追加した cplat_hashtable_upsert() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_STORAGE_FULL,
        actual_ret_update); // [確認_異常系] - 収まらない値で更新した cplat_hashtable_upsert() の戻り値が CPLAT_ERR_STORAGE_FULL であること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}
