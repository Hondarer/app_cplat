#include <testfw.h>

#include "syncTestHelper.h"

using testing::Assign;
using testing::DoAll;
#if defined(PLATFORM_LINUX)
using testing::StrEq;
#endif

// descriptor の出力と復元で同一の interprocess lock を再取得できることの確認
TEST(syncInterprocessLockTest, descriptor_round_trip_reopens_same_lock)
{
    // Arrange
#if defined(PLATFORM_LINUX)
    InterprocessOsMocks os;
    const char *path = kLockIdentity; // [状態] - 識別子を sync.lock とする。
#else
    char path_buf[256];
    make_test_interprocess_path(path_buf, sizeof(path_buf), "interprocess_lock");
    const char *path = path_buf; // [状態] - テスト用 lock file パスを用意する。
#endif
    cplat_interprocess_lock *lock = nullptr;
    cplat_interprocess_lock *restored = nullptr;
    unsigned char descriptor[512];
    size_t descriptor_size = sizeof(descriptor);

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(os.fcntl, open(_, _, _, StrEq(path), O_RDWR | O_CREAT | O_CLOEXEC, 0666))
        .WillOnce(Return(kFakeFd))
        .WillOnce(Return(
            kFakeFd2)); // [Pre-Assert確認_正常系] - open() が path と等しい文字列、O_RDWR | O_CREAT | O_CLOEXEC、0666 を指定して 2 回呼び出されること。
                        // [Pre-Assert手順] - open() から 1 回目はセンチネル記述子 7、2 回目は 8 を返す。
    EXPECT_CALL(os.sys_file, flock(_, _, _, kFakeFd, LOCK_EX | LOCK_NB))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - flock() が kFakeFd、LOCK_EX | LOCK_NB を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - flock() から 0 を返す。
    EXPECT_CALL(os.sys_file, flock(_, _, _, kFakeFd2, LOCK_EX | LOCK_NB))
        .WillOnce(DoAll(
            Assign(&errno, EWOULDBLOCK),
            Return(
                -1))); // [Pre-Assert確認_正常系] - flock() が kFakeFd2、LOCK_EX | LOCK_NB を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - flock() から、errno に EWOULDBLOCK を設定して -1 を返す。
#endif

    // Act
    int open_result = cplat_interprocess_lock_open(
        path, &lock); // [手順] - path を指定して cplat_interprocess_lock_open() を呼び出す。
    int export_result = cplat_interprocess_lock_export_descriptor(
        lock, descriptor,
        &descriptor_size); // [手順] - descriptor のバッファーを指定して cplat_interprocess_lock_export_descriptor() を呼び出す。
    int import_result = cplat_interprocess_lock_import_descriptor(
        descriptor, descriptor_size,
        &restored); // [手順] - descriptor を指定して cplat_interprocess_lock_import_descriptor() を呼び出す。
    int first_lock = cplat_interprocess_lock_lock(
        lock,
        CPLAT_SYNC_NO_WAIT); // [手順] - 元ハンドルと CPLAT_SYNC_NO_WAIT を指定して cplat_interprocess_lock_lock() を呼び出す。
    int second_try = cplat_interprocess_lock_try_lock(
        restored); // [手順] - 復元ハンドルを指定して cplat_interprocess_lock_try_lock() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              open_result); // [確認_正常系] - cplat_interprocess_lock_open() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        export_result); // [確認_正常系] - cplat_interprocess_lock_export_descriptor() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        import_result); // [確認_正常系] - cplat_interprocess_lock_import_descriptor() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        first_lock); // [確認_正常系] - 元ハンドルに対する cplat_interprocess_lock_lock() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_BUSY,
        second_try); // [確認_正常系] - 復元ハンドルに対する cplat_interprocess_lock_try_lock() の戻り値が CPLAT_ERR_BUSY であること。(同一のロック インスタンスを参照することの確認)

    // Cleanup
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(os.sys_file, flock(_, _, _, kFakeFd, LOCK_UN))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - flock() が kFakeFd、LOCK_UN を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - flock() から 0 を返す。
    EXPECT_CALL(os.unistd, close(_, _, _, kFakeFd))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - close() が kFakeFd を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - close() から 0 を返す。
    EXPECT_CALL(os.unistd, close(_, _, _, kFakeFd2))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - close() が kFakeFd2 を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - close() から 0 を返す。
#endif
    (void)cplat_interprocess_lock_unlock(lock);
    cplat_interprocess_lock_dispose(restored);
    cplat_interprocess_lock_dispose(lock);
#if defined(PLATFORM_WINDOWS)
    TEST_INTERPROCESS_UNLINK(path);
#endif
}

// rwlock の descriptor を interprocess lock として import できないことの確認
TEST(syncInterprocessLockTest, rejects_rwlock_descriptor)
{
    // Arrange
#if defined(PLATFORM_LINUX)
    InterprocessOsMocks os;
    const char *path = kRwlockIdentity; // [状態] - 識別子を sync.rwlock とする。
#else
    char path_buf[256];
    make_test_interprocess_path(path_buf, sizeof(path_buf), "interprocess_lock_kind");
    const char *path = path_buf; // [状態] - テスト用 lock file パスを用意する。
#endif
    cplat_interprocess_rwlock *rwlock = nullptr;
    cplat_interprocess_lock *lock = nullptr;
    unsigned char descriptor[512];
    size_t descriptor_size = sizeof(descriptor);

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(os.fcntl, open(_, _, _, StrEq(path), O_RDWR | O_CREAT | O_CLOEXEC, 0666))
        .WillOnce(Return(
            kFakeFd)); // [Pre-Assert確認_正常系] - open() が path と等しい文字列、O_RDWR | O_CREAT | O_CLOEXEC、0666 を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - open() からセンチネル記述子 7 を返す。
#endif

    // Act
    int open_result = cplat_interprocess_rwlock_open(
        path, &rwlock); // [手順] - path を指定して cplat_interprocess_rwlock_open() を呼び出す。
    int export_result = cplat_interprocess_rwlock_export_descriptor(
        rwlock, descriptor,
        &descriptor_size); // [手順] - rwlock と descriptor のバッファーを指定して cplat_interprocess_rwlock_export_descriptor() を呼び出す。
    int import_result = cplat_interprocess_lock_import_descriptor(
        descriptor, descriptor_size,
        &lock); // [手順] - rwlock の descriptor を指定して cplat_interprocess_lock_import_descriptor() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              open_result); // [確認_正常系] - cplat_interprocess_rwlock_open() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        export_result); // [確認_正常系] - cplat_interprocess_rwlock_export_descriptor() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_CORRUPT_DESCRIPTOR,
        import_result); // [確認_異常系] - 種別の異なる descriptor に対する cplat_interprocess_lock_import_descriptor() の戻り値が CPLAT_ERR_CORRUPT_DESCRIPTOR であること。
    EXPECT_EQ(nullptr, lock); // [確認_異常系] - lock が nullptr であること。

    // Cleanup
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(os.unistd, close(_, _, _, kFakeFd))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - close() が kFakeFd を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - close() から 0 を返す。
#endif
    cplat_interprocess_rwlock_dispose(rwlock);
#if defined(PLATFORM_WINDOWS)
    TEST_INTERPROCESS_UNLINK(path);
#endif
}

#if defined(PLATFORM_LINUX)

// 2 つ目のハンドルからの try_lock が BUSY になることの確認
TEST(syncInterprocessLockTest, second_handle_observes_exclusive_lock)
{
    // Arrange
    InterprocessOsMocks os;
    const char *path = kLockIdentity; // [状態] - 識別子を sync.lock とする。
    cplat_interprocess_lock *lock = nullptr;
    cplat_interprocess_lock *other = nullptr;

    // Pre-Assert
    EXPECT_CALL(os.fcntl, open(_, _, _, StrEq(path), O_RDWR | O_CREAT | O_CLOEXEC, 0666))
        .WillOnce(Return(kFakeFd))
        .WillOnce(Return(
            kFakeFd2)); // [Pre-Assert確認_正常系] - open() が path と等しい文字列、O_RDWR | O_CREAT | O_CLOEXEC、0666 を指定して 2 回呼び出されること。
                        // [Pre-Assert手順] - open() から 1 回目はセンチネル記述子 7、2 回目は 8 を返す。
    EXPECT_CALL(os.sys_file, flock(_, _, _, kFakeFd, LOCK_EX | LOCK_NB))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - flock() が kFakeFd、LOCK_EX | LOCK_NB を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - flock() から 0 を返す。
    EXPECT_CALL(os.sys_file, flock(_, _, _, kFakeFd2, LOCK_EX | LOCK_NB))
        .WillOnce(DoAll(
            Assign(&errno, EWOULDBLOCK),
            Return(
                -1))); // [Pre-Assert確認_正常系] - flock() が kFakeFd2、LOCK_EX | LOCK_NB を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - flock() から、errno に EWOULDBLOCK を設定して -1 を返す。

    // Act
    int open_result = cplat_interprocess_lock_open(
        path, &lock); // [手順] - 1 つ目として path を指定して cplat_interprocess_lock_open() を呼び出す。
    int other_open = cplat_interprocess_lock_open(
        path,
        &other); // [手順] - 同一識別子 path と 2 つ目の格納先を指定して cplat_interprocess_lock_open() を呼び出す。
    int lock_result = cplat_interprocess_lock_lock(
        lock,
        CPLAT_SYNC_NO_WAIT); // [手順] - 1 つ目のハンドルと CPLAT_SYNC_NO_WAIT を指定して cplat_interprocess_lock_lock() を呼び出す。
    int other_try = cplat_interprocess_lock_try_lock(
        other); // [手順] - 2 つ目のハンドルを指定して cplat_interprocess_lock_try_lock() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              open_result); // [確認_正常系] - 1 つ目の cplat_interprocess_lock_open() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              other_open); // [確認_正常系] - 2 つ目の cplat_interprocess_lock_open() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        lock_result); // [確認_正常系] - 1 つ目のハンドルに対する cplat_interprocess_lock_lock() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_BUSY,
        other_try); // [確認_正常系] - 2 つ目のハンドルに対する cplat_interprocess_lock_try_lock() の戻り値が CPLAT_ERR_BUSY であること。

    // Cleanup
    EXPECT_CALL(os.sys_file, flock(_, _, _, kFakeFd, LOCK_UN))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - flock() が kFakeFd、LOCK_UN を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - flock() から 0 を返す。
    EXPECT_CALL(os.unistd, close(_, _, _, kFakeFd))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - close() が kFakeFd を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - close() から 0 を返す。
    EXPECT_CALL(os.unistd, close(_, _, _, kFakeFd2))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - close() が kFakeFd2 を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - close() から 0 を返す。
    (void)cplat_interprocess_lock_unlock(lock);
    cplat_interprocess_lock_dispose(other);
    cplat_interprocess_lock_dispose(lock);
}

#endif /* PLATFORM_LINUX */
