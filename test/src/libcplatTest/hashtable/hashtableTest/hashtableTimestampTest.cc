#include <testfw.h>

#include <cplat/base/result.h>
#include <cplat/hashtable/hashtable.h>
#include <mock_cplat.h>

#include <cstring>
#include <string>
#include <vector>

namespace
{

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

void set_timestamp(cplat_timespec *ts, time_t sec, int64_t nsec)
{
    ts->tv_sec = sec;
    ts->tv_nsec = nsec;
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

class hashtableTimestampTest : public Test
{
  protected:
    NiceMock<Mock_cplat> mock_cplat_;
    time_t next_sec_ = 1000;

    void SetUp() override
    {
        ON_CALL(mock_cplat_, cplat_clock_get_realtime(_))
            .WillByDefault(
                [this](cplat_timespec *ts)
                {
                    set_timestamp(ts, next_sec_, 0);
                    next_sec_ += 10;
                }); // [状態] - cplat_clock_get_realtime が呼び出された際に 10 秒ずつ進む時刻を返すようにモックを設定する。
    }
};

// add/update/delete 操作時に実時刻が正しくタイムスタンプに記録されることの確認
TEST_F(hashtableTimestampTest, add_update_delete_stamp_realtime)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    cplat_timespec added = {};
    cplat_timespec added_copy = {};
    cplat_timespec walked = {};
    cplat_timespec updated = {};
    cplat_timespec deleted = {};
    const cplat_timespec *added_ref = nullptr;
    const char *peer = nullptr;

    fill_config(&config, 4, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - lifetime 5 の設定を用意する。
    fill_value(&value, "v1");
    peer = find_colliding_key("a", 4); // [状態] - "a" と同じバケットの別キーを探す。

    // Pre-Assert
    ASSERT_NE(nullptr, peer); // [Pre-Assert確認_正常系] - 同一バケットの別キーが見つかること。

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    int actual_ret_add = cplat_hashtable_add(ht, "a", value.data(),
                                                 CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - キーを追加する。
    int actual_ret_add_time = cplat_hashtable_get_timestamp_val(ht, 1, &added); // [手順] - 追加後の時刻を読む。
    cplat_timespec table_added = {};
    int actual_ret_table_add =
        cplat_hashtable_get_table_timestamp_val(ht, &table_added); // [手順] - 追加後のテーブル時刻を読む。
    int actual_ret_find_ref =
        cplat_hashtable_find_timestamp_ref(ht, "a", &added_ref); // [手順] - キーで時刻参照を取る。
    int actual_ret_find_val =
        cplat_hashtable_find_timestamp_val(ht, "a", &added_copy); // [手順] - キーで時刻を複製する。
    time_t added_ref_sec = (added_ref == nullptr) ? 0 : added_ref->tv_sec;
    (void)cplat_hashtable_add(
        ht, peer, value.data(),
        CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 同一バケットへ別キーを追加してチェイン先頭をずらす。
    int actual_ret_walk =
        cplat_hashtable_find_timestamp_val(ht, "a", &walked); // [手順] - チェインを辿って時刻を読む。
    fill_value(&value, "v2");
    int actual_ret_update = cplat_hashtable_update(ht, "a", value.data());           // [手順] - 値を更新する。
    int actual_ret_update_time = cplat_hashtable_get_timestamp_val(ht, 1, &updated); // [手順] - 更新後の時刻を読む。
    cplat_timespec table_updated = {};
    int actual_ret_table_update = cplat_hashtable_get_table_timestamp_val(ht, &table_updated);
    int actual_ret_delete = cplat_hashtable_delete(ht, "a"); // [手順] - キーを削除する。
    int actual_ret_deleted_time =
        cplat_hashtable_get_timestamp_val(ht, 1, &deleted); // [手順] - 削除後の時刻を読む。
    cplat_timespec table_deleted = {};
    int actual_ret_table_delete = cplat_hashtable_get_table_timestamp_val(ht, &table_deleted);
    int actual_ret_find_deleted =
        cplat_hashtable_find_timestamp_val(ht, "a", &added); // [手順] - 削除済みキーで検索する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_add);          // [確認_正常系] - add が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_add_time);     // [確認_正常系] - 追加後に時刻を読めること。
    EXPECT_EQ(1000, added.tv_sec);                   // [確認_正常系] - 追加時刻が最初の実時刻であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_table_add);    // [確認_正常系] - 追加後にテーブル時刻を読めること。
    EXPECT_EQ(1000, table_added.tv_sec);             // [確認_正常系] - テーブル時刻が追加時刻と一致すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_find_ref);     // [確認_正常系] - find_timestamp_ref が成功すること。
    EXPECT_EQ(1000, added_ref_sec);                  // [確認_正常系] - 参照の秒が追加時刻と一致すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_find_val);     // [確認_正常系] - find_timestamp_val が成功すること。
    EXPECT_EQ(1000, added_copy.tv_sec);              // [確認_正常系] - 複製の秒が追加時刻と一致すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_walk);         // [確認_正常系] - チェインを辿って時刻を読めること。
    EXPECT_EQ(1000, walked.tv_sec);                  // [確認_正常系] - チェイン走査後も追加時刻が保たれること。
    EXPECT_EQ(CPLAT_OK, actual_ret_update);       // [確認_正常系] - update が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_update_time);  // [確認_正常系] - 更新後に時刻を読めること。
    EXPECT_EQ(1020, updated.tv_sec);                 // [確認_正常系] - 更新で時刻が進むこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_table_update); // [確認_正常系] - 更新後にテーブル時刻を読めること。
    EXPECT_EQ(1020, table_updated.tv_sec);           // [確認_正常系] - テーブル時刻が更新時刻と一致すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_delete);       // [確認_正常系] - delete が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_deleted_time); // [確認_正常系] - 削除済みでも時刻を読めること。
    EXPECT_EQ(1030, deleted.tv_sec);                 // [確認_正常系] - 削除時刻が更新されていること。
    EXPECT_EQ(CPLAT_OK, actual_ret_table_delete); // [確認_正常系] - 削除後にテーブル時刻を読めること。
    EXPECT_EQ(1030, table_deleted.tv_sec);           // [確認_正常系] - テーブル時刻が削除時刻と一致すること。
    EXPECT_EQ(CPLAT_ERR_NOT_FOUND,
              actual_ret_find_deleted); // [確認_異常系] - 削除済みキーの find_timestamp が NOT_FOUND であること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// CPLAT_HASHTABLE_ADD_DELETED_REVIVE による復活時に既存の値が維持されタイムスタンプが更新されることの確認
TEST_F(hashtableTimestampTest, add_revive_keeps_previous_value_and_stamps)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    const void *found = nullptr;
    std::string found_text;
    cplat_timespec before_revive = {};
    cplat_timespec after_revive = {};
    cplat_timespec table_before = {};
    cplat_timespec table_after = {};
    size_t in_use = 0;
    size_t deleted = 0;

    fill_config(&config, 2, 8, 8, 5,
                CPLAT_HASHTABLE_KEY_STRING); // [状態] - lifetime 5 の設定を用意する(削除済みが残る)。
    fill_value(&value, "old");

    // Pre-Assert

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    (void)cplat_hashtable_add(ht, "a", value.data(),
                                 CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 元の値で追加する。
    (void)cplat_hashtable_delete(ht, "a"); // [手順] - 削除する(lifetime 5 のため削除済みのまま残る)。
    (void)cplat_hashtable_get_timestamp_val(ht, 1, &before_revive);   // [手順] - 削除直後のレコード時刻を保存する。
    (void)cplat_hashtable_get_table_timestamp_val(ht, &table_before); // [手順] - 削除直後のテーブル時刻を保存する。
    fill_value(&value, "new"); // [状態] - REVIVE では無視されるはずの新しい値を用意する。
    int actual_ret_revive = cplat_hashtable_add(
        ht, "a", value.data(), CPLAT_HASHTABLE_ADD_DELETED_REVIVE);        // [手順] - REVIVE で復活させる。
    int actual_ret_find = cplat_hashtable_find_value_ref(ht, "a", &found); // [手順] - 復活後の値を取得する。
    found_text = (found == nullptr) ? "" : static_cast<const char *>(found);
    (void)cplat_hashtable_get_timestamp_val(ht, 1, &after_revive);   // [手順] - 復活後のレコード時刻を取得する。
    (void)cplat_hashtable_get_table_timestamp_val(ht, &table_after); // [手順] - 復活後のテーブル時刻を取得する。
    int actual_ret_counts = cplat_hashtable_count_status(ht, &in_use, &deleted, NULL); // [手順] - 件数を取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_revive); // [確認_正常系] - REVIVE での add が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_find);   // [確認_正常系] - 復活後に検索できること。
    EXPECT_EQ("old", found_text); // [確認_正常系] - 復活した値が削除前のままであること(new は無視されること)。
    EXPECT_LT(before_revive.tv_sec, after_revive.tv_sec); // [確認_正常系] - REVIVE でもレコード時刻が進むこと。
    EXPECT_LT(table_before.tv_sec, table_after.tv_sec);   // [確認_正常系] - REVIVE でもテーブル時刻が進むこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_counts);            // [確認_正常系] - count_status が成功すること。
    EXPECT_EQ(1u, in_use);                                // [確認_正常系] - 復活により実装中が 1 件に戻ること。
    EXPECT_EQ(0u, deleted);                               // [確認_正常系] - 復活により削除済みが 0 件に戻ること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// cplat_hashtable_push_deleted による削除済みレコード加齢ではタイムスタンプが更新されないことの確認
TEST_F(hashtableTimestampTest, push_deleted_does_not_stamp)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    cplat_timespec before = {};
    cplat_timespec after = {};

    fill_config(&config, 2, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - lifetime 5 の設定を用意する。
    fill_value(&value, "keep");

    // Pre-Assert

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    (void)cplat_hashtable_add(ht, "a", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_delete(ht, "a");
    (void)cplat_hashtable_get_timestamp_val(ht, 1, &before);                 // [手順] - 加齢前の時刻を保存する。
    int actual_ret_push = cplat_hashtable_push_deleted(ht);                  // [手順] - 削除済みを 1 段階加齢する。
    int actual_ret_after = cplat_hashtable_get_timestamp_val(ht, 1, &after); // [手順] - 加齢後の時刻を読む。
    int actual_ret_status = 0;
    int status = -1;
    actual_ret_status = cplat_hashtable_get_status(ht, 1, &status);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_push);  // [確認_正常系] - push_deleted が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_after); // [確認_正常系] - 加齢後も時刻を読めること。
    EXPECT_EQ(before.tv_sec, after.tv_sec);   // [確認_正常系] - 加齢では時刻が変わらないこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_status);
    // [確認_正常系] - `cplat_hashtable_get_status(ht, 1, &status)` の戻り値が `CPLAT_OK` であること。
    EXPECT_EQ(3, status); // [確認_正常系] - 加齢で status が 3 になること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// 空スロットおよび完全削除・パージ後のスロットのタイムスタンプ取得が NOT_FOUND を返すことの確認
TEST_F(hashtableTimestampTest, empty_slot_time_is_not_found_and_zeroed)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    cplat_timespec ts = {};
    ts.tv_sec = 9;
    cplat_timespec insert_timestamp = {};
    set_timestamp(&insert_timestamp, 50, 0);

    fill_config(&config, 2, 8, 8, 2, CPLAT_HASHTABLE_KEY_STRING); // [状態] - 削除で直ちに空へ戻る設定を用意する。
    fill_value(&value, "gone");

    // Pre-Assert

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    int actual_ret_empty = cplat_hashtable_get_timestamp_val(ht, 1, &ts); // [手順] - 空スロットの時刻を読む。
    (void)cplat_hashtable_add(ht, "a", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    int actual_ret_delete = cplat_hashtable_delete(ht, "a"); // [手順] - lifetime 2 で直ちに空へ戻す。
    int actual_ret_after =
        cplat_hashtable_get_timestamp_val(ht, 1, &ts); // [手順] - 空へ戻したスロットの時刻を読む。
    int actual_ret_insert = cplat_hashtable_insert_direct(ht, 1, "b", 1, value.data(), &insert_timestamp, 1);
    int actual_ret_direct_time = cplat_hashtable_get_timestamp_val(ht, 1, &ts);
    (void)cplat_hashtable_delete(ht, "b");
    (void)cplat_hashtable_insert_direct(ht, 2, "c", 3, value.data(), &insert_timestamp, 2);
    int actual_ret_purge = cplat_hashtable_purge_deleted(ht); // [手順] - 削除済みを空へ戻す。
    int actual_ret_purged = cplat_hashtable_get_timestamp_val(ht, 2, &ts);

    // Assert
    EXPECT_EQ(CPLAT_ERR_NOT_FOUND, actual_ret_empty);  // [確認_異常系] - 空の get_time が NOT_FOUND であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_delete);            // [確認_正常系] - 直ちに空へ戻る削除が成功すること。
    EXPECT_EQ(CPLAT_ERR_NOT_FOUND, actual_ret_after);  // [確認_異常系] - 空へ戻したスロットの時刻が無効であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_insert);            // [確認_正常系] - insert_direct が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_direct_time);       // [確認_正常系] - 指定時刻を読めること。
    EXPECT_EQ(50, ts.tv_sec);                             // [確認_正常系] - insert_direct が渡した時刻を保持すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_purge);             // [確認_正常系] - purge_deleted が成功すること。
    EXPECT_EQ(CPLAT_ERR_NOT_FOUND, actual_ret_purged); // [確認_異常系] - 回収後の時刻が無効であること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// タイムスタンプ取得 API 群に不正な引数や範囲外キーを渡した場合のエラー返却の確認
TEST_F(hashtableTimestampTest, accessors_reject_invalid_arguments)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    cplat_timespec ts = {};
    const cplat_timespec *ref = nullptr;
    std::vector<unsigned char> value(8, 0);
    char too_long[16];

    fill_config(&config, 2, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - 妥当な設定を用意する。
    fill_value(&value, "a");
    std::memset(too_long, 'x', sizeof(too_long)); // [状態] - NUL が無いキーを用意する。

    // Pre-Assert

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    (void)cplat_hashtable_add(ht, "a", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    int actual_ret_get_null_ht = cplat_hashtable_get_timestamp_ref(NULL, 1, &ref);
    int actual_ret_get_null_out = cplat_hashtable_get_timestamp_ref(ht, 1, NULL);
    int actual_ret_get_rec0 = cplat_hashtable_get_timestamp_val(ht, 0, &ts);
    int actual_ret_get_rec_hi = cplat_hashtable_get_timestamp_val(ht, 3, &ts);
    int actual_ret_get_val_null = cplat_hashtable_get_timestamp_val(ht, 1, NULL);
    int actual_ret_find_null_ht = cplat_hashtable_find_timestamp_ref(NULL, "a", &ref);
    int actual_ret_find_null_key = cplat_hashtable_find_timestamp_ref(ht, NULL, &ref);
    int actual_ret_find_null_out = cplat_hashtable_find_timestamp_ref(ht, "a", NULL);
    int actual_ret_find_long = cplat_hashtable_find_timestamp_val(ht, too_long, &ts);
    int actual_ret_find_val_null = cplat_hashtable_find_timestamp_val(ht, "a", NULL);
    int actual_ret_find_missing = cplat_hashtable_find_timestamp_val(ht, "b", &ts);

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_get_null_ht); // [確認_異常系] - NULL ht が INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_get_null_out); // [確認_異常系] - NULL 出力が INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_get_rec0); // [確認_異常系] - レコード番号 0 が INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_get_rec_hi); // [確認_異常系] - 範囲外レコードが INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_get_val_null); // [確認_異常系] - NULL 複製先が INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_find_null_ht); // [確認_異常系] - find の NULL ht が INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_find_null_key); // [確認_異常系] - find の NULL key が INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_find_null_out); // [確認_異常系] - find の NULL 出力が INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_OUT_OF_RANGE,
              actual_ret_find_long); // [確認_異常系] - 長すぎるキーが OUT_OF_RANGE であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_find_val_null); // [確認_異常系] - find_val の NULL が INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_NOT_FOUND, actual_ret_find_missing); // [確認_異常系] - 無いキーが NOT_FOUND であること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// テーブル全体のタイムスタンプがコンテンツの変更操作や直接挿入に追従して更新されることの確認
TEST_F(hashtableTimestampTest, table_timestamp_tracks_content_changes)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    cplat_timespec table = {};
    cplat_timespec older = {};
    cplat_timespec newer = {};
    const cplat_timespec *table_ref = nullptr;

    fill_config(&config, 2, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - lifetime 5 の設定を用意する。
    fill_value(&value, "v");
    set_timestamp(&older, 50, 0);
    set_timestamp(&newer, 2000, 0);

    // Pre-Assert

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    int actual_ret_create =
        cplat_hashtable_get_table_timestamp_val(ht, &table); // [手順] - 構築直後のテーブル時刻を読む。
    time_t created_sec = table.tv_sec;
    (void)cplat_hashtable_add(ht, "a", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_delete(ht, "a");
    (void)cplat_hashtable_get_table_timestamp_val(ht, &table); // [手順] - 削除後のテーブル時刻を保存する。
    time_t after_delete = table.tv_sec;
    int actual_ret_push = cplat_hashtable_push_deleted(ht); // [手順] - 削除済みを加齢する。
    int actual_ret_after_push = cplat_hashtable_get_table_timestamp_val(ht, &table);
    time_t after_push = table.tv_sec;
    int actual_ret_purge = cplat_hashtable_purge_deleted(ht); // [手順] - 削除済みを空へ戻す。
    int actual_ret_after_purge = cplat_hashtable_get_table_timestamp_val(ht, &table);
    time_t after_purge = table.tv_sec;
    int actual_ret_old = cplat_hashtable_insert_direct(ht, 1, "b", 1, value.data(), &older, 1);
    int actual_ret_after_old = cplat_hashtable_get_table_timestamp_val(ht, &table);
    time_t after_old = table.tv_sec;
    int actual_ret_new = cplat_hashtable_insert_direct(ht, 2, "c", 1, value.data(), &newer, 2);
    int actual_ret_after_new = cplat_hashtable_get_table_timestamp_val(ht, &table);
    time_t after_new = table.tv_sec;
    int actual_ret_ref = cplat_hashtable_get_table_timestamp_ref(ht, &table_ref);
    time_t ref_sec = (table_ref == nullptr) ? 0 : table_ref->tv_sec;
    int actual_ret_clear = cplat_hashtable_clear(ht); // [手順] - テーブルを空にする。
    int actual_ret_after_clear = cplat_hashtable_get_table_timestamp_val(ht, &table);
    int actual_ret_null_ht = cplat_hashtable_get_table_timestamp_ref(NULL, &table_ref);
    int actual_ret_null_out = cplat_hashtable_get_table_timestamp_ref(ht, NULL);
    int actual_ret_val_null = cplat_hashtable_get_table_timestamp_val(ht, NULL);
    int actual_ret_val_null_ht = cplat_hashtable_get_table_timestamp_val(NULL, &table);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_create);      // [確認_正常系] - 構築直後でもテーブル時刻を読めること。
    EXPECT_EQ(0, created_sec);                      // [確認_正常系] - 構築直後のテーブル時刻が 0 であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_push);        // [確認_正常系] - push_deleted が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_after_push);  // [確認_正常系] - 加齢後もテーブル時刻を読めること。
    EXPECT_EQ(after_delete, after_push);            // [確認_正常系] - 加齢ではテーブル時刻が変わらないこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_purge);       // [確認_正常系] - purge_deleted が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_after_purge); // [確認_正常系] - 回収後もテーブル時刻を読めること。
    EXPECT_EQ(after_delete, after_purge);           // [確認_正常系] - 回収ではテーブル時刻が変わらないこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_old);         // [確認_正常系] - 古い時刻の insert_direct が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_after_old);   // [確認_正常系] - 古い時刻の書き込み後もテーブル時刻を読めること。
    EXPECT_EQ(after_delete, after_old);             // [確認_正常系] - 古い時刻ではテーブル時刻が進まないこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_new);         // [確認_正常系] - 新しい時刻の insert_direct が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_after_new); // [確認_正常系] - 新しい時刻の書き込み後にテーブル時刻を読めること。
    EXPECT_EQ(2000, after_new);                   // [確認_正常系] - 新しい時刻でテーブル時刻が進むこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_ref);       // [確認_正常系] - get_table_timestamp_ref が成功すること。
    EXPECT_EQ(2000, ref_sec);                     // [確認_正常系] - 参照の秒がテーブル時刻と一致すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_clear);     // [確認_正常系] - clear が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_after_clear); // [確認_正常系] - clear 後にテーブル時刻を読めること。
    EXPECT_EQ(1020, table.tv_sec);                  // [確認_正常系] - clear でテーブル時刻が進むこと。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_null_ht); // [確認_異常系] - NULL ht が INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_null_out); // [確認_異常系] - NULL 出力が INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_val_null); // [確認_異常系] - NULL 複製先が INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_val_null_ht); // [確認_異常系] - val の NULL ht が INVALID_ARGUMENT であること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// テーブル粒度設定時にレコード単位のタイムスタンプ API が UNSUPPORTED を返すことの確認
TEST_F(hashtableTimestampTest, scope_table_record_timestamp_apis_are_unsupported)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable_config read_config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    cplat_timespec ts = {};
    cplat_timespec table = {};
    const cplat_timespec *ref = nullptr;
    const cplat_timespec *table_ref = nullptr;

    fill_config(&config, 2, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING);
    config.timestamp_scope = CPLAT_HASHTABLE_TIMESTAMP_SCOPE_TABLE; // [状態] - テーブル横断のみの粒度にする。
    fill_value(&value, "v");

    // Pre-Assert

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    (void)cplat_hashtable_add(ht, "a", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    int actual_ret_get_ref = cplat_hashtable_get_timestamp_ref(ht, 1, &ref);
    int actual_ret_get_val = cplat_hashtable_get_timestamp_val(ht, 1, &ts);
    int actual_ret_find_ref = cplat_hashtable_find_timestamp_ref(ht, "a", &ref);
    int actual_ret_find_val = cplat_hashtable_find_timestamp_val(ht, "a", &ts);
    int actual_ret_get_null_ht = cplat_hashtable_get_timestamp_ref(NULL, 1, &ref);
    int actual_ret_get_null_out = cplat_hashtable_get_timestamp_ref(ht, 1, NULL);
    int actual_ret_get_val_null = cplat_hashtable_get_timestamp_val(ht, 1, NULL);
    int actual_ret_find_null_ht = cplat_hashtable_find_timestamp_ref(NULL, "a", &ref);
    int actual_ret_find_null_key = cplat_hashtable_find_timestamp_ref(ht, NULL, &ref);
    int actual_ret_find_null_out = cplat_hashtable_find_timestamp_ref(ht, "a", NULL);
    int actual_ret_find_val_null = cplat_hashtable_find_timestamp_val(ht, "a", NULL);
    int actual_ret_table_ref = cplat_hashtable_get_table_timestamp_ref(ht, &table_ref);
    int actual_ret_table_val = cplat_hashtable_get_table_timestamp_val(ht, &table);
    int actual_ret_config = cplat_hashtable_get_config_val(ht, &read_config);

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNSUPPORTED,
              actual_ret_get_ref); // [確認_異常系] - SCOPE_TABLE の get_timestamp_ref が UNSUPPORTED であること。
    EXPECT_EQ(CPLAT_ERR_UNSUPPORTED,
              actual_ret_get_val); // [確認_異常系] - SCOPE_TABLE の get_timestamp_val が UNSUPPORTED であること。
    EXPECT_EQ(CPLAT_ERR_UNSUPPORTED,
              actual_ret_find_ref); // [確認_異常系] - SCOPE_TABLE の find_timestamp_ref が UNSUPPORTED であること。
    EXPECT_EQ(CPLAT_ERR_UNSUPPORTED,
              actual_ret_find_val); // [確認_異常系] - SCOPE_TABLE の find_timestamp_val が UNSUPPORTED であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_get_null_ht); // [確認_異常系] - NULL ht が UNSUPPORTED より INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_get_null_out); // [確認_異常系] - NULL 出力が UNSUPPORTED より INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_get_val_null); // [確認_異常系] - NULL 複製先が UNSUPPORTED より INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_find_null_ht); // [確認_異常系] - find の NULL ht が INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_find_null_key); // [確認_異常系] - find の NULL key が INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_find_null_out); // [確認_異常系] - find の NULL 出力が INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_find_val_null);          // [確認_異常系] - find_val の NULL が INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_table_ref); // [確認_正常系] - SCOPE_TABLE でもテーブル時刻参照が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_table_val); // [確認_正常系] - SCOPE_TABLE でもテーブル時刻複製が成功すること。
    EXPECT_EQ(1000, table.tv_sec);                // [確認_正常系] - add でテーブル時刻が進むこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_config);    // [確認_正常系] - get_config_val が成功すること。
    EXPECT_EQ(CPLAT_HASHTABLE_TIMESTAMP_SCOPE_TABLE,
              read_config.timestamp_scope); // [確認_正常系] - 構築時の粒度が設定へ残ること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// テーブル粒度設定における insert_direct の引数制約およびテーブルタイムスタンプの挙動の確認
TEST_F(hashtableTimestampTest, scope_table_insert_direct_and_table_timestamp)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    cplat_timespec supplied = {};
    cplat_timespec table = {};

    fill_config(&config, 4, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING);
    config.timestamp_scope = CPLAT_HASHTABLE_TIMESTAMP_SCOPE_TABLE; // [状態] - テーブル横断のみの粒度にする。
    fill_value(&value, "v");
    set_timestamp(&supplied, 50, 0);

    // Pre-Assert

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    int actual_ret_add = cplat_hashtable_add(ht, "a", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_get_table_timestamp_val(ht, &table);
    time_t after_add = table.tv_sec;
    int actual_ret_update = cplat_hashtable_update(ht, "a", value.data());
    (void)cplat_hashtable_get_table_timestamp_val(ht, &table);
    time_t after_update = table.tv_sec;
    int actual_ret_delete = cplat_hashtable_delete(ht, "a");
    (void)cplat_hashtable_get_table_timestamp_val(ht, &table);
    time_t after_delete = table.tv_sec;
    int actual_ret_nonnull = cplat_hashtable_insert_direct(ht, 2, "b", 1, value.data(), &supplied,
                                                              0); // [手順] - SCOPE_TABLE で時刻を渡す。
    int actual_ret_null = cplat_hashtable_insert_direct(ht, 2, "b", 1, value.data(), NULL,
                                                           0); // [手順] - SCOPE_TABLE で時刻を省略する。
    int actual_ret_after_direct = cplat_hashtable_get_table_timestamp_val(ht, &table);
    time_t after_direct = table.tv_sec;
    int actual_ret_clear = cplat_hashtable_clear(ht);
    int actual_ret_after_clear = cplat_hashtable_get_table_timestamp_val(ht, &table);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_add);    // [確認_正常系] - add が成功すること。
    EXPECT_EQ(1000, after_add);                // [確認_正常系] - add でテーブル時刻が進むこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_update); // [確認_正常系] - update が成功すること。
    EXPECT_EQ(1010, after_update);             // [確認_正常系] - update でテーブル時刻が進むこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_delete); // [確認_正常系] - delete が成功すること。
    EXPECT_EQ(1020, after_delete);             // [確認_正常系] - delete でテーブル時刻が進むこと。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_nonnull);           // [確認_異常系] - SCOPE_TABLE で時刻を渡すと INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_null); // [確認_正常系] - SCOPE_TABLE で時刻省略の insert_direct が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_after_direct); // [確認_正常系] - insert_direct 後もテーブル時刻を読めること。
    EXPECT_EQ(after_delete,
              after_direct); // [確認_正常系] - SCOPE_TABLE の insert_direct ではテーブル時刻が進まないこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_clear);       // [確認_正常系] - clear が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_after_clear); // [確認_正常系] - clear 後にテーブル時刻を読めること。
    EXPECT_EQ(1030, table.tv_sec);                  // [確認_正常系] - clear でテーブル時刻が進むこと。

    // Cleanup
    cplat_hashtable_dispose(ht);
}
