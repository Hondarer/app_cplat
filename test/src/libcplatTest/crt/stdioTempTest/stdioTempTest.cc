#include <testfw.h>
#include <mock_cplat.h>
#include <cplat/base/platform.h>
#include <cplat/crt/stdio.h>
#include <cerrno>
#include <cstring>

#if defined(PLATFORM_LINUX)
    #include <fcntl.h>
    #include <mock_stdio.h>
    #include <mock_unistd.h>
#elif defined(PLATFORM_WINDOWS)
    #include <cplat/crt/temp_dir_internal.h>
    #include <mock_windows.h>
#endif /* PLATFORM_ */

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::StrEq;

#if defined(PLATFORM_LINUX)

namespace
{
FILE *const kStream = reinterpret_cast<FILE *>(static_cast<uintptr_t>(0x70));
const int kFakeFd = 7;

int resolve_mkostemp(char *tmpl, const char *resolved)
{
    std::strcpy(tmpl, resolved);
    return kFakeFd;
}

int getenv_empty_tmpdir(const char *, char *buffer, size_t, int *, cplat_error *)
{
    buffer[0] = '\0';
    return CPLAT_OK;
}
} // namespace

class stdioTempTest : public testing::Test
{
  protected:
    NiceMock<Mock_unistd> mock_unistd_;
    NiceMock<Mock_stdio> mock_stdio_;
    NiceMock<Mock_cplat> mock_cplat_;

    // [サブ手順 名前=stdioTempTest.SetUp]
    void SetUp() override
    {
        ON_CALL(mock_cplat_, cplat_getenv(StrEq("TMPDIR"), _, _, _, _)).WillByDefault(getenv_empty_tmpdir);
        // [状態] - cplat_getenv() から、TMPDIR として空文字列を書き込んで CPLAT_OK を返すように既定動作を設定する。
        ON_CALL(mock_unistd_, close(_, _, _, _)).WillByDefault(Return(0));
        // [状態] - close() から 0 を返すように既定動作を設定する。
        ON_CALL(mock_unistd_, unlink(_, _, _, _)).WillByDefault(Return(0));
        // [状態] - unlink() から 0 を返すように既定動作を設定する。
    }
    // [サブ手順終了]
};

#else /* PLATFORM_LINUX */

class stdioTempTest : public testing::Test
{
};

#endif /* PLATFORM_LINUX */

#if defined(PLATFORM_LINUX)
// 一時ファイルが開き、mkostemp が書き込んだパスが報告されることの確認
// [サブ手順参照 名前=stdioTempTest.SetUp]
TEST_F(stdioTempTest, opens_writable_file_and_reports_path)
{
    // Arrange
    char path[PLATFORM_PATH_MAX] = {}; // [状態] - パスの受け取り先バッファーを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_unistd_, mkostemp(_, _, _, path, O_CLOEXEC))
        .WillOnce(
            [](const char *, int, const char *, char *tmpl, int)
            {
                return resolve_mkostemp(tmpl, "/tmp/ptrAAAAAA");
            }); // [Pre-Assert確認_正常系] - mkostemp() が path、O_CLOEXEC を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - mkostemp() から、パスを "/tmp/ptrAAAAAA" に書き換えてセンチネル記述子 7 を返す。
    EXPECT_CALL(mock_stdio_, fdopen(_, _, _, kFakeFd, StrEq("wb")))
        .WillOnce(Return(
            kStream)); // [Pre-Assert確認_正常系] - fdopen() が kFakeFd、"wb" と等しい文字列を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - fdopen() から、センチネル ストリームを返す。

    // Act
    FILE *fp = cplat_fopen_temp("ptr", "wb", path, sizeof(path),
                                nullptr); // [手順] - prefix "ptr"、モード "wb" で cplat_fopen_temp() を呼び出す。

    // Assert
    EXPECT_EQ(kStream, fp);               // [確認_正常系] - cplat_fopen_temp() の戻り値が kStream であること。
    EXPECT_STREQ("/tmp/ptrAAAAAA", path); // [確認_正常系] - path が "/tmp/ptrAAAAAA" であること。
}

// TMPDIR が空でない場合にそのディレクトリが使われることの確認
// [サブ手順参照 名前=stdioTempTest.SetUp]
TEST_F(stdioTempTest, uses_tmpdir_when_getenv_returns_a_path)
{
    // Arrange
    char path[PLATFORM_PATH_MAX] = {}; // [状態] - パスの受け取り先バッファーを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_getenv(StrEq("TMPDIR"), _, _, _, _))
        .WillOnce(
            [](const char *, char *buffer, size_t buffer_size, int *, cplat_error *)
            {
                std::strncpy(buffer, "/var/tmp", buffer_size);
                buffer[buffer_size - 1u] = '\0';
                return CPLAT_OK;
            }); // [Pre-Assert確認_正常系] - cplat_getenv() が "TMPDIR" と等しい文字列を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_getenv() から、"/var/tmp" を格納して CPLAT_OK を返す。
    EXPECT_CALL(mock_unistd_, mkostemp(_, _, _, path, O_CLOEXEC))
        .WillOnce(
            [](const char *, int, const char *, char *tmpl, int)
            {
                return resolve_mkostemp(tmpl, "/var/tmp/ptrAAAAAA");
            }); // [Pre-Assert確認_正常系] - mkostemp() が path、O_CLOEXEC を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - mkostemp() から、パスを "/var/tmp/ptrAAAAAA" に書き換えてセンチネル記述子 7 を返す。
    EXPECT_CALL(mock_stdio_, fdopen(_, _, _, kFakeFd, StrEq("wb")))
        .WillOnce(Return(
            kStream)); // [Pre-Assert確認_正常系] - fdopen() が kFakeFd、"wb" と等しい文字列を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - fdopen() から、センチネル ストリームを返す。

    // Act
    FILE *fp = cplat_fopen_temp("ptr", "wb", path, sizeof(path),
                                nullptr); // [手順] - TMPDIR が "/var/tmp" の状態で cplat_fopen_temp() を呼び出す。

    // Assert
    EXPECT_EQ(kStream, fp); // [確認_正常系] - cplat_fopen_temp() の戻り値が kStream であること。
    EXPECT_STREQ("/var/tmp/ptrAAAAAA",
                 path); // [確認_正常系] - path が "/var/tmp/ptrAAAAAA" であること。(TMPDIR 配下のパスであることの確認)
}

// 繰り返し呼び出しで毎回異なるパスが返ることの確認
// [サブ手順参照 名前=stdioTempTest.SetUp]
TEST_F(stdioTempTest, returns_unique_paths_for_repeated_calls)
{
    // Arrange
    char path1[PLATFORM_PATH_MAX] = {};
    char path2[PLATFORM_PATH_MAX] = {};
    char path3[PLATFORM_PATH_MAX] = {};
    char path4[PLATFORM_PATH_MAX] = {}; // [状態] - 4 回分のパス受け取り先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_unistd_, mkostemp(_, _, _, _, O_CLOEXEC))
        .WillOnce([](const char *, int, const char *, char *tmpl, int)
                  { return resolve_mkostemp(tmpl, "/tmp/ptrAAAAAA"); })
        .WillOnce([](const char *, int, const char *, char *tmpl, int)
                  { return resolve_mkostemp(tmpl, "/tmp/ptrBBBBBB"); })
        .WillOnce([](const char *, int, const char *, char *tmpl, int)
                  { return resolve_mkostemp(tmpl, "/tmp/ptrCCCCCC"); })
        .WillOnce(
            [](const char *, int, const char *, char *tmpl, int)
            {
                return resolve_mkostemp(tmpl, "/tmp/ptrDDDDDD");
            }); // [Pre-Assert確認_正常系] - mkostemp() が O_CLOEXEC を指定して 4 回呼び出されること。
    // [Pre-Assert手順] - mkostemp() から、互いに異なる 4 つのパスを順に書き込んでセンチネル記述子 7 を返す。
    EXPECT_CALL(mock_stdio_, fdopen(_, _, _, kFakeFd, StrEq("wb")))
        .Times(4)
        .WillRepeatedly(Return(
            kStream)); // [Pre-Assert確認_正常系] - fdopen() が kFakeFd、"wb" と等しい文字列を指定して 4 回呼び出されること。
                       // [Pre-Assert手順] - fdopen() から、センチネル ストリームを返す。

    // Act
    FILE *fp1 = cplat_fopen_temp("ptr", "wb", path1, sizeof(path1),
                                 nullptr); // [手順] - 1 回目の cplat_fopen_temp() を呼び出す。
    FILE *fp2 = cplat_fopen_temp("ptr", "wb", path2, sizeof(path2),
                                 nullptr); // [手順] - 2 回目の cplat_fopen_temp() を呼び出す。
    FILE *fp3 = cplat_fopen_temp("ptr", "wb", path3, sizeof(path3),
                                 nullptr); // [手順] - 3 回目の cplat_fopen_temp() を呼び出す。
    FILE *fp4 = cplat_fopen_temp("ptr", "wb", path4, sizeof(path4),
                                 nullptr); // [手順] - 4 回目の cplat_fopen_temp() を呼び出す。

    // Assert
    EXPECT_EQ(kStream, fp1); // [確認_正常系] - 1 回目の cplat_fopen_temp() の戻り値が kStream であること。
    EXPECT_EQ(kStream, fp2); // [確認_正常系] - 2 回目の cplat_fopen_temp() の戻り値が kStream であること。
    EXPECT_EQ(kStream, fp3); // [確認_正常系] - 3 回目の cplat_fopen_temp() の戻り値が kStream であること。
    EXPECT_EQ(kStream, fp4); // [確認_正常系] - 4 回目の cplat_fopen_temp() の戻り値が kStream であること。
    EXPECT_STREQ("/tmp/ptrAAAAAA", path1); // [確認_正常系] - path1 が "/tmp/ptrAAAAAA" であること。
    EXPECT_STREQ("/tmp/ptrBBBBBB", path2); // [確認_正常系] - path2 が "/tmp/ptrBBBBBB" であること。
    EXPECT_STREQ("/tmp/ptrCCCCCC", path3); // [確認_正常系] - path3 が "/tmp/ptrCCCCCC" であること。
    EXPECT_STREQ("/tmp/ptrDDDDDD", path4); // [確認_正常系] - path4 が "/tmp/ptrDDDDDD" であること。
}

// prefix がファイル名 (basename) に含まれることの確認
// [サブ手順参照 名前=stdioTempTest.SetUp]
TEST_F(stdioTempTest, prefix_is_part_of_basename)
{
    // Arrange
    char path[PLATFORM_PATH_MAX] = {}; // [状態] - パスの受け取り先バッファーを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_unistd_, mkostemp(_, _, _, path, O_CLOEXEC))
        .WillOnce(
            [](const char *, int, const char *, char *tmpl, int)
            {
                return resolve_mkostemp(tmpl, "/tmp/abcAAAAAA");
            }); // [Pre-Assert確認_正常系] - mkostemp() が path、O_CLOEXEC を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - mkostemp() から、パスを "/tmp/abcAAAAAA" に書き換えてセンチネル記述子 7 を返す。
    EXPECT_CALL(mock_stdio_, fdopen(_, _, _, kFakeFd, StrEq("wb")))
        .WillOnce(Return(
            kStream)); // [Pre-Assert確認_正常系] - fdopen() が kFakeFd、"wb" と等しい文字列を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - fdopen() から、センチネル ストリームを返す。

    // Act
    FILE *fp = cplat_fopen_temp("abc", "wb", path, sizeof(path),
                                nullptr); // [手順] - prefix "abc" で cplat_fopen_temp() を呼び出す。

    // Assert
    EXPECT_EQ(kStream, fp); // [確認_正常系] - cplat_fopen_temp() の戻り値が kStream であること。
    EXPECT_STREQ(
        "/tmp/abcAAAAAA",
        path); // [確認_正常系] - path が "/tmp/abcAAAAAA" であること。(basename に prefix "abc" が含まれることの確認)
}

// prefix が NULL でも受理されることの確認
// [サブ手順参照 名前=stdioTempTest.SetUp]
TEST_F(stdioTempTest, null_prefix_is_accepted)
{
    // Arrange
    char path[PLATFORM_PATH_MAX] = {}; // [状態] - パスの受け取り先バッファーを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_unistd_, mkostemp(_, _, _, path, O_CLOEXEC))
        .WillOnce(
            [](const char *, int, const char *, char *tmpl, int)
            {
                return resolve_mkostemp(tmpl, "/tmp/cu_AAAAAA");
            }); // [Pre-Assert確認_正常系] - mkostemp() が path、O_CLOEXEC を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - mkostemp() から、既定 prefix のパスを書き込んでセンチネル記述子 7 を返す。
    EXPECT_CALL(mock_stdio_, fdopen(_, _, _, kFakeFd, StrEq("wb")))
        .WillOnce(Return(
            kStream)); // [Pre-Assert確認_正常系] - fdopen() が kFakeFd、"wb" と等しい文字列を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - fdopen() から、センチネル ストリームを返す。

    // Act
    FILE *fp = cplat_fopen_temp(nullptr, "wb", path, sizeof(path),
                                nullptr); // [手順] - prefix に nullptr を渡して cplat_fopen_temp() を呼び出す。

    // Assert
    EXPECT_EQ(kStream,
              fp); // [確認_正常系] - prefix が nullptr の cplat_fopen_temp() の戻り値が kStream であること。
    EXPECT_STREQ("/tmp/cu_AAAAAA",
                 path); // [確認_正常系] - path が "/tmp/cu_AAAAAA" であること。(既定の prefix "cu_" を含むことの確認)
}

// 3 文字を超える prefix が先頭 3 文字に切り詰められることの確認
// [サブ手順参照 名前=stdioTempTest.SetUp]
TEST_F(stdioTempTest, prefix_longer_than_three_chars_is_truncated)
{
    // Arrange
    char path[PLATFORM_PATH_MAX] = {}; // [状態] - パスの受け取り先バッファーを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_unistd_, mkostemp(_, _, _, path, O_CLOEXEC))
        .WillOnce(
            [](const char *, int, const char *, char *tmpl, int)
            {
                return resolve_mkostemp(tmpl, "/tmp/abcAAAAAA");
            }); // [Pre-Assert確認_正常系] - mkostemp() が path、O_CLOEXEC を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - mkostemp() から、先頭 3 文字の prefix を含むパスを書き込んでセンチネル記述子 7 を返す。
    EXPECT_CALL(mock_stdio_, fdopen(_, _, _, kFakeFd, StrEq("wb")))
        .WillOnce(Return(
            kStream)); // [Pre-Assert確認_正常系] - fdopen() が kFakeFd、"wb" と等しい文字列を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - fdopen() から、センチネル ストリームを返す。

    // Act
    FILE *fp = cplat_fopen_temp("abcd", "wb", path, sizeof(path),
                                nullptr); // [手順] - 4 文字の prefix "abcd" で cplat_fopen_temp() を呼び出す。

    // Assert
    EXPECT_EQ(kStream,
              fp); // [確認_正常系] - 4 文字以上の prefix でも cplat_fopen_temp() の戻り値が kStream であること。
    EXPECT_STREQ(
        "/tmp/abcAAAAAA",
        path); // [確認_正常系] - path が "/tmp/abcAAAAAA" であること。(prefix が先頭 3 文字 "abc" に切り詰められたことの確認)
}
#endif /* PLATFORM_LINUX */

// modes が NULL の場合に EINVAL で失敗することの確認
// [サブ手順参照 名前=stdioTempTest.SetUp]
TEST_F(stdioTempTest, null_modes_returns_einval)
{
    // Arrange
    char path[PLATFORM_PATH_MAX] = {};
    cplat_error err; // [状態] - 詳細エラーの受け取り先を用意する。
    cplat_error last_error;

    // Pre-Assert

    // Act
    FILE *fp = cplat_fopen_temp("ptr", nullptr, path, sizeof(path),
                                &err); // [手順] - modes に nullptr を渡して cplat_fopen_temp() を呼び出す。
    cplat_error_get_last(
        &last_error); // [手順] - TLS に記録された詳細エラーの格納先として last_error を指定して cplat_error_get_last() を呼び出す。

    // Assert
    EXPECT_EQ((FILE *)nullptr, fp); // [確認_異常系] - cplat_fopen_temp() の戻り値が nullptr であること。
    EXPECT_EQ(
        1,
        cplat_error_is(
            &err,
            CPLAT_CAUSE_INVALID_ARGUMENT)); // [確認_異常系] - CPLAT_CAUSE_INVALID_ARGUMENT に対する cplat_error_is() の戻り値が 1 であること。
    EXPECT_EQ(
        1,
        cplat_error_is_set(
            &last_error)); // [確認_異常系] - cplat_error_is_set() の戻り値が 1 であること。(TLS に詳細エラーが記録されたことの確認)
}

// path_out が NULL の場合に EINVAL で失敗することの確認
// [サブ手順参照 名前=stdioTempTest.SetUp]
TEST_F(stdioTempTest, null_path_out_returns_einval)
{
    // Arrange
    cplat_error err; // [状態] - 詳細エラーの受け取り先を用意する。

    // Pre-Assert

    // Act
    FILE *fp = cplat_fopen_temp("ptr", "wb", nullptr, 0u,
                                &err); // [手順] - path_out に nullptr を渡して cplat_fopen_temp() を呼び出す。

    // Assert
    EXPECT_EQ((FILE *)nullptr, fp); // [確認_異常系] - cplat_fopen_temp() の戻り値が nullptr であること。
    EXPECT_EQ(
        1,
        cplat_error_is(
            &err,
            CPLAT_CAUSE_INVALID_ARGUMENT)); // [確認_異常系] - CPLAT_CAUSE_INVALID_ARGUMENT に対する cplat_error_is() の戻り値が 1 であること。
}

// path_size が 0 の場合に EINVAL で失敗することの確認
// [サブ手順参照 名前=stdioTempTest.SetUp]
TEST_F(stdioTempTest, zero_path_size_returns_einval)
{
    // Arrange
    char path[1] = {'x'};
    cplat_error err; // [状態] - 詳細エラーの受け取り先を用意する。

    // Pre-Assert

    // Act
    FILE *fp = cplat_fopen_temp("ptr", "wb", path, 0u,
                                &err); // [手順] - path_size に 0 を渡して cplat_fopen_temp() を呼び出す。

    // Assert
    EXPECT_EQ((FILE *)nullptr, fp); // [確認_異常系] - cplat_fopen_temp() の戻り値が nullptr であること。
    EXPECT_EQ(
        1,
        cplat_error_is(
            &err,
            CPLAT_CAUSE_INVALID_ARGUMENT)); // [確認_異常系] - CPLAT_CAUSE_INVALID_ARGUMENT に対する cplat_error_is() の戻り値が 1 であること。
}

#if defined(PLATFORM_LINUX)
// path_size が必要長未満の場合に ENAMETOOLONG で失敗することの確認
// [サブ手順参照 名前=stdioTempTest.SetUp]
TEST_F(stdioTempTest, path_size_too_small_returns_enametoolong)
{
    // Arrange
    /* "<dir>/<prefix>XXXXXX" + NUL に満たない長さ */
    char path[4] = {}; // [状態] - 必要長に満たない 4 バイトのバッファーを用意する。
    cplat_error err;   // [状態] - 詳細エラーの受け取り先を用意する。

    // Pre-Assert

    // Act
    FILE *fp = cplat_fopen_temp("ptr", "wb", path, sizeof(path),
                                &err); // [手順] - path_size に 4 を渡して cplat_fopen_temp() を呼び出す。

    // Assert
    EXPECT_EQ((FILE *)nullptr, fp); // [確認_異常系] - cplat_fopen_temp() の戻り値が nullptr であること。
    EXPECT_EQ(
        1,
        cplat_error_is(
            &err,
            CPLAT_CAUSE_NAME_TOO_LONG)); // [確認_異常系] - CPLAT_CAUSE_NAME_TOO_LONG に対する cplat_error_is() の戻り値が 1 であること。
}

// TMPDIR の取得結果がバッファーに収まらない場合に失敗することの確認
// [サブ手順参照 名前=stdioTempTest.SetUp]
TEST_F(stdioTempTest, tmpdir_buffer_too_small_returns_enametoolong)
{
    // Arrange
    char path[PLATFORM_PATH_MAX] = {};
    cplat_error err; // [状態] - 詳細エラーの受け取り先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_getenv(StrEq("TMPDIR"), _, _, _, _))
        .WillOnce(Return(
            CPLAT_ERR_BUFFER_TOO_SMALL)); // [Pre-Assert確認_異常系] - cplat_getenv() が "TMPDIR" と等しい文字列を指定して 1 回呼び出されること。
                                          // [Pre-Assert手順] - cplat_getenv() から CPLAT_ERR_BUFFER_TOO_SMALL を返す。

    // Act
    FILE *fp = cplat_fopen_temp("ptr", "wb", path, sizeof(path),
                                &err); // [手順] - バッファー不足を返す TMPDIR の状態で cplat_fopen_temp() を呼び出す。

    // Assert
    EXPECT_EQ((FILE *)nullptr, fp); // [確認_異常系] - cplat_fopen_temp() の戻り値が nullptr であること。
    EXPECT_EQ(
        1,
        cplat_error_is(
            &err,
            CPLAT_CAUSE_NAME_TOO_LONG)); // [確認_異常系] - CPLAT_CAUSE_NAME_TOO_LONG に対する cplat_error_is() の戻り値が 1 であること。
}

// mkostemp の失敗が詳細エラーへ記録されることの確認
// [サブ手順参照 名前=stdioTempTest.SetUp]
TEST_F(stdioTempTest, mkostemp_failure_reports_errno)
{
    // Arrange
    char path[PLATFORM_PATH_MAX] = {};
    cplat_error err; // [状態] - 詳細エラーの受け取り先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_unistd_, mkostemp(_, _, _, path, O_CLOEXEC))
        .WillOnce(
            [](const char *, int, const char *, char *, int)
            {
                errno = ENOENT;
                return -1;
            }); // [Pre-Assert確認_異常系] - mkostemp() が path、O_CLOEXEC を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - mkostemp() から、errno に ENOENT を設定して -1 を返す。

    // Act
    FILE *fp = cplat_fopen_temp("ptr", "wb", path, sizeof(path),
                                &err); // [手順] - mkostemp() が失敗する状態で cplat_fopen_temp() を呼び出す。

    // Assert
    EXPECT_EQ((FILE *)nullptr, fp); // [確認_異常系] - cplat_fopen_temp() の戻り値が nullptr であること。
    EXPECT_EQ(ENOENT,
              cplat_error_get_errno(&err)); // [確認_異常系] - cplat_error_get_errno() の戻り値が ENOENT であること。
}

// fdopen の失敗が詳細エラーへ記録され、一時ファイルが削除されることの確認
// [サブ手順参照 名前=stdioTempTest.SetUp]
TEST_F(stdioTempTest, invalid_modes_reports_fdopen_error)
{
    // Arrange
    char path[PLATFORM_PATH_MAX] = {};
    cplat_error err; // [状態] - 詳細エラーの受け取り先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_unistd_, mkostemp(_, _, _, path, O_CLOEXEC))
        .WillOnce(
            [](const char *, int, const char *, char *tmpl, int)
            {
                return resolve_mkostemp(tmpl, "/tmp/ptrAAAAAA");
            }); // [Pre-Assert確認_異常系] - mkostemp() が path、O_CLOEXEC を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - mkostemp() から、パスを書き込んでセンチネル記述子 7 を返す。
    EXPECT_CALL(mock_stdio_, fdopen(_, _, _, kFakeFd, StrEq("q")))
        .WillOnce(
            [](const char *, int, const char *, int, const char *)
            {
                errno = EINVAL;
                return static_cast<FILE *>(nullptr);
            }); // [Pre-Assert確認_異常系] - fdopen() が kFakeFd、"q" と等しい文字列を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - fdopen() から、errno に EINVAL を設定して nullptr を返す。
    EXPECT_CALL(mock_unistd_, close(_, _, _, kFakeFd))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_異常系] - fdopen() の失敗後に close() が kFakeFd を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - close() から 0 を返す。
    EXPECT_CALL(mock_unistd_, unlink(_, _, _, StrEq("/tmp/ptrAAAAAA")))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_異常系] - fdopen() の失敗後に unlink() が "/tmp/ptrAAAAAA" と等しい文字列を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - unlink() から 0 を返す。

    // Act
    FILE *fp = cplat_fopen_temp("ptr", "q", path, sizeof(path),
                                &err); // [手順] - 不正なモードを指定して cplat_fopen_temp() を呼び出す。

    // Assert
    EXPECT_EQ((FILE *)nullptr, fp); // [確認_異常系] - cplat_fopen_temp() の戻り値が nullptr であること。
    EXPECT_EQ(EINVAL,
              cplat_error_get_errno(&err)); // [確認_異常系] - cplat_error_get_errno() の戻り値が EINVAL であること。
}

// 一時ファイルのパス整形に失敗した場合に ENAMETOOLONG で失敗することの確認
// [サブ手順参照 名前=stdioTempTest.SetUp]
TEST_F(stdioTempTest, path_formatting_failure_returns_enametoolong)
{
    // Arrange
    char path[PLATFORM_PATH_MAX] = {};
    cplat_error err; // [状態] - 詳細エラーの受け取り先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_snprintf(_, _, _))
        .WillOnce(Return(CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_異常系] - cplat_snprintf() が 1 回呼び出されること。
                                              // [Pre-Assert手順] - cplat_snprintf() から CPLAT_ERR_UNKNOWN を返す。

    // Act
    FILE *fp = cplat_fopen_temp("ptr", "wb", path, sizeof(path),
                                &err); // [手順] - パス整形が失敗する状態で cplat_fopen_temp() を呼び出す。

    // Assert
    EXPECT_EQ((FILE *)nullptr, fp); // [確認_異常系] - cplat_fopen_temp() の戻り値が nullptr であること。
    EXPECT_EQ(
        1,
        cplat_error_is(
            &err,
            CPLAT_CAUSE_NAME_TOO_LONG)); // [確認_異常系] - CPLAT_CAUSE_NAME_TOO_LONG に対する cplat_error_is() の戻り値が 1 であること。
}
#elif defined(PLATFORM_WINDOWS)

namespace
{

DWORD WINAPI stdio_temp_resolver_failing(DWORD, LPWSTR)
{
    SetLastError(ERROR_ACCESS_DENIED);
    return 0U;
}

DWORD WINAPI stdio_temp_resolver_custom_system_temp(DWORD buffer_length, LPWSTR buffer)
{
    static const wchar_t kPath[] = L"C:\\CustomSystemTemp\\";
    const DWORD len = (DWORD)(sizeof(kPath) / sizeof(kPath[0]) - 1U);
    if (buffer != nullptr && buffer_length > len)
    {
        std::memcpy(buffer, kPath, sizeof(kPath));
    }
    return len;
}

DWORD WINAPI stdio_temp_resolver_default_system_temp(DWORD buffer_length, LPWSTR buffer)
{
    static const wchar_t kPath[] = L"C:\\Windows\\SystemTemp\\";
    const DWORD len = (DWORD)(sizeof(kPath) / sizeof(kPath[0]) - 1U);
    if (buffer != nullptr && buffer_length > len)
    {
        std::memcpy(buffer, kPath, sizeof(kPath));
    }
    return len;
}

} // namespace

// 内部の一時ディレクトリ取得が失敗した場合に NULL を返し GetTempFileNameW を呼ばないことの確認
TEST_F(stdioTempTest, internal_temp_dir_failure_skips_get_temp_file_namew)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    char path[PLATFORM_PATH_UTF8_MAX] = {};
    cplat_error detail;
    cplat_internal_temp_dir_set_resolver_for_test(stdio_temp_resolver_failing);

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetTempFileNameW(_, _, _, _, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - 内部関数失敗時は GetTempFileNameW() が呼び出されないこと。

    // Act
    FILE *fp = cplat_fopen_temp("ptr", "wb", path, sizeof(path), &detail); // [手順] - 内部関数失敗時。

    // Assert
    EXPECT_EQ(nullptr, fp); // [確認_異常系] - NULL を返すこと。
    EXPECT_EQ(CPLAT_ERROR_DOMAIN_WINDOWS, cplat_error_get_domain(&detail)); // [確認_異常系] - Win32 ドメインであること。
    EXPECT_EQ((unsigned long)ERROR_ACCESS_DENIED, cplat_error_get_windows_error(&detail)); // [確認_異常系] - 詳細エラーが保持されること。

    // Cleanup
    cplat_internal_temp_dir_set_resolver_for_test(nullptr);
}

// GetTempFileNameW が ERROR_BUFFER_OVERFLOW で失敗した場合に詳細エラーを返すことの確認
TEST_F(stdioTempTest, get_temp_file_namew_buffer_overflow_reports_windows_error)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    char path[PLATFORM_PATH_UTF8_MAX] = {};
    cplat_error detail;
    cplat_internal_temp_dir_set_resolver_for_test(stdio_temp_resolver_custom_system_temp);

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetTempFileNameW(_, _, _, testing::StrEq(L"C:\\CustomSystemTemp\\"), _, 0U, _))
        .WillOnce(Return(0U)); // [Pre-Assert確認_異常系] - SystemTemp パスで GetTempFileNameW() が 1 回呼び出されること。
    EXPECT_CALL(mock_windows, GetLastError(_, _, _))
        .WillOnce(Return(ERROR_BUFFER_OVERFLOW)); // [Pre-Assert確認_異常系] - GetLastError() が 1 回呼び出されること。

    // Act
    FILE *fp = cplat_fopen_temp("ptr", "wb", path, sizeof(path), &detail); // [手順] - GetTempFileNameW が ERROR_BUFFER_OVERFLOW で失敗する場合。

    // Assert
    EXPECT_EQ(nullptr, fp); // [確認_異常系] - NULL を返すこと。
    EXPECT_EQ(CPLAT_ERROR_DOMAIN_WINDOWS, cplat_error_get_domain(&detail)); // [確認_異常系] - Win32 ドメインであること。
    EXPECT_EQ((unsigned long)ERROR_BUFFER_OVERFLOW, cplat_error_get_windows_error(&detail)); // [確認_異常系] - ERROR_BUFFER_OVERFLOW が記録されること。

    // Cleanup
    cplat_internal_temp_dir_set_resolver_for_test(nullptr);
}

// SystemTemp の既定値 C:\Windows\SystemTemp をそのまま GetTempFileNameW へ渡すことの確認
TEST_F(stdioTempTest, passes_default_system_temp_unmodified_to_get_temp_file_namew)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    char path[PLATFORM_PATH_UTF8_MAX] = {};
    cplat_error detail;
    cplat_internal_temp_dir_set_resolver_for_test(stdio_temp_resolver_default_system_temp);

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetTempFileNameW(_, _, _, testing::StrEq(L"C:\\Windows\\SystemTemp\\"), _, 0U, _))
        .WillOnce(Return(0U)); // [Pre-Assert確認_異常系] - C:\Windows\SystemTemp\ がそのまま渡されること。
    EXPECT_CALL(mock_windows, GetLastError(_, _, _)).WillOnce(Return(ERROR_PATH_NOT_FOUND));

    // Act
    FILE *fp = cplat_fopen_temp("ptr", "wb", path, sizeof(path), &detail); // [手順] - 既定 SystemTemp 時。

    // Assert
    EXPECT_EQ(nullptr, fp); // [確認_異常系] - Mock の失敗により NULL を返すこと。

    // Cleanup
    cplat_internal_temp_dir_set_resolver_for_test(nullptr);
}
#endif /* PLATFORM_ */
