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

} // namespace

class hashtableTest : public Test
{
  protected:
    NiceMock<Mock_cplat> mock_cplat_;
};

// cplat_hashtable_required_size の不正な引数 (NULL config, 両方 NULL の出力先) の拒否確認
TEST_F(hashtableTest, required_size_rejects_null_arguments)
{
    // Arrange
    cplat_hashtable_config config = {};
    size_t mgmt_size = 1;
    size_t data_size = 1;

    fill_config(&config, 4, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - 妥当な設定を用意する。

    // Pre-Assert

    // Act
    int actual_ret_null_config =
        cplat_hashtable_required_size(NULL, &mgmt_size, &data_size); // [手順] - config に NULL を渡す。
    int actual_ret_null_both =
        cplat_hashtable_required_size(&config, NULL, NULL); // [手順] - 両方の出力先に NULL を渡す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_null_config); // [確認_異常系] - config が NULL のとき INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_null_both); // [確認_異常系] - 両方の出力先が NULL のとき INVALID_ARGUMENT であること。
}

// cplat_hashtable_required_size で片側の出力先のみ NULL を指定した場合に正常取得できることの確認
TEST_F(hashtableTest, required_size_allows_partial_query)
{
    // Arrange
    cplat_hashtable_config config = {};
    size_t mgmt_size = 0;
    size_t data_size = 0;

    fill_config(&config, 4, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - 妥当な設定を用意する。

    // Pre-Assert

    // Act
    int actual_ret_only_mgmt =
        cplat_hashtable_required_size(&config, &mgmt_size, NULL); // [手順] - データ側だけ NULL を渡す。
    int actual_ret_only_data =
        cplat_hashtable_required_size(&config, NULL, &data_size); // [手順] - 管理側だけ NULL を渡す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_only_mgmt); // [確認_正常系] - 管理側だけの問い合わせが成功すること。
    EXPECT_GT(mgmt_size, 0u);                  // [確認_正常系] - 管理領域サイズが取得できていること。
    EXPECT_EQ(CPLAT_OK, actual_ret_only_data); // [確認_正常系] - データ側だけの問い合わせが成功すること。
    EXPECT_GT(data_size, 0u);                  // [確認_正常系] - データ領域サイズが取得できていること。
}

// ハッシュテーブルの生成から追加・検索・ステータス取得までのラウンドトリップ確認
TEST_F(hashtableTest, create_and_add_find_round_trip)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(16, 0);
    const void *found = nullptr;
    uint64_t rec = 0;
    int status = -1;
    size_t in_use = 0;
    size_t deleted = 0;
    size_t empty = 0;

    fill_config(&config, 4, 16, 16, 5,
                CPLAT_HASHTABLE_KEY_STRING); // [状態] - 文字列モードの小さなテーブル設定を用意する。
    fill_value(&value, "apple-value");       // [状態] - 追加する値を用意する。

    // Pre-Assert

    // Act
    int actual_ret_create =
        cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [手順] - 内部確保でテーブルを構築する。
    int actual_ret_add = cplat_hashtable_add(ht, "apple", value.data(),
                                             CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - apple を追加する。
    int actual_ret_find = cplat_hashtable_find_value_ref(ht, "apple", &found);       // [手順] - apple を検索する。
    int actual_ret_rec = cplat_hashtable_find_recno(ht, "apple", &rec);   // [手順] - apple のレコード番号を取得する。
    int actual_ret_status = cplat_hashtable_get_status(ht, rec, &status); // [手順] - レコード状態を取得する。
    int actual_ret_counts = cplat_hashtable_count_status(ht, &in_use, &deleted, &empty); // [手順] - 件数を取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_create); // [確認_正常系] - create が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_add);    // [確認_正常系] - add が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_find);   // [確認_正常系] - find が成功すること。
    EXPECT_STREQ("apple-value",
                 static_cast<const char *>(found)); // [確認_正常系] - 取得した値が追加した文字列であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_rec);            // [確認_正常系] - find_recno が成功すること。
    EXPECT_EQ(1u, rec);                             // [確認_正常系] - 最初の追加のレコード番号が 1 であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_status);         // [確認_正常系] - get_status が成功すること。
    EXPECT_EQ(1, status);                           // [確認_正常系] - 状態が実装中であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_counts);         // [確認_正常系] - count_status が成功すること。
    EXPECT_EQ(1u, in_use);                          // [確認_正常系] - 実装中が 1 件であること。
    EXPECT_EQ(0u, deleted);                         // [確認_正常系] - 削除済みが 0 件であること。
    EXPECT_EQ(3u, empty);                           // [確認_正常系] - 空が 3 件であること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - 検証後にテーブルを破棄する。
}

// cplat_hashtable_create の不正な config パラメーター (capacity, key_size, value_size, lifetime 等) の拒否確認
TEST_F(hashtableTest, create_rejects_invalid_config)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = reinterpret_cast<cplat_hashtable *>(1);

    fill_config(&config, 0, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - capacity 0 の設定を用意する。

    // Pre-Assert

    // Act
    int actual_ret_null_out =
        cplat_hashtable_create(&config, NULL, 0, NULL, 0, NULL); // [手順] - ht_out に NULL を渡す。
    int actual_ret_null_config =
        cplat_hashtable_create(NULL, NULL, 0, NULL, 0, &ht); // [手順] - config に NULL を渡す。
    int actual_ret_capacity =
        cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [手順] - capacity 0 で構築する。
    fill_config(&config, 4, 0, 8, 5, CPLAT_HASHTABLE_KEY_STRING);
    int actual_ret_key_size =
        cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [手順] - key_size 0 で構築する。
    fill_config(&config, 4, 8, 0, 5, CPLAT_HASHTABLE_KEY_STRING);
    int actual_ret_value_size =
        cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [手順] - value_size 0 で構築する。
    fill_config(&config, 4, 8, 8, 1, CPLAT_HASHTABLE_KEY_STRING);
    int actual_ret_lifetime =
        cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [手順] - lifetime 1 で構築する。
    fill_config(&config, 4, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING);
    {
        int invalid_key_type = 2;
        std::memcpy(&config.key_type, &invalid_key_type, sizeof(config.key_type));
    }
    int actual_ret_key_type =
        cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [手順] - 不正な key_type で構築する。
    fill_config(&config, 4, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING);
    {
        int invalid_scope = 2;
        std::memcpy(&config.timestamp_scope, &invalid_scope, sizeof(config.timestamp_scope));
    }
    int actual_ret_scope =
        cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);   // [手順] - 不正な timestamp_scope で構築する。
    fill_config(&config, 4, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - 妥当な設定へ戻す。
    unsigned char mgmt_only[8];
    unsigned char data_only[8];
    int actual_ret_mgmt_only = cplat_hashtable_create(&config, mgmt_only, sizeof(mgmt_only), NULL, 0,
                                                      &ht); // [手順] - 管理領域だけ非 NULL で構築する。
    int actual_ret_data_only = cplat_hashtable_create(&config, NULL, 0, data_only, sizeof(data_only),
                                                      &ht); // [手順] - データ領域だけ非 NULL で構築する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_null_out); // [確認_異常系] - ht_out が NULL のとき INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_null_config); // [確認_異常系] - config が NULL のとき INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_capacity); // [確認_異常系] - capacity 0 が拒否されること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_key_size); // [確認_異常系] - key_size 0 が拒否されること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_value_size);                           // [確認_異常系] - value_size 0 が拒否されること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_lifetime); // [確認_異常系] - lifetime 1 が拒否されること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_key_type); // [確認_異常系] - 不正な key_type が拒否されること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_scope); // [確認_異常系] - 不正な timestamp_scope が拒否されること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_mgmt_only); // [確認_異常系] - 管理領域だけ非 NULL が拒否されること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_data_only); // [確認_異常系] - データ領域だけ非 NULL が拒否されること。
    EXPECT_EQ(nullptr, ht);          // [確認_異常系] - 失敗後の ht_out が NULL であること。
}

// cplat_hashtable_required_size の不正な config パラメーターの拒否確認
TEST_F(hashtableTest, required_size_rejects_invalid_config)
{
    // Arrange
    cplat_hashtable_config config = {};
    size_t mgmt_size = 1;
    size_t data_size = 1;

    fill_config(&config, 0, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - capacity 0 の設定を用意する。

    // Pre-Assert

    // Act
    int actual_ret_capacity =
        cplat_hashtable_required_size(&config, &mgmt_size, &data_size); // [手順] - capacity 0 で問い合わせる。
    fill_config(&config, 4, 0, 8, 5, CPLAT_HASHTABLE_KEY_STRING);
    int actual_ret_key_size =
        cplat_hashtable_required_size(&config, &mgmt_size, &data_size); // [手順] - key_size 0 で問い合わせる。
    fill_config(&config, 4, 8, 0, 5, CPLAT_HASHTABLE_KEY_STRING);
    int actual_ret_value_size =
        cplat_hashtable_required_size(&config, &mgmt_size, &data_size); // [手順] - value_size 0 で問い合わせる。
    fill_config(&config, 4, 8, 8, 1, CPLAT_HASHTABLE_KEY_STRING);
    int actual_ret_lifetime =
        cplat_hashtable_required_size(&config, &mgmt_size, &data_size); // [手順] - lifetime 1 で問い合わせる。
    fill_config(&config, 4, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING);
    {
        int invalid_key_type = 2;
        std::memcpy(&config.key_type, &invalid_key_type, sizeof(config.key_type));
    }
    int actual_ret_key_type =
        cplat_hashtable_required_size(&config, &mgmt_size, &data_size); // [手順] - 不正な key_type で問い合わせる。
    fill_config(&config, 4, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING);
    {
        int invalid_scope = 2;
        std::memcpy(&config.timestamp_scope, &invalid_scope, sizeof(config.timestamp_scope));
    }
    int actual_ret_scope = cplat_hashtable_required_size(
        &config, &mgmt_size, &data_size); // [手順] - 不正な timestamp_scope で問い合わせる。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_capacity); // [確認_異常系] - capacity 0 が拒否されること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_key_size); // [確認_異常系] - key_size 0 が拒否されること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_value_size);                           // [確認_異常系] - value_size 0 が拒否されること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_lifetime); // [確認_異常系] - lifetime 1 が拒否されること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_key_type); // [確認_異常系] - 不正な key_type が拒否されること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_scope); // [確認_異常系] - 不正な timestamp_scope が拒否されること。
}

// cplat_hashtable_add が重複キーや長すぎるキーを拒否し空文字列キーを受け付けることの確認
TEST_F(hashtableTest, add_rejects_duplicate_and_too_long_key)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 1);
    char too_long[9];

    fill_config(&config, 4, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - key_size 8 のテーブル設定を用意する。
    std::memset(too_long, 'x', sizeof(too_long));                 // [状態] - NUL が 8 バイト以内に無いキーを用意する。

    // Pre-Assert

    // Act
    int actual_ret_create = cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [手順] - テーブルを構築する。
    int actual_ret_add = cplat_hashtable_add(ht, "k", value.data(),
                                             CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 短いキーを追加する。
    int actual_ret_dup = cplat_hashtable_add(ht, "k", value.data(),
                                             CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 同じキーを再追加する。
    int actual_ret_long = cplat_hashtable_add(
        ht, too_long, value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 長すぎるキーを追加する。
    int actual_ret_empty = cplat_hashtable_add(
        ht, "", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 空文字列キーを追加する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_create); // [確認_正常系] - create が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_add);    // [確認_正常系] - 最初の add が成功すること。
    EXPECT_EQ(CPLAT_ERR_DUPLICATE_KEY,
              actual_ret_dup);                          // [確認_異常系] - 重複キーが DUPLICATE_KEY であること。
    EXPECT_EQ(CPLAT_ERR_OUT_OF_RANGE, actual_ret_long); // [確認_異常系] - 長すぎるキーが OUT_OF_RANGE であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_empty);              // [確認_正常系] - 空文字列キーが追加できること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - テーブルを破棄する。
}

// cplat_hashtable_add が不正な deleted_policy を拒否することの確認
TEST_F(hashtableTest, add_rejects_invalid_deleted_policy)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 1);
    cplat_hashtable_add_deleted_policy invalid_policy = CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE;

    fill_config(&config, 4, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - 妥当な設定を用意する。
    {
        int invalid_value = 2;
        std::memcpy(&invalid_policy, &invalid_value,
                    sizeof(invalid_policy)); // [状態] - 範囲外の deleted_policy を用意する。
    }

    // Pre-Assert

    // Act
    int actual_ret_create = cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [手順] - テーブルを構築する。
    int actual_ret_add =
        cplat_hashtable_add(ht, "a", value.data(), invalid_policy); // [手順] - 不正な deleted_policy で追加する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_create); // [確認_正常系] - create が成功すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_add); // [確認_異常系] - 不正な deleted_policy が INVALID_ARGUMENT であること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - テーブルを破棄する。
}

// cplat_hashtable_delete 後に加齢を経てレコードが再利用可能になることの確認
TEST_F(hashtableTest, delete_ages_until_reuse)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 2);
    int status_after_delete = -1;
    int status_after_push = -1;
    int add_full = 0;
    int add_after_purge = 0;
    size_t empty = 99;

    fill_config(&config, 2, 16, 8, 5,
                CPLAT_HASHTABLE_KEY_STRING); // [状態] - capacity 2、lifetime 5 の設定を用意する。

    // Pre-Assert

    // Act
    int actual_ret_create = cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [手順] - テーブルを構築する。
    (void)cplat_hashtable_add(ht, "a", value.data(),
                              CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 1 件目を追加する。
    (void)cplat_hashtable_add(ht, "b", value.data(),
                              CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 2 件目を追加する。
    int actual_ret_delete = cplat_hashtable_delete(ht, "a");          // [手順] - a を削除する。
    (void)cplat_hashtable_get_status(ht, 1, &status_after_delete);    // [手順] - 削除直後の状態を取得する。
    add_full = cplat_hashtable_add(ht, "c", value.data(),
                                   CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 満杯のまま別キーを追加する。
    (void)cplat_hashtable_push_deleted(ht);                                // [手順] - 加齢する。
    (void)cplat_hashtable_push_deleted(ht);
    (void)cplat_hashtable_push_deleted(ht);
    (void)cplat_hashtable_get_status(ht, 1, &status_after_push); // [手順] - 寿命到達後の状態を取得する。
    add_after_purge = cplat_hashtable_add(
        ht, "c", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 空きが生じたあと別キーを追加する。
    (void)cplat_hashtable_empty_count(ht, &empty);                     // [手順] - 空件数を取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_create);        // [確認_正常系] - create が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_delete);        // [確認_正常系] - delete が成功すること。
    EXPECT_EQ(2, status_after_delete);             // [確認_正常系] - 削除直後の状態が 2 であること。
    EXPECT_EQ(CPLAT_ERR_LIMIT_EXCEEDED, add_full); // [確認_異常系] - 削除直後は満杯のままであること。
    EXPECT_EQ(0, status_after_push);               // [確認_正常系] - 寿命到達後に空へ戻ること。
    EXPECT_EQ(CPLAT_OK, add_after_purge);          // [確認_正常系] - 空き後の追加が成功すること。
    EXPECT_EQ(0u, empty);                          // [確認_正常系] - 再利用後は空が 0 であること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - テーブルを破棄する。
}

// 外部バッファーを用いたハッシュテーブルの作成・アタッチ・検証・検索の確認
TEST_F(hashtableTest, external_buffer_attach_and_validate)
{
    // Arrange
    cplat_hashtable_config config = {};
    size_t mgmt_needed = 0;
    size_t data_needed = 0;
    cplat_hashtable *ht = nullptr;
    cplat_hashtable *reattached = nullptr;
    std::vector<unsigned char> value(8, 3);
    const void *found = nullptr;
    unsigned char small_mgmt[8];

    fill_config(&config, 2, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - 外部バッファー用の設定を用意する。
    std::memset(small_mgmt, 0xAB, sizeof(small_mgmt));            // [状態] - 不足バッファーの番兵を埋める。

    // Pre-Assert

    // Act
    int actual_ret_size =
        cplat_hashtable_required_size(&config, &mgmt_needed, &data_needed); // [手順] - 必要サイズを求める。
    std::vector<unsigned char> buf_mgmt(mgmt_needed, 0);
    std::vector<unsigned char> buf_data(data_needed, 0);
    std::vector<unsigned char> small_data(1, 0);
    int actual_ret_small_mgmt =
        cplat_hashtable_create(&config, small_mgmt, sizeof(small_mgmt), buf_data.data(), buf_data.size(),
                               &ht); // [手順] - 管理領域だけ不足したバッファーで構築する。
    int actual_ret_small_data =
        cplat_hashtable_create(&config, buf_mgmt.data(), buf_mgmt.size(), small_data.data(), small_data.size(),
                               &ht); // [手順] - データ領域だけ不足したバッファーで構築する。
    int actual_ret_create =
        cplat_hashtable_create(&config, buf_mgmt.data(), buf_mgmt.size(), buf_data.data(), buf_data.size(),
                               &ht); // [手順] - 十分な管理領域とデータ領域で構築する。
    (void)cplat_hashtable_add(ht, "fig", value.data(),
                              CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - キーを追加する。
    std::vector<unsigned char> buf_mgmt2 = buf_mgmt;
    std::vector<unsigned char> buf_data2 = buf_data;
    int actual_ret_attach =
        cplat_hashtable_attach(buf_mgmt2.data(), buf_mgmt2.size(), buf_data2.data(), buf_data2.size(),
                               &reattached); // [手順] - 複製した管理領域とデータ領域へ再接続する。
    int actual_ret_validate = cplat_hashtable_validate(reattached);                  // [手順] - 整合性を検証する。
    int actual_ret_find = cplat_hashtable_find_value_ref(reattached, "fig", &found); // [手順] - 再接続後に検索する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_size); // [確認_正常系] - required_size が成功すること。
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              actual_ret_small_mgmt); // [確認_異常系] - 管理領域不足が BUFFER_TOO_SMALL であること。
    EXPECT_EQ(0xAB, small_mgmt[0]);   // [確認_異常系] - 不足時に管理領域バッファーを書き換えないこと。
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              actual_ret_small_data);         // [確認_異常系] - データ領域不足が BUFFER_TOO_SMALL であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_create);   // [確認_正常系] - 外部バッファーでの create が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_attach);   // [確認_正常系] - attach が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_validate); // [確認_正常系] - validate が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_find);     // [確認_正常系] - 再接続後も検索できること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - 外部バッファーの destroy を呼ぶ。
    cplat_hashtable_dispose(reattached);
}

// 同一外部バッファーへの複数ハンドルアタッチにおいて一方の破棄が他方に影響しないことの確認
TEST_F(hashtableTest, attach_shares_buffer_without_corrupting_other_handle)
{
    // Arrange
    cplat_hashtable_config config = {};
    size_t mgmt_needed = 0;
    size_t data_needed = 0;
    cplat_hashtable *ht1 = nullptr;
    cplat_hashtable *ht2 = nullptr;
    std::vector<unsigned char> value(8, 3);
    const void *found = nullptr;

    fill_config(&config, 2, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - 外部バッファー用の設定を用意する。
    (void)cplat_hashtable_required_size(&config, &mgmt_needed, &data_needed);
    std::vector<unsigned char> buf_mgmt(mgmt_needed, 0);
    std::vector<unsigned char> buf_data(data_needed, 0); // [状態] - 同一の外部バッファーを 1 組だけ用意する。

    // Pre-Assert

    // Act
    int actual_ret_create = cplat_hashtable_create(&config, buf_mgmt.data(), buf_mgmt.size(), buf_data.data(),
                                                   buf_data.size(), &ht1); // [手順] - 外部バッファーで構築する。
    int actual_ret_attach =
        cplat_hashtable_attach(buf_mgmt.data(), buf_mgmt.size(), buf_data.data(), buf_data.size(),
                               &ht2); // [手順] - 同一のバッファーへ、もう一つ独立したハンドルで再接続する。
    int actual_ret_add = cplat_hashtable_add(
        ht1, "shared", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 一方のハンドルで追加する。
    int actual_ret_find_via_ht2 =
        cplat_hashtable_find_value_ref(ht2, "shared", &found); // [手順] - もう一方のハンドルから検索する。
    cplat_hashtable_dispose(ht2); // [手順] - 一方を破棄する(owns_buffer が 0 のため共有バッファーは解放されない)。
    int actual_ret_find_via_ht1_after_dispose = cplat_hashtable_find_value_ref(
        ht1, "shared", &found); // [手順] - もう一方のハンドルが破棄後も正常に動作することを確認する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_create); // [確認_正常系] - 外部バッファーでの create が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_attach); // [確認_正常系] - 同一バッファーへの attach が成功すること。
    EXPECT_NE(static_cast<const void *>(ht1),
              static_cast<const void *>(ht2)); // [確認_正常系] - 内部管理データが別々に確保されること。
    EXPECT_EQ(CPLAT_OK, actual_ret_add);       // [確認_正常系] - 一方のハンドルでの追加が成功すること。
    EXPECT_EQ(CPLAT_OK,
              actual_ret_find_via_ht2); // [確認_正常系] - 追加内容がもう一方のハンドルから見えること。
    EXPECT_EQ(CPLAT_OK,
              actual_ret_find_via_ht1_after_dispose); // [確認_正常系] - 一方の dispose 後も他方が動作を続けること。

    // Cleanup
    cplat_hashtable_dispose(ht1); // [破棄] - 残ったハンドルを破棄する。
}

// バイナリモードにおける全ゼロキー登録とキー・値のコピーAPIの確認
TEST_F(hashtableTest, binary_zero_key_and_copy_apis)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    unsigned char key1[8] = {0};
    unsigned char key2[8] = {0};
    unsigned char key3[8] = {0};
    std::vector<unsigned char> value(8, 0);
    std::vector<unsigned char> key_copy(8, 0);
    std::vector<unsigned char> value_copy(8, 0);
    size_t key_required_size = 0;
    size_t value_required_size = 0;
    uint64_t rec = 0;

    fill_config(&config, 4, 8, 8, 5, CPLAT_HASHTABLE_KEY_BINARY); // [状態] - バイナリ モードの設定を用意する。
    key1[0] = 1;
    key2[0] = 1;
    key2[1] = 2;
    key3[0] = 1;
    key3[7] = 0xFF;
    fill_value(&value, "bin"); // [状態] - 値を用意する。

    // Pre-Assert

    // Act
    int actual_ret_create = cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [手順] - テーブルを構築する。
    int actual_ret_add1 = cplat_hashtable_add(ht, key1, value.data(),
                                              CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - key1 を追加する。
    unsigned char zero_key[8] = {0};
    int actual_ret_zero = cplat_hashtable_add(
        ht, zero_key, value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - 全ゼロ キーを追加する。
    const void *found2 = nullptr;
    int actual_ret_miss = cplat_hashtable_find_value_ref(ht, key2, &found2); // [手順] - 異なるキーを検索する。
    int actual_ret_tail = cplat_hashtable_find_value_ref(ht, key3, &found2); // [手順] - 末尾だけ違うキーを検索する。
    (void)cplat_hashtable_find_recno(ht, key1, &rec);
    int actual_ret_key_copy = cplat_hashtable_get_key_copy(ht, rec, key_copy.data(), key_copy.size(),
                                                           &key_required_size); // [手順] - キーを複製する。
    int actual_ret_value_copy = cplat_hashtable_get_value_copy(ht, rec, value_copy.data(), value_copy.size(),
                                                               &value_required_size); // [手順] - 値を複製する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_create);              // [確認_正常系] - create が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_add1);                // [確認_正常系] - key1 の追加が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_zero);                // [確認_正常系] - 全ゼロ キーの追加が成功すること。
    EXPECT_EQ(CPLAT_ERR_NOT_FOUND, actual_ret_miss);     // [確認_正常系] - 異なるキーが見つからないこと。
    EXPECT_EQ(CPLAT_ERR_NOT_FOUND, actual_ret_tail);     // [確認_正常系] - 末尾差のキーが見つからないこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_key_copy);            // [確認_正常系] - get_key_copy が成功すること。
    EXPECT_EQ(8u, key_required_size);                    // [確認_正常系] - バイナリ キーの必要量が固定幅であること。
    EXPECT_EQ(0, std::memcmp(key_copy.data(), key1, 8)); // [確認_正常系] - 複製キーが一致すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_value_copy);          // [確認_正常系] - get_value_copy が成功すること。
    EXPECT_EQ(8u, value_required_size);                  // [確認_正常系] - バイナリ値の必要量が固定幅であること。
    EXPECT_STREQ("bin", reinterpret_cast<char *>(value_copy.data())); // [確認_正常系] - 複製値が一致すること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - テーブルを破棄する。
}

// cplat_hashtable_update および cplat_hashtable_clear による値更新と全消去の確認
TEST_F(hashtableTest, update_and_clear)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    const void *found = nullptr;
    size_t in_use = 9;
    uint64_t rec = 0;

    fill_config(&config, 4, 16, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - 文字列モードの設定を用意する。
    fill_value(&value, "one");                                     // [状態] - 初期値を用意する。

    // Pre-Assert

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [手順] - テーブルを構築する。
    (void)cplat_hashtable_add(ht, "k", value.data(),
                              CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE); // [手順] - キーを追加する。
    fill_value(&value, "two");
    int actual_ret_update = cplat_hashtable_update(ht, "k", value.data()); // [手順] - キーで更新する。
    (void)cplat_hashtable_find_recno(ht, "k", &rec);
    fill_value(&value, "three");
    int actual_ret_update_rec = cplat_hashtable_update_rec(ht, rec, value.data()); // [手順] - レコード番号で更新する。
    int actual_ret_find = cplat_hashtable_find_value_ref(ht, "k", &found);         // [手順] - 更新後の値を取得する。
    std::string found_text = (found == nullptr) ? "" : static_cast<const char *>(found);
    int actual_ret_clear = cplat_hashtable_clear(ht); // [手順] - テーブルを空にする。
    (void)cplat_hashtable_count(ht, &in_use);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_update);     // [確認_正常系] - update が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_update_rec); // [確認_正常系] - update_rec が成功すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_find);       // [確認_正常系] - 更新後の検索が成功すること。
    EXPECT_EQ("three", found_text);             // [確認_正常系] - 最終値が three であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_clear);      // [確認_正常系] - clear が成功すること。
    EXPECT_EQ(0u, in_use);                      // [確認_正常系] - clear 後の実装中が 0 であること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - テーブルを破棄する。
}

// cplat_hashtable_create でメモリ確保失敗時に OUT_OF_MEMORY を返すことの確認
TEST_F(hashtableTest, create_returns_out_of_memory_when_calloc_fails)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = reinterpret_cast<cplat_hashtable *>(1);

    fill_config(&config, 4, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - 妥当な設定を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_calloc(_, _))
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_calloc が 1 回呼び出されること。
                                    // [Pre-Assert手順] - cplat_calloc から NULL を返却する。

    // Act
    int actual_ret = cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [手順] - 内部確保で構築する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY, actual_ret); // [確認_異常系] - 確保失敗が OUT_OF_MEMORY であること。
    EXPECT_EQ(nullptr, ht);                         // [確認_異常系] - ht_out が NULL であること。
}

// cplat_hashtable_create で内部管理データ確保失敗時に確保済み領域が解放されることの確認
TEST_F(hashtableTest, create_frees_buffer_when_handle_calloc_fails)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = reinterpret_cast<cplat_hashtable *>(1);

    fill_config(&config, 4, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - 妥当な設定を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_calloc(_, _))
        .WillOnce(DoDefault())      // [Pre-Assert手順] - 1 回目(管理領域+データ領域)は成功させる。
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - 2 回目(内部管理データ)から NULL を返却する。
    EXPECT_CALL(mock_cplat_, cplat_free(_))
        .Times(1); // [Pre-Assert確認_異常系] - 確保済みの管理領域+データ領域が解放されること。

    // Act
    int actual_ret = cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [手順] - 内部確保で構築する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY,
              actual_ret);  // [確認_異常系] - 内部管理データの確保失敗が OUT_OF_MEMORY であること。
    EXPECT_EQ(nullptr, ht); // [確認_異常系] - ht_out が NULL であること。
}

// cplat_hashtable_attach で内部管理データ確保失敗時に OUT_OF_MEMORY を返すことの確認
TEST_F(hashtableTest, attach_returns_out_of_memory_when_calloc_fails)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    cplat_hashtable *attached = reinterpret_cast<cplat_hashtable *>(1);
    size_t mgmt_needed = 0;
    size_t data_needed = 0;

    fill_config(&config, 4, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - 外部バッファー用の設定を用意する。
    (void)cplat_hashtable_required_size(&config, &mgmt_needed, &data_needed);
    std::vector<unsigned char> buf_mgmt(mgmt_needed, 0);
    std::vector<unsigned char> buf_data(data_needed, 0);

    (void)cplat_hashtable_create(&config, buf_mgmt.data(), buf_mgmt.size(), buf_data.data(), buf_data.size(),
                                 &ht); // [状態] - 外部バッファーへ構築しておく。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_calloc(_, _))
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - 内部管理データの calloc から NULL を返却する。

    // Act
    int actual_ret = cplat_hashtable_attach(buf_mgmt.data(), buf_mgmt.size(), buf_data.data(), buf_data.size(),
                                            &attached); // [手順] - 同じ外部バッファーへ再接続する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY,
              actual_ret);        // [確認_異常系] - 内部管理データの確保失敗が OUT_OF_MEMORY であること。
    EXPECT_EQ(nullptr, attached); // [確認_異常系] - ht_out が NULL であること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュテーブルを破棄する。
}

// cplat_hashtable_dispose に NULL を渡しても安全に無視されることの確認
TEST_F(hashtableTest, destroy_null_is_safe)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_free(_)).Times(0); // [Pre-Assert確認_正常系] - cplat_free が呼び出されないこと。

    // Act
    cplat_hashtable_dispose(NULL); // [手順] - NULL で destroy を呼ぶ。

    // Assert
    SUCCEED(); // [確認_正常系] - NULL の destroy がクラッシュしないこと。
}
