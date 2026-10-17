#include <testfw.h>

#include <atomic>
#include <thread>

#include <cplat/base/platform.h>

#if defined(PLATFORM_LINUX)

    #include <mock_pthread.h>
    #include <mock_fcntl.h>
    #include <mock_time.h>
    #include <sys/mock_file.h>

    #include <cplat/sync/sync.h>

    #include <errno.h>
    #include <fcntl.h>
    #include <string.h>

    #include "syncTestHelper.h"
    #include "sync_linux.inject.h"

using testing::_;
using testing::DoDefault;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;

namespace
{
std::atomic<bool> s_once_callback_entered(false);
std::atomic<bool> s_once_callback_release(false);
std::atomic<bool> s_once_waiter_started(false);

void blocked_once_callback(void)
{
    s_once_callback_entered.store(true);
    while (!s_once_callback_release.load())
    {
        std::this_thread::yield();
    }
}
} // namespace

// local lock の trylock で未知の pthread エラーを分類することの確認
TEST(syncAdditionalFailureTest, local_lock_maps_unknown_trylock_error)
{
    // Arrange
    cplat_local_lock *lock = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_local_lock_create(&lock)); // [状態] - local lock を生成する。
    // [状態確認] - cplat_local_lock_create() の戻り値が CPLAT_OK であること。
    NiceMock<Mock_pthread> mock_pthread;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_mutex_trylock(_, _, _, _))
        .WillOnce(Return(EINVAL)); // [Pre-Assert確認_異常系] - pthread_mutex_trylock() が 1 回呼び出されること。
                                   // [Pre-Assert手順] - pthread_mutex_trylock() から EINVAL を返す。

    // Act
    int result = cplat_local_lock_try_lock(
        lock); // [手順] - pthread_mutex_trylock() が EINVAL を返す状態で cplat_local_lock_try_lock() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - cplat_local_lock_try_lock() の戻り値が CPLAT_ERR_UNKNOWN であること。

    // Cleanup
    cplat_local_lock_dispose(lock);
}

// condvar の待機・通知エラーを共通結果へ変換することの確認
TEST(syncAdditionalFailureTest, condvar_maps_wait_signal_and_broadcast_errors)
{
    // Arrange
    cplat_condvar *cv = nullptr;
    cplat_local_lock *lock = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_condvar_create(&cv)); // [状態] - condvar を生成する。
    // [状態確認] - cplat_condvar_create() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_local_lock_create(&lock)); // [状態] - local lock を生成する。
    // [状態確認] - cplat_local_lock_create() の戻り値が CPLAT_OK であること。
    NiceMock<Mock_pthread> mock_pthread;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_cond_wait(_, _, _, _, _))
        .WillOnce(Return(EINVAL)); // [Pre-Assert確認_異常系] - pthread_cond_wait() が 1 回呼び出されること。
                                   // [Pre-Assert手順] - pthread_cond_wait() から EINVAL を返す。
    EXPECT_CALL(mock_pthread, pthread_cond_timedwait(_, _, _, _, _, _))
        .WillOnce(Return(ETIMEDOUT)); // [Pre-Assert確認_正常系] - pthread_cond_timedwait() が 1 回呼び出されること。
                                      // [Pre-Assert手順] - pthread_cond_timedwait() から ETIMEDOUT を返す。
    EXPECT_CALL(mock_pthread, pthread_cond_signal(_, _, _, _))
        .WillOnce(Return(EINVAL)); // [Pre-Assert確認_異常系] - pthread_cond_signal() が 1 回呼び出されること。
                                   // [Pre-Assert手順] - pthread_cond_signal() から EINVAL を返す。
    EXPECT_CALL(mock_pthread, pthread_cond_broadcast(_, _, _, _))
        .WillOnce(Return(EINVAL)); // [Pre-Assert確認_異常系] - pthread_cond_broadcast() が 1 回呼び出されること。
                                   // [Pre-Assert手順] - pthread_cond_broadcast() から EINVAL を返す。

    // Act
    int wait_result = cplat_condvar_wait(
        cv, lock,
        CPLAT_SYNC_WAIT_FOREVER); // [手順] - pthread_cond_wait() が EINVAL を返す状態で、CPLAT_SYNC_WAIT_FOREVER を指定して cplat_condvar_wait() を呼び出す。
    int timed_wait_result = cplat_condvar_wait(
        cv, lock,
        CPLAT_SYNC_NO_WAIT); // [手順] - pthread_cond_timedwait() が ETIMEDOUT を返す状態で、CPLAT_SYNC_NO_WAIT を指定して cplat_condvar_wait() を呼び出す。
    int signal_result = cplat_condvar_signal(
        cv); // [手順] - pthread_cond_signal() が EINVAL を返す状態で cplat_condvar_signal() を呼び出す。
    int broadcast_result = cplat_condvar_broadcast(
        cv); // [手順] - pthread_cond_broadcast() が EINVAL を返す状態で cplat_condvar_broadcast() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        wait_result); // [確認_異常系] - CPLAT_SYNC_WAIT_FOREVER を指定した cplat_condvar_wait() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        CPLAT_ERR_TIMEOUT,
        timed_wait_result); // [確認_正常系] - CPLAT_SYNC_NO_WAIT を指定した cplat_condvar_wait() の戻り値が CPLAT_ERR_TIMEOUT であること。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              signal_result); // [確認_異常系] - cplat_condvar_signal() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              broadcast_result); // [確認_異常系] - cplat_condvar_broadcast() の戻り値が CPLAT_ERR_UNKNOWN であること。

    // Cleanup
    cplat_condvar_dispose(cv);
    cplat_local_lock_dispose(lock);
}

// condvar 属性の時計設定失敗を生成失敗として処理することの確認
TEST(syncAdditionalFailureTest, condvar_create_reports_clock_attribute_failure)
{
    // Arrange
    NiceMock<Mock_pthread> mock_pthread;
    cplat_condvar *cv = nullptr;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_condattr_setclock(_, _, _, _, CLOCK_MONOTONIC))
        .WillOnce(Return(
            EINVAL)); // [Pre-Assert確認_異常系] - pthread_condattr_setclock() が CLOCK_MONOTONIC を指定して 1 回呼び出されること。
                      // [Pre-Assert手順] - pthread_condattr_setclock() から EINVAL を返す。

    // Act
    int result =
        cplat_condvar_create(&cv); // [手順] - 時計属性の設定が失敗する状態で cplat_condvar_create() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - cplat_condvar_create() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(nullptr, cv); // [確認_異常系] - 生成失敗時の cv が nullptr であること。
}

// condvar 初期化失敗を生成失敗として処理することの確認
TEST(syncAdditionalFailureTest, condvar_create_reports_native_initialization_failure)
{
    // Arrange
    NiceMock<Mock_pthread> mock_pthread;
    cplat_condvar *cv = nullptr;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_cond_init(_, _, _, _, _))
        .WillOnce(Return(EINVAL)); // [Pre-Assert確認_異常系] - pthread_cond_init() が 1 回呼び出されること。
                                   // [Pre-Assert手順] - pthread_cond_init() から EINVAL を返す。

    // Act
    int result = cplat_condvar_create(
        &cv); // [手順] - ネイティブの condvar の初期化が失敗する状態で cplat_condvar_create() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        result); // [確認_異常系] - 初期化失敗時の cplat_condvar_create() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(nullptr, cv); // [確認_異常系] - 初期化失敗時の cv が nullptr であること。
}

// 条件変数ハンドルの確保失敗が生成失敗として報告されることの確認
TEST(syncAdditionalFailureTest, condvar_create_reports_allocation_failure)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_condvar *cv = nullptr;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_calloc(_, _))
        .WillOnce(
            Return(nullptr)); // [Pre-Assert確認_異常系] - condvar ハンドルの cplat_calloc() が 1 回呼び出されること。
                              // [Pre-Assert手順] - cplat_calloc() から nullptr を返す。

    // Act
    int result =
        cplat_condvar_create(&cv); // [手順] - ハンドルの確保が失敗する状態で cplat_condvar_create() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        result); // [確認_異常系] - ハンドル確保失敗時の cplat_condvar_create() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(nullptr,
              cv); // [確認_異常系] - ハンドル確保失敗時の cv が nullptr であること。
}

// ローカル RW ロックのハンドル確保失敗が生成失敗として報告されることの確認
TEST(syncAdditionalFailureTest, local_rwlock_create_reports_allocation_failure)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_local_rwlock *rwlock = nullptr;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_calloc(_, _))
        .WillOnce(Return(
            nullptr)); // [Pre-Assert確認_異常系] - local rwlock ハンドルの cplat_calloc() が 1 回呼び出されること。
                       // [Pre-Assert手順] - cplat_calloc() から nullptr を返す。

    // Act
    int result = cplat_local_rwlock_create(
        &rwlock); // [手順] - ハンドルの確保が失敗する状態で cplat_local_rwlock_create() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        result); // [確認_異常系] - ハンドル確保失敗時の cplat_local_rwlock_create() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(nullptr,
              rwlock); // [確認_異常系] - ハンドル確保失敗時の rwlock が nullptr であること。
}

// ローカル RW ロックの mutex 初期化失敗が生成失敗として報告されることの確認
TEST(syncAdditionalFailureTest, local_rwlock_create_reports_mutex_initialization_failure)
{
    // Arrange
    NiceMock<Mock_pthread> mock_pthread;
    cplat_local_rwlock *rwlock = nullptr;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_mutex_init(_, _, _, _, _))
        .WillOnce(Return(ENOMEM)); // [Pre-Assert確認_異常系] - pthread_mutex_init() が 1 回呼び出されること。
                                   // [Pre-Assert手順] - pthread_mutex_init() から ENOMEM を返す。
    EXPECT_CALL(mock_pthread, pthread_mutex_destroy(_, _, _, _))
        .WillOnce(
            Return(0)); // [Pre-Assert確認_正常系] - 初期化失敗後に pthread_mutex_destroy() が 1 回呼び出されること。
                        // [Pre-Assert手順] - pthread_mutex_destroy() から 0 を返す。
    EXPECT_CALL(mock_pthread, pthread_cond_destroy(_, _, _, _))
        .WillRepeatedly(Return(
            0)); // [Pre-Assert確認_正常系] - 初期化失敗後に pthread_cond_destroy() が任意の回数呼び出されること。
                 // [Pre-Assert手順] - pthread_cond_destroy() から 0 を返す。

    // Act
    int result = cplat_local_rwlock_create(
        &rwlock); // [手順] - mutex の初期化が失敗する状態で cplat_local_rwlock_create() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        result); // [確認_異常系] - mutex 初期化失敗時の cplat_local_rwlock_create() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(nullptr,
              rwlock); // [確認_異常系] - 生成失敗時の rwlock が nullptr であること。
}

// rwlock の未取得状態での解放要求を拒否することの確認
TEST(syncAdditionalFailureTest, rwlock_rejects_unlock_without_ownership)
{
    // Arrange
    cplat_local_rwlock *rwlock = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_local_rwlock_create(&rwlock)); // [状態] - local rwlock を生成する。
    // [状態確認] - cplat_local_rwlock_create() の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int shared_result = cplat_local_rwlock_unlock_shared(
        rwlock); // [手順] - 共有ロックを取得していない状態で cplat_local_rwlock_unlock_shared() を呼び出す。
    int exclusive_result = cplat_local_rwlock_unlock_exclusive(
        rwlock); // [手順] - 排他ロックを取得していない状態で cplat_local_rwlock_unlock_exclusive() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        shared_result); // [確認_異常系] - 未取得の cplat_local_rwlock_unlock_shared() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        exclusive_result); // [確認_異常系] - 未取得の cplat_local_rwlock_unlock_exclusive() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。

    // Cleanup
    cplat_local_rwlock_dispose(rwlock);
}

// local rwlock の try_lock_exclusive が競合を BUSY へ変換することの確認
TEST(syncAdditionalFailureTest, local_rwlock_try_lock_exclusive_reports_busy)
{
    // Arrange
    cplat_local_rwlock *rwlock = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_local_rwlock_create(&rwlock)); // [状態] - local rwlock を生成する。
    // [状態確認] - cplat_local_rwlock_create() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_local_rwlock_lock_shared(rwlock, CPLAT_SYNC_NO_WAIT)); // [状態] - 共有ロックを取得しておく。
    // [状態確認] - cplat_local_rwlock_lock_shared() の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int result = cplat_local_rwlock_try_lock_exclusive(
        rwlock); // [手順] - 共有ロックを保持した状態で cplat_local_rwlock_try_lock_exclusive() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUSY,
              result); // [確認_正常系] - cplat_local_rwlock_try_lock_exclusive() の戻り値が CPLAT_ERR_BUSY であること。

    // Cleanup
    (void)cplat_local_rwlock_unlock_shared(rwlock);
    cplat_local_rwlock_dispose(rwlock);
}

// local rwlock の共有待機中エラーが UNKNOWN へ変換されることの確認
TEST(syncAdditionalFailureTest, local_rwlock_shared_wait_reports_pthread_failure)
{
    // Arrange
    cplat_local_rwlock *rwlock = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_local_rwlock_create(&rwlock)); // [状態] - local rwlock を生成する。
    // [状態確認] - cplat_local_rwlock_create() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_local_rwlock_lock_exclusive(rwlock, CPLAT_SYNC_NO_WAIT)); // [状態] - 排他ロックを取得しておく。
    // [状態確認] - cplat_local_rwlock_lock_exclusive() の戻り値が CPLAT_OK であること。
    NiceMock<Mock_pthread> mock_pthread;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_mutex_lock(_, _, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 共有待機前に pthread_mutex_lock() が 1 回呼び出されること。
                              // [Pre-Assert手順] - pthread_mutex_lock() から 0 を返す。
    EXPECT_CALL(mock_pthread, pthread_cond_wait(_, _, _, _, _))
        .WillOnce(Return(EINVAL)); // [Pre-Assert確認_異常系] - 共有待機の pthread_cond_wait() が 1 回呼び出されること。
                                   // [Pre-Assert手順] - pthread_cond_wait() から EINVAL を返す。
    EXPECT_CALL(mock_pthread, pthread_mutex_unlock(_, _, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 共有待機後に pthread_mutex_unlock() が 1 回呼び出されること。
                              // [Pre-Assert手順] - pthread_mutex_unlock() から 0 を返す。
    EXPECT_CALL(mock_pthread, pthread_mutex_destroy(_, _, _, _))
        .WillOnce(Return(0)); // pthread_mutex_destroy は Cleanup の destroy 用
    // [Pre-Assert確認_異常系] - pthread_mutex_destroy() が 1 回呼び出されること。
    // [Pre-Assert手順] - pthread_mutex_destroy() から 0 を返す。
    EXPECT_CALL(mock_pthread, pthread_cond_destroy(_, _, _, _))
        .WillRepeatedly(Return(0)); // pthread_cond_destroy は Cleanup の destroy 用
    // [Pre-Assert確認_異常系] - pthread_cond_destroy() が任意の回数呼び出されること。
    // [Pre-Assert手順] - pthread_cond_destroy() から 0 を返す。

    // Act
    int result = cplat_local_rwlock_lock_shared(
        rwlock,
        CPLAT_SYNC_WAIT_FOREVER); // [手順] - 共有待機で pthread エラーが発生する状態で cplat_local_rwlock_lock_shared() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        result); // [確認_異常系] - CPLAT_SYNC_WAIT_FOREVER を指定した cplat_local_rwlock_lock_shared() の戻り値が CPLAT_ERR_UNKNOWN であること。

    // Cleanup
    cplat_local_rwlock_dispose(rwlock);
}

// local rwlock の共有待機タイムアウトが TIMEOUT へ変換されることの確認
TEST(syncAdditionalFailureTest, local_rwlock_shared_wait_reports_timeout)
{
    // Arrange
    cplat_local_rwlock *rwlock = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_local_rwlock_create(&rwlock)); // [状態] - local rwlock を生成する。
    // [状態確認] - cplat_local_rwlock_create() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_local_rwlock_lock_exclusive(rwlock, CPLAT_SYNC_NO_WAIT)); // [状態] - 排他ロックを取得しておく。
    // [状態確認] - cplat_local_rwlock_lock_exclusive() の戻り値が CPLAT_OK であること。
    NiceMock<Mock_pthread> mock_pthread;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_mutex_lock(_, _, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 共有待機前に pthread_mutex_lock() が 1 回呼び出されること。
                              // [Pre-Assert手順] - pthread_mutex_lock() から 0 を返す。
    EXPECT_CALL(mock_pthread, pthread_cond_timedwait(_, _, _, _, _, _))
        .WillOnce(Return(
            ETIMEDOUT)); // [Pre-Assert確認_正常系] - 共有待機の pthread_cond_timedwait() が 1 回呼び出されること。
                         // [Pre-Assert手順] - pthread_cond_timedwait() から ETIMEDOUT を返す。
    EXPECT_CALL(mock_pthread, pthread_mutex_unlock(_, _, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 共有待機後に pthread_mutex_unlock() が 1 回呼び出されること。
                              // [Pre-Assert手順] - pthread_mutex_unlock() から 0 を返す。
    EXPECT_CALL(mock_pthread, pthread_mutex_destroy(_, _, _, _))
        .WillOnce(Return(0)); // pthread_mutex_destroy は Cleanup の destroy 用
    // [Pre-Assert確認_異常系] - pthread_mutex_destroy() が 1 回呼び出されること。
    // [Pre-Assert手順] - pthread_mutex_destroy() から 0 を返す。
    EXPECT_CALL(mock_pthread, pthread_cond_destroy(_, _, _, _))
        .WillRepeatedly(Return(0)); // pthread_cond_destroy は Cleanup の destroy 用
    // [Pre-Assert確認_異常系] - pthread_cond_destroy() が任意の回数呼び出されること。
    // [Pre-Assert手順] - pthread_cond_destroy() から 0 を返す。

    // Act
    int result = cplat_local_rwlock_lock_shared(
        rwlock, 1); // [手順] - 共有待機がタイムアウトする状態で cplat_local_rwlock_lock_shared() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_TIMEOUT,
        result); // [確認_正常系] - 有限待機の cplat_local_rwlock_lock_shared() の戻り値が CPLAT_ERR_TIMEOUT であること。

    // Cleanup
    cplat_local_rwlock_dispose(rwlock);
}

// local rwlock の排他待機中エラーが UNKNOWN へ変換されることの確認
TEST(syncAdditionalFailureTest, local_rwlock_exclusive_wait_reports_pthread_failure)
{
    // Arrange
    cplat_local_rwlock *rwlock = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_local_rwlock_create(&rwlock)); // [状態] - local rwlock を生成する。
    // [状態確認] - cplat_local_rwlock_create() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_local_rwlock_lock_shared(rwlock, CPLAT_SYNC_NO_WAIT)); // [状態] - 共有ロックを取得しておく。
    // [状態確認] - cplat_local_rwlock_lock_shared() の戻り値が CPLAT_OK であること。
    NiceMock<Mock_pthread> mock_pthread;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_mutex_lock(_, _, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 排他待機前に pthread_mutex_lock() が 1 回呼び出されること。
                              // [Pre-Assert手順] - pthread_mutex_lock() から 0 を返す。
    EXPECT_CALL(mock_pthread, pthread_cond_wait(_, _, _, _, _))
        .WillOnce(Return(EINVAL)); // [Pre-Assert確認_異常系] - 排他待機の pthread_cond_wait() が 1 回呼び出されること。
                                   // [Pre-Assert手順] - pthread_cond_wait() から EINVAL を返す。
    EXPECT_CALL(mock_pthread, pthread_mutex_unlock(_, _, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 排他待機後に pthread_mutex_unlock() が 1 回呼び出されること。
                              // [Pre-Assert手順] - pthread_mutex_unlock() から 0 を返す。
    EXPECT_CALL(mock_pthread, pthread_mutex_destroy(_, _, _, _))
        .WillOnce(Return(0)); // pthread_mutex_destroy は Cleanup の destroy 用
    // [Pre-Assert確認_異常系] - pthread_mutex_destroy() が 1 回呼び出されること。
    // [Pre-Assert手順] - pthread_mutex_destroy() から 0 を返す。
    EXPECT_CALL(mock_pthread, pthread_cond_destroy(_, _, _, _))
        .WillRepeatedly(Return(0)); // pthread_cond_destroy は Cleanup の destroy 用
    // [Pre-Assert確認_異常系] - pthread_cond_destroy() が任意の回数呼び出されること。
    // [Pre-Assert手順] - pthread_cond_destroy() から 0 を返す。

    // Act
    int result = cplat_local_rwlock_lock_exclusive(
        rwlock,
        CPLAT_SYNC_WAIT_FOREVER); // [手順] - 排他待機で pthread エラーが発生する状態で cplat_local_rwlock_lock_exclusive() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        result); // [確認_異常系] - CPLAT_SYNC_WAIT_FOREVER を指定した cplat_local_rwlock_lock_exclusive() の戻り値が CPLAT_ERR_UNKNOWN であること。

    // Cleanup
    cplat_local_rwlock_dispose(rwlock);
}

// local rwlock の排他待機タイムアウトが TIMEOUT へ変換されることの確認
TEST(syncAdditionalFailureTest, local_rwlock_exclusive_wait_reports_timeout)
{
    // Arrange
    cplat_local_rwlock *rwlock = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_local_rwlock_create(&rwlock)); // [状態] - local rwlock を生成する。
    // [状態確認] - cplat_local_rwlock_create() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_local_rwlock_lock_shared(rwlock, CPLAT_SYNC_NO_WAIT)); // [状態] - 共有ロックを取得しておく。
    // [状態確認] - cplat_local_rwlock_lock_shared() の戻り値が CPLAT_OK であること。
    NiceMock<Mock_pthread> mock_pthread;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_mutex_lock(_, _, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 排他待機前に pthread_mutex_lock() が 1 回呼び出されること。
                              // [Pre-Assert手順] - pthread_mutex_lock() から 0 を返す。
    EXPECT_CALL(mock_pthread, pthread_cond_timedwait(_, _, _, _, _, _))
        .WillOnce(Return(
            ETIMEDOUT)); // [Pre-Assert確認_正常系] - 排他待機の pthread_cond_timedwait() が 1 回呼び出されること。
                         // [Pre-Assert手順] - pthread_cond_timedwait() から ETIMEDOUT を返す。
    EXPECT_CALL(mock_pthread, pthread_mutex_unlock(_, _, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 排他待機後に pthread_mutex_unlock() が 1 回呼び出されること。
                              // [Pre-Assert手順] - pthread_mutex_unlock() から 0 を返す。
    EXPECT_CALL(mock_pthread, pthread_mutex_destroy(_, _, _, _))
        .WillOnce(Return(0)); // pthread_mutex_destroy は Cleanup の destroy 用
    // [Pre-Assert確認_異常系] - pthread_mutex_destroy() が 1 回呼び出されること。
    // [Pre-Assert手順] - pthread_mutex_destroy() から 0 を返す。
    EXPECT_CALL(mock_pthread, pthread_cond_destroy(_, _, _, _))
        .WillRepeatedly(Return(0)); // pthread_cond_destroy は Cleanup の destroy 用
    // [Pre-Assert確認_異常系] - pthread_cond_destroy() が任意の回数呼び出されること。
    // [Pre-Assert手順] - pthread_cond_destroy() から 0 を返す。

    // Act
    int result = cplat_local_rwlock_lock_exclusive(
        rwlock, 1); // [手順] - 排他待機がタイムアウトする状態で cplat_local_rwlock_lock_exclusive() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_TIMEOUT,
        result); // [確認_正常系] - 有限待機の cplat_local_rwlock_lock_exclusive() の戻り値が CPLAT_ERR_TIMEOUT であること。

    // Cleanup
    cplat_local_rwlock_dispose(rwlock);
}

// local rwlock の共有解放が待機中 writer を通知することの確認
TEST(syncAdditionalFailureTest, local_rwlock_shared_unlock_signals_waiting_writer)
{
    // Arrange
    cplat_local_rwlock *rwlock = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_local_rwlock_create(&rwlock)); // [状態] - local rwlock を生成する。
    // [状態確認] - cplat_local_rwlock_create() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_local_rwlock_lock_shared(rwlock, CPLAT_SYNC_NO_WAIT)); // [状態] - 共有ロックを取得しておく。
    // [状態確認] - cplat_local_rwlock_lock_shared() の戻り値が CPLAT_OK であること。
    std::atomic<bool> writer_started(false);
    std::atomic<int> writer_result(CPLAT_ERR_UNKNOWN);
    std::thread writer(
        [&]()
        {
            writer_started.store(true);
            writer_result.store(cplat_local_rwlock_lock_exclusive(rwlock, CPLAT_SYNC_WAIT_FOREVER));
            if (writer_result.load() == CPLAT_OK)
            {
                (void)cplat_local_rwlock_unlock_exclusive(rwlock);
            }
        });

    // Pre-Assert
    while (!writer_started.load())
    {
        std::this_thread::yield();
    }
    cplat_sleep_ms(5); // [状態] - writer が待機状態へ遷移する時間を確保する。

    // Act
    int unlock_result = cplat_local_rwlock_unlock_shared(
        rwlock);   // [手順] - 待機中の writer がいる状態で cplat_local_rwlock_unlock_shared() を呼び出す。
    writer.join(); // [手順] - 通知された writer の終了を待つため、writer.join() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              unlock_result); // [確認_正常系] - cplat_local_rwlock_unlock_shared() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        writer_result
            .load()); // [確認_正常系] - writer_result.load() の戻り値が CPLAT_OK であること。(通知された writer が排他ロックを取得したことの確認)
}

// local rwlock の排他解放が待機中 writer を通知することの確認
TEST(syncAdditionalFailureTest, local_rwlock_exclusive_unlock_signals_waiting_writer)
{
    // Arrange
    cplat_local_rwlock *rwlock = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_local_rwlock_create(&rwlock)); // [状態] - local rwlock を生成する。
    // [状態確認] - cplat_local_rwlock_create() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_local_rwlock_lock_exclusive(rwlock, CPLAT_SYNC_NO_WAIT)); // [状態] - 排他ロックを取得しておく。
    // [状態確認] - cplat_local_rwlock_lock_exclusive() の戻り値が CPLAT_OK であること。
    std::atomic<bool> writer_started(false);
    std::atomic<int> writer_result(CPLAT_ERR_UNKNOWN);
    std::thread writer(
        [&]()
        {
            writer_started.store(true);
            writer_result.store(cplat_local_rwlock_lock_exclusive(rwlock, CPLAT_SYNC_WAIT_FOREVER));
            if (writer_result.load() == CPLAT_OK)
            {
                (void)cplat_local_rwlock_unlock_exclusive(rwlock);
            }
        });

    // Pre-Assert
    while (!writer_started.load())
    {
        std::this_thread::yield();
    }
    cplat_sleep_ms(5); // [状態] - writer が待機状態へ遷移する時間を確保する。

    // Act
    int unlock_result = cplat_local_rwlock_unlock_exclusive(
        rwlock);   // [手順] - 待機中の writer がいる状態で cplat_local_rwlock_unlock_exclusive() を呼び出す。
    writer.join(); // [手順] - 通知された writer の終了を待つため、writer.join() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              unlock_result); // [確認_正常系] - cplat_local_rwlock_unlock_exclusive() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        writer_result
            .load()); // [確認_正常系] - writer_result.load() の戻り値が CPLAT_OK であること。(通知された writer が排他ロックを取得したことの確認)
}

// thread join の pthread エラーを未知エラーへ変換することの確認
TEST(syncAdditionalFailureTest, thread_join_reports_pthread_failure)
{
    // Arrange
    cplat_thread *thread = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_thread_create(&thread, [](void *) {}, nullptr)); // [状態] - thread を生成する。
    // [状態確認] - cplat_thread_create() の戻り値が CPLAT_OK であること。
    NiceMock<Mock_pthread> mock_pthread;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_join(_, _, _, _, _))
        .WillOnce(Return(EINVAL)); // [Pre-Assert確認_異常系] - pthread_join() が 1 回呼び出されること。
                                   // [Pre-Assert手順] - pthread_join() から EINVAL を返す。
    EXPECT_CALL(mock_pthread, pthread_detach(_, _, _, _))
        .Times(
            1); // [Pre-Assert確認_正常系] - cplat_thread_join() 失敗後の detach で pthread_detach() が 1 回呼び出されること。

    // Act
    int result = cplat_thread_join(
        thread,
        CPLAT_SYNC_WAIT_FOREVER); // [手順] - pthread_join() が EINVAL を返す状態で cplat_thread_join() を呼び出す。
    cplat_thread_detach(
        thread); // [手順] - cplat_thread_join() が失敗した後の thread を解放するため、cplat_thread_detach() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - cplat_thread_join() の戻り値が CPLAT_ERR_UNKNOWN であること。
}

// thread の即時 join timeout を TIMEOUT へ変換することの確認
TEST(syncAdditionalFailureTest, thread_join_no_wait_reports_timeout)
{
    // Arrange
    cplat_thread *thread = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_thread_create(&thread, [](void *) {}, nullptr)); // [状態] - thread を生成する。
    // [状態確認] - cplat_thread_create() の戻り値が CPLAT_OK であること。
    NiceMock<Mock_pthread> mock_pthread;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_tryjoin_np(_, _, _, _, _))
        .WillOnce(Return(EBUSY)); // [Pre-Assert確認_正常系] - pthread_tryjoin_np() が 1 回呼び出されること。
                                  // [Pre-Assert手順] - pthread_tryjoin_np() から EBUSY を返す。
    EXPECT_CALL(mock_pthread, pthread_detach(_, _, _, _))
        .Times(1); // [Pre-Assert確認_正常系] - timeout 後の detach で pthread_detach() が 1 回呼び出されること。

    // Act
    int result = cplat_thread_join(
        thread,
        CPLAT_SYNC_NO_WAIT); // [手順] - pthread_tryjoin_np() が EBUSY を返す状態で cplat_thread_join() を呼び出す。
    cplat_thread_detach(
        thread); // [手順] - タイムアウトした後の thread を解放するため、cplat_thread_detach() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_TIMEOUT,
              result); // [確認_正常系] - cplat_thread_join() の戻り値が CPLAT_ERR_TIMEOUT であること。
}

// thread の有限 join が再試行後に成功することの確認
TEST(syncAdditionalFailureTest, thread_join_finite_wait_retries_then_succeeds)
{
    // Arrange
    cplat_thread *thread = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_thread_create(&thread, [](void *) {}, nullptr)); // [状態] - thread を生成する。
    // [状態確認] - cplat_thread_create() の戻り値が CPLAT_OK であること。
    NiceMock<Mock_pthread> mock_pthread;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_tryjoin_np(_, _, _, _, _))
        .WillOnce(Return(EBUSY))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - pthread_tryjoin_np() が 2 回呼び出されること。
                              // [Pre-Assert手順] - pthread_tryjoin_np() から 1 回目は EBUSY、2 回目は 0 を返す。

    // Act
    int result = cplat_thread_join(
        thread, 5); // [手順] - 有限時間内に thread の終了待機を再試行するため、cplat_thread_join() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - 再試行後の cplat_thread_join() の戻り値が CPLAT_OK であること。
}

// thread の有限 join が未知の pthread エラーを返すことの確認
TEST(syncAdditionalFailureTest, thread_join_finite_wait_reports_pthread_failure)
{
    // Arrange
    cplat_thread *thread = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_thread_create(&thread, [](void *) {}, nullptr)); // [状態] - thread を生成する。
    // [状態確認] - cplat_thread_create() の戻り値が CPLAT_OK であること。
    NiceMock<Mock_pthread> mock_pthread;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_tryjoin_np(_, _, _, _, _))
        .WillOnce(Return(
            EINVAL)); // [Pre-Assert確認_異常系] - 有限 cplat_thread_join() の pthread_tryjoin_np() が 1 回呼び出されること。
                      // [Pre-Assert手順] - pthread_tryjoin_np() から EINVAL を返す。
    EXPECT_CALL(mock_pthread, pthread_detach(_, _, _, _))
        .Times(
            1); // [Pre-Assert確認_正常系] - cplat_thread_join() 失敗後の detach で pthread_detach() が 1 回呼び出されること。

    // Act
    int result = cplat_thread_join(
        thread,
        5); // [手順] - 未知の pthread エラーが発生する状態で、有限の待機時間を指定して cplat_thread_join() を呼び出す。
    cplat_thread_detach(
        thread); // [手順] - cplat_thread_join() が失敗した後の thread を解放するため、cplat_thread_detach() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - 有限待機の cplat_thread_join() の戻り値が CPLAT_ERR_UNKNOWN であること。
}

// interprocess lock の EWOULDBLOCK と EINTR を処理することの確認
TEST(syncAdditionalFailureTest, interprocess_lock_maps_busy_and_retries_eintr)
{
    // Arrange
    InterprocessOpenMocks os;
    NiceMock<Mock_sys_file> mock_sys_file;
    ON_CALL(mock_sys_file, flock(_, _, _, _, _))
        .WillByDefault(Return(0)); // [状態] - flock() から 0 を返すように既定動作を設定する。
    const char *path = kLockIdentity;
    cplat_interprocess_lock *lock = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_interprocess_lock_open(path, &lock)); // [状態] - interprocess lock を開いた状態とする。
    // [状態確認] - cplat_interprocess_lock_open() の戻り値が CPLAT_OK であること。

    // Pre-Assert
    EXPECT_CALL(mock_sys_file, flock(_, _, _, _, LOCK_EX | LOCK_NB))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, const int, const int)
            {
                errno = EWOULDBLOCK;
                return -1;
            })); // [Pre-Assert確認_正常系] - flock() が LOCK_EX | LOCK_NB を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - flock() から、errno に EWOULDBLOCK を設定して -1 を返す。

    // Act
    int busy_result = cplat_interprocess_lock_try_lock(
        lock); // [手順] - 非ブロッキングの flock() が errno に EWOULDBLOCK を設定する状態で cplat_interprocess_lock_try_lock() を呼び出す。

    // Pre-Assert_2
    EXPECT_CALL(mock_sys_file, flock(_, _, _, _, LOCK_EX))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, const int, const int)
            {
                errno = EINTR;
                return -1;
            }))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - flock() が LOCK_EX を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - flock() から、1 回目は errno に EINTR を設定して -1、2 回目は 0 を返す。

    // Act_2
    int retry_result = cplat_interprocess_lock_lock(
        lock,
        CPLAT_SYNC_WAIT_FOREVER); // [手順] - flock() が EINTR 後に成功する状態で cplat_interprocess_lock_lock() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUSY,
              busy_result); // [確認_正常系] - cplat_interprocess_lock_try_lock() の戻り値が CPLAT_ERR_BUSY であること。
    EXPECT_EQ(
        CPLAT_OK,
        retry_result); // [確認_正常系] - EINTR 後の cplat_interprocess_lock_lock() の戻り値が CPLAT_OK であること。

    // Cleanup
    EXPECT_CALL(mock_sys_file, flock(_, _, _, _, LOCK_UN)).WillOnce(Return(0)); // unlock 用の flock を成功させる。
    // [Pre-Assert確認_異常系] - flock() が LOCK_UN を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - flock() から 0 を返す。
    (void)cplat_interprocess_lock_unlock(lock);
    cplat_interprocess_lock_dispose(lock);
}

// thread ハンドル確保の malloc 失敗を未知エラーへ変換することの確認
TEST(syncAdditionalFailureTest, thread_create_reports_context_allocation_failure)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_thread *thread = nullptr;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_calloc(_, _))
        .WillOnce(
            Return(nullptr)); // [Pre-Assert確認_異常系] - thread ハンドルの cplat_calloc() が 1 回呼び出されること。
                              // [Pre-Assert手順] - cplat_calloc() から nullptr を返す。

    // Act
    int result = cplat_thread_create(
        &thread, [](void *) {},
        nullptr); // [手順] - thread ハンドルの cplat_calloc() が失敗する状態で cplat_thread_create() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - cplat_thread_create() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(nullptr, thread); // [確認_異常系] - 生成失敗時の thread が nullptr であること。
}

// interprocess rwlock の有限待機が成功、未知エラー、タイムアウトを分類することの確認
TEST(syncAdditionalFailureTest, interprocess_rwlock_finite_wait_classifies_results)
{
    // Arrange
    InterprocessOpenMocks os;
    const char *path = kRwlockIdentity;
    cplat_interprocess_rwlock *lock = nullptr;
    ASSERT_EQ(CPLAT_OK,
              cplat_interprocess_rwlock_open(path, &lock)); // [状態] - interprocess rwlock を開いた状態とする。
    // [状態確認] - cplat_interprocess_rwlock_open() の戻り値が CPLAT_OK であること。
    NiceMock<Mock_sys_file> mock_sys_file;
    NiceMock<Mock_time> mock_time;
    int clock_count = 0;

    // Pre-Assert
    EXPECT_CALL(mock_sys_file, flock(_, _, _, _, LOCK_SH | LOCK_NB))
        .WillOnce(Return(0))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, const int, const int)
            {
                errno = EIO;
                return -1;
            }))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, const int, const int)
            {
                errno = EWOULDBLOCK;
                return -1;
            })); // [Pre-Assert確認_正常系] - flock() が LOCK_SH | LOCK_NB を指定して 3 回呼び出されること。
    // [Pre-Assert手順] - flock() から、1 回目は 0、2 回目は errno に EIO を設定して -1、3 回目は errno に EWOULDBLOCK を設定して -1 を返す。
    EXPECT_CALL(mock_sys_file, flock(_, _, _, _, LOCK_UN))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - 取得した共有ロックの unlock で flock() が LOCK_UN を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - flock() から、LOCK_UN に対して 0 を返す。
    EXPECT_CALL(mock_time, clock_gettime(_, _, _, CLOCK_MONOTONIC, _))
        .Times(4)
        .WillRepeatedly(Invoke(
            [&clock_count](const char *, const int, const char *, const clockid_t, struct timespec *ts)
            {
                ts->tv_sec = (clock_count++ < 3) ? 0 : 1;
                ts->tv_nsec = 0;
                return 0;
            })); // [Pre-Assert確認_正常系] - 有限待機の期限判定で clock_gettime() が CLOCK_MONOTONIC を指定して 4 回呼び出されること。
    // [Pre-Assert手順] - clock_gettime() から、3 回目まで tv_sec に 0、4 回目は tv_sec に 1 を設定して 0 を返す。

    // Act
    int null_result = cplat_interprocess_rwlock_lock_shared(
        nullptr,
        CPLAT_SYNC_NO_WAIT); // [手順] - ハンドルに nullptr、待機時間に CPLAT_SYNC_NO_WAIT を指定して cplat_interprocess_rwlock_lock_shared() を呼び出す。
    int success_result = cplat_interprocess_rwlock_lock_shared(
        lock,
        1); // [手順] - 共有ロックの取得に有限の待機時間を指定して cplat_interprocess_rwlock_lock_shared() を呼び出す。
    int unlock_result = cplat_interprocess_rwlock_unlock(
        lock); // [手順] - 取得した共有ロックを指定して cplat_interprocess_rwlock_unlock() を呼び出す。
    int unknown_result = cplat_interprocess_rwlock_lock_shared(
        lock,
        1); // [手順] - flock() で未知のエラーが発生する状態で cplat_interprocess_rwlock_lock_shared() を呼び出す。
    int timeout_result = cplat_interprocess_rwlock_lock_shared(
        lock, 1); // [手順] - 有限待機の期限に到達する状態で cplat_interprocess_rwlock_lock_shared() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_result); // [確認_異常系] - nullptr ハンドルに対する cplat_interprocess_rwlock_lock_shared() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_OK,
        success_result); // [確認_正常系] - 有限待機で取得した cplat_interprocess_rwlock_lock_shared() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              unlock_result); // [確認_正常系] - cplat_interprocess_rwlock_unlock() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        unknown_result); // [確認_異常系] - flock() の未知エラー時の cplat_interprocess_rwlock_lock_shared() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        CPLAT_ERR_TIMEOUT,
        timeout_result); // [確認_正常系] - 期限到達時の cplat_interprocess_rwlock_lock_shared() の戻り値が CPLAT_ERR_TIMEOUT であること。

    // Cleanup
    cplat_interprocess_rwlock_dispose(lock);
}

// interprocess lock の有限待機とブロッキング待機のエラーを分類することの確認
TEST(syncAdditionalFailureTest, interprocess_lock_finite_and_forever_wait_classify_errors)
{
    // Arrange
    InterprocessOpenMocks os;
    const char *path = kLockIdentity;
    cplat_interprocess_lock *lock = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_interprocess_lock_open(path, &lock)); // [状態] - interprocess lock を開いた状態とする。
    // [状態確認] - cplat_interprocess_lock_open() の戻り値が CPLAT_OK であること。
    NiceMock<Mock_sys_file> mock_sys_file;
    NiceMock<Mock_time> mock_time;
    int clock_count = 0;

    // Pre-Assert
    EXPECT_CALL(mock_sys_file, flock(_, _, _, _, LOCK_EX | LOCK_NB))
        .WillOnce(Return(0))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, const int, const int)
            {
                errno = EIO;
                return -1;
            }))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, const int, const int)
            {
                errno = EWOULDBLOCK;
                return -1;
            })); // [Pre-Assert確認_正常系] - flock() が LOCK_EX | LOCK_NB を指定して 3 回呼び出されること。
    // [Pre-Assert手順] - flock() から、1 回目は 0、2 回目は errno に EIO を設定して -1、3 回目は errno に EWOULDBLOCK を設定して -1 を返す。
    EXPECT_CALL(mock_sys_file, flock(_, _, _, _, LOCK_EX))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, const int, const int)
            {
                errno = EIO;
                return -1;
            })); // [Pre-Assert確認_異常系] - flock() が LOCK_EX を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - flock() から、errno に EIO を設定して -1 を返す。
    EXPECT_CALL(mock_sys_file, flock(_, _, _, _, LOCK_UN))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - 取得したロックの unlock で flock() が LOCK_UN を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - flock() から、LOCK_UN に対して 0 を返す。
    EXPECT_CALL(mock_time, clock_gettime(_, _, _, CLOCK_MONOTONIC, _))
        .Times(4)
        .WillRepeatedly(Invoke(
            [&clock_count](const char *, const int, const char *, const clockid_t, struct timespec *ts)
            {
                ts->tv_sec = (clock_count++ < 3) ? 0 : 1;
                ts->tv_nsec = 0;
                return 0;
            })); // [Pre-Assert確認_正常系] - 有限待機の期限判定で clock_gettime() が CLOCK_MONOTONIC を指定して 4 回呼び出されること。
    // [Pre-Assert手順] - clock_gettime() から、3 回目まで tv_sec に 0、4 回目は tv_sec に 1 を設定して 0 を返す。

    // Act
    int success_result = cplat_interprocess_lock_lock(
        lock, 1); // [手順] - ロックの取得に有限の待機時間を指定して cplat_interprocess_lock_lock() を呼び出す。
    int unlock_result = cplat_interprocess_lock_unlock(
        lock); // [手順] - 取得したロックを指定して cplat_interprocess_lock_unlock() を呼び出す。
    int unknown_result = cplat_interprocess_lock_lock(
        lock,
        1); // [手順] - flock() で未知のエラーが発生する状態で、有限の待機時間を指定して cplat_interprocess_lock_lock() を呼び出す。
    int timeout_result = cplat_interprocess_lock_lock(
        lock, 1); // [手順] - flock() の有限待機が期限に到達する状態で cplat_interprocess_lock_lock() を呼び出す。
    int forever_error = cplat_interprocess_lock_lock(
        lock,
        CPLAT_SYNC_WAIT_FOREVER); // [手順] - ブロッキングの flock() で未知のエラーが発生する状態で cplat_interprocess_lock_lock() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        success_result); // [確認_正常系] - 有限待機で取得した cplat_interprocess_lock_lock() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              unlock_result); // [確認_正常系] - cplat_interprocess_lock_unlock() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        unknown_result); // [確認_異常系] - 有限待機の未知エラー時の cplat_interprocess_lock_lock() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        CPLAT_ERR_TIMEOUT,
        timeout_result); // [確認_正常系] - 期限到達時の cplat_interprocess_lock_lock() の戻り値が CPLAT_ERR_TIMEOUT であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        forever_error); // [確認_異常系] - CPLAT_SYNC_WAIT_FOREVER を指定した cplat_interprocess_lock_lock() の戻り値が CPLAT_ERR_UNKNOWN であること。

    // Cleanup
    cplat_interprocess_lock_dispose(lock);
}

// local lock の有限待機が成功、未知エラー、タイムアウトを分類することの確認
TEST(syncAdditionalFailureTest, local_lock_finite_wait_classifies_results)
{
    // Arrange
    cplat_local_lock *lock = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_local_lock_create(&lock)); // [状態] - local lock を生成する。
    // [状態確認] - cplat_local_lock_create() の戻り値が CPLAT_OK であること。
    NiceMock<Mock_pthread> mock_pthread;
    NiceMock<Mock_time> mock_time;
    int clock_count = 0;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_mutex_trylock(_, _, _, _))
        .WillOnce(Return(0))
        .WillOnce(Return(EINVAL))
        .WillOnce(Return(EBUSY)); // [Pre-Assert確認_正常系] - pthread_mutex_trylock() が 3 回呼び出されること。
    // [Pre-Assert手順] - pthread_mutex_trylock() から 1 回目は 0、2 回目は EINVAL、3 回目は EBUSY を返す。
    EXPECT_CALL(mock_time, clock_gettime(_, _, _, CLOCK_MONOTONIC, _))
        .Times(4)
        .WillRepeatedly(Invoke(
            [&clock_count](const char *, const int, const char *, const clockid_t, struct timespec *ts)
            {
                ts->tv_sec = (clock_count++ < 3) ? 0 : 1;
                ts->tv_nsec = 0;
                return 0;
            })); // [Pre-Assert確認_正常系] - 有限待機の期限判定で clock_gettime() が CLOCK_MONOTONIC を指定して 4 回呼び出されること。
    // [Pre-Assert手順] - clock_gettime() から、3 回目まで tv_sec に 0、4 回目は tv_sec に 1 を設定して 0 を返す。

    // Act
    int success_result = cplat_local_lock_lock(
        lock, 1); // [手順] - mutex の取得に有限の待機時間を指定して cplat_local_lock_lock() を呼び出す。
    int unlock_result =
        cplat_local_lock_unlock(lock); // [手順] - 取得した mutex を指定して cplat_local_lock_unlock() を呼び出す。
    int unknown_result = cplat_local_lock_lock(
        lock,
        1); // [手順] - pthread_mutex_trylock() で未知のエラーが発生する状態で cplat_local_lock_lock() を呼び出す。
    int timeout_result = cplat_local_lock_lock(
        lock,
        1); // [手順] - pthread_mutex_trylock() の再試行が期限に到達する状態で cplat_local_lock_lock() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        success_result); // [確認_正常系] - 有限待機で取得した cplat_local_lock_lock() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, unlock_result); // [確認_正常系] - cplat_local_lock_unlock() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        unknown_result); // [確認_異常系] - trylock の未知エラー時の cplat_local_lock_lock() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        CPLAT_ERR_TIMEOUT,
        timeout_result); // [確認_正常系] - 期限到達時の cplat_local_lock_lock() の戻り値が CPLAT_ERR_TIMEOUT であること。

    // Cleanup
    cplat_local_lock_dispose(lock);
}

// local lock、local rwlock、interprocess rwlock の NULL 引数が拒否されることの確認
TEST(syncAdditionalFailureTest, lock_apis_reject_null_arguments)
{
    // Arrange
    const char *path = kLockIdentity;
    cplat_thread *thread = nullptr;
    cplat_interprocess_lock *interprocess_lock = nullptr;
    cplat_interprocess_rwlock *interprocess_rwlock = nullptr;

    // Pre-Assert

    // Act
    int local_create_result = cplat_local_lock_create(
        nullptr); // [手順] - local lock の出力先に nullptr を指定して cplat_local_lock_create() を呼び出す。
    int local_unlock_result = cplat_local_lock_unlock(
        nullptr); // [手順] - local lock に nullptr を指定して cplat_local_lock_unlock() を呼び出す。
    int rwlock_create_result = cplat_local_rwlock_create(
        nullptr); // [手順] - local rwlock の出力先に nullptr を指定して cplat_local_rwlock_create() を呼び出す。
    int rwlock_shared_result = cplat_local_rwlock_lock_shared(
        nullptr,
        CPLAT_SYNC_NO_WAIT); // [手順] - local rwlock に nullptr を指定して cplat_local_rwlock_lock_shared() を呼び出す。
    int rwlock_exclusive_result = cplat_local_rwlock_lock_exclusive(
        nullptr,
        CPLAT_SYNC_NO_WAIT); // [手順] - local rwlock に nullptr を指定して cplat_local_rwlock_lock_exclusive() を呼び出す。
    int rwlock_shared_unlock_result = cplat_local_rwlock_unlock_shared(
        nullptr); // [手順] - local rwlock に nullptr を指定して cplat_local_rwlock_unlock_shared() を呼び出す。
    int rwlock_exclusive_unlock_result = cplat_local_rwlock_unlock_exclusive(
        nullptr); // [手順] - local rwlock に nullptr を指定して cplat_local_rwlock_unlock_exclusive() を呼び出す。
    int thread_create_output_result = cplat_thread_create(
        nullptr, [](void *) {},
        nullptr); // [手順] - thread の出力先に nullptr を指定して cplat_thread_create() を呼び出す。
    int thread_create_function_result = cplat_thread_create(
        &thread, nullptr,
        nullptr); // [手順] - thread の開始関数に nullptr を指定して cplat_thread_create() を呼び出す。
    int thread_join_result = cplat_thread_join(
        nullptr, CPLAT_SYNC_WAIT_FOREVER); // [手順] - thread に nullptr を指定して cplat_thread_join() を呼び出す。
    int interprocess_open_result = cplat_interprocess_rwlock_open(
        nullptr,
        &interprocess_rwlock); // [手順] - 識別子に nullptr を指定して cplat_interprocess_rwlock_open() を呼び出す。
    int interprocess_open_empty_result = cplat_interprocess_rwlock_open(
        "", &interprocess_rwlock); // [手順] - 空の識別子を指定して cplat_interprocess_rwlock_open() を呼び出す。
    int interprocess_open_output_result = cplat_interprocess_rwlock_open(
        path,
        nullptr); // [手順] - interprocess rwlock の出力先に nullptr を指定して cplat_interprocess_rwlock_open() を呼び出す。
    int lock_open_result = cplat_interprocess_lock_open(
        nullptr,
        &interprocess_lock); // [手順] - lock の識別子に nullptr を指定して cplat_interprocess_lock_open() を呼び出す。
    int lock_open_empty_result = cplat_interprocess_lock_open(
        "",
        &interprocess_lock); // [手順] - lock の識別子に空文字列を指定して cplat_interprocess_lock_open() を呼び出す。
    int lock_open_output_result = cplat_interprocess_lock_open(
        path, nullptr); // [手順] - lock の出力先に nullptr を指定して cplat_interprocess_lock_open() を呼び出す。
    int lock_result = cplat_interprocess_lock_lock(
        nullptr, CPLAT_SYNC_NO_WAIT); // [手順] - lock に nullptr を指定して cplat_interprocess_lock_lock() を呼び出す。
    int lock_negative_timeout_result = cplat_interprocess_lock_lock(
        nullptr, -1); // [手順] - 負の待機時間を指定して cplat_interprocess_lock_lock() を呼び出す。
    int lock_export_result = cplat_interprocess_lock_export_descriptor(
        nullptr, nullptr,
        nullptr); // [手順] - lock に nullptr を指定して cplat_interprocess_lock_export_descriptor() を呼び出す。
    int lock_import_result = cplat_interprocess_lock_import_descriptor(
        nullptr, 0U,
        nullptr); // [手順] - lock descriptor の出力先に nullptr を指定して cplat_interprocess_lock_import_descriptor() を呼び出す。
    int lock_unlock_result = cplat_interprocess_lock_unlock(
        nullptr); // [手順] - lock に nullptr を指定して cplat_interprocess_lock_unlock() を呼び出す。
    int interprocess_shared_result = cplat_interprocess_rwlock_lock_shared(
        nullptr,
        CPLAT_SYNC_NO_WAIT); // [手順] - interprocess rwlock に nullptr を指定して cplat_interprocess_rwlock_lock_shared() を呼び出す。
    int interprocess_shared_negative_timeout_result = cplat_interprocess_rwlock_lock_shared(
        nullptr,
        -1); // [手順] - 共有ロックの取得に負の待機時間を指定して cplat_interprocess_rwlock_lock_shared() を呼び出す。
    int interprocess_exclusive_negative_timeout_result = cplat_interprocess_rwlock_lock_exclusive(
        nullptr,
        -1); // [手順] - 排他ロックの取得に負の待機時間を指定して cplat_interprocess_rwlock_lock_exclusive() を呼び出す。
    int interprocess_export_result = cplat_interprocess_rwlock_export_descriptor(
        nullptr, nullptr,
        nullptr); // [手順] - ハンドルに nullptr を指定して cplat_interprocess_rwlock_export_descriptor() を呼び出す。
    int interprocess_import_result = cplat_interprocess_rwlock_import_descriptor(
        nullptr, 0U,
        nullptr); // [手順] - descriptor の出力先に nullptr を指定して cplat_interprocess_rwlock_import_descriptor() を呼び出す。
    int interprocess_unlock_result = cplat_interprocess_rwlock_unlock(
        nullptr); // [手順] - interprocess rwlock に nullptr を指定して cplat_interprocess_rwlock_unlock() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        local_create_result); // [確認_異常系] - nullptr を出力先に指定した cplat_local_lock_create() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        local_unlock_result); // [確認_異常系] - nullptr に対する cplat_local_lock_unlock() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        rwlock_create_result); // [確認_異常系] - nullptr を出力先に指定した cplat_local_rwlock_create() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        rwlock_shared_result); // [確認_異常系] - nullptr に対する cplat_local_rwlock_lock_shared() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        rwlock_exclusive_result); // [確認_異常系] - nullptr に対する cplat_local_rwlock_lock_exclusive() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        rwlock_shared_unlock_result); // [確認_異常系] - nullptr に対する cplat_local_rwlock_unlock_shared() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        rwlock_exclusive_unlock_result); // [確認_異常系] - nullptr に対する cplat_local_rwlock_unlock_exclusive() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        thread_create_output_result); // [確認_異常系] - nullptr を出力先に指定した cplat_thread_create() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        thread_create_function_result); // [確認_異常系] - nullptr を開始関数に指定した cplat_thread_create() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        thread_join_result); // [確認_異常系] - nullptr に対する cplat_thread_join() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        interprocess_open_result); // [確認_異常系] - nullptr を識別子に指定した cplat_interprocess_rwlock_open() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        interprocess_open_empty_result); // [確認_異常系] - "" を識別子に指定した cplat_interprocess_rwlock_open() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        interprocess_open_output_result); // [確認_異常系] - nullptr を出力先に指定した cplat_interprocess_rwlock_open() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        lock_open_result); // [確認_異常系] - nullptr を識別子に指定した cplat_interprocess_lock_open() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        lock_open_empty_result); // [確認_異常系] - "" を識別子に指定した cplat_interprocess_lock_open() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        lock_open_output_result); // [確認_異常系] - nullptr を出力先に指定した cplat_interprocess_lock_open() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        lock_result); // [確認_異常系] - nullptr に対する cplat_interprocess_lock_lock() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        lock_negative_timeout_result); // [確認_異常系] - 待機時間 -1 を指定した cplat_interprocess_lock_lock() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        lock_export_result); // [確認_異常系] - nullptr に対する cplat_interprocess_lock_export_descriptor() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        lock_import_result); // [確認_異常系] - nullptr を出力先に指定した cplat_interprocess_lock_import_descriptor() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        lock_unlock_result); // [確認_異常系] - nullptr に対する cplat_interprocess_lock_unlock() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        interprocess_shared_result); // [確認_異常系] - nullptr に対する cplat_interprocess_rwlock_lock_shared() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        interprocess_shared_negative_timeout_result); // [確認_異常系] - 待機時間 -1 を指定した cplat_interprocess_rwlock_lock_shared() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        interprocess_exclusive_negative_timeout_result); // [確認_異常系] - 待機時間 -1 を指定した cplat_interprocess_rwlock_lock_exclusive() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        interprocess_export_result); // [確認_異常系] - nullptr に対する cplat_interprocess_rwlock_export_descriptor() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        interprocess_import_result); // [確認_異常系] - nullptr を出力先に指定した cplat_interprocess_rwlock_import_descriptor() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        interprocess_unlock_result); // [確認_異常系] - nullptr に対する cplat_interprocess_rwlock_unlock() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// interprocess rwlock の open が OS エラーを返すことの確認
TEST(syncAdditionalFailureTest, interprocess_rwlock_open_reports_open_failure)
{
    // Arrange
    NiceMock<Mock_fcntl> mock_fcntl;
    const char *path = kRwlockIdentity;
    cplat_interprocess_rwlock *lock = nullptr;

    // Pre-Assert
    EXPECT_CALL(mock_fcntl, open(_, _, _, _, _, _))
        .WillOnce(
            Return(-1)); // [Pre-Assert確認_異常系] - rwlock の lock file に対する open() が 1 回呼び出されること。
                         // [Pre-Assert手順] - open() から -1 を返す。

    // Act
    int result = cplat_interprocess_rwlock_open(
        path, &lock); // [手順] - open() が失敗する状態で cplat_interprocess_rwlock_open() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - cplat_interprocess_rwlock_open() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(nullptr,
              lock); // [確認_異常系] - open() 失敗時の lock が nullptr であること。
}

// interprocess rwlock の識別子複製失敗が通知されることの確認
TEST(syncAdditionalFailureTest, interprocess_rwlock_open_reports_identity_duplication_failure)
{
    // Arrange
    InterprocessOpenMocks os;
    NiceMock<Mock_cplat> mock_cplat;
    const char *path = kRwlockIdentity;
    cplat_interprocess_rwlock *lock = nullptr;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_strdup(_))
        .WillOnce(Return(
            nullptr)); // [Pre-Assert確認_異常系] - rwlock の識別子複製で cplat_strdup() が 1 回呼び出されること。
                       // [Pre-Assert手順] - cplat_strdup() から nullptr を返す。

    // Act
    int result = cplat_interprocess_rwlock_open(
        path, &lock); // [手順] - 識別子の複製が失敗する状態で cplat_interprocess_rwlock_open() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        result); // [確認_異常系] - 識別子複製失敗時の cplat_interprocess_rwlock_open() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(nullptr,
              lock); // [確認_異常系] - 識別子複製失敗時の lock が nullptr であること。
}

// interprocess rwlock のハンドル確保失敗が通知されることの確認
TEST(syncAdditionalFailureTest, interprocess_rwlock_open_reports_allocation_failure)
{
    // Arrange
    InterprocessOpenMocks os;
    NiceMock<Mock_cplat> mock_cplat;
    const char *path = kRwlockIdentity;
    cplat_interprocess_rwlock *lock = nullptr;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_calloc(_, _))
        .WillOnce(
            Return(nullptr)); // [Pre-Assert確認_異常系] - rwlock ハンドルの cplat_calloc() が 1 回呼び出されること。
                              // [Pre-Assert手順] - cplat_calloc() から nullptr を返す。

    // Act
    int result = cplat_interprocess_rwlock_open(
        path, &lock); // [手順] - ハンドルの確保が失敗する状態で cplat_interprocess_rwlock_open() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        result); // [確認_異常系] - ハンドル確保失敗時の cplat_interprocess_rwlock_open() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(nullptr,
              lock); // [確認_異常系] - ハンドル確保失敗時の lock が nullptr であること。
}

// interprocess rwlock の WAIT_FOREVER が EINTR 後に再試行することの確認
TEST(syncAdditionalFailureTest, interprocess_rwlock_wait_forever_retries_eintr)
{
    // Arrange
    InterprocessOpenMocks os;
    NiceMock<Mock_sys_file> mock_sys_file;
    ON_CALL(mock_sys_file, flock(_, _, _, _, _))
        .WillByDefault(Return(0)); // [状態] - flock() から 0 を返すように既定動作を設定する。
    const char *path = kRwlockIdentity;
    cplat_interprocess_rwlock *lock = nullptr;
    ASSERT_EQ(CPLAT_OK,
              cplat_interprocess_rwlock_open(path, &lock)); // [状態] - interprocess rwlock を開いた状態とする。
    // [状態確認] - cplat_interprocess_rwlock_open() の戻り値が CPLAT_OK であること。

    // Pre-Assert
    EXPECT_CALL(mock_sys_file, flock(_, _, _, _, LOCK_EX))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, const int, const int)
            {
                errno = EINTR;
                return -1;
            }))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - flock() が LOCK_EX を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - flock() から、1 回目は errno に EINTR を設定して -1、2 回目は 0 を返す。

    // Act
    int result = cplat_interprocess_rwlock_lock_exclusive(
        lock,
        CPLAT_SYNC_WAIT_FOREVER); // [手順] - EINTR 後に排他ロックの取得が成功する状態で cplat_interprocess_rwlock_lock_exclusive() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        result); // [確認_正常系] - EINTR 後の cplat_interprocess_rwlock_lock_exclusive() の戻り値が CPLAT_OK であること。

    // Cleanup
    EXPECT_CALL(mock_sys_file, flock(_, _, _, _, LOCK_UN)).WillOnce(Return(0)); // unlock 用の flock を成功させる。
    // [Pre-Assert確認_異常系] - flock() が LOCK_UN を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - flock() から 0 を返す。
    (void)cplat_interprocess_rwlock_unlock(lock);
    cplat_interprocess_rwlock_dispose(lock);
}

// interprocess rwlock の WAIT_FOREVER が未知の flock エラーを返すことの確認
TEST(syncAdditionalFailureTest, interprocess_rwlock_wait_forever_reports_flock_failure)
{
    // Arrange
    InterprocessOpenMocks os;
    NiceMock<Mock_sys_file> mock_sys_file;
    ON_CALL(mock_sys_file, flock(_, _, _, _, _))
        .WillByDefault(Return(0)); // [状態] - flock() から 0 を返すように既定動作を設定する。
    const char *path = kRwlockIdentity;
    cplat_interprocess_rwlock *lock = nullptr;
    ASSERT_EQ(CPLAT_OK,
              cplat_interprocess_rwlock_open(path, &lock)); // [状態] - interprocess rwlock を開いた状態とする。
    // [状態確認] - cplat_interprocess_rwlock_open() の戻り値が CPLAT_OK であること。

    // Pre-Assert
    EXPECT_CALL(mock_sys_file, flock(_, _, _, _, LOCK_SH))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, const int, const int)
            {
                errno = EIO;
                return -1;
            })); // [Pre-Assert確認_異常系] - flock() が LOCK_SH を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - flock() から、errno に EIO を設定して -1 を返す。

    // Act
    int result = cplat_interprocess_rwlock_lock_shared(
        lock,
        CPLAT_SYNC_WAIT_FOREVER); // [手順] - ブロッキングの flock() で未知のエラーが発生する状態で cplat_interprocess_rwlock_lock_shared() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        result); // [確認_異常系] - CPLAT_SYNC_WAIT_FOREVER を指定した cplat_interprocess_rwlock_lock_shared() の戻り値が CPLAT_ERR_UNKNOWN であること。

    // Cleanup
    cplat_interprocess_rwlock_dispose(lock);
}

// interprocess rwlock の unlock 失敗が通知され、destroy が再試行することの確認
TEST(syncAdditionalFailureTest, interprocess_rwlock_unlock_reports_flock_failure)
{
    // Arrange
    InterprocessOpenMocks os;
    NiceMock<Mock_sys_file> mock_sys_file;
    ON_CALL(mock_sys_file, flock(_, _, _, _, _))
        .WillByDefault(Return(0)); // [状態] - flock() から 0 を返すように既定動作を設定する。
    const char *path = kRwlockIdentity;
    cplat_interprocess_rwlock *lock = nullptr;
    ASSERT_EQ(CPLAT_OK,
              cplat_interprocess_rwlock_open(path, &lock)); // [状態] - interprocess rwlock を開いた状態とする。
    // [状態確認] - cplat_interprocess_rwlock_open() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_interprocess_rwlock_lock_exclusive(
                            lock, CPLAT_SYNC_NO_WAIT)); // [状態] - 排他ロックを取得しておく。
    // [状態確認] - cplat_interprocess_rwlock_lock_exclusive() の戻り値が CPLAT_OK であること。

    // Pre-Assert
    EXPECT_CALL(mock_sys_file, flock(_, _, _, _, LOCK_UN))
        .WillRepeatedly(Return(
            -1)); // [Pre-Assert確認_異常系] - unlock の flock() が LOCK_UN を指定して任意の回数呼び出されること。
                  // [Pre-Assert手順] - flock() から、LOCK_UN に対して -1 を返す。

    // Act
    int result = cplat_interprocess_rwlock_unlock(
        lock); // [手順] - ロック解放時の flock() が失敗する状態で cplat_interprocess_rwlock_unlock() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - cplat_interprocess_rwlock_unlock() の戻り値が CPLAT_ERR_UNKNOWN であること。

    // Cleanup
    cplat_interprocess_rwlock_dispose(lock);
}

// interprocess rwlock の try_lock_exclusive がロックを取得できることの確認
TEST(syncAdditionalFailureTest, interprocess_rwlock_try_lock_exclusive_succeeds)
{
    // Arrange
    InterprocessOsMocks os;
    const char *path = kRwlockIdentity;
    cplat_interprocess_rwlock *lock = nullptr;
    ASSERT_EQ(CPLAT_OK,
              cplat_interprocess_rwlock_open(path, &lock)); // [状態] - interprocess rwlock を開いた状態とする。
    // [状態確認] - cplat_interprocess_rwlock_open() の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int result = cplat_interprocess_rwlock_try_lock_exclusive(
        lock); // [手順] - interprocess rwlock を指定して cplat_interprocess_rwlock_try_lock_exclusive() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        result); // [確認_正常系] - cplat_interprocess_rwlock_try_lock_exclusive() の戻り値が CPLAT_OK であること。

    // Cleanup
    (void)cplat_interprocess_rwlock_unlock(lock);
    cplat_interprocess_rwlock_dispose(lock);
}

// call_once の待機側が初期化完了まで待つことの確認
TEST(syncAdditionalFailureTest, call_once_waits_for_running_callback)
{
    // Arrange
    cplat_once_flag flag = {0};
    s_once_callback_entered.store(false);
    s_once_callback_release.store(false);
    s_once_waiter_started.store(false);
    std::thread initializer([&]() { cplat_call_once(&flag, blocked_once_callback); });
    while (!s_once_callback_entered.load())
    {
        std::this_thread::yield();
    }
    std::thread waiter(
        [&]()
        {
            s_once_waiter_started.store(true);
            cplat_call_once(&flag, blocked_once_callback);
        });

    // Pre-Assert
    while (!s_once_waiter_started.load())
    {
        std::this_thread::yield();
    }
    cplat_sleep_ms(5); // [状態] - waiter が待機ループへ遷移する時間を確保する。

    // Act
    s_once_callback_release.store(
        true); // [手順] - 初回のコールバックを終了させるため、true を指定して s_once_callback_release.store() を呼び出す。
    initializer.join(); // [手順] - 初期化側 thread の終了を待つため、initializer.join() を呼び出す。
    waiter.join();      // [手順] - 待機側 thread の終了を待つため、waiter.join() を呼び出す。

    // Assert
    EXPECT_EQ(
        2,
        cplat_atomic_load_i32(
            &flag.state,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_正常系] - callback 完了後の cplat_atomic_load_i32() の戻り値が 2 であること。(初期化済み状態であることの確認)
}

// interprocess lock が重複取得、有限 EINTR、再試行成功を分類することの確認
TEST(syncAdditionalFailureTest, interprocess_locks_cover_locked_and_finite_retry_paths)
{
    // Arrange
    InterprocessOpenMocks os;
    const char *lock_path = kLockIdentity;
    const char *rwlock_path = kRwlockIdentity;
    cplat_interprocess_lock *lock = nullptr;
    cplat_interprocess_rwlock *rwlock = nullptr;
    ASSERT_EQ(CPLAT_OK,
              cplat_interprocess_lock_open(lock_path, &lock)); // [状態] - interprocess lock を開いた状態とする。
    // [状態確認] - cplat_interprocess_lock_open() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_interprocess_rwlock_open(rwlock_path,
                                                       &rwlock)); // [状態] - interprocess rwlock を開いた状態とする。
    // [状態確認] - cplat_interprocess_rwlock_open() の戻り値が CPLAT_OK であること。
    NiceMock<Mock_sys_file> mock_sys_file;
    ON_CALL(mock_sys_file, flock(_, _, _, _, _))
        .WillByDefault(Return(0)); // [状態] - flock() から 0 を返すように既定動作を設定する。
    NiceMock<Mock_time> mock_time;

    // Pre-Assert
    EXPECT_CALL(mock_time, clock_gettime(_, _, _, CLOCK_MONOTONIC, _))
        .Times(4)
        .WillRepeatedly(Invoke(
            [](const char *, const int, const char *, const clockid_t, struct timespec *ts)
            {
                ts->tv_sec = 0;
                ts->tv_nsec = 0;
                return 0;
            })); // [Pre-Assert確認_正常系] - 有限待機の期限判定で clock_gettime() が CLOCK_MONOTONIC を指定して 4 回呼び出されること。
    // [Pre-Assert手順] - clock_gettime() から、tv_sec に 0 を設定して 0 を返す。
    EXPECT_CALL(mock_sys_file, flock(_, _, _, _, LOCK_EX | LOCK_NB))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, const int, const int)
            {
                errno = EINTR;
                return -1;
            }))
        .WillOnce(
            Return(0)); // [Pre-Assert確認_正常系] - flock() が LOCK_EX | LOCK_NB を指定して 2 回呼び出されること。
                        // [Pre-Assert手順] - flock() から、1 回目は errno に EINTR を設定して -1、2 回目は 0 を返す。
    EXPECT_CALL(mock_sys_file, flock(_, _, _, _, LOCK_SH | LOCK_NB))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, const int, const int)
            {
                errno = EINTR;
                return -1;
            }))
        .WillOnce(
            Return(0)); // [Pre-Assert確認_正常系] - flock() が LOCK_SH | LOCK_NB を指定して 2 回呼び出されること。
                        // [Pre-Assert手順] - flock() から、1 回目は errno に EINTR を設定して -1、2 回目は 0 を返す。

    // Act
    int lock_result = cplat_interprocess_lock_lock(
        lock,
        1); // [手順] - EINTR 後にロックの取得が成功する状態で、有限の待機時間を指定して cplat_interprocess_lock_lock() を呼び出す。
    int lock_duplicate = cplat_interprocess_lock_lock(
        lock, 1); // [手順] - 取得済みの lock を指定して cplat_interprocess_lock_lock() を呼び出す。
    int rwlock_result = cplat_interprocess_rwlock_lock_shared(
        rwlock,
        1); // [手順] - EINTR 後に共有ロックの取得が成功する状態で、有限の待機時間を指定して cplat_interprocess_rwlock_lock_shared() を呼び出す。
    int rwlock_duplicate = cplat_interprocess_rwlock_lock_shared(
        rwlock, 1); // [手順] - 取得済みの rwlock を指定して cplat_interprocess_rwlock_lock_shared() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        lock_result); // [確認_正常系] - EINTR 後の cplat_interprocess_lock_lock() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        lock_duplicate); // [確認_異常系] - 取得済みに対する cplat_interprocess_lock_lock() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_OK,
        rwlock_result); // [確認_正常系] - EINTR 後の cplat_interprocess_rwlock_lock_shared() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        rwlock_duplicate); // [確認_異常系] - 取得済みに対する cplat_interprocess_rwlock_lock_shared() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。

    // Cleanup
    EXPECT_CALL(mock_sys_file, flock(_, _, _, _, LOCK_UN))
        .Times(2)
        .WillRepeatedly(Return(0)); // unlock 用の flock を成功させる。
    // [Pre-Assert確認_異常系] - flock() が LOCK_UN を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - flock() から 0 を返す。
    (void)cplat_interprocess_lock_unlock(lock);
    (void)cplat_interprocess_rwlock_unlock(rwlock);
    cplat_interprocess_lock_dispose(lock);
    cplat_interprocess_rwlock_dispose(rwlock);
}

// local synchronization API が非 NULL ハンドルの負値と不足引数を拒否することの確認
TEST(syncAdditionalFailureTest, local_sync_rejects_negative_and_partial_arguments)
{
    // Arrange
    cplat_local_lock *lock = nullptr;
    cplat_local_rwlock *rwlock = nullptr;
    cplat_condvar *cv = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_local_lock_create(&lock)); // [状態] - local lock を生成する。
    // [状態確認] - cplat_local_lock_create() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_local_rwlock_create(&rwlock)); // [状態] - local rwlock を生成する。
    // [状態確認] - cplat_local_rwlock_create() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_condvar_create(&cv)); // [状態] - condvar を生成する。
    // [状態確認] - cplat_condvar_create() の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int null_lock_result = cplat_local_lock_lock(
        nullptr, CPLAT_SYNC_NO_WAIT); // [手順] - local lock に nullptr を指定して cplat_local_lock_lock() を呼び出す。
    int lock_result = cplat_local_lock_lock(
        lock, -2); // [手順] - nullptr 以外の lock と負の待機時間を指定して cplat_local_lock_lock() を呼び出す。
    int shared_result = cplat_local_rwlock_lock_shared(
        rwlock,
        -2); // [手順] - nullptr 以外の rwlock と負の待機時間を指定して cplat_local_rwlock_lock_shared() を呼び出す。
    int exclusive_result = cplat_local_rwlock_lock_exclusive(
        rwlock,
        -2); // [手順] - nullptr 以外の rwlock と負の待機時間を指定して cplat_local_rwlock_lock_exclusive() を呼び出す。
    int null_lock_wait = cplat_condvar_wait(
        cv, nullptr, 0); // [手順] - condvar 待機の lock に nullptr を指定して cplat_condvar_wait() を呼び出す。
    int negative_wait = cplat_condvar_wait(
        cv, lock, -2); // [手順] - condvar 待機に負の待機時間を指定して cplat_condvar_wait() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_lock_result); // [確認_異常系] - nullptr に対する cplat_local_lock_lock() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        lock_result); // [確認_異常系] - 待機時間 -2 を指定した cplat_local_lock_lock() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        shared_result); // [確認_異常系] - 待機時間 -2 を指定した cplat_local_rwlock_lock_shared() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        exclusive_result); // [確認_異常系] - 待機時間 -2 を指定した cplat_local_rwlock_lock_exclusive() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_lock_wait); // [確認_異常系] - nullptr の lock に対する cplat_condvar_wait() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        negative_wait); // [確認_異常系] - 待機時間 -2 を指定した cplat_condvar_wait() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。

    // Cleanup
    cplat_condvar_dispose(cv);
    cplat_local_rwlock_dispose(rwlock);
    cplat_local_lock_dispose(lock);
}

// local rwlock 初期化が readers と writers の condvar 失敗を分類することの確認
TEST(syncAdditionalFailureTest, local_rwlock_create_reports_each_condvar_failure)
{
    // Arrange
    NiceMock<Mock_pthread> mock_pthread;
    cplat_local_rwlock *readers_failure = nullptr;
    cplat_local_rwlock *writers_failure = nullptr;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_cond_init(_, _, _, _, _))
        .WillOnce(Return(EINVAL))
        .WillOnce(Return(0))
        .WillOnce(Return(EINVAL)); // [Pre-Assert確認_異常系] - pthread_cond_init() が 3 回呼び出されること。
    // [Pre-Assert手順] - pthread_cond_init() から 1 回目は EINVAL、2 回目は 0、3 回目は EINVAL を返す。

    // Act
    int readers_result = cplat_local_rwlock_create(
        &readers_failure); // [手順] - readers の condvar の初期化が失敗する状態で cplat_local_rwlock_create() を呼び出す。
    int writers_result = cplat_local_rwlock_create(
        &writers_failure); // [手順] - writers の condvar の初期化が失敗する状態で cplat_local_rwlock_create() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        readers_result); // [確認_異常系] - readers の初期化失敗時の cplat_local_rwlock_create() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        writers_result); // [確認_異常系] - writers の初期化失敗時の cplat_local_rwlock_create() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(nullptr,
              readers_failure); // [確認_異常系] - readers 初期化失敗時の readers_failure が nullptr であること。
    EXPECT_EQ(nullptr,
              writers_failure); // [確認_異常系] - writers 初期化失敗時の writers_failure が nullptr であること。
}

// waiting writer を待つ reader が通知後に取得を完了することの確認
TEST(syncAdditionalFailureTest, local_rwlock_reader_resumes_after_writer_state_clears)
{
    // Arrange
    cplat_local_rwlock *rwlock = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_local_rwlock_create(&rwlock)); // [状態] - local rwlock を生成する。
    // [状態確認] - cplat_local_rwlock_create() の戻り値が CPLAT_OK であること。
    test_sync_set_local_rwlock_state(rwlock, 0, 0U, 1U);
    NiceMock<Mock_pthread> mock_pthread;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_cond_wait(_, _, _, _, _))
        .WillOnce(Invoke(
            [rwlock](const char *, const int, const char *, pthread_cond_t *, pthread_mutex_t *)
            {
                test_sync_set_local_rwlock_state(rwlock, 0, 0U, 0U);
                return 0;
            })); // [Pre-Assert確認_正常系] - waiting writer 待ちの pthread_cond_wait() が 1 回呼び出されること。
                 // [Pre-Assert手順] - pthread_cond_wait() から、通知時に待機中の writer を消して 0 を返す。

    // Act
    int lock_result = cplat_local_rwlock_lock_shared(
        rwlock,
        CPLAT_SYNC_WAIT_FOREVER); // [手順] - 待機中の writer が完了する状態で cplat_local_rwlock_lock_shared() を呼び出す。
    int unlock_result = cplat_local_rwlock_unlock_shared(
        rwlock); // [手順] - 取得した共有 lock を指定して cplat_local_rwlock_unlock_shared() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        lock_result); // [確認_正常系] - 通知後の cplat_local_rwlock_lock_shared() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              unlock_result); // [確認_正常系] - cplat_local_rwlock_unlock_shared() の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_local_rwlock_dispose(rwlock);
}

// thread API が context 確保失敗と NULL detach を処理することの確認
TEST(syncAdditionalFailureTest, thread_apis_cover_context_failure_and_null_detach)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_thread *allocation_failure = nullptr;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_malloc(_))
        .WillOnce(
            Return(nullptr)); // [Pre-Assert確認_異常系] - thread context の cplat_malloc() が 1 回呼び出されること。
                              // [Pre-Assert手順] - cplat_malloc() から nullptr を返す。

    // Act
    int allocation_result = cplat_thread_create(
        &allocation_failure, [](void *) {},
        nullptr);                 // [手順] - thread context の確保が失敗する状態で cplat_thread_create() を呼び出す。
    cplat_thread_detach(nullptr); // [手順] - thread に nullptr を指定して cplat_thread_detach() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              allocation_result); // [確認_異常系] - cplat_thread_create() の戻り値が CPLAT_ERR_UNKNOWN であること。
    SUCCEED(); // [確認_正常系] - SUCCEED() に到達すること。(nullptr thread に対する cplat_thread_detach() が処理を終えたことの確認)
}

// thread join が非 NULL thread に対する負の待機時間を拒否することの確認
TEST(syncAdditionalFailureTest, thread_join_rejects_negative_timeout)
{
    // Arrange
    cplat_thread *thread = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_thread_create(&thread, [](void *) {}, nullptr)); // [状態] - thread を生成する。
    // [状態確認] - cplat_thread_create() の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int negative_result = cplat_thread_join(
        thread, -2); // [手順] - nullptr 以外の thread と負の待機時間を指定して cplat_thread_join() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        negative_result); // [確認_異常系] - 待機時間 -2 を指定した cplat_thread_join() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。

    // Cleanup
    cplat_thread_detach(thread);
}

// thread の有限 join が期限前に再試行することの確認
TEST(syncAdditionalFailureTest, thread_join_finite_retry_uses_monotonic_deadline)
{
    // Arrange
    cplat_thread *thread = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_thread_create(&thread, [](void *) {}, nullptr)); // [状態] - thread を生成する。
    // [状態確認] - cplat_thread_create() の戻り値が CPLAT_OK であること。
    NiceMock<Mock_pthread> mock_pthread;
    NiceMock<Mock_time> mock_time;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_tryjoin_np(_, _, _, _, _))
        .WillOnce(Return(EBUSY))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - pthread_tryjoin_np() が 2 回呼び出されること。
                              // [Pre-Assert手順] - pthread_tryjoin_np() から 1 回目は EBUSY、2 回目は 0 を返す。
    EXPECT_CALL(mock_time, clock_gettime(_, _, _, CLOCK_MONOTONIC, _))
        .Times(2)
        .WillRepeatedly(Invoke(
            [](const char *, const int, const char *, const clockid_t, struct timespec *ts)
            {
                ts->tv_sec = 0;
                ts->tv_nsec = 0;
                return 0;
            })); // [Pre-Assert確認_正常系] - 有限 cplat_thread_join() の期限判定で clock_gettime() が CLOCK_MONOTONIC を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - clock_gettime() から、tv_sec に 0 を設定して 0 を返す。

    // Act
    int result = cplat_thread_join(
        thread, 1); // [手順] - 期限前の EBUSY 後に終了待機を再試行する状態で cplat_thread_join() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, result); // [確認_正常系] - 再試行後の cplat_thread_join() の戻り値が CPLAT_OK であること。
}

// thread の有限 join が deadline 到達時に timeout を返すことの確認
TEST(syncAdditionalFailureTest, thread_join_finite_wait_reports_deadline_timeout)
{
    // Arrange
    cplat_thread *thread = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_thread_create(&thread, [](void *) {}, nullptr)); // [状態] - thread を生成する。
    // [状態確認] - cplat_thread_create() の戻り値が CPLAT_OK であること。
    NiceMock<Mock_pthread> mock_pthread;
    NiceMock<Mock_time> mock_time;
    int clock_count = 0;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_tryjoin_np(_, _, _, _, _))
        .WillOnce(Return(EBUSY)); // [Pre-Assert確認_正常系] - pthread_tryjoin_np() が 1 回呼び出されること。
                                  // [Pre-Assert手順] - pthread_tryjoin_np() から EBUSY を返す。
    EXPECT_CALL(mock_time, clock_gettime(_, _, _, CLOCK_MONOTONIC, _))
        .Times(2)
        .WillRepeatedly(Invoke(
            [&clock_count](const char *, const int, const char *, const clockid_t, struct timespec *ts)
            {
                ts->tv_sec = clock_count++;
                ts->tv_nsec = 0;
                return 0;
            })); // [Pre-Assert確認_正常系] - 有限 cplat_thread_join() の期限判定で clock_gettime() が CLOCK_MONOTONIC を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - clock_gettime() から、1 回目は tv_sec に 0、2 回目は tv_sec に 1 を設定して 0 を返す。

    // Act
    int result = cplat_thread_join(
        thread,
        1); // [手順] - EBUSY のまま期限に到達する状態で、有限の待機時間を指定して cplat_thread_join() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_TIMEOUT,
        result); // [確認_正常系] - deadline 到達時の cplat_thread_join() の戻り値が CPLAT_ERR_TIMEOUT であること。

    // Cleanup
    cplat_thread_detach(thread);
}

// interprocess lock API が未取得ハンドルと NULL destroy を処理することの確認
TEST(syncAdditionalFailureTest, interprocess_locks_reject_unlocked_and_accept_null_destroy)
{
    // Arrange
    InterprocessOpenMocks os;
    const char *lock_path = kLockIdentity;
    const char *rwlock_path = kRwlockIdentity;
    cplat_interprocess_lock *lock = nullptr;
    cplat_interprocess_rwlock *rwlock = nullptr;
    ASSERT_EQ(CPLAT_OK,
              cplat_interprocess_lock_open(lock_path, &lock)); // [状態] - interprocess lock を開いた状態とする。
    // [状態確認] - cplat_interprocess_lock_open() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_interprocess_rwlock_open(rwlock_path,
                                                       &rwlock)); // [状態] - interprocess rwlock を開いた状態とする。
    // [状態確認] - cplat_interprocess_rwlock_open() の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int lock_result = cplat_interprocess_lock_unlock(
        lock); // [手順] - 未取得の interprocess lock を指定して cplat_interprocess_lock_unlock() を呼び出す。
    int rwlock_result = cplat_interprocess_rwlock_unlock(
        rwlock); // [手順] - 未取得の interprocess rwlock を指定して cplat_interprocess_rwlock_unlock() を呼び出す。
    cplat_local_rwlock_dispose(
        nullptr); // [手順] - local rwlock に nullptr を指定して cplat_local_rwlock_dispose() を呼び出す。
    cplat_interprocess_lock_dispose(
        nullptr); // [手順] - interprocess lock に nullptr を指定して cplat_interprocess_lock_dispose() を呼び出す。
    cplat_interprocess_rwlock_dispose(
        nullptr); // [手順] - interprocess rwlock に nullptr を指定して cplat_interprocess_rwlock_dispose() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        lock_result); // [確認_異常系] - 未取得の cplat_interprocess_lock_unlock() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        rwlock_result); // [確認_異常系] - 未取得の cplat_interprocess_rwlock_unlock() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。

    // Cleanup
    cplat_interprocess_lock_dispose(lock);
    cplat_interprocess_rwlock_dispose(rwlock);
}

// sleep が EINTR の残時間を使って再試行することの確認
TEST(syncAdditionalFailureTest, sleep_retries_interrupted_nanosleep)
{
    // Arrange
    NiceMock<Mock_time> mock_time;

    // Pre-Assert
    EXPECT_CALL(mock_time, nanosleep(_, _, _, _, _))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, const struct timespec *, struct timespec *rem)
            {
                rem->tv_sec = 0;
                rem->tv_nsec = 1;
                errno = EINTR;
                return -1;
            }))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - nanosleep() が 2 回呼び出されること。
    // [Pre-Assert手順] - nanosleep() から、1 回目は errno に EINTR を設定して -1、2 回目は 0 を返す。

    // Act
    cplat_sleep_ms(1); // [手順] - EINTR が発生する状態で、待機時間に 1 ms を指定して cplat_sleep_ms() を呼び出す。

    // Assert
    SUCCEED(); // [確認_正常系] - SUCCEED() に到達すること。(cplat_sleep_ms() が EINTR 後に戻ったことの確認)
}

// local lock の有限待機が EBUSY 後に取得へ成功することの確認
TEST(syncAdditionalFailureTest, local_lock_finite_wait_retries_before_deadline)
{
    // Arrange
    cplat_local_lock *lock = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_local_lock_create(&lock)); // [状態] - local lock を生成する。
    // [状態確認] - cplat_local_lock_create() の戻り値が CPLAT_OK であること。
    NiceMock<Mock_pthread> mock_pthread;
    NiceMock<Mock_time> mock_time;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_mutex_trylock(_, _, _, _))
        .WillOnce(Return(EBUSY))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - pthread_mutex_trylock() が 2 回呼び出されること。
                              // [Pre-Assert手順] - pthread_mutex_trylock() から 1 回目は EBUSY、2 回目は 0 を返す。
    EXPECT_CALL(mock_time, clock_gettime(_, _, _, CLOCK_MONOTONIC, _))
        .Times(2)
        .WillRepeatedly(Invoke(
            [](const char *, const int, const char *, const clockid_t, struct timespec *ts)
            {
                ts->tv_sec = 0;
                ts->tv_nsec = 0;
                return 0;
            })); // [Pre-Assert確認_正常系] - 有限待機の期限判定で clock_gettime() が CLOCK_MONOTONIC を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - clock_gettime() から、tv_sec に 0 を設定して 0 を返す。

    // Act
    int result = cplat_local_lock_lock(
        lock,
        1); // [手順] - 期限前の EBUSY 後に local lock の取得を再試行する状態で cplat_local_lock_lock() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, result); // [確認_正常系] - 再試行後の cplat_local_lock_lock() の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_local_lock_unlock(lock);
    cplat_local_lock_dispose(lock);
}

// sleep が EINTR 以外の失敗では再試行しないことの確認
TEST(syncAdditionalFailureTest, sleep_stops_after_non_interrupt_error)
{
    // Arrange
    NiceMock<Mock_time> mock_time;

    // Pre-Assert
    EXPECT_CALL(mock_time, nanosleep(_, _, _, _, _))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, const struct timespec *, struct timespec *)
            {
                errno = EINVAL;
                return -1;
            })); // [Pre-Assert確認_異常系] - nanosleep() が 1 回呼び出されること。
                 // [Pre-Assert手順] - nanosleep() から、errno に EINVAL を設定して -1 を返す。

    // Act
    cplat_sleep_ms(1); // [手順] - EINVAL が発生する状態で、待機時間に 1 ms を指定して cplat_sleep_ms() を呼び出す。

    // Assert
    SUCCEED(); // [確認_正常系] - SUCCEED() に到達すること。(cplat_sleep_ms() が EINVAL 後に再試行せず戻ったことの確認)
}

#endif /* PLATFORM_LINUX */
