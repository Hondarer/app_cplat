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
    int actual_ret_create = cplat_hashtable_create_growable(
        &config, &growth,
        &ht); // [手順] - 上限なしの自動拡張の設定を指定して cplat_hashtable_create_growable() を呼び出す。
    (void)cplat_hashtable_add(ht, "a", "one", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_add(ht, "b", "two", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_find_recno(ht, "a", &record_before);
    int actual_ret_add = cplat_hashtable_add(
        ht, "c", "three",
        CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 満杯のテーブルに 3 件目のキーを指定して cplat_hashtable_add() を呼び出す。
    int actual_ret_find = cplat_hashtable_find_recno(ht, "a", &record_after);
    int actual_ret_config = cplat_hashtable_get_config_val(ht, &current);
    int actual_ret_count = cplat_hashtable_count(ht, &count);
    int actual_ret_validate = cplat_hashtable_validate(ht);

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret_create); // [確認_正常系] - cplat_hashtable_create_growable() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_add); // [確認_正常系] - 満杯時の cplat_hashtable_add() の戻り値が CPLAT_OK であること。(自動拡張して追加が成功したことの確認)
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_find); // [確認_正常系] - 自動拡張後の cplat_hashtable_find_recno() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        record_before,
        record_after); // [確認_正常系] - record_after が record_before と一致すること。(自動拡張で既存レコード番号が変わらないことの確認)
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_config); // [確認_正常系] - 自動拡張後の cplat_hashtable_get_config_val() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        4u, current.capacity); // [確認_正常系] - current.capacity が 4 であること。(capacity が 2 倍になったことの確認)
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_count); // [確認_正常系] - 自動拡張後の cplat_hashtable_count() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(3u, count);  // [確認_正常系] - count が 3 であること。(3 件すべてが格納されていることの確認)
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_validate); // [確認_正常系] - 自動拡張後の cplat_hashtable_validate() の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュ テーブルを破棄する。
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
        ht, "d", "555555",
        CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 7 バイトの値を指定して cplat_hashtable_add() を呼び出す。
    int actual_ret_find = cplat_hashtable_find_value_ref(ht, "d", &value);
    int actual_ret_config = cplat_hashtable_get_config_val(ht, &current);
    int actual_ret_validate = cplat_hashtable_validate(ht);

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_add); // [確認_正常系] - 断片化時の cplat_hashtable_add() の戻り値が CPLAT_OK であること。(再構築して追加が成功したことの確認)
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_find); // [確認_正常系] - 再構築後の cplat_hashtable_find_value_ref() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("555555",
                 static_cast<const char *>(
                     value)); // [確認_正常系] - value が "555555" であること。(追加した値が保存されていることの確認)
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_config); // [確認_正常系] - 再構築後の cplat_hashtable_get_config_val() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        20u,
        current
            .value_storage_size); // [確認_正常系] - current.value_storage_size が 20 であること。(断片化だけの場合はストレージ容量が増えないことの確認)
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_validate); // [確認_正常系] - 同容量再構築後の cplat_hashtable_validate() の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュ テーブルを破棄する。
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
    int actual_ret_update = cplat_hashtable_update_rec(
        ht, record, "12345678901"); // [手順] - 12 バイトの値を指定して cplat_hashtable_update_rec() を呼び出す。
    int actual_ret_find = cplat_hashtable_find_value_ref(ht, "a", &value);
    int actual_ret_config = cplat_hashtable_get_config_val(ht, &current);

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_update); // [確認_正常系] - cplat_hashtable_update_rec() の戻り値が CPLAT_OK であること。(値ストレージを拡張して更新が成功したことの確認)
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_find); // [確認_正常系] - 拡張後の cplat_hashtable_find_value_ref() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "12345678901",
        static_cast<const char *>(
            value)); // [確認_正常系] - value が "12345678901" であること。(更新した値が保存されていることの確認)
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_config); // [確認_正常系] - 更新後の cplat_hashtable_get_config_val() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        22u,
        current
            .value_storage_size); // [確認_正常系] - current.value_storage_size が 22 であること。(旧値と新値が共存できる必要量まで値ストレージが増えたことの確認)
    EXPECT_EQ(
        2u,
        current
            .capacity); // [確認_正常系] - current.capacity が 2 であること。(更新では capacity が変わらないことの確認)

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュ テーブルを破棄する。
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
    int actual_ret_upsert = cplat_hashtable_upsert(
        ht, "b", "22",
        &inserted); // [手順] - 満杯のテーブルに新しいキーを指定して cplat_hashtable_upsert() を呼び出す。
    int actual_ret_update = cplat_hashtable_update(
        ht, "b", "12345678901"); // [手順] - キーと 12 バイトの値を指定して cplat_hashtable_update() を呼び出す。
    int actual_ret_find = cplat_hashtable_find_value_ref(ht, "b", &value);
    int actual_ret_config = cplat_hashtable_get_config_val(ht, &current);

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_upsert); // [確認_正常系] - cplat_hashtable_upsert() の戻り値が CPLAT_OK であること。(レコードと値ストレージを拡張して upsert が成功したことの確認)
    EXPECT_EQ(1, inserted); // [確認_正常系] - inserted が 1 であること。(upsert が新規追加として完了したことの確認)
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_update); // [確認_正常系] - cplat_hashtable_update() の戻り値が CPLAT_OK であること。(値ストレージを拡張して update が成功したことの確認)
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_find); // [確認_正常系] - update 後の cplat_hashtable_find_value_ref() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "12345678901",
        static_cast<const char *>(
            value)); // [確認_正常系] - value が "12345678901" であること。(update 後の値が保存されていることの確認)
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_config); // [確認_正常系] - 更新後の cplat_hashtable_get_config_val() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        2u,
        current
            .capacity); // [確認_正常系] - current.capacity が 2 であること。(upsert により capacity が 2 倍になったことの確認)
    EXPECT_GE(
        current.value_storage_size,
        15u); // [確認_正常系] - current.value_storage_size が 15 以上であること。(新旧値が共存できる値ストレージ容量であることの確認)

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュ テーブルを破棄する。
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
        .WillOnce(Return(
            nullptr)); // [Pre-Assert確認_異常系] - cplat_calloc() が 1 回呼び出されること。(移行計画の確保が呼び出されたことの確認)
                       // [Pre-Assert手順] - cplat_calloc() から、移行計画の確保に対して nullptr を返す。

    // Act
    int actual_ret_add = cplat_hashtable_add(
        ht, "b", "two",
        CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 一時領域を確保できない状態で自動拡張が要る 2 件目のキーを指定して cplat_hashtable_add() を呼び出す。
    int actual_ret_ref = cplat_hashtable_buffer_ref(ht, &mgmt_after, &data_after);
    int actual_ret_count = cplat_hashtable_count(ht, &count);

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_OUT_OF_MEMORY,
        actual_ret_add); // [確認_異常系] - 自動拡張を要求した cplat_hashtable_add() の戻り値が CPLAT_ERR_OUT_OF_MEMORY であること。
    EXPECT_EQ(CPLAT_OK,
              actual_ret_ref); // [確認_正常系] - 失敗後の cplat_hashtable_buffer_ref() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        mgmt_before,
        mgmt_after); // [確認_正常系] - mgmt_after が mgmt_before と一致すること。(失敗後も管理領域の先頭が変わらないことの確認)
    EXPECT_EQ(
        data_before,
        data_after); // [確認_正常系] - data_after が data_before と一致すること。(失敗後もデータ領域の先頭が変わらないことの確認)
    EXPECT_EQ(CPLAT_OK,
              actual_ret_count); // [確認_正常系] - 失敗後の cplat_hashtable_count() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1u, count); // [確認_正常系] - count が 1 であること。(失敗した追加によって件数が変わらないことの確認)

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュ テーブルを破棄する。
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
        ht, "c", "three",
        CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 最大レコード数へ達したテーブルに 3 件目のキーを指定して cplat_hashtable_add() を呼び出す。
    int actual_ret_count = cplat_hashtable_count(ht, &count);
    int actual_ret_validate = cplat_hashtable_validate(ht);

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_LIMIT_EXCEEDED,
        actual_ret_add); // [確認_異常系] - 上限到達時の cplat_hashtable_add() の戻り値が CPLAT_ERR_LIMIT_EXCEEDED であること。
    EXPECT_EQ(CPLAT_OK,
              actual_ret_count); // [確認_正常系] - 失敗後の cplat_hashtable_count() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(2u, count); // [確認_正常系] - count が 2 であること。(失敗した追加によって件数が変わらないことの確認)
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_validate); // [確認_正常系] - 失敗後の cplat_hashtable_validate() の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュ テーブルを破棄する。
}

// 通常の create で構築したテーブルが満杯時に自動拡張せず LIMIT_EXCEEDED を返すことの確認
TEST_F(hashtableGrowableTest, ordinary_create_remains_fixed_capacity)
{
    // Arrange
    cplat_hashtable_config config = fixed_config(1);
    cplat_hashtable *ht = nullptr;

    // Pre-Assert

    // Act
    (void)cplat_hashtable_create(&config, nullptr, 0, nullptr, 0, &ht);
    (void)cplat_hashtable_add(ht, "a", "one", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    int actual_ret_add = cplat_hashtable_add(
        ht, "b", "two",
        CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 通常の満杯テーブルに 2 件目のキーを指定して cplat_hashtable_add() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_LIMIT_EXCEEDED,
        actual_ret_add); // [確認_異常系] - 通常の create で構築した満杯テーブルへの cplat_hashtable_add() の戻り値が CPLAT_ERR_LIMIT_EXCEEDED であること。(自動拡張せず固定長の結果コードを維持したことの確認)

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュ テーブルを破棄する。
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
    int actual_ret = cplat_hashtable_create_growable(
        &config, &growth,
        &ht); // [手順] - 初期値未満かつ固定長値用の上限を指定して cplat_hashtable_create_growable() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret); // [確認_異常系] - 不正な拡張設定を渡した cplat_hashtable_create_growable() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(nullptr,
              ht); // [確認_異常系] - ht が nullptr であること。(失敗時に出力ハンドルが nullptr になったことの確認)
}

// 有限の寿命 (3) で reuse_deleted が 0 の場合、push_deleted を呼ばなければ拡張し、1 回呼べば拡張しないことの確認
TEST_F(hashtableGrowableTest, finite_lifetime_three_grows_without_push_and_reuses_after_push)
{
    // Arrange
    cplat_hashtable_config config = fixed_config(2);
    cplat_hashtable_growth_config growth = {};
    cplat_hashtable_config current_no_push = {};
    cplat_hashtable_config current_with_push = {};
    cplat_hashtable *ht_no_push = nullptr;
    cplat_hashtable *ht_with_push = nullptr;
    size_t deleted_before_push = 0;
    size_t deleted_after_push = 0;
    size_t empty_after_push = 0;

    config.lifetime = 3;
    config.reuse_deleted = 0;

    // Pre-Assert

    // Act
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_create_growable(&config, &growth, &ht_no_push));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_add(ht_no_push, "k1", "v1", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_add(ht_no_push, "k2", "v2", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_delete(ht_no_push, "k1"));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_delete(ht_no_push, "k2"));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_add(ht_no_push, "k3", "v3", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE)); // [手順] - push_deleted なしで別キーを追加する。
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_get_config_val(ht_no_push, &current_no_push));

    ASSERT_EQ(CPLAT_OK, cplat_hashtable_create_growable(&config, &growth, &ht_with_push));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_add(ht_with_push, "k1", "v1", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_add(ht_with_push, "k2", "v2", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_delete(ht_with_push, "k1"));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_delete(ht_with_push, "k2"));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_deleted_count(ht_with_push, &deleted_before_push));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_push_deleted(ht_with_push)); // [手順] - lifetime=3 のため 1 回目の push_deleted で削除済みを空へ戻す。
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_deleted_count(ht_with_push, &deleted_after_push));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_empty_count(ht_with_push, &empty_after_push));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_add(ht_with_push, "k3", "v3", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE)); // [手順] - 空へ戻った後に別キーを追加する。
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_get_config_val(ht_with_push, &current_with_push));

    // Assert
    EXPECT_GT(current_no_push.capacity, 2U); // [確認_正常系] - push_deleted なしでは容量が増えること。
    EXPECT_EQ(2U, deleted_before_push); // [確認_正常系] - push_deleted 前の削除済み件数が 2 であること。
    EXPECT_EQ(0U, deleted_after_push);  // [確認_正常系] - push_deleted 1 回で削除済みが 0 になること。
    EXPECT_EQ(2U, empty_after_push);    // [確認_正常系] - 空きスロット数が 2 に戻ること。
    EXPECT_EQ(2U, current_with_push.capacity); // [確認_正常系] - push_deleted 1 回後は容量が増えないこと。

    // Cleanup
    cplat_hashtable_dispose(ht_with_push);
    cplat_hashtable_dispose(ht_no_push);
}

// 寿命が無限の場合、push_deleted を複数回呼んでも拡張し、purge_deleted を呼べば拡張しないことの確認
TEST_F(hashtableGrowableTest, infinite_lifetime_grows_even_with_push_and_does_not_grow_after_purge)
{
    // Arrange
    cplat_hashtable_config config = fixed_config(2);
    cplat_hashtable_growth_config growth = {};
    cplat_hashtable_config current_push = {};
    cplat_hashtable_config current_purge = {};
    cplat_hashtable *ht_push = nullptr;
    cplat_hashtable *ht_purge = nullptr;
    size_t deleted_after_push = 0;
    size_t empty_after_purge = 0;

    config.lifetime = CPLAT_HASHTABLE_LIFETIME_INFINITE;
    config.reuse_deleted = 0;

    // Pre-Assert

    // Act
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_create_growable(&config, &growth, &ht_push));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_add(ht_push, "k1", "v1", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_add(ht_push, "k2", "v2", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_delete(ht_push, "k1"));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_delete(ht_push, "k2"));
    for (int i = 0; i < 5; i++)
    {
        ASSERT_EQ(CPLAT_OK, cplat_hashtable_push_deleted(ht_push)); // [手順] - 寿命無限で push_deleted を繰り返す。
    }
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_deleted_count(ht_push, &deleted_after_push));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_add(ht_push, "k3", "v3", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_get_config_val(ht_push, &current_push));

    ASSERT_EQ(CPLAT_OK, cplat_hashtable_create_growable(&config, &growth, &ht_purge));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_add(ht_purge, "k1", "v1", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_add(ht_purge, "k2", "v2", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_delete(ht_purge, "k1"));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_delete(ht_purge, "k2"));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_purge_deleted(ht_purge)); // [手順] - purge_deleted で削除済みを空へ戻す。
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_empty_count(ht_purge, &empty_after_purge));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_add(ht_purge, "k3", "v3", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_get_config_val(ht_purge, &current_purge));

    // Assert
    EXPECT_EQ(2U, deleted_after_push); // [確認_正常系] - push_deleted を繰り返しても削除済み件数が 2 のままであること。
    EXPECT_GT(current_push.capacity, 2U); // [確認_正常系] - 別キーの追加で容量が増えること。
    EXPECT_EQ(2U, empty_after_purge); // [確認_正常系] - purge_deleted 後の空きスロット数が 2 であること。
    EXPECT_EQ(2U, current_purge.capacity); // [確認_正常系] - purge_deleted 後は容量が増えないこと。

    // Cleanup
    cplat_hashtable_dispose(ht_purge);
    cplat_hashtable_dispose(ht_push);
}

// 寿命が 2 の場合、異なるキーの削除と追加を繰り返しても容量が増えないことの確認
TEST_F(hashtableGrowableTest, lifetime_two_does_not_grow_on_repeated_add_delete)
{
    // Arrange
    cplat_hashtable_config config = fixed_config(2);
    cplat_hashtable_growth_config growth = {};
    cplat_hashtable_config current = {};
    cplat_hashtable *ht = nullptr;
    size_t deleted_count = 0;

    config.lifetime = 2;
    config.reuse_deleted = 0;

    // Pre-Assert

    // Act
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_create_growable(&config, &growth, &ht));
    for (int i = 0; i < 6; i++)
    {
        char key[8] = {};
        (void)snprintf(key, sizeof(key), "k%d", i);
        ASSERT_EQ(CPLAT_OK, cplat_hashtable_add(ht, key, "v", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE));
        ASSERT_EQ(CPLAT_OK, cplat_hashtable_delete(ht, key));
    } // [手順] - lifetime=2 で異なるキーの追加と削除を 6 回繰り返す。
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_get_config_val(ht, &current));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_deleted_count(ht, &deleted_count));

    // Assert
    EXPECT_EQ(2U, current.capacity); // [確認_正常系] - 容量が初期値 2 のままであること。
    EXPECT_EQ(0U, deleted_count);    // [確認_正常系] - 削除済みが残らないこと。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// reuse_deleted が非 0 の場合、寿命が無限でも容量が増えないことの確認
TEST_F(hashtableGrowableTest, reuse_deleted_prevents_growth_even_with_infinite_lifetime)
{
    // Arrange
    cplat_hashtable_config config = fixed_config(2);
    cplat_hashtable_growth_config growth = {};
    cplat_hashtable_config current = {};
    cplat_hashtable *ht = nullptr;
    size_t deleted_count = 0;

    config.lifetime = CPLAT_HASHTABLE_LIFETIME_INFINITE;
    config.reuse_deleted = 1;

    // Pre-Assert

    // Act
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_create_growable(&config, &growth, &ht));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_add(ht, "k1", "v1", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_add(ht, "k2", "v2", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_delete(ht, "k1"));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_delete(ht, "k2"));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_add(ht, "k3", "v3", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_add(ht, "k4", "v4", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE)); // [手順] - 削除済みスロットを再利用して別キーを追加する。
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_get_config_val(ht, &current));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_deleted_count(ht, &deleted_count));

    // Assert
    EXPECT_EQ(2U, current.capacity); // [確認_正常系] - 容量が 2 のまま増えないこと。
    EXPECT_EQ(0U, deleted_count);    // [確認_正常系] - 削除済みスロットが再利用されたこと。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// 同じキーの削除と再追加では削除済みが復活し容量が増えないことの確認
TEST_F(hashtableGrowableTest, deleting_and_readding_same_key_does_not_grow_capacity)
{
    // Arrange
    cplat_hashtable_config config = fixed_config(2);
    cplat_hashtable_growth_config growth = {};
    cplat_hashtable_config current = {};
    cplat_hashtable *ht = nullptr;
    size_t deleted_count = 0;

    config.lifetime = CPLAT_HASHTABLE_LIFETIME_INFINITE;
    config.reuse_deleted = 0;

    // Pre-Assert

    // Act
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_create_growable(&config, &growth, &ht));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_add(ht, "k1", "v1", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_add(ht, "k2", "v2", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_delete(ht, "k1"));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_add(ht, "k1", "v1_new", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE)); // [手順] - 削除済みとなった同一キーを再び追加する。
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_get_config_val(ht, &current));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_deleted_count(ht, &deleted_count));

    // Assert
    EXPECT_EQ(2U, current.capacity); // [確認_正常系] - 容量が 2 のまま増えないこと。
    EXPECT_EQ(0U, deleted_count);    // [確認_正常系] - 削除済みレコードが復活し削除済み数が 0 になること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// レコード用スロットに空きがあっても可変長キー ストレージが不足する追加で自動拡張されることの確認
TEST_F(hashtableGrowableTest, add_grows_key_storage_even_when_record_slots_are_empty)
{
    // Arrange
    cplat_hashtable_config config = fixed_config(4);
    cplat_hashtable_growth_config growth = {};
    cplat_hashtable_config current = {};
    cplat_hashtable *ht = nullptr;
    size_t empty_count = 0;
    const void *val_a = nullptr;
    const void *val_b = nullptr;

    config.key_type = CPLAT_HASHTABLE_FIELD_VARIABLE_STRING;
    config.key_size = 0;
    config.key_storage_size = 10;

    // Pre-Assert

    // Act
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_create_growable(&config, &growth, &ht));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_add(ht, "aaaa", "v1", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE)); // 5 byte 消費
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_empty_count(ht, &empty_count));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_add(ht, "bbbbbb", "v2", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE)); // [手順] - 空きスロットが残る状態で可変長キー領域を超える 7 byte のキーを追加する。
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_get_config_val(ht, &current));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_find_value_ref(ht, "aaaa", &val_a));
    ASSERT_EQ(CPLAT_OK, cplat_hashtable_find_value_ref(ht, "bbbbbb", &val_b));

    // Assert
    EXPECT_EQ(3U, empty_count); // [確認_正常系] - 追加前の空きスロットが 3 件存在すること。
    EXPECT_EQ(4U, current.capacity); // [確認_正常系] - レコード容量 4 は増えないこと。
    EXPECT_GT(current.key_storage_size, 10U); // [確認_正常系] - key_storage_size が 10 より大きく拡張されること。
    EXPECT_STREQ("v1", static_cast<const char *>(val_a)); // [確認_正常系] - 既存キーの値が保たれること。
    EXPECT_STREQ("v2", static_cast<const char *>(val_b)); // [確認_正常系] - 追加キーの値が読めること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}
