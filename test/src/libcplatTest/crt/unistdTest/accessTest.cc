#include <testfw.h>
#include <cplat/base/platform.h>
#include <cplat/crt/unistd.h>
#include <cplat/crt/path.h>
#include <mock_cplat.h>

#include <errno.h>
#include <string>

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

class accessTest : public testing::Test
{
  protected:
    NiceMock<Mock_unistd> mock_unistd_;
};

#else /* PLATFORM_LINUX */

class accessTest : public testing::Test
{
};

#endif /* PLATFORM_LINUX */

#if defined(PLATFORM_LINUX)
// 存在するファイルに対して cplat_access が 0 を返すことの確認
TEST_F(accessTest, returns_zero_for_existing_file)
{
    // Arrange
    cplat_error detail; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_unistd_, access(_, _, _, StrEq("work.bin"), CPLAT_ACCESS_FMT_F_OK))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - access が work.bin と F_OK で 1 回呼び出されること。
                              // [Pre-Assert手順] - 0 を返却する。

    // Act
    int actual_ret = cplat_access("work.bin", CPLAT_ACCESS_FMT_F_OK,
                                  &detail); // [手順] - 存在するパスに F_OK を指定して cplat_access を呼び出す。

    // Assert
    EXPECT_EQ(0, actual_ret);                  // [確認_正常系] - cplat_access の戻り値が 0 であること。
    EXPECT_EQ(0, cplat_error_is_set(&detail)); // [確認_正常系] - detail に詳細エラーが記録されないこと。
}

// 存在しないファイルに対して cplat_access が errno とともに -1 を返すことの確認
TEST_F(accessTest, returns_minus1_for_missing_file)
{
    // Arrange
    cplat_error detail; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_unistd_, access(_, _, _, StrEq("missing.bin"), CPLAT_ACCESS_FMT_F_OK))
        .WillOnce(DoAll(Assign(&errno, ENOENT),
                        Return(-1))); // [Pre-Assert確認_異常系] - access が missing.bin で 1 回呼び出されること。
                                      // [Pre-Assert手順] - errno に ENOENT を設定し、-1 を返却する。

    // Act
    int actual_ret = cplat_access("missing.bin", CPLAT_ACCESS_FMT_F_OK,
                                  &detail); // [手順] - 存在しないパスに F_OK を指定して cplat_access を呼び出す。

    // Assert
    EXPECT_EQ(-1, actual_ret); // [確認_異常系] - cplat_access の戻り値が -1 であること。
    EXPECT_EQ(1, cplat_error_is(&detail,
                                CPLAT_CAUSE_NOT_FOUND)); // [確認_異常系] - 見つからないことが要因であること。
}
#endif /* PLATFORM_LINUX */

// パスに NULL を渡した場合に cplat_access が EINVAL とともに -1 を返すことの確認
TEST_F(accessTest, returns_minus1_for_null_path)
{
    // Arrange
    cplat_error detail; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_access(NULL, CPLAT_ACCESS_FMT_F_OK,
                                  &detail); // [手順] - パスに NULL を指定して cplat_access を呼び出す。

    // Assert
    EXPECT_EQ(-1, actual_ret); // [確認_異常系] - cplat_access の戻り値が -1 であること。
    EXPECT_EQ(EINVAL,
              cplat_error_get_errno(&detail)); // [確認_異常系] - cplat_error_get_errno の戻り値が EINVAL であること。
}

#if defined(PLATFORM_WINDOWS)

// Windows でパスがワイド文字へ変換できない場合に ENAMETOOLONG が返ることの確認
// Windows の cplat_access は _waccess を呼ぶ前に cplat_utf8_to_wpath で変換するため、
// この分岐は Windows のみに存在する
TEST_F(accessTest, returns_enametoolong_when_path_exceeds_wide_buffer)
{
    // Arrange
    std::string long_path(PLATFORM_PATH_MAX + 1u,
                          'a'); // [状態] - PLATFORM_PATH_MAX を 1 文字超えるパスを用意する。
    cplat_error detail;         // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_access(long_path.c_str(), CPLAT_ACCESS_FMT_F_OK,
                                  &detail); // [手順] - 変換先バッファーに収まらないパスで cplat_access を呼び出す。

    // Assert
    EXPECT_EQ(-1, actual_ret); // [確認_異常系] - cplat_access の戻り値が -1 であること。
    EXPECT_EQ(1, cplat_error_is(&detail,
                                CPLAT_CAUSE_NAME_TOO_LONG)); // [確認_異常系] - ENAMETOOLONG の要因であること。
}

#endif /* PLATFORM_WINDOWS */
