#include "fileTestCommon.h"

#include <cstring>

class fileTest : public fileTestFixture
{
};

// 未オープンのハンドルに対する init と多重 close が安全であることの確認
// [サブ手順参照 名前=fileTestFixture.SetUp]
TEST_F(fileTest, init_and_close_are_safe_for_unopened_handle)
{
    // Arrange
    cplat_file file; // [状態] - 未オープンのファイル ハンドルを用意する。

    // Pre-Assert

    // Act
    cplat_file_init(&file);           // [手順] - cplat_file_init() で初期化する。
    cplat_file_close(&file, nullptr); // [手順] - 未オープンのまま cplat_file_close() を呼び出す。
    cplat_file_close(&file, nullptr); // [手順] - 続けてもう一度 cplat_file_close() を呼び出す。

    // Assert
    SUCCEED(); // [確認_正常系] - SUCCEED() に到達すること。(未オープンのハンドルに対する cplat_file_init() と多重の cplat_file_close() がクラッシュしないことの確認)
}

// 不正な引数で各関数が CPLAT_ERR_INVALID_ARGUMENT を返すことの確認
// [サブ手順参照 名前=fileTestFixture.SetUp]
TEST_F(fileTest, invalid_arguments_fail)
{
    // Arrange
    cplat_file file;
    size_t size = 0;

    cplat_file_init(&file); // [状態] - 未オープンのハンドルを初期化して用意する。

    // Pre-Assert

    // Act
    int actual_ret_open_file = cplat_file_open(nullptr, "x", CPLAT_FILE_OPEN_CREATE, nullptr);
    int actual_ret_open_path = cplat_file_open(&file, nullptr, CPLAT_FILE_OPEN_CREATE, nullptr);
    int actual_ret_write = cplat_file_write(&file, "abc", 3, nullptr);
    int actual_ret_get_size_closed = cplat_file_get_size(&file, &size, nullptr);
    int actual_ret_get_size_file = cplat_file_get_size(nullptr, &size, nullptr);
    int actual_ret_get_size_out = cplat_file_get_size(&file, nullptr, nullptr);

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_open_file); // [確認_異常系] - file が NULL の cplat_file_open() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_open_path); // [確認_異常系] - path が NULL の cplat_file_open() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_write); // [確認_異常系] - 未オープンの cplat_file_write() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_get_size_closed); // [確認_異常系] - 未オープンの cplat_file_get_size() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_get_size_file); // [確認_異常系] - file が NULL の cplat_file_get_size() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_get_size_out); // [確認_異常系] - size が NULL の cplat_file_get_size() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// 負のフラグでオープンすると CPLAT_ERR_INVALID_ARGUMENT を返すことの確認
// [サブ手順参照 名前=fileTestFixture.SetUp]
TEST_F(fileTest, open_rejects_negative_flags)
{
    // Arrange
    cplat_file file;
    cplat_error detail;

    cplat_file_init(&file); // [状態] - 未オープンのハンドルを用意する。

    // Pre-Assert

    // Act
    int result = cplat_file_open(&file, kPath, -1,
                                 &detail); // [手順] - 負のフラグを指定して cplat_file_open() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        result); // [確認_異常系] - 負のフラグに対する cplat_file_open() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        EINVAL,
        cplat_error_get_errno(
            &detail)); // [確認_異常系] - 負のフラグに対する cplat_error_get_errno() の戻り値が EINVAL であること。
}

// 同一性 ID 取得が不正な引数で CPLAT_ERR_INVALID_ARGUMENT を、存在しないパスで CPLAT_ERR_NOT_FOUND を返すことの確認
// [サブ手順参照 名前=fileTestFixture.SetUp]
TEST_F(fileTest, file_id_invalid_arguments_fail)
{
    // Arrange
    cplat_file file;
    cplat_file_id id;

    cplat_file_init(&file); // [状態] - 未オープンのハンドルを初期化して用意する。

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_sys_stat_, stat(_, _, _, StrEq("missing.dat"), _))
        .WillOnce(DoAll(
            Assign(&errno, ENOENT),
            Return(
                -1))); // [Pre-Assert確認_異常系] - stat() が "missing.dat" と等しい文字列を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - errno に ENOENT を設定し、-1 を返却する。
#endif                                /* PLATFORM_LINUX */

    // Act
    int actual_ret_get_id_closed = cplat_file_get_id(&file, &id, nullptr);
    int actual_ret_get_id_file = cplat_file_get_id(nullptr, &id, nullptr);
    int actual_ret_get_path_null = cplat_file_get_path_id(nullptr, &id, nullptr);
    int actual_ret_get_path_missing = cplat_file_get_path_id("missing.dat", &id, nullptr);
    int actual_ret_get_path_id = cplat_file_get_path_id("x", nullptr, nullptr);

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_get_id_closed); // [確認_異常系] - 未オープンの cplat_file_get_id() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_get_id_file); // [確認_異常系] - file が NULL の cplat_file_get_id() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_get_path_null); // [確認_異常系] - path が NULL の cplat_file_get_path_id() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_NOT_FOUND,
        actual_ret_get_path_missing); // [確認_異常系] - 存在しないパスの cplat_file_get_path_id() の戻り値が CPLAT_ERR_NOT_FOUND であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_get_path_id); // [確認_異常系] - id が NULL の cplat_file_get_path_id() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// CREATE_NEW を CREATE なしで指定すると CPLAT_ERR_INVALID_ARGUMENT を返すことの確認
// [サブ手順参照 名前=fileTestFixture.SetUp]
TEST_F(fileTest, create_new_without_create_fails)
{
    // Arrange
    cplat_file file;

    cplat_file_init(&file); // [状態] - 未オープンのハンドルを用意する。

    // Pre-Assert

    // Act
    int actual_ret_file_open =
        cplat_file_open(&file, kPath, CPLAT_FILE_OPEN_CREATE_NEW | CPLAT_FILE_OPEN_READ | CPLAT_FILE_OPEN_WRITE,
                        nullptr); // [手順] - CREATE を指定せず CREATE_NEW | READ | WRITE でオープンを試みる。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_file_open); // [確認_異常系] - CREATE なしの CREATE_NEW に対する cplat_file_open() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// cplat_file_set_size が不正な引数で CPLAT_ERR_INVALID_ARGUMENT を返すことの確認
// [サブ手順参照 名前=fileTestFixture.SetUp]
TEST_F(fileTest, set_size_invalid_arguments_fail)
{
    // Arrange
    cplat_file file;

    cplat_file_init(&file); // [状態] - 未オープンのハンドルを初期化して用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_file_set_size(&file, 16, nullptr);

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret); // [確認_異常系] - 未オープンの cplat_file_set_size() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// cplat_file_read が不正な引数で CPLAT_ERR_INVALID_ARGUMENT を返すことの確認
// [サブ手順参照 名前=fileTestFixture.SetUp]
TEST_F(fileTest, read_invalid_arguments_fail)
{
    // Arrange
    cplat_file file;
    char buf[16] = {0};
    size_t read_len = 0;

    cplat_file_init(&file); // [状態] - 未オープンのハンドルを初期化して用意する。

    // Pre-Assert

    // Act
    int actual_ret_not_open =
        cplat_file_read(&file, buf, sizeof(buf), &read_len, nullptr); // [手順] - 未オープンのハンドルで読み取りを行う。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_not_open); // [確認_異常系] - 未オープンのハンドルを渡した cplat_file_read() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

#if defined(PLATFORM_LINUX)

// 追記オープンで既存サイズが fstat から報告されることの確認
// [サブ手順参照 名前=fileTestFixture.SetUp]
TEST_F(fileTest, append_open_reports_existing_size)
{
    // Arrange
    cplat_file file;
    size_t size = 0;

    cplat_file_init(&file); // [状態] - 未オープンのハンドルを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_fcntl_, open(_, _, _, StrEq(kPath), O_WRONLY | O_CREAT | O_APPEND | kWriteThroughFlag, 0644))
        .WillOnce(Return(
            kFakeFd)); // [Pre-Assert確認_正常系] - open() が kPath と等しい文字列、O_WRONLY | O_CREAT | O_APPEND | kWriteThroughFlag、0644 を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - センチネル記述子 7 を返却する。
    EXPECT_CALL(mock_sys_stat_, fstat(_, _, _, kFakeFd, _))
        .WillOnce(
            [](const char *, int, const char *, int, struct stat *st)
            {
                fill_stat(st, 5, 1, 1);
                return 0;
            }); // [Pre-Assert確認_正常系] - fstat() が kFakeFd を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - サイズ 5 を設定する。

    // Act
    int actual_ret_file_open =
        cplat_file_open(&file, kPath, CPLAT_FILE_OPEN_CREATE | CPLAT_FILE_OPEN_APPEND | CPLAT_FILE_OPEN_WRITE_THROUGH,
                        nullptr); // [手順] - CREATE | APPEND | WRITE_THROUGH でオープンする。
    int actual_ret_file_get_size =
        cplat_file_get_size(&file, &size, nullptr); // [手順] - cplat_file_get_size() でサイズを取得する。

    // Assert
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_file_open); // [確認_正常系] - CREATE | APPEND | WRITE_THROUGH の cplat_file_open() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK,
              actual_ret_file_get_size); // [確認_正常系] - cplat_file_get_size() の戻り値が CPLAT_OK であること。
    EXPECT_EQ((size_t)5,
              size); // [確認_正常系] - size が 5 であること。(fstat が設定したサイズ 5 が報告されたことの確認)

    // Cleanup
    (void)cplat_file_close(&file, nullptr);
}

// TRUNCATE 付きオープンが O_TRUNC を渡し、サイズ 0 を報告することの確認
// [サブ手順参照 名前=fileTestFixture.SetUp]
TEST_F(fileTest, truncate_open_resets_existing_file_size)
{
    // Arrange
    cplat_file file;
    size_t size = 99; // [状態] - サイズの受け取り先を初期値 99 とする。

    cplat_file_init(&file);

    // Pre-Assert
    EXPECT_CALL(mock_fcntl_,
                open(_, _, _, StrEq(kPath), O_WRONLY | O_CREAT | O_TRUNC | O_APPEND | kWriteThroughFlag, 0644))
        .WillOnce(Return(
            kFakeFd)); // [Pre-Assert確認_正常系] - open() が kPath と等しい文字列、O_WRONLY | O_CREAT | O_TRUNC | O_APPEND | kWriteThroughFlag、0644 を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - センチネル記述子 7 を返却する。
    EXPECT_CALL(mock_sys_stat_, fstat(_, _, _, kFakeFd, _))
        .WillOnce(
            [](const char *, int, const char *, int, struct stat *st)
            {
                fill_stat(st, 0, 1, 1);
                return 0;
            }); // [Pre-Assert確認_正常系] - fstat() が kFakeFd を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - サイズ 0 を設定する。

    // Act
    int actual_ret_file_open = cplat_file_open(&file, kPath,
                                               CPLAT_FILE_OPEN_CREATE | CPLAT_FILE_OPEN_TRUNCATE |
                                                   CPLAT_FILE_OPEN_APPEND | CPLAT_FILE_OPEN_WRITE_THROUGH,
                                               nullptr); // [手順] - TRUNCATE を含むフラグでオープンする。
    int actual_ret_file_get_size =
        cplat_file_get_size(&file, &size, nullptr); // [手順] - cplat_file_get_size() でサイズを取得する。

    // Assert
    ASSERT_EQ(CPLAT_OK,
              actual_ret_file_open); // [確認_正常系] - TRUNCATE 付き cplat_file_open() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK,
              actual_ret_file_get_size); // [確認_正常系] - cplat_file_get_size() の戻り値が CPLAT_OK であること。
    EXPECT_EQ((size_t)0,
              size); // [確認_正常系] - size が 0 であること。(fstat が設定したサイズ 0 が報告されたことの確認)

    // Cleanup
    (void)cplat_file_close(&file, nullptr);
}

// 書き込み後の再オープンで追記 write が呼ばれることの確認 (マルチ フェーズ テスト)
// [サブ手順参照 名前=fileTestFixture.SetUp]
TEST_F(fileTest, write_then_reopen_appends)
{
    // Arrange
    cplat_file file;
    size_t size = 0;

    cplat_file_init(&file); // [状態] - 未オープンのハンドルを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_fcntl_,
                open(_, _, _, StrEq(kPath), O_WRONLY | O_CREAT | O_TRUNC | O_APPEND | kWriteThroughFlag, 0644))
        .WillOnce(Return(kFakeFd));
    // [Pre-Assert確認_正常系] - open() が kPath と等しい文字列、O_WRONLY | O_CREAT | O_TRUNC | O_APPEND | kWriteThroughFlag、0644 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - open() にて kFakeFd を返す。
    EXPECT_CALL(mock_unistd_, write(_, _, _, kFakeFd, _, 3u)).WillOnce(Return(3));
    // [Pre-Assert確認_正常系] - write() が kFakeFd、3u を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - write() にて 3 を返す。
    EXPECT_CALL(mock_sys_stat_, fstat(_, _, _, kFakeFd, _))
        .WillOnce(
            [](const char *, int, const char *, int, struct stat *st)
            {
                fill_stat(st, 3, 1, 1);
                return 0;
            }); // [Pre-Assert確認_正常系] - fstat() が kFakeFd を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - センチネル記述子 7 と書き込み長 3、サイズ 3 を返却する。

    // Act
    int actual_ret_file_open = cplat_file_open(&file, kPath,
                                               CPLAT_FILE_OPEN_CREATE | CPLAT_FILE_OPEN_TRUNCATE |
                                                   CPLAT_FILE_OPEN_APPEND | CPLAT_FILE_OPEN_WRITE_THROUGH,
                                               nullptr);                    // [手順] - 新規作成でオープンする。
    int actual_ret_file_write = cplat_file_write(&file, "abc", 3, nullptr); // [手順] - "abc" 3 バイトを書き込む。
    int actual_ret_file_get_size = cplat_file_get_size(&file, &size, nullptr);

    // Assert
    ASSERT_EQ(CPLAT_OK,
              actual_ret_file_open); // [確認_正常系] - 新規作成の cplat_file_open() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_file_write); // [確認_正常系] - "abc" を渡した cplat_file_write() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_file_get_size); // [確認_正常系] - 書き込み後の cplat_file_get_size() の戻り値が CPLAT_OK であること。
    EXPECT_EQ((size_t)3,
              size); // [確認_正常系] - size が 3 であること。(fstat が設定したサイズ 3 が報告されたことの確認)

    // Pre-Assert_2
    EXPECT_CALL(mock_fcntl_, open(_, _, _, StrEq(kPath), O_WRONLY | O_CREAT | O_APPEND | kWriteThroughFlag, 0644))
        .WillOnce(Return(kFakeFd));
    // [Pre-Assert確認_正常系] - open() が kPath と等しい文字列、O_WRONLY | O_CREAT | O_APPEND | kWriteThroughFlag、0644 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - open() にて kFakeFd を返す。
    EXPECT_CALL(mock_unistd_, write(_, _, _, kFakeFd, _, 3u))
        .WillOnce(Return(3)); // [Pre-Assert確認_正常系] - write() が kFakeFd、3u を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - センチネル記述子 7 と書き込み長 3 を返却する。

    // Act_2
    (void)cplat_file_close(&file, nullptr);
    int actual_ret_file_open_2 =
        cplat_file_open(&file, kPath, CPLAT_FILE_OPEN_CREATE | CPLAT_FILE_OPEN_APPEND | CPLAT_FILE_OPEN_WRITE_THROUGH,
                        nullptr); // [手順] - クローズ後に追記モードで再オープンする。
    int actual_ret_file_write_2 = cplat_file_write(&file, "def", 3, nullptr); // [手順] - "def" 3 バイトを追記する。

    // Assert_2
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_file_open_2); // [確認_正常系] - 再オープンの cplat_file_open() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_file_write_2); // [確認_正常系] - "def" を渡した cplat_file_write() の戻り値が CPLAT_OK であること。

    // Cleanup
    (void)cplat_file_close(&file, nullptr);
}

// ハンドル由来とパス由来のファイル同一性 ID が一致することの確認
// [サブ手順参照 名前=fileTestFixture.SetUp]
TEST_F(fileTest, file_id_matches_between_handle_and_path)
{
    // Arrange
    cplat_file file;
    cplat_file_id handle_id;
    cplat_file_id path_id;

    cplat_file_init(&file); // [状態] - 未オープンのハンドルを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_sys_stat_, fstat(_, _, _, kFakeFd, _))
        .WillOnce(
            [](const char *, int, const char *, int, struct stat *st)
            {
                fill_stat(st, 0, 11, 22);
                return 0;
            });
    // [Pre-Assert確認_正常系] - fstat() が kFakeFd を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - fstat() にて volume 11、index 22 を設定し、0 を返す。
    EXPECT_CALL(mock_sys_stat_, stat(_, _, _, StrEq(kPath), _))
        .WillOnce(
            [](const char *, int, const char *, const char *, struct stat *st)
            {
                fill_stat(st, 0, 11, 22);
                return 0;
            }); // [Pre-Assert確認_正常系] - stat() が kPath と等しい文字列を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - volume 11、index 22 を設定する。

    // Act
    int actual_ret_file_open =
        cplat_file_open(&file, kPath, CPLAT_FILE_OPEN_CREATE | CPLAT_FILE_OPEN_APPEND | CPLAT_FILE_OPEN_WRITE_THROUGH,
                        nullptr);              // [手順] - オープンする。
    ASSERT_EQ(CPLAT_OK, actual_ret_file_open); // [状態確認] - cplat_file_open() の戻り値が CPLAT_OK であること。
    int actual_ret_file_get_id =
        cplat_file_get_id(&file, &handle_id, nullptr); // [手順] - ハンドルから同一性 ID を取得する。
    int actual_ret_file_get_path_id =
        cplat_file_get_path_id(kPath, &path_id, nullptr); // [手順] - パスから同一性 ID を取得する。

    // Assert
    ASSERT_EQ(CPLAT_OK,
              actual_ret_file_get_id); // [確認_正常系] - cplat_file_get_id() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK,
              actual_ret_file_get_path_id); // [確認_正常系] - cplat_file_get_path_id() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(handle_id.volume, path_id.volume); // [確認_正常系] - path_id.volume が handle_id.volume と一致すること。
    EXPECT_EQ(handle_id.index, path_id.index);   // [確認_正常系] - path_id.index が handle_id.index と一致すること。

    // Cleanup
    (void)cplat_file_close(&file, nullptr);
}

// パス側の stat が別実体を返したとき同一性 ID が一致しないことの確認
// [サブ手順参照 名前=fileTestFixture.SetUp]
TEST_F(fileTest, file_id_differs_when_path_stat_differs)
{
    // Arrange
    cplat_file file;
    cplat_file_id handle_id;
    cplat_file_id path_id;

    cplat_file_init(&file); // [状態] - 未オープンのハンドルを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_sys_stat_, fstat(_, _, _, kFakeFd, _))
        .WillOnce(
            [](const char *, int, const char *, int, struct stat *st)
            {
                fill_stat(st, 0, 11, 22);
                return 0;
            });
    // [Pre-Assert確認_正常系] - fstat() が kFakeFd を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - fstat() にて volume 11、index 22 を設定し、0 を返す。
    EXPECT_CALL(mock_sys_stat_, stat(_, _, _, StrEq(kPath), _))
        .WillOnce(
            [](const char *, int, const char *, const char *, struct stat *st)
            {
                fill_stat(st, 0, 11, 33);
                return 0;
            }); // [Pre-Assert確認_正常系] - stat() が kPath と等しい文字列を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - ハンドル側とパス側で異なる index を設定する。

    // Act
    int actual_ret_file_open =
        cplat_file_open(&file, kPath, CPLAT_FILE_OPEN_CREATE | CPLAT_FILE_OPEN_APPEND | CPLAT_FILE_OPEN_WRITE_THROUGH,
                        nullptr);              // [手順] - オープンする。
    ASSERT_EQ(CPLAT_OK, actual_ret_file_open); // [状態確認] - cplat_file_open() の戻り値が CPLAT_OK であること。
    int actual_ret_file_get_id =
        cplat_file_get_id(&file, &handle_id, nullptr); // [手順] - ハンドルから同一性 ID を取得する。
    int actual_ret_file_get_path_id =
        cplat_file_get_path_id(kPath, &path_id, nullptr); // [手順] - パスから同一性 ID を取得する。

    // Assert
    ASSERT_EQ(CPLAT_OK,
              actual_ret_file_get_id); // [確認_正常系] - cplat_file_get_id() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK,
              actual_ret_file_get_path_id); // [確認_正常系] - cplat_file_get_path_id() の戻り値が CPLAT_OK であること。
    EXPECT_FALSE(
        handle_id.volume == path_id.volume &&
        handle_id.index ==
            path_id
                .index); // [確認_正常系] - handle_id.volume == path_id.volume && handle_id.index == path_id.index が false であること。(volume と index の組が一致しないことの確認)

    // Cleanup
    (void)cplat_file_close(&file, nullptr);
}

// READ/WRITE フラグを指定しない場合に、既定で書き込み専用としてオープンすることの確認
// [サブ手順参照 名前=fileTestFixture.SetUp]
TEST_F(fileTest, default_access_remains_write_only)
{
    // Arrange
    cplat_file file;

    cplat_file_init(&file); // [状態] - 未オープンのハンドルを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_fcntl_, open(_, _, _, StrEq(kPath), O_WRONLY | O_CREAT, 0644))
        .WillOnce(Return(
            kFakeFd)); // [Pre-Assert確認_正常系] - open() が kPath と等しい文字列、O_WRONLY | O_CREAT、0644 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - センチネル記述子 7 を返却する。
    EXPECT_CALL(mock_unistd_, write(_, _, _, kFakeFd, _, 3u))
        .WillOnce(Return(3)); // [Pre-Assert確認_正常系] - write() が kFakeFd、3u を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - 書き込み長 3 を返却する。

    // Act
    int actual_ret_file_open = cplat_file_open(&file, kPath, CPLAT_FILE_OPEN_CREATE,
                                               nullptr); // [手順] - READ/WRITE を指定せず CREATE のみでオープンする。
    int actual_ret_file_write = cplat_file_write(&file, "abc", 3, nullptr); // [手順] - "abc" 3 バイトを書き込む。

    // Assert
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_file_open); // [確認_正常系] - READ/WRITE 無指定の cplat_file_open() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_file_write); // [確認_正常系] - 既定 (書き込み専用) の cplat_file_write() の戻り値が CPLAT_OK であること。

    // Cleanup
    (void)cplat_file_close(&file, nullptr);
}

// CPLAT_FILE_OPEN_WRITE のみを指定した場合に書き込み可能であることの確認
// [サブ手順参照 名前=fileTestFixture.SetUp]
TEST_F(fileTest, explicit_write_only_open_allows_write)
{
    // Arrange
    cplat_file file;

    cplat_file_init(&file); // [状態] - 未オープンのハンドルを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_fcntl_, open(_, _, _, StrEq(kPath), O_WRONLY | O_CREAT, 0644))
        .WillOnce(Return(
            kFakeFd)); // [Pre-Assert確認_正常系] - open() が kPath と等しい文字列、O_WRONLY | O_CREAT、0644 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - センチネル記述子 7 を返却する。
    EXPECT_CALL(mock_unistd_, write(_, _, _, kFakeFd, _, 3u))
        .WillOnce(Return(3)); // [Pre-Assert確認_正常系] - write() が kFakeFd、3u を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - 書き込み長 3 を返却する。

    // Act
    int open_result = cplat_file_open(&file, kPath, CPLAT_FILE_OPEN_CREATE | CPLAT_FILE_OPEN_WRITE,
                                      nullptr); // [手順] - CREATE | WRITE でオープンする。
    int write_result =
        cplat_file_write(&file, "abc", 3u, nullptr); // [手順] - オープンしたファイルへ 3 バイトを書き込む。

    // Assert
    ASSERT_EQ(CPLAT_OK,
              open_result); // [確認_正常系] - CREATE | WRITE の cplat_file_open() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        write_result); // [確認_正常系] - WRITE のみでオープンした cplat_file_write() の戻り値が CPLAT_OK であること。

    // Cleanup
    (void)cplat_file_close(&file, nullptr);
}

// CPLAT_FILE_OPEN_READ のみを指定した場合に書き込みが失敗することの確認
// [サブ手順参照 名前=fileTestFixture.SetUp]
TEST_F(fileTest, read_only_open_rejects_write)
{
    // Arrange
    cplat_file file;

    cplat_file_init(&file); // [状態] - 未オープンのハンドルを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_fcntl_, open(_, _, _, StrEq(kPath), O_RDONLY, 0644))
        .WillOnce(Return(
            kFakeFd)); // [Pre-Assert確認_正常系] - open() が kPath と等しい文字列、O_RDONLY、0644 を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - センチネル記述子 7 を返却する。
    EXPECT_CALL(mock_unistd_, write(_, _, _, _, _, _))
        .Times(
            0); // [Pre-Assert確認_異常系] - write() が呼び出されないこと。(読み取り専用でオープンした後は write() へ進まないことの確認)

    // Act
    int actual_ret_file_open = cplat_file_open(&file, kPath, CPLAT_FILE_OPEN_READ,
                                               nullptr); // [手順] - CPLAT_FILE_OPEN_READ のみでオープンする。
    int actual_ret_file_write = cplat_file_write(&file, "x", 1, nullptr); // [手順] - 1 バイトの書き込みを試みる。

    // Assert
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_file_open); // [確認_正常系] - 読み取り専用の cplat_file_open() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_PERMISSION_DENIED,
        actual_ret_file_write); // [確認_異常系] - 読み取り専用ハンドルへの cplat_file_write() の戻り値が CPLAT_ERR_PERMISSION_DENIED であること。

    // Cleanup
    (void)cplat_file_close(&file, nullptr);
}

// CPLAT_FILE_OPEN_READ が存在しないファイルに対して失敗することの確認
// [サブ手順参照 名前=fileTestFixture.SetUp]
TEST_F(fileTest, read_only_open_fails_for_missing_file)
{
    // Arrange
    cplat_file file;

    cplat_file_init(&file); // [状態] - 未オープンのハンドルを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_fcntl_, open(_, _, _, StrEq(kPath), O_RDONLY, 0644))
        .WillOnce(DoAll(
            Assign(&errno, ENOENT),
            Return(
                -1))); // [Pre-Assert確認_異常系] - open() が kPath と等しい文字列、O_RDONLY、0644 を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - errno に ENOENT を設定し、-1 を返却する。

    // Act
    int actual_ret_file_open = cplat_file_open(&file, kPath, CPLAT_FILE_OPEN_READ,
                                               nullptr); // [手順] - CREATE を伴わずに READ のみでオープンを試みる。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_NOT_FOUND,
        actual_ret_file_open); // [確認_異常系] - 存在しないファイルに対する cplat_file_open() の戻り値が CPLAT_ERR_NOT_FOUND であること。
}

// READ | WRITE を指定した場合に読み書き両用でオープンできることの確認
// [サブ手順参照 名前=fileTestFixture.SetUp]
TEST_F(fileTest, read_write_open_allows_write_and_reports_size)
{
    // Arrange
    cplat_file file;
    size_t size = 0;

    cplat_file_init(&file); // [状態] - 未オープンのハンドルを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_fcntl_, open(_, _, _, StrEq(kPath), O_RDWR | O_CREAT, 0644)).WillOnce(Return(kFakeFd));
    // [Pre-Assert確認_正常系] - open() が kPath と等しい文字列、O_RDWR | O_CREAT、0644 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - open() にて kFakeFd を返す。
    EXPECT_CALL(mock_unistd_, write(_, _, _, kFakeFd, _, 5u)).WillOnce(Return(5));
    // [Pre-Assert確認_正常系] - write() が kFakeFd、5u を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - write() にて 5 を返す。
    EXPECT_CALL(mock_sys_stat_, fstat(_, _, _, kFakeFd, _))
        .WillOnce(
            [](const char *, int, const char *, int, struct stat *st)
            {
                fill_stat(st, 5, 1, 1);
                return 0;
            }); // [Pre-Assert確認_正常系] - fstat() が kFakeFd を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - センチネル記述子 7 と書き込み長 5、サイズ 5 を返却する。

    // Act
    int actual_ret_file_open =
        cplat_file_open(&file, kPath, CPLAT_FILE_OPEN_CREATE | CPLAT_FILE_OPEN_READ | CPLAT_FILE_OPEN_WRITE,
                        nullptr); // [手順] - CREATE | READ | WRITE でオープンする。
    int actual_ret_file_write = cplat_file_write(&file, "abcde", 5, nullptr);  // [手順] - "abcde" 5 バイトを書き込む。
    int actual_ret_file_get_size = cplat_file_get_size(&file, &size, nullptr); // [手順] - サイズを取得する。

    // Assert
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_file_open); // [確認_正常系] - CREATE | READ | WRITE の cplat_file_open() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_file_write); // [確認_正常系] - 読み書き両用ハンドルへの cplat_file_write() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK,
              actual_ret_file_get_size); // [確認_正常系] - cplat_file_get_size() の戻り値が CPLAT_OK であること。
    EXPECT_EQ((size_t)5,
              size); // [確認_正常系] - size が 5 であること。(fstat が設定したサイズ 5 が報告されたことの確認)

    // Cleanup
    (void)cplat_file_close(&file, nullptr);
}

// CREATE | CREATE_NEW で新規ファイルの作成に成功することの確認
// [サブ手順参照 名前=fileTestFixture.SetUp]
TEST_F(fileTest, create_new_succeeds_for_absent_file)
{
    // Arrange
    cplat_file file;

    cplat_file_init(&file); // [状態] - 未オープンのハンドルを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_fcntl_, open(_, _, _, StrEq(kPath), O_RDWR | O_CREAT | O_EXCL, 0644))
        .WillOnce(Return(
            kFakeFd)); // [Pre-Assert確認_正常系] - open() が kPath と等しい文字列、O_RDWR | O_CREAT | O_EXCL、0644 を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - センチネル記述子 7 を返却する。

    // Act
    int actual_ret_file_open = cplat_file_open(&file, kPath,
                                               CPLAT_FILE_OPEN_CREATE | CPLAT_FILE_OPEN_CREATE_NEW |
                                                   CPLAT_FILE_OPEN_READ | CPLAT_FILE_OPEN_WRITE,
                                               nullptr); // [手順] - CREATE | CREATE_NEW | READ | WRITE でオープンする。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_file_open); // [確認_正常系] - 存在しないファイルへの CREATE_NEW の cplat_file_open() の戻り値が CPLAT_OK であること。

    // Cleanup
    (void)cplat_file_close(&file, nullptr);
}

// CREATE | CREATE_NEW が既存ファイルに対して失敗することの確認
// [サブ手順参照 名前=fileTestFixture.SetUp]
TEST_F(fileTest, create_new_fails_for_existing_file)
{
    // Arrange
    cplat_file file;

    cplat_file_init(&file); // [状態] - 未オープンのハンドルを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_fcntl_, open(_, _, _, StrEq(kPath), O_RDWR | O_CREAT | O_EXCL, 0644))
        .WillOnce(DoAll(
            Assign(&errno, EEXIST),
            Return(
                -1))); // [Pre-Assert確認_異常系] - open() が kPath と等しい文字列、O_RDWR | O_CREAT | O_EXCL、0644 を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - errno に EEXIST を設定し、-1 を返却する。

    // Act
    int actual_ret_file_open = cplat_file_open(
        &file, kPath,
        CPLAT_FILE_OPEN_CREATE | CPLAT_FILE_OPEN_CREATE_NEW | CPLAT_FILE_OPEN_READ | CPLAT_FILE_OPEN_WRITE,
        nullptr); // [手順] - 既存ファイルに対して CREATE | CREATE_NEW でオープンを試みる。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_file_open); // [確認_異常系] - 既存ファイルに対する CREATE_NEW の cplat_file_open() の戻り値が CPLAT_ERR_UNKNOWN であること。
}

// cplat_file_set_size が ftruncate に拡張と縮小のサイズを渡すことの確認 (マルチ フェーズ テスト)
// [サブ手順参照 名前=fileTestFixture.SetUp]
TEST_F(fileTest, set_size_extends_and_truncates_file)
{
    // Arrange
    cplat_file file;
    size_t size = 0;

    cplat_file_init(&file);
    ASSERT_EQ(CPLAT_OK,
              cplat_file_open(&file, kPath, CPLAT_FILE_OPEN_CREATE | CPLAT_FILE_OPEN_READ | CPLAT_FILE_OPEN_WRITE,
                              nullptr)); // [状態] - CREATE | READ | WRITE でオープンする。
                                         // [状態確認] - cplat_file_open() の戻り値が CPLAT_OK であること。

    // Pre-Assert
    EXPECT_CALL(mock_unistd_, ftruncate(_, _, _, kFakeFd, 128)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - ftruncate() が kFakeFd、128 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - ftruncate() にて 0 を返す。
    EXPECT_CALL(mock_sys_stat_, fstat(_, _, _, kFakeFd, _))
        .WillOnce(
            [](const char *, int, const char *, int, struct stat *st)
            {
                fill_stat(st, 128, 1, 1);
                return 0;
            }); // [Pre-Assert確認_正常系] - fstat() が kFakeFd を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - ftruncate() は 0、fstat() はサイズ 128 を返却する。

    // Act
    int actual_ret_set_size_1 = cplat_file_set_size(&file, 128, nullptr); // [手順] - サイズを 128 バイトへ拡張する。
    int actual_ret_get_size_1 = cplat_file_get_size(&file, &size, nullptr);

    // Assert
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_set_size_1); // [確認_正常系] - 128 バイトへ拡張する cplat_file_set_size() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, actual_ret_get_size_1);
    // [確認_正常系] - 1 回目の cplat_file_get_size() の戻り値が CPLAT_OK であること。
    EXPECT_EQ((size_t)128,
              size); // [確認_正常系] - size が 128 であること。(fstat が設定したサイズ 128 が報告されたことの確認)

    // Pre-Assert_2
    EXPECT_CALL(mock_unistd_, ftruncate(_, _, _, kFakeFd, 16)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - ftruncate() が kFakeFd、16 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - ftruncate() にて 0 を返す。
    EXPECT_CALL(mock_sys_stat_, fstat(_, _, _, kFakeFd, _))
        .WillOnce(
            [](const char *, int, const char *, int, struct stat *st)
            {
                fill_stat(st, 16, 1, 1);
                return 0;
            }); // [Pre-Assert確認_正常系] - fstat() が kFakeFd を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - ftruncate() は 0、fstat() はサイズ 16 を返却する。

    // Act_2
    int actual_ret_set_size_2 = cplat_file_set_size(&file, 16, nullptr); // [手順] - サイズを 16 バイトへ縮小する。
    int actual_ret_get_size_2 = cplat_file_get_size(&file, &size, nullptr);

    // Assert_2
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_set_size_2); // [確認_正常系] - 16 バイトへ縮小する cplat_file_set_size() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, actual_ret_get_size_2);
    // [確認_正常系] - 2 回目の cplat_file_get_size() の戻り値が CPLAT_OK であること。
    EXPECT_EQ((size_t)16,
              size); // [確認_正常系] - size が 16 であること。(fstat が設定したサイズ 16 が報告されたことの確認)

    // Cleanup
    (void)cplat_file_close(&file, nullptr);
}

// cplat_file_read が read の返却バッファーを呼び出し元へ渡すことの確認
// [サブ手順参照 名前=fileTestFixture.SetUp]
TEST_F(fileTest, read_returns_written_content)
{
    // Arrange
    cplat_file file;
    char buf[16] = {0};
    size_t read_len = 0;

    cplat_file_init(&file);
    ASSERT_EQ(CPLAT_OK, cplat_file_open(&file, kPath, CPLAT_FILE_OPEN_READ, nullptr)); // [状態] - READ でオープンする。
    // [状態確認] - cplat_file_open() の戻り値が CPLAT_OK であること。

    // Pre-Assert
    EXPECT_CALL(mock_unistd_, read(_, _, _, kFakeFd, _, sizeof(buf)))
        .WillOnce(
            [](const char *, int, const char *, int, void *out, size_t)
            {
                memcpy(out, "abcde", 6);
                return 5;
            }); // [Pre-Assert確認_正常系] - read() が kFakeFd、sizeof(buf) を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - "abcde" を書き込み、5 を返却する。

    // Act
    int actual_ret_read =
        cplat_file_read(&file, buf, sizeof(buf), &read_len, nullptr); // [手順] - 16 バイトを要求して読み取る。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_read); // [確認_正常系] - cplat_file_read() の戻り値が CPLAT_OK であること。
    EXPECT_EQ((size_t)5,
              read_len); // [確認_正常系] - read_len が 5 であること。(read() が返した長さ 5 が報告されたことの確認)
    EXPECT_STREQ("abcde", buf); // [確認_正常系] - buf が "abcde" であること。

    // Cleanup
    (void)cplat_file_close(&file, nullptr);
}

// cplat_file_read がファイル終端で 0 バイトを返すことの確認
// [サブ手順参照 名前=fileTestFixture.SetUp]
TEST_F(fileTest, read_at_end_of_file_returns_zero_length)
{
    // Arrange
    cplat_file file;
    char buf[16] = {0};
    size_t read_len = 0;

    cplat_file_init(&file);
    ASSERT_EQ(CPLAT_OK, cplat_file_open(&file, kPath, CPLAT_FILE_OPEN_READ, nullptr)); // [状態] - READ でオープンする。
    // [状態確認] - cplat_file_open() の戻り値が CPLAT_OK であること。

    // Pre-Assert
    EXPECT_CALL(mock_unistd_, read(_, _, _, kFakeFd, _, sizeof(buf)))
        .WillOnce(
            [](const char *, int, const char *, int, void *out, size_t)
            {
                memcpy(out, "ab", 3);
                return 2;
            })
        .WillOnce(
            Return(0)); // [Pre-Assert確認_正常系] - read() が kFakeFd、sizeof(buf) を指定して 2 回呼び出されること。
                        // [Pre-Assert手順] - 1 回目は 2 バイト、2 回目は 0 を返却する。

    // Act
    int actual_ret_read = cplat_file_read(&file, buf, sizeof(buf), &read_len, nullptr);
    size_t first_len = read_len;
    int actual_ret_read_eof =
        cplat_file_read(&file, buf, sizeof(buf), &read_len, nullptr); // [手順] - 終端到達後に再度読み取りを行う。

    // Assert
    ASSERT_EQ(CPLAT_OK, actual_ret_read); // [確認_正常系] - 1 回目の cplat_file_read() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(
        (size_t)2,
        first_len); // [確認_正常系] - first_len が 2 であること。(1 回目の read() が返した長さ 2 が報告されたことの確認)
    EXPECT_EQ(CPLAT_OK,
              actual_ret_read_eof); // [確認_正常系] - 終端到達後の cplat_file_read() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        (size_t)0,
        read_len); // [確認_正常系] - read_len が 0 であること。(ファイル終端で 2 回目の読み取りバイト数が 0 であることの確認)

    // Cleanup
    (void)cplat_file_close(&file, nullptr);
}

// cplat_file_flush が fsync 成功を結果コードへ反映することの確認
// [サブ手順参照 名前=fileTestFixture.SetUp]
TEST_F(fileTest, flush_reports_success)
{
    // Arrange
    cplat_file file;
    cplat_error detail;

    cplat_file_init(&file);
    ASSERT_EQ(CPLAT_OK, cplat_file_open(&file, kPath, CPLAT_FILE_OPEN_CREATE | CPLAT_FILE_OPEN_TRUNCATE,
                                        nullptr)); // [状態] - CREATE | TRUNCATE でオープンする。
    // [状態確認] - cplat_file_open() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_file_write(&file, "data", 4, nullptr)); // [状態] - "data" 4 バイトを書き込む。
    // [状態確認] - cplat_file_write() の戻り値が CPLAT_OK であること。

    // Pre-Assert
    EXPECT_CALL(mock_unistd_, fsync(_, _, _, kFakeFd))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - fsync() が kFakeFd を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - 0 を返却する。

    // Act
    int result = cplat_file_flush(&file, &detail); // [手順] - 書き込み済みハンドルを flush する。

    // Assert
    EXPECT_EQ(CPLAT_OK, result); // [確認_正常系] - cplat_file_flush() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_ERROR_DOMAIN_NONE,
              cplat_error_get_domain(
                  &detail)); // [確認_正常系] - cplat_error_get_domain() の戻り値が CPLAT_ERROR_DOMAIN_NONE であること。

    // Cleanup
    (void)cplat_file_close(&file, nullptr);
}

#endif /* PLATFORM_LINUX */
