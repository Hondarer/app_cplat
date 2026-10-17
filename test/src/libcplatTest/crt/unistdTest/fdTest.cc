#include <testfw.h>
#include <cplat/base/platform.h>
#include <cplat/crt/select.h>
#include <cplat/crt/unistd.h>
#include <mock_unistd.h>

#include <cerrno>
#include <cstring>

using testing::_;
using testing::Assign;
using testing::DoAll;
using testing::NiceMock;
using testing::Return;

namespace
{
const int kFakeFd = 7;
const int kDupFd = 8;

void invoke_cplat_fd_set_with_negative_fd()
{
    fd_set fds;
    FD_ZERO(&fds);
    CPLAT_FD_SET(-1, &fds);
}

#if defined(PLATFORM_LINUX)
void invoke_cplat_fd_set_with_out_of_range_fd()
{
    fd_set fds;
    FD_ZERO(&fds);
    CPLAT_FD_SET(FD_SETSIZE, &fds);
}
#elif defined(PLATFORM_WINDOWS)
void invoke_cplat_fd_set_with_full_set()
{
    fd_set fds;
    int index;

    FD_ZERO(&fds);
    for (index = 0; index < FD_SETSIZE; index++)
    {
        /* 相異なる値を FD_SETSIZE 個格納し、集合を満杯にする */
        CPLAT_FD_SET(index + 1, &fds);
    }
    CPLAT_FD_SET(FD_SETSIZE + 1, &fds);
}
#endif /* PLATFORM_ */
} // namespace

class fdTest : public testing::Test
{
  protected:
    NiceMock<Mock_unistd> mock_unistd_;
};

// write、lseek、read の一連の操作でデータが往復することの確認
TEST_F(fdTest, write_read_lseek_roundtrip)
{
    // Arrange
    const char data[] = "abcdef"; // [状態] - 書き込みデータを "abcdef" (6 バイト) とする。
    char buf[16] = {};

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_unistd_, write(_, _, _, kFakeFd, data, 6u))
        .WillOnce(
            Return(6)); // [Pre-Assert確認_正常系] - write() が kFakeFd、data、6u を指定して 1 回呼び出されること。
                        // [Pre-Assert手順] - write() から 6 を返す。
    EXPECT_CALL(mock_unistd_, lseek(_, _, _, kFakeFd, static_cast<off_t>(0), SEEK_SET))
        .WillOnce(Return(static_cast<off_t>(
            0))); // [Pre-Assert確認_正常系] - lseek() が kFakeFd、0、SEEK_SET を指定して 1 回呼び出されること。
                  // [Pre-Assert手順] - lseek() から位置 0 を返す。
    EXPECT_CALL(mock_unistd_, read(_, _, _, kFakeFd, buf, sizeof(buf)))
        .WillOnce(
            [](const char *, int, const char *, int, void *dest, size_t)
            {
                std::memcpy(dest, "abcdef", 6u);
                return static_cast<ssize_t>(6);
            }); // [Pre-Assert確認_正常系] - read() が kFakeFd、buf、sizeof(buf) を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - read() から、"abcdef" を格納して 6 を返す。
    EXPECT_CALL(mock_unistd_, lseek(_, _, _, kFakeFd, static_cast<off_t>(0), SEEK_END))
        .WillOnce(Return(static_cast<off_t>(
            6))); // [Pre-Assert確認_正常系] - lseek() が kFakeFd、0、SEEK_END を指定して 1 回呼び出されること。
                  // [Pre-Assert手順] - lseek() から位置 6 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_unistd_, _write(_, _, _, kFakeFd, data, 6u))
        .WillOnce(
            Return(6)); // [Pre-Assert確認_正常系] - _write() が kFakeFd、data、6u を指定して 1 回呼び出されること。
                        // [Pre-Assert手順] - _write() から 6 を返す。
    EXPECT_CALL(mock_unistd_, _lseeki64(_, _, _, kFakeFd, static_cast<__int64>(0), SEEK_SET))
        .WillOnce(Return(static_cast<__int64>(
            0))); // [Pre-Assert確認_正常系] - _lseeki64() が kFakeFd、0、SEEK_SET を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - _lseeki64() から位置 0 を返す。
    EXPECT_CALL(mock_unistd_, _read(_, _, _, kFakeFd, buf, 16u))
        .WillOnce(
            [](const char *, int, const char *, int, void *dest, unsigned int)
            {
                std::memcpy(dest, "abcdef", 6u);
                return 6;
            }); // [Pre-Assert確認_正常系] - _read() が kFakeFd、buf、16u を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - _read() から、"abcdef" を格納して 6 を返す。
    EXPECT_CALL(mock_unistd_, _lseeki64(_, _, _, kFakeFd, static_cast<__int64>(0), SEEK_END))
        .WillOnce(Return(static_cast<__int64>(
            6))); // [Pre-Assert確認_正常系] - _lseeki64() が kFakeFd、0、SEEK_END を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - _lseeki64() から位置 6 を返す。
#endif /* PLATFORM_ */

    // Act
    int64_t written = cplat_write(kFakeFd, data, 6, nullptr); // [手順] - 6 バイトを指定して cplat_write() を呼び出す。
    int64_t pos_head =
        cplat_lseek(kFakeFd, 0, SEEK_SET, nullptr); // [手順] - 先頭 (SEEK_SET) を指定して cplat_lseek() を呼び出す。
    int64_t read_len =
        cplat_read(kFakeFd, buf, sizeof(buf),
                   nullptr); // [手順] - ファイル全体を読み取れるバッファーを指定して cplat_read() を呼び出す。
    int64_t pos_end =
        cplat_lseek(kFakeFd, 0, SEEK_END, nullptr); // [手順] - 終端 (SEEK_END) を指定して cplat_lseek() を呼び出す。

    // Assert
    EXPECT_EQ(6, written);  // [確認_正常系] - cplat_write() の戻り値が 6 であること。
    EXPECT_EQ(0, pos_head); // [確認_正常系] - SEEK_SET を指定した cplat_lseek() の戻り値が 0 であること。
    EXPECT_EQ(6, read_len); // [確認_正常系] - cplat_read() の戻り値が 6 であること。
    EXPECT_EQ(
        0,
        memcmp(
            data, buf,
            6)); // [確認_正常系] - memcmp() の戻り値が 0 であること。(読み取った内容が書き込んだ内容と一致することの確認)
    EXPECT_EQ(
        6,
        pos_end); // [確認_正常系] - SEEK_END を指定した cplat_lseek() の戻り値が 6 であること。(ファイル サイズであることの確認)
}

// ファイル終端の読み取りで 0 が返ることの確認
TEST_F(fdTest, read_reaches_eof_returns_zero)
{
    // Arrange
    char buf[8]; // [状態] - 読み取り先バッファーを用意する。

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_unistd_, read(_, _, _, kFakeFd, buf, sizeof(buf)))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - read() が kFakeFd、buf、sizeof(buf) を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - read() から 0 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_unistd_, _read(_, _, _, kFakeFd, buf, 8u))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - _read() が kFakeFd、buf、8u を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - _read() から 0 を返す。
#endif /* PLATFORM_ */

    // Act
    int64_t read_len =
        cplat_read(kFakeFd, buf, sizeof(buf), nullptr); // [手順] - 空ファイルを対象に cplat_read() を呼び出す。

    // Assert
    EXPECT_EQ(0, read_len); // [確認_正常系] - cplat_read() の戻り値が 0 であること。(ファイル終端であることの確認)
}

// dup で複製した記述子へ書き込めることの確認
TEST_F(fdTest, dup_shares_file_offset)
{
    // Arrange

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_unistd_, dup(_, _, _, kFakeFd))
        .WillOnce(Return(kDupFd)); // [Pre-Assert確認_正常系] - dup() が kFakeFd を指定して 1 回呼び出されること。
                                   // [Pre-Assert手順] - dup() から複製記述子 8 を返す。
    EXPECT_CALL(mock_unistd_, write(_, _, _, kDupFd, _, 4u))
        .WillOnce(Return(4)); // [Pre-Assert確認_正常系] - write() が kDupFd、4u を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - write() から 4 を返す。
    EXPECT_CALL(mock_unistd_, lseek(_, _, _, kFakeFd, static_cast<off_t>(0), SEEK_CUR))
        .WillOnce(Return(static_cast<off_t>(
            4))); // [Pre-Assert確認_正常系] - lseek() が kFakeFd、0、SEEK_CUR を指定して 1 回呼び出されること。
                  // [Pre-Assert手順] - lseek() から位置 4 を返す。
    EXPECT_CALL(mock_unistd_, close(_, _, _, kDupFd))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - close() が kDupFd を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - close() から 0 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_unistd_, _dup(_, _, _, kFakeFd))
        .WillOnce(Return(kDupFd)); // [Pre-Assert確認_正常系] - _dup() が kFakeFd を指定して 1 回呼び出されること。
                                   // [Pre-Assert手順] - _dup() から複製記述子 8 を返す。
    EXPECT_CALL(mock_unistd_, _write(_, _, _, kDupFd, _, 4u))
        .WillOnce(Return(4)); // [Pre-Assert確認_正常系] - _write() が kDupFd、4u を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - _write() から 4 を返す。
    EXPECT_CALL(mock_unistd_, _lseeki64(_, _, _, kFakeFd, static_cast<__int64>(0), SEEK_CUR))
        .WillOnce(Return(static_cast<__int64>(
            4))); // [Pre-Assert確認_正常系] - _lseeki64() が kFakeFd、0、SEEK_CUR を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - _lseeki64() から位置 4 を返す。
    EXPECT_CALL(mock_unistd_, _close(_, _, _, kDupFd))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - _close() が kDupFd を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - _close() から 0 を返す。
#endif /* PLATFORM_ */

    // Act
    int dup_fd = cplat_dup(kFakeFd, nullptr); // [手順] - ファイル記述子を指定して cplat_dup() を呼び出す。
    int64_t written = cplat_write(dup_fd, "wxyz", 4,
                                  nullptr); // [手順] - 複製側の記述子と 4 バイトを指定して cplat_write() を呼び出す。
    int64_t pos = cplat_lseek(kFakeFd, 0, SEEK_CUR,
                              nullptr); // [手順] - 複製元の記述子と SEEK_CUR を指定して cplat_lseek() を呼び出す。
    int actual_ret_close = cplat_close(dup_fd, nullptr); // [手順] - 複製側の記述子を指定して cplat_close() を呼び出す。

    // Assert
    EXPECT_EQ(kDupFd, dup_fd);      // [確認_正常系] - cplat_dup() の戻り値が kDupFd であること。
    EXPECT_EQ(4, written);          // [確認_正常系] - 複製側に対する cplat_write() の戻り値が 4 であること。
    EXPECT_EQ(4, pos);              // [確認_正常系] - SEEK_CUR を指定した cplat_lseek() の戻り値が 4 であること。
    EXPECT_EQ(0, actual_ret_close); // [確認_正常系] - 複製側に対する cplat_close() の戻り値が 0 であること。
}

// dup2 の成功時に 0 が返ることの確認
TEST_F(fdTest, dup2_returns_zero_on_success)
{
    // Arrange

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_unistd_, dup2(_, _, _, kFakeFd, kDupFd))
        .WillOnce(
            Return(kDupFd)); // [Pre-Assert確認_正常系] - dup2() が kFakeFd、kDupFd を指定して 1 回呼び出されること。
                             // [Pre-Assert手順] - dup2() から複製先記述子 8 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_unistd_, _dup2(_, _, _, kFakeFd, kDupFd))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - _dup2() が kFakeFd、kDupFd を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - _dup2() から 0 を返す。
#endif /* PLATFORM_ */

    // Act
    int actual_ret = cplat_dup2(kFakeFd, kDupFd,
                                nullptr); // [手順] - センチネル記述子 7 と複製先 8 を指定して cplat_dup2() を呼び出す。

    // Assert
    EXPECT_EQ(
        0,
        actual_ret); // [確認_正常系] - cplat_dup2() の戻り値が 0 であること。(POSIX/Windows とも成功時は 0 に正規化されることの確認)
}

// close の成功時に 0 が返ることの確認
TEST_F(fdTest, close_success_returns_zero)
{
    // Arrange

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_unistd_, close(_, _, _, kFakeFd))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - close() が kFakeFd を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - close() から 0 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_unistd_, _close(_, _, _, kFakeFd))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - _close() が kFakeFd を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - _close() から 0 を返す。
#endif /* PLATFORM_ */

    // Act
    int actual_ret = cplat_close(kFakeFd, nullptr); // [手順] - 開いている記述子を指定して cplat_close() を呼び出す。

    // Assert
    EXPECT_EQ(0, actual_ret); // [確認_正常系] - cplat_close() の戻り値が 0 であること。
}

// 負のファイル記述子で各関数が -1 を返すことの確認
TEST_F(fdTest, negative_fd_returns_minus1)
{
    // Arrange
    char buf[4]; // [状態] - 読み書き用バッファーを用意する。

    // Pre-Assert

    // Act
    // [手順] - 負のファイル記述子を指定して cplat_lseek()、cplat_close()、cplat_dup()、cplat_dup2()、cplat_read()、cplat_write() を呼び出す。
    int64_t actual_ret_lseek = cplat_lseek(-1, 0, SEEK_SET, nullptr);
    int actual_ret_close = cplat_close(-1, nullptr);
    int actual_ret_dup = cplat_dup(-1, nullptr);
    int actual_ret_dup2_oldfd = cplat_dup2(-1, kFakeFd, nullptr);
    int actual_ret_dup2_newfd = cplat_dup2(kFakeFd, -1, nullptr);
    int64_t actual_ret_read = cplat_read(-1, buf, sizeof(buf), nullptr);
    int64_t actual_ret_write = cplat_write(-1, buf, sizeof(buf), nullptr);

    // Assert
    EXPECT_EQ(-1,
              actual_ret_lseek); // [確認_異常系] - 負のファイル記述子に対する cplat_lseek() の戻り値が -1 であること。
    EXPECT_EQ(-1,
              actual_ret_close); // [確認_異常系] - 負のファイル記述子に対する cplat_close() の戻り値が -1 であること。
    EXPECT_EQ(-1, actual_ret_dup); // [確認_異常系] - 負のファイル記述子に対する cplat_dup() の戻り値が -1 であること。
    EXPECT_EQ(-1, actual_ret_dup2_oldfd); // [確認_異常系] - oldfd が負の cplat_dup2() の戻り値が -1 であること。
    EXPECT_EQ(-1, actual_ret_dup2_newfd); // [確認_異常系] - newfd が負の cplat_dup2() の戻り値が -1 であること。
    EXPECT_EQ(-1,
              actual_ret_read); // [確認_異常系] - 負のファイル記述子に対する cplat_read() の戻り値が -1 であること。
    EXPECT_EQ(-1,
              actual_ret_write); // [確認_異常系] - 負のファイル記述子に対する cplat_write() の戻り値が -1 であること。
}

// バッファーが NULL の場合に read / write が -1 を返すことの確認
TEST_F(fdTest, null_buf_returns_minus1)
{
    // Arrange

    // Pre-Assert

    // Act
    // [手順] - nullptr バッファーを指定して cplat_read() と cplat_write() を呼び出す。
    int64_t actual_ret_read = cplat_read(kFakeFd, nullptr, 4, nullptr);
    int64_t actual_ret_write = cplat_write(kFakeFd, nullptr, 4, nullptr);

    // Assert
    EXPECT_EQ(-1,
              actual_ret_read); // [確認_異常系] - nullptr バッファーに対する cplat_read() の戻り値が -1 であること。
    EXPECT_EQ(-1,
              actual_ret_write); // [確認_異常系] - nullptr バッファーに対する cplat_write() の戻り値が -1 であること。
}

// 定義外の whence を与えた lseek が -1 を返すことの確認
TEST_F(fdTest, lseek_invalid_whence_returns_minus1)
{
    // Arrange

    // Pre-Assert

    // Act
    int64_t actual_ret =
        cplat_lseek(kFakeFd, 0, 99, nullptr); // [手順] - 定義外の whence を指定して cplat_lseek() を呼び出す。

    // Assert
    EXPECT_EQ(
        -1, actual_ret); // [確認_異常系] - cplat_lseek() の戻り値が -1 であること。(OS の API を呼び出さないことの確認)
}

// 下位の lseek 系 API が失敗した場合に -1 が返ることの確認
TEST_F(fdTest, lseek_returns_minus1_when_platform_lseek_fails)
{
    // Arrange

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_unistd_, lseek(_, _, _, _, _, _))
        .WillOnce(Return(static_cast<off_t>(-1))); // [Pre-Assert確認_異常系] - lseek() が 1 回呼び出されること。
                                                   // [Pre-Assert手順] - lseek() から -1 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_unistd_, _lseeki64(_, _, _, _, _, _))
        .WillOnce(Return(static_cast<__int64>(-1))); // [Pre-Assert確認_異常系] - _lseeki64() が 1 回呼び出されること。
                                                     // [Pre-Assert手順] - _lseeki64() から -1 を返す。
#endif /* PLATFORM_ */

    // Act
    int64_t actual_ret =
        cplat_lseek(kFakeFd, 0, SEEK_SET, nullptr); // [手順] - 有効な引数を指定して cplat_lseek() を呼び出す。

    // Assert
    EXPECT_EQ(
        -1, actual_ret); // [確認_異常系] - cplat_lseek() の戻り値が -1 であること。(OS の失敗がそのまま返ることの確認)
}

// 下位の close 系 API が失敗した場合に -1 が返ることの確認
TEST_F(fdTest, close_returns_minus1_when_platform_close_fails)
{
    // Arrange

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_unistd_, close(_, _, _, _))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - close() が 1 回呼び出されること。
                               // [Pre-Assert手順] - close() から -1 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_unistd_, _close(_, _, _, _))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - _close() が 1 回呼び出されること。
                               // [Pre-Assert手順] - _close() から -1 を返す。
#endif /* PLATFORM_ */

    // Act
    int actual_ret = cplat_close(kFakeFd, nullptr); // [手順] - 有効な記述子を指定して cplat_close() を呼び出す。

    // Assert
    EXPECT_EQ(
        -1, actual_ret); // [確認_異常系] - cplat_close() の戻り値が -1 であること。(OS の失敗がそのまま返ることの確認)
}

// 下位の dup 系 API が失敗した場合に -1 が返ることの確認
TEST_F(fdTest, dup_returns_minus1_when_platform_dup_fails)
{
    // Arrange

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_unistd_, dup(_, _, _, _))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - dup() が 1 回呼び出されること。
                               // [Pre-Assert手順] - dup() から -1 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_unistd_, _dup(_, _, _, _))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - _dup() が 1 回呼び出されること。
                               // [Pre-Assert手順] - _dup() から -1 を返す。
#endif /* PLATFORM_ */

    // Act
    int actual_ret = cplat_dup(kFakeFd, nullptr); // [手順] - 有効な記述子を指定して cplat_dup() を呼び出す。

    // Assert
    EXPECT_EQ(-1,
              actual_ret); // [確認_異常系] - cplat_dup() の戻り値が -1 であること。(OS の失敗がそのまま返ることの確認)
}

// 下位の dup2 系 API が失敗した場合に -1 が返ることの確認
TEST_F(fdTest, dup2_returns_minus1_when_platform_dup2_fails)
{
    // Arrange

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_unistd_, dup2(_, _, _, _, _))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - dup2() が 1 回呼び出されること。
                               // [Pre-Assert手順] - dup2() から -1 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_unistd_, _dup2(_, _, _, _, _))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - _dup2() が 1 回呼び出されること。
                               // [Pre-Assert手順] - _dup2() から -1 を返す。
#endif /* PLATFORM_ */

    // Act
    int actual_ret = cplat_dup2(kFakeFd, kFakeFd, nullptr); // [手順] - 有効な記述子を指定して cplat_dup2() を呼び出す。

    // Assert
    EXPECT_EQ(-1,
              actual_ret); // [確認_異常系] - cplat_dup2() の戻り値が -1 であること。(OS の失敗がそのまま返ることの確認)
}

// 下位の read 系 API が失敗した場合に -1 が返ることの確認
TEST_F(fdTest, read_returns_minus1_when_platform_read_fails)
{
    // Arrange
    char buf[4];

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_unistd_, read(_, _, _, _, _, _))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - read() が 1 回呼び出されること。
                               // [Pre-Assert手順] - read() から -1 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_unistd_, _read(_, _, _, _, _, _))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - _read() が 1 回呼び出されること。
                               // [Pre-Assert手順] - _read() から -1 を返す。
#endif /* PLATFORM_ */

    // Act
    int64_t actual_ret =
        cplat_read(kFakeFd, buf, sizeof(buf), nullptr); // [手順] - 有効な引数を指定して cplat_read() を呼び出す。

    // Assert
    EXPECT_EQ(-1,
              actual_ret); // [確認_異常系] - cplat_read() の戻り値が -1 であること。(OS の失敗がそのまま返ることの確認)
}

// 下位の write 系 API が失敗した場合に -1 が返ることの確認
TEST_F(fdTest, write_returns_minus1_when_platform_write_fails)
{
    // Arrange
    const char buf[4] = "abc";

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_unistd_, write(_, _, _, _, _, _))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - write() が 1 回呼び出されること。
                               // [Pre-Assert手順] - write() から -1 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_unistd_, _write(_, _, _, _, _, _))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - _write() が 1 回呼び出されること。
                               // [Pre-Assert手順] - _write() から -1 を返す。
#endif /* PLATFORM_ */

    // Act
    int64_t actual_ret =
        cplat_write(kFakeFd, buf, sizeof(buf), nullptr); // [手順] - 有効な引数を指定して cplat_write() を呼び出す。

    // Assert
    EXPECT_EQ(
        -1, actual_ret); // [確認_異常系] - cplat_write() の戻り値が -1 であること。(OS の失敗がそのまま返ることの確認)
}

// 追加できる FD が集合へ格納されることの確認
TEST_F(fdTest, fd_set_adds_fd_within_range)
{
    // Arrange
    fd_set fds;
    FD_ZERO(&fds); // [状態] - 空の FD 集合を用意する。

    // Pre-Assert

    // Act
    CPLAT_FD_SET(FD_SETSIZE - 1, &fds); // [手順] - 追加できる FD を指定して CPLAT_FD_SET() を呼び出す。

    // Assert
    EXPECT_TRUE(FD_ISSET(
        FD_SETSIZE - 1,
        &fds)); // [確認_正常系] - FD_ISSET(FD_SETSIZE - 1, &fds) が true であること。(追加した FD が集合に格納されていることの確認)
}

// 負の FD を渡すと abort することの確認
TEST_F(fdTest, fd_set_aborts_for_negative_fd)
{
    // Arrange

    // Pre-Assert

    // Act

    // Assert
    EXPECT_DEATH(
        invoke_cplat_fd_set_with_negative_fd(),
        "CPLAT_ASSERT failed"); // [確認_異常系] - invoke_cplat_fd_set_with_negative_fd() がプロセスを終了させ、出力が "CPLAT_ASSERT failed" を含むこと。(負の FD を範囲検査付き FD_SET へ渡すと abort することの確認)
}

#if defined(PLATFORM_LINUX)
/* Linux の fd_set は FD をインデックスとして扱うため、上限の確認は FD の値で行う */
// FD_SETSIZE 以上の FD を渡すと abort することの確認
TEST_F(fdTest, fd_set_aborts_for_fd_out_of_range)
{
    // Arrange

    // Pre-Assert

    // Act

    // Assert
    EXPECT_DEATH(
        invoke_cplat_fd_set_with_out_of_range_fd(),
        "CPLAT_ASSERT failed"); // [確認_異常系] - invoke_cplat_fd_set_with_out_of_range_fd() がプロセスを終了させ、出力が "CPLAT_ASSERT failed" を含むこと。(FD_SETSIZE と同じ FD を範囲検査付き FD_SET へ渡すと abort することの確認)
}
#elif defined(PLATFORM_WINDOWS)
/* Windows の fd_set は SOCKET の配列であり、上限は値ではなく格納数で決まる */
// 満杯の集合へ追加すると abort することの確認
TEST_F(fdTest, fd_set_aborts_when_set_is_full)
{
    // Arrange

    // Pre-Assert

    // Act

    // Assert
    EXPECT_DEATH(
        invoke_cplat_fd_set_with_full_set(),
        "CPLAT_ASSERT failed"); // [確認_異常系] - invoke_cplat_fd_set_with_full_set() がプロセスを終了させ、出力が "CPLAT_ASSERT failed" を含むこと。(FD_SETSIZE 個を格納した集合へ、さらに FD を追加すると abort することの確認)
}
#endif /* PLATFORM_ */

#if defined(PLATFORM_LINUX)
/* シグナルによる中断は Linux 固有のため、再試行の確認は Linux でのみ実施する */
// 下位の read 系 API がシグナルで中断された場合に再試行されることの確認
TEST_F(fdTest, read_retries_after_interrupt)
{
    // Arrange
    char buf[4];

    // Pre-Assert
    EXPECT_CALL(mock_unistd_, read(_, _, _, _, _, _))
        .WillOnce(DoAll(Assign(&errno, EINTR), Return(-1)))
        .WillOnce(Return(4)); // [Pre-Assert確認_正常系] - read() が 2 回呼び出されること。
    // [Pre-Assert手順] - read() から 1 回目は errno に EINTR を設定して -1、2 回目は 4 を返す。

    // Act
    int64_t actual_ret =
        cplat_read(kFakeFd, buf, sizeof(buf), nullptr); // [手順] - 有効な引数を指定して cplat_read() を呼び出す。

    // Assert
    EXPECT_EQ(
        (int64_t)4,
        actual_ret); // [確認_正常系] - 再試行後の cplat_read() の戻り値が 4 であること。(再試行後の転送量であることの確認)
}

// 下位の write 系 API がシグナルで中断された場合に再試行されることの確認
TEST_F(fdTest, write_retries_after_interrupt)
{
    // Arrange
    const char buf[4] = "abc";

    // Pre-Assert
    EXPECT_CALL(mock_unistd_, write(_, _, _, _, _, _))
        .WillOnce(DoAll(Assign(&errno, EINTR), Return(-1)))
        .WillOnce(Return(4)); // [Pre-Assert確認_正常系] - write() が 2 回呼び出されること。
    // [Pre-Assert手順] - write() から 1 回目は errno に EINTR を設定して -1、2 回目は 4 を返す。

    // Act
    int64_t actual_ret =
        cplat_write(kFakeFd, buf, sizeof(buf), nullptr); // [手順] - 有効な引数を指定して cplat_write() を呼び出す。

    // Assert
    EXPECT_EQ(
        (int64_t)4,
        actual_ret); // [確認_正常系] - 再試行後の cplat_write() の戻り値が 4 であること。(再試行後の転送量であることの確認)
}

/* Windows の CRT はクローズ済み記述子で invalid parameter handler を起動するため、Linux でのみ実施する */
// クローズ済みの記述子で各関数が -1 を返すことの確認
TEST_F(fdTest, closed_fd_operations_return_minus1)
{
    // Arrange
    char buf[4];

    // Pre-Assert
    EXPECT_CALL(mock_unistd_, lseek(_, _, _, kFakeFd, _, _))
        .WillOnce(Return(
            static_cast<off_t>(-1))); // [Pre-Assert確認_異常系] - lseek() が kFakeFd を指定して 1 回呼び出されること。
                                      // [Pre-Assert手順] - lseek() から -1 を返す。
    EXPECT_CALL(mock_unistd_, read(_, _, _, kFakeFd, _, _))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - read() が kFakeFd を指定して 1 回呼び出されること。
                               // [Pre-Assert手順] - read() から -1 を返す。
    EXPECT_CALL(mock_unistd_, write(_, _, _, kFakeFd, _, _))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - write() が kFakeFd を指定して 1 回呼び出されること。
                               // [Pre-Assert手順] - write() から -1 を返す。
    EXPECT_CALL(mock_unistd_, dup(_, _, _, kFakeFd))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - dup() が kFakeFd を指定して 1 回呼び出されること。
                               // [Pre-Assert手順] - dup() から -1 を返す。
    EXPECT_CALL(mock_unistd_, dup2(_, _, _, kFakeFd, kFakeFd))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - dup2() が kFakeFd、kFakeFd を指定して 1 回呼び出されること。
                               // [Pre-Assert手順] - dup2() から -1 を返す。
    EXPECT_CALL(mock_unistd_, close(_, _, _, kFakeFd))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - close() が kFakeFd を指定して 1 回呼び出されること。
                               // [Pre-Assert手順] - close() から -1 を返す。

    // Act
    // [手順] - クローズ済みのファイル記述子で lseek()、read()、write()、dup()、dup2()、close() を呼び出す。
    int64_t actual_ret_lseek = cplat_lseek(kFakeFd, 0, SEEK_SET, nullptr);
    int64_t actual_ret_read = cplat_read(kFakeFd, buf, sizeof(buf), nullptr);
    int64_t actual_ret_write = cplat_write(kFakeFd, buf, sizeof(buf), nullptr);
    int actual_ret_dup = cplat_dup(kFakeFd, nullptr);
    int actual_ret_dup2 = cplat_dup2(kFakeFd, kFakeFd, nullptr);
    int actual_ret_close = cplat_close(kFakeFd, nullptr);

    // Assert
    EXPECT_EQ(-1,
              actual_ret_lseek); // [確認_異常系] - クローズ済み記述子に対する cplat_lseek() の戻り値が -1 であること。
    EXPECT_EQ(-1,
              actual_ret_read); // [確認_異常系] - クローズ済み記述子に対する cplat_read() の戻り値が -1 であること。
    EXPECT_EQ(-1,
              actual_ret_write); // [確認_異常系] - クローズ済み記述子に対する cplat_write() の戻り値が -1 であること。
    EXPECT_EQ(-1, actual_ret_dup); // [確認_異常系] - クローズ済み記述子に対する cplat_dup() の戻り値が -1 であること。
    EXPECT_EQ(-1,
              actual_ret_dup2); // [確認_異常系] - クローズ済み記述子に対する cplat_dup2() の戻り値が -1 であること。
    EXPECT_EQ(-1,
              actual_ret_close); // [確認_異常系] - クローズ済み記述子に対する cplat_close() の戻り値が -1 であること。
}
#endif /* PLATFORM_LINUX */
