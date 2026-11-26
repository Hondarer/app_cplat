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
    #include <cplat/crt/stdlib.h>
    #include <cplat/crt/temp_dir_internal.h>
    #include <mock_stdio.h>
    #include <mock_windows.h>
    #include <cstdint>
    #include <cwchar>
    #include <string>
    #include <vector>
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

// 一時ディレクトリの取得の差し替えで返すパス。テストごとに設定する。
std::wstring g_temp_dir;

DWORD WINAPI stdio_temp_resolver_failing(DWORD, LPWSTR)
{
    SetLastError(ERROR_ACCESS_DENIED);
    return 0U;
}

DWORD WINAPI stdio_temp_resolver_from_global(DWORD buffer_length, LPWSTR buffer)
{
    const DWORD len = static_cast<DWORD>(g_temp_dir.size());
    if (buffer != nullptr && buffer_length > len)
    {
        std::memcpy(buffer, g_temp_dir.c_str(), (g_temp_dir.size() + 1U) * sizeof(wchar_t));
    }
    return len;
}

// 実在の一時ディレクトリの配下に、長さが dir_length 文字以上 (区切りを含まない) の作業用のディレクトリを作る。
// 破棄で、作ったディレクトリを削除する。
class TempWorkDir
{
  public:
    explicit TempWorkDir(size_t dir_length)
    {
        wchar_t base[MAX_PATH + 2] = {};
        const DWORD base_length = GetTempPathW(static_cast<DWORD>(sizeof(base) / sizeof(base[0])), base);
        if (base_length == 0U)
        {
            return;
        }

        std::wstring dir = std::wstring(base) + L"cplat_stdio_temp_" + std::to_wstring(GetCurrentProcessId());
        if (!create_dir(dir))
        {
            return;
        }

        while (dir.size() < dir_length)
        {
            const size_t remaining = dir_length - dir.size() - 1U;
            const size_t take = remaining < 50U ? remaining : 50U;
            if (take == 0U)
            {
                break;
            }
            dir += L"\\" + std::wstring(take, L'd');
            if (!create_dir(dir))
            {
                return;
            }
        }

        dir_ = dir;
    }

    ~TempWorkDir()
    {
        for (auto it = created_.rbegin(); it != created_.rend(); ++it)
        {
            (void)RemoveDirectoryW((L"\\\\?\\" + *it).c_str());
        }
    }

    TempWorkDir(const TempWorkDir &) = delete;
    TempWorkDir &operator=(const TempWorkDir &) = delete;

    // 用意したディレクトリ (末尾の区切りを含まない)。用意できなかった場合は空。
    const std::wstring &dir() const
    {
        return dir_;
    }

    // 用意したディレクトリが空であれば true を返す (作成した一時ファイルが削除されたことの確認に使う)。
    bool is_empty() const
    {
        WIN32_FIND_DATAW data;
        const std::wstring pattern = L"\\\\?\\" + dir_ + L"\\*";
        HANDLE find = FindFirstFileW(pattern.c_str(), &data);
        bool empty = true;
        if (find == INVALID_HANDLE_VALUE)
        {
            return true;
        }
        do
        {
            if (std::wcscmp(data.cFileName, L".") != 0 && std::wcscmp(data.cFileName, L"..") != 0)
            {
                empty = false;
            }
        } while (FindNextFileW(find, &data));
        (void)FindClose(find);
        return empty;
    }

  private:
    bool create_dir(const std::wstring &dir)
    {
        if (!CreateDirectoryW((L"\\\\?\\" + dir).c_str(), nullptr))
        {
            return false;
        }
        created_.push_back(dir);
        return true;
    }

    std::vector<std::wstring> created_;
    std::wstring dir_;
};

// 環境変数 TMP を一時的に変更する。破棄で元に戻す。
class TmpEnvGuard
{
  public:
    explicit TmpEnvGuard(const std::wstring &value)
    {
        const DWORD saved_size = GetEnvironmentVariableW(L"TMP", nullptr, 0U);
        if (saved_size != 0U)
        {
            std::vector<wchar_t> saved(saved_size, L'\0');
            if (GetEnvironmentVariableW(L"TMP", saved.data(), saved_size) + 1U == saved_size)
            {
                saved_.assign(saved.data());
                has_saved_ = true;
            }
        }
        changed_ = SetEnvironmentVariableW(L"TMP", value.c_str()) != FALSE;
    }

    ~TmpEnvGuard()
    {
        if (changed_)
        {
            (void)SetEnvironmentVariableW(L"TMP", has_saved_ ? saved_.c_str() : nullptr);
        }
    }

    TmpEnvGuard(const TmpEnvGuard &) = delete;
    TmpEnvGuard &operator=(const TmpEnvGuard &) = delete;

    bool changed() const
    {
        return changed_;
    }

  private:
    std::wstring saved_;
    bool has_saved_ = false;
    bool changed_ = false;
};

std::string to_utf8_with_slashes(const std::wstring &text)
{
    const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>(length > 0 ? length : 1), '\0');
    if (length > 0)
    {
        (void)WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, &result[0], length, nullptr, nullptr);
    }
    result.resize(std::strlen(result.c_str()));
    for (char &ch : result)
    {
        if (ch == '\\')
        {
            ch = '/';
        }
    }
    return result;
}

std::wstring utf8_path_to_backslash_wide(const char *path)
{
    const int length = MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
    std::wstring wide(static_cast<size_t>(length > 0 ? length : 1), L'\0');
    if (length > 0)
    {
        (void)MultiByteToWideChar(CP_UTF8, 0, path, -1, &wide[0], length);
    }
    wide.resize(std::wcslen(wide.c_str()));
    for (wchar_t &ch : wide)
    {
        if (ch == L'/')
        {
            ch = L'\\';
        }
    }
    return wide;
}

// path が "<dir_utf8>/<prefix>" に続く 16 進 16 文字と ".tmp" であるかを判定する。
bool is_temp_file_path(const std::string &path, const std::string &dir_utf8, const std::string &prefix)
{
    const std::string head = dir_utf8 + "/" + prefix;
    if (path.size() != head.size() + 16U + 4U || path.compare(0, head.size(), head) != 0)
    {
        return false;
    }
    for (size_t i = head.size(); i < head.size() + 16U; ++i)
    {
        if (std::strchr("0123456789abcdef", path[i]) == nullptr)
        {
            return false;
        }
    }
    return path.compare(head.size() + 16U, 4U, ".tmp") == 0;
}

} // namespace

// 内部の一時ディレクトリの取得が失敗した場合に NULL を返し、詳細エラーを記録することの確認
TEST_F(stdioTempTest, internal_temp_dir_failure_returns_null)
{
    // Arrange
    char path[PLATFORM_PATH_UTF8_MAX] = {};
    cplat_error detail;
    cplat_internal_temp_dir_set_resolver_for_test(
        stdio_temp_resolver_failing); // [状態] - 一時ディレクトリの取得が ERROR_ACCESS_DENIED で失敗するように差し替える。

    // Pre-Assert

    // Act
    FILE *fp =
        cplat_fopen_temp("ptr", "wb", path, sizeof(path),
                         &detail); // [手順] - 一時ディレクトリの取得が失敗する状態で cplat_fopen_temp() を呼び出す。

    // Assert
    EXPECT_EQ(nullptr, fp); // [確認_異常系] - cplat_fopen_temp() の戻り値が nullptr であること。
    EXPECT_EQ(
        CPLAT_ERROR_DOMAIN_WINDOWS,
        cplat_error_get_domain(&detail)); // [確認_異常系] - detail のドメインが CPLAT_ERROR_DOMAIN_WINDOWS であること。
    EXPECT_EQ((unsigned long)ERROR_ACCESS_DENIED,
              cplat_error_get_windows_error(
                  &detail)); // [確認_異常系] - detail の Win32 エラーが ERROR_ACCESS_DENIED であること。

    // Cleanup
    cplat_internal_temp_dir_set_resolver_for_test(nullptr);
}

// prefix が NULL の場合に、既定の接頭辞 "cu_" の一時ファイルを一時ディレクトリの直下に作り、
// 区切りを '/' にした UTF-8 のパスを path_out へ返すことの確認
TEST_F(stdioTempTest, reports_path_with_default_prefix_under_temp_dir)
{
    // Arrange
    TempWorkDir work(0U);             // [状態] - 実在の一時ディレクトリの配下に、作業用のディレクトリを作成する。
    ASSERT_FALSE(work.dir().empty()); // [状態確認] - work.dir() が空でないこと。
    g_temp_dir = work.dir() + L"\\";  // [状態] - 一時ディレクトリを、作業用のディレクトリとする。
    cplat_internal_temp_dir_set_resolver_for_test(stdio_temp_resolver_from_global);
    char path[PLATFORM_PATH_UTF8_MAX] = {};
    int close_result = -1;
    BOOL deleted = FALSE;

    // Pre-Assert

    // Act
    FILE *fp = cplat_fopen_temp(nullptr, "wb", path, sizeof(path),
                                nullptr); // [手順] - prefix に nullptr を渡して cplat_fopen_temp() を呼び出す。
    if (fp != nullptr)
    {
        close_result = std::fclose(fp);                                   // [手順] - ストリームを閉じる。
        deleted = DeleteFileW(utf8_path_to_backslash_wide(path).c_str()); // [手順] - 作成されたファイルを削除する。
    }

    // Assert
    EXPECT_NE(nullptr, fp); // [確認_正常系] - cplat_fopen_temp() の戻り値が nullptr 以外であること。
    EXPECT_TRUE(is_temp_file_path(
        path, to_utf8_with_slashes(work.dir()),
        "cu_")); // [確認_正常系] - path が、区切りを '/' にした作業用のディレクトリの直下の "cu_" + 16 進 16 文字 + ".tmp" であること。
    EXPECT_EQ(0, close_result); // [確認_正常系] - fclose() の戻り値が 0 であること。
    EXPECT_NE(FALSE, deleted);  // [確認_正常系] - 作成されたファイルを削除できたこと。

    // Cleanup
    cplat_internal_temp_dir_set_resolver_for_test(nullptr);
}

// path_out が変換結果に足りない場合に、作成した一時ファイルを削除して NULL を返すことの確認
TEST_F(stdioTempTest, small_path_out_deletes_created_file)
{
    // Arrange
    TempWorkDir work(0U);             // [状態] - 実在の一時ディレクトリの配下に、作業用のディレクトリを作成する。
    ASSERT_FALSE(work.dir().empty()); // [状態確認] - work.dir() が空でないこと。
    g_temp_dir = work.dir() + L"\\";  // [状態] - 一時ディレクトリを、作業用のディレクトリとする。
    cplat_internal_temp_dir_set_resolver_for_test(stdio_temp_resolver_from_global);
    char path[8] = {};
    cplat_error detail;

    // Pre-Assert

    // Act
    FILE *fp = cplat_fopen_temp("ptr", "wb", path, sizeof(path),
                                &detail); // [手順] - 8 バイトの path_out で cplat_fopen_temp() を呼び出す。

    // Assert
    EXPECT_EQ(nullptr, fp); // [確認_異常系] - cplat_fopen_temp() の戻り値が nullptr であること。
    EXPECT_EQ(
        1,
        cplat_error_is_set(
            &detail)); // [確認_異常系] - cplat_error_is_set() の戻り値が 1 であること。(detail に詳細が記録されたことの確認)
    EXPECT_TRUE(work.is_empty()); // [確認_異常系] - 作成された一時ファイルが削除されていること。

    // Cleanup
    cplat_internal_temp_dir_set_resolver_for_test(nullptr);
}

// _wfsopen が失敗した場合に、作成した一時ファイルを削除し、その errno で失敗を返すことの確認
TEST_F(stdioTempTest, wfsopen_failure_deletes_created_file)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    TempWorkDir work(0U);             // [状態] - 実在の一時ディレクトリの配下に、作業用のディレクトリを作成する。
    ASSERT_FALSE(work.dir().empty()); // [状態確認] - work.dir() が空でないこと。
    g_temp_dir = work.dir() + L"\\";  // [状態] - 一時ディレクトリを、作業用のディレクトリとする。
    cplat_internal_temp_dir_set_resolver_for_test(stdio_temp_resolver_from_global);
    char path[PLATFORM_PATH_UTF8_MAX] = {};
    cplat_error detail;

    // Pre-Assert
    EXPECT_CALL(mock_stdio, _wfsopen(_, _, _, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, const wchar_t *, const wchar_t *, int) -> FILE *
            {
                errno = EACCES;
                return nullptr;
            }); // [Pre-Assert確認_異常系] - _wfsopen() が 1 回呼び出されること。
                // [Pre-Assert手順] - _wfsopen() から、errno に EACCES を設定して nullptr を返す。

    // Act
    FILE *fp = cplat_fopen_temp("ptr", "wb", path, sizeof(path),
                                &detail); // [手順] - _wfsopen() が失敗する状態で cplat_fopen_temp() を呼び出す。

    // Assert
    EXPECT_EQ(nullptr, fp); // [確認_異常系] - cplat_fopen_temp() の戻り値が nullptr であること。
    EXPECT_EQ(EACCES, cplat_error_get_errno(&detail)); // [確認_異常系] - detail の errno が EACCES であること。
    EXPECT_TRUE(work.is_empty());                      // [確認_異常系] - 作成された一時ファイルが削除されていること。

    // Cleanup
    cplat_internal_temp_dir_set_resolver_for_test(nullptr);
}

// 一時ファイルのパスが MAX_PATH 以上になる実在の一時ディレクトリで、作成、書き込み、削除ができ、
// path_out が拡張パスの接頭辞付きのパスになることの確認
TEST(stdioTempLongPathTest, creates_writes_and_deletes_file_under_long_temp_dir)
{
    // Arrange
    // 一時ディレクトリ (末尾の区切りを含めて 241 文字) と名前 (23 文字) で、完全なパスが MAX_PATH を超える。
    // GetTempPathW は MAX_PATH + 1 文字までしか返さないため、ディレクトリ自体は MAX_PATH 未満にする。
    TempWorkDir work(240U); // [状態] - 実在の一時ディレクトリの配下に、240 文字の作業用のディレクトリを作成する。
    ASSERT_FALSE(work.dir().empty()); // [状態確認] - work.dir() が空でないこと。
    TmpEnvGuard tmp(work.dir());      // [状態] - 環境変数 TMP を作業用のディレクトリへ向ける。
    ASSERT_TRUE(tmp.changed());       // [状態確認] - tmp.changed() が true であること。
    std::wstring resolved_dir;
    {
        wchar_t *resolved = nullptr;
        ASSERT_EQ(CPLAT_OK,
                  cplat_internal_get_temp_dir_w(
                      &resolved, nullptr,
                      nullptr)); // [状態確認] - cplat_internal_get_temp_dir_w() の戻り値が CPLAT_OK であること。
        resolved_dir = resolved;
        cplat_free(resolved);
    }
    if (resolved_dir.find(L"SystemTemp") != std::wstring::npos)
    {
        // SYSTEM アカウントでは GetTempPath2W が TMP を参照せず、SystemTemp を返す。
        GTEST_SKIP() << "the temp directory does not follow TMP (SYSTEM account)";
    }
    ASSERT_EQ(work.dir() + L"\\",
              resolved_dir); // [状態確認] - 一時ディレクトリが TMP に設定したディレクトリであること。
    char path[PLATFORM_PATH_UTF8_MAX] = {};
    cplat_error detail;
    const char data[] = "hello";
    char read_back[sizeof(data)] = {};
    size_t written = 0U;
    size_t read = 0U;
    int close_result = -1;
    BOOL deleted = FALSE;
    DWORD attributes_after = 0U;

    // Pre-Assert

    // Act
    FILE *fp =
        cplat_fopen_temp("ptr", "w+b", path, sizeof(path),
                         &detail); // [手順] - 長い一時ディレクトリで、モード "w+b" で cplat_fopen_temp() を呼び出す。
    if (fp != nullptr)
    {
        written = std::fwrite(data, 1U, sizeof(data) - 1U, fp);  // [手順] - 5 バイトを書き込む。
        (void)std::fseek(fp, 0L, SEEK_SET);                      // [手順] - 先頭へ戻す。
        read = std::fread(read_back, 1U, sizeof(data) - 1U, fp); // [手順] - 5 バイトを読み戻す。
        close_result = std::fclose(fp);                          // [手順] - ストリームを閉じる。
        const std::wstring wide_path = utf8_path_to_backslash_wide(path);
        deleted = DeleteFileW(wide_path.c_str()); // [手順] - 区切りを '\' へ戻したパスで DeleteFileW() を呼び出す。
        attributes_after = GetFileAttributesW(wide_path.c_str()); // [手順] - 削除後のファイルの属性を取得する。
    }

    // Assert
    EXPECT_NE(nullptr, fp); // [確認_正常系] - cplat_fopen_temp() の戻り値が nullptr 以外であること。
    EXPECT_TRUE(is_temp_file_path(
        path, "//?/" + to_utf8_with_slashes(work.dir()),
        "ptr")); // [確認_正常系] - path が "//?/" を前置した作業用のディレクトリの直下の "ptr" + 16 進 16 文字 + ".tmp" であること。
    EXPECT_GE(std::strlen(path),
              static_cast<size_t>(MAX_PATH)); // [確認_正常系] - path の長さが MAX_PATH 以上であること。
    EXPECT_EQ(sizeof(data) - 1U, written);    // [確認_正常系] - 書き込めたバイト数が 5 であること。
    EXPECT_EQ(sizeof(data) - 1U, read);       // [確認_正常系] - 読み戻せたバイト数が 5 であること。
    EXPECT_STREQ(data, read_back);            // [確認_正常系] - 読み戻した内容が "hello" であること。
    EXPECT_EQ(0, close_result);               // [確認_正常系] - fclose() の戻り値が 0 であること。
    EXPECT_NE(FALSE, deleted);                // [確認_正常系] - DeleteFileW() の戻り値が FALSE 以外であること。
    EXPECT_EQ(INVALID_FILE_ATTRIBUTES, attributes_after); // [確認_正常系] - 削除後にファイルが存在しないこと。
}
#endif /* PLATFORM_ */
