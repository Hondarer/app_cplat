#include <testfw.h>
#include <cplat/base/platform.h>
#include <cplat/base/result.h>
#include <cplat/crt/fcntl.h>
#include <cplat/crt/path.h>
#include <mock_cplat.h>

#if defined(PLATFORM_LINUX)
    #include <mock_fcntl.h>
#endif /* PLATFORM_LINUX */

#include <cerrno>
#include <fcntl.h>
#include <string>

using testing::_;
using testing::Assign;
using testing::DoAll;
using testing::NiceMock;
using testing::Return;
using testing::StrEq;

#if defined(PLATFORM_LINUX)

class fcntlTest : public testing::Test
{
  protected:
    NiceMock<Mock_fcntl> mock_fcntl_;
};

#else /* PLATFORM_LINUX */

class fcntlTest : public testing::Test
{
};

#endif /* PLATFORM_LINUX */

#if defined(PLATFORM_LINUX)
// 新規ファイルが作成され記述子が返ることの確認
TEST_F(fcntlTest, creates_file_and_returns_descriptor)
{
    // Arrange
    cplat_error detail; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_fcntl_, open(_, _, _, StrEq("work.bin"), O_RDWR | O_CREAT | O_TRUNC, 0644))
        .WillOnce(Return(7)); // [Pre-Assert確認_正常系] - open が work.bin と作成フラグで 1 回呼び出されること。
                              // [Pre-Assert手順] - 番兵記述子 7 を返却する。

    // Act
    int fd = cplat_open("work.bin", O_RDWR | O_CREAT | O_TRUNC, 0644,
                        &detail); // [手順] - 存在しないパスに作成フラグを指定して cplat_open を呼び出す。

    // Assert
    EXPECT_EQ(7, fd);                          // [確認_正常系] - cplat_open の戻り値が番兵記述子 7 であること。
    EXPECT_EQ(0, cplat_error_is_set(&detail)); // [確認_正常系] - detail に詳細エラーが記録されないこと。
}

// 存在しないファイルを読み取り専用で開くと失敗することの確認
TEST_F(fcntlTest, returns_minus1_for_missing_file)
{
    // Arrange
    cplat_error detail; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_fcntl_, open(_, _, _, StrEq("missing.bin"), O_RDONLY, 0))
        .WillOnce(DoAll(Assign(&errno, ENOENT),
                        Return(-1))); // [Pre-Assert確認_異常系] - open が missing.bin で 1 回呼び出されること。
                                      // [Pre-Assert手順] - errno に ENOENT を設定し、-1 を返却する。

    // Act
    int fd = cplat_open("missing.bin", O_RDONLY, 0,
                        &detail); // [手順] - 存在しないパスを読み取り専用で cplat_open に指定する。

    // Assert
    EXPECT_EQ(-1, fd); // [確認_異常系] - cplat_open の戻り値が -1 であること。
    EXPECT_EQ(1, cplat_error_is(&detail,
                                CPLAT_CAUSE_NOT_FOUND)); // [確認_異常系] - 見つからないことが要因であること。
}
#endif /* PLATFORM_LINUX */

// パスに NULL を渡した場合に EINVAL とともに失敗することの確認
TEST_F(fcntlTest, returns_minus1_for_null_path)
{
    // Arrange
    cplat_error detail; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert

    // Act
    int fd = cplat_open(NULL, O_RDONLY, 0, &detail); // [手順] - パスに NULL を指定して cplat_open を呼び出す。

    // Assert
    EXPECT_EQ(-1, fd); // [確認_異常系] - cplat_open の戻り値が -1 であること。
    EXPECT_EQ(EINVAL,
              cplat_error_get_errno(&detail)); // [確認_異常系] - cplat_error_get_errno の戻り値が EINVAL であること。
}

// detail_out に NULL を渡してもクラッシュしないことの確認
TEST_F(fcntlTest, accepts_null_detail_out)
{
    // Arrange

    // Pre-Assert

    // Act
    int fd = cplat_open(NULL, O_RDONLY, 0, NULL); // [手順] - detail_out に NULL を指定して cplat_open を呼び出す。

    // Assert
    EXPECT_EQ(-1, fd); // [確認_異常系] - cplat_open の戻り値が -1 であること。
}

#if defined(PLATFORM_LINUX)
/* シグナルによる中断は Linux 固有のため、再試行の確認は Linux でのみ実施する */
// オープンがシグナルで中断された場合に再試行されることの確認
TEST_F(fcntlTest, open_retries_after_interrupt)
{
    // Arrange
    cplat_error detail; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_fcntl_, open(_, _, _, StrEq("work.bin"), O_RDONLY, 0))
        .WillOnce(DoAll(Assign(&errno, EINTR), Return(-1)))
        .WillOnce(Return(5)); // [Pre-Assert確認_正常系] - 下位の open API が 2 回呼び出されること。
    // [Pre-Assert手順] - 下位の open API から、errno に EINTR を設定した -1 ののち記述子 5 を返却する。

    // Act
    int fd = cplat_open("work.bin", O_RDONLY, 0,
                        &detail); // [手順] - 中断ののち成功する cplat_open を呼び出す。

    // Assert
    EXPECT_EQ(5,
              fd); // [確認_正常系] - 再試行後の cplat_open の戻り値が 5 であること。
}
#endif /* PLATFORM_LINUX */

#if defined(PLATFORM_WINDOWS)

// Windows でパスがワイド文字へ変換できない場合に ENAMETOOLONG が返ることの確認
// Windows の cplat_open は _wsopen_s を呼ぶ前に cplat_utf8_to_wpath で変換するため、
// この分岐は Windows のみに存在する
TEST_F(fcntlTest, returns_enametoolong_when_path_exceeds_wide_buffer)
{
    // Arrange
    std::string long_path(PLATFORM_PATH_MAX + 1u,
                          'a'); // [状態] - PLATFORM_PATH_MAX を 1 文字超えるパスを用意する。
    cplat_error detail;         // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert

    // Act
    int fd = cplat_open(long_path.c_str(), O_RDONLY, 0,
                        &detail); // [手順] - 変換先バッファーに収まらないパスで cplat_open を呼び出す。

    // Assert
    EXPECT_EQ(-1, fd); // [確認_異常系] - cplat_open の戻り値が -1 であること。
    EXPECT_EQ(1, cplat_error_is(&detail,
                                CPLAT_CAUSE_NAME_TOO_LONG)); // [確認_異常系] - ENAMETOOLONG の要因であること。
}

#endif /* PLATFORM_WINDOWS */
