#include <testfw.h>
#include <cplat/base/result.h>
#include <cplat/console/console.h>
#include <mock_cplat.h>
#include <mock_unistd.h>

#if defined(PLATFORM_WINDOWS)
    #include <mock_windows.h>
#endif

#include <errno.h>
#if defined(PLATFORM_LINUX)
    #include <unistd.h>
#endif

class consoleWriteTest : public Test
{
};

// stdout への書き込みが成功することの確認
TEST_F(consoleWriteTest, writes_to_stdout)
{
    // Arrange
#if defined(PLATFORM_LINUX)
    NiceMock<Mock_unistd> mock_unistd;
    const char text[] = "consoleWriteTest: stdout\n";
#endif /* PLATFORM_LINUX */

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_unistd, write(_, _, _, STDOUT_FILENO, _, sizeof(text) - 1U))
        .WillOnce(Return(static_cast<ssize_t>(sizeof(text) - 1U)));
    // [Pre-Assert確認_正常系] - write() が STDOUT_FILENO、sizeof(text) - 1U を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - write() から、要求長と同じバイト数を返す。
#endif /* PLATFORM_LINUX */

    // Act
    int actual_ret = cplat_console_write(
        CPLAT_STREAM_STDOUT,
        "consoleWriteTest: stdout\n"); // [手順] - stdout と文字列を指定して cplat_console_write() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_console_write() の戻り値が CPLAT_OK であること。
}

// stderr への書き込みが成功することの確認
TEST_F(consoleWriteTest, writes_to_stderr)
{
    // Arrange
#if defined(PLATFORM_LINUX)
    NiceMock<Mock_unistd> mock_unistd;
    const char text[] = "consoleWriteTest: stderr\n";
#endif /* PLATFORM_LINUX */

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_unistd, write(_, _, _, STDERR_FILENO, _, sizeof(text) - 1U))
        .WillOnce(Return(static_cast<ssize_t>(sizeof(text) - 1U)));
    // [Pre-Assert確認_正常系] - write() が STDERR_FILENO、sizeof(text) - 1U を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - write() から、要求長と同じバイト数を返す。
#endif /* PLATFORM_LINUX */

    // Act
    int actual_ret = cplat_console_write(
        CPLAT_STREAM_STDERR,
        "consoleWriteTest: stderr\n"); // [手順] - stderr と文字列を指定して cplat_console_write() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_console_write() の戻り値が CPLAT_OK であること。
}

// 空文字列の書き込みが成功することの確認
TEST_F(consoleWriteTest, writes_empty_text)
{
    // Arrange
#if defined(PLATFORM_LINUX)
    NiceMock<Mock_unistd> mock_unistd;
#endif /* PLATFORM_LINUX */

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_unistd, write(_, _, _, _, _, _))
        .Times(0); // [Pre-Assert確認_正常系] - 空文字列では write() が呼び出されないこと。
#endif             /* PLATFORM_LINUX */

    // Act
    int actual_ret = cplat_console_write(CPLAT_STREAM_STDOUT,
                                         ""); // [手順] - 長さ 0 の文字列を指定して cplat_console_write() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_console_write() の戻り値が CPLAT_OK であること。
}

// text に NULL を渡した場合に拒否されることの確認
TEST_F(consoleWriteTest, returns_invalid_argument_for_null_text)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_console_write(CPLAT_STREAM_STDOUT,
                            nullptr); // [手順] - text に nullptr を指定して cplat_console_write() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_console_write() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// 未定義のストリーム種別を渡した場合に拒否されることの確認
TEST_F(consoleWriteTest, returns_invalid_argument_for_unknown_stream)
{
    // Arrange
    cplat_stream unknown =
        static_cast<cplat_stream>(3); // [状態] - 列挙子に定義されていないストリーム種別 3 を用意する。

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_console_write(unknown,
                            "text"); // [手順] - 未定義のストリーム種別で cplat_console_write() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_console_write() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

#if defined(PLATFORM_WINDOWS)

// 不正な UTF-8 を WriteFile へ切り替えずに失敗することの確認
TEST_F(consoleWriteTest, rejects_invalid_utf8_without_write_file_fallback)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    const char invalid_utf8[] = "\x80";
    HANDLE output_handle = reinterpret_cast<HANDLE>(1);

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_OUTPUT_HANDLE)).WillOnce(Return(output_handle));
    // [Pre-Assert確認_正常系] - GetStdHandle() が標準出力を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - GetStdHandle() から有効なダミー ハンドルを返す。
    EXPECT_CALL(mock_cplat, WriteConsoleU(output_handle, invalid_utf8, 1U, _, nullptr)).WillOnce(Return(FALSE));
    // [Pre-Assert確認_異常系] - WriteConsoleU() が不正な UTF-8 を受け取り 1 回失敗すること。
    // [Pre-Assert手順] - WriteConsoleU() から FALSE を返す。
    EXPECT_CALL(mock_windows, GetLastError(_, _, _)).WillOnce(Return(ERROR_NO_UNICODE_TRANSLATION));
    // [Pre-Assert確認_異常系] - GetLastError() が 1 回呼び出されること。
    // [Pre-Assert手順] - GetLastError() から ERROR_NO_UNICODE_TRANSLATION を返す。
    EXPECT_CALL(mock_windows, WriteFile(_, _, _, _, _, _, _, _)).Times(0);
    // [Pre-Assert確認_異常系] - 不正な UTF-8 のため WriteFile() へ切り替わらないこと。

    // Act
    int actual_ret = cplat_console_write(CPLAT_STREAM_STDOUT, invalid_utf8);
    // [手順] - 不正な UTF-8 を指定して cplat_console_write() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ENCODING, actual_ret);
    // [確認_異常系] - cplat_console_write() が CPLAT_ERR_INVALID_ENCODING を返すこと。
}

#endif /* PLATFORM_WINDOWS */

#if defined(PLATFORM_LINUX)

// 部分書き込みが繰り返されて全体が書き切られることの確認
// Windows は WriteConsoleW / fwrite を使うため、この分割ループは Linux のみに存在する
TEST_F(consoleWriteTest, repeats_write_until_all_bytes_are_written)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;

    // Pre-Assert
    EXPECT_CALL(mock_unistd, write(_, _, _, _, _, 5))
        .WillOnce(Return(2)); // [Pre-Assert確認_正常系] - write() が 5 を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - write() から、2 byte だけ書き込んだものとして 2 を返す。
    EXPECT_CALL(mock_unistd, write(_, _, _, _, _, 3))
        .WillOnce(Return(3)); // [Pre-Assert確認_正常系] - write() が 3 を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - write() から、残りすべてを書き込んだものとして 3 を返す。

    // Act
    int actual_ret =
        cplat_console_write(CPLAT_STREAM_STDOUT,
                            "abcde"); // [手順] - 5 byte の文字列を指定して cplat_console_write() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_console_write() の戻り値が CPLAT_OK であること。
}

// 書き込みの失敗が通知されることの確認
// Windows は WriteConsoleW / fwrite を使うため、この失敗経路は Linux のみに存在する
TEST_F(consoleWriteTest, returns_unknown_when_write_fails)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;

    // Pre-Assert
    EXPECT_CALL(mock_unistd, write(_, _, _, _, _, _))
        .WillOnce(DoAll(Assign(&errno, EIO),
                        Return(-1))); // [Pre-Assert確認_異常系] - write() が 1 回呼び出されること。
                                      // [Pre-Assert手順] - write() から、errno に EIO を設定して -1 を返す。

    // Act
    int actual_ret =
        cplat_console_write(CPLAT_STREAM_STDOUT,
                            "abcde"); // [手順] - 5 byte の文字列を指定して cplat_console_write() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_console_write() の戻り値が CPLAT_ERR_UNKNOWN であること。
}

// 書き込みがシグナルで中断された場合に再試行されることの確認
TEST_F(consoleWriteTest, retries_write_after_interrupt)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;

    // Pre-Assert
    EXPECT_CALL(mock_unistd, write(_, _, _, _, _, _))
        .WillOnce(DoAll(Assign(&errno, EINTR),
                        Return(-1))) // [Pre-Assert確認_正常系] - write() が 2 回呼び出されること。
                                     // [Pre-Assert手順] - write() から、errno に EINTR を設定して -1 を返す。
        .WillOnce(Return(5));        // [Pre-Assert手順] - write() から 2 回目は 5 を返す。

    // Act
    int actual_ret =
        cplat_console_write(CPLAT_STREAM_STDOUT,
                            "abcde"); // [手順] - 5 byte の文字列を指定して cplat_console_write() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret); // [確認_正常系] - 中断後に再試行した cplat_console_write() の戻り値が CPLAT_OK であること。
}

#endif /* PLATFORM_LINUX */
