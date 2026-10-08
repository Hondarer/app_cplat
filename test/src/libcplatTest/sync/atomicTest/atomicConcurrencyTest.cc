#include <testfw.h>

#include <cplat/base/result.h>
#include <cplat/sync/atomic.h>
#include <cplat/sync/sync.h>

#include <cstdint>

namespace
{
const int kThreadCount = 4;
const int kFetchAddIncrementsPerThread = 100000;
const int kSpinlockIncrementsPerThread = 20000; // 実スレッドの競合を伴うため、fetch_add より少なくして実行時間を抑える

struct fetch_add_worker_args
{
    cplat_atomic_u64 *counter;
};

// RELAXED の fetch_add_u64 を規定回数だけ繰り返すワーカー スレッドの本体
void fetch_add_worker(void *raw_arg)
{
    fetch_add_worker_args *args = static_cast<fetch_add_worker_args *>(raw_arg);
    int index;

    for (index = 0; index < kFetchAddIncrementsPerThread; index++)
    {
        (void)cplat_atomic_fetch_add_u64(args->counter, 1ULL, CPLAT_MEMORY_ORDER_RELAXED);
    }
}

// compare_exchange による自前のスピンロックを acquire で取得する
void spin_lock(cplat_atomic_i32 *lock_flag)
{
    for (;;)
    {
        int32_t expected = 0;

        if (cplat_atomic_compare_exchange_i32(lock_flag, &expected, 1, CPLAT_MEMORY_ORDER_ACQUIRE) != 0)
        {
            return;
        }
    }
}

// 自前のスピンロックを release で解放する
void spin_unlock(cplat_atomic_i32 *lock_flag)
{
    cplat_atomic_store_i32(lock_flag, 0, CPLAT_MEMORY_ORDER_RELEASE);
}

struct spinlock_counter_args
{
    cplat_atomic_i32 *lock_flag; // 0 = 未ロック、1 = ロック中
    int *counter;                // スピンロックで保護された通常の int
};

// スピンロックで保護した通常の int を規定回数だけカウント アップするワーカー スレッドの本体
void spinlock_counter_worker(void *raw_arg)
{
    spinlock_counter_args *args = static_cast<spinlock_counter_args *>(raw_arg);
    int index;

    for (index = 0; index < kSpinlockIncrementsPerThread; index++)
    {
        spin_lock(args->lock_flag);
        (*args->counter)++;
        spin_unlock(args->lock_flag);
    }
}
} // namespace

// RELAXED の fetch_add_u64 を 4 スレッドから同時に実行しても、合計が欠落なく一致することの確認
TEST(atomicConcurrencyTest, concurrent_relaxed_fetch_add_u64_sums_without_loss_across_four_threads)
{
    // Arrange
    cplat_atomic_u64 counter = CPLAT_ATOMIC_INIT(0U); // [状態] - 0 で初期化した共有カウンターを用意する。
    fetch_add_worker_args args;
    cplat_thread *threads[kThreadCount];
    int index;

    args.counter = &counter;

    // Pre-Assert

    // Act
    for (index = 0; index < kThreadCount; index++)
    {
        ASSERT_EQ(CPLAT_OK, cplat_thread_create(&threads[index], fetch_add_worker,
                                                &args)); // [手順] - fetch_add を繰り返すスレッドを 4 本起動する。
        // [確認_正常系 回数=4] - `cplat_thread_create(&threads[index], fetch_add_worker, &args)` の戻り値が `CPLAT_OK` であること。
    }
    for (index = 0; index < kThreadCount; index++)
    {
        ASSERT_EQ(CPLAT_OK,
                  cplat_thread_join(threads[index], CPLAT_SYNC_WAIT_FOREVER)); // [手順] - 各スレッドの終了を待機する。
        // [確認_正常系 回数=4] - `cplat_thread_join(threads[index], CPLAT_SYNC_WAIT_FOREVER)` の戻り値が `CPLAT_OK` であること。
    }
    const uint64_t total =
        cplat_atomic_load_u64(&counter, CPLAT_MEMORY_ORDER_SEQ_CST); // [手順] - 最終的な合計値を読み取る。

    // Assert
    EXPECT_EQ((uint64_t)(kThreadCount * kFetchAddIncrementsPerThread),
              total); // [確認_正常系] - 4 スレッド分の加算が欠落なく反映されていること。
}

// compare_exchange (acquire/release) による自前のスピンロックで保護した通常の int が、
// 複数スレッドからの同時カウント アップでも正しく数えられることの確認
TEST(atomicConcurrencyTest, compare_exchange_protected_spinlock_counts_correctly_under_contention)
{
    // Arrange
    cplat_atomic_i32 lock_flag = CPLAT_ATOMIC_INIT(0); // [状態] - 未ロックで初期化したスピンロック フラグを用意する。
    int counter = 0;
    spinlock_counter_args args;
    cplat_thread *threads[kThreadCount];
    int index;

    args.lock_flag = &lock_flag;
    args.counter = &counter;

    // Pre-Assert

    // Act
    for (index = 0; index < kThreadCount; index++)
    {
        ASSERT_EQ(CPLAT_OK,
                  cplat_thread_create(
                      &threads[index], spinlock_counter_worker,
                      &args)); // [手順] - スピンロックで保護したカウント アップを行うスレッドを 4 本起動する。
        // [確認_正常系 回数=4] - `cplat_thread_create(&threads[index], spinlock_counter_worker, &args)` の戻り値が `CPLAT_OK` であること。
    }
    for (index = 0; index < kThreadCount; index++)
    {
        ASSERT_EQ(CPLAT_OK,
                  cplat_thread_join(threads[index], CPLAT_SYNC_WAIT_FOREVER)); // [手順] - 各スレッドの終了を待機する。
        // [確認_正常系 回数=4] - `cplat_thread_join(threads[index], CPLAT_SYNC_WAIT_FOREVER)` の戻り値が `CPLAT_OK` であること。
    }

    // Assert
    EXPECT_EQ(kThreadCount * kSpinlockIncrementsPerThread,
              counter); // [確認_正常系] - スピンロックで保護したカウンターが、欠落や二重カウントなく一致すること。
}
