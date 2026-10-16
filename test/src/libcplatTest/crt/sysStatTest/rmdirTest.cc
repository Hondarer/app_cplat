#include <testfw.h>
#include <cplat/base/platform.h>
#include <cplat/base/result.h>
#include <cplat/crt/sys/stat.h>
#include <cerrno>

#if defined(PLATFORM_LINUX)
    #include <mock_unistd.h>
#endif /* PLATFORM_LINUX */

using testing::_;
using testing::Assign;
using testing::DoAll;
using testing::NiceMock;
using testing::Return;
using testing::StrEq;

#if defined(PLATFORM_LINUX)

class rmdirTest : public testing::Test
{
  protected:
    NiceMock<Mock_unistd> mock_unistd_;
};

#else /* PLATFORM_LINUX */

class rmdirTest : public testing::Test
{
};

#endif /* PLATFORM_LINUX */

#if defined(PLATFORM_LINUX)
// cplat_rmdir が空のディレクトリを削除することの確認
TEST_F(rmdirTest, removes_empty_directory)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_unistd_, rmdir(_, _, _, StrEq("empty_dir")))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - rmdir() が "empty_dir" と等しい文字列を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - 0 を返却する。

    // Act
    int actual_ret_rmdir = cplat_rmdir("empty_dir", nullptr); // [手順] - 空のディレクトリを削除する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_rmdir); // [確認_正常系] - cplat_rmdir() の戻り値が CPLAT_OK であること。
}

// cplat_rmdir が空でないディレクトリの削除に失敗することの確認
TEST_F(rmdirTest, fails_for_non_empty_directory)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_unistd_, rmdir(_, _, _, StrEq("non_empty_dir")))
        .WillOnce(DoAll(
            Assign(&errno, ENOTEMPTY),
            Return(
                -1))); // [Pre-Assert確認_異常系] - rmdir() が "non_empty_dir" と等しい文字列を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - errno に ENOTEMPTY を設定し、-1 を返却する。

    // Act
    int actual_ret_rmdir = cplat_rmdir("non_empty_dir", nullptr); // [手順] - 空でないディレクトリを削除する。

    // Assert
    EXPECT_NE(
        CPLAT_OK,
        actual_ret_rmdir); // [確認_異常系] - 空でないディレクトリに対する cplat_rmdir() の戻り値が CPLAT_OK 以外であること。
}
#endif /* PLATFORM_LINUX */

// cplat_rmdir が NULL に対して CPLAT_ERR_INVALID_ARGUMENT を返すことの確認
TEST_F(rmdirTest, null_path_is_rejected)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret_rmdir = cplat_rmdir(nullptr, nullptr); // [手順] - パスに NULL を指定して呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_rmdir); // [確認_異常系] - cplat_rmdir() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}
