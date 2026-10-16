#include <testfw.h>

#include "syncTestHelper.h"

// 共有ロック同士が同時に取得できることの確認
TEST(syncLocalRwlockTest, shared_locks_can_coexist)
{
    // Arrange
    cplat_local_rwlock *rwlock = nullptr; // [状態] - 新規 rwlock ハンドルの格納先を用意する。

    // Pre-Assert

    // Act
    int create_result = cplat_local_rwlock_create(&rwlock); // [手順] - rwlock を作成する。
    int first_lock =
        cplat_local_rwlock_lock_shared(rwlock, CPLAT_SYNC_NO_WAIT); // [手順] - 1 つ目の共有ロックを取得する。
    int second_lock =
        cplat_local_rwlock_lock_shared(rwlock, CPLAT_SYNC_NO_WAIT); // [手順] - 2 つ目の共有ロックを取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              create_result); // [確認_正常系] - cplat_local_rwlock_create() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              first_lock); // [確認_正常系] - 1 つ目の cplat_local_rwlock_lock_shared() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, second_lock);
    // [確認_正常系] - 2 つ目の cplat_local_rwlock_lock_shared() の戻り値が CPLAT_OK であること。(共有ロック同士が同時に取得されていることの確認)

    // Cleanup
    (void)cplat_local_rwlock_unlock_shared(rwlock);
    (void)cplat_local_rwlock_unlock_shared(rwlock);
    cplat_local_rwlock_dispose(rwlock);
}

// 排他ロック中の共有 try_lock が BUSY になることの確認
TEST(syncLocalRwlockTest, exclusive_lock_blocks_shared_try_lock)
{
    // Arrange
    cplat_local_rwlock *rwlock = nullptr; // [状態] - 新規 rwlock ハンドルの格納先を用意する。

    // Pre-Assert

    // Act
    int create_result = cplat_local_rwlock_create(&rwlock); // [手順] - rwlock を作成する。
    int exclusive_lock =
        cplat_local_rwlock_lock_exclusive(rwlock, CPLAT_SYNC_NO_WAIT); // [手順] - 排他ロックを取得する。
    int shared_try = cplat_local_rwlock_try_lock_shared(rwlock); // [手順] - 排他ロック中に共有ロック取得を試行する。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              create_result); // [確認_正常系] - cplat_local_rwlock_create() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              exclusive_lock); // [確認_正常系] - cplat_local_rwlock_lock_exclusive() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_ERR_BUSY, shared_try);
    // [確認_正常系] - 排他ロック中の cplat_local_rwlock_try_lock_shared() の戻り値が CPLAT_ERR_BUSY であること。

    // Cleanup
    (void)cplat_local_rwlock_unlock_exclusive(rwlock);
    cplat_local_rwlock_dispose(rwlock);
}

// 待機中の writer がいる間は新規 reader が取得できないことの確認
TEST(syncLocalRwlockTest, waiting_writer_prevents_new_reader)
{
    // Arrange
    cplat_local_rwlock *rwlock = nullptr; // [状態] - 新規 rwlock ハンドルの格納先を用意する。

    // Pre-Assert

    // Act
    int create_result = cplat_local_rwlock_create(&rwlock);                       // [手順] - rwlock を作成する。
    int shared_lock = cplat_local_rwlock_lock_shared(rwlock, CPLAT_SYNC_NO_WAIT); // [手順] - 先行 reader を取得する。
    int writer_try =
        cplat_local_rwlock_lock_exclusive(rwlock, 5U); // [手順] - writer 待機を発生させてタイムアウトさせる。
    int second_reader = cplat_local_rwlock_try_lock_shared(rwlock); // [手順] - writer 待機後に新規 reader を試行する。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              create_result); // [確認_正常系] - cplat_local_rwlock_create() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              shared_lock); // [確認_正常系] - cplat_local_rwlock_lock_shared() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_ERR_TIMEOUT, writer_try);
    // [確認_正常系] - reader 解放待ちの cplat_local_rwlock_lock_exclusive() の戻り値が CPLAT_ERR_TIMEOUT であること。
    EXPECT_EQ(CPLAT_OK, second_reader);
    // [確認_正常系] - writer 待機後の cplat_local_rwlock_try_lock_shared() の戻り値が CPLAT_OK であること。(タイムアウトした writer が待機数に残らないことの確認)

    // Cleanup
    (void)cplat_local_rwlock_unlock_shared(rwlock);
    (void)cplat_local_rwlock_unlock_shared(rwlock);
    cplat_local_rwlock_dispose(rwlock);
}
