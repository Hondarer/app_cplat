#include <testfw.h>
#include <cplat/base/result.h>
#include <cplat/console/console.h>
#include <mock_unistd.h>

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
    // [Pre-Assert確認_正常系] - write が STDOUT_FILENO を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - 要求長と同じバイト数を返却する。
#endif /* PLATFORM_LINUX */

    // Act
    int actual_ret = cplat_console_write(CPLAT_STREAM_STDOUT,
                                         "consoleWriteTest: stdout\n"); // [手順] - stdout に文字列を書き込む。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_console_write の戻り値が CPLAT_OK であること。
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
    // [Pre-Assert確認_正常系] - write が STDERR_FILENO を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - 要求長と同じバイト数を返却する。
#endif /* PLATFORM_LINUX */

    // Act
    int actual_ret = cplat_console_write(CPLAT_STREAM_STDERR,
                                         "consoleWriteTest: stderr\n"); // [手順] - stderr に文字列を書き込む。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_console_write の戻り値が CPLAT_OK であること。
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
        .Times(0); // [Pre-Assert確認_正常系] - 空文字列では write が呼び出されないこと。
#endif             /* PLATFORM_LINUX */

    // Act
    int actual_ret = cplat_console_write(CPLAT_STREAM_STDOUT,
                                         ""); // [手順] - 長さ 0 の文字列を指定して cplat_console_write を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_console_write の戻り値が CPLAT_OK であること。
}

// text に NULL を渡した場合に拒否されることの確認
TEST_F(consoleWriteTest, returns_invalid_argument_for_null_text)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret = cplat_console_write(CPLAT_STREAM_STDOUT,
                                         NULL); // [手順] - text に NULL を指定して cplat_console_write を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_console_write の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// 未定義のストリーム種別を渡した場合に拒否されることの確認
TEST_F(consoleWriteTest, returns_invalid_argument_for_unknown_stream)
{
    // Arrange
    cplat_stream unknown =
        static_cast<cplat_stream>(3); // [状態] - 列挙子に定義されていないストリーム種別 3 を用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_console_write(unknown,
                                         "text"); // [手順] - 未定義のストリーム種別で cplat_console_write を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_console_write の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

#if defined(PLATFORM_LINUX)

// 部分書き込みが繰り返されて全体が書き切られることの確認
// Windows は WriteConsoleW / fwrite を使うため、この分割ループは Linux のみに存在する
TEST_F(consoleWriteTest, repeats_write_until_all_bytes_are_written)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;

    // Pre-Assert
    EXPECT_CALL(mock_unistd, write(_, _, _, _, _, 5))
        .WillOnce(Return(2)); // [Pre-Assert確認_正常系] - write が残り 5 byte を指定して 1 回目に呼び出されること。
                              // [Pre-Assert手順] - 2 byte だけ書き込んだものとして 2 を返却する。
    EXPECT_CALL(mock_unistd, write(_, _, _, _, _, 3))
        .WillOnce(Return(3)); // [Pre-Assert確認_正常系] - write が残り 3 byte を指定して 2 回目に呼び出されること。
                              // [Pre-Assert手順] - 残り全てを書き込んだものとして 3 を返却する。

    // Act
    int actual_ret = cplat_console_write(CPLAT_STREAM_STDOUT,
                                         "abcde"); // [手順] - 5 byte の文字列を cplat_console_write で書き込む。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_console_write の戻り値が CPLAT_OK であること。
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
                        Return(-1))); // [Pre-Assert確認_異常系] - write が 1 回呼び出されること。
                                      // [Pre-Assert手順] - errno に EIO を設定し、write から -1 を返却する。

    // Act
    int actual_ret = cplat_console_write(CPLAT_STREAM_STDOUT,
                                         "abcde"); // [手順] - 5 byte の文字列を cplat_console_write で書き込む。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_console_write の戻り値が CPLAT_ERR_UNKNOWN であること。
}

// 書き込みがシグナルで中断された場合に再試行されることの確認
TEST_F(consoleWriteTest, retries_write_after_interrupt)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;

    // Pre-Assert
    EXPECT_CALL(mock_unistd, write(_, _, _, _, _, _))
        .WillOnce(DoAll(Assign(&errno, EINTR),
                        Return(-1))) // [Pre-Assert確認_正常系] - write が 2 回呼び出されること。
                                     // [Pre-Assert手順] - errno に EINTR を設定し、write から -1 を返却する。
        .WillOnce(Return(5));        // [Pre-Assert手順] - 2 回目の write から 5 を返却する。

    // Act
    int actual_ret = cplat_console_write(CPLAT_STREAM_STDOUT,
                                         "abcde"); // [手順] - 5 byte の文字列を cplat_console_write で書き込む。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret); // [確認_正常系] - 中断後に再試行した cplat_console_write の戻り値が CPLAT_OK であること。
}

#endif /* PLATFORM_LINUX */
