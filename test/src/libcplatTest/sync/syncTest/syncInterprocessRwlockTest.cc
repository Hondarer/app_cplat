#include <testfw.h>

#include "syncTestHelper.h"

using testing::Assign;
using testing::DoAll;
#if defined(PLATFORM_LINUX)
using testing::StrEq;
#endif

// descriptor の出力と復元で同一の interprocess rwlock を再取得できることの確認
TEST(syncInterprocessRwlockTest, descriptor_round_trip_reopens_same_lock)
{
    // Arrange
#if defined(PLATFORM_LINUX)
    InterprocessOsMocks os;
    const char *path = kRwlockIdentity; // [状態] - 識別子を sync.rwlock とする。
#else
    char path_buf[256];
    make_test_interprocess_path(path_buf, sizeof(path_buf), "interprocess_rwlock");
    const char *path = path_buf; // [状態] - テスト用 lock file パスを用意する。
#endif
    cplat_interprocess_rwlock *lock = NULL;
    cplat_interprocess_rwlock *restored = NULL;
    unsigned char descriptor[512];
    size_t descriptor_size = sizeof(descriptor);

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(os.fcntl, open(_, _, _, StrEq(path), O_RDWR | O_CREAT | O_CLOEXEC, 0666))
        .WillOnce(Return(kFakeFd))
        .WillOnce(Return(
            kFakeFd2)); // [Pre-Assert確認_正常系] - open() が path と等しい文字列、O_RDWR | O_CREAT | O_CLOEXEC、0666 を指定して 2 回呼び出されること。
                        // [Pre-Assert手順] - 番兵記述子 7 と 8 を順に返却する。
    EXPECT_CALL(os.sys_file, flock(_, _, _, kFakeFd, LOCK_EX | LOCK_NB))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - flock() が kFakeFd、LOCK_EX | LOCK_NB を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - 0 を返却する。
    EXPECT_CALL(os.sys_file, flock(_, _, _, kFakeFd2, LOCK_SH | LOCK_NB))
        .WillOnce(DoAll(
            Assign(&errno, EWOULDBLOCK),
            Return(
                -1))); // [Pre-Assert確認_正常系] - flock() が kFakeFd2、LOCK_SH | LOCK_NB を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - errno に EWOULDBLOCK を設定し、-1 を返却する。
#endif

    // Act
    int create_result = cplat_interprocess_rwlock_open(path, &lock); // [手順] - interprocess rwlock を開く。
    int export_result = cplat_interprocess_rwlock_export_descriptor(
        lock, descriptor, &descriptor_size); // [手順] - descriptor をバッファーへ出力する。
    int import_result = cplat_interprocess_rwlock_import_descriptor(
        descriptor, descriptor_size, &restored); // [手順] - descriptor から interprocess rwlock を復元する。
    int exclusive_lock = cplat_interprocess_rwlock_lock_exclusive(
        lock, CPLAT_SYNC_NO_WAIT); // [手順] - 元ハンドルで排他ロックを取得する。
    int shared_try =
        cplat_interprocess_rwlock_try_lock_shared(restored); // [手順] - 復元ハンドルで共有ロック取得を試行する。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              create_result); // [確認_正常系] - cplat_interprocess_rwlock_open() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        export_result); // [確認_正常系] - cplat_interprocess_rwlock_export_descriptor() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        import_result); // [確認_正常系] - cplat_interprocess_rwlock_import_descriptor() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        exclusive_lock); // [確認_正常系] - 元ハンドルに対する cplat_interprocess_rwlock_lock_exclusive() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_BUSY,
        shared_try); // [確認_正常系] - 復元ハンドルに対する cplat_interprocess_rwlock_try_lock_shared() の戻り値が CPLAT_ERR_BUSY であること。(同一の排他インスタンスを参照することの確認)

    // Cleanup
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(os.sys_file, flock(_, _, _, kFakeFd, LOCK_UN))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - flock() が kFakeFd、LOCK_UN を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - 0 を返却する。
    EXPECT_CALL(os.unistd, close(_, _, _, kFakeFd))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - close() が kFakeFd を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - 0 を返却する。
    EXPECT_CALL(os.unistd, close(_, _, _, kFakeFd2))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - close() が kFakeFd2 を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - 0 を返却する。
#endif
    (void)cplat_interprocess_rwlock_unlock(lock);
    cplat_interprocess_rwlock_dispose(restored);
    cplat_interprocess_rwlock_dispose(lock);
#if defined(PLATFORM_WINDOWS)
    TEST_INTERPROCESS_UNLINK(path);
#endif
}

// NULL バッファーの export で必要な descriptor サイズが報告されることの確認
TEST(syncInterprocessRwlockTest, export_reports_required_descriptor_size)
{
    // Arrange
#if defined(PLATFORM_LINUX)
    InterprocessOsMocks os;
    const char *path = kRwlockIdentity; // [状態] - 識別子を sync.rwlock とする。
#else
    char path_buf[256];
    make_test_interprocess_path(path_buf, sizeof(path_buf), "interprocess_rwlock_size");
    const char *path = path_buf; // [状態] - テスト用 lock file パスを用意する。
#endif
    cplat_interprocess_rwlock *lock = NULL;
    size_t descriptor_size = 0U;
    const size_t expected_size = 3U + strlen(path); // [状態] - stub の必要サイズは種別・backend・長さと識別子である。

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(os.fcntl, open(_, _, _, StrEq(path), O_RDWR | O_CREAT | O_CLOEXEC, 0666))
        .WillOnce(Return(
            kFakeFd)); // [Pre-Assert確認_正常系] - open() が path と等しい文字列、O_RDWR | O_CREAT | O_CLOEXEC、0666 を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - 番兵記述子 7 を返却する。
#endif

    // Act
    int create_result = cplat_interprocess_rwlock_open(path, &lock); // [手順] - interprocess rwlock を開く。
    int export_result = cplat_interprocess_rwlock_export_descriptor(
        lock, NULL, &descriptor_size); // [手順] - NULL バッファーで必要サイズを問い合わせる。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              create_result); // [確認_正常系] - cplat_interprocess_rwlock_open() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_BUFFER_TOO_SMALL,
        export_result); // [確認_正常系] - NULL バッファーに対する cplat_interprocess_rwlock_export_descriptor() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_EQ(
        expected_size,
        descriptor_size); // [確認_正常系] - descriptor_size が expected_size と一致すること。(必要な descriptor サイズが報告されたことの確認)

    // Cleanup
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(os.unistd, close(_, _, _, kFakeFd))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - close() が kFakeFd を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - 0 を返却する。
#endif
    cplat_interprocess_rwlock_dispose(lock);
#if defined(PLATFORM_WINDOWS)
    TEST_INTERPROCESS_UNLINK(path);
#endif
}

// 不正な descriptor の import が拒否されることの確認
TEST(syncInterprocessRwlockTest, corrupt_descriptor_is_rejected)
{
    // Arrange
    unsigned char descriptor[20] = {0}; // [状態] - magic を持たない descriptor を用意する。
    cplat_interprocess_rwlock *lock = NULL;

    // Pre-Assert

    // Act
    int result = cplat_interprocess_rwlock_import_descriptor(descriptor, sizeof(descriptor),
                                                             &lock); // [手順] - 不正 descriptor を import する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_CORRUPT_DESCRIPTOR,
        result); // [確認_異常系] - 不正な descriptor に対する cplat_interprocess_rwlock_import_descriptor() の戻り値が CPLAT_ERR_CORRUPT_DESCRIPTOR であること。
    EXPECT_EQ(NULL, lock); // [確認_異常系] - lock が NULL であること。
}

#if defined(PLATFORM_LINUX)

// 2 つ目のハンドルからの共有 try_lock が BUSY になることの確認
TEST(syncInterprocessRwlockTest, second_handle_observes_exclusive_lock)
{
    // Arrange
    InterprocessOsMocks os;
    const char *path = kRwlockIdentity; // [状態] - 識別子を sync.rwlock とする。
    cplat_interprocess_rwlock *lock = NULL;
    cplat_interprocess_rwlock *other = NULL;

    // Pre-Assert
    EXPECT_CALL(os.fcntl, open(_, _, _, StrEq(path), O_RDWR | O_CREAT | O_CLOEXEC, 0666))
        .WillOnce(Return(kFakeFd))
        .WillOnce(Return(
            kFakeFd2)); // [Pre-Assert確認_正常系] - open() が path と等しい文字列、O_RDWR | O_CREAT | O_CLOEXEC、0666 を指定して 2 回呼び出されること。
                        // [Pre-Assert手順] - 番兵記述子 7 と 8 を順に返却する。
    EXPECT_CALL(os.sys_file, flock(_, _, _, kFakeFd, LOCK_EX | LOCK_NB))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - flock() が kFakeFd、LOCK_EX | LOCK_NB を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - 0 を返却する。
    EXPECT_CALL(os.sys_file, flock(_, _, _, kFakeFd2, LOCK_SH | LOCK_NB))
        .WillOnce(DoAll(
            Assign(&errno, EWOULDBLOCK),
            Return(
                -1))); // [Pre-Assert確認_正常系] - flock() が kFakeFd2、LOCK_SH | LOCK_NB を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - errno に EWOULDBLOCK を設定し、-1 を返却する。

    // Act
    int open_result = cplat_interprocess_rwlock_open(path, &lock); // [手順] - 1 つ目の interprocess rwlock を開く。
    int other_open = cplat_interprocess_rwlock_open(path, &other); // [手順] - 同一識別子でもう 1 つ開く。
    int lock_result = cplat_interprocess_rwlock_lock_exclusive(
        lock, CPLAT_SYNC_NO_WAIT); // [手順] - 1 つ目のハンドルで排他ロックを取得する。
    int other_try =
        cplat_interprocess_rwlock_try_lock_shared(other); // [手順] - 2 つ目のハンドルで共有 try_lock を試行する。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        open_result); // [確認_正常系] - 1 つ目の cplat_interprocess_rwlock_open() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              other_open); // [確認_正常系] - 2 つ目の cplat_interprocess_rwlock_open() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        lock_result); // [確認_正常系] - 1 つ目のハンドルに対する cplat_interprocess_rwlock_lock_exclusive() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_BUSY,
        other_try); // [確認_正常系] - 2 つ目のハンドルに対する cplat_interprocess_rwlock_try_lock_shared() の戻り値が CPLAT_ERR_BUSY であること。

    // Cleanup
    EXPECT_CALL(os.sys_file, flock(_, _, _, kFakeFd, LOCK_UN))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - flock() が kFakeFd、LOCK_UN を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - 0 を返却する。
    EXPECT_CALL(os.unistd, close(_, _, _, kFakeFd))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - close() が kFakeFd を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - 0 を返却する。
    EXPECT_CALL(os.unistd, close(_, _, _, kFakeFd2))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - close() が kFakeFd2 を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - 0 を返却する。
    (void)cplat_interprocess_rwlock_unlock(lock);
    cplat_interprocess_rwlock_dispose(other);
    cplat_interprocess_rwlock_dispose(lock);
}

#endif /* PLATFORM_LINUX */
