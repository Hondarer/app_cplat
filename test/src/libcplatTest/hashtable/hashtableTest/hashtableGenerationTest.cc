#include <testfw.h>

#include <cplat/base/result.h>
#include <cplat/hashtable/hashtable.h>
#include <mock_cplat.h>

#include <cstring>
#include <vector>

namespace
{

void fill_config(cplat_hashtable_config *config, size_t capacity, unsigned char lifetime,
                 cplat_hashtable_timestamp_scope scope)
{
    *config = {};
    config->capacity = capacity;
    config->key_type = CPLAT_HASHTABLE_FIELD_FIXED_STRING;
    config->value_type = CPLAT_HASHTABLE_FIELD_FIXED_BINARY;
    config->timestamp_scope = scope;
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

cplat_timespec make_timestamp(time_t sec)
{
    cplat_timespec ts = {};

    ts.tv_sec = sec;
    ts.tv_nsec = 0;
    return ts;
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

class hashtableGenerationTest : public Test
{
  protected:
    NiceMock<Mock_cplat> mock_cplat_;
};

// レコード粒度のハッシュテーブルで add/update/delete 操作により世代番号が進むことの確認
TEST_F(hashtableGenerationTest, add_update_delete_advance_generation)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    uint64_t created = 0;
    uint64_t added_table = 0;
    uint64_t added_record = 0;
    uint64_t added_found = 0;
    uint64_t updated_table = 0;
    uint64_t updated_record = 0;
    uint64_t deleted_table = 0;
    uint64_t deleted_record = 0;
    uint64_t after_delete_found = 0;

    fill_config(&config, 4, 5, CPLAT_HASHTABLE_TIMESTAMP_SCOPE_RECORD); // [状態] - レコード粒度の設定を用意する。
    fill_value(&value, "v1");

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    int actual_ret_created =
        cplat_hashtable_get_table_generation(ht, &created); // [手順] - 構築直後のテーブル世代を読む。
    int actual_ret_add = cplat_hashtable_add(ht, "a", value.data(),
                                             CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - キーを追加する。
    int actual_ret_added_table =
        cplat_hashtable_get_table_generation(ht, &added_table); // [手順] - 追加後のテーブル世代を読む。
    int actual_ret_added_record =
        cplat_hashtable_get_generation(ht, 1, &added_record); // [手順] - 追加後のレコード世代を読む。
    int actual_ret_added_found =
        cplat_hashtable_find_generation(ht, "a", &added_found); // [手順] - キーでレコード世代を読む。
    fill_value(&value, "v2");
    int actual_ret_update = cplat_hashtable_update(ht, "a", value.data()); // [手順] - 値を更新する。
    int actual_ret_updated_table = cplat_hashtable_get_table_generation(ht, &updated_table);
    int actual_ret_updated_record = cplat_hashtable_get_generation(ht, 1, &updated_record);
    int actual_ret_delete = cplat_hashtable_delete(ht, "a"); // [手順] - キーを削除する。
    int actual_ret_deleted_table = cplat_hashtable_get_table_generation(ht, &deleted_table);
    int actual_ret_deleted_record =
        cplat_hashtable_get_generation(ht, 1, &deleted_record); // [手順] - 削除済みレコードの世代を読む。
    int actual_ret_after_delete_found =
        cplat_hashtable_find_generation(ht, "a", &after_delete_found); // [手順] - 削除済みキーで検索する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_created);       // [確認_正常系] - 構築直後にテーブル世代を読めること。
    EXPECT_EQ(0u, created);                        // [確認_正常系] - 構築直後のテーブル世代が 0 であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_add);           // [確認_正常系] - add が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_added_table);   // [確認_正常系] - 追加後にテーブル世代を読めること。
    EXPECT_EQ(1u, added_table);                    // [確認_正常系] - 追加でテーブル世代が 1 増えること。
    EXPECT_EQ(CPLAT_OK, actual_ret_added_record);  // [確認_正常系] - 追加後にレコード世代を読めること。
    EXPECT_EQ(added_table, added_record);          // [確認_正常系] - レコード世代が追加後のテーブル世代と一致すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_added_found);   // [確認_正常系] - キーでレコード世代を読めること。
    EXPECT_EQ(added_record, added_found);          // [確認_正常系] - キー経由と番号経由で同じ世代が得られること。
    EXPECT_EQ(CPLAT_OK, actual_ret_update);        // [確認_正常系] - update が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_updated_table); // [確認_正常系] - 更新後にテーブル世代を読めること。
    EXPECT_EQ(2u, updated_table);                  // [確認_正常系] - 更新でテーブル世代がさらに 1 増えること。
    EXPECT_EQ(CPLAT_OK, actual_ret_updated_record); // [確認_正常系] - 更新後にレコード世代を読めること。
    EXPECT_EQ(updated_table, updated_record);      // [確認_正常系] - レコード世代が更新後のテーブル世代と一致すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_delete);        // [確認_正常系] - delete が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_deleted_table); // [確認_正常系] - 削除後にテーブル世代を読めること。
    EXPECT_EQ(3u, deleted_table);                  // [確認_正常系] - 削除でテーブル世代がさらに 1 増えること。
    EXPECT_EQ(CPLAT_OK, actual_ret_deleted_record); // [確認_正常系] - 削除済みレコードの世代を読めること。
    EXPECT_EQ(deleted_table, deleted_record);       // [確認_正常系] - 削除済みレコードの世代が削除時の値であること。
    EXPECT_EQ(
        CPLAT_ERR_NOT_FOUND,
        actual_ret_after_delete_found); // [確認_異常系] - 削除済みキーの find_generation が NOT_FOUND であること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// 実時刻が逆行する場合でも世代番号が単調増加し続けることの確認
TEST_F(hashtableGenerationTest, generation_advances_while_realtime_goes_backward)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    time_t next_sec = 3000;
    cplat_timespec first_time = {};
    cplat_timespec second_time = {};
    uint64_t first_generation = 0;
    uint64_t second_generation = 0;

    fill_config(&config, 4, 5, CPLAT_HASHTABLE_TIMESTAMP_SCOPE_RECORD);
    fill_value(&value, "v1");
    ON_CALL(mock_cplat_, cplat_clock_get_realtime(_))
        .WillByDefault(
            [&next_sec](cplat_timespec *ts)
            {
                ts->tv_sec = next_sec;
                ts->tv_nsec = 0;
                next_sec -= 10;
            }); // [状態] - cplat_clock_get_realtime が呼び出されるたびに 10 秒ずつ戻る時刻を返すようにモックを設定する。

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    (void)cplat_hashtable_add(ht, "a", value.data(),
                              CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - キーを追加する。
    int actual_ret_first_time = cplat_hashtable_get_timestamp_val(ht, 1, &first_time);
    int actual_ret_first_generation = cplat_hashtable_get_generation(ht, 1, &first_generation);
    fill_value(&value, "v2");
    (void)cplat_hashtable_update(ht, "a", value.data()); // [手順] - 時計が戻った状態で値を更新する。
    int actual_ret_second_time = cplat_hashtable_get_timestamp_val(ht, 1, &second_time);
    int actual_ret_second_generation = cplat_hashtable_get_generation(ht, 1, &second_generation);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_first_time);        // [確認_正常系] - 追加後の時刻を読めること。
    EXPECT_EQ(CPLAT_OK, actual_ret_first_generation);  // [確認_正常系] - 追加後の世代を読めること。
    EXPECT_EQ(CPLAT_OK, actual_ret_second_time);       // [確認_正常系] - 更新後の時刻を読めること。
    EXPECT_EQ(CPLAT_OK, actual_ret_second_generation); // [確認_正常系] - 更新後の世代を読めること。
    EXPECT_LT(second_time.tv_sec, first_time.tv_sec);  // [確認_正常系] - 実時刻は時計の巻き戻しにより逆行すること。
    EXPECT_GT(second_generation, first_generation);    // [確認_正常系] - 世代カウンターは時計が戻っても増え続けること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// 削除済みレコード再利用時に実時刻ではなく世代番号が最小のレコードが選択されることの確認
TEST_F(hashtableGenerationTest, reuse_deleted_selects_smallest_generation)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    cplat_timespec old_time = make_timestamp(100);
    cplat_timespec new_time = make_timestamp(200);
    uint64_t reused_record = 0;

    fill_config(&config, 2, CPLAT_HASHTABLE_LIFETIME_INFINITE, CPLAT_HASHTABLE_TIMESTAMP_SCOPE_RECORD);
    config.reuse_deleted = 1; // [状態] - 空きが無いとき削除済みを再利用する設定にする。
    fill_value(&value, "v");

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    /* レコード 1 は時刻が古く世代が新しい。レコード 2 は時刻が新しく世代が古い。 */
    int actual_ret_first =
        cplat_hashtable_insert_direct(ht, 1, "a", 2, value.data(), &old_time,
                                      50); // [手順] - 時刻が古く世代が新しい削除済みレコードをレコード 1 へ置く。
    int actual_ret_second =
        cplat_hashtable_insert_direct(ht, 2, "b", 2, value.data(), &new_time,
                                      10); // [手順] - 時刻が新しく世代が古い削除済みレコードをレコード 2 へ置く。
    int actual_ret_add =
        cplat_hashtable_add(ht, "z", value.data(),
                            CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 空きが無い状態で新しいキーを追加する。
    int actual_ret_recno = cplat_hashtable_find_recno(ht, "z", &reused_record);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_first);  // [確認_正常系] - レコード 1 への直接書き込みが成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_second); // [確認_正常系] - レコード 2 への直接書き込みが成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_add);    // [確認_正常系] - 削除済みを追い出して追加できること。
    EXPECT_EQ(CPLAT_OK, actual_ret_recno);  // [確認_正常系] - 追加したキーのレコード番号を引けること。
    EXPECT_EQ(2u, reused_record);           // [確認_正常系] - 実時刻ではなく世代が最小のレコード 2 が再利用されること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// cplat_hashtable_insert_direct で渡された世代番号に応じてテーブル世代が正しく更新および維持されることの確認
TEST_F(hashtableGenerationTest, insert_direct_keeps_largest_generation)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    cplat_timespec timestamp = make_timestamp(500);
    uint64_t after_large = 0;
    uint64_t after_small = 0;
    uint64_t record_small = 0;

    fill_config(&config, 4, 5, CPLAT_HASHTABLE_TIMESTAMP_SCOPE_RECORD);
    fill_value(&value, "v");

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    int actual_ret_large = cplat_hashtable_insert_direct(ht, 1, "a", 1, value.data(), &timestamp,
                                                         40); // [手順] - 大きい世代でレコードを置く。
    int actual_ret_after_large = cplat_hashtable_get_table_generation(ht, &after_large);
    int actual_ret_small = cplat_hashtable_insert_direct(ht, 2, "b", 1, value.data(), &timestamp,
                                                         7); // [手順] - 小さい世代でレコードを置く。
    int actual_ret_after_small = cplat_hashtable_get_table_generation(ht, &after_small);
    int actual_ret_record_small = cplat_hashtable_get_generation(ht, 2, &record_small);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_large);        // [確認_正常系] - 大きい世代での直接書き込みが成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_after_large);  // [確認_正常系] - テーブル世代を読めること。
    EXPECT_EQ(40u, after_large);                  // [確認_正常系] - テーブル世代が渡した値まで進むこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_small);        // [確認_正常系] - 小さい世代での直接書き込みが成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_after_small);  // [確認_正常系] - テーブル世代を読めること。
    EXPECT_EQ(40u, after_small);                  // [確認_正常系] - 小さい世代を渡してもテーブル世代が戻らないこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_record_small); // [確認_正常系] - レコード世代を読めること。
    EXPECT_EQ(7u, record_small);                  // [確認_正常系] - レコードには渡した値がそのまま書かれること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// テーブル粒度設定時にレコード世代関連 API が UNSUPPORTED または INVALID_ARGUMENT を返すことの確認
TEST_F(hashtableGenerationTest, scope_table_has_no_record_generation)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    cplat_timespec timestamp = make_timestamp(500);
    uint64_t table_generation = 0;
    uint64_t record_generation = 0;
    uint64_t found_generation = 0;

    fill_config(&config, 4, 5, CPLAT_HASHTABLE_TIMESTAMP_SCOPE_TABLE); // [状態] - テーブル粒度の設定を用意する。
    fill_value(&value, "v");

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    (void)cplat_hashtable_add(ht, "a", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    int actual_ret_table = cplat_hashtable_get_table_generation(ht, &table_generation);
    int actual_ret_record = cplat_hashtable_get_generation(ht, 1, &record_generation);
    int actual_ret_found = cplat_hashtable_find_generation(ht, "a", &found_generation);
    int actual_ret_direct_generation = cplat_hashtable_insert_direct(ht, 2, "b", 1, value.data(), NULL,
                                                                     3); // [手順] - テーブル粒度で 0 以外の世代を渡す。
    int actual_ret_direct_timestamp = cplat_hashtable_insert_direct(
        ht, 2, "b", 1, value.data(), &timestamp, 0); // [手順] - テーブル粒度で NULL 以外の時刻を渡す。
    int actual_ret_direct_ok =
        cplat_hashtable_insert_direct(ht, 2, "b", 1, value.data(), NULL, 0); // [手順] - どちらも省いて置く。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_table); // [確認_正常系] - テーブル粒度でもテーブル世代を読めること。
    EXPECT_EQ(1u, table_generation);       // [確認_正常系] - テーブル粒度でもテーブル世代が進むこと。
    EXPECT_EQ(CPLAT_ERR_UNSUPPORTED,
              actual_ret_record); // [確認_異常系] - テーブル粒度では get_generation が UNSUPPORTED であること。
    EXPECT_EQ(CPLAT_ERR_UNSUPPORTED,
              actual_ret_found); // [確認_異常系] - テーブル粒度では find_generation が UNSUPPORTED であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_direct_generation); // [確認_異常系] - テーブル粒度で 0 以外の世代は INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_direct_timestamp); // [確認_異常系] - テーブル粒度で NULL 以外の時刻は INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_direct_ok); // [確認_正常系] - どちらも省けば直接書き込みが成功すること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// cplat_hashtable_find_generation が衝突チェインを走査して正しくレコード世代を取得できることの確認
TEST_F(hashtableGenerationTest, find_generation_walks_chain)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    const char *peer = nullptr;
    uint64_t first_generation = 0;
    uint64_t walked_generation = 0;
    uint64_t missing_generation = 0;

    fill_config(&config, 4, 5, CPLAT_HASHTABLE_TIMESTAMP_SCOPE_RECORD);
    fill_value(&value, "v");
    peer = find_colliding_key("a", 4); // [状態] - "a" と同じバケットの別キーを探す。

    // Pre-Assert
    ASSERT_NE(nullptr, peer); // [Pre-Assert確認_正常系] - 同一バケットの別キーが見つかること。

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    (void)cplat_hashtable_add(ht, "a", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    int actual_ret_first = cplat_hashtable_find_generation(ht, "a", &first_generation);
    (void)cplat_hashtable_add(
        ht, peer, value.data(),
        CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 同一バケットへ別キーを追加してチェイン先頭をずらす。
    int actual_ret_walked =
        cplat_hashtable_find_generation(ht, "a", &walked_generation); // [手順] - チェインを辿って世代を読む。
    int actual_ret_missing =
        cplat_hashtable_find_generation(ht, "zz", &missing_generation); // [手順] - 未登録のキーで検索する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_first);          // [確認_正常系] - チェイン先頭のキーで世代を読めること。
    EXPECT_EQ(CPLAT_OK, actual_ret_walked);         // [確認_正常系] - チェインを辿ってもキーで世代を読めること。
    EXPECT_EQ(first_generation, walked_generation); // [確認_正常系] - 辿っても同じ世代が得られること。
    EXPECT_EQ(CPLAT_ERR_NOT_FOUND,
              actual_ret_missing); // [確認_異常系] - 未登録のキーの find_generation が NOT_FOUND であること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// cplat_hashtable_purge_deleted および clear 実行時の世代番号の挙動の確認
TEST_F(hashtableGenerationTest, clear_and_purge_handle_generation)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    uint64_t before_purge = 0;
    uint64_t after_purge = 0;
    uint64_t after_clear = 0;
    uint64_t cleared_record = 0;
    int actual_ret_validate = CPLAT_ERR_UNKNOWN;

    fill_config(&config, 4, 5, CPLAT_HASHTABLE_TIMESTAMP_SCOPE_RECORD);
    fill_value(&value, "v");

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    (void)cplat_hashtable_add(ht, "a", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_delete(ht, "a"); // [手順] - 削除済みレコードを作る。
    int actual_ret_before = cplat_hashtable_get_table_generation(ht, &before_purge);
    int actual_ret_purge = cplat_hashtable_purge_deleted(ht); // [手順] - 削除済みを空へ戻す。
    int actual_ret_after_purge = cplat_hashtable_get_table_generation(ht, &after_purge);
    int actual_ret_purged_record =
        cplat_hashtable_get_generation(ht, 1, &cleared_record); // [手順] - 空へ戻したレコードの世代を読む。
    actual_ret_validate = cplat_hashtable_validate(ht);         // [手順] - 内部整合性を検証する。
    (void)cplat_hashtable_add(ht, "a", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    int actual_ret_clear = cplat_hashtable_clear(ht); // [手順] - テーブルを空にする。
    int actual_ret_after_clear = cplat_hashtable_get_table_generation(ht, &after_clear);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_before);      // [確認_正常系] - 削除後にテーブル世代を読めること。
    EXPECT_EQ(2u, before_purge);                 // [確認_正常系] - 追加と削除でテーブル世代が 2 になること。
    EXPECT_EQ(CPLAT_OK, actual_ret_purge);       // [確認_正常系] - purge_deleted が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_after_purge); // [確認_正常系] - 回収後にテーブル世代を読めること。
    EXPECT_EQ(before_purge, after_purge);        // [確認_正常系] - purge_deleted がテーブル世代を進めないこと。
    EXPECT_EQ(CPLAT_ERR_NOT_FOUND,
              actual_ret_purged_record);         // [確認_異常系] - 空へ戻したレコードの世代は NOT_FOUND であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_validate);    // [確認_正常系] - 空へ戻した後も内部整合性が保たれること。
    EXPECT_EQ(CPLAT_OK, actual_ret_clear);       // [確認_正常系] - clear が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_after_clear); // [確認_正常系] - clear 後にテーブル世代を読めること。
    EXPECT_GT(after_clear, after_purge);         // [確認_正常系] - clear がテーブル世代を進めること。
    EXPECT_EQ(4u, after_clear);                  // [確認_正常系] - 追加と clear でテーブル世代がさらに 2 増えること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// 世代関連 API に不正な引数や範囲外キーを渡した場合のエラー返却の確認
TEST_F(hashtableGenerationTest, generation_guards_reject_invalid_arguments)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    uint64_t generation = 0;

    fill_config(&config, 4, 5, CPLAT_HASHTABLE_TIMESTAMP_SCOPE_RECORD);
    fill_value(&value, "v");

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    (void)cplat_hashtable_add(ht, "a", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    int actual_ret_table_null_ht = cplat_hashtable_get_table_generation(NULL, &generation);
    int actual_ret_table_null_out = cplat_hashtable_get_table_generation(ht, NULL);
    int actual_ret_record_null_ht = cplat_hashtable_get_generation(NULL, 1, &generation);
    int actual_ret_record_null_out = cplat_hashtable_get_generation(ht, 1, NULL);
    int actual_ret_record_zero = cplat_hashtable_get_generation(ht, 0, &generation);
    int actual_ret_record_over = cplat_hashtable_get_generation(ht, 5, &generation);
    int actual_ret_record_empty =
        cplat_hashtable_get_generation(ht, 2, &generation); // [手順] - 空のレコードの世代を読む。
    int actual_ret_find_null_ht = cplat_hashtable_find_generation(NULL, "a", &generation);
    int actual_ret_find_null_key = cplat_hashtable_find_generation(ht, NULL, &generation);
    int actual_ret_find_null_out = cplat_hashtable_find_generation(ht, "a", NULL);
    int actual_ret_find_long_key = cplat_hashtable_find_generation(
        ht, "0123456789", &generation); // [手順] - key_size に収まらないキーで検索する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_table_null_ht); // [確認_異常系] - ht が NULL なら失敗すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_table_null_out); // [確認_異常系] - 格納先が NULL なら失敗すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_record_null_ht); // [確認_異常系] - ht が NULL なら失敗すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_record_null_out); // [確認_異常系] - 格納先が NULL なら失敗すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_record_zero); // [確認_異常系] - レコード番号 0 は失敗すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_record_over); // [確認_異常系] - capacity を超えるレコード番号は失敗すること。
    EXPECT_EQ(CPLAT_ERR_NOT_FOUND,
              actual_ret_record_empty); // [確認_異常系] - 空のレコードは NOT_FOUND であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_find_null_ht); // [確認_異常系] - ht が NULL なら失敗すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_find_null_key); // [確認_異常系] - キーが NULL なら失敗すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_find_null_out); // [確認_異常系] - 格納先が NULL なら失敗すること。
    EXPECT_EQ(CPLAT_ERR_OUT_OF_RANGE,
              actual_ret_find_long_key); // [確認_異常系] - key_size に収まらないキーは OUT_OF_RANGE であること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}
