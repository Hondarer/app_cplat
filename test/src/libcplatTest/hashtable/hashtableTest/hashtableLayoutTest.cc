#include <testfw.h>

#include <cplat/base/result.h>
#include <cplat/hashtable/hashtable.h>
#include <mock_cplat.h>

#include "hashtable_inject.h"

#include <cstdint>
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

} // namespace

class hashtableLayoutTest : public Test
{
  protected:
    NiceMock<Mock_cplat> mock_cplat_;
};

/*
 * hashtable_mgmt_layout 内の align_up_checked 呼び出しのうち、次の 2 箇所は現行の実装では
 * 到達不能である(how-to-test.md「到達できない条件への対処」手順 3)。
 * - 先頭オフセットの整列: 入力が sizeof(struct cplat_hashtable) 固定の小さな値のため、
 *   uint64_t 境界への整列でオーバーフローする余地がない。
 * - バケット領域直後の整列: オフセットは header(8 バイト境界) + capacity * sizeof(uint64_t)
 *   の和であり、常に 8 の倍数になるため pad が 0 に固定され、加算は失敗しない。
 */

// cplat_hashtable_required_size がキーストライド計算のオーバーフロー時に INVALID_ARGUMENT を返すことの確認
TEST_F(hashtableLayoutTest, rejects_entry_stride_overflow)
{
    // Arrange
    cplat_hashtable_config config = {};
    size_t mgmt_size = 1;
    size_t data_size = 1;

    fill_config(&config, 1, SIZE_MAX, 8, 5,
                CPLAT_HASHTABLE_KEY_STRING); // [状態] - entry_stride_checked 内の加算が破綻する key_size を用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_hashtable_required_size(&config, &mgmt_size, &data_size); // [手順] - 必要サイズを求める。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - キー ストライド計算のオーバーフローが INVALID_ARGUMENT であること。
}

// cplat_hashtable_required_size がバケット領域オフセット計算のオーバーフロー時に INVALID_ARGUMENT を返すことの確認
TEST_F(hashtableLayoutTest, rejects_bucket_region_offset_overflow)
{
    // Arrange
    cplat_hashtable_config config = {};
    size_t mgmt_size = 1;
    size_t data_size = 1;

    fill_config(
        &config, SIZE_MAX / sizeof(uint64_t), 8, 8, 5,
        CPLAT_HASHTABLE_KEY_STRING); // [状態] - バケット領域の乗算は成功するが加算が破綻する capacity を用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_hashtable_required_size(&config, &mgmt_size, &data_size); // [手順] - 必要サイズを求める。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - バケット領域オフセット加算のオーバーフローが INVALID_ARGUMENT であること。
}

// cplat_hashtable_required_size がエントリ領域サイズ乗算のオーバーフロー時に INVALID_ARGUMENT を返すことの確認
TEST_F(hashtableLayoutTest, rejects_entries_region_multiplication_overflow)
{
    // Arrange
    cplat_hashtable_config config = {};
    size_t mgmt_size = 1;
    size_t data_size = 1;

    fill_config(
        &config, 3, SIZE_MAX / 2, 8, 5,
        CPLAT_HASHTABLE_KEY_STRING); // [状態] - entry_stride は算出できるが capacity 倍で破綻する組み合わせを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_hashtable_required_size(&config, &mgmt_size, &data_size); // [手順] - 必要サイズを求める。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - エントリ領域サイズ計算のオーバーフローが INVALID_ARGUMENT であること。
}

// cplat_hashtable_required_size がエントリ領域オフセット加算のオーバーフロー時に INVALID_ARGUMENT を返すことの確認
TEST_F(hashtableLayoutTest, rejects_entries_region_offset_overflow)
{
    // Arrange
    cplat_hashtable_config config = {};
    size_t mgmt_size = 1;
    size_t data_size = 1;

    fill_config(
        &config, 1, SIZE_MAX - 50, 8, 5,
        CPLAT_HASHTABLE_KEY_STRING); // [状態] - entry_stride の乗算は成功するが加算が破綻する組み合わせを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_hashtable_required_size(&config, &mgmt_size, &data_size); // [手順] - 必要サイズを求める。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - エントリ領域オフセット加算のオーバーフローが INVALID_ARGUMENT であること。
}

// cplat_hashtable_required_size がデータ領域サイズ乗算のオーバーフロー時に INVALID_ARGUMENT を返すことの確認
TEST_F(hashtableLayoutTest, rejects_data_region_multiplication_overflow)
{
    // Arrange
    cplat_hashtable_config config = {};
    size_t mgmt_size = 1;
    size_t data_size = 1;

    fill_config(&config, 2, 8, SIZE_MAX, 5,
                CPLAT_HASHTABLE_KEY_STRING); // [状態] - データ領域サイズの乗算が破綻する value_size を用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_hashtable_required_size(&config, &mgmt_size, &data_size); // [手順] - 必要サイズを求める。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - データ領域サイズ計算のオーバーフローが INVALID_ARGUMENT であること。
}

// cplat_hashtable_required_size が capacity に 0 を指定した場合に INVALID_ARGUMENT を返すことの確認
TEST_F(hashtableLayoutTest, required_size_rejects_zero_capacity)
{
    // Arrange
    cplat_hashtable_config config = {};
    size_t mgmt_size = 0;
    size_t data_size = 0;

    fill_config(&config, 0, 8, 8, 5,
                CPLAT_HASHTABLE_KEY_STRING); // [状態] - capacity 0 の設定を用意する(create と同じ基準で拒否される)。

    // Pre-Assert

    // Act
    int actual_ret = cplat_hashtable_required_size(&config, &mgmt_size, &data_size); // [手順] - 必要サイズを求める。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - capacity 0 が create と同じ基準で INVALID_ARGUMENT であること。
}

// cplat_hashtable_create が未整列の外部管理バッファーを指定した場合に INVALID_ARGUMENT を返すことの確認
TEST_F(hashtableLayoutTest, create_rejects_misaligned_external_buffer)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = reinterpret_cast<cplat_hashtable *>(1);
    size_t mgmt_needed = 0;
    size_t data_needed = 0;

    fill_config(&config, 2, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - 妥当な設定を用意する。
    (void)cplat_hashtable_required_size(&config, &mgmt_needed, &data_needed);
    std::vector<unsigned char> buf_mgmt(mgmt_needed + 1, 0); // [状態] - 1 バイトずらして未整列にできる領域を用意する。
    std::vector<unsigned char> buf_data(data_needed, 0);

    // Pre-Assert

    // Act
    int actual_ret = cplat_hashtable_create(&config, buf_mgmt.data() + 1, mgmt_needed, buf_data.data(), buf_data.size(),
                                            &ht); // [手順] - 1 バイトずれた管理領域で構築する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret);  // [確認_異常系] - 未整列の外部バッファーが INVALID_ARGUMENT であること。
    EXPECT_EQ(nullptr, ht); // [確認_異常系] - 失敗後の ht_out が NULL であること。
}

// cplat_hashtable_create が内部確保時の管理領域とデータ領域の合計サイズオーバーフローで INVALID_ARGUMENT を返すことの確認
TEST_F(hashtableLayoutTest, create_internal_alloc_rejects_combined_size_overflow)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = reinterpret_cast<cplat_hashtable *>(1);
    size_t mgmt_size = 0;
    size_t data_size = 0;

    fill_config(&config, 1, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING);
    (void)cplat_hashtable_required_size(&config, &mgmt_size, &data_size); // [状態] - 管理領域サイズを求めておく。
    fill_config(&config, 1, 8, SIZE_MAX - mgmt_size + 1, 5,
                CPLAT_HASHTABLE_KEY_STRING); // [状態] - 内部確保の mgmt_size + data_size 加算が破綻する value_size
                                             // を用意する(capacity 1 なので data_size 単体はあふれない)。

    // Pre-Assert

    // Act
    int actual_ret = cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht); // [手順] - 内部確保で構築する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret); // [確認_異常系] - 管理領域とデータ領域の合計サイズのオーバーフローが INVALID_ARGUMENT であること。
    EXPECT_EQ(nullptr, ht); // [確認_異常系] - 失敗後の ht_out が NULL であること。
}

// SCOPE_TABLE 指定時の必要管理領域サイズが SCOPE_RECORD 指定時よりも小さくなることの確認
TEST_F(hashtableLayoutTest, required_size_table_scope_is_smaller_than_record_scope)
{
    // Arrange
    cplat_hashtable_config table_config = {};
    cplat_hashtable_config record_config = {};
    size_t table_mgmt = 0;
    size_t table_data = 0;
    size_t record_mgmt = 0;
    size_t record_data = 0;

    fill_config(&table_config, 4, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING);
    table_config.timestamp_scope = CPLAT_HASHTABLE_TIMESTAMP_SCOPE_TABLE; // [状態] - テーブル横断のみの粒度にする。
    fill_config(&record_config, 4, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING);
    record_config.timestamp_scope = CPLAT_HASHTABLE_TIMESTAMP_SCOPE_RECORD; // [状態] - レコード単位の粒度にする。

    // Pre-Assert

    // Act
    int actual_ret_table = cplat_hashtable_required_size(&table_config, &table_mgmt, &table_data);
    int actual_ret_record = cplat_hashtable_required_size(&record_config, &record_mgmt, &record_data);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_table);  // [確認_正常系] - SCOPE_TABLE の必要サイズが求まること。
    EXPECT_EQ(CPLAT_OK, actual_ret_record); // [確認_正常系] - SCOPE_RECORD の必要サイズが求まること。
    EXPECT_LT(table_mgmt, record_mgmt);     // [確認_正常系] - SCOPE_TABLE の管理領域が厳密に小さいこと。
    EXPECT_EQ(table_data, record_data);     // [確認_正常系] - データ領域サイズは粒度で変わらないこと。
}

// 不正な value_align (非2冪、上限超過、可変長値指定) に対して INVALID_ARGUMENT を返すことの確認
TEST_F(hashtableLayoutTest, rejects_invalid_value_align)
{
    // Arrange
    cplat_hashtable_config not_power_of_two = {};
    cplat_hashtable_config too_large = {};
    cplat_hashtable_config variable_value = {};
    size_t mgmt_size = 0;
    size_t data_size = 0;

    fill_config(&not_power_of_two, 4, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING);
    not_power_of_two.value_align = 6; // [状態] - 2 の冪でない境界を指定する。
    fill_config(&too_large, 4, 8, 8, 5, CPLAT_HASHTABLE_KEY_STRING);
    too_large.value_align = CPLAT_HASHTABLE_VALUE_ALIGN_MAX * 2; // [状態] - 上限を超える境界を指定する。
    fill_config(&variable_value, 4, 8, 0, 5, CPLAT_HASHTABLE_KEY_STRING);
    variable_value.value_type = CPLAT_HASHTABLE_FIELD_VARIABLE_STRING;
    variable_value.value_storage_size = 64;
    variable_value.value_align = 8; // [状態] - 可変長値に 0 以外の境界を指定する。

    // Pre-Assert

    // Act
    int actual_ret_not_power_of_two = cplat_hashtable_required_size(&not_power_of_two, &mgmt_size, &data_size);
    int actual_ret_too_large = cplat_hashtable_required_size(&too_large, &mgmt_size, &data_size);
    int actual_ret_variable_value = cplat_hashtable_required_size(&variable_value, &mgmt_size, &data_size);

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_not_power_of_two); // [確認_異常系] - 2 の冪でない境界が INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_too_large); // [確認_異常系] - 上限を超える境界が INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_variable_value); // [確認_異常系] - 可変長値に境界を指定すると INVALID_ARGUMENT であること。
}

// value_align 指定時にデータ領域が必要幅へ切り上げられ値参照が境界に整列することの確認
TEST_F(hashtableLayoutTest, value_align_rounds_up_data_region_and_aligns_references)
{
    // Arrange
    cplat_hashtable_config packed = {};
    cplat_hashtable_config aligned = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(5, 0);
    size_t packed_data_size = 0;
    size_t aligned_data_size = 0;
    const void *first_ref = nullptr;
    const void *second_ref = nullptr;

    fill_config(&packed, 4, 8, 5, 5, CPLAT_HASHTABLE_KEY_STRING); // [状態] - 5 バイトの値を詰めて並べる設定。
    fill_config(&aligned, 4, 8, 5, 5, CPLAT_HASHTABLE_KEY_STRING);
    aligned.value_align = 8; // [状態] - 5 バイトの値を 8 境界へ整列させる設定。

    // Pre-Assert

    // Act
    int actual_ret_packed_size = cplat_hashtable_required_size(&packed, NULL, &packed_data_size);
    int actual_ret_aligned_size = cplat_hashtable_required_size(&aligned, NULL, &aligned_data_size);
    int actual_ret_create = cplat_hashtable_create(&aligned, NULL, 0, NULL, 0, &ht);
    (void)cplat_hashtable_add(ht, "a", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    (void)cplat_hashtable_add(ht, "b", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    int actual_ret_first = cplat_hashtable_get_value_ref(ht, 1, &first_ref);
    int actual_ret_second = cplat_hashtable_get_value_ref(ht, 2, &second_ref);
    uintptr_t first_addr = reinterpret_cast<uintptr_t>(first_ref);
    uintptr_t second_addr = reinterpret_cast<uintptr_t>(second_ref);

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_packed_size);  // [確認_正常系] - 詰めて並べる設定で必要サイズを求められること。
    EXPECT_EQ(CPLAT_OK, actual_ret_aligned_size); // [確認_正常系] - 整列させる設定で必要サイズを求められること。
    EXPECT_EQ(static_cast<size_t>(4 * 5),
              packed_data_size); // [確認_正常系] - 詰めた場合は value_size の総和であること。
    EXPECT_EQ(static_cast<size_t>(4 * 8),
              aligned_data_size);           // [確認_正常系] - 整列させた場合は境界へ切り上げた幅の総和であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_create); // [確認_正常系] - 整列させる設定で構築できること。
    EXPECT_EQ(CPLAT_OK, actual_ret_first);  // [確認_正常系] - 1 件目の値参照を取れること。
    EXPECT_EQ(CPLAT_OK, actual_ret_second); // [確認_正常系] - 2 件目の値参照を取れること。
    EXPECT_EQ(0u, first_addr % 8u);         // [確認_正常系] - 1 件目の値参照が指定した境界に整列していること。
    EXPECT_EQ(0u, second_addr % 8u);        // [確認_正常系] - 2 件目の値参照が指定した境界に整列していること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュテーブルを破棄する。
}

// cplat_hashtable_create が value_align の境界を満たさない外部データバッファーで INVALID_ARGUMENT を返すことの確認
TEST_F(hashtableLayoutTest, create_rejects_external_data_buffer_misaligned_for_value_align)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    size_t mgmt_size = 0;
    size_t data_size = 0;
    std::vector<uint64_t> mgmt_buf;
    std::vector<unsigned char> data_buf;

    fill_config(&config, 4, 8, 5, 5, CPLAT_HASHTABLE_KEY_STRING);
    config.value_align = 8; // [状態] - 8 境界を要求する設定を用意する。
    (void)cplat_hashtable_required_size(&config, &mgmt_size, &data_size);
    mgmt_buf.assign((mgmt_size / sizeof(uint64_t)) + 1u, 0);
    data_buf.assign(data_size + 8u, 0);

    // Pre-Assert
    ASSERT_NE(0u, data_size); // [Pre-Assert確認_正常系] - データ領域の必要サイズが求まること。

    // Act
    /* 8 境界から 1 バイトずらした位置をデータ領域として渡す。 */
    int actual_ret = cplat_hashtable_create(&config, mgmt_buf.data(), mgmt_size, data_buf.data() + 1, data_size, &ht);

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret);  // [確認_異常系] - 境界を満たさないデータ領域が INVALID_ARGUMENT であること。
    EXPECT_EQ(nullptr, ht); // [確認_異常系] - 失敗時にハンドルが NULL のままであること。
}

// SCOPE_TABLE において可変長キーの記述子が uint64_t 境界に整列することの確認
TEST_F(hashtableLayoutTest, variable_key_descriptor_is_aligned_in_table_scope)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    uintptr_t first_addr = 0;
    uintptr_t second_addr = 0;

    fill_config(&config, 4, 0, 8, 5, CPLAT_HASHTABLE_KEY_STRING);
    config.key_type = CPLAT_HASHTABLE_FIELD_VARIABLE_STRING;
    config.key_storage_size = 64;
    config.timestamp_scope = CPLAT_HASHTABLE_TIMESTAMP_SCOPE_TABLE; // [状態] - 可変長キーとテーブル粒度を組み合わせる。

    // Pre-Assert

    // Act
    int actual_ret_create = cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    (void)cplat_hashtable_add(ht, "alpha", value.data(), CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    first_addr = reinterpret_cast<uintptr_t>(test_hashtable_key_ref_at(ht, 0));
    second_addr = reinterpret_cast<uintptr_t>(test_hashtable_key_ref_at(ht, 1));

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_create); // [確認_正常系] - 可変長キーとテーブル粒度で構築できること。
    EXPECT_EQ(0u,
              first_addr % alignof(uint64_t)); // [確認_正常系] - 1 件目のキー descriptor が uint64_t 境界にあること。
    EXPECT_EQ(0u,
              second_addr % alignof(uint64_t)); // [確認_正常系] - 2 件目のキー descriptor が uint64_t 境界にあること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュテーブルを破棄する。
}

// 8の倍数でないキーストレージ容量指定時でも内部データ領域および値記述子が uint64_t 境界に整列することの確認
TEST_F(hashtableLayoutTest, internal_data_region_is_aligned_after_odd_key_storage)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    uintptr_t data_addr = 0;
    uintptr_t ref_addr = 0;

    fill_config(&config, 4, 0, 0, 5, CPLAT_HASHTABLE_KEY_STRING);
    config.key_type = CPLAT_HASHTABLE_FIELD_VARIABLE_STRING;
    config.value_type = CPLAT_HASHTABLE_FIELD_VARIABLE_STRING;
    config.key_storage_size = 61; // [状態] - 8 の倍数でないキー ストレージ容量にする。
    config.value_storage_size = 64;

    // Pre-Assert

    // Act
    int actual_ret_create = cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    const void *data_ref = nullptr;
    int actual_ret_buffer = cplat_hashtable_buffer_ref(ht, NULL, &data_ref);
    (void)cplat_hashtable_add(ht, "alpha", "v1", CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE);
    data_addr = reinterpret_cast<uintptr_t>(data_ref);
    ref_addr = reinterpret_cast<uintptr_t>(test_hashtable_value_ref_at(ht, 0));

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_create);       // [確認_正常系] - 8 の倍数でないキー ストレージ容量で構築できること。
    EXPECT_EQ(CPLAT_OK, actual_ret_buffer);       // [確認_正常系] - データ領域の先頭を取れること。
    EXPECT_EQ(0u, data_addr % alignof(uint64_t)); // [確認_正常系] - データ領域の先頭が uint64_t 境界にあること。
    EXPECT_EQ(0u, ref_addr % alignof(uint64_t));  // [確認_正常系] - 値 descriptor が uint64_t 境界にあること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - ハッシュテーブルを破棄する。
}
