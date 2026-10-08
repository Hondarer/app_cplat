#include <testfw.h>

#include "filterTestSupport.h"

#include "gen/filter_test_trace.h"

#include <cplat/base/result.h>
#include <cplat/sync/sync.h>

#include <atomic>
#include <cstddef>
#include <cstdint>

using namespace filter_test;

namespace
{
/** ワーカー スレッドへ渡す引数です。ok はそのスレッドだけが書き込み、join 後に読むため競合しません。 */
struct worker_args
{
    cplat_string_catalog_filter_slot *slot;
    std::atomic<int> *stop_flag; /**< スレッド間の停止通知。volatile は同期に使わない。 */
    int ok;
    int pad; /**< 明示的アラインメントです。 */
};

/** slot_format を、停止が通知されるまで繰り返すワーカー スレッドの本体です。 */
void format_worker(void *raw_arg)
{
    worker_args *args = static_cast<worker_args *>(raw_arg);
    char dest[CPLAT_STRING_CATALOG_TEXT_MAX];
    int matched;
    int ret;

    args->ok = 1;
    while (args->stop_flag->load() == 0)
    {
        matched = -1;
        ret = cplat_string_catalog_filter_slot_format(args->slot, dest, sizeof(dest), &matched,
                                                      FILTER_TEST_TRACE_KEY_JOB_RECEIVED, (uint32_t)1, (uint64_t)1,
                                                      "job", (int32_t)0, FILTER_TEST_CONTEXT_ARGS(7));
        if ((ret != CPLAT_OK) || ((matched != 0) && (matched != 1)))
        {
            args->ok = 0;
        }
    }
}
} // namespace

class stringCatalogFilterSlotConcurrencyTest : public Test
{
  protected:
    void SetUp() override
    {
        ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_create(
                                filter_test_trace_catalog(), filter_test_trace_key_names(),
                                filter_test_trace_key_name_count(), nullptr, kLineCapacity, kLineWidth, &slot_));
        // [状態確認] - `cplat_string_catalog_filter_slot_create( filter_test_trace_catalog(), filter_test_trace_key_names(), filter_test_trace_key_name_count(), nullptr, kLineCapacity, kLineWidth, &slot_)` の戻り値が `CPLAT_OK` であること。
    }

    void TearDown() override
    {
        cplat_string_catalog_filter_slot_dispose(&slot_);
    }

    cplat_string_catalog_filter_slot *slot_ = nullptr;
};

// 判定と書式展開を繰り返す複数スレッドの最中に、メイン スレッドが 2 種類のイメージを繰り返し適用しても、
// 各スレッドの呼び出しが常に CPLAT_OK を返し、クラッシュしないことの確認
TEST_F(stringCatalogFilterSlotConcurrencyTest, concurrent_apply_does_not_break_concurrent_format_calls)
{
    // Arrange
    static unsigned char image_key2[kImageSize];
    static unsigned char image_key3[kImageSize];
    worker_args args[4];
    cplat_thread *threads[4];
    std::atomic<int> stop_flag(0);
    int iteration;

    ASSERT_EQ(CPLAT_OK,
              compile_single_line("key == 2",
                                  image_key2)); // [状態] - JOB_RECEIVED (key=2) に一致するイメージをコンパイルする。
    // [状態確認] - `compile_single_line("key == 2", image_key2)` の戻り値が `CPLAT_OK` であること。
    ASSERT_EQ(CPLAT_OK,
              compile_single_line("key == 3",
                                  image_key3)); // [状態] - JOB_PROGRESS (key=3) に一致するイメージをコンパイルする。
    // [状態確認] - `compile_single_line("key == 3", image_key3)` の戻り値が `CPLAT_OK` であること。
    for (std::size_t index = 0; index < 4U; index++)
    {
        args[index].slot = slot_;
        args[index].stop_flag = &stop_flag;
        args[index].ok = 0;
    }

    // Pre-Assert

    // Act
    for (std::size_t index = 0; index < 4U; index++)
    {
        ASSERT_EQ(CPLAT_OK, cplat_thread_create(&threads[index], format_worker,
                                                &args[index])); // [手順] - format を繰り返すスレッドを 4 本起動する。
        // [確認_正常系 回数=4] - `cplat_thread_create(&threads[index], format_worker, &args[index])` の戻り値が `CPLAT_OK` であること。
    }
    for (iteration = 0; iteration < 200; iteration++)
    {
        ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_apply(
                                slot_, ((iteration % 2) == 0) ? image_key2 : image_key3, kImageSize, nullptr, 0U,
                                nullptr)); // [手順] - 2 種類のイメージを交互に 200 回適用する。
        // [確認_正常系 回数=200] - `cplat_string_catalog_filter_slot_apply( slot_, ((iteration % 2) == 0) ? image_key2 : image_key3, kImageSize, nullptr, 0U, nullptr)` の戻り値が `CPLAT_OK` であること。
    }
    stop_flag = 1; // [手順] - ワーカー スレッドへ終了を通知する。
    for (std::size_t index = 0; index < 4U; index++)
    {
        ASSERT_EQ(CPLAT_OK,
                  cplat_thread_join(threads[index], CPLAT_SYNC_WAIT_FOREVER)); // [手順] - 各スレッドの終了を待機する。
        // [確認_正常系 回数=4] - `cplat_thread_join(threads[index], CPLAT_SYNC_WAIT_FOREVER)` の戻り値が `CPLAT_OK` であること。
    }

    // Assert
    for (std::size_t index = 0; index < 4U; index++)
    {
        EXPECT_EQ(
            1,
            args[index]
                .ok); // [確認_正常系 回数=4] - 各スレッドの format 呼び出しが、常に CPLAT_OK かつ 0/1 の判定結果を返していたこと。
    }
}
