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
#endif /* PLATFORM_LINUX */

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
    int actual_ret = cplat_makedirs(NULL, NULL); // [手順] - パスに NULL を渡して cplat_makedirs を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_makedirs の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// 空文字列パスは CPLAT_ERR_INVALID_ARGUMENT を返すことの確認
TEST_F(makedirsTest, empty_path_returns_invalid_argument)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret = cplat_makedirs("", NULL); // [手順] - 空文字列パスで cplat_makedirs を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_makedirs の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// cplat_stat が NULL 引数を拒否することの確認
TEST_F(makedirsTest, stat_rejects_null_arguments)
{
    // Arrange
    cplat_file_stat_t stat_buffer;

    // Pre-Assert

    // Act
    const int null_buffer_result = cplat_stat(NULL, NULL, "missing");  // [手順] - stat の出力先に NULL を指定する。
    const int null_path_result = cplat_stat(&stat_buffer, NULL, NULL); // [手順] - stat のパスに NULL を指定する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              null_buffer_result); // [確認_異常系] - 出力先 NULL の cplat_stat が INVALID_ARGUMENT を返すこと。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              null_path_result); // [確認_異常系] - パス NULL の cplat_stat が INVALID_ARGUMENT を返すこと。
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
            }); // [Pre-Assert確認_異常系] - stat が missing で 1 回呼び出されること。
                // [Pre-Assert手順] - errno に ENOENT を設定し、-1 を返却する。

    // Act
    const int result = cplat_stat(&stat_buffer, NULL, "missing"); // [手順] - 存在しないパスを指定して stat を呼び出す。

    // Assert
    EXPECT_NE(CPLAT_OK,
              result); // [確認_異常系] - 存在しないパスの cplat_stat が CPLAT_OK 以外を返すこと。
}
#endif /* PLATFORM_LINUX */

// cplat_mkdir が NULL パスを拒否することの確認
TEST_F(makedirsTest, mkdir_rejects_null_path)
{
    // Arrange

    // Pre-Assert

    // Act
    const int result = cplat_mkdir(NULL, NULL); // [手順] - パスに NULL を指定して mkdir を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              result); // [確認_異常系] - NULL パスの cplat_mkdir が INVALID_ARGUMENT を返すこと。
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
            }); // [Pre-Assert確認_異常系] - mkdir が existing で 1 回呼び出されること。
                // [Pre-Assert手順] - errno に EEXIST を設定し、-1 を返却する。

    // Act
    const int result = cplat_mkdir("existing", NULL); // [手順] - 既存ディレクトリを指定して mkdir を呼び出す。

    // Assert
    EXPECT_NE(CPLAT_OK,
              result); // [確認_異常系] - 既存ディレクトリの cplat_mkdir が CPLAT_OK 以外を返すこと。
}
#endif /* PLATFORM_LINUX */

// PLATFORM_PATH_MAX 以上のパスが拒否されることの確認
TEST_F(makedirsTest, overlong_path_returns_name_too_long)
{
    // Arrange
    const std::string path(PLATFORM_PATH_MAX, 'x'); // [状態] - PLATFORM_PATH_MAX 以上のパスを用意する。

    // Pre-Assert

    // Act
    const int result = cplat_makedirs(path.c_str(), NULL); // [手順] - 長過ぎるパスを指定して makedirs を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              result); // [確認_異常系] - 長過ぎるパスの cplat_makedirs が BUFFER_TOO_SMALL を返すこと。
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
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - dir1 に対する stat が 2 回呼び出されること。
                              // [Pre-Assert手順] - 1 回目は ENOENT の -1、2 回目は 0 を返却する。
    EXPECT_CALL(mock_sys_stat_, mkdir(_, _, _, StrEq("dir1"), _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - dir1 に対する mkdir が 1 回呼び出されること。
                              // [Pre-Assert手順] - 0 を返却する。

    // Act
    int actual_ret =
        cplat_makedirs("dir1", NULL);        // [手順] - 存在しない単一階層ディレクトリを cplat_makedirs で作成する。
    int ret2 = cplat_makedirs("dir1", NULL); // [手順] - 既存ディレクトリに cplat_makedirs を再呼び出しする。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 1 回目の cplat_makedirs の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              ret2); // [確認_正常系] - 2 回目の cplat_makedirs の戻り値が CPLAT_OK であり、べき等であること。
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
            }); // [Pre-Assert確認_正常系] - root に対する stat が 1 回呼び出されること。
                // [Pre-Assert手順] - errno に ENOENT を設定し、-1 を返却する。
    EXPECT_CALL(mock_sys_stat_, mkdir(_, _, _, StrEq("root"), _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - root に対する mkdir が 1 回呼び出されること。
                              // [Pre-Assert手順] - 0 を返却する。
    EXPECT_CALL(mock_sys_stat_, stat(_, _, _, StrEq("root/sub"), _))
        .WillOnce(
            [](const char *, int, const char *, const char *, struct stat *)
            {
                errno = ENOENT;
                return -1;
            }); // [Pre-Assert確認_正常系] - root/sub に対する stat が 1 回呼び出されること。
                // [Pre-Assert手順] - errno に ENOENT を設定し、-1 を返却する。
    EXPECT_CALL(mock_sys_stat_, mkdir(_, _, _, StrEq("root/sub"), _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - root/sub に対する mkdir が 1 回呼び出されること。
                              // [Pre-Assert手順] - 0 を返却する。
    EXPECT_CALL(mock_sys_stat_, stat(_, _, _, StrEq("root/sub/leaf"), _))
        .WillOnce(
            [](const char *, int, const char *, const char *, struct stat *)
            {
                errno = ENOENT;
                return -1;
            }); // [Pre-Assert確認_正常系] - root/sub/leaf に対する stat が 1 回呼び出されること。
                // [Pre-Assert手順] - errno に ENOENT を設定し、-1 を返却する。
    EXPECT_CALL(mock_sys_stat_, mkdir(_, _, _, StrEq("root/sub/leaf"), _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - root/sub/leaf に対する mkdir が 1 回呼び出されること。
                              // [Pre-Assert手順] - 0 を返却する。

    // Act
    int actual_ret =
        cplat_makedirs("root/sub/leaf",
                       NULL); // [手順] - 中間ディレクトリが存在しない 2 階層パスを cplat_makedirs で作成する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_makedirs の戻り値が CPLAT_OK であること。
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
            }); // [Pre-Assert確認_正常系] - rel に対する stat が 1 回呼び出されること。
                // [Pre-Assert手順] - errno に ENOENT を設定し、-1 を返却する。
    EXPECT_CALL(mock_sys_stat_, mkdir(_, _, _, StrEq("rel"), _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - rel に対する mkdir が 1 回呼び出されること。
                              // [Pre-Assert手順] - 0 を返却する。
    EXPECT_CALL(mock_sys_stat_, stat(_, _, _, StrEq("rel/sub"), _))
        .WillOnce(
            [](const char *, int, const char *, const char *, struct stat *)
            {
                errno = ENOENT;
                return -1;
            }); // [Pre-Assert確認_正常系] - rel/sub に対する stat が 1 回呼び出されること。
                // [Pre-Assert手順] - errno に ENOENT を設定し、-1 を返却する。
    EXPECT_CALL(mock_sys_stat_, mkdir(_, _, _, StrEq("rel/sub"), _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - rel/sub に対する mkdir が 1 回呼び出されること。
                              // [Pre-Assert手順] - 0 を返却する。

    // Act
    const int result = cplat_makedirs("rel/sub", NULL); // [手順] - 相対パスの複数階層を指定して makedirs を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - 相対パスの cplat_makedirs が CPLAT_OK を返すこと。
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
            }); // [Pre-Assert確認_正常系] - 完全なパス ///target に対する stat が 1 回呼び出されること。
                // [Pre-Assert手順] - stat で errno に ENOENT を設定し、-1 を返却する。
    EXPECT_CALL(mock_sys_stat_, mkdir(_, _, _, StrEq("///target"), _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 完全なパス ///target に対する mkdir が 1 回呼び出されること。
                              // [Pre-Assert手順] - mkdir で 0 を返却する。

    // Act
    const int result =
        cplat_makedirs("///target", NULL); // [手順] - ルート直後に区切り文字が連続するパスで makedirs を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_makedirs の戻り値が CPLAT_OK であること。
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
            }); // [Pre-Assert確認_正常系] - target に対する stat が 1 回呼び出されること。
                // [Pre-Assert手順] - stat で errno に ENOENT を設定し、-1 を返却する。
    EXPECT_CALL(mock_sys_stat_, mkdir(_, _, _, StrEq("target"), _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - stat の後に target に対する mkdir が 1 回呼び出されること。
                              // [Pre-Assert手順] - mkdir で 0 を返却する。

    // Act
    int result = cplat_makedirs("target", NULL); // [手順] - stat 失敗後に mkdir が成功する条件で makedirs を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_makedirs の戻り値が CPLAT_OK であること。
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
            }); // [Pre-Assert確認_正常系] - mkdir の前に target に対する stat が 1 回呼び出されること。
                // [Pre-Assert手順] - mkdir の前の stat で errno に ENOENT を設定し、-1 を返却する。
    EXPECT_CALL(mock_sys_stat_, mkdir(_, _, _, StrEq("target"), _))
        .WillOnce(
            [](const char *, int, const char *, const char *, mode_t)
            {
                errno = EEXIST;
                return -1;
            }); // [Pre-Assert確認_正常系] - 1 回目の stat の後に target に対する mkdir が 1 回呼び出されること。
                // [Pre-Assert手順] - mkdir で errno に EEXIST を設定し、-1 を返却する。
    EXPECT_CALL(mock_sys_stat_, stat(_, _, _, StrEq("target"), _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - mkdir の後に target に対する stat が 1 回呼び出されること。
                              // [Pre-Assert手順] - mkdir の後の stat で 0 を返却する。

    // Act
    int result =
        cplat_makedirs("target", NULL); // [手順] - mkdir 失敗後の stat が成功する競合生成の条件で makedirs を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_makedirs の戻り値が CPLAT_OK であること。
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
            }); // [Pre-Assert確認_異常系] - mkdir の前に parent に対する stat が 1 回呼び出されること。
                // [Pre-Assert手順] - mkdir の前の stat で errno に ENOENT を設定し、-1 を返却する。
    EXPECT_CALL(mock_sys_stat_, mkdir(_, _, _, StrEq("parent"), _))
        .WillOnce(
            [](const char *, int, const char *, const char *, mode_t)
            {
                errno = EACCES;
                return -1;
            }); // [Pre-Assert確認_異常系] - 1 回目の stat の後に parent に対する mkdir が 1 回呼び出されること。
                // [Pre-Assert手順] - mkdir で errno に EACCES を設定し、-1 を返却する。
    EXPECT_CALL(mock_sys_stat_, stat(_, _, _, StrEq("parent"), _))
        .WillOnce(
            [](const char *, int, const char *, const char *, struct stat *)
            {
                errno = ENOENT;
                return -1;
            }); // [Pre-Assert確認_異常系] - mkdir の後に parent に対する stat が 1 回呼び出されること。
                // [Pre-Assert手順] - mkdir の後の stat で errno に ENOENT を設定し、-1 を返却する。

    // Act
    int result = cplat_makedirs(
        "parent/leaf", NULL); // [手順] - 中間ディレクトリの stat、mkdir、再 stat が失敗する条件で makedirs を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - cplat_makedirs の戻り値が CPLAT_ERR_UNKNOWN であること。
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
    int actual_ret = cplat_makedirs(windows_path.c_str(),
                                    NULL); // [手順] - Windows スタイル区切りの 2 階層パスを cplat_makedirs で作成する。
    int actual_ret_exist = cplat_stat(&st, NULL, nested.c_str()); // [手順] - 正規化後のパスでリーフの存在を確認する。
    int ret2 = cplat_makedirs(windows_path.c_str(),
                              NULL); // [手順] - 既存ディレクトリに cplat_makedirs を再呼び出しする。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret);       // [確認_正常系] - cplat_makedirs の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_exist); // [確認_正常系] - 正規化後のパスでリーフが存在すること。
    EXPECT_EQ(
        CPLAT_OK,
        ret2); // [確認_正常系] - 既存ディレクトリに対する cplat_makedirs の戻り値が CPLAT_OK であり、べき等であること。

    // Cleanup
    (void)std::filesystem::remove_all(root, ec);
}
#endif /* PLATFORM_WINDOWS */
