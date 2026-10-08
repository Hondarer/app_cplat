#include <testfw.h>
#include "syncTestHelper.h"
#if defined(PLATFORM_LINUX)
    #include <errno.h>
    #include <mock_pthread.h>
#endif

// ロック取得済みの local lock への try_lock が BUSY を報告することの確認
TEST(syncLocalLockTest, try_lock_reports_busy_when_already_locked)
{
    // Arrange
#if defined(PLATFORM_LINUX)
    testing::NiceMock<Mock_pthread> mock_pthread;
#endif                             /* PLATFORM_LINUX */
    cplat_local_lock *lock = NULL; // [状態] - 新規 local lock ハンドルの格納先を用意する。

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_pthread, pthread_mutex_trylock(_, _, _, _))
        .WillOnce(testing::Return(0))
        .WillOnce(testing::Return(EBUSY));
    // [Pre-Assert確認_正常系] - pthread_mutex_trylock が 2 回呼び出されること。
    // [Pre-Assert手順] - 1 回目は 0、2 回目は EBUSY を返却する。
#endif /* PLATFORM_LINUX */

    // Act
    int create_result = cplat_local_lock_create(&lock);               // [手順] - local lock を作成する。
    int first_lock = cplat_local_lock_lock(lock, CPLAT_SYNC_NO_WAIT); // [手順] - 1 回目のロックを取得する。
    int second_try = cplat_local_lock_try_lock(lock);                 // [手順] - ロック保持中に try_lock を試行する。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        create_result); // [確認_正常系] - cplat_local_lock_create の戻り値から、local lock 作成が成功したと判断できること。
    EXPECT_EQ(
        CPLAT_OK,
        first_lock); // [確認_正常系] - cplat_local_lock_lock の戻り値から、1 回目のロック取得が成功したと判断できること。
    EXPECT_EQ(CPLAT_ERR_BUSY,
              second_try); // [確認_正常系] - 保持中の try_lock が BUSY を返すこと。

    // Cleanup
    (void)cplat_local_lock_unlock(lock);
    cplat_local_lock_dispose(lock);
}

// 負のタイムアウト指定が INVALID_ARGUMENT になることの確認
TEST(syncLocalLockTest, negative_timeout_returns_invalid_argument)
{
    // Arrange
    cplat_local_lock *lock = NULL; // [状態] - 新規 local lock ハンドルの格納先を用意する。

    // Pre-Assert

    // Act
    int create_result = cplat_local_lock_create(&lock); // [手順] - local lock を作成する。
    int lock_result = cplat_local_lock_lock(lock, -1);  // [手順] - 負のタイムアウト -1 を指定してロックする。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        create_result); // [確認_正常系] - cplat_local_lock_create の戻り値から、local lock 作成が成功したと判断できること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, lock_result); // [確認_異常系] - 負値で INVALID_ARGUMENT を返すこと。

    // Cleanup
    cplat_local_lock_dispose(lock);
}
