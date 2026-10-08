#include <testfw.h>

#include "syncTestHelper.h"

#if defined(PLATFORM_LINUX)

    #include <mock_pthread.h>

using testing::DoDefault;
using testing::NiceMock;
using testing::StrEq;

class syncFailureInjectionTest : public Test
{
  protected:
    InterprocessOsMocks os_;
};

/*
 * cplat_local_lock_create
 */

// ミューテックスの初期化に失敗した場合に生成が失敗することの確認
// Windows は CRITICAL_SECTION を使うため、この失敗経路は Linux のみに存在する
TEST_F(syncFailureInjectionTest, local_lock_create_fails_when_mutex_init_fails)
{
    // Arrange
    NiceMock<Mock_pthread> mock_pthread;
    cplat_local_lock *lock = NULL; // [状態] - ハンドルの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_mutex_init(_, _, _, _, _))
        .WillOnce(Return(ENOMEM))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - pthread_mutex_init が 1 回目に呼び出されること。
                                      // [Pre-Assert手順] - 1 回目は ENOMEM を返却し、以降は本物へ委譲する。

    // Act
    int actual_ret = cplat_local_lock_create(&lock); // [手順] - cplat_local_lock_create を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_local_lock_create の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ((cplat_local_lock *)NULL, lock); // [確認_異常系] - ハンドルが設定されないこと。
}

// ハンドルの確保に失敗した場合に生成が失敗することの確認
TEST_F(syncFailureInjectionTest, local_lock_create_fails_when_allocation_fails)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_local_lock *lock = NULL; // [状態] - ハンドルの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_calloc(_, _))
        .WillOnce(Return(nullptr))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - cplat_calloc が 1 回目に呼び出されること。
                                      // [Pre-Assert手順] - 1 回目は NULL を返却し、以降は本物へ委譲する。

    // Act
    int actual_ret = cplat_local_lock_create(&lock); // [手順] - cplat_local_lock_create を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_local_lock_create の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ((cplat_local_lock *)NULL, lock); // [確認_異常系] - ハンドルが設定されないこと。
}

/*
 * cplat_interprocess_lock_open
 */

// ロック ファイルを開けない場合に取得が失敗することの確認
TEST_F(syncFailureInjectionTest, interprocess_lock_open_fails_for_unopenable_path)
{
    // Arrange
    cplat_interprocess_lock *lock = NULL; // [状態] - ハンドルの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(os_.fcntl, open(_, _, _, StrEq(kLockIdentity), _, _))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - open が識別子 sync.lock で 1 回失敗すること。
                               // [Pre-Assert手順] - -1 を返却する。

    // Act
    int actual_ret = cplat_interprocess_lock_open(kLockIdentity,
                                                  &lock); // [手順] - open 失敗を注入してロックを開く。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_interprocess_lock_open の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ((cplat_interprocess_lock *)NULL, lock); // [確認_異常系] - ハンドルが設定されないこと。
}

// open の失敗を注入した場合にロック取得が失敗することの確認
TEST_F(syncFailureInjectionTest, interprocess_lock_open_reports_open_failure)
{
    // Arrange
    cplat_interprocess_lock *lock = NULL; // [状態] - ハンドルの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(os_.fcntl, open(_, _, _, _, _, _))
        .WillOnce(Return(-1))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - open が 1 回目に失敗すること。

    // Act
    int actual_ret = cplat_interprocess_lock_open(kLockIdentity, &lock); // [手順] - open 失敗を注入してロックを開く。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - open 失敗時の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ((cplat_interprocess_lock *)NULL, lock); // [確認_異常系] - open 失敗時にハンドルが NULL であること。
}

// 識別子の複製に失敗した場合に取得が失敗することの確認
// Windows は識別子を CreateMutexW へ直接渡すため、この失敗経路は Linux のみに存在する
TEST_F(syncFailureInjectionTest, interprocess_lock_open_fails_when_identity_duplication_fails)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_interprocess_lock *lock = NULL; // [状態] - ハンドルの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_strdup(_))
        .WillOnce(Return(nullptr))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - cplat_strdup が 1 回目に呼び出されること。
                                      // [Pre-Assert手順] - 1 回目は NULL を返却し、以降は本物へ委譲する。

    // Act
    int actual_ret =
        cplat_interprocess_lock_open(kLockIdentity, &lock); // [手順] - cplat_interprocess_lock_open を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_interprocess_lock_open の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ((cplat_interprocess_lock *)NULL, lock); // [確認_異常系] - ハンドルが設定されないこと。
}

// ハンドルの確保に失敗した場合に取得が失敗することの確認
TEST_F(syncFailureInjectionTest, interprocess_lock_open_fails_when_allocation_fails)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_interprocess_lock *lock = NULL; // [状態] - ハンドルの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_calloc(_, _))
        .WillOnce(Return(nullptr))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - cplat_calloc が 1 回目に呼び出されること。
                                      // [Pre-Assert手順] - 1 回目は NULL を返却し、以降は本物へ委譲する。

    // Act
    int actual_ret =
        cplat_interprocess_lock_open(kLockIdentity, &lock); // [手順] - cplat_interprocess_lock_open を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_interprocess_lock_open の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ((cplat_interprocess_lock *)NULL, lock); // [確認_異常系] - ハンドルが設定されないこと。
}

/*
 * flock の失敗
 */

// ロック解除に失敗した場合に通知されることの確認
// Windows は ReleaseMutex を使うため、この失敗経路は Linux のみに存在する
TEST_F(syncFailureInjectionTest, interprocess_lock_unlock_reports_flock_failure)
{
    // Arrange
    cplat_interprocess_lock *lock = NULL;

    ASSERT_EQ(CPLAT_OK,
              cplat_interprocess_lock_open(kLockIdentity, &lock)); // [状態] - interprocess lock を開いた状態とする。
    // [状態確認] - cplat_interprocess_lock_open の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_interprocess_lock_lock(lock, CPLAT_SYNC_NO_WAIT)); // [状態] - ロックを取得済みにする。
    // [状態確認] - cplat_interprocess_lock_lock の戻り値が CPLAT_OK であること。

    // Pre-Assert
    EXPECT_CALL(os_.sys_file, flock(_, _, _, _, LOCK_UN))
        .WillOnce(Return(-1))
        .WillRepeatedly(
            DoDefault()); // [Pre-Assert確認_異常系] - flock が LOCK_UN を指定して 1 回目に呼び出されること。
                          // [Pre-Assert手順] - 1 回目は -1 を返却し、以降は既定の成功へ戻す。

    // Act
    int actual_ret = cplat_interprocess_lock_unlock(lock); // [手順] - cplat_interprocess_lock_unlock を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_interprocess_lock_unlock の戻り値が CPLAT_ERR_UNKNOWN であること。

    // Cleanup
    cplat_interprocess_lock_dispose(lock);
}

/*
 * cplat_thread_create
 */

// スレッドの生成に失敗した場合に通知されることの確認
// Windows は _beginthreadex を使うため、この失敗経路は Linux のみに存在する
TEST_F(syncFailureInjectionTest, thread_create_fails_when_pthread_create_fails)
{
    // Arrange
    NiceMock<Mock_pthread> mock_pthread;
    cplat_thread *thread = NULL; // [状態] - ハンドルの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_create(_, _, _, _, _, _, _))
        .WillOnce(Return(EAGAIN))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - pthread_create が 1 回目に呼び出されること。
                                      // [Pre-Assert手順] - 1 回目は EAGAIN を返却し、以降は本物へ委譲する。

    // Act
    int actual_ret = cplat_thread_create(&thread, [](void *) {}, NULL); // [手順] - cplat_thread_create を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_thread_create の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ((cplat_thread *)NULL, thread); // [確認_異常系] - ハンドルが設定されないこと。
}

// pthread の戻り値を共通結果へ変換することの確認
TEST_F(syncFailureInjectionTest, local_lock_maps_pthread_failure_results)
{
    // Arrange
    cplat_local_lock *lock = NULL;
    ASSERT_EQ(CPLAT_OK, cplat_local_lock_create(&lock)); // [状態] - local lock を生成する。
    // [状態確認] - cplat_local_lock_create の戻り値が CPLAT_OK であること。
    NiceMock<Mock_pthread> mock_pthread;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_mutex_lock(_, _, _, _))
        .WillOnce(Return(EBUSY)); // [Pre-Assert確認_異常系] - mutex lock が EBUSY を返すこと。
    EXPECT_CALL(mock_pthread, pthread_mutex_trylock(_, _, _, _))
        .WillOnce(Return(EACCES)); // [Pre-Assert確認_異常系] - mutex trylock が EACCES を返すこと。
    EXPECT_CALL(mock_pthread, pthread_mutex_unlock(_, _, _, _))
        .WillOnce(Return(EINVAL)); // [Pre-Assert確認_異常系] - mutex unlock が EINVAL を返すこと。

    // Act
    int lock_result =
        cplat_local_lock_lock(lock, CPLAT_SYNC_WAIT_FOREVER); // [手順] - EBUSY を返す mutex lock を実行する。
    int try_result = cplat_local_lock_try_lock(lock);         // [手順] - EACCES を返す mutex trylock を実行する。
    int unlock_result = cplat_local_lock_unlock(lock);        // [手順] - EINVAL を返す mutex unlock を実行する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUSY,
              lock_result); // [確認_異常系] - mutex lock の EBUSY が BUSY へ変換されること。
    EXPECT_EQ(CPLAT_ERR_BUSY,
              try_result); // [確認_異常系] - mutex trylock の EACCES が BUSY へ変換されること。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              unlock_result); // [確認_異常系] - mutex unlock の EINVAL が UNKNOWN へ変換されること。

    // Cleanup
    cplat_local_lock_dispose(lock);
}

// 条件変数の初期化失敗が生成失敗として報告されることの確認
TEST_F(syncFailureInjectionTest, condvar_create_reports_condition_attribute_failure)
{
    // Arrange
    NiceMock<Mock_pthread> mock_pthread;
    cplat_condvar *cv = NULL; // [状態] - 条件変数ハンドルの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_condattr_init(_, _, _, _))
        .WillOnce(Return(ENOMEM))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - 条件変数属性の初期化が失敗すること。

    // Act
    int actual_ret = cplat_condvar_create(&cv); // [手順] - 条件変数を生成する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret);                // [確認_異常系] - 属性初期化失敗時の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ((cplat_condvar *)NULL, cv); // [確認_異常系] - 生成失敗時にハンドルが NULL であること。
}

#endif /* PLATFORM_LINUX */
