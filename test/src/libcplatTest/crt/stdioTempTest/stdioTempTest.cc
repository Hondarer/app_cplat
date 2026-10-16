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
#endif /* PLATFORM_LINUX */

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
        // [状態] - cplat_getenv() の既定動作を設定する。
        ON_CALL(mock_unistd_, close(_, _, _, _)).WillByDefault(Return(0));
        // [状態] - close() の既定動作を設定する。
        ON_CALL(mock_unistd_, unlink(_, _, _, _)).WillByDefault(Return(0));
        // [状態] - unlink() の既定動作を設定する。
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
                // [Pre-Assert手順] - パスを "/tmp/ptrAAAAAA" に書き換え、センチネル記述子 7 を返却する。
    EXPECT_CALL(mock_stdio_, fdopen(_, _, _, kFakeFd, StrEq("wb")))
        .WillOnce(Return(
            kStream)); // [Pre-Assert確認_正常系] - fdopen() が kFakeFd、"wb" と等しい文字列を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - センチネル ストリームを返却する。

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
                // [Pre-Assert手順] - "/var/tmp" を格納し、CPLAT_OK を返却する。
    EXPECT_CALL(mock_unistd_, mkostemp(_, _, _, path, O_CLOEXEC))
        .WillOnce(
            [](const char *, int, const char *, char *tmpl, int)
            {
                return resolve_mkostemp(tmpl, "/var/tmp/ptrAAAAAA");
            }); // [Pre-Assert確認_正常系] - mkostemp() が path、O_CLOEXEC を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - パスを "/var/tmp/ptrAAAAAA" に書き換え、センチネル記述子 7 を返却する。
    EXPECT_CALL(mock_stdio_, fdopen(_, _, _, kFakeFd, StrEq("wb")))
        .WillOnce(Return(
            kStream)); // [Pre-Assert確認_正常系] - fdopen() が kFakeFd、"wb" と等しい文字列を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - センチネル ストリームを返却する。

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
                // [Pre-Assert手順] - 互いに異なる 4 つのパスを順に書き込み、センチネル記述子 7 を返却する。
    EXPECT_CALL(mock_stdio_, fdopen(_, _, _, kFakeFd, StrEq("wb")))
        .Times(4)
        .WillRepeatedly(Return(
            kStream)); // [Pre-Assert確認_正常系] - fdopen() が kFakeFd、"wb" と等しい文字列を指定して 4 回呼び出されること。
                       // [Pre-Assert手順] - センチネル ストリームを返却する。

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
                // [Pre-Assert手順] - パスを "/tmp/abcAAAAAA" に書き換え、センチネル記述子 7 を返却する。
    EXPECT_CALL(mock_stdio_, fdopen(_, _, _, kFakeFd, StrEq("wb")))
        .WillOnce(Return(
            kStream)); // [Pre-Assert確認_正常系] - fdopen() が kFakeFd、"wb" と等しい文字列を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - センチネル ストリームを返却する。

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
                // [Pre-Assert手順] - 既定 prefix のパスを書き込み、センチネル記述子 7 を返却する。
    EXPECT_CALL(mock_stdio_, fdopen(_, _, _, kFakeFd, StrEq("wb")))
        .WillOnce(Return(
            kStream)); // [Pre-Assert確認_正常系] - fdopen() が kFakeFd、"wb" と等しい文字列を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - センチネル ストリームを返却する。

    // Act
    FILE *fp = cplat_fopen_temp(nullptr, "wb", path, sizeof(path),
                                nullptr); // [手順] - prefix に NULL を渡して cplat_fopen_temp() を呼び出す。

    // Assert
    EXPECT_EQ(kStream,
              fp); // [確認_正常系] - prefix が NULL の cplat_fopen_temp() の戻り値が kStream であること。
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
                // [Pre-Assert手順] - 先頭 3 文字の prefix を含むパスを書き込み、センチネル記述子 7 を返却する。
    EXPECT_CALL(mock_stdio_, fdopen(_, _, _, kFakeFd, StrEq("wb")))
        .WillOnce(Return(
            kStream)); // [Pre-Assert確認_正常系] - fdopen() が kFakeFd、"wb" と等しい文字列を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - センチネル ストリームを返却する。

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
                                &err); // [手順] - modes に NULL を渡して cplat_fopen_temp() を呼び出す。
    cplat_error_get_last(&last_error); // [手順] - TLS に記録された詳細エラーを取得する。

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
                                &err); // [手順] - path_out に NULL を渡して cplat_fopen_temp() を呼び出す。

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
                                          // [Pre-Assert手順] - CPLAT_ERR_BUFFER_TOO_SMALL を返却する。

    // Act
    FILE *fp = cplat_fopen_temp("ptr", "wb", path, sizeof(path),
                                &err); // [手順] - バッファー不足を返す TMPDIR を指定して呼び出す。

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
                // [Pre-Assert手順] - errno に ENOENT を設定し、-1 を返却する。

    // Act
    FILE *fp = cplat_fopen_temp("ptr", "wb", path, sizeof(path),
                                &err); // [手順] - mkostemp() の失敗を注入して呼び出す。

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
                // [Pre-Assert手順] - パスを書き込み、センチネル記述子 7 を返却する。
    EXPECT_CALL(mock_stdio_, fdopen(_, _, _, kFakeFd, StrEq("q")))
        .WillOnce(
            [](const char *, int, const char *, int, const char *)
            {
                errno = EINVAL;
                return static_cast<FILE *>(nullptr);
            }); // [Pre-Assert確認_異常系] - fdopen() が kFakeFd、"q" と等しい文字列を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - errno に EINVAL を設定し、NULL を返却する。
    EXPECT_CALL(mock_unistd_, close(_, _, _, kFakeFd))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_異常系] - fdopen の失敗後に close() が kFakeFd を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - 0 を返却する。
    EXPECT_CALL(mock_unistd_, unlink(_, _, _, StrEq("/tmp/ptrAAAAAA")))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_異常系] - fdopen の失敗後に unlink() が "/tmp/ptrAAAAAA" と等しい文字列を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - 0 を返却する。

    // Act
    FILE *fp = cplat_fopen_temp("ptr", "q", path, sizeof(path),
                                &err); // [手順] - 不正なモードを指定して呼び出す。

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
                                              // [Pre-Assert手順] - CPLAT_ERR_UNKNOWN を返却する。

    // Act
    FILE *fp = cplat_fopen_temp("ptr", "wb", path, sizeof(path),
                                &err); // [手順] - パス整形失敗を注入して呼び出す。

    // Assert
    EXPECT_EQ((FILE *)nullptr, fp); // [確認_異常系] - cplat_fopen_temp() の戻り値が nullptr であること。
    EXPECT_EQ(
        1,
        cplat_error_is(
            &err,
            CPLAT_CAUSE_NAME_TOO_LONG)); // [確認_異常系] - CPLAT_CAUSE_NAME_TOO_LONG に対する cplat_error_is() の戻り値が 1 であること。
}
#endif /* PLATFORM_LINUX */
