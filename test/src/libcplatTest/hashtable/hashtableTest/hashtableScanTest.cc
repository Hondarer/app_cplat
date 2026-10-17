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

/* status_mask に該当するレコード番号を、走査して順に集める。 */
std::vector<uint64_t> collect(const cplat_hashtable *ht, unsigned int status_mask, int *last_ret)
{
    std::vector<uint64_t> records;
    uint64_t cursor = 0;
    int has_record = 1;

    while (has_record != 0)
    {
        uint64_t record = 0;

        *last_ret = cplat_hashtable_next_record(ht, cursor, status_mask, &record, &has_record);
        if (*last_ret != CPLAT_OK)
        {
            break;
        }
        if (has_record == 0)
        {
            break;
        }
        records.push_back(record);
        cursor = record;
    }
    return records;
}

} // namespace

class hashtableScanTest : public Test
{
  protected:
    NiceMock<Mock_cplat> mock_cplat_;
};

// 各状態グループ (使用中、削除済み、空、およびその組み合わせ) に該当するレコード番号を順に走査取得できることの確認
TEST_F(hashtableScanTest, walks_each_status_group)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    std::vector<unsigned char> value(8, 0);
    int ret_in_use = CPLAT_ERR_UNKNOWN;
    int ret_deleted = CPLAT_ERR_UNKNOWN;
    int ret_empty = CPLAT_ERR_UNKNOWN;
    int ret_all = CPLAT_ERR_UNKNOWN;

    fill_config(&config, 4, CPLAT_HASHTABLE_LIFETIME_INFINITE);
    config.timestamp_scope =
        CPLAT_HASHTABLE_TIMESTAMP_SCOPE_TABLE; // [状態] - 時刻と世代を省いて直接配置できる粒度にする。

    (void)cplat_hashtable_create(&config, nullptr, 0, nullptr, 0, &ht);
    (void)cplat_hashtable_insert_direct(ht, 1, "a", 1, value.data(), nullptr, 0);
    (void)cplat_hashtable_insert_direct(ht, 2, "b", 2, value.data(), nullptr, 0);
    (void)cplat_hashtable_insert_direct(ht, 3, "c", 1, value.data(), nullptr, 0);
    /* レコード 4 は空のまま残す。 */

    // Pre-Assert

    // Act
    std::vector<uint64_t> in_use = collect(ht, CPLAT_HASHTABLE_SCAN_IN_USE,
                                           &ret_in_use); // [手順] - 使用中のレコードを指定して collect() を呼び出す。
    std::vector<uint64_t> deleted = collect(
        ht, CPLAT_HASHTABLE_SCAN_DELETED, &ret_deleted); // [手順] - 削除済みのレコードを指定して collect() を呼び出す。
    std::vector<uint64_t> empty =
        collect(ht, CPLAT_HASHTABLE_SCAN_EMPTY, &ret_empty); // [手順] - 空のレコードを指定して collect() を呼び出す。
    std::vector<uint64_t> all =
        collect(ht, CPLAT_HASHTABLE_SCAN_IN_USE | CPLAT_HASHTABLE_SCAN_DELETED | CPLAT_HASHTABLE_SCAN_EMPTY,
                &ret_all); // [手順] - 使用中、削除済み、空のレコードをすべて指定して collect() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, ret_in_use); // [確認_正常系] - 使用中の走査での ret_in_use が CPLAT_OK であること。
    EXPECT_EQ(
        std::vector<uint64_t>({1u, 3u}),
        in_use); // [確認_正常系] - 使用中を走査した collect() の戻り値が std::vector<uint64_t>({1u, 3u}) と一致すること。(使用中のレコードだけを昇順に返したことの確認)
    EXPECT_EQ(CPLAT_OK, ret_deleted); // [確認_正常系] - 削除済みの走査での ret_deleted が CPLAT_OK であること。
    EXPECT_EQ(
        std::vector<uint64_t>({2u}),
        deleted); // [確認_正常系] - 削除済みを走査した collect() の戻り値が std::vector<uint64_t>({2u}) と一致すること。(削除済みのレコードだけを返したことの確認)
    EXPECT_EQ(CPLAT_OK, ret_empty); // [確認_正常系] - 空の走査での ret_empty が CPLAT_OK であること。
    EXPECT_EQ(
        std::vector<uint64_t>({4u}),
        empty); // [確認_正常系] - 空を走査した collect() の戻り値が std::vector<uint64_t>({4u}) と一致すること。(空のレコードだけを返したことの確認)
    EXPECT_EQ(CPLAT_OK, ret_all); // [確認_正常系] - 全状態の走査での ret_all が CPLAT_OK であること。
    EXPECT_EQ(
        std::vector<uint64_t>({1u, 2u, 3u, 4u}),
        all); // [確認_正常系] - 全状態を走査した collect() の戻り値が std::vector<uint64_t>({1u, 2u, 3u, 4u}) と一致すること。(すべてのレコードを昇順に返したことの確認)

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - テーブルを破棄する。
}

// 該当するレコードがない場合や終端からの走査でエラーにならず終了が通知されることの確認
TEST_F(hashtableScanTest, reports_end_of_scan_without_error)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    uint64_t record = 12345;
    int has_record = -1;

    fill_config(&config, 4, 5); // [状態] - 空のテーブルを用意する。
    (void)cplat_hashtable_create(&config, nullptr, 0, nullptr, 0, &ht);

    // Pre-Assert

    // Act
    int actual_ret_empty_table = cplat_hashtable_next_record(
        ht, 0, CPLAT_HASHTABLE_SCAN_IN_USE, &record,
        &has_record); // [手順] - 空テーブルに対して cplat_hashtable_next_record() を呼び出す。
    int has_record_at_end = -1;
    uint64_t record_at_end = 54321;
    int actual_ret_at_end = cplat_hashtable_next_record(
        ht, 4, CPLAT_HASHTABLE_SCAN_EMPTY, &record_at_end,
        &has_record_at_end); // [手順] - 終端の開始位置を指定して cplat_hashtable_next_record() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_empty_table); // [確認_正常系] - 空テーブルに対する cplat_hashtable_next_record() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0,
              has_record); // [確認_正常系] - has_record が 0 であること。(該当が無いことが出力引数で分かったことの確認)
    EXPECT_EQ(12345u,
              record); // [確認_正常系] - record が 12345 であること。(該当が無いとき格納先を書き換えなかったことの確認)
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_at_end); // [確認_正常系] - capacity と等しい位置からの cplat_hashtable_next_record() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0,
              has_record_at_end); // [確認_正常系] - has_record_at_end が 0 であること。(終端では該当が無いことの確認)
    EXPECT_EQ(
        54321u,
        record_at_end); // [確認_正常系] - record_at_end が 54321 であること。(終端でも格納先を書き換えなかったことの確認)

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - テーブルを破棄する。
}

// 走査関数に NULL ポインター、無効なマスク値、範囲外の開始位置を指定した場合に不正引数エラーとなることの確認
TEST_F(hashtableScanTest, guards_reject_invalid_arguments)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    uint64_t record = 0;
    int has_record = 0;

    fill_config(&config, 4, 5);
    (void)cplat_hashtable_create(&config, nullptr, 0, nullptr, 0, &ht);

    // Pre-Assert

    // Act
    int actual_ret_null_ht = cplat_hashtable_next_record(
        nullptr, 0, CPLAT_HASHTABLE_SCAN_IN_USE, &record,
        &has_record); // [手順] - ht に nullptr を指定して cplat_hashtable_next_record() を呼び出す。
    int actual_ret_null_record = cplat_hashtable_next_record(
        ht, 0, CPLAT_HASHTABLE_SCAN_IN_USE, nullptr,
        &has_record); // [手順] - record に nullptr を指定して cplat_hashtable_next_record() を呼び出す。
    int actual_ret_null_has = cplat_hashtable_next_record(
        ht, 0, CPLAT_HASHTABLE_SCAN_IN_USE, &record,
        nullptr); // [手順] - has_record に nullptr を指定して cplat_hashtable_next_record() を呼び出す。
    int actual_ret_zero_mask = cplat_hashtable_next_record(
        ht, 0, 0, &record, &has_record); // [手順] - マスクに 0 を指定して cplat_hashtable_next_record() を呼び出す。
    int actual_ret_unknown_mask = cplat_hashtable_next_record(
        ht, 0, 0x8u, &record,
        &has_record); // [手順] - 未定義のビットを指定して cplat_hashtable_next_record() を呼び出す。
    int actual_ret_from_over = cplat_hashtable_next_record(
        ht, 5, CPLAT_HASHTABLE_SCAN_IN_USE, &record,
        &has_record); // [手順] - capacity を超える開始位置を指定して cplat_hashtable_next_record() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_ht); // [確認_異常系] - ht に nullptr を渡した cplat_hashtable_next_record() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_record); // [確認_異常系] - record に nullptr を渡した cplat_hashtable_next_record() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_has); // [確認_異常系] - has_record に nullptr を渡した cplat_hashtable_next_record() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_zero_mask); // [確認_異常系] - マスクに 0 を渡した cplat_hashtable_next_record() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_unknown_mask); // [確認_異常系] - 未定義のビットを含むマスクを渡した cplat_hashtable_next_record() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_from_over); // [確認_異常系] - capacity を超える開始位置を渡した cplat_hashtable_next_record() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - テーブルを破棄する。
}
