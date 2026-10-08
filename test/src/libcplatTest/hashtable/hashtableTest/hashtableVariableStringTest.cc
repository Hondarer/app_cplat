#include <testfw.h>

#include <cplat/hashtable/hashtable.h>
#include <mock_cplat.h>

#include <cstring>
#include <string>
#include <vector>

class hashtableVariableStringTest : public Test
{
  protected:
    NiceMock<Mock_cplat> mock_cplat;
};

static cplat_hashtable_config variable_config(size_t key_storage_size, size_t value_storage_size)
{
    cplat_hashtable_config config = {};

    config.capacity = 4;
    config.key_type = CPLAT_HASHTABLE_FIELD_VARIABLE_STRING;
    config.value_type = CPLAT_HASHTABLE_FIELD_VARIABLE_STRING;
    config.key_storage_size = key_storage_size;
    config.value_storage_size = value_storage_size;
    config.lifetime = 5;
    return config;
}

// 可変長文字列キーおよび値の永続化、サイズ問い合わせ付き複製、再接続後の読み出しが正常に行われることの確認
TEST_F(hashtableVariableStringTest, persists_variable_key_and_value_and_copies_with_size_query)
{
    // Arrange
    cplat_hashtable_config config = variable_config(64, 64);
    cplat_hashtable *ht = nullptr;
    cplat_hashtable *attached = nullptr;
    const void *mgmt = nullptr;
    const void *data = nullptr;
    const void *found = nullptr;
    size_t mgmt_size = 0;
    size_t data_size = 0;
    size_t required_size = 0;
    char copied[32] = {};

    // Pre-Assert

    // Act
    int actual_ret_create =
        cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [手順] - 可変長設定でテーブルを構築する。
    int actual_ret_add =
        cplat_hashtable_add(ht, "variable-key", "variable-value",
                            CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 可変長キーと値を追加する。
    int actual_ret_find = cplat_hashtable_find_value_ref(ht, "variable-key", &found);
    int actual_ret_query = cplat_hashtable_find_value_copy(ht, "variable-key", NULL, 0,
                                                           &required_size); // [手順] - 必要サイズを問い合わせる。
    int actual_ret_copy = cplat_hashtable_find_value_copy(ht, "variable-key", copied, sizeof(copied),
                                                          &required_size); // [手順] - 値をバッファーへ複製する。
    (void)cplat_hashtable_buffer_size(ht, &mgmt_size, &data_size);
    (void)cplat_hashtable_buffer_ref(ht, &mgmt, &data);
    std::vector<unsigned char> mgmt_copy(static_cast<const unsigned char *>(mgmt),
                                         static_cast<const unsigned char *>(mgmt) + mgmt_size);
    std::vector<unsigned char> data_copy(static_cast<const unsigned char *>(data),
                                         static_cast<const unsigned char *>(data) + data_size);
    int actual_ret_attach = cplat_hashtable_attach(mgmt_copy.data(), mgmt_copy.size(), data_copy.data(),
                                                   data_copy.size(), &attached); // [手順] - 複製領域へ再接続する。
    const void *attached_value = nullptr;
    int actual_ret_attached_find = cplat_hashtable_find_value_ref(attached, "variable-key", &attached_value);
    std::string found_text = static_cast<const char *>(found);
    std::string attached_text = static_cast<const char *>(attached_value);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_create);  // [確認_正常系] - テーブル構築が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_add);     // [確認_正常系] - 可変長キーと値の追加が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_find);    // [確認_正常系] - 値参照の取得が成功すること。
    EXPECT_EQ("variable-value", found_text); // [確認_正常系] - 取得した値が一致すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_query);   // [確認_正常系] - 必要サイズの問い合わせが成功すること。
    EXPECT_EQ(std::strlen("variable-value") + 1u,
              required_size);                      // [確認_正常系] - 必要サイズが終端文字を含んだ長さであること。
    EXPECT_EQ(CPLAT_OK, actual_ret_copy);          // [確認_正常系] - 値の複製が成功すること。
    EXPECT_STREQ("variable-value", copied);        // [確認_正常系] - 複製された内容が一致すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_attach);        // [確認_正常系] - 再接続が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_attached_find); // [確認_正常系] - 再接続後の検索が成功すること。
    EXPECT_EQ("variable-value", attached_text);    // [確認_正常系] - 再接続後も値が一致すること。

    // Cleanup
    cplat_hashtable_dispose(attached);
    cplat_hashtable_dispose(ht);
}

// 可変長値の更新時に断片化により領域不足となった場合に STORAGE_FULL が返り既存値が保持されることの確認
TEST_F(hashtableVariableStringTest, fragmented_update_returns_storage_full_and_preserves_value)
{
    // Arrange
    cplat_hashtable_config config = variable_config(32, 12);
    cplat_hashtable *ht = nullptr;
    const void *found = nullptr;

    // Pre-Assert

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    int actual_ret_add_a = cplat_hashtable_add(ht, "a", "1111", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    int actual_ret_add_b = cplat_hashtable_add(ht, "b", "2222", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    int actual_ret_update = cplat_hashtable_update(ht, "a", "123456"); // [手順] - 収まらないサイズへ更新する。
    int actual_ret_find = cplat_hashtable_find_value_ref(ht, "a", &found);
    int actual_ret_validate = cplat_hashtable_validate(ht);
    std::string found_text = static_cast<const char *>(found);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_add_a);                // [確認_正常系] - a の追加が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_add_b);                // [確認_正常系] - b の追加が成功すること。
    EXPECT_EQ(CPLAT_ERR_STORAGE_FULL, actual_ret_update); // [確認_異常系] - ストレージ不足で STORAGE_FULL が返ること。
    EXPECT_EQ(CPLAT_OK, actual_ret_find);                 // [確認_正常系] - 既存値の検索が成功すること。
    EXPECT_EQ("1111", found_text);                        // [確認_正常系] - 更新失敗後も既存値が保持されていること。
    EXPECT_EQ(CPLAT_OK, actual_ret_validate);             // [確認_正常系] - 内部整合性が保たれていること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// cplat_hashtable_compact による明示的圧縮で断片化が解消され空き領域への追加が可能になることの確認
TEST_F(hashtableVariableStringTest, explicit_compaction_enables_fragmented_add_and_invalidates_moved_reference)
{
    // Arrange
    cplat_hashtable_config config = variable_config(32, 20);
    cplat_hashtable *ht = nullptr;
    const void *before_compact = nullptr;
    const void *after_compact = nullptr;
    const void *key_before_compact = nullptr;
    const void *key_after_compact = nullptr;
    const void *added = nullptr;

    // Pre-Assert

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    (void)cplat_hashtable_add(ht, "a", "1111", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_add(ht, "b", "2222", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_add(ht, "c", "3333", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_find_value_ref(ht, "c", &before_compact);
    uint64_t c_record = 0;
    (void)cplat_hashtable_find_recno(ht, "c", &c_record);
    (void)cplat_hashtable_get_key_ref(ht, c_record, &key_before_compact);
    (void)cplat_hashtable_delete(ht, "b");
    (void)cplat_hashtable_purge_deleted(ht);
    int actual_ret_fragmented = cplat_hashtable_add(
        ht, "d", "55555", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 断片化状態で追加を試みる。
    const void *after_failed_add = nullptr;
    (void)cplat_hashtable_find_value_ref(ht, "c", &after_failed_add);
    int actual_ret_compact = cplat_hashtable_compact(ht); // [手順] - ストレージを圧縮する。
    (void)cplat_hashtable_find_value_ref(ht, "c", &after_compact);
    (void)cplat_hashtable_get_key_ref(ht, c_record, &key_after_compact);
    bool vacated_key_zeroed = static_cast<const unsigned char *>(key_before_compact)[0] == 0 &&
                              static_cast<const unsigned char *>(key_before_compact)[1] == 0;
    bool vacated_value_zeroed = true;
    for (size_t i = 0; i < std::strlen("3333") + 1u; i++)
    {
        vacated_value_zeroed = vacated_value_zeroed && (static_cast<const unsigned char *>(before_compact)[i] == 0);
    }
    int actual_ret_retry =
        cplat_hashtable_add(ht, "d", "55555", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 圧縮後に再度追加する。
    int actual_ret_find = cplat_hashtable_find_value_ref(ht, "d", &added);
    int actual_ret_validate = cplat_hashtable_validate(ht);
    std::string compacted_text = static_cast<const char *>(after_compact);
    std::string added_text = static_cast<const char *>(added);

    // Assert
    EXPECT_EQ(CPLAT_ERR_STORAGE_FULL,
              actual_ret_fragmented);                 // [確認_異常系] - 圧縮前は断片化により STORAGE_FULL であること。
    EXPECT_EQ(before_compact, after_failed_add);      // [確認_正常系] - 失敗した追加では既存参照が変わらないこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_compact);          // [確認_正常系] - compact が成功すること。
    EXPECT_NE(before_compact, after_compact);         // [確認_正常系] - 圧縮により値アドレスが移動すること。
    EXPECT_NE(key_before_compact, key_after_compact); // [確認_正常系] - 圧縮によりキーアドレスが移動すること。
    EXPECT_TRUE(vacated_key_zeroed);                  // [確認_正常系] - 移動元のキー領域がゼロクリアされること。
    EXPECT_TRUE(vacated_value_zeroed);                // [確認_正常系] - 移動元の値領域がゼロクリアされること。
    EXPECT_EQ("3333", compacted_text);                // [確認_正常系] - 移動後も値の内容が保たれていること。
    EXPECT_EQ(CPLAT_OK, actual_ret_retry);            // [確認_正常系] - 圧縮後の追加が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_find);             // [確認_正常系] - 追加したキーの検索が成功すること。
    EXPECT_EQ("55555", added_text);                   // [確認_正常系] - 追加された値が一致すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_validate);         // [確認_正常系] - 内部整合性が保たれていること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// cplat_hashtable_purge_deleted 時に解放された可変長キーおよび値のストレージがゼロクリアされることの確認
TEST_F(hashtableVariableStringTest, purge_zero_fills_released_variable_key_and_value)
{
    // Arrange
    cplat_hashtable_config config = variable_config(32, 32);
    cplat_hashtable *ht = nullptr;
    const void *key_ref = nullptr;
    const void *value_ref = nullptr;
    uint64_t record = 0;

    // Pre-Assert

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    (void)cplat_hashtable_add(ht, "secret-key", "secret-value", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_find_recno(ht, "secret-key", &record);
    (void)cplat_hashtable_get_key_ref(ht, record, &key_ref);
    (void)cplat_hashtable_get_value_ref(ht, record, &value_ref);
    (void)cplat_hashtable_delete(ht, "secret-key");
    bool retained_while_deleted = std::strcmp(static_cast<const char *>(key_ref), "secret-key") == 0 &&
                                  std::strcmp(static_cast<const char *>(value_ref), "secret-value") == 0;
    int actual_ret_purge = cplat_hashtable_purge_deleted(ht); // [手順] - 削除済みをパージする。
    bool key_zeroed = true;
    bool value_zeroed = true;
    for (size_t i = 0; i < std::strlen("secret-key") + 1u; i++)
    {
        key_zeroed = key_zeroed && (static_cast<const unsigned char *>(key_ref)[i] == 0);
    }
    for (size_t i = 0; i < std::strlen("secret-value") + 1u; i++)
    {
        value_zeroed = value_zeroed && (static_cast<const unsigned char *>(value_ref)[i] == 0);
    }

    // Assert
    EXPECT_TRUE(retained_while_deleted);   // [確認_正常系] - パージ前は削除済みでも内容が残っていること。
    EXPECT_EQ(CPLAT_OK, actual_ret_purge); // [確認_正常系] - purge が成功すること。
    EXPECT_TRUE(key_zeroed);               // [確認_正常系] - 解放されたキー領域がゼロクリアされること。
    EXPECT_TRUE(value_zeroed);             // [確認_正常系] - 解放された値領域がゼロクリアされること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// ライフタイム満了による削除レコード回収時に可変長ストレージ領域がゼロクリアされることの確認
TEST_F(hashtableVariableStringTest, lifetime_expiration_zero_fills_released_variable_storage)
{
    // Arrange
    cplat_hashtable_config config = variable_config(32, 32);
    cplat_hashtable *ht = nullptr;
    const void *key_ref = nullptr;
    const void *value_ref = nullptr;
    uint64_t record = 0;

    config.lifetime = 3;

    // Pre-Assert

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    (void)cplat_hashtable_add(ht, "expired", "private", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_find_recno(ht, "expired", &record);
    (void)cplat_hashtable_get_key_ref(ht, record, &key_ref);
    (void)cplat_hashtable_get_value_ref(ht, record, &value_ref);
    (void)cplat_hashtable_delete(ht, "expired");
    int actual_ret_push = cplat_hashtable_push_deleted(ht); // [手順] - 加齢によりライフタイムを満了させる。
    bool key_zeroed = true;
    bool value_zeroed = true;
    for (size_t i = 0; i < std::strlen("expired") + 1u; i++)
    {
        key_zeroed = key_zeroed && (static_cast<const unsigned char *>(key_ref)[i] == 0);
    }
    for (size_t i = 0; i < std::strlen("private") + 1u; i++)
    {
        value_zeroed = value_zeroed && (static_cast<const unsigned char *>(value_ref)[i] == 0);
    }
    int status = -1;
    (void)cplat_hashtable_get_status(ht, record, &status);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_push); // [確認_正常系] - push_deleted が成功すること。
    EXPECT_EQ(0, status);                 // [確認_正常系] - 満了によりステータスが空(0)に戻ること。
    EXPECT_TRUE(key_zeroed);              // [確認_正常系] - キー領域がゼロクリアされること。
    EXPECT_TRUE(value_zeroed);            // [確認_正常系] - 値領域がゼロクリアされること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// cplat_hashtable_compact が NULL 引数でエラーとなり固定長テーブルでは no-op として成功することの確認
TEST_F(hashtableVariableStringTest, compact_accepts_null_and_is_noop_for_fixed_fields)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    const void *before = nullptr;
    const void *after = nullptr;

    config.capacity = 1;
    config.key_type = CPLAT_HASHTABLE_FIELD_FIXED_STRING;
    config.value_type = CPLAT_HASHTABLE_FIELD_FIXED_STRING;
    config.key_size = 8;
    config.value_size = 8;
    config.lifetime = 5;

    // Pre-Assert

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    (void)cplat_hashtable_add(ht, "key", "value", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_find_value_ref(ht, "key", &before);
    int actual_ret_null = cplat_hashtable_compact(NULL);  // [手順] - ht に NULL を渡す。
    int actual_ret_compact = cplat_hashtable_compact(ht); // [手順] - 固定長テーブルで compact を呼び出す。
    (void)cplat_hashtable_find_value_ref(ht, "key", &after);
    std::string after_text = static_cast<const char *>(after);

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_null); // [確認_異常系] - NULL ht が INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_compact);                // [確認_正常系] - 固定長での compact が成功すること。
    EXPECT_EQ(before, after);                               // [確認_正常系] - 固定長では参照アドレスが変化しないこと。
    EXPECT_EQ("value", after_text);                         // [確認_正常系] - 値の内容が変化しないこと。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// 可変長文字列キーと固定長バイナリ値の組み合わせが正常に動作することの確認
TEST_F(hashtableVariableStringTest, supports_variable_key_with_fixed_binary_value)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    const unsigned char value[] = {0, 1, 2, 3};
    unsigned char copied[sizeof(value)] = {};
    size_t required_size = 0;

    config.capacity = 2;
    config.key_type = CPLAT_HASHTABLE_FIELD_VARIABLE_STRING;
    config.value_type = CPLAT_HASHTABLE_FIELD_FIXED_BINARY;
    config.key_storage_size = 32;
    config.value_size = sizeof(value);
    config.lifetime = 5;

    // Pre-Assert

    // Act
    int actual_ret_create = cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    int actual_ret_add = cplat_hashtable_add(ht, "key", value, CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    int actual_ret_copy = cplat_hashtable_find_value_copy(ht, "key", copied, sizeof(copied), &required_size);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_create);                  // [確認_正常系] - 作成が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_add);                     // [確認_正常系] - 追加が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_copy);                    // [確認_正常系] - 複製取得が成功すること。
    EXPECT_EQ(sizeof(value), required_size);                 // [確認_正常系] - 必要サイズが一致すること。
    EXPECT_EQ(0, std::memcmp(value, copied, sizeof(value))); // [確認_正常系] - 複製内容が一致すること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// 固定長文字列フィールドで文字列リテラルを受け入れ未使用ストレージ領域をゼロ埋めすることの確認
TEST_F(hashtableVariableStringTest, fixed_strings_accept_literals_and_zero_fill_unused_storage)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    const void *key_ref = nullptr;
    const void *value_ref = nullptr;
    char key_copy[8] = {};
    char value_copy[8] = {};
    unsigned char stored_key[8] = {};
    unsigned char stored_value[8] = {};
    size_t key_required_size = 0;
    size_t value_required_size = 0;
    uint64_t record = 0;
    const unsigned char expected_key[8] = {'k', 0, 0, 0, 0, 0, 0, 0};
    const unsigned char expected_value[8] = {'n', 'e', 'w', 0, 0, 0, 0, 0};

    config.capacity = 2;
    config.key_type = CPLAT_HASHTABLE_FIELD_FIXED_STRING;
    config.value_type = CPLAT_HASHTABLE_FIELD_FIXED_STRING;
    config.key_size = sizeof(key_copy);
    config.value_size = sizeof(value_copy);
    config.lifetime = 5;

    // Pre-Assert

    // Act
    int actual_ret_create = cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    int actual_ret_add = cplat_hashtable_add(ht, "k", "old", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    int actual_ret_update = cplat_hashtable_update(ht, "k", "new");
    (void)cplat_hashtable_find_recno(ht, "k", &record);
    (void)cplat_hashtable_get_key_ref(ht, record, &key_ref);
    (void)cplat_hashtable_get_value_ref(ht, record, &value_ref);
    int actual_ret_key_copy = cplat_hashtable_get_key_copy(ht, record, key_copy, sizeof(key_copy), &key_required_size);
    int actual_ret_value_copy =
        cplat_hashtable_get_value_copy(ht, record, value_copy, sizeof(value_copy), &value_required_size);
    std::memcpy(stored_key, key_ref, sizeof(stored_key));
    std::memcpy(stored_value, value_ref, sizeof(stored_value));

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_create); // [確認_正常系] - 作成が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_add);    // [確認_正常系] - 追加が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_update); // [確認_正常系] - 更新が成功すること。
    EXPECT_EQ(0, std::memcmp(expected_key, stored_key,
                             sizeof(expected_key))); // [確認_正常系] - キー未使用部がゼロ埋めされること。
    EXPECT_EQ(0, std::memcmp(expected_value, stored_value,
                             sizeof(expected_value))); // [確認_正常系] - 値未使用部がゼロ埋めされること。
    EXPECT_EQ(CPLAT_OK, actual_ret_key_copy);          // [確認_正常系] - キー複製が成功すること。
    EXPECT_EQ(2u, key_required_size);                  // [確認_正常系] - キー必要サイズが一致すること。
    EXPECT_STREQ("k", key_copy);                       // [確認_正常系] - キー複製内容が一致すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_value_copy);        // [確認_正常系] - 値複製が成功すること。
    EXPECT_EQ(4u, value_required_size);                // [確認_正常系] - 値必要サイズが一致すること。
    EXPECT_STREQ("new", value_copy);                   // [確認_正常系] - 値複製内容が一致すること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// キーおよび値の全フィールド型組み合わせ（固定長バイナリ・固定長文字列・可変長文字列）が正常に動作することの確認
TEST_F(hashtableVariableStringTest, supports_all_key_and_value_field_type_combinations)
{
    const cplat_hashtable_field_type field_types[] = {
        CPLAT_HASHTABLE_FIELD_FIXED_BINARY, CPLAT_HASHTABLE_FIELD_FIXED_STRING, CPLAT_HASHTABLE_FIELD_VARIABLE_STRING};
    const unsigned char binary_key[] = {'k', 'e', 'y', 0};
    const unsigned char binary_value[] = {'v', 'a', 'l', 0};

    for (cplat_hashtable_field_type key_type : field_types)
    {
        for (cplat_hashtable_field_type value_type : field_types)
        {
            // Arrange
            cplat_hashtable_config config = {};
            cplat_hashtable *ht = nullptr;
            unsigned char copied[16] = {};
            size_t required_size = 0;
            const void *key = key_type == CPLAT_HASHTABLE_FIELD_FIXED_BINARY ? static_cast<const void *>(binary_key)
                                                                             : static_cast<const void *>("key");
            const void *value = value_type == CPLAT_HASHTABLE_FIELD_FIXED_BINARY
                                    ? static_cast<const void *>(binary_value)
                                    : static_cast<const void *>("val");

            config.capacity = 2;
            config.key_type = key_type;
            config.value_type = value_type;
            config.key_size = key_type == CPLAT_HASHTABLE_FIELD_VARIABLE_STRING ? 0 : sizeof(binary_key);
            config.value_size = value_type == CPLAT_HASHTABLE_FIELD_VARIABLE_STRING ? 0 : sizeof(binary_value);
            config.key_storage_size = key_type == CPLAT_HASHTABLE_FIELD_VARIABLE_STRING ? 16 : 0;
            config.value_storage_size = value_type == CPLAT_HASHTABLE_FIELD_VARIABLE_STRING ? 16 : 0;
            config.lifetime = 5;

            // Pre-Assert

            // Act
            int actual_ret_create = cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
            int actual_ret_add = cplat_hashtable_add(ht, key, value, CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
            int actual_ret_query = cplat_hashtable_find_value_copy(ht, key, NULL, 0, &required_size);
            int actual_ret_too_small =
                cplat_hashtable_find_value_copy(ht, key, copied, required_size - 1, &required_size);
            bool too_small_unchanged = true;
            for (unsigned char byte : copied)
            {
                too_small_unchanged = too_small_unchanged && (byte == 0);
            }
            int actual_ret_copy = cplat_hashtable_find_value_copy(ht, key, copied, sizeof(copied), &required_size);

            // Assert
            EXPECT_EQ(CPLAT_OK, actual_ret_create);         // [確認_正常系 回数=3*3] - 作成が成功すること。
            EXPECT_EQ(CPLAT_OK, actual_ret_add);            // [確認_正常系 回数=3*3] - 追加が成功すること。
            EXPECT_EQ(CPLAT_OK, actual_ret_query);          // [確認_正常系 回数=3*3] - クエリが成功すること。
            EXPECT_EQ(sizeof(binary_value), required_size); // [確認_正常系 回数=3*3] - 必要サイズが一致すること。
            EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
                      actual_ret_too_small);      // [確認_異常系 回数=3*3] - 短いバッファーで BUFFER_TOO_SMALL であること。
            EXPECT_TRUE(too_small_unchanged);     // [確認_正常系 回数=3*3] - 短いバッファーでは内容が変更されないこと。
            EXPECT_EQ(CPLAT_OK, actual_ret_copy); // [確認_正常系 回数=3*3] - 複製が成功すること。
            EXPECT_EQ(
                0, std::memcmp(binary_value, copied, sizeof(binary_value))); // [確認_正常系 回数=3*3] - 複製内容が一致すること。

            // Cleanup
            cplat_hashtable_dispose(ht);
        }
    }
}

/*
 *  次は可変長ストレージの配置に関する特性化テストです。
 *  先着適合の探索順と圧縮の詰め直し順を、オフセットの実測値で固定します。
 *  ストレージ アロケーターの内部実装を差し替えても、配置が変わらないことを保証します。
 */

static cplat_hashtable_config variable_config_with_capacity(size_t capacity, size_t key_storage_size,
                                                            size_t value_storage_size)
{
    cplat_hashtable_config config = {};

    config.capacity = capacity;
    config.key_type = CPLAT_HASHTABLE_FIELD_VARIABLE_STRING;
    config.value_type = CPLAT_HASHTABLE_FIELD_VARIABLE_STRING;
    config.key_storage_size = key_storage_size;
    config.value_storage_size = value_storage_size;
    config.lifetime = 5;
    return config;
}

/**
 *  可変長ストレージの先頭は、キーが管理領域の末尾、値がデータ領域の末尾に置かれます。
 *  この位置関係は公開 API の buffer_ref と buffer_size から算出できます。
 */
class storage_origin
{
  public:
    storage_origin(const cplat_hashtable *ht, const cplat_hashtable_config &config)
    {
        const void *mgmt = nullptr;
        const void *data = nullptr;
        size_t mgmt_size = 0;
        size_t data_size = 0;

        (void)cplat_hashtable_buffer_ref(ht, &mgmt, &data);
        (void)cplat_hashtable_buffer_size(ht, &mgmt_size, &data_size);
        key_base_ = static_cast<const unsigned char *>(mgmt) + mgmt_size - config.key_storage_size;
        value_base_ = static_cast<const unsigned char *>(data) + data_size - config.value_storage_size;
    }

    /** キーが見つからない場合は -1 を返します。 */
    long key_offset(const cplat_hashtable *ht, const char *key) const
    {
        const void *ref = nullptr;
        uint64_t record = 0;

        if (cplat_hashtable_find_recno(ht, key, &record) != CPLAT_OK)
        {
            return -1;
        }
        if (cplat_hashtable_get_key_ref(ht, record, &ref) != CPLAT_OK)
        {
            return -1;
        }
        return static_cast<long>(static_cast<const unsigned char *>(ref) - key_base_);
    }

    /** キーが見つからない場合は -1 を返します。 */
    long value_offset(const cplat_hashtable *ht, const char *key) const
    {
        const void *ref = nullptr;

        if (cplat_hashtable_find_value_ref(ht, key, &ref) != CPLAT_OK)
        {
            return -1;
        }
        return static_cast<long>(static_cast<const unsigned char *>(ref) - value_base_);
    }

  private:
    const unsigned char *key_base_;
    const unsigned char *value_base_;
};

// 可変長ストレージの先着適合配置が追加・パージ・更新を通じて決定論的に動作することの確認
TEST_F(hashtableVariableStringTest, first_fit_placement_is_stable_across_add_purge_and_update)
{
    // Arrange
    cplat_hashtable_config config =
        variable_config_with_capacity(8, 64, 32); // [状態] - キー 64 バイト、値 32 バイトの可変長ストレージを用意する。
    cplat_hashtable *ht = nullptr;
    std::vector<long> actual_value_offsets;
    std::vector<long> actual_key_offsets;

    // Pre-Assert

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [手順] - テーブルを構築する。
    storage_origin origin(ht, config);
    (void)cplat_hashtable_add(ht, "k1", "aaaa", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_add(ht, "k2", "bbbb", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_add(ht, "k3", "cccc", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_add(ht, "k4", "dddd",
                              CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 5 バイト値を 4 件詰める。
    (void)cplat_hashtable_delete(ht, "k2");
    (void)cplat_hashtable_purge_deleted(ht); // [手順] - 2 件目を回収し、途中に空きブロックを作る。
    int actual_ret_reuse_free_block = cplat_hashtable_add(ht, "k5", "ee", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    long actual_offset_reuse_free_block =
        origin.value_offset(ht, "k5"); // [手順] - 空きブロックに収まる 3 バイト値を追加する。
    int actual_ret_skip_free_block = cplat_hashtable_add(ht, "k6", "ffffff", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    long actual_offset_skip_free_block =
        origin.value_offset(ht, "k6"); // [手順] - 空きブロックに収まらない 7 バイト値を追加する。
    (void)cplat_hashtable_add(ht, "k7", "gg", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    int actual_ret_shrink = cplat_hashtable_update(ht, "k1", "hh");
    long actual_offset_shrink = origin.value_offset(ht, "k1"); // [手順] - 先頭の値を短い値へ更新する。
    int actual_ret_merge_own = cplat_hashtable_update(ht, "k3", "iiiiii");
    long actual_offset_merge_own =
        origin.value_offset(ht, "k3"); // [手順] - 自ブロックと隣接する空きブロックの結合が要る更新を行う。
    for (const char *key : {"k1", "k3", "k4", "k5", "k6", "k7"})
    {
        actual_value_offsets.push_back(origin.value_offset(ht, key));
        actual_key_offsets.push_back(origin.key_offset(ht, key));
    }
    int actual_ret_validate = cplat_hashtable_validate(ht);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_reuse_free_block); // [確認_正常系] - 空きブロックに収まる add が成功すること。
    EXPECT_EQ(
        5,
        actual_offset_reuse_free_block); // [確認_正常系] - 空きブロックに収まる add が、回収済みの空きブロックの先頭へ配置されること。
    EXPECT_EQ(CPLAT_OK, actual_ret_skip_free_block); // [確認_正常系] - 空きブロックに収まらない add が成功すること。
    EXPECT_EQ(
        20,
        actual_offset_skip_free_block); // [確認_正常系] - 空きブロックに収まらない add が、小さすぎる空きブロックを読み飛ばして末尾側へ配置されること。
    EXPECT_EQ(CPLAT_OK, actual_ret_shrink); // [確認_正常系] - 短い値への update が成功すること。
    EXPECT_EQ(0, actual_offset_shrink);     // [確認_正常系] - 短い値への update が、自ブロックの先頭を維持すること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_merge_own); // [確認_正常系] - 自ブロックと隣接する空きブロックの結合が要る update が成功すること。
    EXPECT_EQ(
        8,
        actual_offset_merge_own); // [確認_正常系] - 当該 update が、直前の空きブロックと自ブロックを結合した位置へ配置されること。
    EXPECT_EQ(std::vector<long>({0, 8, 15, 5, 20, 27}),
              actual_value_offsets); // [確認_正常系] - 一連の操作後の値オフセットが記録どおりであること。
    EXPECT_EQ(std::vector<long>({0, 6, 9, 3, 12, 15}),
              actual_key_offsets);            // [確認_正常系] - 一連の操作後のキー オフセットが記録どおりであること。
    EXPECT_EQ(CPLAT_OK, actual_ret_validate); // [確認_正常系] - 一連の操作後の validate が成功すること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// 可変長ストレージ圧縮がブロックをオフセット順に隙間なく詰め直し末尾を解放することの確認
TEST_F(hashtableVariableStringTest, compaction_packs_blocks_in_offset_order_and_frees_the_tail)
{
    // Arrange
    cplat_hashtable_config config =
        variable_config_with_capacity(8, 64, 32); // [状態] - キー 64 バイト、値 32 バイトの可変長ストレージを用意する。
    cplat_hashtable *ht = nullptr;
    std::vector<long> actual_value_offsets;

    // Pre-Assert

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [手順] - テーブルを構築する。
    storage_origin origin(ht, config);
    (void)cplat_hashtable_add(ht, "k1", "aaaa", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_add(ht, "k2", "bbbb", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_add(ht, "k3", "cccc", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_add(ht, "k4", "dddd", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_delete(ht, "k2");
    (void)cplat_hashtable_purge_deleted(ht);
    (void)cplat_hashtable_add(ht, "k5", "ee", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_add(ht, "k6", "ffffff", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_add(ht, "k7", "gg", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_update(ht, "k1", "hh");
    (void)cplat_hashtable_update(ht, "k3", "iiiiii");     // [手順] - 空きブロックが残る状態を作る。
    int actual_ret_compact = cplat_hashtable_compact(ht); // [手順] - 明示的に圧縮する。
    for (const char *key : {"k1", "k5", "k3", "k4", "k6", "k7"})
    {
        actual_value_offsets.push_back(origin.value_offset(ht, key));
    }
    int actual_ret_add_after_compact =
        cplat_hashtable_add(ht, "k8", "jjj",
                            CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 圧縮で空いた末尾へ追加する。
    long actual_offset_after_compact = origin.value_offset(ht, "k8");
    int actual_ret_validate = cplat_hashtable_validate(ht);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_compact); // [確認_正常系] - compact が成功すること。
    EXPECT_EQ(std::vector<long>({0, 3, 6, 13, 18, 25}),
              actual_value_offsets);                   // [確認_正常系] - compact がオフセット順に隙間なく詰め直すこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_add_after_compact); // [確認_正常系] - 圧縮後の add が成功すること。
    EXPECT_EQ(28, actual_offset_after_compact); // [確認_正常系] - 圧縮後の add が、詰め直した末尾へ配置されること。
    EXPECT_EQ(CPLAT_OK, actual_ret_validate);   // [確認_正常系] - 圧縮と追加の後の validate が成功すること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// ちょうど収まるサイズの追加で最後の空きブロックが消費され、次の追加で STORAGE_FULL となることの確認
TEST_F(hashtableVariableStringTest, exact_fit_consumes_the_last_free_block_and_the_next_add_reports_storage_full)
{
    // Arrange
    cplat_hashtable_config config =
        variable_config_with_capacity(8, 64, 16); // [状態] - 値ストレージを 16 バイトに絞る。
    cplat_hashtable *ht = nullptr;

    // Pre-Assert

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [手順] - テーブルを構築する。
    storage_origin origin(ht, config);
    (void)cplat_hashtable_add(ht, "k1", "aaaaaaa", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    int actual_ret_exact_fit = cplat_hashtable_add(ht, "k2", "bbbbbbb", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    long actual_offset_exact_fit = origin.value_offset(ht, "k2"); // [手順] - 残り 8 バイトへ 8 バイト値を追加する。
    int actual_ret_full = cplat_hashtable_add(ht, "k3", "c",
                                              CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - さらに追加する。
    int actual_ret_validate = cplat_hashtable_validate(ht);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_exact_fit); // [確認_正常系] - 残り容量ちょうどの add が成功すること。
    EXPECT_EQ(
        8,
        actual_offset_exact_fit); // [確認_正常系] - 残り容量ちょうどの add が、末尾の空きブロックの先頭へ配置されること。
    EXPECT_EQ(CPLAT_ERR_STORAGE_FULL,
              actual_ret_full);               // [確認_異常系] - 空きが無くなった後の add が STORAGE_FULL であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_validate); // [確認_正常系] - 満杯状態の validate が成功すること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// cplat_hashtable_resize 時に可変長ストレージが再パッキングされ参照が正しく移動することの確認
TEST_F(hashtableVariableStringTest, resize_repacks_variable_storage_and_moves_references)
{
    // Arrange
    cplat_hashtable_config config =
        variable_config_with_capacity(4, 64, 32); // [状態] - キー 64 バイト、値 32 バイトの可変長ストレージを用意する。
    cplat_hashtable_config grown =
        variable_config_with_capacity(8, 64, 32); // [状態] - capacity だけを 8 へ広げた設定を用意する。
    cplat_hashtable *ht = nullptr;
    const void *before_resize = nullptr;
    const void *after_resize = nullptr;
    std::vector<long> actual_offsets_before;
    std::vector<long> actual_offsets_after;

    // Pre-Assert

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [手順] - テーブルを構築する。
    (void)cplat_hashtable_add(ht, "k1", "aaaa", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_add(ht, "k2", "bbbb", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_add(ht, "k3", "cccc", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_delete(ht, "k2");
    (void)cplat_hashtable_purge_deleted(ht); // [手順] - 中間の 1 件を回収し、ストレージの途中に空きブロックを作る。
    {
        storage_origin origin(ht, config);

        for (const char *key : {"k1", "k3"})
        {
            actual_offsets_before.push_back(origin.value_offset(ht, key));
        }
        (void)cplat_hashtable_find_value_ref(ht, "k3", &before_resize);
    }
    int actual_ret_resize = cplat_hashtable_resize(ht, &grown); // [手順] - capacity を広げる。
    {
        storage_origin origin(ht, grown);

        for (const char *key : {"k1", "k3"})
        {
            actual_offsets_after.push_back(origin.value_offset(ht, key));
        }
        (void)cplat_hashtable_find_value_ref(ht, "k3", &after_resize);
    }
    std::string moved_text = static_cast<const char *>(after_resize);
    int actual_ret_validate = cplat_hashtable_validate(ht);

    // Assert
    EXPECT_EQ(std::vector<long>({0, 10}),
              actual_offsets_before);       // [確認_正常系] - resize 前は回収済みの空きブロックがそのまま残ること。
    EXPECT_EQ(CPLAT_OK, actual_ret_resize); // [確認_正常系] - resize が成功すること。
    EXPECT_EQ(std::vector<long>({0, 5}),
              actual_offsets_after); // [確認_正常系] - resize が残すレコードをレコード番号順に隙間なく詰め直すこと。
    EXPECT_NE(before_resize, after_resize);   // [確認_正常系] - resize が取得済みの可変長参照を移動させること。
    EXPECT_EQ("cccc", moved_text);            // [確認_正常系] - 詰め直した後も値の内容が変わらないこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_validate); // [確認_正常系] - resize 後の validate が成功すること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}
