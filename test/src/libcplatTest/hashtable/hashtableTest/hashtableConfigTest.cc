#include <testfw.h>

#include <cplat/base/result.h>
#include <cplat/hashtable/hashtable.h>
#include <mock_cplat.h>

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

} // namespace

class hashtableConfigTest : public Test
{
  protected:
    NiceMock<Mock_cplat> mock_cplat_;
};

// cplat_hashtable_get_config_ref に NULL 引数を渡した場合に INVALID_ARGUMENT が返ることの確認
TEST_F(hashtableConfigTest, get_config_ref_rejects_null_arguments)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    const cplat_hashtable_config *out = nullptr;

    fill_config(&config, 2, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - 妥当な設定を用意する。

    // Pre-Assert

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    int actual_ret_null_ht = cplat_hashtable_get_config_ref(NULL, &out); // [手順] - ht に NULL を渡す。
    int actual_ret_null_out = cplat_hashtable_get_config_ref(ht, NULL);  // [手順] - config_out に NULL を渡す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_null_ht); // [確認_異常系] - NULL ht が INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_null_out); // [確認_異常系] - NULL config_out が INVALID_ARGUMENT であること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// cplat_hashtable_get_config_val に不正な引数を渡した際のエラー返却、および妥当な引数での設定複製の成功の確認
TEST_F(hashtableConfigTest, get_config_val_rejects_null_out_and_propagates_ref_failure)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable_config out = {};
    cplat_hashtable *ht = nullptr;

    fill_config(&config, 2, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - 妥当な設定を用意する。

    // Pre-Assert

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    int actual_ret_null_out = cplat_hashtable_get_config_val(ht, NULL);  // [手順] - config_out に NULL を渡す。
    int actual_ret_null_ht = cplat_hashtable_get_config_val(NULL, &out); // [手順] - ht に NULL を渡す。
    int actual_ret_ok = cplat_hashtable_get_config_val(ht, &out);        // [手順] - 妥当な引数で呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_null_out); // [確認_異常系] - NULL config_out が INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_null_ht);      // [確認_異常系] - NULL ht が get_config_ref の失敗として伝播すること。
    EXPECT_EQ(CPLAT_OK, actual_ret_ok); // [確認_正常系] - 妥当な引数で複製できること。
    EXPECT_EQ(2u, out.capacity);        // [確認_正常系] - 複製内容が一致すること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// cplat_hashtable_buffer_size の不正な引数 (NULL ht, 両方 NULL の出力先) の拒否確認
TEST_F(hashtableConfigTest, buffer_size_rejects_null_arguments)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    size_t mgmt_size = 0;
    size_t data_size = 0;

    fill_config(&config, 2, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - 妥当な設定を用意する。
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [状態] - テーブルを構築しておく。

    // Pre-Assert

    // Act
    int actual_ret_null_ht = cplat_hashtable_buffer_size(NULL, &mgmt_size, &data_size); // [手順] - ht に NULL を渡す。
    int actual_ret_null_both = cplat_hashtable_buffer_size(ht, NULL, NULL); // [手順] - 両方の出力先に NULL を渡す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_null_ht); // [確認_異常系] - NULL ht が INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_null_both); // [確認_異常系] - 両方 NULL のとき INVALID_ARGUMENT であること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// cplat_hashtable_buffer_size で領域サイズを取得できることおよび片側 NULL 指定を許容することの確認
TEST_F(hashtableConfigTest, buffer_size_allows_partial_query_and_succeeds)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    size_t mgmt_size = 0;
    size_t data_size = 0;
    size_t full_mgmt_size = 0;
    size_t full_data_size = 0;

    fill_config(&config, 2, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - 妥当な設定を用意する。
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [状態] - テーブルを構築しておく。

    // Pre-Assert

    // Act
    int actual_ret_only_mgmt = cplat_hashtable_buffer_size(ht, &mgmt_size, NULL); // [手順] - データ側だけ NULL を渡す。
    int actual_ret_only_data = cplat_hashtable_buffer_size(ht, NULL, &data_size); // [手順] - 管理側だけ NULL を渡す。
    int actual_ret_ok = cplat_hashtable_buffer_size(ht, &full_mgmt_size,
                                                    &full_data_size); // [手順] - 両方の引数を渡してサイズを取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_only_mgmt); // [確認_正常系] - 管理側だけの問い合わせが成功すること。
    EXPECT_GT(mgmt_size, 0u);                  // [確認_正常系] - 管理領域サイズが取得できていること。
    EXPECT_EQ(CPLAT_OK, actual_ret_only_data); // [確認_正常系] - データ側だけの問い合わせが成功すること。
    EXPECT_GT(data_size, 0u);                  // [確認_正常系] - データ領域サイズが取得できていること。
    EXPECT_EQ(CPLAT_OK, actual_ret_ok);        // [確認_正常系] - 両方の問い合わせが成功すること。
    EXPECT_EQ(mgmt_size, full_mgmt_size);      // [確認_正常系] - 管理領域サイズが一致すること。
    EXPECT_EQ(data_size, full_data_size);      // [確認_正常系] - データ領域サイズが一致すること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// cplat_hashtable_buffer_ref の不正な引数 (NULL ht, 両方 NULL の出力先) の拒否確認
TEST_F(hashtableConfigTest, buffer_ref_rejects_null_arguments)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    const void *mgmt = nullptr;
    const void *data = nullptr;

    fill_config(&config, 2, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - 妥当な設定を用意する。
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [状態] - テーブルを構築しておく。

    // Pre-Assert

    // Act
    int actual_ret_null_ht = cplat_hashtable_buffer_ref(NULL, &mgmt, &data); // [手順] - ht に NULL を渡す。
    int actual_ret_null_both = cplat_hashtable_buffer_ref(ht, NULL, NULL);   // [手順] - 両方の出力先に NULL を渡す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_null_ht); // [確認_異常系] - NULL ht が INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_null_both); // [確認_異常系] - 両方 NULL のとき INVALID_ARGUMENT であること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// cplat_hashtable_buffer_ref で片側の出力先のみ NULL を指定した場合に正常取得できることの確認
TEST_F(hashtableConfigTest, buffer_ref_allows_partial_query)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    const void *mgmt = nullptr;
    const void *data = nullptr;

    fill_config(&config, 2, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - 妥当な設定を用意する。
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [状態] - テーブルを構築しておく。

    // Pre-Assert

    // Act
    int actual_ret_only_mgmt = cplat_hashtable_buffer_ref(ht, &mgmt, NULL); // [手順] - データ側だけ NULL を渡す。
    int actual_ret_only_data = cplat_hashtable_buffer_ref(ht, NULL, &data); // [手順] - 管理側だけ NULL を渡す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_only_mgmt); // [確認_正常系] - 管理側だけの問い合わせが成功すること。
    EXPECT_NE(nullptr, mgmt);                  // [確認_正常系] - 管理領域ポインタが取得できること。
    EXPECT_EQ(CPLAT_OK, actual_ret_only_data); // [確認_正常系] - データ側だけの問い合わせが成功すること。
    EXPECT_NE(nullptr, data);                  // [確認_正常系] - データ領域ポインタが取得できること。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// 内部確保されたハッシュテーブルにおいて cplat_hashtable_buffer_ref が管理領域とデータ領域の連続した参照を返すことの確認
TEST_F(hashtableConfigTest, buffer_ref_returns_internal_regions_contiguously)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    const void *mgmt = nullptr;
    const void *data = nullptr;
    size_t mgmt_size = 0;
    size_t data_size = 0;

    fill_config(&config, 2, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - 妥当な設定を用意する。

    // Pre-Assert

    // Act
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);  // [手順] - 内部確保で構築する。
    int actual_ret = cplat_hashtable_buffer_ref(ht, &mgmt, &data); // [手順] - 両領域の先頭を取得する。
    (void)cplat_hashtable_buffer_size(ht, &mgmt_size, &data_size);
    const unsigned char *mgmt_bytes = static_cast<const unsigned char *>(mgmt);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - buffer_ref が成功すること。
    EXPECT_NE(nullptr, mgmt);        // [確認_正常系] - 管理領域の先頭が非 NULL であること。
    EXPECT_NE(nullptr, data);        // [確認_正常系] - データ領域の先頭が非 NULL であること。
    EXPECT_EQ(static_cast<const void *>(mgmt_bytes + mgmt_size),
              data); // [確認_正常系] - 内部確保では管理領域の直後にデータ領域が続くこと。

    // Cleanup
    cplat_hashtable_dispose(ht);
}

// 外部領域指定で構築および再接続したハッシュテーブルにおいて cplat_hashtable_buffer_ref が指定領域の参照を正しく返すことの確認
TEST_F(hashtableConfigTest, buffer_ref_returns_external_regions_as_supplied)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    cplat_hashtable *attached = nullptr;
    const void *mgmt = nullptr;
    const void *data = nullptr;
    const void *attached_mgmt = nullptr;
    const void *attached_data = nullptr;
    size_t mgmt_size = 0;
    size_t data_size = 0;

    fill_config(&config, 2, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - 妥当な設定を用意する。
    (void)cplat_hashtable_required_size(&config, &mgmt_size, &data_size);
    std::vector<uint64_t> buf_mgmt((mgmt_size + sizeof(uint64_t) - 1u) / sizeof(uint64_t), 0);
    std::vector<unsigned char> buf_data(data_size, 0); // [状態] - 外部指定用の 2 領域を用意する。

    // Pre-Assert

    // Act
    (void)cplat_hashtable_create(&config, buf_mgmt.data(), mgmt_size, buf_data.data(), buf_data.size(),
                                 &ht); // [手順] - 外部指定で構築する。
    int actual_ret_create = cplat_hashtable_buffer_ref(ht, &mgmt, &data);
    (void)cplat_hashtable_attach(buf_mgmt.data(), mgmt_size, buf_data.data(), buf_data.size(),
                                 &attached); // [手順] - 同じ領域へ再接続する。
    int actual_ret_attach = cplat_hashtable_buffer_ref(attached, &attached_mgmt, &attached_data);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_create); // [確認_正常系] - 外部指定でも buffer_ref が成功すること。
    EXPECT_EQ(static_cast<const void *>(buf_mgmt.data()),
              mgmt); // [確認_正常系] - 渡した管理領域がそのまま返ること。
    EXPECT_EQ(static_cast<const void *>(buf_data.data()),
              data);                        // [確認_正常系] - 渡したデータ領域がそのまま返ること。
    EXPECT_EQ(CPLAT_OK, actual_ret_attach); // [確認_正常系] - 再接続後も buffer_ref が成功すること。
    EXPECT_EQ(static_cast<const void *>(buf_mgmt.data()),
              attached_mgmt); // [確認_正常系] - 再接続後も管理領域が一致すること。
    EXPECT_EQ(static_cast<const void *>(buf_data.data()),
              attached_data); // [確認_正常系] - 再接続がデータ領域アドレスを渡した値で上書きすること。

    // Cleanup
    cplat_hashtable_dispose(ht);
    cplat_hashtable_dispose(attached);
}
