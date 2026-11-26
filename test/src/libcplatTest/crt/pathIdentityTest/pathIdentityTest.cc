#include <cplat/base/platform.h>

#include <testfw.h>
#include <mock_cplat.h>
#include <cplat/base/error.h>
#include <cplat/base/result.h>
#include <cplat/crt/path.h>

#if defined(PLATFORM_LINUX)
    #include <sys/mock_stat.h>
    #include <sys/stat.h>
    #include <unistd.h>
#elif defined(PLATFORM_WINDOWS)
    #include <mock_windows.h>
#endif /* PLATFORM_ */

#include <cerrno>
#include <filesystem>
#include <fstream>
#include <string>

using testing::_;
using testing::NiceMock;
using testing::StrEq;

class pathIdentityTest : public testing::Test
{
  protected:
    // [サブ手順 名前=pathIdentityTest.SetUp]
    void SetUp() override
    {
        std::error_code ec;

        work_dir_ =
            std::string("pathIdentityTest_work_") + testing::UnitTest::GetInstance()->current_test_info()->name();
        (void)std::filesystem::remove_all(work_dir_, ec);
        ASSERT_TRUE(std::filesystem::create_directory(work_dir_, ec));
        // [状態確認] - カレント ディレクトリに、テストごとの作業用ディレクトリを作成できること。
        write_file(path("a.txt"));
        // [状態] - 作業用ディレクトリに、内容のあるファイル a.txt を作成する。
        write_file(path("b.txt"));
        // [状態] - 作業用ディレクトリに、内容のあるファイル b.txt を作成する。
        ASSERT_TRUE(std::filesystem::create_directory(path("dir"), ec));
        // [状態確認] - 作業用ディレクトリに、ディレクトリ dir を作成できること。
    }
    // [サブ手順終了]

    // [サブ手順 名前=pathIdentityTest.TearDown]
    void TearDown() override
    {
        std::error_code ec;

        (void)std::filesystem::remove_all(work_dir_, ec);
        // [状態] - 作業用ディレクトリを、中のファイルとリンクを含めて削除する。
    }
    // [サブ手順終了]

    // 作業用ディレクトリの中のパス (カレント ディレクトリからの相対パス) を返す。
    std::string path(const char *name) const
    {
        return work_dir_ + "/" + name;
    }

    // 内容のあるファイルを作成する。
    static void write_file(const std::string &file_path)
    {
        std::ofstream stream(file_path, std::ios::binary);
        stream << "data";
    }

    std::string work_dir_;
};

// lhs が NULL の場合に CPLAT_ERR_INVALID_ARGUMENT を返し、same_out を変更しないことの確認
// [サブ手順参照 名前=pathIdentityTest.SetUp]
TEST_F(pathIdentityTest, null_lhs_returns_invalid_argument)
{
    // Arrange
    int same = -1; // [状態] - same を -1 で初期化する。
    cplat_error detail;

    // Pre-Assert

    // Act
    const int rc =
        cplat_path_is_same_file(nullptr, path("a.txt").c_str(), &same,
                                &detail); // [手順] - lhs に nullptr を渡して cplat_path_is_same_file() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, rc); // [確認_異常系] - 戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(EINVAL, cplat_error_get_errno(&detail)); // [確認_異常系] - detail の errno が EINVAL であること。
    EXPECT_EQ(-1, same);                               // [確認_異常系] - same が -1 のまま変更されていないこと。
}
// [サブ手順参照 名前=pathIdentityTest.TearDown]

// rhs が NULL の場合に CPLAT_ERR_INVALID_ARGUMENT を返し、same_out を変更しないことの確認
// [サブ手順参照 名前=pathIdentityTest.SetUp]
TEST_F(pathIdentityTest, null_rhs_returns_invalid_argument)
{
    // Arrange
    int same = -1; // [状態] - same を -1 で初期化する。
    cplat_error detail;

    // Pre-Assert

    // Act
    const int rc =
        cplat_path_is_same_file(path("a.txt").c_str(), nullptr, &same,
                                &detail); // [手順] - rhs に nullptr を渡して cplat_path_is_same_file() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, rc); // [確認_異常系] - 戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(EINVAL, cplat_error_get_errno(&detail)); // [確認_異常系] - detail の errno が EINVAL であること。
    EXPECT_EQ(-1, same);                               // [確認_異常系] - same が -1 のまま変更されていないこと。
}
// [サブ手順参照 名前=pathIdentityTest.TearDown]

// same_out が NULL の場合に CPLAT_ERR_INVALID_ARGUMENT を返すことの確認
// [サブ手順参照 名前=pathIdentityTest.SetUp]
TEST_F(pathIdentityTest, null_same_out_returns_invalid_argument)
{
    // Arrange
    cplat_error detail;

    // Pre-Assert

    // Act
    const int rc = cplat_path_is_same_file(
        path("a.txt").c_str(), path("a.txt").c_str(), nullptr,
        &detail); // [手順] - same_out に nullptr を渡して cplat_path_is_same_file() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, rc); // [確認_異常系] - 戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(EINVAL, cplat_error_get_errno(&detail)); // [確認_異常系] - detail の errno が EINVAL であること。
}
// [サブ手順参照 名前=pathIdentityTest.TearDown]

// 空文字のパスの場合に CPLAT_ERR_INVALID_ARGUMENT を返し、same_out を変更しないことの確認
// [サブ手順参照 名前=pathIdentityTest.SetUp]
TEST_F(pathIdentityTest, empty_path_returns_invalid_argument)
{
    // Arrange
    int same = -1; // [状態] - same を -1 で初期化する。
    cplat_error detail;

    // Pre-Assert

    // Act
    const int rc =
        cplat_path_is_same_file(path("a.txt").c_str(), "", &same,
                                &detail); // [手順] - rhs に空文字を渡して cplat_path_is_same_file() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, rc); // [確認_異常系] - 戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(EINVAL, cplat_error_get_errno(&detail)); // [確認_異常系] - detail の errno が EINVAL であること。
    EXPECT_EQ(-1, same);                               // [確認_異常系] - same が -1 のまま変更されていないこと。
}
// [サブ手順参照 名前=pathIdentityTest.TearDown]

// 同じパスを比べた場合に同じ実体と判定することの確認
// [サブ手順参照 名前=pathIdentityTest.SetUp]
TEST_F(pathIdentityTest, same_path_is_same_file)
{
    // Arrange
    int same = 0; // [状態] - same を 0 で初期化する。
    cplat_error detail;

    // Pre-Assert

    // Act
    const int rc = cplat_path_is_same_file(
        path("a.txt").c_str(), path("a.txt").c_str(), &same,
        &detail); // [手順] - 同じパスの a.txt を 2 つ指定して cplat_path_is_same_file() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, rc);                   // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_EQ(1, same);                        // [確認_正常系] - same が 1 (同じ実体) であること。
    EXPECT_EQ(0, cplat_error_is_set(&detail)); // [確認_正常系] - detail が空の値であること。
}
// [サブ手順参照 名前=pathIdentityTest.TearDown]

// 相対パスと絶対パスが同じファイルを指す場合に同じ実体と判定することの確認
// [サブ手順参照 名前=pathIdentityTest.SetUp]
TEST_F(pathIdentityTest, relative_and_absolute_paths_are_same_file)
{
    // Arrange
    char absolute[PLATFORM_PATH_UTF8_MAX];
    int same = 0; // [状態] - same を 0 で初期化する。
    cplat_error detail;
    ASSERT_EQ(CPLAT_OK, cplat_path_get_full(absolute, sizeof(absolute), nullptr, path("a.txt").c_str()));
    // [状態確認] - a.txt の絶対パスを取得できること。

    // Pre-Assert

    // Act
    const int rc = cplat_path_is_same_file(
        path("a.txt").c_str(), absolute, &same,
        &detail); // [手順] - a.txt の相対パスと絶対パスを指定して cplat_path_is_same_file() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, rc); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_EQ(1, same);      // [確認_正常系] - same が 1 (同じ実体) であること。
}
// [サブ手順参照 名前=pathIdentityTest.TearDown]

// 異なるファイルを比べた場合に異なる実体と判定することの確認
// [サブ手順参照 名前=pathIdentityTest.SetUp]
TEST_F(pathIdentityTest, different_files_are_not_same_file)
{
    // Arrange
    int same = 1; // [状態] - same を 1 で初期化する。
    cplat_error detail;

    // Pre-Assert

    // Act
    const int rc = cplat_path_is_same_file(
        path("a.txt").c_str(), path("b.txt").c_str(), &same,
        &detail); // [手順] - 異なるファイル a.txt と b.txt を指定して cplat_path_is_same_file() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, rc); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, same);      // [確認_正常系] - same が 0 (異なる実体) であること。
}
// [サブ手順参照 名前=pathIdentityTest.TearDown]

// ハード リンクとリンク元を比べた場合に同じ実体と判定することの確認
// [サブ手順参照 名前=pathIdentityTest.SetUp]
TEST_F(pathIdentityTest, hard_link_is_same_file)
{
    // Arrange
    int same = 0; // [状態] - same を 0 で初期化する。
    cplat_error detail;
    std::error_code ec;
    std::filesystem::create_hard_link(path("a.txt"), path("hard.txt"), ec);
    ASSERT_FALSE(ec);
    // [状態確認] - a.txt のハード リンク hard.txt を作成できること (Windows は CreateHardLinkW による)。

    // Pre-Assert

    // Act
    const int rc = cplat_path_is_same_file(
        path("a.txt").c_str(), path("hard.txt").c_str(), &same,
        &detail); // [手順] - a.txt とハード リンク hard.txt を指定して cplat_path_is_same_file() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, rc); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_EQ(1, same);      // [確認_正常系] - same が 1 (同じ実体) であること。
}
// [サブ手順参照 名前=pathIdentityTest.TearDown]

// lhs が存在しない場合に CPLAT_OK を返し、異なる実体と判定することの確認
// [サブ手順参照 名前=pathIdentityTest.SetUp]
TEST_F(pathIdentityTest, missing_lhs_is_not_same_file)
{
    // Arrange
    int same = 1; // [状態] - same を 1 で初期化する。
    cplat_error detail;

    // Pre-Assert

    // Act
    const int rc = cplat_path_is_same_file(
        path("missing.txt").c_str(), path("a.txt").c_str(), &same,
        &detail); // [手順] - lhs に存在しない missing.txt を指定して cplat_path_is_same_file() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, rc);                   // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, same);                        // [確認_正常系] - same が 0 (異なる実体) であること。
    EXPECT_EQ(0, cplat_error_is_set(&detail)); // [確認_正常系] - detail が空の値であること。
}
// [サブ手順参照 名前=pathIdentityTest.TearDown]

// rhs が存在しない場合に CPLAT_OK を返し、異なる実体と判定することの確認
// [サブ手順参照 名前=pathIdentityTest.SetUp]
TEST_F(pathIdentityTest, missing_rhs_is_not_same_file)
{
    // Arrange
    int same = 1; // [状態] - same を 1 で初期化する。
    cplat_error detail;

    // Pre-Assert

    // Act
    const int rc = cplat_path_is_same_file(
        path("a.txt").c_str(), path("missing.txt").c_str(), &same,
        &detail); // [手順] - rhs に存在しない missing.txt を指定して cplat_path_is_same_file() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, rc);                   // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, same);                        // [確認_正常系] - same が 0 (異なる実体) であること。
    EXPECT_EQ(0, cplat_error_is_set(&detail)); // [確認_正常系] - detail が空の値であること。
}
// [サブ手順参照 名前=pathIdentityTest.TearDown]

// 両方が存在しない場合に CPLAT_OK を返し、異なる実体と判定することの確認
// [サブ手順参照 名前=pathIdentityTest.SetUp]
TEST_F(pathIdentityTest, both_missing_are_not_same_file)
{
    // Arrange
    int same = 1; // [状態] - same を 1 で初期化する。
    cplat_error detail;

    // Pre-Assert

    // Act
    const int rc = cplat_path_is_same_file(
        path("missing.txt").c_str(), path("missing.txt").c_str(), &same,
        &detail); // [手順] - 両方に存在しない同じパス missing.txt を指定して cplat_path_is_same_file() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, rc); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, same);      // [確認_正常系] - same が 0 (異なる実体) であること。
}
// [サブ手順参照 名前=pathIdentityTest.TearDown]

// 同じディレクトリを別の表記で比べた場合に同じ実体と判定することの確認
// [サブ手順参照 名前=pathIdentityTest.SetUp]
TEST_F(pathIdentityTest, same_directory_is_same_file)
{
    // Arrange
    int same = 0; // [状態] - same を 0 で初期化する。
    cplat_error detail;

    // Pre-Assert

    // Act
    const int rc = cplat_path_is_same_file(
        path("dir").c_str(), (path("dir") + "/../dir").c_str(), &same,
        &detail); // [手順] - ディレクトリ dir と dir/../dir を指定して cplat_path_is_same_file() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, rc); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_EQ(1, same);      // [確認_正常系] - same が 1 (同じ実体) であること。
}
// [サブ手順参照 名前=pathIdentityTest.TearDown]

// ディレクトリとファイルを比べた場合に異なる実体と判定することの確認
// [サブ手順参照 名前=pathIdentityTest.SetUp]
TEST_F(pathIdentityTest, directory_and_file_are_not_same_file)
{
    // Arrange
    int same = 1; // [状態] - same を 1 で初期化する。
    cplat_error detail;

    // Pre-Assert

    // Act
    const int rc = cplat_path_is_same_file(
        path("dir").c_str(), path("a.txt").c_str(), &same,
        &detail); // [手順] - ディレクトリ dir とファイル a.txt を指定して cplat_path_is_same_file() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, rc); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, same);      // [確認_正常系] - same が 0 (異なる実体) であること。
}
// [サブ手順参照 名前=pathIdentityTest.TearDown]

#if defined(PLATFORM_LINUX)

// シンボリック リンクとリンク先を比べた場合に同じ実体と判定することの確認 (Linux)
// [サブ手順参照 名前=pathIdentityTest.SetUp]
TEST_F(pathIdentityTest, symbolic_link_is_same_file)
{
    // Arrange
    int same = 0; // [状態] - same を 0 で初期化する。
    cplat_error detail;
    ASSERT_EQ(0, ::symlink("a.txt", path("sym.txt").c_str()));
    // [状態確認] - a.txt を指すシンボリック リンク sym.txt を作成できること。

    // Pre-Assert

    // Act
    const int rc = cplat_path_is_same_file(
        path("a.txt").c_str(), path("sym.txt").c_str(), &same,
        &detail); // [手順] - a.txt とシンボリック リンク sym.txt を指定して cplat_path_is_same_file() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, rc); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_EQ(1, same);      // [確認_正常系] - same が 1 (同じ実体) であること。
}
// [サブ手順参照 名前=pathIdentityTest.TearDown]

// 途中の要素がディレクトリでないパス (ENOTDIR) を、存在しないパスとして扱うことの確認 (Linux)
// [サブ手順参照 名前=pathIdentityTest.SetUp]
TEST_F(pathIdentityTest, not_directory_component_is_not_same_file)
{
    // Arrange
    int same = 1; // [状態] - same を 1 で初期化する。
    cplat_error detail;

    // Pre-Assert

    // Act
    const int rc = cplat_path_is_same_file(
        (path("a.txt") + "/child").c_str(), path("a.txt").c_str(), &same,
        &detail); // [手順] - ファイル a.txt の下の a.txt/child と a.txt を指定して cplat_path_is_same_file() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, rc); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, same);      // [確認_正常系] - same が 0 (異なる実体) であること。
}
// [サブ手順参照 名前=pathIdentityTest.TearDown]

// stat() が EACCES で失敗した場合に CPLAT_ERR_PERMISSION_DENIED を返すことの確認 (Linux)
// [サブ手順参照 名前=pathIdentityTest.SetUp]
TEST_F(pathIdentityTest, stat_permission_denied_returns_permission_denied)
{
    // Arrange
    NiceMock<Mock_sys_stat> mock_sys_stat;
    int same = -1; // [状態] - same を -1 で初期化する。
    cplat_error detail;

    // Pre-Assert
    EXPECT_CALL(mock_sys_stat, stat(_, _, _, StrEq("denied.txt"), _))
        .WillOnce(
            [](const char *, int, const char *, const char *, struct stat *)
            {
                errno = EACCES;
                return -1;
            }); // [Pre-Assert確認_異常系] - stat() が "denied.txt" と等しい文字列を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - stat() から、errno に EACCES を設定して -1 を返す。

    // Act
    const int rc = cplat_path_is_same_file(
        "denied.txt", path("a.txt").c_str(), &same,
        &detail); // [手順] - stat() が EACCES で失敗するパスを lhs に指定して cplat_path_is_same_file() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_PERMISSION_DENIED, rc); // [確認_異常系] - 戻り値が CPLAT_ERR_PERMISSION_DENIED であること。
    EXPECT_EQ(EACCES, cplat_error_get_errno(&detail)); // [確認_異常系] - detail の errno が EACCES であること。
    EXPECT_EQ(-1, same);                               // [確認_異常系] - same が -1 のまま変更されていないこと。
}
// [サブ手順参照 名前=pathIdentityTest.TearDown]

// stat() が ENAMETOOLONG で失敗した場合に CPLAT_ERR_BUFFER_TOO_SMALL を返すことの確認 (Linux)
// [サブ手順参照 名前=pathIdentityTest.SetUp]
TEST_F(pathIdentityTest, stat_name_too_long_returns_buffer_too_small)
{
    // Arrange
    NiceMock<Mock_sys_stat> mock_sys_stat;
    int same = -1; // [状態] - same を -1 で初期化する。
    cplat_error detail;

    // Pre-Assert
    EXPECT_CALL(mock_sys_stat, stat(_, _, _, testing::Not(StrEq("long.txt")), _))
        .Times(
            testing::
                AnyNumber()); // [Pre-Assert確認_正常系] - lhs の stat() が任意の回数呼び出されること。(lhs の実ファイルの照会は実 stat() へ委譲する)
    EXPECT_CALL(mock_sys_stat, stat(_, _, _, StrEq("long.txt"), _))
        .WillOnce(
            [](const char *, int, const char *, const char *, struct stat *)
            {
                errno = ENAMETOOLONG;
                return -1;
            }); // [Pre-Assert確認_異常系] - stat() が "long.txt" と等しい文字列を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - stat() から、errno に ENAMETOOLONG を設定して -1 を返す。

    // Act
    const int rc = cplat_path_is_same_file(
        path("a.txt").c_str(), "long.txt", &same,
        &detail); // [手順] - stat() が ENAMETOOLONG で失敗するパスを rhs に指定して cplat_path_is_same_file() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL, rc); // [確認_異常系] - 戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_EQ(ENAMETOOLONG,
              cplat_error_get_errno(&detail)); // [確認_異常系] - detail の errno が ENAMETOOLONG であること。
    EXPECT_EQ(-1, same);                       // [確認_異常系] - same が -1 のまま変更されていないこと。
}
// [サブ手順参照 名前=pathIdentityTest.TearDown]

// stat() がそのほかの errno で失敗した場合に CPLAT_ERR_UNKNOWN を返し、errno を詳細に残すことの確認 (Linux)
// [サブ手順参照 名前=pathIdentityTest.SetUp]
TEST_F(pathIdentityTest, stat_other_failure_returns_unknown)
{
    // Arrange
    NiceMock<Mock_sys_stat> mock_sys_stat;
    int same = -1; // [状態] - same を -1 で初期化する。
    cplat_error detail;

    // Pre-Assert
    EXPECT_CALL(mock_sys_stat, stat(_, _, _, testing::Not(StrEq("broken.txt")), _))
        .Times(
            testing::
                AnyNumber()); // [Pre-Assert確認_正常系] - lhs の stat() が任意の回数呼び出されること。(lhs の実ファイルの照会は実 stat() へ委譲する)
    EXPECT_CALL(mock_sys_stat, stat(_, _, _, StrEq("broken.txt"), _))
        .WillOnce(
            [](const char *, int, const char *, const char *, struct stat *)
            {
                errno = EIO;
                return -1;
            }); // [Pre-Assert確認_異常系] - stat() が "broken.txt" と等しい文字列を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - stat() から、errno に EIO を設定して -1 を返す。

    // Act
    const int rc = cplat_path_is_same_file(
        path("a.txt").c_str(), "broken.txt", &same,
        &detail); // [手順] - stat() が EIO で失敗するパスを rhs に指定して cplat_path_is_same_file() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, rc);               // [確認_異常系] - 戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(EIO, cplat_error_get_errno(&detail)); // [確認_異常系] - detail の errno が EIO であること。
    EXPECT_EQ(-1, same);                            // [確認_異常系] - same が -1 のまま変更されていないこと。
}
// [サブ手順参照 名前=pathIdentityTest.TearDown]

#elif defined(PLATFORM_WINDOWS)

// 大文字と小文字だけが違うパスを比べた場合に同じ実体と判定することの確認 (Windows)
// [サブ手順参照 名前=pathIdentityTest.SetUp]
TEST_F(pathIdentityTest, case_only_difference_is_same_file)
{
    // Arrange
    int same = 0; // [状態] - same を 0 で初期化する。
    cplat_error detail;

    // Pre-Assert

    // Act
    const int rc = cplat_path_is_same_file(
        path("a.txt").c_str(), path("A.TXT").c_str(), &same,
        &detail); // [手順] - a.txt と、大文字と小文字だけが違う A.TXT を指定して cplat_path_is_same_file() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, rc); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_EQ(1, same);      // [確認_正常系] - same が 1 (同じ実体) であること。
}
// [サブ手順参照 名前=pathIdentityTest.TearDown]

// CreateFileW() を、アクセス権 0、すべての共有、OPEN_EXISTING、FILE_FLAG_BACKUP_SEMANTICS で呼び出すことの確認 (Windows)
// [サブ手順参照 名前=pathIdentityTest.SetUp]
TEST_F(pathIdentityTest, opens_with_attribute_query_access_and_all_share_modes)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    int same = 0; // [状態] - same を 0 で初期化する。
    cplat_error detail;

    // Pre-Assert
    EXPECT_CALL(mock_windows, CreateFileW(_, _, _, _, 0U, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                          nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr))
        .Times(
            2); // [Pre-Assert確認_正常系] - CreateFileW() がアクセス権 0、すべての共有、OPEN_EXISTING、FILE_FLAG_BACKUP_SEMANTICS を指定して 2 回呼び出されること。
    EXPECT_CALL(mock_windows, GetFileInformationByHandleEx(_, _, _, _, FileIdInfo, _,
                                                           static_cast<DWORD>(sizeof(FILE_ID_INFO))))
        .Times(
            2); // [Pre-Assert確認_正常系] - GetFileInformationByHandleEx() が FileIdInfo と FILE_ID_INFO の大きさを指定して 2 回呼び出されること。

    // Act
    const int rc = cplat_path_is_same_file(
        path("a.txt").c_str(), path("a.txt").c_str(), &same,
        &detail); // [手順] - 同じパスの a.txt を 2 つ指定して cplat_path_is_same_file() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, rc); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_EQ(1, same);      // [確認_正常系] - same が 1 (同じ実体) であること。
}
// [サブ手順参照 名前=pathIdentityTest.TearDown]

// 不正な UTF-8 のパスの場合に CPLAT_ERR_INVALID_ENCODING を返し、ファイルを開かないことの確認 (Windows)
// [サブ手順参照 名前=pathIdentityTest.SetUp]
TEST_F(pathIdentityTest, invalid_utf8_returns_invalid_encoding)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    int same = -1; // [状態] - same を -1 で初期化する。
    cplat_error detail;

    // Pre-Assert
    EXPECT_CALL(mock_windows, CreateFileW(_, _, _, _, _, _, _, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - CreateFileW() が呼び出されないこと。

    // Act
    const int rc = cplat_path_is_same_file(
        "\x80", path("a.txt").c_str(), &same,
        &detail); // [手順] - lhs に不正な UTF-8 の "\x80" を指定して cplat_path_is_same_file() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ENCODING, rc); // [確認_異常系] - 戻り値が CPLAT_ERR_INVALID_ENCODING であること。
    EXPECT_EQ(EILSEQ, cplat_error_get_errno(&detail)); // [確認_異常系] - detail の errno が EILSEQ であること。
    EXPECT_EQ(-1, same);                               // [確認_異常系] - same が -1 のまま変更されていないこと。
}
// [サブ手順参照 名前=pathIdentityTest.TearDown]

// CreateFileW() が ERROR_ACCESS_DENIED で失敗した場合に CPLAT_ERR_PERMISSION_DENIED を返すことの確認 (Windows)
// [サブ手順参照 名前=pathIdentityTest.SetUp]
TEST_F(pathIdentityTest, create_file_access_denied_returns_permission_denied)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    int same = -1; // [状態] - same を -1 で初期化する。
    cplat_error detail;

    // Pre-Assert
    EXPECT_CALL(mock_windows, CreateFileW(_, _, _, _, _, _, _, _, _, _))
        .WillOnce(
            [](const char *, int, const char *, LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE)
            {
                SetLastError(ERROR_ACCESS_DENIED);
                return INVALID_HANDLE_VALUE;
            }); // [Pre-Assert確認_異常系] - CreateFileW() が 1 回呼び出されること。
    // [Pre-Assert手順] - CreateFileW() から、最終エラーに ERROR_ACCESS_DENIED を設定して INVALID_HANDLE_VALUE を返す。

    // Act
    const int rc = cplat_path_is_same_file(
        path("a.txt").c_str(), path("b.txt").c_str(), &same,
        &detail); // [手順] - CreateFileW() が ERROR_ACCESS_DENIED で失敗する状態で cplat_path_is_same_file() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_PERMISSION_DENIED, rc); // [確認_異常系] - 戻り値が CPLAT_ERR_PERMISSION_DENIED であること。
    EXPECT_EQ((unsigned long)ERROR_ACCESS_DENIED,
              cplat_error_get_windows_error(
                  &detail)); // [確認_異常系] - detail の Win32 エラーが ERROR_ACCESS_DENIED であること。
    EXPECT_EQ(-1, same);     // [確認_異常系] - same が -1 のまま変更されていないこと。
}
// [サブ手順参照 名前=pathIdentityTest.TearDown]

// CreateFileW() がそのほかのエラーで失敗した場合に CPLAT_ERR_UNKNOWN を返し、エラーを詳細に残すことの確認 (Windows)
// [サブ手順参照 名前=pathIdentityTest.SetUp]
TEST_F(pathIdentityTest, create_file_other_failure_returns_unknown)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    int same = -1; // [状態] - same を -1 で初期化する。
    cplat_error detail;

    // Pre-Assert
    EXPECT_CALL(mock_windows, CreateFileW(_, _, _, _, _, _, _, _, _, _))
        .WillOnce(
            [](const char *, int, const char *, LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE)
            {
                SetLastError(ERROR_SHARING_VIOLATION);
                return INVALID_HANDLE_VALUE;
            }); // [Pre-Assert確認_異常系] - CreateFileW() が 1 回呼び出されること。
    // [Pre-Assert手順] - CreateFileW() から、最終エラーに ERROR_SHARING_VIOLATION を設定して INVALID_HANDLE_VALUE を返す。

    // Act
    const int rc = cplat_path_is_same_file(
        path("a.txt").c_str(), path("b.txt").c_str(), &same,
        &detail); // [手順] - CreateFileW() が ERROR_SHARING_VIOLATION で失敗する状態で cplat_path_is_same_file() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, rc); // [確認_異常系] - 戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ((unsigned long)ERROR_SHARING_VIOLATION,
              cplat_error_get_windows_error(
                  &detail)); // [確認_異常系] - detail の Win32 エラーが ERROR_SHARING_VIOLATION であること。
    EXPECT_EQ(-1, same);     // [確認_異常系] - same が -1 のまま変更されていないこと。
}
// [サブ手順参照 名前=pathIdentityTest.TearDown]

// GetFileInformationByHandleEx() が ERROR_ACCESS_DENIED で失敗した場合に CPLAT_ERR_PERMISSION_DENIED を返し、ハンドルを閉じることの確認 (Windows)
// [サブ手順参照 名前=pathIdentityTest.SetUp]
TEST_F(pathIdentityTest, get_file_id_access_denied_returns_permission_denied)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    int same = -1; // [状態] - same を -1 で初期化する。
    cplat_error detail;

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetFileInformationByHandleEx(_, _, _, _, _, _, _))
        .WillOnce(
            [](const char *, int, const char *, HANDLE, FILE_INFO_BY_HANDLE_CLASS, LPVOID, DWORD)
            {
                SetLastError(ERROR_ACCESS_DENIED);
                return FALSE;
            }); // [Pre-Assert確認_異常系] - GetFileInformationByHandleEx() が 1 回呼び出されること。
    // [Pre-Assert手順] - GetFileInformationByHandleEx() から、最終エラーに ERROR_ACCESS_DENIED を設定して FALSE を返す。
    EXPECT_CALL(mock_windows, CloseHandle(_, _, _, _))
        .Times(1); // [Pre-Assert確認_異常系] - CloseHandle() が 1 回呼び出され、開いたハンドルを閉じること。

    // Act
    const int rc = cplat_path_is_same_file(
        path("a.txt").c_str(), path("b.txt").c_str(), &same,
        &detail); // [手順] - 識別子の取得が ERROR_ACCESS_DENIED で失敗する状態で cplat_path_is_same_file() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_PERMISSION_DENIED, rc); // [確認_異常系] - 戻り値が CPLAT_ERR_PERMISSION_DENIED であること。
    EXPECT_EQ((unsigned long)ERROR_ACCESS_DENIED,
              cplat_error_get_windows_error(
                  &detail)); // [確認_異常系] - detail の Win32 エラーが ERROR_ACCESS_DENIED であること。
    EXPECT_EQ(-1, same);     // [確認_異常系] - same が -1 のまま変更されていないこと。
}
// [サブ手順参照 名前=pathIdentityTest.TearDown]

// GetFileInformationByHandleEx() がそのほかのエラーで失敗した場合に CPLAT_ERR_UNKNOWN を返し、ハンドルを閉じることの確認 (Windows)
// [サブ手順参照 名前=pathIdentityTest.SetUp]
TEST_F(pathIdentityTest, get_file_id_other_failure_returns_unknown)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    int same = -1; // [状態] - same を -1 で初期化する。
    cplat_error detail;

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetFileInformationByHandleEx(_, _, _, _, _, _, _))
        .WillOnce(
            [](const char *, int, const char *, HANDLE, FILE_INFO_BY_HANDLE_CLASS, LPVOID, DWORD)
            {
                SetLastError(ERROR_INVALID_PARAMETER);
                return FALSE;
            }); // [Pre-Assert確認_異常系] - GetFileInformationByHandleEx() が 1 回呼び出されること。
    // [Pre-Assert手順] - GetFileInformationByHandleEx() から、最終エラーに ERROR_INVALID_PARAMETER を設定して FALSE を返す。
    EXPECT_CALL(mock_windows, CloseHandle(_, _, _, _))
        .Times(1); // [Pre-Assert確認_異常系] - CloseHandle() が 1 回呼び出され、開いたハンドルを閉じること。

    // Act
    const int rc = cplat_path_is_same_file(
        path("a.txt").c_str(), path("b.txt").c_str(), &same,
        &detail); // [手順] - 識別子の取得が ERROR_INVALID_PARAMETER で失敗する状態で cplat_path_is_same_file() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, rc); // [確認_異常系] - 戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ((unsigned long)ERROR_INVALID_PARAMETER,
              cplat_error_get_windows_error(
                  &detail)); // [確認_異常系] - detail の Win32 エラーが ERROR_INVALID_PARAMETER であること。
    EXPECT_EQ(-1, same);     // [確認_異常系] - same が -1 のまま変更されていないこと。
}
// [サブ手順参照 名前=pathIdentityTest.TearDown]

#endif /* PLATFORM_ */
