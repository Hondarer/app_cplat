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

// 各状態グループ（使用中、削除済み、空、およびその組み合わせ）に該当するレコード番号を順に走査取得できることの確認
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

    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);
    (void)cplat_hashtable_insert_direct(ht, 1, "a", 1, value.data(), NULL, 0);
    (void)cplat_hashtable_insert_direct(ht, 2, "b", 2, value.data(), NULL, 0);
    (void)cplat_hashtable_insert_direct(ht, 3, "c", 1, value.data(), NULL, 0);
    /* レコード 4 は空のまま残す。 */

    // Pre-Assert

    // Act
    std::vector<uint64_t> in_use =
        collect(ht, CPLAT_HASHTABLE_SCAN_IN_USE, &ret_in_use); // [手順] - 使用中レコードを走査する。
    std::vector<uint64_t> deleted =
        collect(ht, CPLAT_HASHTABLE_SCAN_DELETED, &ret_deleted); // [手順] - 削除済みレコードを走査する。
    std::vector<uint64_t> empty =
        collect(ht, CPLAT_HASHTABLE_SCAN_EMPTY, &ret_empty); // [手順] - 空レコードを走査する。
    std::vector<uint64_t> all =
        collect(ht, CPLAT_HASHTABLE_SCAN_IN_USE | CPLAT_HASHTABLE_SCAN_DELETED | CPLAT_HASHTABLE_SCAN_EMPTY,
                &ret_all); // [手順] - 全レコードを走査する。

    // Assert
    EXPECT_EQ(CPLAT_OK, ret_in_use);                         // [確認_正常系] - 使用中の走査が成功すること。
    EXPECT_EQ(std::vector<uint64_t>({1u, 3u}), in_use);      // [確認_正常系] - 使用中のレコードだけを昇順に返すこと。
    EXPECT_EQ(CPLAT_OK, ret_deleted);                        // [確認_正常系] - 削除済みの走査が成功すること。
    EXPECT_EQ(std::vector<uint64_t>({2u}), deleted);         // [確認_正常系] - 削除済みのレコードだけを返すこと。
    EXPECT_EQ(CPLAT_OK, ret_empty);                          // [確認_正常系] - 空の走査が成功すること。
    EXPECT_EQ(std::vector<uint64_t>({4u}), empty);           // [確認_正常系] - 空のレコードだけを返すこと。
    EXPECT_EQ(CPLAT_OK, ret_all);                            // [確認_正常系] - 全状態の走査が成功すること。
    EXPECT_EQ(std::vector<uint64_t>({1u, 2u, 3u, 4u}), all); // [確認_正常系] - すべてのレコードを昇順に返すこと。

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
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);

    // Pre-Assert

    // Act
    int actual_ret_empty_table = cplat_hashtable_next_record(ht, 0, CPLAT_HASHTABLE_SCAN_IN_USE, &record,
                                                             &has_record); // [手順] - 空テーブルを走査する。
    int has_record_at_end = -1;
    uint64_t record_at_end = 54321;
    int actual_ret_at_end = cplat_hashtable_next_record(ht, 4, CPLAT_HASHTABLE_SCAN_EMPTY, &record_at_end,
                                                        &has_record_at_end); // [手順] - 終端から走査する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_empty_table); // [確認_正常系] - 該当が無くてもエラーにならないこと。
    EXPECT_EQ(0, has_record);                    // [確認_正常系] - 該当が無いことが出力引数で分かること。
    EXPECT_EQ(12345u, record);                   // [確認_正常系] - 該当が無いとき格納先を書き換えないこと。
    EXPECT_EQ(CPLAT_OK, actual_ret_at_end);      // [確認_正常系] - capacity と等しい位置からの走査が成功すること。
    EXPECT_EQ(0, has_record_at_end);             // [確認_正常系] - 終端では該当が無いこと。
    EXPECT_EQ(54321u, record_at_end);            // [確認_正常系] - 終端でも格納先を書き換えないこと。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - テーブルを破棄する。
}

// 走査関数にNULLポインタ、無効なマスク値、範囲外の開始位置を指定した場合に不正引数エラーとなることの確認
TEST_F(hashtableScanTest, guards_reject_invalid_arguments)
{
    // Arrange
    cplat_hashtable_config config = {};
    cplat_hashtable *ht = nullptr;
    uint64_t record = 0;
    int has_record = 0;

    fill_config(&config, 4, 5);
    (void)cplat_hashtable_create(&config, NULL, 0, NULL, 0, &ht);

    // Pre-Assert

    // Act
    int actual_ret_null_ht = cplat_hashtable_next_record(NULL, 0, CPLAT_HASHTABLE_SCAN_IN_USE, &record,
                                                         &has_record); // [手順] - ht に NULL を渡す。
    int actual_ret_null_record = cplat_hashtable_next_record(ht, 0, CPLAT_HASHTABLE_SCAN_IN_USE, NULL,
                                                             &has_record); // [手順] - record に NULL を渡す。
    int actual_ret_null_has = cplat_hashtable_next_record(ht, 0, CPLAT_HASHTABLE_SCAN_IN_USE, &record,
                                                          NULL); // [手順] - has_record に NULL を渡す。
    int actual_ret_zero_mask =
        cplat_hashtable_next_record(ht, 0, 0, &record, &has_record); // [手順] - マスクに 0 を渡す。
    int actual_ret_unknown_mask =
        cplat_hashtable_next_record(ht, 0, 0x8u, &record, &has_record); // [手順] - 未定義のビットを渡す。
    int actual_ret_from_over = cplat_hashtable_next_record(ht, 5, CPLAT_HASHTABLE_SCAN_IN_USE, &record,
                                                           &has_record); // [手順] - capacity を超える開始位置を渡す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_null_ht); // [確認_異常系] - ht が NULL なら失敗すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_null_record); // [確認_異常系] - レコード番号の格納先が NULL なら失敗すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_null_has); // [確認_異常系] - 該当有無の格納先が NULL なら失敗すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_zero_mask); // [確認_異常系] - マスクが 0 なら失敗すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_unknown_mask); // [確認_異常系] - 未定義のビットを含むマスクが失敗すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_from_over); // [確認_異常系] - capacity を超える開始位置が失敗すること。

    // Cleanup
    cplat_hashtable_dispose(ht); // [破棄] - テーブルを破棄する。
}
