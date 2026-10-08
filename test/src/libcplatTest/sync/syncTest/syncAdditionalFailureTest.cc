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
    cplat_local_lock *lock = NULL;
    ASSERT_EQ(CPLAT_OK, cplat_local_lock_create(&lock)); // [状態] - local lock を生成する。
                                                               // [状態確認] - cplat_local_lock_create の戻り値が CPLAT_OK であること。
    NiceMock<Mock_pthread> mock_pthread;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_mutex_trylock(_, _, _, _))
        .WillOnce(Return(EINVAL)); // [Pre-Assert確認_異常系] - pthread_mutex_trylock が 1 回呼び出されること。
                                   // [Pre-Assert手順] - pthread_mutex_trylock にて EINVAL を返却する。

    // Act
    int result = cplat_local_lock_try_lock(lock); // [手順] - EINVAL を返す trylock を実行する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - cplat_local_lock_try_lock の戻り値が CPLAT_ERR_UNKNOWN であること。

    // Cleanup
    cplat_local_lock_dispose(lock);
}

// condvar の待機・通知エラーを共通結果へ変換することの確認
TEST(syncAdditionalFailureTest, condvar_maps_wait_signal_and_broadcast_errors)
{
    // Arrange
    cplat_condvar *cv = NULL;
    cplat_local_lock *lock = NULL;
    ASSERT_EQ(CPLAT_OK, cplat_condvar_create(&cv)); // [状態] - condvar を生成する。
                                                          // [状態確認] - cplat_condvar_create の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_local_lock_create(&lock)); // [状態] - local lock を生成する。
                                                               // [状態確認] - cplat_local_lock_create の戻り値が CPLAT_OK であること。
    NiceMock<Mock_pthread> mock_pthread;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_cond_wait(_, _, _, _, _))
        .WillOnce(Return(EINVAL)); // [Pre-Assert確認_異常系] - pthread_cond_wait が 1 回呼び出されること。
                                   // [Pre-Assert手順] - pthread_cond_wait にて EINVAL を返却する。
    EXPECT_CALL(mock_pthread, pthread_cond_timedwait(_, _, _, _, _, _))
        .WillOnce(Return(ETIMEDOUT)); // [Pre-Assert確認_正常系] - pthread_cond_timedwait が 1 回呼び出されること。
                                      // [Pre-Assert手順] - pthread_cond_timedwait にて ETIMEDOUT を返却する。
    EXPECT_CALL(mock_pthread, pthread_cond_signal(_, _, _, _))
        .WillOnce(Return(EINVAL)); // [Pre-Assert確認_異常系] - pthread_cond_signal が 1 回呼び出されること。
                                   // [Pre-Assert手順] - pthread_cond_signal にて EINVAL を返却する。
    EXPECT_CALL(mock_pthread, pthread_cond_broadcast(_, _, _, _))
        .WillOnce(Return(EINVAL)); // [Pre-Assert確認_異常系] - pthread_cond_broadcast が 1 回呼び出されること。
                                   // [Pre-Assert手順] - pthread_cond_broadcast にて EINVAL を返却する。

    // Act
    int wait_result =
        cplat_condvar_wait(cv, lock, CPLAT_SYNC_WAIT_FOREVER); // [手順] - cond_wait の EINVAL 失敗を注入する。
    int timed_wait_result =
        cplat_condvar_wait(cv, lock, CPLAT_SYNC_NO_WAIT); // [手順] - cond_timedwait の timeout を注入する。
    int signal_result = cplat_condvar_signal(cv);            // [手順] - cond_signal の EINVAL 失敗を注入する。
    int broadcast_result = cplat_condvar_broadcast(cv);      // [手順] - cond_broadcast の EINVAL 失敗を注入する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, wait_result); // [確認_異常系] - WAIT_FOREVER の condvar 待機が UNKNOWN になること。
    EXPECT_EQ(CPLAT_ERR_TIMEOUT,
              timed_wait_result);                      // [確認_正常系] - NO_WAIT の condvar 待機が TIMEOUT になること。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, signal_result);    // [確認_異常系] - condvar signal が UNKNOWN になること。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, broadcast_result); // [確認_異常系] - condvar broadcast が UNKNOWN になること。

    // Cleanup
    cplat_condvar_dispose(cv);
    cplat_local_lock_dispose(lock);
}

// condvar 属性の時計設定失敗を生成失敗として処理することの確認
TEST(syncAdditionalFailureTest, condvar_create_reports_clock_attribute_failure)
{
    // Arrange
    NiceMock<Mock_pthread> mock_pthread;
    cplat_condvar *cv = NULL;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_condattr_setclock(_, _, _, _, CLOCK_MONOTONIC))
        .WillOnce(Return(EINVAL)); // [Pre-Assert確認_異常系] - pthread_condattr_setclock が 1 回呼び出されること。
                                   // [Pre-Assert手順] - pthread_condattr_setclock にて EINVAL を返却する。

    // Act
    int result = cplat_condvar_create(&cv); // [手順] - 時計属性設定失敗を注入して condvar を生成する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - cplat_condvar_create の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ((cplat_condvar *)NULL, cv); // [確認_異常系] - 生成失敗時に condvar が NULL であること。
}

// condvar 初期化失敗を生成失敗として処理することの確認
TEST(syncAdditionalFailureTest, condvar_create_reports_native_initialization_failure)
{
    // Arrange
    NiceMock<Mock_pthread> mock_pthread;
    cplat_condvar *cv = NULL;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_cond_init(_, _, _, _, _))
        .WillOnce(Return(EINVAL)); // [Pre-Assert確認_異常系] - pthread_cond_init が 1 回呼び出されること。
                                   // [Pre-Assert手順] - pthread_cond_init にて EINVAL を返却する。

    // Act
    int result = cplat_condvar_create(&cv); // [手順] - native condvar 初期化失敗を注入して生成する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result);                       // [確認_異常系] - 初期化失敗時の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ((cplat_condvar *)NULL, cv); // [確認_異常系] - 初期化失敗時に condvar が NULL であること。
}

// 条件変数ハンドルの確保失敗が生成失敗として報告されることの確認
TEST(syncAdditionalFailureTest, condvar_create_reports_allocation_failure)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_condvar *cv = NULL;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_calloc(_, _))
        .WillOnce(Return(
            static_cast<void *>(NULL))); // [Pre-Assert確認_異常系] - condvar ハンドルの cplat_calloc が 1 回呼び出されること。
                                         // [Pre-Assert手順] - cplat_calloc にて NULL を返却する。

    // Act
    int result = cplat_condvar_create(&cv); // [手順] - ハンドル確保失敗を注入して condvar を生成する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - ハンドル確保失敗時の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ((cplat_condvar *)NULL, cv); // [確認_異常系] - ハンドル確保失敗時に NULL が返ること。
}

// ローカル RW ロックのハンドル確保失敗が生成失敗として報告されることの確認
TEST(syncAdditionalFailureTest, local_rwlock_create_reports_allocation_failure)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_local_rwlock *rwlock = NULL;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_calloc(_, _))
        .WillOnce(Return(static_cast<void *>(
            NULL))); // [Pre-Assert確認_異常系] - local rwlock ハンドルの cplat_calloc が 1 回呼び出されること。
                     // [Pre-Assert手順] - cplat_calloc にて NULL を返却する。

    // Act
    int result = cplat_local_rwlock_create(&rwlock); // [手順] - ハンドル確保失敗を注入して local rwlock を生成する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - ハンドル確保失敗時の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ((cplat_local_rwlock *)NULL, rwlock); // [確認_異常系] - ハンドル確保失敗時に NULL が返ること。
}

// ローカル RW ロックの mutex 初期化失敗が生成失敗として報告されることの確認
TEST(syncAdditionalFailureTest, local_rwlock_create_reports_mutex_initialization_failure)
{
    // Arrange
    NiceMock<Mock_pthread> mock_pthread;
    cplat_local_rwlock *rwlock = NULL;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_mutex_init(_, _, _, _, _))
        .WillOnce(Return(ENOMEM)); // [Pre-Assert確認_異常系] - pthread_mutex_init が 1 回呼び出されること。
                                   // [Pre-Assert手順] - pthread_mutex_init にて ENOMEM を返却する。
    EXPECT_CALL(mock_pthread, pthread_mutex_destroy(_, _, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 初期化失敗後に pthread_mutex_destroy が呼び出されること。
                              // [Pre-Assert手順] - pthread_mutex_destroy にて 0 を返却する。
    EXPECT_CALL(mock_pthread, pthread_cond_destroy(_, _, _, _))
        .WillRepeatedly(Return(0)); // [Pre-Assert確認_正常系] - 初期化失敗後に pthread_cond_destroy が呼び出されること。
                                    // [Pre-Assert手順] - pthread_cond_destroy にて 0 を返却する。

    // Act
    int result = cplat_local_rwlock_create(&rwlock); // [手順] - mutex 初期化失敗を注入して local rwlock を生成する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - mutex 初期化失敗時の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ((cplat_local_rwlock *)NULL, rwlock); // [確認_異常系] - 生成失敗時に NULL が返ること。
}

// rwlock の未取得状態での解放要求を拒否することの確認
TEST(syncAdditionalFailureTest, rwlock_rejects_unlock_without_ownership)
{
    // Arrange
    cplat_local_rwlock *rwlock = NULL;
    ASSERT_EQ(CPLAT_OK, cplat_local_rwlock_create(&rwlock)); // [状態] - local rwlock を生成する。
                                                                   // [状態確認] - cplat_local_rwlock_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int shared_result =
        cplat_local_rwlock_unlock_shared(rwlock); // [手順] - 共有ロック未取得で unlock_shared を呼び出す。
    int exclusive_result =
        cplat_local_rwlock_unlock_exclusive(rwlock); // [手順] - 排他ロック未取得で unlock_exclusive を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              shared_result); // [確認_異常系] - 未取得の共有ロック解放が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              exclusive_result); // [確認_異常系] - 未取得の排他ロック解放が INVALID_ARGUMENT になること。

    // Cleanup
    cplat_local_rwlock_dispose(rwlock);
}

// local rwlock の try_lock_exclusive が競合を BUSY へ変換することの確認
TEST(syncAdditionalFailureTest, local_rwlock_try_lock_exclusive_reports_busy)
{
    // Arrange
    cplat_local_rwlock *rwlock = NULL;
    ASSERT_EQ(CPLAT_OK, cplat_local_rwlock_create(&rwlock)); // [状態] - local rwlock を生成する。
                                                                   // [状態確認] - cplat_local_rwlock_create の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_local_rwlock_lock_shared(rwlock, CPLAT_SYNC_NO_WAIT)); // [状態] - 共有ロックを取得しておく。
                                                                                 // [状態確認] - cplat_local_rwlock_lock_shared の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int result =
        cplat_local_rwlock_try_lock_exclusive(rwlock); // [手順] - 共有ロック保持中に排他 try_lock を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUSY,
              result); // [確認_正常系] - 排他 try_lock の戻り値が CPLAT_ERR_BUSY であること。

    // Cleanup
    (void)cplat_local_rwlock_unlock_shared(rwlock);
    cplat_local_rwlock_dispose(rwlock);
}

// local rwlock の共有待機中エラーが UNKNOWN へ変換されることの確認
TEST(syncAdditionalFailureTest, local_rwlock_shared_wait_reports_pthread_failure)
{
    // Arrange
    cplat_local_rwlock *rwlock = NULL;
    ASSERT_EQ(CPLAT_OK, cplat_local_rwlock_create(&rwlock)); // [状態] - local rwlock を生成する。
                                                                   // [状態確認] - cplat_local_rwlock_create の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_local_rwlock_lock_exclusive(
                               rwlock, CPLAT_SYNC_NO_WAIT)); // [状態] - 排他ロックを取得しておく。
                                                                // [状態確認] - cplat_local_rwlock_lock_exclusive の戻り値が CPLAT_OK であること。
    NiceMock<Mock_pthread> mock_pthread;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_mutex_lock(_, _, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 共有待機前に pthread_mutex_lock が呼び出されること。
                              // [Pre-Assert手順] - pthread_mutex_lock にて 0 を返却する。
    EXPECT_CALL(mock_pthread, pthread_cond_wait(_, _, _, _, _))
        .WillOnce(Return(EINVAL)); // [Pre-Assert確認_異常系] - 共有待機の pthread_cond_wait が 1 回呼び出されること。
                                   // [Pre-Assert手順] - pthread_cond_wait にて EINVAL を返却する。
    EXPECT_CALL(mock_pthread, pthread_mutex_unlock(_, _, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 共有待機後に pthread_mutex_unlock が呼び出されること。
                              // [Pre-Assert手順] - pthread_mutex_unlock にて 0 を返却する。
    EXPECT_CALL(mock_pthread, pthread_mutex_destroy(_, _, _, _))
        .WillOnce(Return(0)); // pthread_mutex_destroy は Cleanup の destroy 用
    // [Pre-Assert確認_異常系] - mock_pthread の pthread_mutex_destroy(_, _, _, _) が登録した呼び出し期待を満たすこと。
    EXPECT_CALL(mock_pthread, pthread_cond_destroy(_, _, _, _))
        .WillRepeatedly(Return(0)); // pthread_cond_destroy は Cleanup の destroy 用
    // [Pre-Assert確認_異常系] - mock_pthread の pthread_cond_destroy(_, _, _, _) が登録した呼び出し期待を満たすこと。

    // Act
    int result = cplat_local_rwlock_lock_shared(
        rwlock, CPLAT_SYNC_WAIT_FOREVER); // [手順] - 共有待機の pthread エラーを注入する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - 共有待機の pthread エラーが UNKNOWN へ変換されること。

    // Cleanup
    cplat_local_rwlock_dispose(rwlock);
}

// local rwlock の共有待機タイムアウトが TIMEOUT へ変換されることの確認
TEST(syncAdditionalFailureTest, local_rwlock_shared_wait_reports_timeout)
{
    // Arrange
    cplat_local_rwlock *rwlock = NULL;
    ASSERT_EQ(CPLAT_OK, cplat_local_rwlock_create(&rwlock)); // [状態] - local rwlock を生成する。
                                                                   // [状態確認] - cplat_local_rwlock_create の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_local_rwlock_lock_exclusive(
                               rwlock, CPLAT_SYNC_NO_WAIT)); // [状態] - 排他ロックを取得しておく。
                                                                // [状態確認] - cplat_local_rwlock_lock_exclusive の戻り値が CPLAT_OK であること。
    NiceMock<Mock_pthread> mock_pthread;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_mutex_lock(_, _, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 共有待機前に pthread_mutex_lock が呼び出されること。
                              // [Pre-Assert手順] - pthread_mutex_lock にて 0 を返却する。
    EXPECT_CALL(mock_pthread, pthread_cond_timedwait(_, _, _, _, _, _))
        .WillOnce(Return(ETIMEDOUT)); // [Pre-Assert確認_正常系] - 共有待機の pthread_cond_timedwait が 1 回呼び出されること。
                                      // [Pre-Assert手順] - pthread_cond_timedwait にて ETIMEDOUT を返却する。
    EXPECT_CALL(mock_pthread, pthread_mutex_unlock(_, _, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 共有待機後に pthread_mutex_unlock が呼び出されること。
                              // [Pre-Assert手順] - pthread_mutex_unlock にて 0 を返却する。
    EXPECT_CALL(mock_pthread, pthread_mutex_destroy(_, _, _, _))
        .WillOnce(Return(0)); // pthread_mutex_destroy は Cleanup の destroy 用
    // [Pre-Assert確認_異常系] - mock_pthread の pthread_mutex_destroy(_, _, _, _) が登録した呼び出し期待を満たすこと。
    EXPECT_CALL(mock_pthread, pthread_cond_destroy(_, _, _, _))
        .WillRepeatedly(Return(0)); // pthread_cond_destroy は Cleanup の destroy 用
    // [Pre-Assert確認_異常系] - mock_pthread の pthread_cond_destroy(_, _, _, _) が登録した呼び出し期待を満たすこと。

    // Act
    int result = cplat_local_rwlock_lock_shared(rwlock, 1); // [手順] - 共有待機のタイムアウトを注入する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_TIMEOUT,
              result); // [確認_正常系] - 共有待機のタイムアウトが TIMEOUT へ変換されること。

    // Cleanup
    cplat_local_rwlock_dispose(rwlock);
}

// local rwlock の排他待機中エラーが UNKNOWN へ変換されることの確認
TEST(syncAdditionalFailureTest, local_rwlock_exclusive_wait_reports_pthread_failure)
{
    // Arrange
    cplat_local_rwlock *rwlock = NULL;
    ASSERT_EQ(CPLAT_OK, cplat_local_rwlock_create(&rwlock)); // [状態] - local rwlock を生成する。
                                                                   // [状態確認] - cplat_local_rwlock_create の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_local_rwlock_lock_shared(rwlock, CPLAT_SYNC_NO_WAIT)); // [状態] - 共有ロックを取得しておく。
                                                                                 // [状態確認] - cplat_local_rwlock_lock_shared の戻り値が CPLAT_OK であること。
    NiceMock<Mock_pthread> mock_pthread;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_mutex_lock(_, _, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 排他待機前に pthread_mutex_lock が呼び出されること。
                              // [Pre-Assert手順] - pthread_mutex_lock にて 0 を返却する。
    EXPECT_CALL(mock_pthread, pthread_cond_wait(_, _, _, _, _))
        .WillOnce(Return(EINVAL)); // [Pre-Assert確認_異常系] - 排他待機の pthread_cond_wait が 1 回呼び出されること。
                                   // [Pre-Assert手順] - pthread_cond_wait にて EINVAL を返却する。
    EXPECT_CALL(mock_pthread, pthread_mutex_unlock(_, _, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 排他待機後に pthread_mutex_unlock が呼び出されること。
                              // [Pre-Assert手順] - pthread_mutex_unlock にて 0 を返却する。
    EXPECT_CALL(mock_pthread, pthread_mutex_destroy(_, _, _, _))
        .WillOnce(Return(0)); // pthread_mutex_destroy は Cleanup の destroy 用
    // [Pre-Assert確認_異常系] - mock_pthread の pthread_mutex_destroy(_, _, _, _) が登録した呼び出し期待を満たすこと。
    EXPECT_CALL(mock_pthread, pthread_cond_destroy(_, _, _, _))
        .WillRepeatedly(Return(0)); // pthread_cond_destroy は Cleanup の destroy 用
    // [Pre-Assert確認_異常系] - mock_pthread の pthread_cond_destroy(_, _, _, _) が登録した呼び出し期待を満たすこと。

    // Act
    int result = cplat_local_rwlock_lock_exclusive(
        rwlock, CPLAT_SYNC_WAIT_FOREVER); // [手順] - 排他待機の pthread エラーを注入する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - 排他待機の pthread エラーが UNKNOWN へ変換されること。

    // Cleanup
    cplat_local_rwlock_dispose(rwlock);
}

// local rwlock の排他待機タイムアウトが TIMEOUT へ変換されることの確認
TEST(syncAdditionalFailureTest, local_rwlock_exclusive_wait_reports_timeout)
{
    // Arrange
    cplat_local_rwlock *rwlock = NULL;
    ASSERT_EQ(CPLAT_OK, cplat_local_rwlock_create(&rwlock)); // [状態] - local rwlock を生成する。
                                                                   // [状態確認] - cplat_local_rwlock_create の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_local_rwlock_lock_shared(rwlock, CPLAT_SYNC_NO_WAIT)); // [状態] - 共有ロックを取得しておく。
                                                                                 // [状態確認] - cplat_local_rwlock_lock_shared の戻り値が CPLAT_OK であること。
    NiceMock<Mock_pthread> mock_pthread;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_mutex_lock(_, _, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 排他待機前に pthread_mutex_lock が呼び出されること。
                              // [Pre-Assert手順] - pthread_mutex_lock にて 0 を返却する。
    EXPECT_CALL(mock_pthread, pthread_cond_timedwait(_, _, _, _, _, _))
        .WillOnce(Return(ETIMEDOUT)); // [Pre-Assert確認_正常系] - 排他待機の pthread_cond_timedwait が 1 回呼び出されること。
                                      // [Pre-Assert手順] - pthread_cond_timedwait にて ETIMEDOUT を返却する。
    EXPECT_CALL(mock_pthread, pthread_mutex_unlock(_, _, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 排他待機後に pthread_mutex_unlock が呼び出されること。
                              // [Pre-Assert手順] - pthread_mutex_unlock にて 0 を返却する。
    EXPECT_CALL(mock_pthread, pthread_mutex_destroy(_, _, _, _))
        .WillOnce(Return(0)); // pthread_mutex_destroy は Cleanup の destroy 用
    // [Pre-Assert確認_異常系] - mock_pthread の pthread_mutex_destroy(_, _, _, _) が登録した呼び出し期待を満たすこと。
    EXPECT_CALL(mock_pthread, pthread_cond_destroy(_, _, _, _))
        .WillRepeatedly(Return(0)); // pthread_cond_destroy は Cleanup の destroy 用
    // [Pre-Assert確認_異常系] - mock_pthread の pthread_cond_destroy(_, _, _, _) が登録した呼び出し期待を満たすこと。

    // Act
    int result = cplat_local_rwlock_lock_exclusive(rwlock, 1); // [手順] - 排他待機のタイムアウトを注入する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_TIMEOUT,
              result); // [確認_正常系] - 排他待機のタイムアウトが TIMEOUT へ変換されること。

    // Cleanup
    cplat_local_rwlock_dispose(rwlock);
}

// local rwlock の共有解放が待機中 writer を通知することの確認
TEST(syncAdditionalFailureTest, local_rwlock_shared_unlock_signals_waiting_writer)
{
    // Arrange
    cplat_local_rwlock *rwlock = NULL;
    ASSERT_EQ(CPLAT_OK, cplat_local_rwlock_create(&rwlock)); // [状態] - local rwlock を生成する。
                                                                   // [状態確認] - cplat_local_rwlock_create の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_local_rwlock_lock_shared(rwlock, CPLAT_SYNC_NO_WAIT)); // [状態] - 共有ロックを取得しておく。
                                                                                 // [状態確認] - cplat_local_rwlock_lock_shared の戻り値が CPLAT_OK であること。
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
    int unlock_result =
        cplat_local_rwlock_unlock_shared(rwlock); // [手順] - 待機中 writer がいる状態で共有ロックを解放する。
    writer.join();                                   // [手順] - 通知された writer の終了を待つ。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              unlock_result); // [確認_正常系] - 共有ロック解放の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              writer_result.load()); // [確認_正常系] - 通知された writer が排他ロックを取得できること。
}

// local rwlock の排他解放が待機中 writer を通知することの確認
TEST(syncAdditionalFailureTest, local_rwlock_exclusive_unlock_signals_waiting_writer)
{
    // Arrange
    cplat_local_rwlock *rwlock = NULL;
    ASSERT_EQ(CPLAT_OK, cplat_local_rwlock_create(&rwlock)); // [状態] - local rwlock を生成する。
                                                                   // [状態確認] - cplat_local_rwlock_create の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_local_rwlock_lock_exclusive(
                               rwlock, CPLAT_SYNC_NO_WAIT)); // [状態] - 排他ロックを取得しておく。
                                                                // [状態確認] - cplat_local_rwlock_lock_exclusive の戻り値が CPLAT_OK であること。
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
    int unlock_result =
        cplat_local_rwlock_unlock_exclusive(rwlock); // [手順] - 待機中 writer がいる状態で排他ロックを解放する。
    writer.join();                                      // [手順] - 通知された writer の終了を待つ。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              unlock_result); // [確認_正常系] - 排他ロック解放の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              writer_result.load()); // [確認_正常系] - 通知された writer が排他ロックを取得できること。
}

// thread join の pthread エラーを未知エラーへ変換することの確認
TEST(syncAdditionalFailureTest, thread_join_reports_pthread_failure)
{
    // Arrange
    cplat_thread *thread = NULL;
    ASSERT_EQ(CPLAT_OK, cplat_thread_create(&thread, [](void *) {}, NULL)); // [状態] - thread を生成する。
                                                                                  // [状態確認] - cplat_thread_create の戻り値が CPLAT_OK であること。
    NiceMock<Mock_pthread> mock_pthread;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_join(_, _, _, _, _))
        .WillOnce(Return(EINVAL)); // [Pre-Assert確認_異常系] - pthread_join が 1 回呼び出されること。
                                   // [Pre-Assert手順] - pthread_join にて EINVAL を返却する。
    EXPECT_CALL(mock_pthread, pthread_detach(_, _, _, _))
        .WillOnce(DoDefault()); // [Pre-Assert確認_正常系] - join 失敗後の detach で pthread_detach が呼び出されること。
                                // [Pre-Assert手順] - pthread_detach は既定動作へ委譲する。

    // Act
    int result =
        cplat_thread_join(thread, CPLAT_SYNC_WAIT_FOREVER); // [手順] - pthread_join の EINVAL 失敗を注入する。
    cplat_thread_detach(thread); // [手順] - join 失敗後の thread を detach して解放する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - cplat_thread_join の戻り値が CPLAT_ERR_UNKNOWN であること。
}

// thread の即時 join timeout を TIMEOUT へ変換することの確認
TEST(syncAdditionalFailureTest, thread_join_no_wait_reports_timeout)
{
    // Arrange
    cplat_thread *thread = NULL;
    ASSERT_EQ(CPLAT_OK, cplat_thread_create(&thread, [](void *) {}, NULL)); // [状態] - thread を生成する。
                                                                                  // [状態確認] - cplat_thread_create の戻り値が CPLAT_OK であること。
    NiceMock<Mock_pthread> mock_pthread;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_tryjoin_np(_, _, _, _, _))
        .WillOnce(Return(EBUSY)); // [Pre-Assert確認_正常系] - pthread_tryjoin_np が 1 回呼び出されること。
                                  // [Pre-Assert手順] - pthread_tryjoin_np にて EBUSY を返却する。
    EXPECT_CALL(mock_pthread, pthread_detach(_, _, _, _))
        .WillOnce(DoDefault()); // [Pre-Assert確認_正常系] - timeout 後の detach で pthread_detach が呼び出されること。
                                // [Pre-Assert手順] - pthread_detach は既定動作へ委譲する。

    // Act
    int result =
        cplat_thread_join(thread, CPLAT_SYNC_NO_WAIT); // [手順] - pthread_tryjoin_np の EBUSY を注入する。
    cplat_thread_detach(thread);                          // [手順] - timeout 後の thread を detach して解放する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_TIMEOUT,
              result); // [確認_正常系] - cplat_thread_join の戻り値が CPLAT_ERR_TIMEOUT であること。
}

// thread の有限 join が再試行後に成功することの確認
TEST(syncAdditionalFailureTest, thread_join_finite_wait_retries_then_succeeds)
{
    // Arrange
    cplat_thread *thread = NULL;
    ASSERT_EQ(CPLAT_OK, cplat_thread_create(&thread, [](void *) {}, NULL)); // [状態] - thread を生成する。
                                                                                  // [状態確認] - cplat_thread_create の戻り値が CPLAT_OK であること。
    NiceMock<Mock_pthread> mock_pthread;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_tryjoin_np(_, _, _, _, _))
        .WillOnce(Return(EBUSY))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - pthread_tryjoin_np が 2 回呼び出されること。
                              // [Pre-Assert手順] - 1 回目は EBUSY、2 回目は 0 を返却する。

    // Act
    int result = cplat_thread_join(thread, 5); // [手順] - 有限時間で thread join を再試行する。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - 再試行後の cplat_thread_join が CPLAT_OK になること。
}

// thread の有限 join が未知の pthread エラーを返すことの確認
TEST(syncAdditionalFailureTest, thread_join_finite_wait_reports_pthread_failure)
{
    // Arrange
    cplat_thread *thread = NULL;
    ASSERT_EQ(CPLAT_OK, cplat_thread_create(&thread, [](void *) {}, NULL)); // [状態] - thread を生成する。
                                                                                  // [状態確認] - cplat_thread_create の戻り値が CPLAT_OK であること。
    NiceMock<Mock_pthread> mock_pthread;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_tryjoin_np(_, _, _, _, _))
        .WillOnce(Return(EINVAL)); // [Pre-Assert確認_異常系] - 有限 join の pthread_tryjoin_np が 1 回呼び出されること。
                                   // [Pre-Assert手順] - pthread_tryjoin_np にて EINVAL を返却する。
    EXPECT_CALL(mock_pthread, pthread_detach(_, _, _, _))
        .WillOnce(DoDefault()); // [Pre-Assert確認_正常系] - join 失敗後の detach で pthread_detach が呼び出されること。
                                // [Pre-Assert手順] - pthread_detach は既定動作へ委譲する。

    // Act
    int result = cplat_thread_join(thread, 5); // [手順] - 未知の pthread エラーを返す有限 join を実行する。
    cplat_thread_detach(thread);               // [手順] - join 失敗後の thread を detach して解放する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - 未知の pthread エラーが UNKNOWN へ変換されること。
}

// interprocess lock の EWOULDBLOCK と EINTR を処理することの確認
TEST(syncAdditionalFailureTest, interprocess_lock_maps_busy_and_retries_eintr)
{
    // Arrange
    InterprocessOpenMocks os;
    NiceMock<Mock_sys_file> mock_sys_file;
    ON_CALL(mock_sys_file, flock(_, _, _, _, _))
        .WillByDefault(Return(0)); // [状態] - flock が呼び出された際に 0 を返すようにモックを設定する。
    const char *path = kLockIdentity;
    cplat_interprocess_lock *lock = NULL;
    ASSERT_EQ(CPLAT_OK, cplat_interprocess_lock_open(path, &lock)); // [状態] - interprocess lock を開いた状態とする。
                                                                          // [状態確認] - cplat_interprocess_lock_open の戻り値が CPLAT_OK であること。

    // Pre-Assert
    EXPECT_CALL(mock_sys_file, flock(_, _, _, _, LOCK_EX | LOCK_NB))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, const int, const int)
            {
                errno = EWOULDBLOCK;
                return -1;
            })); // [Pre-Assert確認_正常系] - try_lock が非ブロッキング flock を 1 回呼び出すこと。
                 // [Pre-Assert手順] - flock は EWOULDBLOCK を設定して -1 を返却する。

    // Act
    int busy_result = cplat_interprocess_lock_try_lock(
        lock); // [手順] - 非ブロッキング flock が EWOULDBLOCK になる状態で取得する。

    // Pre-Assert_2
    EXPECT_CALL(mock_sys_file, flock(_, _, _, _, LOCK_EX))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, const int, const int)
            {
                errno = EINTR;
                return -1;
            }))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - ブロッキング flock が EINTR のあと成功すること。
                              // [Pre-Assert手順] - 1 回目は EINTR で -1、2 回目は 0 を返却する。

    // Act_2
    int retry_result = cplat_interprocess_lock_lock(
        lock, CPLAT_SYNC_WAIT_FOREVER); // [手順] - EINTR 後に成功する flock を実行する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUSY,
              busy_result); // [確認_正常系] - try_lock が BUSY を返すこと。
    EXPECT_EQ(CPLAT_OK,
              retry_result); // [確認_正常系] - EINTR 後の interprocess lock 取得が CPLAT_OK であること。

    // Cleanup
    EXPECT_CALL(mock_sys_file, flock(_, _, _, _, LOCK_UN)).WillOnce(Return(0)); // unlock 用の flock を成功させる。
    // [Pre-Assert確認_異常系] - mock_sys_file の flock(_, _, _, _, LOCK_UN) が登録した呼び出し期待を満たすこと。
    (void)cplat_interprocess_lock_unlock(lock);
    cplat_interprocess_lock_dispose(lock);
}

// thread ハンドル確保の malloc 失敗を未知エラーへ変換することの確認
TEST(syncAdditionalFailureTest, thread_create_reports_context_allocation_failure)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_thread *thread = NULL;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_calloc(_, _))
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - thread ハンドルの cplat_calloc が 1 回呼び出されること。
                                    // [Pre-Assert手順] - cplat_calloc にて NULL を返却する。

    // Act
    int result =
        cplat_thread_create(&thread, [](void *) {}, NULL); // [手順] - thread ハンドルの calloc 失敗を注入する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - cplat_thread_create の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ((cplat_thread *)NULL, thread); // [確認_異常系] - 生成失敗時に thread が NULL であること。
}

// interprocess rwlock の有限待機が成功、未知エラー、タイムアウトを分類することの確認
TEST(syncAdditionalFailureTest, interprocess_rwlock_finite_wait_classifies_results)
{
    // Arrange
    InterprocessOpenMocks os;
    const char *path = kRwlockIdentity;
    cplat_interprocess_rwlock *lock = NULL;
    ASSERT_EQ(CPLAT_OK, cplat_interprocess_rwlock_open(path, &lock)); // [状態] - interprocess rwlock を開いた状態とする。
                                                                            // [状態確認] - cplat_interprocess_rwlock_open の戻り値が CPLAT_OK であること。
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
            })); // [Pre-Assert確認_正常系] - 共有 flock が成功、未知エラー、EWOULDBLOCK の順で 3 回呼び出されること。
                 // [Pre-Assert手順] - 1 回目は 0、2 回目は EIO で -1、3 回目は EWOULDBLOCK で -1 を返却する。
    EXPECT_CALL(mock_sys_file, flock(_, _, _, _, LOCK_UN))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 取得した共有ロックの unlock で flock が呼び出されること。
                              // [Pre-Assert手順] - LOCK_UN の flock にて 0 を返却する。
    EXPECT_CALL(mock_time, clock_gettime(_, _, _, CLOCK_MONOTONIC, _))
        .Times(4)
        .WillRepeatedly(Invoke(
            [&clock_count](const char *, const int, const char *, const clockid_t, struct timespec *ts)
            {
                ts->tv_sec = (clock_count++ < 3) ? 0 : 1;
                ts->tv_nsec = 0;
                return 0;
            })); // [Pre-Assert確認_正常系] - 有限待機の期限判定で clock_gettime が 4 回呼び出されること。
                 // [Pre-Assert手順] - 3 回目まで tv_sec=0、4 回目は tv_sec=1 を返却する。

    // Act
    int null_result =
        cplat_interprocess_rwlock_lock_shared(NULL, CPLAT_SYNC_NO_WAIT); // [手順] - NULL ハンドルを有限待機する。
    int success_result = cplat_interprocess_rwlock_lock_shared(lock, 1); // [手順] - 有限待機で共有ロックを取得する。
    int unlock_result = cplat_interprocess_rwlock_unlock(lock);          // [手順] - 取得した共有ロックを解放する。
    int unknown_result = cplat_interprocess_rwlock_lock_shared(lock, 1); // [手順] - flock の未知エラーを処理する。
    int timeout_result = cplat_interprocess_rwlock_lock_shared(lock, 1); // [手順] - 有限待機の期限到達を処理する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              null_result);                          // [確認_異常系] - NULL ハンドルが INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_OK, success_result);          // [確認_正常系] - 有限待機の共有ロック取得が OK になること。
    EXPECT_EQ(CPLAT_OK, unlock_result);           // [確認_正常系] - 共有ロックの解放が OK になること。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, unknown_result); // [確認_異常系] - flock の未知エラーが UNKNOWN になること。
    EXPECT_EQ(CPLAT_ERR_TIMEOUT, timeout_result); // [確認_正常系] - 有限待機の期限到達が TIMEOUT になること。

    // Cleanup
    cplat_interprocess_rwlock_dispose(lock);
}

// interprocess lock の有限待機とブロッキング待機のエラーを分類することの確認
TEST(syncAdditionalFailureTest, interprocess_lock_finite_and_forever_wait_classify_errors)
{
    // Arrange
    InterprocessOpenMocks os;
    const char *path = kLockIdentity;
    cplat_interprocess_lock *lock = NULL;
    ASSERT_EQ(CPLAT_OK, cplat_interprocess_lock_open(path, &lock)); // [状態] - interprocess lock を開いた状態とする。
                                                                          // [状態確認] - cplat_interprocess_lock_open の戻り値が CPLAT_OK であること。
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
            })); // [Pre-Assert確認_正常系] - 非ブロッキング flock が成功、未知エラー、EWOULDBLOCK の順で 3 回呼び出されること。
                 // [Pre-Assert手順] - 1 回目は 0、2 回目は EIO で -1、3 回目は EWOULDBLOCK で -1 を返却する。
    EXPECT_CALL(mock_sys_file, flock(_, _, _, _, LOCK_EX))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, const int, const int)
            {
                errno = EIO;
                return -1;
            })); // [Pre-Assert確認_異常系] - ブロッキング flock が 1 回呼び出されること。
                 // [Pre-Assert手順] - flock は EIO を設定して -1 を返却する。
    EXPECT_CALL(mock_sys_file, flock(_, _, _, _, LOCK_UN))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 取得したロックの unlock で flock が呼び出されること。
                              // [Pre-Assert手順] - LOCK_UN の flock にて 0 を返却する。
    EXPECT_CALL(mock_time, clock_gettime(_, _, _, CLOCK_MONOTONIC, _))
        .Times(4)
        .WillRepeatedly(Invoke(
            [&clock_count](const char *, const int, const char *, const clockid_t, struct timespec *ts)
            {
                ts->tv_sec = (clock_count++ < 3) ? 0 : 1;
                ts->tv_nsec = 0;
                return 0;
            })); // [Pre-Assert確認_正常系] - 有限待機の期限判定で clock_gettime が 4 回呼び出されること。
                 // [Pre-Assert手順] - 3 回目まで tv_sec=0、4 回目は tv_sec=1 を返却する。

    // Act
    int success_result = cplat_interprocess_lock_lock(lock, 1); // [手順] - 有限待機でロックを取得する。
    int unlock_result = cplat_interprocess_lock_unlock(lock);   // [手順] - 取得したロックを解放する。
    int unknown_result = cplat_interprocess_lock_lock(lock, 1); // [手順] - 有限 flock の未知エラーを処理する。
    int timeout_result = cplat_interprocess_lock_lock(lock, 1); // [手順] - 有限 flock の期限到達を処理する。
    int forever_error = cplat_interprocess_lock_lock(
        lock, CPLAT_SYNC_WAIT_FOREVER); // [手順] - ブロッキング flock の未知エラーを処理する。

    // Assert
    EXPECT_EQ(CPLAT_OK, success_result);          // [確認_正常系] - 有限待機のロック取得が OK になること。
    EXPECT_EQ(CPLAT_OK, unlock_result);           // [確認_正常系] - ロック解放が OK になること。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, unknown_result); // [確認_異常系] - 有限 flock の未知エラーが UNKNOWN になること。
    EXPECT_EQ(CPLAT_ERR_TIMEOUT, timeout_result); // [確認_正常系] - 有限 flock の期限到達が TIMEOUT になること。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              forever_error); // [確認_異常系] - ブロッキング flock の未知エラーが UNKNOWN になること。

    // Cleanup
    cplat_interprocess_lock_dispose(lock);
}

// local lock の有限待機が成功、未知エラー、タイムアウトを分類することの確認
TEST(syncAdditionalFailureTest, local_lock_finite_wait_classifies_results)
{
    // Arrange
    cplat_local_lock *lock = NULL;
    ASSERT_EQ(CPLAT_OK, cplat_local_lock_create(&lock)); // [状態] - local lock を生成する。
                                                               // [状態確認] - cplat_local_lock_create の戻り値が CPLAT_OK であること。
    NiceMock<Mock_pthread> mock_pthread;
    NiceMock<Mock_time> mock_time;
    int clock_count = 0;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_mutex_trylock(_, _, _, _))
        .WillOnce(Return(0))
        .WillOnce(Return(EINVAL))
        .WillOnce(Return(EBUSY)); // [Pre-Assert確認_正常系] - pthread_mutex_trylock が成功、EINVAL、EBUSY の順で 3 回呼び出されること。
                                  // [Pre-Assert手順] - 1 回目は 0、2 回目は EINVAL、3 回目は EBUSY を返却する。
    EXPECT_CALL(mock_time, clock_gettime(_, _, _, CLOCK_MONOTONIC, _))
        .Times(4)
        .WillRepeatedly(Invoke(
            [&clock_count](const char *, const int, const char *, const clockid_t, struct timespec *ts)
            {
                ts->tv_sec = (clock_count++ < 3) ? 0 : 1;
                ts->tv_nsec = 0;
                return 0;
            })); // [Pre-Assert確認_正常系] - 有限待機の期限判定で clock_gettime が 4 回呼び出されること。
                 // [Pre-Assert手順] - 3 回目まで tv_sec=0、4 回目は tv_sec=1 を返却する。

    // Act
    int success_result = cplat_local_lock_lock(lock, 1); // [手順] - 有限待機で mutex を取得する。
    int unlock_result = cplat_local_lock_unlock(lock);   // [手順] - 取得した mutex を解放する。
    int unknown_result = cplat_local_lock_lock(lock, 1); // [手順] - trylock の未知エラーを処理する。
    int timeout_result = cplat_local_lock_lock(lock, 1); // [手順] - trylock の期限到達を処理する。

    // Assert
    EXPECT_EQ(CPLAT_OK, success_result);          // [確認_正常系] - 有限待機の mutex 取得が OK になること。
    EXPECT_EQ(CPLAT_OK, unlock_result);           // [確認_正常系] - mutex 解放が OK になること。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, unknown_result); // [確認_異常系] - trylock の未知エラーが UNKNOWN になること。
    EXPECT_EQ(CPLAT_ERR_TIMEOUT, timeout_result); // [確認_正常系] - trylock の期限到達が TIMEOUT になること。

    // Cleanup
    cplat_local_lock_dispose(lock);
}

// local lock、local rwlock、interprocess rwlock の NULL 引数が拒否されることの確認
TEST(syncAdditionalFailureTest, lock_apis_reject_null_arguments)
{
    // Arrange
    const char *path = kLockIdentity;
    cplat_thread *thread = NULL;
    cplat_interprocess_lock *interprocess_lock = NULL;
    cplat_interprocess_rwlock *interprocess_rwlock = NULL;

    // Pre-Assert

    // Act
    int local_create_result = cplat_local_lock_create(NULL);    // [手順] - local lock の出力先へ NULL を指定する。
    int local_unlock_result = cplat_local_lock_unlock(NULL);    // [手順] - NULL の local lock を解放する。
    int rwlock_create_result = cplat_local_rwlock_create(NULL); // [手順] - local rwlock の出力先へ NULL を指定する。
    int rwlock_shared_result = cplat_local_rwlock_lock_shared(
        NULL, CPLAT_SYNC_NO_WAIT); // [手順] - NULL の local rwlock を共有取得する。
    int rwlock_exclusive_result = cplat_local_rwlock_lock_exclusive(
        NULL, CPLAT_SYNC_NO_WAIT); // [手順] - NULL の local rwlock を排他取得する。
    int rwlock_shared_unlock_result =
        cplat_local_rwlock_unlock_shared(NULL); // [手順] - NULL の local rwlock を共有解放する。
    int rwlock_exclusive_unlock_result =
        cplat_local_rwlock_unlock_exclusive(NULL); // [手順] - NULL の local rwlock を排他解放する。
    int thread_create_output_result =
        cplat_thread_create(NULL, [](void *) {}, NULL); // [手順] - thread の出力先へ NULL を指定する。
    int thread_create_function_result =
        cplat_thread_create(&thread, NULL, NULL); // [手順] - thread の開始関数へ NULL を指定する。
    int thread_join_result =
        cplat_thread_join(NULL, CPLAT_SYNC_WAIT_FOREVER); // [手順] - NULL の thread を join する。
    int interprocess_open_result =
        cplat_interprocess_rwlock_open(NULL, &interprocess_rwlock); // [手順] - 識別子へ NULL を指定する。
    int interprocess_open_empty_result =
        cplat_interprocess_rwlock_open("", &interprocess_rwlock); // [手順] - 空の識別子を指定する。
    int interprocess_open_output_result =
        cplat_interprocess_rwlock_open(path, NULL); // [手順] - interprocess rwlock の出力先へ NULL を指定する。
    int lock_open_result =
        cplat_interprocess_lock_open(NULL, &interprocess_lock); // [手順] - lock の識別子へ NULL を指定する。
    int lock_open_empty_result =
        cplat_interprocess_lock_open("", &interprocess_lock); // [手順] - lock の識別子へ空文字列を指定する。
    int lock_open_output_result =
        cplat_interprocess_lock_open(path, NULL); // [手順] - lock の出力先へ NULL を指定する。
    int lock_result =
        cplat_interprocess_lock_lock(NULL, CPLAT_SYNC_NO_WAIT); // [手順] - NULL の lock を取得する。
    int lock_negative_timeout_result =
        cplat_interprocess_lock_lock(NULL, -1); // [手順] - 負の待機時間で lock を取得する。
    int lock_export_result =
        cplat_interprocess_lock_export_descriptor(NULL, NULL, NULL); // [手順] - NULL の lock を export する。
    int lock_import_result = cplat_interprocess_lock_import_descriptor(
        NULL, 0U, NULL); // [手順] - NULL の出力先へ lock descriptor を import する。
    int lock_unlock_result = cplat_interprocess_lock_unlock(NULL); // [手順] - NULL の lock を解放する。
    int interprocess_shared_result = cplat_interprocess_rwlock_lock_shared(
        NULL, CPLAT_SYNC_NO_WAIT); // [手順] - NULL の interprocess rwlock を共有取得する。
    int interprocess_shared_negative_timeout_result =
        cplat_interprocess_rwlock_lock_shared(NULL, -1); // [手順] - 負の待機時間で共有ロックを取得する。
    int interprocess_exclusive_negative_timeout_result =
        cplat_interprocess_rwlock_lock_exclusive(NULL, -1); // [手順] - 負の待機時間で排他ロックを取得する。
    int interprocess_export_result =
        cplat_interprocess_rwlock_export_descriptor(NULL, NULL, NULL); // [手順] - NULL のハンドルを export する。
    int interprocess_import_result = cplat_interprocess_rwlock_import_descriptor(
        NULL, 0U, NULL); // [手順] - NULL の出力先へ descriptor を import する。
    int interprocess_unlock_result =
        cplat_interprocess_rwlock_unlock(NULL); // [手順] - NULL の interprocess rwlock を解放する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              local_create_result); // [確認_異常系] - local lock の NULL 出力先が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              local_unlock_result); // [確認_異常系] - NULL の local lock 解放が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              rwlock_create_result); // [確認_異常系] - local rwlock の NULL 出力先が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              rwlock_shared_result); // [確認_異常系] - NULL の local rwlock 共有取得が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              rwlock_exclusive_result); // [確認_異常系] - NULL の local rwlock 排他取得が INVALID_ARGUMENT になること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        rwlock_shared_unlock_result); // [確認_異常系] - NULL の local rwlock 共有解放が INVALID_ARGUMENT になること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        rwlock_exclusive_unlock_result); // [確認_異常系] - NULL の local rwlock 排他解放が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              thread_create_output_result); // [確認_異常系] - NULL の thread 出力先が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              thread_create_function_result); // [確認_異常系] - NULL の thread 開始関数が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              thread_join_result); // [確認_異常系] - NULL の thread join が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              interprocess_open_result); // [確認_異常系] - NULL 識別子が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              interprocess_open_empty_result); // [確認_異常系] - 空識別子が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              interprocess_open_output_result); // [確認_異常系] - NULL 出力先が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              lock_open_result); // [確認_異常系] - lock の NULL 識別子が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              lock_open_empty_result); // [確認_異常系] - lock の空識別子が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              lock_open_output_result); // [確認_異常系] - lock の NULL 出力先が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              lock_result); // [確認_異常系] - NULL の lock 取得が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              lock_negative_timeout_result); // [確認_異常系] - 負の待機時間の lock 取得が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              lock_export_result); // [確認_異常系] - NULL の lock export が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              lock_import_result); // [確認_異常系] - NULL 出力先の lock import が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              lock_unlock_result); // [確認_異常系] - NULL の lock 解放が INVALID_ARGUMENT になること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        interprocess_shared_result); // [確認_異常系] - NULL の interprocess rwlock 共有取得が INVALID_ARGUMENT になること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        interprocess_shared_negative_timeout_result); // [確認_異常系] - 負の待機時間の共有取得が INVALID_ARGUMENT になること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        interprocess_exclusive_negative_timeout_result); // [確認_異常系] - 負の待機時間の排他取得が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              interprocess_export_result); // [確認_異常系] - NULL ハンドルの export が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              interprocess_import_result); // [確認_異常系] - NULL 出力先の import が INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              interprocess_unlock_result); // [確認_異常系] - NULL ハンドルの unlock が INVALID_ARGUMENT になること。
}

// interprocess rwlock の open が OS エラーを返すことの確認
TEST(syncAdditionalFailureTest, interprocess_rwlock_open_reports_open_failure)
{
    // Arrange
    NiceMock<Mock_fcntl> mock_fcntl;
    const char *path = kRwlockIdentity;
    cplat_interprocess_rwlock *lock = NULL;

    // Pre-Assert
    EXPECT_CALL(mock_fcntl, open(_, _, _, _, _, _))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - rwlock の lock file open が 1 回呼び出されること。
                               // [Pre-Assert手順] - open にて -1 を返却する。

    // Act
    int result = cplat_interprocess_rwlock_open(path, &lock); // [手順] - open 失敗を注入して rwlock を開く。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - cplat_interprocess_rwlock_open の戻り値が UNKNOWN であること。
    EXPECT_EQ((cplat_interprocess_rwlock *)NULL,
              lock); // [確認_異常系] - open 失敗時にハンドルが NULL であること。
}

// interprocess rwlock の識別子複製失敗が通知されることの確認
TEST(syncAdditionalFailureTest, interprocess_rwlock_open_reports_identity_duplication_failure)
{
    // Arrange
    InterprocessOpenMocks os;
    NiceMock<Mock_cplat> mock_cplat;
    const char *path = kRwlockIdentity;
    cplat_interprocess_rwlock *lock = NULL;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_strdup(_))
        .WillOnce(Return(static_cast<char *>(NULL))); // [Pre-Assert確認_異常系] - rwlock の識別子複製で cplat_strdup が 1 回呼び出されること。
                                                      // [Pre-Assert手順] - cplat_strdup にて NULL を返却する。

    // Act
    int result = cplat_interprocess_rwlock_open(path, &lock); // [手順] - 識別子複製失敗を注入して rwlock を開く。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - 識別子複製失敗の戻り値が UNKNOWN であること。
    EXPECT_EQ((cplat_interprocess_rwlock *)NULL,
              lock); // [確認_異常系] - 識別子複製失敗時にハンドルが NULL であること。
}

// interprocess rwlock のハンドル確保失敗が通知されることの確認
TEST(syncAdditionalFailureTest, interprocess_rwlock_open_reports_allocation_failure)
{
    // Arrange
    InterprocessOpenMocks os;
    NiceMock<Mock_cplat> mock_cplat;
    const char *path = kRwlockIdentity;
    cplat_interprocess_rwlock *lock = NULL;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_calloc(_, _))
        .WillOnce(Return(
            static_cast<void *>(NULL))); // [Pre-Assert確認_異常系] - rwlock ハンドルの cplat_calloc が 1 回呼び出されること。
                                         // [Pre-Assert手順] - cplat_calloc にて NULL を返却する。

    // Act
    int result = cplat_interprocess_rwlock_open(path, &lock); // [手順] - ハンドル確保失敗を注入して rwlock を開く。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - ハンドル確保失敗の戻り値が UNKNOWN であること。
    EXPECT_EQ((cplat_interprocess_rwlock *)NULL,
              lock); // [確認_異常系] - ハンドル確保失敗時に NULL が返ること。
}

// interprocess rwlock の WAIT_FOREVER が EINTR 後に再試行することの確認
TEST(syncAdditionalFailureTest, interprocess_rwlock_wait_forever_retries_eintr)
{
    // Arrange
    InterprocessOpenMocks os;
    NiceMock<Mock_sys_file> mock_sys_file;
    ON_CALL(mock_sys_file, flock(_, _, _, _, _))
        .WillByDefault(Return(0)); // [状態] - flock が呼び出された際に 0 を返すようにモックを設定する。
    const char *path = kRwlockIdentity;
    cplat_interprocess_rwlock *lock = NULL;
    ASSERT_EQ(CPLAT_OK, cplat_interprocess_rwlock_open(path, &lock)); // [状態] - interprocess rwlock を開いた状態とする。
                                                                            // [状態確認] - cplat_interprocess_rwlock_open の戻り値が CPLAT_OK であること。

    // Pre-Assert
    EXPECT_CALL(mock_sys_file, flock(_, _, _, _, LOCK_EX))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, const int, const int)
            {
                errno = EINTR;
                return -1;
            }))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 排他 flock が EINTR のあと成功すること。
                              // [Pre-Assert手順] - 1 回目は EINTR で -1、2 回目は 0 を返却する。

    // Act
    int result = cplat_interprocess_rwlock_lock_exclusive(
        lock, CPLAT_SYNC_WAIT_FOREVER); // [手順] - EINTR 後に成功する排他ロック取得を実行する。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - EINTR 後の排他ロック取得が CPLAT_OK になること。

    // Cleanup
    EXPECT_CALL(mock_sys_file, flock(_, _, _, _, LOCK_UN)).WillOnce(Return(0)); // unlock 用の flock を成功させる。
    // [Pre-Assert確認_異常系] - mock_sys_file の flock(_, _, _, _, LOCK_UN) が登録した呼び出し期待を満たすこと。
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
        .WillByDefault(Return(0)); // [状態] - flock が呼び出された際に 0 を返すようにモックを設定する。
    const char *path = kRwlockIdentity;
    cplat_interprocess_rwlock *lock = NULL;
    ASSERT_EQ(CPLAT_OK, cplat_interprocess_rwlock_open(path, &lock)); // [状態] - interprocess rwlock を開いた状態とする。
                                                                            // [状態確認] - cplat_interprocess_rwlock_open の戻り値が CPLAT_OK であること。

    // Pre-Assert
    EXPECT_CALL(mock_sys_file, flock(_, _, _, _, LOCK_SH))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, const int, const int)
            {
                errno = EIO;
                return -1;
            })); // [Pre-Assert確認_異常系] - 共有 blocking flock が 1 回呼び出されること。
                 // [Pre-Assert手順] - flock は EIO を設定して -1 を返却する。

    // Act
    int result = cplat_interprocess_rwlock_lock_shared(
        lock, CPLAT_SYNC_WAIT_FOREVER); // [手順] - blocking flock の未知エラーを注入する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - blocking flock の未知エラーが UNKNOWN へ変換されること。

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
        .WillByDefault(Return(0)); // [状態] - flock が呼び出された際に 0 を返すようにモックを設定する。
    const char *path = kRwlockIdentity;
    cplat_interprocess_rwlock *lock = NULL;
    ASSERT_EQ(CPLAT_OK, cplat_interprocess_rwlock_open(path, &lock)); // [状態] - interprocess rwlock を開いた状態とする。
                                                                            // [状態確認] - cplat_interprocess_rwlock_open の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_interprocess_rwlock_lock_exclusive(
                               lock, CPLAT_SYNC_NO_WAIT)); // [状態] - 排他ロックを取得しておく。
                                                              // [状態確認] - cplat_interprocess_rwlock_lock_exclusive の戻り値が CPLAT_OK であること。

    // Pre-Assert
    EXPECT_CALL(mock_sys_file, flock(_, _, _, _, LOCK_UN))
        .WillRepeatedly(Return(-1)); // [Pre-Assert確認_異常系] - unlock の flock が呼び出されること。
                                     // [Pre-Assert手順] - LOCK_UN の flock にて -1 を返却する。

    // Act
    int result = cplat_interprocess_rwlock_unlock(lock); // [手順] - unlock の flock 失敗を注入する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - unlock の flock 失敗が UNKNOWN へ変換されること。

    // Cleanup
    cplat_interprocess_rwlock_dispose(lock);
}

// interprocess rwlock の try_lock_exclusive がロックを取得できることの確認
TEST(syncAdditionalFailureTest, interprocess_rwlock_try_lock_exclusive_succeeds)
{
    // Arrange
    InterprocessOsMocks os;
    const char *path = kRwlockIdentity;
    cplat_interprocess_rwlock *lock = NULL;
    ASSERT_EQ(CPLAT_OK, cplat_interprocess_rwlock_open(path, &lock)); // [状態] - interprocess rwlock を開いた状態とする。
                                                                            // [状態確認] - cplat_interprocess_rwlock_open の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int result = cplat_interprocess_rwlock_try_lock_exclusive(
        lock); // [手順] - interprocess rwlock の排他 try_lock を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - 排他 try_lock の戻り値が CPLAT_OK であること。

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
    s_once_callback_release.store(true); // [手順] - 初回 callback を終了させる。
    initializer.join();                  // [手順] - 初期化側 thread の終了を待つ。
    waiter.join();                       // [手順] - 待機側 thread の終了を待つ。

    // Assert
    EXPECT_EQ(2, cplat_atomic_load_i32(&flag.state,
                                       CPLAT_MEMORY_ORDER_RELAXED)); // [確認_正常系] - callback 完了後に初期化済み状態になること。
}

// interprocess lock が重複取得、有限 EINTR、再試行成功を分類することの確認
TEST(syncAdditionalFailureTest, interprocess_locks_cover_locked_and_finite_retry_paths)
{
    // Arrange
    InterprocessOpenMocks os;
    const char *lock_path = kLockIdentity;
    const char *rwlock_path = kRwlockIdentity;
    cplat_interprocess_lock *lock = NULL;
    cplat_interprocess_rwlock *rwlock = NULL;
    ASSERT_EQ(CPLAT_OK, cplat_interprocess_lock_open(lock_path, &lock)); // [状態] - interprocess lock を開いた状態とする。
                                                                               // [状態確認] - cplat_interprocess_lock_open の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_interprocess_rwlock_open(rwlock_path, &rwlock)); // [状態] - interprocess rwlock を開いた状態とする。
                                                                                     // [状態確認] - cplat_interprocess_rwlock_open の戻り値が CPLAT_OK であること。
    NiceMock<Mock_sys_file> mock_sys_file;
    ON_CALL(mock_sys_file, flock(_, _, _, _, _))
        .WillByDefault(Return(0)); // [状態] - flock が呼び出された際に 0 を返すようにモックを設定する。
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
            })); // [Pre-Assert確認_正常系] - 有限待機の期限判定で clock_gettime が 4 回呼び出されること。
                 // [Pre-Assert手順] - clock_gettime にて tv_sec=0 を返却する。
    EXPECT_CALL(mock_sys_file, flock(_, _, _, _, LOCK_EX | LOCK_NB))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, const int, const int)
            {
                errno = EINTR;
                return -1;
            }))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 排他非ブロッキング flock が EINTR のあと成功すること。
                              // [Pre-Assert手順] - 1 回目は EINTR で -1、2 回目は 0 を返却する。
    EXPECT_CALL(mock_sys_file, flock(_, _, _, _, LOCK_SH | LOCK_NB))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, const int, const int)
            {
                errno = EINTR;
                return -1;
            }))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 共有非ブロッキング flock が EINTR のあと成功すること。
                              // [Pre-Assert手順] - 1 回目は EINTR で -1、2 回目は 0 を返却する。

    // Act
    int lock_result = cplat_interprocess_lock_lock(lock, 1);    // [手順] - EINTR 後に有限待機の lock を取得する。
    int lock_duplicate = cplat_interprocess_lock_lock(lock, 1); // [手順] - 取得済み lock を再取得する。
    int rwlock_result =
        cplat_interprocess_rwlock_lock_shared(rwlock, 1); // [手順] - EINTR 後に有限待機の rwlock を取得する。
    int rwlock_duplicate =
        cplat_interprocess_rwlock_lock_shared(rwlock, 1); // [手順] - 取得済み rwlock を再取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, lock_result); // [確認_正常系] - cplat_interprocess_lock_lock が EINTR 後に成功すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              lock_duplicate); // [確認_異常系] - cplat_interprocess_lock_lock が重複取得を拒否すること。
    EXPECT_EQ(CPLAT_OK,
              rwlock_result); // [確認_正常系] - cplat_interprocess_rwlock_lock_shared が EINTR 後に成功すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              rwlock_duplicate); // [確認_異常系] - cplat_interprocess_rwlock_lock_shared が重複取得を拒否すること。

    // Cleanup
    EXPECT_CALL(mock_sys_file, flock(_, _, _, _, LOCK_UN))
        .Times(2)
        .WillRepeatedly(Return(0)); // unlock 用の flock を成功させる。
    // [Pre-Assert確認_異常系] - mock_sys_file の flock(_, _, _, _, LOCK_UN) が登録した呼び出し期待を満たすこと。
    (void)cplat_interprocess_lock_unlock(lock);
    (void)cplat_interprocess_rwlock_unlock(rwlock);
    cplat_interprocess_lock_dispose(lock);
    cplat_interprocess_rwlock_dispose(rwlock);
}

// local synchronization API が非 NULL ハンドルの負値と不足引数を拒否することの確認
TEST(syncAdditionalFailureTest, local_sync_rejects_negative_and_partial_arguments)
{
    // Arrange
    cplat_local_lock *lock = NULL;
    cplat_local_rwlock *rwlock = NULL;
    cplat_condvar *cv = NULL;
    ASSERT_EQ(CPLAT_OK, cplat_local_lock_create(&lock)); // [状態] - local lock を生成する。
                                                               // [状態確認] - cplat_local_lock_create の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_local_rwlock_create(&rwlock)); // [状態] - local rwlock を生成する。
                                                                   // [状態確認] - cplat_local_rwlock_create の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_condvar_create(&cv)); // [状態] - condvar を生成する。
                                                          // [状態確認] - cplat_condvar_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int null_lock_result =
        cplat_local_lock_lock(NULL, CPLAT_SYNC_NO_WAIT); // [手順] - NULL local lock を取得する。
    int lock_result = cplat_local_lock_lock(lock, -2);      // [手順] - 非 NULL lock に負の待機時間を指定する。
    int shared_result =
        cplat_local_rwlock_lock_shared(rwlock, -2); // [手順] - 非 NULL rwlock の共有取得へ負の待機時間を指定する。
    int exclusive_result = cplat_local_rwlock_lock_exclusive(
        rwlock, -2); // [手順] - 非 NULL rwlock の排他取得へ負の待機時間を指定する。
    int null_lock_wait = cplat_condvar_wait(cv, NULL, 0); // [手順] - condvar 待機の lock を NULL とする。
    int negative_wait = cplat_condvar_wait(cv, lock, -2); // [手順] - condvar 待機へ負の待機時間を指定する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              null_lock_result); // [確認_異常系] - cplat_local_lock_lock が NULL lock を拒否すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              lock_result); // [確認_異常系] - cplat_local_lock_lock が負の待機時間を拒否すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              shared_result); // [確認_異常系] - cplat_local_rwlock_lock_shared が負の待機時間を拒否すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              exclusive_result); // [確認_異常系] - cplat_local_rwlock_lock_exclusive が負の待機時間を拒否すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              null_lock_wait); // [確認_異常系] - cplat_condvar_wait が NULL lock を拒否すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              negative_wait); // [確認_異常系] - cplat_condvar_wait が負の待機時間を拒否すること。

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
    cplat_local_rwlock *readers_failure = NULL;
    cplat_local_rwlock *writers_failure = NULL;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_cond_init(_, _, _, _, _))
        .WillOnce(Return(EINVAL))
        .WillOnce(Return(0))
        .WillOnce(Return(EINVAL)); // [Pre-Assert確認_異常系] - pthread_cond_init が 3 回呼び出されること。
                                   // [Pre-Assert手順] - 1 回目は EINVAL、2 回目は 0、3 回目は EINVAL を返却する。

    // Act
    int readers_result =
        cplat_local_rwlock_create(&readers_failure); // [手順] - readers condvar 初期化失敗を発生させる。
    int writers_result =
        cplat_local_rwlock_create(&writers_failure); // [手順] - writers condvar 初期化失敗を発生させる。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              readers_result); // [確認_異常系] - cplat_local_rwlock_create が readers 初期化失敗を通知すること。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              writers_result); // [確認_異常系] - cplat_local_rwlock_create が writers 初期化失敗を通知すること。
    EXPECT_EQ((cplat_local_rwlock *)NULL,
              readers_failure); // [確認_異常系] - readers 初期化失敗時に NULL のままであること。
    EXPECT_EQ((cplat_local_rwlock *)NULL,
              writers_failure); // [確認_異常系] - writers 初期化失敗時に NULL のままであること。
}

// waiting writer を待つ reader が通知後に取得を完了することの確認
TEST(syncAdditionalFailureTest, local_rwlock_reader_resumes_after_writer_state_clears)
{
    // Arrange
    cplat_local_rwlock *rwlock = NULL;
    ASSERT_EQ(CPLAT_OK, cplat_local_rwlock_create(&rwlock)); // [状態] - local rwlock を生成する。
                                                                   // [状態確認] - cplat_local_rwlock_create の戻り値が CPLAT_OK であること。
    test_sync_set_local_rwlock_state(rwlock, 0, 0U, 1U);
    NiceMock<Mock_pthread> mock_pthread;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_cond_wait(_, _, _, _, _))
        .WillOnce(Invoke(
            [rwlock](const char *, const int, const char *, pthread_cond_t *, pthread_mutex_t *)
            {
                test_sync_set_local_rwlock_state(rwlock, 0, 0U, 0U);
                return 0;
            })); // [Pre-Assert確認_正常系] - waiting writer 待ちの pthread_cond_wait が 1 回呼び出されること。
                 // [Pre-Assert手順] - 通知時に waiting writer を消し、0 を返却する。

    // Act
    int lock_result = cplat_local_rwlock_lock_shared(
        rwlock, CPLAT_SYNC_WAIT_FOREVER); // [手順] - waiting writer の完了後に共有 lock を取得する。
    int unlock_result = cplat_local_rwlock_unlock_shared(rwlock); // [手順] - 取得した共有 lock を解放する。

    // Assert
    EXPECT_EQ(CPLAT_OK, lock_result);   // [確認_正常系] - cplat_local_rwlock_lock_shared が通知後に成功すること。
    EXPECT_EQ(CPLAT_OK, unlock_result); // [確認_正常系] - cplat_local_rwlock_unlock_shared が成功すること。

    // Cleanup
    cplat_local_rwlock_dispose(rwlock);
}

// thread API が context 確保失敗と NULL detach を処理することの確認
TEST(syncAdditionalFailureTest, thread_apis_cover_context_failure_and_null_detach)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_thread *allocation_failure = NULL;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_malloc(_))
        .WillOnce(Return(
            static_cast<void *>(NULL))); // [Pre-Assert確認_異常系] - thread context の cplat_malloc が 1 回呼び出されること。
                                         // [Pre-Assert手順] - cplat_malloc にて NULL を返却する。

    // Act
    int allocation_result =
        cplat_thread_create(&allocation_failure, [](void *) {}, NULL); // [手順] - context 確保失敗を注入する。
    cplat_thread_detach(NULL);                                         // [手順] - NULL thread を detach する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              allocation_result); // [確認_異常系] - cplat_thread_create が context 確保失敗を通知すること。
    SUCCEED();                    // [確認_正常系] - cplat_thread_detach が NULL thread を安全に処理すること。
}

// thread join が非 NULL thread に対する負の待機時間を拒否することの確認
TEST(syncAdditionalFailureTest, thread_join_rejects_negative_timeout)
{
    // Arrange
    cplat_thread *thread = NULL;
    ASSERT_EQ(CPLAT_OK, cplat_thread_create(&thread, [](void *) {}, NULL)); // [状態] - thread を生成する。
                                                                                  // [状態確認] - cplat_thread_create の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int negative_result =
        cplat_thread_join(thread, -2); // [手順] - 非 NULL thread の join へ負の待機時間を指定する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              negative_result); // [確認_異常系] - cplat_thread_join が負の待機時間を拒否すること。

    // Cleanup
    cplat_thread_detach(thread);
}

// thread の有限 join が期限前に再試行することの確認
TEST(syncAdditionalFailureTest, thread_join_finite_retry_uses_monotonic_deadline)
{
    // Arrange
    cplat_thread *thread = NULL;
    ASSERT_EQ(CPLAT_OK, cplat_thread_create(&thread, [](void *) {}, NULL)); // [状態] - thread を生成する。
                                                                                  // [状態確認] - cplat_thread_create の戻り値が CPLAT_OK であること。
    NiceMock<Mock_pthread> mock_pthread;
    NiceMock<Mock_time> mock_time;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_tryjoin_np(_, _, _, _, _))
        .WillOnce(Return(EBUSY))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - pthread_tryjoin_np が 2 回呼び出されること。
                              // [Pre-Assert手順] - 1 回目は EBUSY、2 回目は 0 を返却する。
    EXPECT_CALL(mock_time, clock_gettime(_, _, _, CLOCK_MONOTONIC, _))
        .Times(2)
        .WillRepeatedly(Invoke(
            [](const char *, const int, const char *, const clockid_t, struct timespec *ts)
            {
                ts->tv_sec = 0;
                ts->tv_nsec = 0;
                return 0;
            })); // [Pre-Assert確認_正常系] - 有限 join の期限判定で clock_gettime が 2 回呼び出されること。
                 // [Pre-Assert手順] - clock_gettime にて tv_sec=0 を返却する。

    // Act
    int result = cplat_thread_join(thread, 1); // [手順] - deadline 前の EBUSY 後に thread join を再試行する。

    // Assert
    EXPECT_EQ(CPLAT_OK, result); // [確認_正常系] - cplat_thread_join が再試行後に成功すること。
}

// thread の有限 join が deadline 到達時に timeout を返すことの確認
TEST(syncAdditionalFailureTest, thread_join_finite_wait_reports_deadline_timeout)
{
    // Arrange
    cplat_thread *thread = NULL;
    ASSERT_EQ(CPLAT_OK, cplat_thread_create(&thread, [](void *) {}, NULL)); // [状態] - thread を生成する。
                                                                                  // [状態確認] - cplat_thread_create の戻り値が CPLAT_OK であること。
    NiceMock<Mock_pthread> mock_pthread;
    NiceMock<Mock_time> mock_time;
    int clock_count = 0;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_tryjoin_np(_, _, _, _, _))
        .WillOnce(Return(EBUSY)); // [Pre-Assert確認_正常系] - pthread_tryjoin_np が 1 回呼び出されること。
                                  // [Pre-Assert手順] - pthread_tryjoin_np にて EBUSY を返却する。
    EXPECT_CALL(mock_time, clock_gettime(_, _, _, CLOCK_MONOTONIC, _))
        .Times(2)
        .WillRepeatedly(Invoke(
            [&clock_count](const char *, const int, const char *, const clockid_t, struct timespec *ts)
            {
                ts->tv_sec = clock_count++;
                ts->tv_nsec = 0;
                return 0;
            })); // [Pre-Assert確認_正常系] - 有限 join の期限判定で clock_gettime が 2 回呼び出されること。
                 // [Pre-Assert手順] - 1 回目は tv_sec=0、2 回目は tv_sec=1 を返却する。

    // Act
    int result = cplat_thread_join(thread, 1); // [手順] - EBUSY のまま deadline に到達する有限 join を実行する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_TIMEOUT, result); // [確認_正常系] - cplat_thread_join が deadline timeout を返すこと。

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
    cplat_interprocess_lock *lock = NULL;
    cplat_interprocess_rwlock *rwlock = NULL;
    ASSERT_EQ(CPLAT_OK, cplat_interprocess_lock_open(lock_path, &lock)); // [状態] - interprocess lock を開いた状態とする。
                                                                               // [状態確認] - cplat_interprocess_lock_open の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_interprocess_rwlock_open(rwlock_path, &rwlock)); // [状態] - interprocess rwlock を開いた状態とする。
                                                                                     // [状態確認] - cplat_interprocess_rwlock_open の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int lock_result = cplat_interprocess_lock_unlock(lock); // [手順] - 未取得の interprocess lock を解放する。
    int rwlock_result =
        cplat_interprocess_rwlock_unlock(rwlock); // [手順] - 未取得の interprocess rwlock を解放する。
    cplat_local_rwlock_dispose(NULL);             // [手順] - NULL local rwlock を破棄する。
    cplat_interprocess_lock_dispose(NULL);        // [手順] - NULL interprocess lock を破棄する。
    cplat_interprocess_rwlock_dispose(NULL);      // [手順] - NULL interprocess rwlock を破棄する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              lock_result); // [確認_異常系] - cplat_interprocess_lock_unlock が未取得を拒否すること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              rwlock_result); // [確認_異常系] - cplat_interprocess_rwlock_unlock が未取得を拒否すること。

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
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - nanosleep が 2 回呼び出されること。
                              // [Pre-Assert手順] - 1 回目は EINTR で -1、2 回目は 0 を返却する。

    // Act
    cplat_sleep_ms(1); // [手順] - EINTR を発生させて 1 ms sleep を実行する。

    // Assert
    SUCCEED(); // [確認_正常系] - cplat_sleep_ms が EINTR 後に戻ること。
}

// local lock の有限待機が EBUSY 後に取得へ成功することの確認
TEST(syncAdditionalFailureTest, local_lock_finite_wait_retries_before_deadline)
{
    // Arrange
    cplat_local_lock *lock = NULL;
    ASSERT_EQ(CPLAT_OK, cplat_local_lock_create(&lock)); // [状態] - local lock を生成する。
                                                               // [状態確認] - cplat_local_lock_create の戻り値が CPLAT_OK であること。
    NiceMock<Mock_pthread> mock_pthread;
    NiceMock<Mock_time> mock_time;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_mutex_trylock(_, _, _, _))
        .WillOnce(Return(EBUSY))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - pthread_mutex_trylock が 2 回呼び出されること。
                              // [Pre-Assert手順] - 1 回目は EBUSY、2 回目は 0 を返却する。
    EXPECT_CALL(mock_time, clock_gettime(_, _, _, CLOCK_MONOTONIC, _))
        .Times(2)
        .WillRepeatedly(Invoke(
            [](const char *, const int, const char *, const clockid_t, struct timespec *ts)
            {
                ts->tv_sec = 0;
                ts->tv_nsec = 0;
                return 0;
            })); // [Pre-Assert確認_正常系] - 有限待機の期限判定で clock_gettime が 2 回呼び出されること。
                 // [Pre-Assert手順] - clock_gettime にて tv_sec=0 を返却する。

    // Act
    int result = cplat_local_lock_lock(lock, 1); // [手順] - deadline 前の EBUSY 後に local lock 取得を再試行する。

    // Assert
    EXPECT_EQ(CPLAT_OK, result); // [確認_正常系] - cplat_local_lock_lock が再試行後に成功すること。

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
            })); // [Pre-Assert確認_異常系] - nanosleep が 1 回呼び出されること。
                 // [Pre-Assert手順] - nanosleep は EINVAL を設定して -1 を返却する。

    // Act
    cplat_sleep_ms(1); // [手順] - EINVAL を発生させて 1 ms sleep を実行する。

    // Assert
    SUCCEED(); // [確認_正常系] - cplat_sleep_ms が EINVAL 後に再試行せず戻ること。
}

#endif /* PLATFORM_LINUX */
