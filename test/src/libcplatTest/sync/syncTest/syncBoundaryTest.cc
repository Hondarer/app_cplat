#include <testfw.h>

#include <atomic>
#include <thread>

#include <cplat/base/platform.h>

#if defined(PLATFORM_LINUX)
    #include <errno.h>
    #include <mock_time.h>
#endif

#include "syncTestHelper.h"
#include "sync_linux.inject.h"

using testing::_;
#if defined(PLATFORM_LINUX)
using testing::Invoke;
#endif

#if defined(PLATFORM_LINUX)

// 待機結果コードが対応する共通結果へ変換されることの確認
TEST(syncBoundaryTest, map_wait_rc_translates_supported_results)
{
    // Arrange

    // Pre-Assert

    // Act
    int ok_result = test_sync_map_wait_rc(0); // [手順] - 成功コード 0 を指定して test_sync_map_wait_rc() を呼び出す。
    int timeout_result = test_sync_map_wait_rc(
        ETIMEDOUT); // [手順] - タイムアウト コード ETIMEDOUT を指定して test_sync_map_wait_rc() を呼び出す。
    int busy_result =
        test_sync_map_wait_rc(EBUSY); // [手順] - ビジー コード EBUSY を指定して test_sync_map_wait_rc() を呼び出す。
    int again_result =
        test_sync_map_wait_rc(EAGAIN); // [手順] - 再試行コード EAGAIN を指定して test_sync_map_wait_rc() を呼び出す。
    int access_result = test_sync_map_wait_rc(
        EACCES); // [手順] - アクセス拒否コード EACCES を指定して test_sync_map_wait_rc() を呼び出す。
    int unknown_result = test_sync_map_wait_rc(
        EINVAL); // [手順] - 未知のエラー コード EINVAL を指定して test_sync_map_wait_rc() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        ok_result); // [確認_正常系] - 成功コード 0 に対する test_sync_map_wait_rc() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_ERR_TIMEOUT, timeout_result);
    // [確認_正常系] - ETIMEDOUT に対する test_sync_map_wait_rc() の戻り値が CPLAT_ERR_TIMEOUT であること。
    EXPECT_EQ(CPLAT_ERR_BUSY, busy_result);
    // [確認_正常系] - EBUSY に対する test_sync_map_wait_rc() の戻り値が CPLAT_ERR_BUSY であること。
    EXPECT_EQ(CPLAT_ERR_BUSY, again_result);
    // [確認_正常系] - EAGAIN に対する test_sync_map_wait_rc() の戻り値が CPLAT_ERR_BUSY であること。
    EXPECT_EQ(CPLAT_ERR_BUSY, access_result);
    // [確認_正常系] - EACCES に対する test_sync_map_wait_rc() の戻り値が CPLAT_ERR_BUSY であること。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, unknown_result);
    // [確認_異常系] - 未知のコード EINVAL に対する test_sync_map_wait_rc() の戻り値が CPLAT_ERR_UNKNOWN であること。
}

// 単調 deadline のナノ秒が繰り上がり正規化されることの確認
TEST(syncBoundaryTest, monotonic_deadline_normalizes_nanoseconds)
{
    // Arrange
    NiceMock<Mock_time> mock_time;
    struct timespec deadline;

    // Pre-Assert
    EXPECT_CALL(mock_time, clock_gettime(_, _, _, CLOCK_MONOTONIC, _))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, const clockid_t, struct timespec *ts)
            {
                ts->tv_sec = 10;
                ts->tv_nsec = 900000000L;
                return 0;
            })); // [Pre-Assert確認_正常系] - clock_gettime() が CLOCK_MONOTONIC を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - clock_gettime() から、基準時刻として tv_sec に 10、tv_nsec に 900000000L を設定して 0 を返す。

    // Act
    test_sync_monotonic_deadline(&deadline,
                                 200); // [手順] - 200 ms を指定して test_sync_monotonic_deadline() を呼び出す。

    // Assert
    EXPECT_EQ(11, deadline.tv_sec);
    // [確認_正常系] - deadline.tv_sec が 11 であること。(基準時刻 10 秒 900000000 ナノ秒へ 200 ms を加えて秒が繰り上がったことの確認)
    EXPECT_EQ(100000000L, deadline.tv_nsec);
    // [確認_正常系] - deadline.tv_nsec が 100000000L であること。(900000000 ナノ秒へ 200 ms を加えて 100000000 ナノ秒へ正規化されたことの確認)
}

#endif /* PLATFORM_LINUX */

// condvar API が不正引数を拒否することの確認
TEST(syncBoundaryTest, condvar_rejects_invalid_arguments)
{
    // Arrange
    cplat_condvar *cv = nullptr;
    cplat_local_lock *lock = nullptr;

    // Pre-Assert

    // Act
    int create_result =
        cplat_condvar_create(nullptr); // [手順] - 格納先に nullptr を指定して cplat_condvar_create() を呼び出す。
    int wait_result = cplat_condvar_wait(
        nullptr, nullptr, CPLAT_SYNC_NO_WAIT); // [手順] - 引数に nullptr を指定して cplat_condvar_wait() を呼び出す。
    int signal_result =
        cplat_condvar_signal(nullptr); // [手順] - condvar に nullptr を指定して cplat_condvar_signal() を呼び出す。
    int broadcast_result = cplat_condvar_broadcast(
        nullptr);                   // [手順] - condvar に nullptr を指定して cplat_condvar_broadcast() を呼び出す。
    cplat_condvar_dispose(nullptr); // [手順] - condvar に nullptr を指定して cplat_condvar_dispose() を呼び出す。
    cplat_local_lock_dispose(
        nullptr); // [手順] - local lock に nullptr を指定して cplat_local_lock_dispose() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, create_result);
    // [確認_異常系] - nullptr 格納先に対する cplat_condvar_create() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, wait_result);
    // [確認_異常系] - nullptr 引数に対する cplat_condvar_wait() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, signal_result);
    // [確認_異常系] - nullptr に対する cplat_condvar_signal() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, broadcast_result);
    // [確認_異常系] - nullptr に対する cplat_condvar_broadcast() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        nullptr,
        cv); // [確認_正常系] - cv が nullptr であること。(nullptr の condvar 破棄後も格納先が変わらないことの確認)
    EXPECT_EQ(nullptr, lock);
    // [確認_正常系] - lock が nullptr であること。(nullptr の local lock 破棄後も格納先が変わらないことの確認)
}

// condvar の即時タイムアウト、通知、一斉通知が成功することの確認
TEST(syncBoundaryTest, condvar_timeout_signal_and_broadcast_are_supported)
{
    // Arrange
    cplat_condvar *cv = nullptr;
    cplat_local_lock *lock = nullptr;

    // Pre-Assert

    // Act
    int create_cv_result =
        cplat_condvar_create(&cv); // [手順] - condvar の格納先を指定して cplat_condvar_create() を呼び出す。
    int create_lock_result = cplat_local_lock_create(
        &lock); // [手順] - 待機用 lock の格納先を指定して cplat_local_lock_create() を呼び出す。
    int lock_result = cplat_local_lock_lock(
        lock, CPLAT_SYNC_WAIT_FOREVER); // [手順] - 待機用 lock を指定して cplat_local_lock_lock() を呼び出す。
    int wait_result = cplat_condvar_wait(
        cv, lock, CPLAT_SYNC_NO_WAIT); // [手順] - 即時タイムアウトを指定して cplat_condvar_wait() を呼び出す。
    int unlock_result =
        cplat_local_lock_unlock(lock);            // [手順] - lock を指定して cplat_local_lock_unlock() を呼び出す。
    int signal_result = cplat_condvar_signal(cv); // [手順] - condvar を指定して cplat_condvar_signal() を呼び出す。
    int broadcast_result =
        cplat_condvar_broadcast(cv); // [手順] - condvar を指定して cplat_condvar_broadcast() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, create_cv_result); // [確認_正常系] - cplat_condvar_create() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, create_lock_result);
    // [確認_正常系] - cplat_local_lock_create() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, lock_result); // [確認_正常系] - cplat_local_lock_lock() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_ERR_TIMEOUT, wait_result);
    // [確認_正常系] - 即時タイムアウトの cplat_condvar_wait() の戻り値が CPLAT_ERR_TIMEOUT であること。
    EXPECT_EQ(CPLAT_OK, unlock_result); // [確認_正常系] - cplat_local_lock_unlock() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, signal_result); // [確認_正常系] - cplat_condvar_signal() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, broadcast_result);
    // [確認_正常系] - cplat_condvar_broadcast() の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_condvar_dispose(cv);
    cplat_local_lock_dispose(lock);
}

// WAIT_FOREVER の condvar 待機が通知で戻ることの確認
TEST(syncBoundaryTest, condvar_wait_forever_returns_after_signal)
{
    // Arrange
    cplat_condvar *cv = nullptr;
    cplat_local_lock *lock = nullptr;
    std::atomic<bool> waiter_ready(false);
    int wait_result = CPLAT_ERR_UNKNOWN;
    ASSERT_EQ(CPLAT_OK, cplat_condvar_create(&cv)); // [状態] - condvar を生成する。
    // [状態確認] - cplat_condvar_create() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_local_lock_create(&lock)); // [状態] - local lock を生成する。
    // [状態確認] - cplat_local_lock_create() の戻り値が CPLAT_OK であること。
    std::thread waiter(
        [&]()
        {
            ASSERT_EQ(CPLAT_OK,
                      cplat_local_lock_lock(lock, CPLAT_SYNC_WAIT_FOREVER)); // [状態] - 待機側の lock を取得する。
            // [状態確認] - cplat_local_lock_lock() の戻り値が CPLAT_OK であること。
            waiter_ready.store(true);
            wait_result = cplat_condvar_wait(cv, lock, CPLAT_SYNC_WAIT_FOREVER);
            EXPECT_EQ(CPLAT_OK, cplat_local_lock_unlock(lock)); // [状態] - 待機側の lock を解放する。
            // [状態確認] - cplat_local_lock_unlock() の戻り値が CPLAT_OK であること。
        });

    // Pre-Assert
    while (!waiter_ready.load())
    {
        std::this_thread::yield();
    }

    // Act
    int lock_result = cplat_local_lock_lock(
        lock,
        CPLAT_SYNC_WAIT_FOREVER); // [手順] - 待機側が解放した lock を指定して cplat_local_lock_lock() を呼び出す。
    int signal_result =
        cplat_condvar_signal(cv); // [手順] - 待機側を通知する condvar を指定して cplat_condvar_signal() を呼び出す。
    int unlock_result =
        cplat_local_lock_unlock(lock); // [手順] - 通知側の lock を指定して cplat_local_lock_unlock() を呼び出す。
    waiter.join();

    // Assert
    EXPECT_EQ(CPLAT_OK,
              lock_result); // [確認_正常系] - 通知側の cplat_local_lock_lock() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, signal_result); // [確認_正常系] - cplat_condvar_signal() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, unlock_result);
    // [確認_正常系] - 通知側の cplat_local_lock_unlock() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, wait_result);
    // [確認_正常系] - WAIT_FOREVER の cplat_condvar_wait() の戻り値が CPLAT_OK であること。(通知で待機が終了することの確認)

    // Cleanup
    cplat_condvar_dispose(cv);
    cplat_local_lock_dispose(lock);
}

static void set_thread_flag(void *arg)
{
    int *flag = static_cast<int *>(arg);
    *flag = 1;
}

// スレッド生成と join で開始関数が実行されることの確認
TEST(syncBoundaryTest, thread_create_and_join_runs_start_function)
{
    // Arrange
    cplat_thread *thread = nullptr;
    int flag = 0;

    // Pre-Assert

    // Act
    int create_result = cplat_thread_create(
        &thread, set_thread_flag,
        &flag); // [手順] - スレッド関数 set_thread_flag() を指定して cplat_thread_create() を呼び出す。
    int join_result = cplat_thread_join(
        thread, CPLAT_SYNC_WAIT_FOREVER); // [手順] - 生成したスレッドを指定して cplat_thread_join() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, create_result); // [確認_正常系] - cplat_thread_create() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, join_result);   // [確認_正常系] - cplat_thread_join() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1, flag); // [確認_正常系] - flag が 1 であること。(set_thread_flag() が実行されたことの確認)
}

static int s_once_call_count;

static void count_once_call(void)
{
    s_once_call_count++;
}

// call_once が関数を 1 回だけ実行することの確認
TEST(syncBoundaryTest, call_once_runs_function_only_once)
{
    // Arrange
    cplat_once_flag flag = {0};
    s_once_call_count = 0;

    // Pre-Assert

    // Act
    cplat_call_once(
        &flag,
        count_once_call); // [手順] - flag と count_once_call() を指定して 1 回目の cplat_call_once() を呼び出す。
    cplat_call_once(
        &flag,
        count_once_call); // [手順] - flag と count_once_call() を指定して 2 回目の cplat_call_once() を呼び出す。
    cplat_call_once(nullptr, count_once_call); // [手順] - flag に nullptr を指定して cplat_call_once() を呼び出す。
    cplat_call_once(&flag, nullptr);           // [手順] - 関数に nullptr を指定して cplat_call_once() を呼び出す。

    // Assert
    EXPECT_EQ(1, s_once_call_count);
    // [確認_正常系] - s_once_call_count が 1 であること。(count_once_call() が 1 回だけ呼び出されたことの確認)
    EXPECT_EQ(2, cplat_atomic_load_i32(&flag.state, CPLAT_MEMORY_ORDER_RELAXED));
    // [確認_正常系] - cplat_atomic_load_i32() の戻り値が 2 であること。(実行済み状態であることの確認)
}
