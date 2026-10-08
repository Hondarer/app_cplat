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

void fill_variable_config(cplat_hashtable_config *config, size_t capacity, size_t key_storage, size_t value_storage)
{
    *config = {};
    config->capacity = capacity;
    config->key_type = CPLAT_HASHTABLE_FIELD_VARIABLE_STRING;
    config->value_type = CPLAT_HASHTABLE_FIELD_VARIABLE_STRING;
    config->timestamp_scope = CPLAT_HASHTABLE_TIMESTAMP_SCOPE_RECORD;
    config->key_storage_size = key_storage;
    config->value_storage_size = value_storage;
    config->lifetime = 5;
}

void fill_value(std::vector<unsigned char> *buf, const char *text)
{
    buf->assign(buf->size(), 0);
    if (text != nullptr)
    {
        std::memcpy(buf->data(), text, std::strlen(text));
    }
}

cplat_timespec make_timestamp(time_t sec)
{
    cplat_timespec ts = {};

    ts.tv_sec = sec;
    ts.tv_nsec = 0;
    return ts;
}

} // namespace

class hashtableResizeTest : public Test
{
  protected:
    NiceMock<Mock_cplat> mock_cplat_;
};

// ハッシュテーブル拡大時にレコード番号、世代番号、タイムスタンプが保持されることの確認
TEST_F(hashtableResizeTest, grow_preserves_record_numbers_and_stamps)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable_config grown = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    uint64_t rec_a_before = 0;
    uint64_t rec_a_after = 0;
    uint64_t generation_before = 0;
    uint64_t generation_after = 0;
    cplat_timespec timestamp_before = {};
    cplat_timespec timestamp_after = {};
    size_t in_use = 0;
    size_t empty = 0;

    fill_config(&config, 4, 5);
    fill_value(&value, "v1");

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    (void)cplat_hashtable_add(ht, "a", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_add(ht, "b", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_find_recno(ht, "a", &rec_a_before);
    (void)cplat_hashtable_get_table_generation(ht, &generation_before);
    (void)cplat_hashtable_get_table_timestamp_val(ht, &timestamp_before);
    grown = config;
    grown.capacity = 16;                                        // [状態] - レコード数を 4 から 16 へ増やす。
    int actual_ret_resize = cplat_hashtable_resize(ht, &grown); // [手順] - 拡大する。
    int actual_ret_find = cplat_hashtable_find_recno(ht, "a", &rec_a_after);
    int actual_ret_generation = cplat_hashtable_get_table_generation(ht, &generation_after);
    int actual_ret_timestamp = cplat_hashtable_get_table_timestamp_val(ht, &timestamp_after);
    int actual_ret_counts = cplat_hashtable_count_status(ht, &in_use, NULL, &empty);
    int actual_ret_validate = cplat_hashtable_validate(ht);
    int actual_ret_add_more = cplat_hashtable_add(ht, "z", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_resize); // [確認_正常系] - 拡大が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_find);   // [確認_正常系] - 拡大後もキーを引けること。
    EXPECT_EQ(rec_a_before, rec_a_after);   // [確認_正常系] - 拡大でレコード番号が保存されること。
    EXPECT_EQ(CPLAT_OK, actual_ret_generation);
    // [確認_正常系] - `cplat_hashtable_get_table_generation(ht, &generation_after)` の戻り値が `CPLAT_OK` であること。
    EXPECT_EQ(generation_before, generation_after); // [確認_正常系] - 拡大でテーブル世代が進まないこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_timestamp);
    // [確認_正常系] - `cplat_hashtable_get_table_timestamp_val(ht, &timestamp_after)` の戻り値が `CPLAT_OK` であること。
    EXPECT_EQ(timestamp_before.tv_sec, timestamp_after.tv_sec); // [確認_正常系] - 拡大でテーブル時刻が変わらないこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_counts);
    // [確認_正常系] - `cplat_hashtable_count_status(ht, &in_use, NULL, &empty)` の戻り値が `CPLAT_OK` であること。
    EXPECT_EQ(2u, in_use);                    // [確認_正常系] - 使用中件数が保たれること。
    EXPECT_EQ(14u, empty);                    // [確認_正常系] - 増やしたぶんが空きとして使えること。
    EXPECT_EQ(CPLAT_OK, actual_ret_validate); // [確認_正常系] - 拡大後も内部整合性が保たれること。
    EXPECT_EQ(CPLAT_OK, actual_ret_add_more); // [確認_正常系] - 拡大後に追加できること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// ハッシュテーブル縮小時に範囲外となったレコード番号が詰め直され値と世代が保持されることの確認
TEST_F(hashtableResizeTest, shrink_renumbers_records_and_keeps_values)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable_config shrunk = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    std::vector<unsigned char> read_back(8, 0);
    cplat_timespec timestamp = make_timestamp(100);
    uint64_t rec_before = 0;
    uint64_t rec_after = 0;
    uint64_t generation_after = 0;
    size_t in_use = 0;
    size_t required = 0;

    fill_config(&config, 8, 5);
    fill_value(&value, "keep");

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    (void)cplat_hashtable_insert_direct(ht, 7, "a", 1, value.data(), &timestamp,
                                        4); // [手順] - 縮小後の範囲外になる番号へ置く。
    (void)cplat_hashtable_find_recno(ht, "a", &rec_before);
    shrunk = config;
    shrunk.capacity = 2;                                         // [状態] - レコード数を 8 から 2 へ減らす。
    int actual_ret_resize = cplat_hashtable_resize(ht, &shrunk); // [手順] - 縮小する。
    int actual_ret_find = cplat_hashtable_find_recno(ht, "a", &rec_after);
    int actual_ret_read = cplat_hashtable_find_value_copy(ht, "a", read_back.data(), read_back.size(), &required);
    int actual_ret_generation = cplat_hashtable_find_generation(ht, "a", &generation_after);
    int actual_ret_counts = cplat_hashtable_count(ht, &in_use);
    int actual_ret_validate = cplat_hashtable_validate(ht);

    // Assert
    EXPECT_EQ(7u, rec_before);              // [確認_正常系] - 縮小前は指定した番号にあること。
    EXPECT_EQ(CPLAT_OK, actual_ret_resize); // [確認_正常系] - 縮小が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_find);   // [確認_正常系] - 縮小後もキーを引けること。
    EXPECT_EQ(1u, rec_after);               // [確認_正常系] - 範囲外だったレコード番号が詰め直されること。
    EXPECT_EQ(CPLAT_OK, actual_ret_read);
    // [確認_正常系] - `cplat_hashtable_find_value_copy(ht, "a", read_back.data(), read_back.size(), &required)` の戻り値が `CPLAT_OK` であること。
    EXPECT_STREQ("keep", reinterpret_cast<const char *>(read_back.data())); // [確認_正常系] - 値が保たれること。
    EXPECT_EQ(CPLAT_OK, actual_ret_generation);
    // [確認_正常系] - `cplat_hashtable_find_generation(ht, "a", &generation_after)` の戻り値が `CPLAT_OK` であること。
    EXPECT_EQ(4u, generation_after); // [確認_正常系] - レコードの世代カウンターが引き継がれること。
    EXPECT_EQ(CPLAT_OK, actual_ret_counts);
    // [確認_正常系] - `cplat_hashtable_count(ht, &in_use)` の戻り値が `CPLAT_OK` であること。
    EXPECT_EQ(1u, in_use);                    // [確認_正常系] - 使用中件数が保たれること。
    EXPECT_EQ(CPLAT_OK, actual_ret_validate); // [確認_正常系] - 縮小後も内部整合性が保たれること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// 使用中レコード数が縮小後の容量に収まらない場合に LIMIT_EXCEEDED が返り状態が維持されることの確認
TEST_F(hashtableResizeTest, shrink_rejects_when_in_use_records_do_not_fit)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable_config shrunk = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    size_t in_use = 0;
    uint64_t rec_a = 0;

    fill_config(&config, 4, 5);
    fill_value(&value, "v");

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    (void)cplat_hashtable_add(ht, "a", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_add(ht, "b", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_add(ht, "c", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    shrunk = config;
    shrunk.capacity = 2; // [状態] - 使用中 3 件が収まらないレコード数にする。
    int actual_ret_resize = cplat_hashtable_resize(ht, &shrunk);
    int actual_ret_counts = cplat_hashtable_count(ht, &in_use);
    int actual_ret_find = cplat_hashtable_find_recno(ht, "a", &rec_a);
    int actual_ret_validate = cplat_hashtable_validate(ht);

    // Assert
    EXPECT_EQ(CPLAT_ERR_LIMIT_EXCEEDED,
              actual_ret_resize); // [確認_異常系] - 使用中が収まらない縮小が LIMIT_EXCEEDED であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_counts);
    // [確認_正常系] - `cplat_hashtable_count(ht, &in_use)` の戻り値が `CPLAT_OK` であること。
    EXPECT_EQ(3u, in_use);                    // [確認_正常系] - 失敗してもテーブルが変わらないこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_find);     // [確認_正常系] - 失敗後もキーを引けること。
    EXPECT_EQ(CPLAT_OK, actual_ret_validate); // [確認_正常系] - 失敗後も内部整合性が保たれること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// reuse_deleted が無効な場合に削除済みレコードを破棄できず縮小が LIMIT_EXCEEDED となることの確認
TEST_F(hashtableResizeTest, shrink_rejects_deleted_records_when_reuse_is_disabled)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable_config shrunk = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    size_t deleted = 0;

    fill_config(&config, 4, CPLAT_HASHTABLE_LIFETIME_INFINITE);
    config.reuse_deleted = 0; // [状態] - 削除済みを自動的には手放さない設定にする。
    fill_value(&value, "v");

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    (void)cplat_hashtable_add(ht, "a", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_add(ht, "b", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_add(ht, "c", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_delete(ht, "b");
    (void)cplat_hashtable_delete(ht, "c"); // [手順] - 削除済みを 2 件つくる。
    shrunk = config;
    shrunk.capacity = 2; // [状態] - 使用中 1 件と削除済み 2 件が収まらないレコード数にする。
    int actual_ret_resize = cplat_hashtable_resize(ht, &shrunk);
    int actual_ret_counts = cplat_hashtable_deleted_count(ht, &deleted);

    // Assert
    EXPECT_EQ(CPLAT_ERR_LIMIT_EXCEEDED,
              actual_ret_resize); // [確認_異常系] - 削除済みを捨てずに失敗すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_counts);
    // [確認_正常系] - `cplat_hashtable_deleted_count(ht, &deleted)` の戻り値が `CPLAT_OK` であること。
    EXPECT_EQ(2u, deleted); // [確認_正常系] - 削除済みが 1 件も捨てられていないこと。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// reuse_deleted が有効な場合に最も古い世代の削除済みレコードを破棄して縮小できることの確認
TEST_F(hashtableResizeTest, shrink_drops_oldest_deleted_records_when_reuse_is_enabled)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable_config shrunk = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    std::vector<char> kept_key(16, 0);
    cplat_timespec timestamp = make_timestamp(100);
    uint64_t deleted_record = 0;
    int has_deleted = 0;
    size_t deleted = 0;
    size_t required = 0;

    fill_config(&config, 4, CPLAT_HASHTABLE_LIFETIME_INFINITE);
    config.reuse_deleted = 1; // [状態] - 削除済みを手放してよい設定にする。
    fill_value(&value, "v");

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    (void)cplat_hashtable_insert_direct(ht, 1, "a", 1, value.data(), &timestamp, 9); // [手順] - 使用中を 1 件置く。
    (void)cplat_hashtable_insert_direct(ht, 2, "b", 2, value.data(), &timestamp,
                                        8); // [手順] - 世代が新しい削除済みを置く。
    (void)cplat_hashtable_insert_direct(ht, 3, "c", 2, value.data(), &timestamp,
                                        3); // [手順] - 世代が古い削除済みを置く。
    shrunk = config;
    shrunk.capacity = 2; // [状態] - 3 件のうち 1 件が収まらないレコード数にする。
    int actual_ret_resize = cplat_hashtable_resize(ht, &shrunk);
    int actual_ret_counts = cplat_hashtable_deleted_count(ht, &deleted);
    /* 削除済みは find_recno で引けないため、走査してキーを読み、どちらが残ったかを判定する。 */
    int actual_ret_scan =
        cplat_hashtable_next_record(ht, 0, CPLAT_HASHTABLE_SCAN_DELETED, &deleted_record, &has_deleted);
    int actual_ret_key = cplat_hashtable_get_key_copy(ht, deleted_record, kept_key.data(), kept_key.size(), &required);
    int actual_ret_validate = cplat_hashtable_validate(ht);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_resize); // [確認_正常系] - 削除済みを除外して縮小できること。
    EXPECT_EQ(CPLAT_OK, actual_ret_counts);
    // [確認_正常系] - `cplat_hashtable_deleted_count(ht, &deleted)` の戻り値が `CPLAT_OK` であること。
    EXPECT_EQ(1u, deleted);               // [確認_正常系] - 削除済みが 1 件だけ残ること。
    EXPECT_EQ(CPLAT_OK, actual_ret_scan); // [確認_正常系] - 削除済みを走査できること。
    EXPECT_EQ(1, has_deleted);            // [確認_正常系] - 削除済みが 1 件見つかること。
    EXPECT_EQ(CPLAT_OK, actual_ret_key);
    // [確認_正常系] - `cplat_hashtable_get_key_copy(ht, deleted_record, kept_key.data(), kept_key.size(), &required)` の戻り値が `CPLAT_OK` であること。
    EXPECT_STREQ("b", kept_key.data());       // [確認_正常系] - 世代が新しい削除済みが残り、古い方が破棄されること。
    EXPECT_EQ(CPLAT_OK, actual_ret_validate); // [確認_正常系] - 縮小後も内部整合性が保たれること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// 可変長ストレージのリサイズおよび収まらない容量への縮小拒否の確認
TEST_F(hashtableResizeTest, resizes_variable_storage_and_rejects_when_it_does_not_fit)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable_config grown = {};
    cplat_hashtable_config too_small = {};
    cplat_hashtable *ht = nullptr;
    std::vector<char> read_back(64, 0);
    size_t required = 0;

    fill_variable_config(&config, 4, 32, 32);

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    (void)cplat_hashtable_add(ht, "alpha", "value-alpha", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    grown = config;
    grown.key_storage_size = 128;
    grown.value_storage_size = 128; // [状態] - 可変長ストレージを増やす。
    int actual_ret_grow = cplat_hashtable_resize(ht, &grown);
    int actual_ret_read = cplat_hashtable_find_value_copy(ht, "alpha", read_back.data(), read_back.size(), &required);
    int actual_ret_add_after =
        cplat_hashtable_add(ht, "bravo", "value-bravo-is-longer", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    too_small = grown;
    too_small.value_storage_size = 4; // [状態] - 保持している値が収まらない容量にする。
    int actual_ret_shrink = cplat_hashtable_resize(ht, &too_small);
    int actual_ret_validate = cplat_hashtable_validate(ht);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_grow); // [確認_正常系] - 可変長ストレージを増やせること。
    EXPECT_EQ(CPLAT_OK, actual_ret_read);
    // [確認_正常系] - `cplat_hashtable_find_value_copy(ht, "alpha", read_back.data(), read_back.size(), &required)` の戻り値が `CPLAT_OK` であること。
    EXPECT_STREQ("value-alpha", read_back.data()); // [確認_正常系] - 可変長の値が保たれること。
    EXPECT_EQ(CPLAT_OK, actual_ret_add_after);     // [確認_正常系] - 増やしたストレージを使えること。
    EXPECT_EQ(CPLAT_ERR_STORAGE_FULL,
              actual_ret_shrink);             // [確認_異常系] - 収まらない縮小が STORAGE_FULL であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_validate); // [確認_正常系] - 失敗後も内部整合性が保たれること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// cplat_hashtable_resize に不正な引数や変更不可項目を指定した場合のエラー返却の確認
TEST_F(hashtableResizeTest, resize_guards_reject_invalid_arguments)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable_config changed = {};
    cplat_hashtable *ht = nullptr;
    cplat_hashtable *external = nullptr;
    std::vector<uint64_t> mgmt_buf;
    std::vector<uint64_t> data_buf;
    size_t mgmt_size = 0;
    size_t data_size = 0;

    fill_config(&config, 4, 5);
    (void)cplat_hashtable_required_size(&config, &mgmt_size, &data_size);
    mgmt_buf.assign((mgmt_size / sizeof(uint64_t)) + 1u, 0);
    data_buf.assign((data_size / sizeof(uint64_t)) + 1u, 0);

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    (void)cplat_hashtable_create(&config, mgmt_buf.data(), mgmt_size, data_buf.data(), data_size, &external);
    int actual_ret_null_ht = cplat_hashtable_resize(NULL, &config);
    int actual_ret_null_config = cplat_hashtable_resize(ht, NULL);
    changed = config;
    changed.value_size = 16; // [状態] - 変えてはならない項目を変えた設定を用意する。
    int actual_ret_incompatible = cplat_hashtable_resize(ht, &changed);
    changed = config;
    changed.capacity = 0; // [状態] - 設定として不正な値を用意する。
    int actual_ret_invalid_config = cplat_hashtable_resize(ht, &changed);
    changed = config;
    changed.capacity = 8;
    int actual_ret_external =
        cplat_hashtable_resize(external, &changed); // [手順] - 外部領域のテーブルを縮めようとする。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_null_ht);     // [確認_異常系] - ht が NULL なら失敗すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_null_config); // [確認_異常系] - 設定が NULL なら失敗すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_incompatible); // [確認_異常系] - 変えてはならない項目の変更が失敗すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_invalid_config); // [確認_異常系] - 設定として不正な値が失敗すること。
    EXPECT_EQ(CPLAT_ERR_UNSUPPORTED,
              actual_ret_external); // [確認_異常系] - 外部領域のテーブルは UNSUPPORTED であること。

    // Cleanup
    cplat_hashtable_dispose(external);
    cplat_hashtable_dispose(ht);
}

// cplat_hashtable_rebuild_into で呼び出し元指定領域へテーブルを再構築・移行できることの確認
TEST_F(hashtableResizeTest, rebuild_into_moves_table_to_caller_supplied_region)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable_config grown = {};
    cplat_hashtable *src = nullptr;
    cplat_hashtable *dst = nullptr;
    std::vector<unsigned char> value(8, 0);
    std::vector<unsigned char> read_back(8, 0);
    std::vector<uint64_t> mgmt_buf;
    std::vector<uint64_t> data_buf;
    size_t mgmt_size = 0;
    size_t data_size = 0;
    uint64_t rec_before = 0;
    uint64_t rec_after = 0;
    size_t required = 0;

    fill_config(&config, 4, 5);
    fill_value(&value, "v1");
    grown = config;
    grown.capacity = 16;
    (void)cplat_hashtable_required_size(&grown, &mgmt_size, &data_size);
    mgmt_buf.assign((mgmt_size / sizeof(uint64_t)) + 1u, 0);
    data_buf.assign((data_size / sizeof(uint64_t)) + 1u, 0);

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &src);
    (void)cplat_hashtable_add(src, "a", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_find_recno(src, "a", &rec_before);
    int actual_ret_rebuild = cplat_hashtable_rebuild_into(src, &grown, mgmt_buf.data(), mgmt_size, data_buf.data(),
                                                          data_size, &dst); // [手順] - 外部領域へ移す。
    int actual_ret_find_dst = cplat_hashtable_find_recno(dst, "a", &rec_after);
    int actual_ret_read = cplat_hashtable_find_value_copy(dst, "a", read_back.data(), read_back.size(), &required);
    int actual_ret_validate = cplat_hashtable_validate(dst);
    uint64_t rec_src_after = 0;
    int actual_ret_src_intact = cplat_hashtable_find_recno(src, "a", &rec_src_after);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_rebuild);  // [確認_正常系] - 外部領域への移行が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_find_dst); // [確認_正常系] - 移行先でキーを引けること。
    EXPECT_EQ(rec_before, rec_after);         // [確認_正常系] - 拡大なのでレコード番号が保存されること。
    EXPECT_EQ(CPLAT_OK, actual_ret_read);
    // [確認_正常系] - `cplat_hashtable_find_value_copy(dst, "a", read_back.data(), read_back.size(), &required)` の戻り値が `CPLAT_OK` であること。
    EXPECT_STREQ("v1", reinterpret_cast<const char *>(read_back.data())); // [確認_正常系] - 値が保たれること。
    EXPECT_EQ(CPLAT_OK, actual_ret_validate);   // [確認_正常系] - 移行先の内部整合性が保たれること。
    EXPECT_EQ(CPLAT_OK, actual_ret_src_intact); // [確認_正常系] - 移行元が変更されずに残ること。
    EXPECT_EQ(rec_before, rec_src_after);       // [確認_正常系] - 移行元のレコード番号が変わらないこと。

    // Cleanup
    cplat_hashtable_dispose(dst);
    cplat_hashtable_dispose(src);
}

// cplat_hashtable_rebuild_into に不正な引数や容量不足を指定した場合のエラー返却の確認
TEST_F(hashtableResizeTest, rebuild_into_guards_reject_invalid_arguments)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable_config shrunk = {};
    cplat_hashtable *src = nullptr;
    cplat_hashtable *dst = reinterpret_cast<cplat_hashtable *>(1);
    std::vector<unsigned char> value(8, 0);
    std::vector<uint64_t> mgmt_buf;
    std::vector<uint64_t> data_buf;
    size_t mgmt_size = 0;
    size_t data_size = 0;

    fill_config(&config, 4, 5);
    fill_value(&value, "v");
    shrunk = config;
    shrunk.capacity = 1;
    (void)cplat_hashtable_required_size(&shrunk, &mgmt_size, &data_size);
    mgmt_buf.assign((mgmt_size / sizeof(uint64_t)) + 1u, 0);
    data_buf.assign((data_size / sizeof(uint64_t)) + 1u, 0);

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &src);
    (void)cplat_hashtable_add(src, "a", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_add(src, "b", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    int actual_ret_null_src =
        cplat_hashtable_rebuild_into(NULL, &shrunk, mgmt_buf.data(), mgmt_size, data_buf.data(), data_size, &dst);
    int actual_ret_null_mgmt =
        cplat_hashtable_rebuild_into(src, &shrunk, NULL, mgmt_size, data_buf.data(), data_size, &dst);
    int actual_ret_null_out =
        cplat_hashtable_rebuild_into(src, &shrunk, mgmt_buf.data(), mgmt_size, data_buf.data(), data_size, NULL);
    /* 使用中 2 件がレコード数 1 へ収まらないため、領域へ触れずに失敗する。 */
    int actual_ret_limit =
        cplat_hashtable_rebuild_into(src, &shrunk, mgmt_buf.data(), mgmt_size, data_buf.data(), data_size, &dst);

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_null_src); // [確認_異常系] - 移行元が NULL なら失敗すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_null_mgmt); // [確認_異常系] - 管理領域が NULL なら失敗すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_null_out); // [確認_異常系] - 出力先が NULL なら失敗すること。
    EXPECT_EQ(CPLAT_ERR_LIMIT_EXCEEDED,
              actual_ret_limit); // [確認_異常系] - 収まらない移行が LIMIT_EXCEEDED であること。
    EXPECT_EQ(nullptr, dst);     // [確認_異常系] - 失敗時に出力先が NULL になること。
    EXPECT_EQ(0u, mgmt_buf[0]);  // [確認_異常系] - 失敗時に渡された領域へ触れていないこと。

    // Cleanup
    cplat_hashtable_dispose(src);
}

/*
 * 次の防御的な分岐は、公開 API 経由では到達できない
 * (how-to-test.md「到達できない条件への対処」手順 3)。
 * - hashtable_plan_migration の追い出しループで候補が尽きる分岐:
 *   使用中が新しい capacity に収まることを先に検査しているため、
 *   kept が capacity を超える間は必ず削除済みが 1 件以上残っている。
 * - hashtable_apply_migration の insert_direct 失敗分岐、および
 *   resize / rebuild_into がその戻り値で失敗する分岐:
 *   収まることと可変長ストレージが足りることを事前に検査済みで、
 *   移行先は構築直後で断片化がなく、移行元のキーは一意である。
 */

// リサイズ時に変更不可な設定項目を変更しようとした場合にすべて INVALID_ARGUMENT が返ることの確認
TEST_F(hashtableResizeTest, resize_rejects_every_immutable_config_field)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<int> results;

    fill_config(&config, 4, 5);

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    {
        cplat_hashtable_config changed = config;

        changed.key_type = CPLAT_HASHTABLE_FIELD_FIXED_BINARY;
        results.push_back(cplat_hashtable_resize(ht, &changed)); // [手順] - キーの形式を変えて呼ぶ。
    }
    {
        cplat_hashtable_config changed = config;

        changed.value_type = CPLAT_HASHTABLE_FIELD_VARIABLE_STRING;
        changed.value_size = 0;
        changed.value_storage_size = 64;
        results.push_back(cplat_hashtable_resize(ht, &changed)); // [手順] - 値の形式を変えて呼ぶ。
    }
    {
        cplat_hashtable_config changed = config;

        changed.key_size = 16;
        results.push_back(cplat_hashtable_resize(ht, &changed)); // [手順] - 固定長キーの長さを変えて呼ぶ。
    }
    {
        cplat_hashtable_config changed = config;

        changed.value_align = 8;
        results.push_back(cplat_hashtable_resize(ht, &changed)); // [手順] - 値の格納境界を変えて呼ぶ。
    }
    {
        cplat_hashtable_config changed = config;

        changed.timestamp_scope = CPLAT_HASHTABLE_TIMESTAMP_SCOPE_TABLE;
        results.push_back(cplat_hashtable_resize(ht, &changed)); // [手順] - 刻印の粒度を変えて呼ぶ。
    }
    {
        cplat_hashtable_config changed = config;

        changed.lifetime = 9;
        results.push_back(cplat_hashtable_resize(ht, &changed)); // [手順] - 寿命を変えて呼ぶ。
    }
    {
        cplat_hashtable_config changed = config;

        changed.reuse_deleted = 1;
        results.push_back(cplat_hashtable_resize(ht, &changed)); // [手順] - 削除済みの再利用可否を変えて呼ぶ。
    }

    // Assert
    ASSERT_EQ(7u, results.size()); // [確認_正常系] - 7 通りすべてを試したこと。
    for (size_t i = 0; i < results.size(); ++i)
    {
        EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, results[i])
            << "index " << i; // [確認_異常系 回数=7] - 変えてはならない項目の変更がすべて INVALID_ARGUMENT であること。
    }

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// 縮小時に複数の削除済みレコードが世代の古い順に破棄されることの確認
TEST_F(hashtableResizeTest, shrink_drops_multiple_deleted_records_in_generation_order)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable_config shrunk = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    std::vector<char> kept_key(16, 0);
    cplat_timespec timestamp = make_timestamp(100);
    uint64_t deleted_record = 0;
    int has_deleted = 0;
    size_t deleted = 0;
    size_t required = 0;

    fill_config(&config, 4, CPLAT_HASHTABLE_LIFETIME_INFINITE);
    config.reuse_deleted = 1;
    fill_value(&value, "v");

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    (void)cplat_hashtable_insert_direct(ht, 1, "a", 2, value.data(), &timestamp, 30);
    (void)cplat_hashtable_insert_direct(ht, 2, "b", 2, value.data(), &timestamp, 10);
    (void)cplat_hashtable_insert_direct(ht, 3, "c", 2, value.data(), &timestamp,
                                        20); // [手順] - 世代の異なる削除済みを 3 件置く。
    shrunk = config;
    shrunk.capacity = 1; // [状態] - 3 件のうち 2 件が収まらないレコード数にする。
    int actual_ret_resize = cplat_hashtable_resize(ht, &shrunk);
    int actual_ret_counts = cplat_hashtable_deleted_count(ht, &deleted);
    int actual_ret_scan =
        cplat_hashtable_next_record(ht, 0, CPLAT_HASHTABLE_SCAN_DELETED, &deleted_record, &has_deleted);
    int actual_ret_key = cplat_hashtable_get_key_copy(ht, deleted_record, kept_key.data(), kept_key.size(), &required);
    int actual_ret_validate = cplat_hashtable_validate(ht);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_resize); // [確認_正常系] - 削除済みを 2 件除外して縮小できること。
    EXPECT_EQ(CPLAT_OK, actual_ret_counts);
    // [確認_正常系] - `cplat_hashtable_deleted_count(ht, &deleted)` の戻り値が `CPLAT_OK` であること。
    EXPECT_EQ(1u, deleted); // [確認_正常系] - 削除済みが 1 件だけ残ること。
    EXPECT_EQ(CPLAT_OK, actual_ret_scan);
    // [確認_正常系] - `cplat_hashtable_next_record(ht, 0, CPLAT_HASHTABLE_SCAN_DELETED, &deleted_record, &has_deleted)` の戻り値が `CPLAT_OK` であること。
    EXPECT_EQ(1, has_deleted);
    // [確認_正常系] - 削除済みのレコードが見つかり、`has_deleted` が `1` であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_key);
    // [確認_正常系] - `cplat_hashtable_get_key_copy(ht, deleted_record, kept_key.data(), kept_key.size(), &required)` の戻り値が `CPLAT_OK` であること。
    EXPECT_STREQ("a", kept_key.data());       // [確認_正常系] - 世代が最も新しい削除済みだけが残ること。
    EXPECT_EQ(CPLAT_OK, actual_ret_validate); // [確認_正常系] - 縮小後も内部整合性が保たれること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// リサイズ時の calloc 失敗により OUT_OF_MEMORY が返り状態が維持されることの確認
TEST_F(hashtableResizeTest, resize_reports_out_of_memory_when_calloc_fails)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable_config grown = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    size_t in_use = 0;

    fill_config(&config, 4, 5);
    fill_value(&value, "v");

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    (void)cplat_hashtable_add(ht, "a", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    grown = config;
    grown.capacity = 8;

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_calloc(_, _))
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_calloc が 1 回呼び出されること。
                                    // [Pre-Assert手順] - cplat_calloc から NULL を返却する。

    // Act_2
    int actual_ret_resize = cplat_hashtable_resize(ht, &grown); // [手順] - 確保に失敗する状態で拡大する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY, actual_ret_resize); // [確認_異常系] - 確保失敗が OUT_OF_MEMORY であること。
    EXPECT_EQ(CPLAT_OK, cplat_hashtable_count(ht, &in_use));
    // [確認_正常系] - `cplat_hashtable_count(ht, &in_use)` の戻り値が `CPLAT_OK` であること。
    EXPECT_EQ(1u, in_use); // [確認_正常系] - 失敗してもテーブルが変わらないこと。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// cplat_hashtable_rebuild_into に不正な設定や互換性のない設定を渡した場合のエラー返却の確認
TEST_F(hashtableResizeTest, rebuild_into_rejects_invalid_and_incompatible_config)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *src = nullptr;
    cplat_hashtable *dst = reinterpret_cast<cplat_hashtable *>(1);
    std::vector<uint64_t> mgmt_buf;
    std::vector<uint64_t> data_buf;
    size_t mgmt_size = 0;
    size_t data_size = 0;

    fill_config(&config, 4, 5);
    (void)cplat_hashtable_required_size(&config, &mgmt_size, &data_size);
    mgmt_buf.assign((mgmt_size / sizeof(uint64_t)) + 1u, 0);
    data_buf.assign((data_size / sizeof(uint64_t)) + 1u, 0);

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &src);
    cplat_hashtable_config invalid = config;
    invalid.capacity = 0; // [状態] - 設定として不正な値を用意する。
    int actual_ret_invalid =
        cplat_hashtable_rebuild_into(src, &invalid, mgmt_buf.data(), mgmt_size, data_buf.data(), data_size, &dst);
    cplat_hashtable_config incompatible = config;
    incompatible.lifetime = 9; // [状態] - 変えてはならない項目を変えた設定を用意する。
    int actual_ret_incompatible =
        cplat_hashtable_rebuild_into(src, &incompatible, mgmt_buf.data(), mgmt_size, data_buf.data(), data_size, &dst);
    int actual_ret_too_small = cplat_hashtable_rebuild_into(src, &config, mgmt_buf.data(), 1, data_buf.data(),
                                                            data_size, &dst); // [手順] - 管理領域を小さく渡す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_invalid); // [確認_異常系] - 設定として不正な値が失敗すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_incompatible); // [確認_異常系] - 変えてはならない項目の変更が失敗すること。
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              actual_ret_too_small); // [確認_異常系] - 領域が足りないと BUFFER_TOO_SMALL であること。
    EXPECT_EQ(nullptr, dst);         // [確認_異常系] - 失敗時に出力先が NULL になること。

    // Cleanup
    cplat_hashtable_dispose(src);
}

// 移行先領域のメモリ確保失敗時に OUT_OF_MEMORY が返り状態が維持されることの確認
TEST_F(hashtableResizeTest, resize_reports_out_of_memory_when_new_region_allocation_fails)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable_config grown = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    size_t in_use = 0;

    fill_config(&config, 4, 5);
    fill_value(&value, "v");

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    (void)cplat_hashtable_add(ht, "a", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    grown = config;
    grown.capacity = 8;

    // Pre-Assert
    /* 1 回目は取捨マスクの確保、2 回目は移行先の領域の確保。後者を失敗させる。 */
    EXPECT_CALL(mock_cplat_, cplat_calloc(_, _))
        .WillOnce(DoDefault())
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_calloc が 2 回呼び出されること。
                                    // [Pre-Assert手順] - 2 回目の cplat_calloc から NULL を返却する。

    // Act_2
    int actual_ret_resize = cplat_hashtable_resize(ht, &grown); // [手順] - 移行先の確保に失敗する状態で拡大する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY, actual_ret_resize); // [確認_異常系] - 確保失敗が OUT_OF_MEMORY であること。
    EXPECT_EQ(CPLAT_OK, cplat_hashtable_count(ht, &in_use));
    // [確認_正常系] - `cplat_hashtable_count(ht, &in_use)` の戻り値が `CPLAT_OK` であること。
    EXPECT_EQ(1u, in_use); // [確認_正常系] - 失敗してもテーブルが変わらないこと。

    // Cleanup
    cplat_hashtable_dispose(ht);
}
