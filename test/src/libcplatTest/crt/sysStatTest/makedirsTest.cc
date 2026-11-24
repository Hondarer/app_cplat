#include <testfw.h>
#include <cplat/base/platform.h>
#include <cplat/base/result.h>
#include <cplat/crt/sys/stat.h>
#include <cplat/crt/path.h>
#include <mock_cplat.h>
#include <cerrno>
#include <cstring>
#include <string>

#if defined(PLATFORM_LINUX)
    #include <sys/mock_stat.h>
    #include <sys/stat.h>
    #include <unistd.h>
#elif defined(PLATFORM_WINDOWS)
    #include <mock_direct.h>
    #include <sys/mock_stat.h>
#endif /* PLATFORM_ */
#include <filesystem>
#include <fstream>

using testing::_;
using testing::InSequence;
using testing::NiceMock;
using testing::Return;
using testing::StrEq;

#if defined(PLATFORM_LINUX)

class makedirsTest : public testing::Test
{
  protected:
    NiceMock<Mock_sys_stat> mock_sys_stat_;
};

#else /* PLATFORM_LINUX */

class makedirsTest : public testing::Test
{
};

#endif /* PLATFORM_LINUX */

#if defined(PLATFORM_WINDOWS)
    #include <filesystem>

static std::string to_windows_sep(const char *path)
{
    std::string result(path);
    for (char &ch : result)
    {
        if (ch == '/')
        {
            ch = '\\';
        }
    }
    return result;
}
#endif /* PLATFORM_WINDOWS */

// NULL パスは CPLAT_ERR_INVALID_ARGUMENT を返すことの確認
TEST_F(makedirsTest, null_path_returns_invalid_argument)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret = cplat_makedirs(nullptr, nullptr); // [手順] - パスに nullptr を渡して cplat_makedirs() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_makedirs() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// 空文字列パスは CPLAT_ERR_INVALID_ARGUMENT を返すことの確認
TEST_F(makedirsTest, empty_path_returns_invalid_argument)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret = cplat_makedirs("", nullptr); // [手順] - 空文字列パスで cplat_makedirs() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_makedirs() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// cplat_stat が NULL 引数を拒否することの確認
TEST_F(makedirsTest, stat_rejects_null_arguments)
{
    // Arrange
    cplat_file_stat_t stat_buffer;

    // Pre-Assert

    // Act
    const int null_buffer_result =
        cplat_stat(nullptr, nullptr, "missing"); // [手順] - 出力先に nullptr を指定して cplat_stat() を呼び出す。
    const int null_path_result =
        cplat_stat(&stat_buffer, nullptr, nullptr); // [手順] - パスに nullptr を指定して cplat_stat() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_buffer_result); // [確認_異常系] - 出力先 nullptr の cplat_stat() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_path_result); // [確認_異常系] - パス nullptr の cplat_stat() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

#if defined(PLATFORM_LINUX)
// 存在しないパスの stat が失敗することの確認
TEST_F(makedirsTest, stat_reports_missing_path)
{
    // Arrange
    cplat_file_stat_t stat_buffer;

    // Pre-Assert
    EXPECT_CALL(mock_sys_stat_, stat(_, _, _, StrEq("missing"), _))
        .WillOnce(
            [](const char *, int, const char *, const char *, struct stat *)
            {
                errno = ENOENT;
                return -1;
            }); // [Pre-Assert確認_異常系] - stat() が "missing" と等しい文字列を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - stat() から、errno に ENOENT を設定して -1 を返す。

    // Act
    const int result =
        cplat_stat(&stat_buffer, nullptr, "missing"); // [手順] - 存在しないパスを指定して cplat_stat() を呼び出す。

    // Assert
    EXPECT_NE(CPLAT_OK,
              result); // [確認_異常系] - 存在しないパスの cplat_stat() の戻り値が CPLAT_OK 以外であること。
}
#endif /* PLATFORM_LINUX */

// cplat_mkdir が NULL パスを拒否することの確認
TEST_F(makedirsTest, mkdir_rejects_null_path)
{
    // Arrange

    // Pre-Assert

    // Act
    const int result = cplat_mkdir(nullptr, nullptr); // [手順] - パスに nullptr を指定して cplat_mkdir() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        result); // [確認_異常系] - nullptr パスの cplat_mkdir() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

#if defined(PLATFORM_LINUX)
// 既存ディレクトリの mkdir が失敗することの確認
TEST_F(makedirsTest, mkdir_reports_existing_directory)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_sys_stat_, mkdir(_, _, _, StrEq("existing"), _))
        .WillOnce(
            [](const char *, int, const char *, const char *, mode_t)
            {
                errno = EEXIST;
                return -1;
            }); // [Pre-Assert確認_異常系] - mkdir() が "existing" と等しい文字列を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - mkdir() から、errno に EEXIST を設定して -1 を返す。

    // Act
    const int result =
        cplat_mkdir("existing", nullptr); // [手順] - 既存ディレクトリを指定して cplat_mkdir() を呼び出す。

    // Assert
    EXPECT_NE(CPLAT_OK,
              result); // [確認_異常系] - 既存ディレクトリの cplat_mkdir() の戻り値が CPLAT_OK 以外であること。
}
#endif /* PLATFORM_LINUX */

// PLATFORM_PATH_MAX 以上のパスが拒否されることの確認
TEST_F(makedirsTest, overlong_path_returns_name_too_long)
{
    // Arrange
    const std::string path(PLATFORM_PATH_MAX, 'x'); // [状態] - PLATFORM_PATH_MAX 以上のパスを用意する。

    // Pre-Assert

    // Act
    const int result =
        cplat_makedirs(path.c_str(), nullptr); // [手順] - 長過ぎるパスを指定して cplat_makedirs() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_BUFFER_TOO_SMALL,
        result); // [確認_異常系] - 長過ぎるパスの cplat_makedirs() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
}

#if defined(PLATFORM_LINUX)
// 単一階層ディレクトリの新規作成とべき等性が保たれることの確認
TEST_F(makedirsTest, single_level_creates_directory)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_sys_stat_, stat(_, _, _, StrEq("dir1"), _))
        .WillOnce(
            [](const char *, int, const char *, const char *, struct stat *)
            {
                errno = ENOENT;
                return -1;
            })
        .WillOnce(
            Return(0)); // [Pre-Assert確認_正常系] - stat() が "dir1" と等しい文字列を指定して 2 回呼び出されること。
                        // [Pre-Assert手順] - stat() から 1 回目は errno に ENOENT を設定して -1、2 回目は 0 を返す。
    EXPECT_CALL(mock_sys_stat_, mkdir(_, _, _, StrEq("dir1"), _))
        .WillOnce(
            Return(0)); // [Pre-Assert確認_正常系] - mkdir() が "dir1" と等しい文字列を指定して 1 回呼び出されること。
                        // [Pre-Assert手順] - mkdir() から 0 を返す。

    // Act
    int actual_ret = cplat_makedirs(
        "dir1", nullptr); // [手順] - 存在しない単一階層ディレクトリを指定して cplat_makedirs() を呼び出す。
    int ret2 = cplat_makedirs("dir1", nullptr); // [手順] - 既存ディレクトリを指定して cplat_makedirs() を再度呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 1 回目の cplat_makedirs() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        ret2); // [確認_正常系] - 2 回目の cplat_makedirs() の戻り値が CPLAT_OK であること。(再呼び出しがべき等であることの確認)
}

// 複数階層ディレクトリが再帰的に作成されることの確認
TEST_F(makedirsTest, nested_levels_creates_all_directories)
{
    // Arrange
    InSequence sequence;

    // Pre-Assert
    EXPECT_CALL(mock_sys_stat_, stat(_, _, _, StrEq("root"), _))
        .WillOnce(
            [](const char *, int, const char *, const char *, struct stat *)
            {
                errno = ENOENT;
                return -1;
            }); // [Pre-Assert確認_正常系] - stat() が "root" と等しい文字列を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - stat() から、errno に ENOENT を設定して -1 を返す。
    EXPECT_CALL(mock_sys_stat_, mkdir(_, _, _, StrEq("root"), _))
        .WillOnce(
            Return(0)); // [Pre-Assert確認_正常系] - mkdir() が "root" と等しい文字列を指定して 1 回呼び出されること。
                        // [Pre-Assert手順] - mkdir() から 0 を返す。
    EXPECT_CALL(mock_sys_stat_, stat(_, _, _, StrEq("root/sub"), _))
        .WillOnce(
            [](const char *, int, const char *, const char *, struct stat *)
            {
                errno = ENOENT;
                return -1;
            }); // [Pre-Assert確認_正常系] - stat() が "root/sub" と等しい文字列を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - stat() から、errno に ENOENT を設定して -1 を返す。
    EXPECT_CALL(mock_sys_stat_, mkdir(_, _, _, StrEq("root/sub"), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - mkdir() が "root/sub" と等しい文字列を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - mkdir() から 0 を返す。
    EXPECT_CALL(mock_sys_stat_, stat(_, _, _, StrEq("root/sub/leaf"), _))
        .WillOnce(
            [](const char *, int, const char *, const char *, struct stat *)
            {
                errno = ENOENT;
                return -1;
            }); // [Pre-Assert確認_正常系] - stat() が "root/sub/leaf" と等しい文字列を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - stat() から、errno に ENOENT を設定して -1 を返す。
    EXPECT_CALL(mock_sys_stat_, mkdir(_, _, _, StrEq("root/sub/leaf"), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - mkdir() が "root/sub/leaf" と等しい文字列を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - mkdir() から 0 を返す。

    // Act
    int actual_ret = cplat_makedirs(
        "root/sub/leaf",
        nullptr); // [手順] - 中間ディレクトリが存在しない 2 階層パスを指定して cplat_makedirs() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_makedirs() の戻り値が CPLAT_OK であること。
}

// 相対パスの複数階層ディレクトリを作成できることの確認
TEST_F(makedirsTest, relative_path_creates_nested_directories)
{
    // Arrange
    InSequence sequence;

    // Pre-Assert
    EXPECT_CALL(mock_sys_stat_, stat(_, _, _, StrEq("rel"), _))
        .WillOnce(
            [](const char *, int, const char *, const char *, struct stat *)
            {
                errno = ENOENT;
                return -1;
            }); // [Pre-Assert確認_正常系] - stat() が "rel" と等しい文字列を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - stat() から、errno に ENOENT を設定して -1 を返す。
    EXPECT_CALL(mock_sys_stat_, mkdir(_, _, _, StrEq("rel"), _))
        .WillOnce(
            Return(0)); // [Pre-Assert確認_正常系] - mkdir() が "rel" と等しい文字列を指定して 1 回呼び出されること。
                        // [Pre-Assert手順] - mkdir() から 0 を返す。
    EXPECT_CALL(mock_sys_stat_, stat(_, _, _, StrEq("rel/sub"), _))
        .WillOnce(
            [](const char *, int, const char *, const char *, struct stat *)
            {
                errno = ENOENT;
                return -1;
            }); // [Pre-Assert確認_正常系] - stat() が "rel/sub" と等しい文字列を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - stat() から、errno に ENOENT を設定して -1 を返す。
    EXPECT_CALL(mock_sys_stat_, mkdir(_, _, _, StrEq("rel/sub"), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - mkdir() が "rel/sub" と等しい文字列を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - mkdir() から 0 を返す。

    // Act
    const int result =
        cplat_makedirs("rel/sub", nullptr); // [手順] - 相対パスの複数階層を指定して cplat_makedirs() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - 相対パスの cplat_makedirs() の戻り値が CPLAT_OK であること。
}

// ルート直後の連続区切り文字を中間ディレクトリとして扱わないことの確認
TEST_F(makedirsTest, repeated_root_separators_do_not_create_intermediate_directory)
{
    // Arrange
    InSequence sequence;

    // Pre-Assert
    EXPECT_CALL(mock_sys_stat_, stat(_, _, _, StrEq("///target"), _))
        .WillOnce(
            [](const char *, int, const char *, const char *, struct stat *)
            {
                errno = ENOENT;
                return -1;
            }); // [Pre-Assert確認_正常系] - stat() が "///target" と等しい文字列を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - stat() から、errno に ENOENT を設定して -1 を返す。
    EXPECT_CALL(mock_sys_stat_, mkdir(_, _, _, StrEq("///target"), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - mkdir() が "///target" と等しい文字列を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - mkdir() から 0 を返す。

    // Act
    const int result = cplat_makedirs(
        "///target", nullptr); // [手順] - ルート直後に区切り文字が連続するパスを指定して cplat_makedirs() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_makedirs() の戻り値が CPLAT_OK であること。
}

// 対象が存在せず mkdir が成功した場合に成功することの確認
TEST_F(makedirsTest, returns_success_when_mkdir_creates_target)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_sys_stat_, stat(_, _, _, StrEq("target"), _))
        .WillOnce(
            [](const char *, int, const char *, const char *, struct stat *)
            {
                errno = ENOENT;
                return -1;
            }); // [Pre-Assert確認_正常系] - stat() が "target" と等しい文字列を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - stat() から、errno に ENOENT を設定して -1 を返す。
    EXPECT_CALL(mock_sys_stat_, mkdir(_, _, _, StrEq("target"), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - stat() の後の mkdir() が "target" と等しい文字列を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - mkdir() から 0 を返す。

    // Act
    int result = cplat_makedirs(
        "target", nullptr); // [手順] - stat() が失敗した後に mkdir() が成功する状態で cplat_makedirs() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_makedirs() の戻り値が CPLAT_OK であること。
}

// mkdir と競合した後の再確認で対象を検出した場合に成功することの確認
TEST_F(makedirsTest, returns_success_when_target_appears_after_mkdir_failure)
{
    // Arrange
    InSequence sequence;

    // Pre-Assert
    EXPECT_CALL(mock_sys_stat_, stat(_, _, _, StrEq("target"), _))
        .WillOnce(
            [](const char *, int, const char *, const char *, struct stat *)
            {
                errno = ENOENT;
                return -1;
            }); // [Pre-Assert確認_正常系] - mkdir() の前の stat() が "target" と等しい文字列を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - stat() から、mkdir() の前の呼び出しに対して errno に ENOENT を設定して -1 を返す。
    EXPECT_CALL(mock_sys_stat_, mkdir(_, _, _, StrEq("target"), _))
        .WillOnce(
            [](const char *, int, const char *, const char *, mode_t)
            {
                errno = EEXIST;
                return -1;
            }); // [Pre-Assert確認_正常系] - 1 回目の stat() の後の mkdir() が "target" と等しい文字列を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - mkdir() から、errno に EEXIST を設定して -1 を返す。
    EXPECT_CALL(mock_sys_stat_, stat(_, _, _, StrEq("target"), _))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - mkdir() の後の stat() が "target" と等しい文字列を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - stat() から、mkdir() の後の呼び出しに対して 0 を返す。

    // Act
    int result = cplat_makedirs(
        "target",
        nullptr); // [手順] - mkdir() が失敗した後の stat() が成功する競合生成の状態で cplat_makedirs() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_makedirs() の戻り値が CPLAT_OK であること。
}

// 中間ディレクトリの生成と再確認が失敗した場合に処理を中断することの確認
TEST_F(makedirsTest, returns_unknown_when_intermediate_directory_cannot_be_created)
{
    // Arrange
    InSequence sequence;

    // Pre-Assert
    EXPECT_CALL(mock_sys_stat_, stat(_, _, _, StrEq("parent"), _))
        .WillOnce(
            [](const char *, int, const char *, const char *, struct stat *)
            {
                errno = ENOENT;
                return -1;
            }); // [Pre-Assert確認_異常系] - mkdir() の前の stat() が "parent" と等しい文字列を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - stat() から、mkdir() の前の呼び出しに対して errno に ENOENT を設定して -1 を返す。
    EXPECT_CALL(mock_sys_stat_, mkdir(_, _, _, StrEq("parent"), _))
        .WillOnce(
            [](const char *, int, const char *, const char *, mode_t)
            {
                errno = EACCES;
                return -1;
            }); // [Pre-Assert確認_異常系] - 1 回目の stat() の後の mkdir() が "parent" と等しい文字列を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - mkdir() から、errno に EACCES を設定して -1 を返す。
    EXPECT_CALL(mock_sys_stat_, stat(_, _, _, StrEq("parent"), _))
        .WillOnce(
            [](const char *, int, const char *, const char *, struct stat *)
            {
                errno = ENOENT;
                return -1;
            }); // [Pre-Assert確認_異常系] - mkdir() の後の stat() が "parent" と等しい文字列を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - stat() から、mkdir() の後の呼び出しに対して errno に ENOENT を設定して -1 を返す。

    // Act
    int result = cplat_makedirs(
        "parent/leaf",
        nullptr); // [手順] - 中間ディレクトリの stat()、mkdir()、再 stat() が失敗する状態で cplat_makedirs() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_PERMISSION_DENIED,
              result); // [確認_異常系] - cplat_makedirs() の戻り値が CPLAT_ERR_PERMISSION_DENIED であること。
}

// 競合で既存ディレクトリができた場合は成功し、通常ファイルがある場合は ENOTDIR で失敗することの確認 (Linux)
TEST_F(makedirsTest, handles_race_and_non_directory_via_mocks_on_linux)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_error detail;
    (void)mock_cplat;

    // Pre-Assert
    {
        InSequence seq;
        EXPECT_CALL(mock_sys_stat_, stat(_, _, _, StrEq("race_dir"), _))
            .WillOnce(
                [](const char *, int, const char *, const char *, struct stat *)
                {
                    errno = ENOENT;
                    return -1;
                });
        EXPECT_CALL(mock_sys_stat_, mkdir(_, _, _, StrEq("race_dir"), _))
            .WillOnce(
                [](const char *, int, const char *, const char *, mode_t)
                {
                    errno = EEXIST;
                    return -1;
                });
        EXPECT_CALL(mock_sys_stat_, stat(_, _, _, StrEq("race_dir"), _))
            .WillOnce(
                [](const char *, int, const char *, const char *, struct stat *st)
                {
                    st->st_mode = S_IFDIR | 0755;
                    return 0;
                });

        EXPECT_CALL(mock_sys_stat_, stat(_, _, _, StrEq("race_file"), _))
            .WillOnce(
                [](const char *, int, const char *, const char *, struct stat *)
                {
                    errno = ENOENT;
                    return -1;
                });
        EXPECT_CALL(mock_sys_stat_, mkdir(_, _, _, StrEq("race_file"), _))
            .WillOnce(
                [](const char *, int, const char *, const char *, mode_t)
                {
                    errno = EEXIST;
                    return -1;
                });
        EXPECT_CALL(mock_sys_stat_, stat(_, _, _, StrEq("race_file"), _))
            .WillOnce(
                [](const char *, int, const char *, const char *, struct stat *st)
                {
                    st->st_mode = S_IFREG | 0644;
                    return 0;
                });
    }

    // Act
    int rc_dir = cplat_makedirs("race_dir", &detail);
    cplat_error file_detail;
    int rc_file = cplat_makedirs("race_file", &file_detail);

    // Assert
    EXPECT_EQ(CPLAT_OK, rc_dir);
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, rc_file);
    EXPECT_EQ(1, cplat_error_is(&file_detail, CPLAT_CAUSE_NOT_A_DIRECTORY));
    EXPECT_EQ(ENOTDIR, cplat_error_get_errno(&file_detail));
}

// mkdir が EACCES で失敗し再 stat が ENOENT で失敗した場合に、中間と末尾の両方で EACCES を保持することの確認 (Linux)
TEST_F(makedirsTest, preserves_mkdir_eacces_at_intermediate_and_leaf_on_linux)
{
    // Arrange
    cplat_error detail;

    // Pre-Assert
    {
        InSequence seq;
        EXPECT_CALL(mock_sys_stat_, stat(_, _, _, StrEq("perm_dir"), _))
            .WillOnce(
                [](const char *, int, const char *, const char *, struct stat *)
                {
                    errno = ENOENT;
                    return -1;
                });
        EXPECT_CALL(mock_sys_stat_, mkdir(_, _, _, StrEq("perm_dir"), _))
            .WillOnce(
                [](const char *, int, const char *, const char *, mode_t)
                {
                    errno = EACCES;
                    return -1;
                });
        EXPECT_CALL(mock_sys_stat_, stat(_, _, _, StrEq("perm_dir"), _))
            .WillOnce(
                [](const char *, int, const char *, const char *, struct stat *)
                {
                    errno = ENOENT;
                    return -1;
                });
    }

    // Act
    int rc = cplat_makedirs("perm_dir/leaf", &detail);

    // Assert
    EXPECT_EQ(CPLAT_ERR_PERMISSION_DENIED, rc);
    EXPECT_EQ(EACCES, cplat_error_get_errno(&detail));
}

// 実ファイル システム上で書き込み権限のない親ディレクトリ配下への作成が権限不足になることの確認 (Linux 非 root)
TEST_F(makedirsTest, linux_non_root_reports_permission_denied_under_read_only_parent)
{
    // Arrange
    if (::geteuid() == 0)
    {
        GTEST_SKIP() << "root では権限不足を再現できないため省略します。";
    }
    std::string root = "/tmp/cplat_makedirs_perm_test";
    std::string nested = root + "/sub";
    std::error_code ec;
    (void)std::filesystem::remove_all(root, ec);
    ASSERT_EQ(CPLAT_OK, cplat_mkdir(root.c_str(), nullptr));
    ASSERT_EQ(0, ::chmod(root.c_str(), 0555));
    cplat_error detail;

    // Pre-Assert

    // Act
    int rc = cplat_makedirs(nested.c_str(), &detail);

    // Assert
    EXPECT_EQ(CPLAT_ERR_PERMISSION_DENIED, rc);
    EXPECT_EQ(EACCES, cplat_error_get_errno(&detail));

    // Cleanup
    (void)::chmod(root.c_str(), 0755);
    (void)std::filesystem::remove_all(root, ec);
}
#endif /* PLATFORM_LINUX */

#if defined(PLATFORM_WINDOWS)
// Windows スタイル区切りの絶対パスで再帰作成できることの確認
TEST_F(makedirsTest, windows_separator_path_creates_directory)
{
    // Arrange
    cplat_file_stat_t st;
    std::string root = "makedirsTest_windows_sep";
    std::string nested = root + "/sub/leaf";
    std::error_code ec;
    (void)std::filesystem::remove_all(root, ec);

    std::string windows_path =
        to_windows_sep(nested.c_str()); // [状態] - 区切りを '\\' に置き換えた Windows スタイルのパスとする。

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_makedirs(windows_path.c_str(),
                       nullptr); // [手順] - Windows スタイル区切りの 2 階層パスを指定して cplat_makedirs() を呼び出す。
    int actual_ret_exist =
        cplat_stat(&st, nullptr, nested.c_str()); // [手順] - 正規化後のリーフのパスを指定して cplat_stat() を呼び出す。
    int ret2 = cplat_makedirs(windows_path.c_str(),
                              nullptr); // [手順] - 既存ディレクトリを指定して cplat_makedirs() を再度呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 1 回目の cplat_makedirs() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_exist); // [確認_正常系] - 正規化後のパスに対する cplat_stat() の戻り値が CPLAT_OK であること。(リーフが存在することの確認)
    EXPECT_EQ(
        CPLAT_OK,
        ret2); // [確認_正常系] - 既存ディレクトリに対する cplat_makedirs() の戻り値が CPLAT_OK であること。(再呼び出しがべき等であることの確認)

    // Cleanup
    (void)std::filesystem::remove_all(root, ec);
}

// 260 バイト超かつ UTF-16 で MAX_PATH 未満の非 ASCII ディレクトリを作成できることの確認 (Windows)
TEST_F(makedirsTest, creates_non_ascii_directory_over_260_bytes_windows)
{
    // Arrange
    char temp_dir[PLATFORM_PATH_UTF8_MAX] = {};
    ASSERT_EQ(CPLAT_OK,
              cplat_path_get_temp_dir(temp_dir, sizeof(temp_dir), nullptr)); // [状態] - 一時ディレクトリを取得する。
    // [状態確認] - cplat_path_get_temp_dir() の戻り値が CPLAT_OK であること。
    std::string dir_name;
    for (int i = 0; i < 90; ++i)
    {
        dir_name += "\xE3\x81\x82";
    }
    std::string target =
        std::string(temp_dir) + "/" + dir_name; // [状態] - 一時ディレクトリ配下に 270 バイトの非 ASCII パスを用意する。
    (void)cplat_rmdir(target.c_str(), nullptr);

    // Pre-Assert

    // Act
    int actual_ret = cplat_makedirs(
        target.c_str(),
        nullptr); // [手順] - 260 バイト超の非 ASCII ディレクトリパスを指定して cplat_makedirs() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_makedirs() の戻り値が CPLAT_OK であること。

    // Cleanup
    (void)cplat_rmdir(target.c_str(), nullptr);
}

// 中間および末尾に通常ファイルがある場合に失敗し、既存ディレクトリがべき等に成功することの確認 (Windows)
TEST_F(makedirsTest, rejects_non_directory_and_keeps_idempotency_windows)
{
    // Arrange
    char temp_dir[PLATFORM_PATH_UTF8_MAX] = {};
    ASSERT_EQ(CPLAT_OK, cplat_path_get_temp_dir(temp_dir, sizeof(temp_dir), nullptr));
    std::string base = std::string(temp_dir) + "/cplat_makedirs_nondir_win";
    std::error_code ec;
    (void)std::filesystem::remove_all(base, ec);
    ASSERT_EQ(CPLAT_OK, cplat_makedirs(base.c_str(), nullptr));

    std::string regular_file = base + "/existing_file.txt";
    {
        std::ofstream ofs(regular_file, std::ios::binary);
        ofs << "data";
        ASSERT_TRUE(ofs.good());
    }
    std::string under_file = regular_file + "/child";
    std::string existing_dir = base + "/existing_dir";
    ASSERT_EQ(CPLAT_OK, cplat_makedirs(existing_dir.c_str(), nullptr));

    cplat_error leaf_detail;
    cplat_error mid_detail;
    cplat_file_stat_t st = {};

    // Pre-Assert

    // Act
    int rc_leaf = cplat_makedirs(regular_file.c_str(), &leaf_detail);
    int rc_mid = cplat_makedirs(under_file.c_str(), &mid_detail);
    int rc_idempotent = cplat_makedirs(existing_dir.c_str(), nullptr);
    int rc_child_stat = cplat_stat(&st, nullptr, under_file.c_str());

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, rc_leaf);
    EXPECT_EQ(1, cplat_error_is(&leaf_detail, CPLAT_CAUSE_NOT_A_DIRECTORY));
    EXPECT_EQ(ENOTDIR, cplat_error_get_errno(&leaf_detail));
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, rc_mid);
    EXPECT_EQ(1, cplat_error_is(&mid_detail, CPLAT_CAUSE_NOT_A_DIRECTORY));
    EXPECT_EQ(ENOTDIR, cplat_error_get_errno(&mid_detail));
    EXPECT_EQ(CPLAT_OK, rc_idempotent);
    EXPECT_NE(CPLAT_OK, rc_child_stat);

    // Cleanup
    (void)std::filesystem::remove_all(base, ec);
}

// 競合生成・EACCES 保持・再確認非ディレクトリを Windows のモックで確認するテスト
TEST_F(makedirsTest, handles_race_and_eacces_preservation_via_mocks_windows)
{
    // Arrange
    NiceMock<Mock_sys_stat> mock_sys_stat;
    NiceMock<Mock_direct> mock_direct;
    cplat_error eacces_detail;
    cplat_error race_dir_detail;
    cplat_error race_file_detail;

    // Pre-Assert
    {
        InSequence seq;
        // 1) 中間階層の wmkdir が EACCES で失敗し、再確認 _wstat64 が ENOENT で失敗 -> EACCES を保持
        EXPECT_CALL(mock_sys_stat, wstat64(_, _, _, testing::StrEq(L"C:/perm_mid"), _))
            .WillOnce(
                [](const char *, int, const char *, const wchar_t *, struct _stat64 *)
                {
                    errno = ENOENT;
                    return -1;
                });
        EXPECT_CALL(mock_direct, wmkdir(_, _, _, testing::StrEq(L"C:/perm_mid")))
            .WillOnce(
                [](const char *, int, const char *, const wchar_t *)
                {
                    errno = EACCES;
                    return -1;
                });
        EXPECT_CALL(mock_sys_stat, wstat64(_, _, _, testing::StrEq(L"C:/perm_mid"), _))
            .WillOnce(
                [](const char *, int, const char *, const wchar_t *, struct _stat64 *)
                {
                    errno = ENOENT;
                    return -1;
                });

        // 2) wmkdir が EEXIST で失敗し、再確認 _wstat64 がディレクトリ -> CPLAT_OK
        EXPECT_CALL(mock_sys_stat, wstat64(_, _, _, testing::StrEq(L"C:/race_dir"), _))
            .WillOnce(
                [](const char *, int, const char *, const wchar_t *, struct _stat64 *)
                {
                    errno = ENOENT;
                    return -1;
                });
        EXPECT_CALL(mock_direct, wmkdir(_, _, _, testing::StrEq(L"C:/race_dir")))
            .WillOnce(
                [](const char *, int, const char *, const wchar_t *)
                {
                    errno = EEXIST;
                    return -1;
                });
        EXPECT_CALL(mock_sys_stat, wstat64(_, _, _, testing::StrEq(L"C:/race_dir"), _))
            .WillOnce(
                [](const char *, int, const char *, const wchar_t *, struct _stat64 *st)
                {
                    st->st_mode = _S_IFDIR | 0755;
                    return 0;
                });

        // 3) wmkdir が EEXIST で失敗し、再確認 _wstat64 が通常ファイル -> CPLAT_CAUSE_NOT_A_DIRECTORY
        EXPECT_CALL(mock_sys_stat, wstat64(_, _, _, testing::StrEq(L"C:/race_file"), _))
            .WillOnce(
                [](const char *, int, const char *, const wchar_t *, struct _stat64 *)
                {
                    errno = ENOENT;
                    return -1;
                });
        EXPECT_CALL(mock_direct, wmkdir(_, _, _, testing::StrEq(L"C:/race_file")))
            .WillOnce(
                [](const char *, int, const char *, const wchar_t *)
                {
                    errno = EEXIST;
                    return -1;
                });
        EXPECT_CALL(mock_sys_stat, wstat64(_, _, _, testing::StrEq(L"C:/race_file"), _))
            .WillOnce(
                [](const char *, int, const char *, const wchar_t *, struct _stat64 *st)
                {
                    st->st_mode = _S_IFREG | 0644;
                    return 0;
                });
    }

    // Act
    int rc_eacces = cplat_makedirs("C:/perm_mid/leaf", &eacces_detail);
    int rc_race_dir = cplat_makedirs("C:/race_dir", &race_dir_detail);
    int rc_race_file = cplat_makedirs("C:/race_file", &race_file_detail);

    // Assert
    EXPECT_EQ(CPLAT_ERR_PERMISSION_DENIED, rc_eacces);
    EXPECT_EQ(EACCES, cplat_error_get_errno(&eacces_detail));
    EXPECT_EQ(CPLAT_OK, rc_race_dir);
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, rc_race_file);
    EXPECT_EQ(1, cplat_error_is(&race_file_detail, CPLAT_CAUSE_NOT_A_DIRECTORY));
    EXPECT_EQ(ENOTDIR, cplat_error_get_errno(&race_file_detail));
}
#endif /* PLATFORM_WINDOWS */
