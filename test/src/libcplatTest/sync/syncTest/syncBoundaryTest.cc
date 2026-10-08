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
    int ok_result = test_sync_map_wait_rc(0);              // [手順] - 成功コードを変換する。
    int timeout_result = test_sync_map_wait_rc(ETIMEDOUT); // [手順] - タイムアウトコードを変換する。
    int busy_result = test_sync_map_wait_rc(EBUSY);        // [手順] - ビジーコードを変換する。
    int again_result = test_sync_map_wait_rc(EAGAIN);      // [手順] - 再試行コードを変換する。
    int access_result = test_sync_map_wait_rc(EACCES);     // [手順] - アクセス拒否コードを変換する。
    int unknown_result = test_sync_map_wait_rc(EINVAL);    // [手順] - 未知のエラーコードを変換する。

    // Assert
    EXPECT_EQ(CPLAT_OK, ok_result); // [確認_正常系] - 成功コードが CPLAT_OK に変換されること。
    EXPECT_EQ(CPLAT_ERR_TIMEOUT,
              timeout_result);                // [確認_正常系] - ETIMEDOUT が CPLAT_ERR_TIMEOUT に変換されること。
    EXPECT_EQ(CPLAT_ERR_BUSY, busy_result);   // [確認_正常系] - EBUSY が CPLAT_ERR_BUSY に変換されること。
    EXPECT_EQ(CPLAT_ERR_BUSY, again_result);  // [確認_正常系] - EAGAIN が CPLAT_ERR_BUSY に変換されること。
    EXPECT_EQ(CPLAT_ERR_BUSY, access_result); // [確認_正常系] - EACCES が CPLAT_ERR_BUSY に変換されること。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              unknown_result); // [確認_異常系] - 未知のコードが CPLAT_ERR_UNKNOWN に変換されること。
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
            })); // [Pre-Assert確認_正常系] - 基準時刻を注入すること。

    // Act
    test_sync_monotonic_deadline(&deadline, 200); // [手順] - 200 ms 後の deadline を生成する。

    // Assert
    EXPECT_EQ(11, deadline.tv_sec);          // [確認_正常系] - 秒が繰り上がること。
    EXPECT_EQ(100000000L, deadline.tv_nsec); // [確認_正常系] - ナノ秒が正規化されること。
}

#endif /* PLATFORM_LINUX */

// condvar API が不正引数を拒否することの確認
TEST(syncBoundaryTest, condvar_rejects_invalid_arguments)
{
    // Arrange
    cplat_condvar *cv = NULL;
    cplat_local_lock *lock = NULL;

    // Pre-Assert

    // Act
    int create_result = cplat_condvar_create(NULL); // [手順] - NULL の格納先で condvar を生成する。
    int wait_result = cplat_condvar_wait(NULL, NULL, CPLAT_SYNC_NO_WAIT); // [手順] - NULL 引数で待機する。
    int signal_result = cplat_condvar_signal(NULL);                       // [手順] - NULL の condvar を通知する。
    int broadcast_result = cplat_condvar_broadcast(NULL);                 // [手順] - NULL の condvar を一斉通知する。
    cplat_condvar_dispose(NULL);                                          // [手順] - NULL の condvar を破棄する。
    cplat_local_lock_dispose(NULL);                                       // [手順] - NULL の local lock を破棄する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              create_result); // [確認_異常系] - condvar 生成が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              wait_result); // [確認_異常系] - condvar 待機が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              signal_result); // [確認_異常系] - condvar 通知が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              broadcast_result); // [確認_異常系] - condvar 一斉通知が INVALID_ARGUMENT になること。
    EXPECT_EQ(NULL, cv);         // [確認_正常系] - NULL 破棄が安全に完了すること。
    EXPECT_EQ(NULL, lock);       // [確認_正常系] - NULL 破棄が安全に完了すること。
}

// condvar の即時タイムアウト、通知、一斉通知が成功することの確認
TEST(syncBoundaryTest, condvar_timeout_signal_and_broadcast_are_supported)
{
    // Arrange
    cplat_condvar *cv = NULL;
    cplat_local_lock *lock = NULL;

    // Pre-Assert

    // Act
    int create_cv_result = cplat_condvar_create(&cv);                       // [手順] - condvar を生成する。
    int create_lock_result = cplat_local_lock_create(&lock);                // [手順] - 待機用 lock を生成する。
    int lock_result = cplat_local_lock_lock(lock, CPLAT_SYNC_WAIT_FOREVER); // [手順] - lock を取得する。
    int wait_result = cplat_condvar_wait(cv, lock, CPLAT_SYNC_NO_WAIT);     // [手順] - 即時タイムアウトで待機する。
    int unlock_result = cplat_local_lock_unlock(lock);                      // [手順] - lock を解放する。
    int signal_result = cplat_condvar_signal(cv);                           // [手順] - condvar を通知する。
    int broadcast_result = cplat_condvar_broadcast(cv);                     // [手順] - condvar を一斉通知する。

    // Assert
    EXPECT_EQ(CPLAT_OK, create_cv_result);     // [確認_正常系] - condvar 生成が成功すること。
    EXPECT_EQ(CPLAT_OK, create_lock_result);   // [確認_正常系] - lock 生成が成功すること。
    EXPECT_EQ(CPLAT_OK, lock_result);          // [確認_正常系] - lock 取得が成功すること。
    EXPECT_EQ(CPLAT_ERR_TIMEOUT, wait_result); // [確認_正常系] - 即時待機が TIMEOUT になること。
    EXPECT_EQ(CPLAT_OK, unlock_result);        // [確認_正常系] - lock 解放が成功すること。
    EXPECT_EQ(CPLAT_OK, signal_result);        // [確認_正常系] - condvar 通知が成功すること。
    EXPECT_EQ(CPLAT_OK, broadcast_result);     // [確認_正常系] - condvar 一斉通知が成功すること。

    // Cleanup
    cplat_condvar_dispose(cv);
    cplat_local_lock_dispose(lock);
}

// WAIT_FOREVER の condvar 待機が通知で戻ることの確認
TEST(syncBoundaryTest, condvar_wait_forever_returns_after_signal)
{
    // Arrange
    cplat_condvar *cv = NULL;
    cplat_local_lock *lock = NULL;
    std::atomic<bool> waiter_ready(false);
    int wait_result = CPLAT_ERR_UNKNOWN;
    ASSERT_EQ(CPLAT_OK, cplat_condvar_create(&cv)); // [状態] - condvar を生成する。
    // [状態確認] - cplat_condvar_create の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_local_lock_create(&lock)); // [状態] - local lock を生成する。
    // [状態確認] - cplat_local_lock_create の戻り値が CPLAT_OK であること。
    std::thread waiter(
        [&]()
        {
            ASSERT_EQ(CPLAT_OK,
                      cplat_local_lock_lock(lock, CPLAT_SYNC_WAIT_FOREVER)); // [状態] - 待機側の lock を取得する。
            // [状態確認] - cplat_local_lock_lock の戻り値が CPLAT_OK であること。
            waiter_ready.store(true);
            wait_result = cplat_condvar_wait(cv, lock, CPLAT_SYNC_WAIT_FOREVER);
            EXPECT_EQ(CPLAT_OK, cplat_local_lock_unlock(lock)); // [状態] - 待機側の lock を解放する。
            // [状態確認] - cplat_local_lock_unlock の戻り値が CPLAT_OK であること。
        });

    // Pre-Assert
    while (!waiter_ready.load())
    {
        std::this_thread::yield();
    }

    // Act
    int lock_result =
        cplat_local_lock_lock(lock, CPLAT_SYNC_WAIT_FOREVER); // [手順] - 待機側が解放した lock を取得する。
    int signal_result = cplat_condvar_signal(cv);             // [手順] - 待機側を通知する。
    int unlock_result = cplat_local_lock_unlock(lock);        // [手順] - 通知側の lock を解放する。
    waiter.join();

    // Assert
    EXPECT_EQ(CPLAT_OK, lock_result);   // [確認_正常系] - 通知側の lock 取得が成功すること。
    EXPECT_EQ(CPLAT_OK, signal_result); // [確認_正常系] - condvar 通知が成功すること。
    EXPECT_EQ(CPLAT_OK, unlock_result); // [確認_正常系] - 通知側の lock 解放が成功すること。
    EXPECT_EQ(CPLAT_OK, wait_result);   // [確認_正常系] - WAIT_FOREVER の待機が通知で終了すること。

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
    cplat_thread *thread = NULL;
    int flag = 0;

    // Pre-Assert

    // Act
    int create_result = cplat_thread_create(&thread, set_thread_flag, &flag); // [手順] - スレッドを生成する。
    int join_result = cplat_thread_join(thread, CPLAT_SYNC_WAIT_FOREVER);     // [手順] - スレッド終了を待機する。

    // Assert
    EXPECT_EQ(CPLAT_OK, create_result); // [確認_正常系] - スレッド生成が成功すること。
    EXPECT_EQ(CPLAT_OK, join_result);   // [確認_正常系] - スレッド join が成功すること。
    EXPECT_EQ(1, flag);                 // [確認_正常系] - 開始関数が実行されること。
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
    cplat_call_once(&flag, count_once_call); // [手順] - call_once を 1 回目に呼び出す。
    cplat_call_once(&flag, count_once_call); // [手順] - call_once を 2 回目に呼び出す。
    cplat_call_once(NULL, count_once_call);  // [手順] - NULL flag で call_once を呼び出す。
    cplat_call_once(&flag, NULL);            // [手順] - NULL 関数で call_once を呼び出す。

    // Assert
    EXPECT_EQ(1, s_once_call_count); // [確認_正常系] - 関数が 1 回だけ呼び出されること。
    EXPECT_EQ(2, cplat_atomic_load_i32(&flag.state,
                                       CPLAT_MEMORY_ORDER_RELAXED)); // [確認_正常系] - 初期化済み状態になること。
}
