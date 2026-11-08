#include <testfw.h>

#include "syncTestHelper.h"
#include "sync_linux.inject.h"

#if defined(PLATFORM_LINUX)
    #include <mock_pthread.h>
    #include <mock_time.h>

using testing::Assign;
using testing::DoAll;
using testing::DoDefault;
using testing::Invoke;

// 時刻取得に失敗した内部関数が出力値を変更しないことの確認
TEST(syncClockFailureTest, failed_clock_preserves_helper_outputs)
{
    // Arrange
    NiceMock<Mock_time> mock_time;
    uint64_t milliseconds = 1234U;
    struct timespec deadline = {12, 345};

    // Pre-Assert
    EXPECT_CALL(mock_time, clock_gettime(_, _, _, CLOCK_MONOTONIC, _))
        .Times(2)
        .WillRepeatedly(DoAll(Assign(&errno, EIO), Return(-1)));

    // [Pre-Assert確認_異常系] - CLOCK_MONOTONIC の取得が 2 回呼び出されること。
    // [Pre-Assert手順] - 出力時刻へ書き込まず、errno に EIO を設定して -1 を返す。

    // Act
    int milliseconds_result = test_sync_monotonic_ms(&milliseconds);    // [手順] - ミリ秒の取得を失敗させる。
    int deadline_result = test_sync_monotonic_deadline(&deadline, 200); // [手順] - 期限の取得を失敗させる。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, milliseconds_result); // [確認_異常系] - ミリ秒取得が CPLAT_ERR_UNKNOWN を返すこと。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, deadline_result);     // [確認_異常系] - 期限取得が CPLAT_ERR_UNKNOWN を返すこと。
    EXPECT_EQ(1234U, milliseconds);                    // [確認_異常系] - milliseconds が 1234U のままであること。
    EXPECT_EQ(12, deadline.tv_sec);                    // [確認_異常系] - deadline.tv_sec が 12 のままであること。
    EXPECT_EQ(345, deadline.tv_nsec);                  // [確認_異常系] - deadline.tv_nsec が 345 のままであること。
}

// local lock が開始時と再試行時の時刻取得失敗を返し、再取得できることの確認
TEST(syncClockFailureTest, local_lock_reports_initial_and_retry_clock_failures)
{
    // Arrange
    cplat_local_lock *lock = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_local_lock_create(&lock)); // [状態] - local lock を生成する。

    // [状態確認] - cplat_local_lock_create() が CPLAT_OK を返すこと。
    NiceMock<Mock_time> mock_time;
    NiceMock<Mock_pthread> mock_pthread;

    // Pre-Assert
    EXPECT_CALL(mock_time, clock_gettime(_, _, _, CLOCK_MONOTONIC, _))
        .WillOnce(Return(-1))
        .WillOnce(Invoke(
            [](const char *, int, const char *, clockid_t, struct timespec *ts)
            {
                ts->tv_sec = 0;
                ts->tv_nsec = 0;
                return 0;
            }))
        .WillOnce(Return(-1));

    // [Pre-Assert確認_異常系] - CLOCK_MONOTONIC の取得が 3 回呼び出されること。
    // [Pre-Assert手順] - 1 回目と 3 回目は出力せず失敗し、2 回目だけ時刻 0 を返す。
    EXPECT_CALL(mock_pthread, pthread_mutex_trylock(_, _, _, _)).WillOnce(Return(EBUSY)).WillOnce(DoDefault());

    // [Pre-Assert確認_異常系] - pthread_mutex_trylock() が 2 回呼び出されること。
    // [Pre-Assert手順] - 最初は EBUSY を返し、再取得では実関数へ委譲する。

    // Act
    int initial_result = cplat_local_lock_lock(lock, 10); // [手順] - 開始時の時刻取得を失敗させる。
    int retry_result = cplat_local_lock_lock(lock, 10);   // [手順] - 再試行の時刻取得を失敗させる。
    int recover_result = cplat_local_lock_try_lock(lock); // [手順] - 時刻取得が不要な即時取得を行う。
    int unlock_result = cplat_local_lock_unlock(lock);    // [手順] - 取得したロックを解放する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, initial_result); // [確認_異常系] - 開始時の失敗を CPLAT_ERR_UNKNOWN として返すこと。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, retry_result);   // [確認_異常系] - 再試行時の失敗を CPLAT_ERR_UNKNOWN として返すこと。
    EXPECT_EQ(CPLAT_OK, recover_result);          // [確認_正常系] - 失敗後の即時取得が CPLAT_OK を返すこと。
    EXPECT_EQ(CPLAT_OK, unlock_result);           // [確認_正常系] - 取得したロックの解放が CPLAT_OK を返すこと。

    // Cleanup
    cplat_local_lock_dispose(lock);
}

// interprocess_lock が時刻取得の失敗後も未取得の状態を維持することの確認
TEST(syncClockFailureTest, interprocess_lock_preserves_state_on_clock_failures)
{
    // Arrange
    InterprocessOsMocks os;
    cplat_interprocess_lock *lock = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_interprocess_lock_open(kLockIdentity, &lock)); // [状態] - ロックを生成する。

    // [状態確認] - cplat_interprocess_lock_open() が CPLAT_OK を返すこと。
    NiceMock<Mock_time> mock_time;

    // Pre-Assert
    EXPECT_CALL(mock_time, clock_gettime(_, _, _, CLOCK_MONOTONIC, _))
        .WillOnce(Return(-1))
        .WillOnce(Invoke(
            [](const char *, int, const char *, clockid_t, struct timespec *ts)
            {
                ts->tv_sec = 0;
                ts->tv_nsec = 0;
                return 0;
            }))
        .WillOnce(Return(-1))
        .WillOnce(Invoke(
            [](const char *, int, const char *, clockid_t, struct timespec *ts)
            {
                ts->tv_sec = 0;
                ts->tv_nsec = 0;
                return 0;
            }));

    // [Pre-Assert確認_異常系] - CLOCK_MONOTONIC の取得が 4 回呼び出されること。
    // [Pre-Assert手順] - 1 回目と 3 回目は出力せず失敗し、2 回目と 4 回目は時刻 0 を返す。
    EXPECT_CALL(os.sys_file, flock(_, _, _, kFakeFd, LOCK_EX | LOCK_NB))
        .WillOnce(DoAll(Assign(&errno, EWOULDBLOCK), Return(-1)))
        .WillOnce(Return(0));

    // [Pre-Assert確認_異常系] - kFakeFd と LOCK_EX | LOCK_NB を指定した flock() が 2 回呼び出されること。
    // [Pre-Assert手順] - 1 回目は errno に EWOULDBLOCK を設定して失敗し、再取得は成功する。
    EXPECT_CALL(os.sys_file, flock(_, _, _, kFakeFd, LOCK_UN)).WillOnce(Return(0));

    // [Pre-Assert確認_正常系] - kFakeFd と LOCK_UN を指定した flock() が 1 回呼び出されること。
    // [Pre-Assert手順] - 取得後の解放で 0 を返す。

    // Act
    int initial_result = cplat_interprocess_lock_lock(lock, 10); // [手順] - 開始時の時刻取得を失敗させる。
    int retry_result = cplat_interprocess_lock_lock(lock, 10);   // [手順] - 再試行の時刻取得を失敗させる。
    int recover_result =
        cplat_interprocess_lock_lock(lock, CPLAT_SYNC_NO_WAIT); // [手順] - 同じハンドルを即時取得する。
    int unlock_result = cplat_interprocess_lock_unlock(lock);   // [手順] - 取得したロックを解放する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, initial_result); // [確認_異常系] - 開始時の失敗を CPLAT_ERR_UNKNOWN として返すこと。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, retry_result);   // [確認_異常系] - 再試行時の失敗を CPLAT_ERR_UNKNOWN として返すこと。
    EXPECT_EQ(CPLAT_OK, recover_result);          // [確認_正常系] - 失敗後の取得が CPLAT_OK を返すこと。
    EXPECT_EQ(CPLAT_OK, unlock_result);           // [確認_正常系] - 取得後の解放が CPLAT_OK を返すこと。

    // Cleanup
    cplat_interprocess_lock_dispose(lock);
}

// interprocess_rwlock_shared が時刻取得の失敗後も未取得の状態を維持することの確認
TEST(syncClockFailureTest, interprocess_rwlock_shared_preserves_state_on_clock_failures)
{
    // Arrange
    InterprocessOsMocks os;
    cplat_interprocess_rwlock *lock = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_interprocess_rwlock_open(kLockIdentity, &lock)); // [状態] - ロックを生成する。

    // [状態確認] - cplat_interprocess_rwlock_open() が CPLAT_OK を返すこと。
    NiceMock<Mock_time> mock_time;

    // Pre-Assert
    EXPECT_CALL(mock_time, clock_gettime(_, _, _, CLOCK_MONOTONIC, _))
        .WillOnce(Return(-1))
        .WillOnce(Invoke(
            [](const char *, int, const char *, clockid_t, struct timespec *ts)
            {
                ts->tv_sec = 0;
                ts->tv_nsec = 0;
                return 0;
            }))
        .WillOnce(Return(-1))
        .WillOnce(Invoke(
            [](const char *, int, const char *, clockid_t, struct timespec *ts)
            {
                ts->tv_sec = 0;
                ts->tv_nsec = 0;
                return 0;
            }));

    // [Pre-Assert確認_異常系] - CLOCK_MONOTONIC の取得が 4 回呼び出されること。
    // [Pre-Assert手順] - 1 回目と 3 回目は出力せず失敗し、2 回目と 4 回目は時刻 0 を返す。
    EXPECT_CALL(os.sys_file, flock(_, _, _, kFakeFd, LOCK_SH | LOCK_NB))
        .WillOnce(DoAll(Assign(&errno, EWOULDBLOCK), Return(-1)))
        .WillOnce(Return(0));

    // [Pre-Assert確認_異常系] - kFakeFd と LOCK_SH | LOCK_NB を指定した flock() が 2 回呼び出されること。
    // [Pre-Assert手順] - 1 回目は errno に EWOULDBLOCK を設定して失敗し、再取得は成功する。
    EXPECT_CALL(os.sys_file, flock(_, _, _, kFakeFd, LOCK_UN)).WillOnce(Return(0));

    // [Pre-Assert確認_正常系] - kFakeFd と LOCK_UN を指定した flock() が 1 回呼び出されること。
    // [Pre-Assert手順] - 取得後の解放で 0 を返す。

    // Act
    int initial_result = cplat_interprocess_rwlock_lock_shared(lock, 10); // [手順] - 開始時の時刻取得を失敗させる。
    int retry_result = cplat_interprocess_rwlock_lock_shared(lock, 10);   // [手順] - 再試行の時刻取得を失敗させる。
    int recover_result =
        cplat_interprocess_rwlock_lock_shared(lock, CPLAT_SYNC_NO_WAIT); // [手順] - 同じハンドルを即時取得する。
    int unlock_result = cplat_interprocess_rwlock_unlock(lock);          // [手順] - 取得したロックを解放する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, initial_result); // [確認_異常系] - 開始時の失敗を CPLAT_ERR_UNKNOWN として返すこと。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, retry_result);   // [確認_異常系] - 再試行時の失敗を CPLAT_ERR_UNKNOWN として返すこと。
    EXPECT_EQ(CPLAT_OK, recover_result);          // [確認_正常系] - 失敗後の取得が CPLAT_OK を返すこと。
    EXPECT_EQ(CPLAT_OK, unlock_result);           // [確認_正常系] - 取得後の解放が CPLAT_OK を返すこと。

    // Cleanup
    cplat_interprocess_rwlock_dispose(lock);
}

// interprocess_rwlock_exclusive が時刻取得の失敗後も未取得の状態を維持することの確認
TEST(syncClockFailureTest, interprocess_rwlock_exclusive_preserves_state_on_clock_failures)
{
    // Arrange
    InterprocessOsMocks os;
    cplat_interprocess_rwlock *lock = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_interprocess_rwlock_open(kLockIdentity, &lock)); // [状態] - ロックを生成する。

    // [状態確認] - cplat_interprocess_rwlock_open() が CPLAT_OK を返すこと。
    NiceMock<Mock_time> mock_time;

    // Pre-Assert
    EXPECT_CALL(mock_time, clock_gettime(_, _, _, CLOCK_MONOTONIC, _))
        .WillOnce(Return(-1))
        .WillOnce(Invoke(
            [](const char *, int, const char *, clockid_t, struct timespec *ts)
            {
                ts->tv_sec = 0;
                ts->tv_nsec = 0;
                return 0;
            }))
        .WillOnce(Return(-1))
        .WillOnce(Invoke(
            [](const char *, int, const char *, clockid_t, struct timespec *ts)
            {
                ts->tv_sec = 0;
                ts->tv_nsec = 0;
                return 0;
            }));

    // [Pre-Assert確認_異常系] - CLOCK_MONOTONIC の取得が 4 回呼び出されること。
    // [Pre-Assert手順] - 1 回目と 3 回目は出力せず失敗し、2 回目と 4 回目は時刻 0 を返す。
    EXPECT_CALL(os.sys_file, flock(_, _, _, kFakeFd, LOCK_EX | LOCK_NB))
        .WillOnce(DoAll(Assign(&errno, EWOULDBLOCK), Return(-1)))
        .WillOnce(Return(0));

    // [Pre-Assert確認_異常系] - kFakeFd と LOCK_EX | LOCK_NB を指定した flock() が 2 回呼び出されること。
    // [Pre-Assert手順] - 1 回目は errno に EWOULDBLOCK を設定して失敗し、再取得は成功する。
    EXPECT_CALL(os.sys_file, flock(_, _, _, kFakeFd, LOCK_UN)).WillOnce(Return(0));

    // [Pre-Assert確認_正常系] - kFakeFd と LOCK_UN を指定した flock() が 1 回呼び出されること。
    // [Pre-Assert手順] - 取得後の解放で 0 を返す。

    // Act
    int initial_result = cplat_interprocess_rwlock_lock_exclusive(lock, 10); // [手順] - 開始時の時刻取得を失敗させる。
    int retry_result = cplat_interprocess_rwlock_lock_exclusive(lock, 10);   // [手順] - 再試行の時刻取得を失敗させる。
    int recover_result =
        cplat_interprocess_rwlock_lock_exclusive(lock, CPLAT_SYNC_NO_WAIT); // [手順] - 同じハンドルを即時取得する。
    int unlock_result = cplat_interprocess_rwlock_unlock(lock);             // [手順] - 取得したロックを解放する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, initial_result); // [確認_異常系] - 開始時の失敗を CPLAT_ERR_UNKNOWN として返すこと。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, retry_result);   // [確認_異常系] - 再試行時の失敗を CPLAT_ERR_UNKNOWN として返すこと。
    EXPECT_EQ(CPLAT_OK, recover_result);          // [確認_正常系] - 失敗後の取得が CPLAT_OK を返すこと。
    EXPECT_EQ(CPLAT_OK, unlock_result);           // [確認_正常系] - 取得後の解放が CPLAT_OK を返すこと。

    // Cleanup
    cplat_interprocess_rwlock_dispose(lock);
}

// 条件変数の期限取得に失敗しても呼び出し元の mutex を保持することの確認
TEST(syncClockFailureTest, condvar_clock_failure_keeps_caller_mutex_locked)
{
    // Arrange
    cplat_condvar *cv = nullptr;
    cplat_local_lock *lock = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_condvar_create(&cv)); // [状態] - 条件変数を生成する。

    // [状態確認] - cplat_condvar_create() が CPLAT_OK を返すこと。
    ASSERT_EQ(CPLAT_OK, cplat_local_lock_create(&lock)); // [状態] - mutex を生成する。

    // [状態確認] - cplat_local_lock_create() が CPLAT_OK を返すこと。
    ASSERT_EQ(CPLAT_OK, cplat_local_lock_try_lock(lock)); // [状態] - mutex を取得する。

    // [状態確認] - cplat_local_lock_try_lock() が CPLAT_OK を返すこと。
    NiceMock<Mock_time> mock_time;
    NiceMock<Mock_pthread> mock_pthread;

    // Pre-Assert
    EXPECT_CALL(mock_time, clock_gettime(_, _, _, CLOCK_MONOTONIC, _)).WillOnce(Return(-1));

    // [Pre-Assert確認_異常系] - CLOCK_MONOTONIC の取得が 1 回呼び出されること。
    // [Pre-Assert手順] - 出力を設定せず -1 を返す。
    EXPECT_CALL(mock_pthread, pthread_cond_timedwait(_, _, _, _, _, _)).Times(0);

    // [Pre-Assert確認_異常系] - pthread_cond_timedwait() が呼び出されないこと。

    // Act
    int wait_result = cplat_condvar_wait(cv, lock, 10);        // [手順] - 期限の取得を失敗させる。
    int still_locked_result = cplat_local_lock_try_lock(lock); // [手順] - 呼び出し元の mutex の保持を調べる。
    int unlock_result = cplat_local_lock_unlock(lock);         // [手順] - 呼び出し元の mutex を解放する。
    int recover_result = cplat_local_lock_try_lock(lock);      // [手順] - 解放後に再取得する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, wait_result);      // [確認_異常系] - 待機が CPLAT_ERR_UNKNOWN を返すこと。
    EXPECT_EQ(CPLAT_ERR_BUSY, still_locked_result); // [確認_異常系] - 呼び出し元の mutex が保持されたままであること。
    EXPECT_EQ(CPLAT_OK, unlock_result);             // [確認_正常系] - 呼び出し元が解放できること。
    EXPECT_EQ(CPLAT_OK, recover_result);            // [確認_正常系] - 解放後に再取得できること。

    // Cleanup
    (void)cplat_local_lock_unlock(lock);
    cplat_condvar_dispose(cv);
    cplat_local_lock_dispose(lock);
}

// rwlock の期限取得失敗が待機者数や内部 mutex を残さないことの確認
TEST(syncClockFailureTest, local_rwlock_clock_failures_allow_subsequent_read_and_write)
{
    // Arrange
    cplat_local_rwlock *lock = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_local_rwlock_create(&lock)); // [状態] - rwlock を生成する。

    // [状態確認] - cplat_local_rwlock_create() が CPLAT_OK を返すこと。
    NiceMock<Mock_time> mock_time;

    // Pre-Assert
    EXPECT_CALL(mock_time, clock_gettime(_, _, _, CLOCK_MONOTONIC, _))
        .WillOnce(Return(-1))
        .WillOnce(Return(-1))
        .WillOnce(Invoke(
            [](const char *, int, const char *, clockid_t, struct timespec *ts)
            {
                ts->tv_sec = 0;
                ts->tv_nsec = 0;
                return 0;
            }))
        .WillOnce(Invoke(
            [](const char *, int, const char *, clockid_t, struct timespec *ts)
            {
                ts->tv_sec = 0;
                ts->tv_nsec = 0;
                return 0;
            }));

    // [Pre-Assert確認_異常系] - CLOCK_MONOTONIC の取得が 4 回呼び出されること。
    // [Pre-Assert手順] - 最初の 2 回は出力せず失敗し、以降は時刻 0 を返す。

    // Act
    int shared_result = cplat_local_rwlock_lock_shared(lock, 10);        // [手順] - 共有取得の期限生成を失敗させる。
    int exclusive_result = cplat_local_rwlock_lock_exclusive(lock, 10);  // [手順] - 排他取得の期限生成を失敗させる。
    int read_result = cplat_local_rwlock_try_lock_shared(lock);          // [手順] - 失敗後に共有取得する。
    int read_unlock_result = cplat_local_rwlock_unlock_shared(lock);     // [手順] - 共有ロックを解放する。
    int write_result = cplat_local_rwlock_try_lock_exclusive(lock);      // [手順] - 失敗後に排他取得する。
    int write_unlock_result = cplat_local_rwlock_unlock_exclusive(lock); // [手順] - 排他ロックを解放する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, shared_result);    // [確認_異常系] - 共有取得が CPLAT_ERR_UNKNOWN を返すこと。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, exclusive_result); // [確認_異常系] - 排他取得が CPLAT_ERR_UNKNOWN を返すこと。
    EXPECT_EQ(CPLAT_OK, read_result);               // [確認_正常系] - 共有取得が待機者の残留で妨げられないこと。
    EXPECT_EQ(CPLAT_OK, read_unlock_result);        // [確認_正常系] - 共有ロックを解放できること。
    EXPECT_EQ(CPLAT_OK, write_result);              // [確認_正常系] - 排他取得が CPLAT_OK を返すこと。
    EXPECT_EQ(CPLAT_OK, write_unlock_result);       // [確認_正常系] - 排他ロックを解放できること。

    // Cleanup
    cplat_local_rwlock_dispose(lock);
}

// 終了待機の時刻取得失敗が thread ハンドルを解放しないことの確認
TEST(syncClockFailureTest, join_clock_failures_preserve_thread_for_later_join)
{
    // Arrange
    cplat_thread *thread = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_thread_create(&thread, [](void *) {}, nullptr)); // [状態] - 終了する thread を生成する。

    // [状態確認] - cplat_thread_create() が CPLAT_OK を返すこと。
    NiceMock<Mock_time> mock_time;
    NiceMock<Mock_pthread> mock_pthread;

    // Pre-Assert
    EXPECT_CALL(mock_time, clock_gettime(_, _, _, CLOCK_MONOTONIC, _))
        .WillOnce(Return(-1))
        .WillOnce(Invoke(
            [](const char *, int, const char *, clockid_t, struct timespec *ts)
            {
                ts->tv_sec = 0;
                ts->tv_nsec = 0;
                return 0;
            }))
        .WillOnce(Return(-1));

    // [Pre-Assert確認_異常系] - CLOCK_MONOTONIC の取得が 3 回呼び出されること。
    // [Pre-Assert手順] - 1 回目と 3 回目は出力せず失敗し、2 回目だけ時刻 0 を返す。
    EXPECT_CALL(mock_pthread, pthread_tryjoin_np(_, _, _, _, _)).WillOnce(Return(EBUSY));

    // [Pre-Assert確認_異常系] - pthread_tryjoin_np() が 1 回呼び出されること。
    // [Pre-Assert手順] - 終了待ちが継続するよう EBUSY を返す。

    // Act
    int initial_result = cplat_thread_join(thread, 10); // [手順] - 開始時の時刻取得を失敗させる。
    int retry_result = cplat_thread_join(thread, 10);   // [手順] - 再試行の時刻取得を失敗させる。
    int recover_result =
        cplat_thread_join(thread, CPLAT_SYNC_WAIT_FOREVER); // [手順] - 保持したハンドルで終了待ちを完了する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, initial_result); // [確認_異常系] - 開始時の失敗が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, retry_result);   // [確認_異常系] - 再試行時の失敗が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(CPLAT_OK, recover_result);          // [確認_正常系] - 同じハンドルで終了待ちを完了できること。
}

#endif /* PLATFORM_LINUX */
