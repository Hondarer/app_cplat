#include <testfw.h>
#include <mock_cplat.h>

#include "filterTestSupport.h"

#include "filter_test_catalog.h"
#include "gen/filter_test_trace.h"

/* 版番号と署名を書き換えるため、モジュール私有ヘッダーを取り込む */
#include "filter.h"

#include <cplat/base/result.h>
#include <cplat/sync/atomic.h>

#include <cstdint>
#include <cstring>

using namespace filter_test;
using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;

namespace
{
/** テストで使用するソース領域のバイト数です。 */
constexpr std::size_t kSourceSize = CPLAT_STRING_CATALOG_FILTER_SOURCE_SIZE(kLineCapacity, kLineWidth);
} // namespace

class stringCatalogFilterSourceTest : public Test
{
  protected:
    NiceMock<Mock_cplat> mock_cplat;

    /** ソース領域です。版番号をアトミックに読み書きするため、8 バイト境界に置きます。 */
    alignas(8) unsigned char source_[kSourceSize];

    /** 公開するフィルター オブジェクトです。 */
    unsigned char image_[kImageSize];

    /** 組み立てた文字列の格納先です。 */
    char dest_[256];

    cplat_string_catalog_filter_slot *slot_ = nullptr;

    /** テスト用カタログの識別値です。公開で指定します。 */
    uint64_t catalog_id_ = 0U;

    void SetUp() override
    {
        memset(source_, 0, sizeof(source_));
        memset(image_, 0, sizeof(image_));
        ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_get_catalog_id(filter_test_trace_catalog(), &catalog_id_));
        ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_create(
                                filter_test_trace_catalog(), filter_test_trace_key_names(),
                                filter_test_trace_key_name_count(), nullptr, kLineCapacity, kLineWidth, &slot_));
    }

    void TearDown() override
    {
        cplat_string_catalog_filter_slot_dispose(&slot_);
    }

    string_catalog_filter_source_header *header()
    {
        return reinterpret_cast<string_catalog_filter_source_header *>(source_);
    }

    /** 条件式 1 行をコンパイルして、ソース領域へ公開します。 */
    int publish_line(const char *text, uint64_t *revision_out = nullptr)
    {
        int ret = compile_single_line(text, image_);
        if (ret != CPLAT_OK)
        {
            return ret;
        }
        return cplat_string_catalog_filter_source_publish(source_, sizeof(source_), image_, sizeof(image_), catalog_id_,
                                                          nullptr, revision_out);
    }

    /** WARNING の JOB_FAILED を判定付きで組み立て、一致結果を返します。 */
    int format_job_failed(int *matched_out)
    {
        return cplat_string_catalog_filter_slot_format(slot_, dest_, sizeof(dest_), matched_out,
                                                       FILTER_TEST_TRACE_KEY_JOB_FAILED, (uint64_t)5U, 2,
                                                       FILTER_TEST_CONTEXT_ARGS(7));
    }

    cplat_string_catalog_filter_source_status source_status()
    {
        cplat_string_catalog_filter_source_status status;
        memset(&status, 0xFF, sizeof(status));
        EXPECT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_get_source_status(slot_, &status));
        return status;
    }
};

// 公開した条件を、次の判定付きの組み立てで取り込むことの確認
TEST_F(stringCatalogFilterSourceTest, format_takes_published_conditions)
{
    // Arrange
    uint64_t published_revision = 0U;
    int actual_ret;
    int actual_matched = 0;
    ASSERT_EQ(CPLAT_OK, publish_line("category <= 2", &published_revision)); // [状態] - 条件を公開する。
    ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_attach_source(slot_, source_, sizeof(source_),
                                                                       nullptr)); // [状態] - 結び付ける。

    // Pre-Assert

    // Act
    actual_ret = format_job_failed(&actual_matched); // [手順] - JOB_FAILED を組み立てる。

    // Assert
    cplat_string_catalog_filter_source_status status = source_status();
    EXPECT_EQ(CPLAT_OK, actual_ret);                      // [確認_正常系] - 組み立てに成功すること。
    EXPECT_NE(0, actual_matched);                         // [確認_正常系] - 公開した条件で一致すること。
    EXPECT_EQ(published_revision, status.taken_revision); // [確認_正常系] - 版番号を取り込み済みとすること。
    EXPECT_EQ(CPLAT_OK, status.last_result);              // [確認_正常系] - 取り込みの結果が成功であること。
    EXPECT_EQ(0U, status.last_invalid_count);             // [確認_正常系] - 無効にした行がないこと。
}

// 版番号が変わらない場合は、ロックを取らずに判定することの確認
TEST_F(stringCatalogFilterSourceTest, unchanged_revision_skips_lock)
{
    // Arrange
    int actual_matched = 0;
    ASSERT_EQ(CPLAT_OK, publish_line("category <= 2")); // [状態] - 条件を公開する。
    ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_attach_source(slot_, source_, sizeof(source_),
                                                                       nullptr)); // [状態] - 結び付ける。
    ASSERT_EQ(CPLAT_OK, format_job_failed(&actual_matched)); // [状態] - 1 回目の組み立てで取り込む。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_local_lock_try_lock(_))
        .Times(0); // [Pre-Assert確認_正常系] - 取り込みのロックを試みないこと。

    // Act
    actual_matched = 0;
    (void)format_job_failed(&actual_matched); // [手順] - 2 回目の組み立てを行う。

    // Assert
    EXPECT_NE(0, actual_matched); // [確認_正常系] - 取り込み済みの条件で一致すること。
}

// 公開し直した条件を取り込み、版番号が増加することの確認
TEST_F(stringCatalogFilterSourceTest, republished_conditions_replace_previous)
{
    // Arrange
    uint64_t first_revision = 0U;
    uint64_t second_revision = 0U;
    int actual_matched = 0;
    ASSERT_EQ(CPLAT_OK, publish_line("category <= 2", &first_revision)); // [状態] - 1 回目の条件を公開する。
    ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_attach_source(slot_, source_, sizeof(source_),
                                                                       nullptr)); // [状態] - 結び付ける。
    ASSERT_EQ(CPLAT_OK, format_job_failed(&actual_matched));                      // [状態] - 1 回目の条件を取り込む。
    ASSERT_EQ(CPLAT_OK, publish_line("category >= 3", &second_revision));         // [状態] - 2 回目の条件を公開する。

    // Pre-Assert

    // Act
    actual_matched = 1;
    (void)format_job_failed(&actual_matched); // [手順] - JOB_FAILED を組み立てる。

    // Assert
    EXPECT_EQ(2U, first_revision);  // [確認_正常系] - 未公開の領域への最初の公開は 2 であること。
    EXPECT_EQ(4U, second_revision); // [確認_正常系] - 公開のたびに 2 ずつ増えること。
    EXPECT_EQ(0, actual_matched);   // [確認_正常系] - 2 回目の条件で判定すること。
    EXPECT_EQ(second_revision,
              source_status().taken_revision); // [確認_正常系] - 2 回目の版番号を取り込み済みとすること。
}

// 版番号が奇数 (書き込み中) の間は取り込まず、次の公開で回復することの確認
TEST_F(stringCatalogFilterSourceTest, writing_source_is_not_taken_until_next_publish)
{
    // Arrange
    uint64_t published_revision = 0U;
    uint64_t recovered_revision = 0U;
    int actual_matched_while_writing = 1;
    int actual_matched_after_recovery = 0;
    ASSERT_EQ(CPLAT_OK, publish_line("category <= 2", &published_revision)); // [状態] - 条件を公開する。
    cplat_atomic_store_u64(&header()->published_revision, published_revision | 1U,
                           CPLAT_MEMORY_ORDER_RELAXED); // [状態] - 書き込みの途中で中断した状態にする。
    ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_attach_source(slot_, source_, sizeof(source_),
                                                                       nullptr)); // [状態] - 結び付ける。

    // Pre-Assert

    // Act
    (void)format_job_failed(&actual_matched_while_writing); // [手順] - 書き込み中に組み立てる。
    uint64_t taken_while_writing = source_status().taken_revision;
    ASSERT_EQ(CPLAT_OK, publish_line("category <= 2", &recovered_revision)); // [手順] - 公開し直す。
    (void)format_job_failed(&actual_matched_after_recovery);                 // [手順] - 公開後に組み立てる。

    // Assert
    EXPECT_EQ(0, actual_matched_while_writing); // [確認_異常系] - 書き込み中は取り込まず、以前の条件で判定すること。
    EXPECT_EQ(0U, taken_while_writing);         // [確認_異常系] - 書き込み中は取り込み済みとしないこと。
    EXPECT_EQ(published_revision + 2U,
              recovered_revision);               // [確認_正常系] - 中断前の版番号に 2 を加えた値になること。
    EXPECT_NE(0, actual_matched_after_recovery); // [確認_正常系] - 公開し直した条件を取り込むこと。
}

// 複製の間に公開が重なった場合は取り込まず、次の判定で取り込むことの確認
TEST_F(stringCatalogFilterSourceTest, torn_copy_is_discarded_and_retried)
{
    // Arrange
    int actual_matched_torn = 1;
    int actual_matched_retry = 0;
    ASSERT_EQ(CPLAT_OK, publish_line("category <= 2")); // [状態] - 条件を公開する。
    ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_attach_source(slot_, source_, sizeof(source_),
                                                                       nullptr)); // [状態] - 結び付ける。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_local_lock_try_lock(_))
        .WillOnce(Invoke(
            [this](cplat_local_lock *mtx)
            {
                uint64_t revision = cplat_atomic_load_u64(&header()->published_revision, CPLAT_MEMORY_ORDER_RELAXED);
                cplat_atomic_store_u64(&header()->published_revision, revision + 2U, CPLAT_MEMORY_ORDER_RELAXED);
                return delegate_real_cplat_local_lock_try_lock(mtx);
            })) // [Pre-Assert手順] - 1 回目は版番号を読んだ後に、公開が重なった状態を作る。
        .WillRepeatedly(
            Invoke(delegate_real_cplat_local_lock_try_lock)); // [Pre-Assert手順] - 2 回目以降は実関数を呼ぶ。

    // Act
    (void)format_job_failed(&actual_matched_torn); // [手順] - 公開が重なった状態で組み立てる。
    uint64_t taken_after_torn = source_status().taken_revision;
    (void)format_job_failed(&actual_matched_retry); // [手順] - もう一度組み立てる。

    // Assert
    EXPECT_EQ(0, actual_matched_torn);  // [確認_異常系] - 重なった複製を適用せず、以前の条件で判定すること。
    EXPECT_EQ(0U, taken_after_torn);    // [確認_異常系] - 重なった版番号を取り込み済みとしないこと。
    EXPECT_NE(0, actual_matched_retry); // [確認_正常系] - 次の判定で取り込むこと。
}

// ほかのスレッドが取り込み中の場合は待たずに、適用済みの条件で判定することの確認
TEST_F(stringCatalogFilterSourceTest, busy_lock_skips_take_without_waiting)
{
    // Arrange
    int actual_ret;
    int actual_matched = 1;
    ASSERT_EQ(CPLAT_OK, publish_line("category <= 2")); // [状態] - 条件を公開する。
    ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_attach_source(slot_, source_, sizeof(source_),
                                                                       nullptr)); // [状態] - 結び付ける。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_local_lock_try_lock(_))
        .WillOnce(Return(CPLAT_ERR_BUSY)); // [Pre-Assert手順] - 取り込みのロックで CPLAT_ERR_BUSY を返却する。
    EXPECT_CALL(mock_cplat, cplat_local_lock_lock(_, _)).Times(0); // [Pre-Assert確認_正常系] - ロックを待たないこと。

    // Act
    actual_ret = format_job_failed(&actual_matched); // [手順] - JOB_FAILED を組み立てる。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 組み立てに成功すること。
    EXPECT_EQ(0, actual_matched);    // [確認_正常系] - 適用済みの条件で判定すること。
}

// 検証に失敗する公開内容は、結果を記録して繰り返し取り込まないことの確認
TEST_F(stringCatalogFilterSourceTest, corrupt_publication_is_recorded_and_not_retried)
{
    // Arrange
    uint64_t published_revision = 0U;
    int actual_matched = 0;
    ASSERT_EQ(CPLAT_OK, publish_line("category <= 2", &published_revision)); // [状態] - 条件を公開する。
    ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_attach_source(slot_, source_, sizeof(source_),
                                                                       nullptr)); // [状態] - 結び付ける。
    ASSERT_EQ(CPLAT_OK, format_job_failed(&actual_matched));                      // [状態] - 正しい条件を取り込む。
    source_[CPLAT_STRING_CATALOG_FILTER_SOURCE_HEADER_SIZE + CPLAT_STRING_CATALOG_FILTER_HEADER_SIZE +
            CPLAT_STRING_CATALOG_FILTER_RECORD_HEADER_SIZE] ^= 0xFFU; // [状態] - 行レコードを壊す。
    cplat_atomic_store_u64(&header()->published_revision, published_revision + 2U,
                           CPLAT_MEMORY_ORDER_RELEASE); // [状態] - 壊れた内容を公開済みにする。

    // Pre-Assert

    // Act
    actual_matched = 0;
    (void)format_job_failed(&actual_matched); // [手順] - 壊れた公開内容で組み立てる。
    cplat_string_catalog_filter_source_status status = source_status();
    EXPECT_CALL(mock_cplat, cplat_local_lock_try_lock(_))
        .Times(0);                            // [確認_異常系] - 同じ公開内容の取り込みを試みないこと。
    (void)format_job_failed(&actual_matched); // [手順] - もう一度組み立てる。

    // Assert
    EXPECT_NE(0, actual_matched);                                // [確認_異常系] - 以前の条件を維持すること。
    EXPECT_EQ(published_revision + 2U, status.taken_revision);   // [確認_異常系] - 試みた版番号を記録すること。
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR, status.last_result); // [確認_異常系] - 検証の失敗を記録すること。
}

// 結び付けで、アラインメントと大きさを確認し、NULL で解除できることの確認
TEST_F(stringCatalogFilterSourceTest, attach_validates_region_and_detaches_with_null)
{
    // Arrange
    int actual_misaligned_ret;
    int actual_small_ret;
    int actual_detach_ret;
    int actual_matched = 1;
    ASSERT_EQ(CPLAT_OK, publish_line("category <= 2")); // [状態] - 条件を公開する。

    // Pre-Assert

    // Act
    actual_misaligned_ret = cplat_string_catalog_filter_slot_attach_source(
        slot_, source_ + 4, sizeof(source_) - 8U, nullptr); // [手順] - 境界に合わない領域を結び付ける。
    actual_small_ret = cplat_string_catalog_filter_slot_attach_source(slot_, source_, sizeof(source_) - 1U,
                                                                      nullptr); // [手順] - 小さい領域を結び付ける。
    ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_attach_source(slot_, source_, sizeof(source_),
                                                                       nullptr)); // [手順] - 正しい領域を結び付ける。
    actual_detach_ret =
        cplat_string_catalog_filter_slot_attach_source(slot_, nullptr, 0U, nullptr); // [手順] - 解除する。
    (void)format_job_failed(&actual_matched);                                        // [手順] - 解除後に組み立てる。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_misaligned_ret); // [確認_異常系] - 境界に合わない領域を拒否すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_small_ret);      // [確認_異常系] - 小さい領域を拒否すること。
    EXPECT_EQ(CPLAT_OK, actual_detach_ret);                       // [確認_正常系] - 解除できること。
    EXPECT_EQ(0, actual_matched);                                 // [確認_正常系] - 解除後は取り込まないこと。
}

// 公開で、不正な入力と異なる形式の領域を拒否し、領域を変更しないことの確認
TEST_F(stringCatalogFilterSourceTest, publish_rejects_invalid_input_without_change)
{
    // Arrange
    unsigned char expected_source[kSourceSize];
    int actual_null_ret;
    int actual_small_ret;
    int actual_corrupt_image_ret;
    int actual_foreign_ret;
    ASSERT_EQ(CPLAT_OK, compile_single_line("category <= 2", image_)); // [状態] - 条件をコンパイルする。
    header()->signature = 0x12345678U;                                 // [状態] - 異なる形式の署名を置く。
    memcpy(expected_source, source_, sizeof(source_));

    // Pre-Assert

    // Act
    actual_null_ret = cplat_string_catalog_filter_source_publish(nullptr, sizeof(source_), image_, sizeof(image_),
                                                                 catalog_id_, nullptr,
                                                                 nullptr); // [手順] - 領域に NULL を指定する。
    actual_small_ret = cplat_string_catalog_filter_source_publish(source_, sizeof(source_) - 1U, image_, sizeof(image_),
                                                                  catalog_id_, nullptr,
                                                                  nullptr); // [手順] - 小さい領域を指定する。
    image_[CPLAT_STRING_CATALOG_FILTER_HEADER_SIZE] ^= 0xFFU;
    actual_corrupt_image_ret =
        cplat_string_catalog_filter_source_publish(source_, sizeof(source_), image_, sizeof(image_), catalog_id_,
                                                   nullptr, nullptr); // [手順] - 壊れたイメージを公開する。
    image_[CPLAT_STRING_CATALOG_FILTER_HEADER_SIZE] ^= 0xFFU;
    actual_foreign_ret = cplat_string_catalog_filter_source_publish(source_, sizeof(source_), image_, sizeof(image_),
                                                                    catalog_id_, nullptr,
                                                                    nullptr); // [手順] - 異なる形式の領域へ公開する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_null_ret);            // [確認_異常系] - NULL を拒否すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_small_ret);           // [確認_異常系] - 小さい領域を拒否すること。
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR, actual_corrupt_image_ret); // [確認_異常系] - 壊れたイメージを拒否すること。
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR, actual_foreign_ret);     // [確認_異常系] - 異なる形式の領域を拒否すること。
    EXPECT_EQ(0, memcmp(expected_source, source_, sizeof(source_))); // [確認_異常系] - 領域を変更しないこと。
}

// 公開の情報を、未公開、公開済み、書き込み中、異なる形式の各状態で返すことの確認
TEST_F(stringCatalogFilterSourceTest, get_info_reports_each_state)
{
    // Arrange
    cplat_string_catalog_filter_source_info actual_unpublished;
    cplat_string_catalog_filter_source_info actual_published;
    cplat_string_catalog_filter_source_info actual_writing;
    cplat_string_catalog_filter_source_info actual_foreign;
    uint64_t published_revision = 0U;
    int actual_unpublished_ret;
    int actual_published_ret;
    int actual_writing_ret;
    int actual_foreign_ret;

    // Pre-Assert

    // Act
    actual_unpublished_ret =
        cplat_string_catalog_filter_source_get_info(source_, sizeof(source_),
                                                    &actual_unpublished);    // [手順] - 未公開の情報を読む。
    ASSERT_EQ(CPLAT_OK, publish_line("category <= 2", &published_revision)); // [手順] - 条件を公開する。
    actual_published_ret =
        cplat_string_catalog_filter_source_get_info(source_, sizeof(source_),
                                                    &actual_published); // [手順] - 公開済みの情報を読む。
    cplat_atomic_store_u64(&header()->published_revision, published_revision | 1U, CPLAT_MEMORY_ORDER_RELAXED);
    actual_writing_ret =
        cplat_string_catalog_filter_source_get_info(source_, sizeof(source_),
                                                    &actual_writing); // [手順] - 書き込み中の情報を読む。
    cplat_atomic_store_u64(&header()->published_revision, published_revision, CPLAT_MEMORY_ORDER_RELAXED);
    header()->signature = 0x12345678U;
    actual_foreign_ret =
        cplat_string_catalog_filter_source_get_info(source_, sizeof(source_),
                                                    &actual_foreign); // [手順] - 異なる形式の情報を読む。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_unpublished_ret);          // [確認_正常系] - 未公開でも成功すること。
    EXPECT_EQ(0U, actual_unpublished.published_revision); // [確認_正常系] - 未公開の版番号が 0 であること。
    EXPECT_EQ(CPLAT_OK, actual_published_ret);            // [確認_正常系] - 公開済みの情報を読めること。
    EXPECT_EQ(published_revision, actual_published.published_revision);  // [確認_正常系] - 版番号を返すこと。
    EXPECT_EQ((uint32_t)kLineCapacity, actual_published.line_capacity);  // [確認_正常系] - 行数の上限を返すこと。
    EXPECT_EQ((uint32_t)kLineWidth, actual_published.line_width);        // [確認_正常系] - 行幅を返すこと。
    EXPECT_NE(0U, actual_published.publisher_process_id);                // [確認_正常系] - プロセス ID を返すこと。
    EXPECT_NE(0, (long long)actual_published.published_realtime.tv_sec); // [確認_正常系] - 実時刻を返すこと。
    EXPECT_EQ(CPLAT_ERR_BUSY, actual_writing_ret); // [確認_異常系] - 書き込み中は CPLAT_ERR_BUSY を返すこと。
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR, actual_foreign_ret); // [確認_異常系] - 異なる形式を拒否すること。
}

// 前回の版番号が大きい領域 (再起動を越えて残ったファイルのマップ) でも、版番号が前回の値から増え続けることの確認
TEST_F(stringCatalogFilterSourceTest, publish_continues_from_carried_over_revision)
{
    // Arrange
    const uint64_t carried_over = UINT64_C(0x4000000000000000);
    uint64_t actual_revision = 0U;
    int actual_matched = 0;
    ASSERT_EQ(CPLAT_OK, publish_line("category >= 3")); // [状態] - 以前の条件を公開する。
    cplat_atomic_store_u64(&header()->published_revision, carried_over,
                           CPLAT_MEMORY_ORDER_RELAXED); // [状態] - 前回の版番号を大きな値にする。
    ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_attach_source(slot_, source_, sizeof(source_),
                                                                       nullptr)); // [状態] - 結び付ける。

    // Pre-Assert

    // Act
    ASSERT_EQ(CPLAT_OK, publish_line("category <= 2", &actual_revision)); // [手順] - 新しい条件を公開する。
    (void)format_job_failed(&actual_matched);                             // [手順] - JOB_FAILED を組み立てる。

    // Assert
    EXPECT_EQ(carried_over + 2U, actual_revision); // [確認_正常系] - 前回の値に 2 を加えた値になること。
    EXPECT_NE(0, actual_matched);                  // [確認_正常系] - 新しい条件を取り込むこと。
    EXPECT_EQ(actual_revision,
              source_status().taken_revision); // [確認_正常系] - 新しい版番号を取り込み済みとすること。
}

// 版番号が上限に達した領域では、0 でない最小の偶数へ戻り、読み取り側が取り込むことの確認
// 読み取り側は上限の手前の版番号を取り込み済みとし、戻った値と一致しないようにする
TEST_F(stringCatalogFilterSourceTest, publish_wraps_around_at_upper_limit)
{
    const uint64_t limits[] = {UINT64_MAX - 1U, UINT64_MAX};

    for (const uint64_t limit : limits)
    {
        SCOPED_TRACE(limit);

        // Arrange
        uint64_t actual_revision = 0U;
        int actual_matched = 0;
        memset(source_, 0, sizeof(source_));
        cplat_atomic_store_u64(&header()->published_revision, UINT64_MAX - 5U,
                               CPLAT_MEMORY_ORDER_RELAXED); // [状態] - 上限の手前の版番号にする。
        ASSERT_EQ(CPLAT_OK, publish_line("category >= 3")); // [状態] - 以前の条件を上限の手前の版番号で公開する。
        ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_attach_source(slot_, source_, sizeof(source_),
                                                                           nullptr)); // [状態] - 結び付ける。
        ASSERT_EQ(CPLAT_OK, format_job_failed(&actual_matched));                      // [状態] - 以前の条件を取り込む。
        cplat_atomic_store_u64(
            &header()->published_revision, limit,
            CPLAT_MEMORY_ORDER_RELAXED); // [状態] - 版番号を上限 (偶数) または上限で書き込み中 (奇数) にする。

        // Pre-Assert

        // Act
        ASSERT_EQ(CPLAT_OK, publish_line("category <= 2", &actual_revision)); // [手順] - 新しい条件を公開する。
        actual_matched = 0;
        (void)format_job_failed(&actual_matched); // [手順] - JOB_FAILED を組み立てる。

        // Assert
        EXPECT_EQ(2U, actual_revision); // [確認_正常系] - 未公開を表す 0 を避け、最小の偶数 2 へ戻ること。
        EXPECT_NE(0, actual_matched);   // [確認_正常系] - 戻った版番号の条件を取り込むこと。
        EXPECT_EQ(actual_revision,
                  source_status().taken_revision); // [確認_正常系] - 戻った版番号を取り込み済みとすること。
    }
}

// 形式版や大きさが異なるヘッダーの公開内容は、フィルター オブジェクトを読まずに記録することの確認
// 形式版の不一致は CPLAT_ERR_VERSION_MISMATCH、そのほかの不一致は CPLAT_ERR_CORRUPT_DESCRIPTOR で区別する
TEST_F(stringCatalogFilterSourceTest, foreign_header_is_recorded_without_taking)
{
    struct header_change
    {
        const char *label;
        void (*apply)(string_catalog_filter_source_header *);
        int expected_result;
        unsigned int pad;
    };
    const header_change changes[] = {
        {"format_version", [](string_catalog_filter_source_header *h) { h->format_version = 2U; },
         CPLAT_ERR_VERSION_MISMATCH, 0U},
        {"header_size", [](string_catalog_filter_source_header *h) { h->header_size = 32U; },
         CPLAT_ERR_CORRUPT_DESCRIPTOR, 0U},
        {"line_width", [](string_catalog_filter_source_header *h) { h->line_width = h->line_width + 8U; },
         CPLAT_ERR_CORRUPT_DESCRIPTOR, 0U},
    };

    for (const header_change &change : changes)
    {
        SCOPED_TRACE(change.label);

        // Arrange
        uint64_t published_revision = 0U;
        int actual_matched = 1;
        memset(source_, 0, sizeof(source_));
        ASSERT_EQ(CPLAT_OK, publish_line("category <= 2", &published_revision)); // [状態] - 条件を公開する。
        change.apply(header()); // [状態] - ヘッダーを異なる版や大きさに書き換える。
        ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_attach_source(slot_, source_, sizeof(source_),
                                                                           nullptr)); // [状態] - 結び付ける。

        // Pre-Assert

        // Act
        (void)format_job_failed(&actual_matched); // [手順] - JOB_FAILED を組み立てる。

        // Assert
        cplat_string_catalog_filter_source_status status = source_status();
        EXPECT_EQ(0, actual_matched); // [確認_異常系] - 取り込まず、以前の条件で判定すること。
        EXPECT_EQ(published_revision,
                  status.taken_revision);                      // [確認_異常系] - 版番号を記録し、繰り返し試みないこと。
        EXPECT_EQ(change.expected_result, status.last_result); // [確認_異常系] - 不一致の種類を記録すること。
    }
}

// 署名が一致して形式版だけが異なる領域は、公開と情報の読み取りで CPLAT_ERR_VERSION_MISMATCH を返すことの確認
TEST_F(stringCatalogFilterSourceTest, other_format_version_region_reports_version_mismatch)
{
    // Arrange
    unsigned char expected_source[sizeof(source_)];
    cplat_string_catalog_filter_source_info actual_info;
    ASSERT_EQ(CPLAT_OK, publish_line("category <= 2")); // [状態] - 条件を公開する。
    header()->format_version = 2U;                      // [状態] - 異なる形式版の領域にする。
    memcpy(expected_source, source_, sizeof(source_));

    // Pre-Assert

    // Act
    int actual_publish_ret = publish_line("category >= 3"); // [手順] - 異なる形式版の領域へ公開する。
    int actual_info_ret = cplat_string_catalog_filter_source_get_info(source_, sizeof(source_),
                                                                      &actual_info); // [手順] - 情報を読む。

    // Assert
    EXPECT_EQ(CPLAT_ERR_VERSION_MISMATCH, actual_publish_ret);       // [確認_異常系] - 公開が版の不一致を返すこと。
    EXPECT_EQ(0, memcmp(expected_source, source_, sizeof(source_))); // [確認_異常系] - 領域を変更しないこと。
    EXPECT_EQ(CPLAT_ERR_VERSION_MISMATCH, actual_info_ret); // [確認_異常系] - 情報の読み取りも版の不一致を返すこと。
}

namespace
{
/** 書き込み側の排他の代わりに、取得と解放の回数を数え、取得の時点で任意の処理を行います。 */
struct counting_lock
{
    int lock_count;
    int unlock_count;
    int lock_result;
    int pad; /**< 明示的アラインメントです。 */
    void (*on_lock)(void *);
    void *on_lock_context;
};

int counting_lock_acquire(void *context)
{
    counting_lock *lock = static_cast<counting_lock *>(context);

    lock->lock_count++;
    if ((lock->lock_result == CPLAT_OK) && (lock->on_lock != nullptr))
    {
        lock->on_lock(lock->on_lock_context);
    }
    return lock->lock_result;
}

void counting_lock_release(void *context)
{
    static_cast<counting_lock *>(context)->unlock_count++;
}
} // namespace

class stringCatalogFilterLockedSourceTest : public stringCatalogFilterSourceTest
{
  protected:
    counting_lock counter_ = {0, 0, CPLAT_OK, 0, nullptr, nullptr};
    cplat_string_catalog_filter_source_lock lock_ = {counting_lock_acquire, counting_lock_release, &counter_};

    int attach_with_lock()
    {
        return cplat_string_catalog_filter_slot_attach_source(slot_, source_, sizeof(source_), &lock_);
    }

    /** 排他を取った時点で呼び出し、待つ間に終わった公開として別の条件を公開します。 */
    static void publish_while_waiting(void *context)
    {
        (void)static_cast<stringCatalogFilterLockedSourceTest *>(context)->publish_line("category >= 3");
    }
};

// 書き込み側の排他を結び付けた場合、変化を検知したときだけ排他を取って取り込むことの確認
TEST_F(stringCatalogFilterLockedSourceTest, takes_under_writer_lock_only_when_changed)
{
    // Arrange
    int actual_first_matched = 0;
    int actual_second_matched = 0;
    ASSERT_EQ(CPLAT_OK, publish_line("category <= 2")); // [状態] - 条件を公開する。
    ASSERT_EQ(CPLAT_OK, attach_with_lock());            // [状態] - 書き込み側の排他とともに結び付ける。

    // Pre-Assert

    // Act
    (void)format_job_failed(&actual_first_matched);  // [手順] - 1 回目の組み立てを行う。
    (void)format_job_failed(&actual_second_matched); // [手順] - 版番号が変わらないまま 2 回目の組み立てを行う。

    // Assert
    EXPECT_NE(0, actual_first_matched);  // [確認_正常系] - 公開した条件を取り込むこと。
    EXPECT_NE(0, actual_second_matched); // [確認_正常系] - 取り込み済みの条件で判定すること。
    EXPECT_EQ(1, counter_.lock_count);   // [確認_正常系] - 変化を検知した 1 回目だけ排他を取ること。
    EXPECT_EQ(1, counter_.unlock_count); // [確認_正常系] - 取った排他を解放すること。
}

// 排他を待つ間に公開が進んだ場合、排他の下で読み直した新しい版番号を取り込むことの確認
TEST_F(stringCatalogFilterLockedSourceTest, rechecks_revision_under_writer_lock)
{
    // Arrange
    uint64_t first_revision = 0U;
    int actual_matched = 1;
    ASSERT_EQ(CPLAT_OK, publish_line("category <= 2", &first_revision)); // [状態] - 1 回目の条件を公開する。
    ASSERT_EQ(CPLAT_OK, attach_with_lock()); // [状態] - 書き込み側の排他とともに結び付ける。
    counter_.on_lock =
        publish_while_waiting; // [状態] - 排他を取った時点で、待つ間に終わった公開として 2 回目の条件を公開する。
    counter_.on_lock_context = this;

    // Pre-Assert

    // Act
    (void)format_job_failed(&actual_matched); // [手順] - JOB_FAILED を組み立てる。

    // Assert
    cplat_string_catalog_filter_source_status status = source_status();
    EXPECT_EQ(0, actual_matched);                     // [確認_正常系] - 2 回目の条件で判定すること。
    EXPECT_GT(status.taken_revision, first_revision); // [確認_正常系] - 読み直した新しい版番号を取り込むこと。
    cplat_string_catalog_filter_source_info info;
    ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_source_get_info(source_, sizeof(source_), &info));
    EXPECT_EQ(info.published_revision, status.taken_revision); // [確認_正常系] - 最新の版番号と一致すること。
}

// 排他の下で書き込み中 (書き込み側が途中で停止した状態) が見えた場合は取り込まず、排他を解放することの確認
TEST_F(stringCatalogFilterLockedSourceTest, interrupted_write_seen_under_lock_is_not_taken)
{
    // Arrange
    int actual_matched = 1;
    ASSERT_EQ(CPLAT_OK, publish_line("category <= 2")); // [状態] - 条件を公開する。
    ASSERT_EQ(CPLAT_OK, attach_with_lock());            // [状態] - 書き込み側の排他とともに結び付ける。
    counter_.on_lock = [](void *context)
    {
        string_catalog_filter_source_header *h = static_cast<string_catalog_filter_source_header *>(context);
        uint64_t revision = cplat_atomic_load_u64(&h->published_revision, CPLAT_MEMORY_ORDER_RELAXED);
        cplat_atomic_store_u64(&h->published_revision, revision | 1U, CPLAT_MEMORY_ORDER_RELAXED);
    }; // [状態] - 排他を取った時点で、書き込みの途中で停止した状態にする。
    counter_.on_lock_context = header();

    // Pre-Assert

    // Act
    (void)format_job_failed(&actual_matched); // [手順] - JOB_FAILED を組み立てる。

    // Assert
    EXPECT_EQ(0, actual_matched);                          // [確認_異常系] - 取り込まず、以前の条件で判定すること。
    EXPECT_EQ(0U, source_status().taken_revision);         // [確認_異常系] - 取り込み済みとしないこと。
    EXPECT_EQ(counter_.lock_count, counter_.unlock_count); // [確認_異常系] - 取った排他を解放すること。
}

// 書き込み側の排他を取得できない場合は取り込まず、結果を記録して次の判定で改めて試みることの確認
TEST_F(stringCatalogFilterLockedSourceTest, lock_failure_is_recorded_and_retried)
{
    // Arrange
    uint64_t published = 0U;
    int actual_failed_matched = 1;
    int actual_retry_matched = 0;
    ASSERT_EQ(CPLAT_OK, publish_line("category <= 2", &published)); // [状態] - 条件を公開する。
    ASSERT_EQ(CPLAT_OK, attach_with_lock());                        // [状態] - 書き込み側の排他とともに結び付ける。
    counter_.lock_result = CPLAT_ERR_TIMEOUT;                       // [状態] - 排他の取得が失敗するようにする。

    // Pre-Assert

    // Act
    (void)format_job_failed(&actual_failed_matched); // [手順] - 排他を取得できない状態で組み立てる。
    cplat_string_catalog_filter_source_status failed_status = source_status();
    counter_.lock_result = CPLAT_OK;                // [手順] - 排他を取得できる状態に戻す。
    (void)format_job_failed(&actual_retry_matched); // [手順] - もう一度組み立てる。

    // Assert
    EXPECT_EQ(0, actual_failed_matched);                     // [確認_異常系] - 取り込まず、以前の条件で判定すること。
    EXPECT_EQ(0U, failed_status.taken_revision);             // [確認_異常系] - 取り込み済みとしないこと。
    EXPECT_EQ(CPLAT_ERR_TIMEOUT, failed_status.last_result); // [確認_異常系] - 排他の取得の結果コードを記録すること。
    EXPECT_EQ(1, counter_.unlock_count);                     // [確認_異常系] - 取得できなかった排他は解放しないこと。
    EXPECT_NE(0, actual_retry_matched);                      // [確認_正常系] - 次の判定で取り込むこと。
    EXPECT_EQ(published, source_status().taken_revision);    // [確認_正常系] - 版番号を取り込み済みとすること。
}

// 関数が NULL の排他を結び付けられないことの確認
TEST_F(stringCatalogFilterLockedSourceTest, attach_rejects_incomplete_lock)
{
    // Arrange
    cplat_string_catalog_filter_source_lock without_unlock = {counting_lock_acquire, nullptr, &counter_};
    cplat_string_catalog_filter_source_lock without_lock = {nullptr, counting_lock_release, &counter_};

    // Pre-Assert

    // Act
    int actual_without_unlock =
        cplat_string_catalog_filter_slot_attach_source(slot_, source_, sizeof(source_),
                                                       &without_unlock); // [手順] - 解放の関数がない排他を結び付ける。
    int actual_without_lock =
        cplat_string_catalog_filter_slot_attach_source(slot_, source_, sizeof(source_),
                                                       &without_lock); // [手順] - 取得の関数がない排他を結び付ける。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_without_unlock);                           // [確認_異常系] - 解放の関数がない排他を拒否すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_without_lock); // [確認_異常系] - 取得の関数がない排他を拒否すること。
}

// 公開に排他を渡した場合、書き込みの間だけ 1 回取得して解放することの確認
TEST_F(stringCatalogFilterLockedSourceTest, publish_takes_lock_once_while_writing)
{
    // Arrange
    uint64_t actual_revision = 0U;
    ASSERT_EQ(CPLAT_OK, compile_single_line("category <= 2", image_)); // [状態] - 条件をコンパイルする。

    // Pre-Assert

    // Act
    int actual_ret = cplat_string_catalog_filter_source_publish(source_, sizeof(source_), image_, sizeof(image_),
                                                                catalog_id_, &lock_,
                                                                &actual_revision); // [手順] - 排他とともに公開する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret);     // [確認_正常系] - 公開に成功すること。
    EXPECT_NE(0U, actual_revision);      // [確認_正常系] - 版番号を格納すること。
    EXPECT_EQ(1, counter_.lock_count);   // [確認_正常系] - 排他を 1 回取得すること。
    EXPECT_EQ(1, counter_.unlock_count); // [確認_正常系] - 取得した排他を解放すること。
}

// 公開で排他を取得できない場合は、その結果コードを返し、領域を変更しないことの確認
TEST_F(stringCatalogFilterLockedSourceTest, publish_lock_failure_keeps_region)
{
    // Arrange
    unsigned char expected_source[sizeof(source_)];
    ASSERT_EQ(CPLAT_OK, compile_single_line("category <= 2", image_)); // [状態] - 条件をコンパイルする。
    counter_.lock_result = CPLAT_ERR_TIMEOUT;                          // [状態] - 排他の取得が失敗するようにする。
    memcpy(expected_source, source_, sizeof(source_));

    // Pre-Assert

    // Act
    int actual_ret = cplat_string_catalog_filter_source_publish(source_, sizeof(source_), image_, sizeof(image_),
                                                                catalog_id_, &lock_,
                                                                nullptr); // [手順] - 排他とともに公開する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_TIMEOUT, actual_ret); // [確認_異常系] - 排他の取得の結果コードを返すこと。
    EXPECT_EQ(0, counter_.unlock_count);      // [確認_異常系] - 取得できなかった排他は解放しないこと。
    EXPECT_EQ(0, memcmp(expected_source, source_, sizeof(source_))); // [確認_異常系] - 領域を変更しないこと。
}

// 公開で異なる形式の領域を拒否した場合も、取得した排他を解放することの確認
TEST_F(stringCatalogFilterLockedSourceTest, publish_rejecting_foreign_region_releases_lock)
{
    // Arrange
    ASSERT_EQ(CPLAT_OK, compile_single_line("category <= 2", image_)); // [状態] - 条件をコンパイルする。
    header()->signature = 0x12345678U;                                 // [状態] - 異なる形式の署名を置く。

    // Pre-Assert

    // Act
    int actual_ret = cplat_string_catalog_filter_source_publish(source_, sizeof(source_), image_, sizeof(image_),
                                                                catalog_id_, &lock_,
                                                                nullptr); // [手順] - 排他とともに公開する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_CORRUPT_DESCRIPTOR, actual_ret); // [確認_異常系] - 異なる形式の領域を拒否すること。
    EXPECT_EQ(1, counter_.lock_count);                   // [確認_異常系] - ヘッダーの確認のために排他を取得すること。
    EXPECT_EQ(1, counter_.unlock_count);                 // [確認_異常系] - 取得した排他を解放すること。
}

// 公開で関数が NULL の排他を拒否することの確認
TEST_F(stringCatalogFilterLockedSourceTest, publish_rejects_incomplete_lock)
{
    // Arrange
    cplat_string_catalog_filter_source_lock without_unlock = {counting_lock_acquire, nullptr, &counter_};
    ASSERT_EQ(CPLAT_OK, compile_single_line("category <= 2", image_)); // [状態] - 条件をコンパイルする。

    // Pre-Assert

    // Act
    int actual_ret = cplat_string_catalog_filter_source_publish(source_, sizeof(source_), image_, sizeof(image_),
                                                                catalog_id_, &without_unlock,
                                                                nullptr); // [手順] - 解放の関数がない排他で公開する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret); // [確認_異常系] - 関数が欠けた排他を拒否すること。
    EXPECT_EQ(0, counter_.lock_count);                 // [確認_異常系] - 排他を取得しないこと。
}

// カタログの識別値は同じ定義で同じ値になり、異なるカタログでは異なる値になることの確認
TEST_F(stringCatalogFilterSourceTest, catalog_id_is_stable_and_distinguishes_catalogs)
{
    // Arrange
    uint64_t actual_again = 0U;
    uint64_t actual_other = 0U;

    // Pre-Assert

    // Act
    int actual_again_ret = cplat_string_catalog_filter_get_catalog_id(
        filter_test_trace_catalog(), &actual_again); // [手順] - 同じカタログで求め直す。
    int actual_other_ret = cplat_string_catalog_filter_get_catalog_id(filter_test_catalog(),
                                                                      &actual_other); // [手順] - 別のカタログで求める。
    int actual_null_ret =
        cplat_string_catalog_filter_get_catalog_id(nullptr, &actual_other); // [手順] - NULL を指定する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_again_ret);                  // [確認_正常系] - 求められること。
    EXPECT_EQ(CPLAT_OK, actual_other_ret);                  // [確認_正常系] - 求められること。
    EXPECT_NE(0U, catalog_id_);                             // [確認_正常系] - 0 にならないこと。
    EXPECT_EQ(catalog_id_, actual_again);                   // [確認_正常系] - 同じ定義で同じ値になること。
    EXPECT_NE(catalog_id_, actual_other);                   // [確認_正常系] - 異なるカタログで異なる値になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_null_ret); // [確認_異常系] - NULL を拒否すること。
}

// 公開でカタログの識別値に 0 を指定した場合は拒否し、指定した識別値をヘッダーへ記録することの確認
TEST_F(stringCatalogFilterSourceTest, publish_records_catalog_id_and_rejects_zero)
{
    // Arrange
    cplat_string_catalog_filter_source_info actual_info;
    ASSERT_EQ(CPLAT_OK, compile_single_line("category <= 2", image_)); // [状態] - 条件をコンパイルする。

    // Pre-Assert

    // Act
    int actual_zero_ret = cplat_string_catalog_filter_source_publish(
        source_, sizeof(source_), image_, sizeof(image_), 0U, nullptr, nullptr); // [手順] - 識別値 0 で公開する。
    ASSERT_EQ(CPLAT_OK, publish_line("category <= 2"));                          // [手順] - 識別値を指定して公開する。
    ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_source_get_info(source_, sizeof(source_),
                                                                    &actual_info)); // [手順] - 公開の情報を読む。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_zero_ret); // [確認_異常系] - 識別値 0 を拒否すること。
    EXPECT_EQ(catalog_id_, actual_info.catalog_id);         // [確認_正常系] - 指定した識別値を記録すること。
}

// 別のカタログ向けの公開内容と、識別値を持たない以前の版の領域は取り込まず、記録して再試行しないことの確認
TEST_F(stringCatalogFilterSourceTest, publication_for_another_catalog_is_not_taken)
{
    struct publication
    {
        const char *label;
        bool is_legacy;
        unsigned char pad[7]; /**< 明示的アラインメントです。 */
    };
    const publication publications[] = {{"another catalog", false, {0}}, {"legacy region", true, {0}}};

    for (const publication &case_item : publications)
    {
        SCOPED_TRACE(case_item.label);

        // Arrange
        uint64_t other_catalog_id = 0U;
        uint64_t revision = 0U;
        int actual_matched = 1;
        memset(source_, 0, sizeof(source_));
        ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_get_catalog_id(filter_test_catalog(), &other_catalog_id));
        ASSERT_EQ(CPLAT_OK, compile_single_line("category <= 2", image_)); // [状態] - 条件をコンパイルする。
        ASSERT_EQ(CPLAT_OK,
                  cplat_string_catalog_filter_source_publish(source_, sizeof(source_), image_, sizeof(image_),
                                                             case_item.is_legacy ? catalog_id_ : other_catalog_id,
                                                             nullptr, &revision)); // [状態] - 公開する。
        if (case_item.is_legacy)
        {
            header()->catalog_id = 0U; // [状態] - 識別値を持たない以前の版の領域にする。
        }
        ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_attach_source(slot_, source_, sizeof(source_),
                                                                           nullptr)); // [状態] - 結び付ける。

        // Pre-Assert

        // Act
        (void)format_job_failed(&actual_matched); // [手順] - JOB_FAILED を組み立てる。
        cplat_string_catalog_filter_source_status status = source_status();
        EXPECT_CALL(mock_cplat, cplat_local_lock_try_lock(_))
            .Times(0);                            // [確認_異常系] - 同じ公開内容の取り込みを試みないこと。
        (void)format_job_failed(&actual_matched); // [手順] - もう一度組み立てる。
        testing::Mock::VerifyAndClearExpectations(&mock_cplat);

        // Assert
        EXPECT_EQ(0, actual_matched);               // [確認_異常系] - 取り込まず、以前の条件で判定すること。
        EXPECT_EQ(revision, status.taken_revision); // [確認_異常系] - 版番号を記録すること。
        EXPECT_EQ(CPLAT_ERR_IDENTITY_MISMATCH, status.last_result); // [確認_異常系] - カタログの不一致を記録すること。
    }
}
