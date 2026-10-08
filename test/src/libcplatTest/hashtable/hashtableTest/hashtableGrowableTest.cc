#include <testfw.h>

#include <cplat/base/result.h>
#include <cplat/hashtable/hashtable.h>
#include <mock_cplat.h>

namespace
{

cplat_hashtable_config fixed_config(size_t capacity)
{
    cplat_hashtable_config config = {};

    config.capacity = capacity;
    config.key_type = CPLAT_HASHTABLE_FIELD_FIXED_STRING;
    config.value_type = CPLAT_HASHTABLE_FIELD_FIXED_STRING;
    config.key_size = 8;
    config.value_size = 16;
    config.lifetime = 5;
    return config;
}

cplat_hashtable_config variable_value_config(size_t capacity, size_t value_storage_size)
{
    cplat_hashtable_config config = fixed_config(capacity);

    config.value_type = CPLAT_HASHTABLE_FIELD_VARIABLE_STRING;
    config.value_size = 0;
    config.value_storage_size = value_storage_size;
    return config;
}

} // namespace

class hashtableGrowableTest : public Test
{
  protected:
    NiceMock<Mock_cplat> mock_cplat;
};

// レコード領域満杯時に capacity が倍増し既存レコード番号を維持して追加完了することの確認
TEST_F(hashtableGrowableTest, add_grows_capacity_and_preserves_existing_record)
{
    // Arrange
    cplat_hashtable_config config = fixed_config(2);
    cplat_hashtable_growth_config growth = {};
    cplat_hashtable_config current = {};
    cplat_hashtable *ht = nullptr;
    uint64_t record_before = 0;
    uint64_t record_after = 0;
    size_t count = 0;

    // Pre-Assert

    // Act
    int actual_ret_create =
        cplat_hashtable_create_growable(&config, &growth, &ht); // [手順] - 上限なしの自動拡張テーブルを構築する。
    (void)cplat_hashtable_add(ht, "a", "one", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_add(ht, "b", "two", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_find_recno(ht, "a", &record_before);
    int actual_ret_add = cplat_hashtable_add(
        ht, "c", "three", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 満杯のテーブルへ 3 件目を追加する。
    int actual_ret_find = cplat_hashtable_find_recno(ht, "a", &record_after);
    int actual_ret_config = cplat_hashtable_get_config_val(ht, &current);
    int actual_ret_count = cplat_hashtable_count(ht, &count);
    int actual_ret_validate = cplat_hashtable_validate(ht);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_create);   // [確認_正常系] - 自動拡張テーブルの構築が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_add);      // [確認_正常系] - 満杯時の add が自動拡張して成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_find);     // [確認_正常系] - 自動拡張後も既存キーを検索できること。
    EXPECT_EQ(record_before, record_after);   // [確認_正常系] - 自動拡張で既存レコード番号が変わらないこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_config);   // [確認_正常系] - 自動拡張後の設定を取得できること。
    EXPECT_EQ(4u, current.capacity);          // [確認_正常系] - capacity が 2 倍になること。
    EXPECT_EQ(CPLAT_OK, actual_ret_count);    // [確認_正常系] - 自動拡張後の件数を取得できること。
    EXPECT_EQ(3u, count);                     // [確認_正常系] - 3 件すべてが格納されていること。
    EXPECT_EQ(CPLAT_OK, actual_ret_validate); // [確認_正常系] - 自動拡張後の内部状態が整合すること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュテーブルを破棄する。
}

// 断片化による連続領域不足時にストレージを増やさず同容量で再構築することの確認
TEST_F(hashtableGrowableTest, add_rebuilds_same_size_for_fragmentation)
{
    // Arrange
    cplat_hashtable_config config = variable_value_config(4, 20);
    cplat_hashtable_growth_config growth = {};
    cplat_hashtable_config current = {};
    cplat_hashtable *ht = nullptr;
    const void *value = nullptr;

    // Pre-Assert

    // Act
    (void)cplat_hashtable_create_growable(&config, &growth, &ht);
    (void)cplat_hashtable_add(ht, "a", "1111", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_add(ht, "b", "2222", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_add(ht, "c", "3333", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_delete(ht, "b");
    (void)cplat_hashtable_purge_deleted(ht); // [状態] - 中央と末尾に 5 バイトずつの空きを作る。
    int actual_ret_add = cplat_hashtable_add(
        ht, "d", "555555", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 7 バイトの値を追加する。
    int actual_ret_find = cplat_hashtable_find_value_ref(ht, "d", &value);
    int actual_ret_config = cplat_hashtable_get_config_val(ht, &current);
    int actual_ret_validate = cplat_hashtable_validate(ht);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_add);  // [確認_正常系] - 断片化時の add が再構築して成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_find); // [確認_正常系] - 再構築後に追加した値を検索できること。
    EXPECT_STREQ("555555", static_cast<const char *>(value)); // [確認_正常系] - 追加した値が保存されていること。
    EXPECT_EQ(CPLAT_OK, actual_ret_config);                   // [確認_正常系] - 再構築後の設定を取得できること。
    EXPECT_EQ(20u, current.value_storage_size); // [確認_正常系] - 断片化だけの場合はストレージ容量が増えないこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_validate);   // [確認_正常系] - 同容量再構築後の内部状態が整合すること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュテーブルを破棄する。
}

// 更新値が現在容量に収まらない場合に値ストレージのみを拡張して更新成功することの確認
TEST_F(hashtableGrowableTest, update_rec_grows_value_storage)
{
    // Arrange
    cplat_hashtable_config config = variable_value_config(2, 10);
    cplat_hashtable_growth_config growth = {};
    cplat_hashtable_config current = {};
    cplat_hashtable *ht = nullptr;
    uint64_t record = 0;
    const void *value = nullptr;

    // Pre-Assert

    // Act
    (void)cplat_hashtable_create_growable(&config, &growth, &ht);
    (void)cplat_hashtable_add(ht, "a", "1111", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_add(ht, "b", "2222", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_find_recno(ht, "a", &record);
    int actual_ret_update = cplat_hashtable_update_rec(ht, record, "12345678901"); // [手順] - 12 バイトの値へ更新する。
    int actual_ret_find = cplat_hashtable_find_value_ref(ht, "a", &value);
    int actual_ret_config = cplat_hashtable_get_config_val(ht, &current);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_update); // [確認_正常系] - update_rec が値ストレージを拡張して成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_find);   // [確認_正常系] - 拡張後に更新した値を検索できること。
    EXPECT_STREQ("12345678901", static_cast<const char *>(value)); // [確認_正常系] - 更新した値が保存されていること。
    EXPECT_EQ(CPLAT_OK, actual_ret_config);                        // [確認_正常系] - 更新後の設定を取得できること。
    EXPECT_EQ(22u,
              current.value_storage_size); // [確認_正常系] - 旧値と新値が共存できる必要量まで値ストレージが増えること。
    EXPECT_EQ(2u, current.capacity);       // [確認_正常系] - 更新では capacity が変わらないこと。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュテーブルを破棄する。
}

// upsert の追加およびキー指定更新において必要な領域が自動拡張されることの確認
TEST_F(hashtableGrowableTest, upsert_and_update_grow_required_regions)
{
    // Arrange
    cplat_hashtable_config config = variable_value_config(1, 5);
    cplat_hashtable_growth_config growth = {};
    cplat_hashtable_config current = {};
    cplat_hashtable *ht = nullptr;
    int inserted = 0;
    const void *value = nullptr;

    // Pre-Assert

    // Act
    (void)cplat_hashtable_create_growable(&config, &growth, &ht);
    (void)cplat_hashtable_add(ht, "a", "1111", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    int actual_ret_upsert =
        cplat_hashtable_upsert(ht, "b", "22", &inserted); // [手順] - 満杯のテーブルへ upsert で追加する。
    int actual_ret_update =
        cplat_hashtable_update(ht, "b", "12345678901"); // [手順] - キー指定で 12 バイトの値へ更新する。
    int actual_ret_find = cplat_hashtable_find_value_ref(ht, "b", &value);
    int actual_ret_config = cplat_hashtable_get_config_val(ht, &current);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_upsert); // [確認_正常系] - upsert がレコードと値ストレージを拡張して成功すること。
    EXPECT_EQ(1, inserted);                 // [確認_正常系] - upsert が新規追加として完了すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_update); // [確認_正常系] - update が値ストレージを拡張して成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_find);   // [確認_正常系] - update 後の値を検索できること。
    EXPECT_STREQ("12345678901",
                 static_cast<const char *>(value)); // [確認_正常系] - update 後の値が保存されていること。
    EXPECT_EQ(CPLAT_OK, actual_ret_config);         // [確認_正常系] - 更新後の設定を取得できること。
    EXPECT_EQ(2u, current.capacity);                // [確認_正常系] - upsert により capacity が 2 倍になること。
    EXPECT_GE(current.value_storage_size, 15u);     // [確認_正常系] - 新旧値が共存できる値ストレージ容量であること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュテーブルを破棄する。
}

// 自動再構築用の一時領域確保失敗時に元の領域と内容が維持されることの確認
TEST_F(hashtableGrowableTest, allocation_failure_preserves_original_buffers)
{
    // Arrange
    cplat_hashtable_config config = fixed_config(1);
    cplat_hashtable_growth_config growth = {};
    cplat_hashtable *ht = nullptr;
    const void *mgmt_before = nullptr;
    const void *data_before = nullptr;
    const void *mgmt_after = nullptr;
    const void *data_after = nullptr;
    size_t count = 0;

    (void)cplat_hashtable_create_growable(&config, &growth, &ht);
    (void)cplat_hashtable_add(ht, "a", "one", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_buffer_ref(ht, &mgmt_before, &data_before);

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_calloc(_, _))
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - 移行計画の確保が 1 回呼ばれること。
                                    // [Pre-Assert手順] - 移行計画の確保に失敗させる。

    // Act
    int actual_ret_add = cplat_hashtable_add(
        ht, "b", "two",
        CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 一時領域を確保できない状態で自動拡張を要求する。
    int actual_ret_ref = cplat_hashtable_buffer_ref(ht, &mgmt_after, &data_after);
    int actual_ret_count = cplat_hashtable_count(ht, &count);

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY, actual_ret_add); // [確認_異常系] - 自動拡張が OUT_OF_MEMORY を返すこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_ref);                // [確認_正常系] - 失敗後の領域参照を取得できること。
    EXPECT_EQ(mgmt_before, mgmt_after);                 // [確認_正常系] - 失敗後も管理領域の先頭が変わらないこと。
    EXPECT_EQ(data_before, data_after);                 // [確認_正常系] - 失敗後もデータ領域の先頭が変わらないこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_count);              // [確認_正常系] - 失敗後の件数を取得できること。
    EXPECT_EQ(1u, count);                               // [確認_正常系] - 失敗した追加によって件数が変わらないこと。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュテーブルを破棄する。
}

// 自動拡張上限到達時に LIMIT_EXCEEDED を返し元のテーブルが維持されることの確認
TEST_F(hashtableGrowableTest, limits_preserve_original_table)
{
    // Arrange
    cplat_hashtable_config config = fixed_config(2);
    cplat_hashtable_growth_config growth = {};
    cplat_hashtable *ht = nullptr;
    size_t count = 0;

    growth.max_capacity = 2;

    // Pre-Assert

    // Act
    (void)cplat_hashtable_create_growable(&config, &growth, &ht);
    (void)cplat_hashtable_add(ht, "a", "one", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_add(ht, "b", "two", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    int actual_ret_add = cplat_hashtable_add(
        ht, "c", "three", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 最大レコード数へ達したテーブルへ追加する。
    int actual_ret_count = cplat_hashtable_count(ht, &count);
    int actual_ret_validate = cplat_hashtable_validate(ht);

    // Assert
    EXPECT_EQ(CPLAT_ERR_LIMIT_EXCEEDED,
              actual_ret_add);                // [確認_異常系] - 上限到達時の add が LIMIT_EXCEEDED を返すこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_count);    // [確認_正常系] - 失敗後の件数を取得できること。
    EXPECT_EQ(2u, count);                     // [確認_正常系] - 失敗した追加によって件数が変わらないこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_validate); // [確認_正常系] - 失敗後も内部状態が整合すること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュテーブルを破棄する。
}

// 通常の create で構築したテーブルが満杯時に自動拡張せず LIMIT_EXCEEDED を返すことの確認
TEST_F(hashtableGrowableTest, ordinary_create_remains_fixed_capacity)
{
    // Arrange
    cplat_hashtable_config config = fixed_config(1);
    cplat_hashtable *ht = nullptr;

    // Pre-Assert

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    (void)cplat_hashtable_add(ht, "a", "one", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    int actual_ret_add = cplat_hashtable_add(
        ht, "b", "two", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 通常の満杯テーブルへ追加する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_LIMIT_EXCEEDED,
              actual_ret_add); // [確認_異常系] - 通常の create では固定長の結果コードを維持すること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュテーブルを破棄する。
}

// 初期設定と矛盾する自動拡張上限の指定が INVALID_ARGUMENT で拒否されることの確認
TEST_F(hashtableGrowableTest, create_rejects_invalid_growth_config)
{
    // Arrange
    cplat_hashtable_config config = fixed_config(2);
    cplat_hashtable_growth_config growth = {};
    cplat_hashtable *ht = reinterpret_cast<cplat_hashtable *>(1);

    growth.max_capacity = 1;
    growth.max_value_storage_size = 10;

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_hashtable_create_growable(&config, &growth, &ht); // [手順] - 初期値未満かつ固定長値用の上限を指定する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret); // [確認_異常系] - 不正な拡張設定を拒否すること。
    EXPECT_EQ(nullptr, ht);                            // [確認_異常系] - 失敗時に出力ハンドルが NULL になること。
}
