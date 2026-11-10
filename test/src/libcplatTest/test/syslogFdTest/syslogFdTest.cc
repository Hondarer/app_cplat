#include <testfw.h>
#include <mock_cplat.h>
#if defined(PLATFORM_LINUX)
    #include <mock_stdlib.h>
    #include <mock_unistd.h>
    #include <cplat/test/syslog_test.h>
using namespace testing;

// 環境変数が設定されていれば、不正な FD でも通常の syslog 送信を抑止することの確認
TEST(syslogFdTest, rejects_invalid_fd_without_writing)
{
    // Arrange
    NiceMock<Mock_stdlib> mock_stdlib;
    NiceMock<Mock_unistd> mock_unistd;
    const char *invalid[] = {"", "abc", "-1", "2147483648", "999999999999999999999999"};
    int results[5] = {0};
    size_t index = 0;

    // Pre-Assert
    EXPECT_CALL(mock_stdlib, getenv(_, _, _, StrEq("SYSLOG_TEST_FD")))
        .WillOnce(Return(const_cast<char *>(invalid[0])))
        .WillOnce(Return(const_cast<char *>(invalid[1])))
        .WillOnce(Return(const_cast<char *>(invalid[2])))
        .WillOnce(Return(const_cast<char *>(invalid[3])))
        .WillOnce(Return(const_cast<char *>(invalid[4])));
    // [Pre-Assert確認_異常系] - 環境変数を 5 回取得すること。
    // [Pre-Assert手順] - 空、不正文字、負値、int 超過、整数変換の範囲超過を返す。
    EXPECT_CALL(mock_unistd, write(_, _, _, _, _, _)).Times(0);
    // [Pre-Assert確認_異常系] - 不正または範囲外の FD を write() へ渡さないこと。

    // Act
    for (size_t i = 0; i < 5; i++)
    {
        results[index++] = cplat_syslog_test_fd_write("x", 1);
        // [手順] - 各環境変数の値でテスト用 FD への送信を要求する。
    }

    // Assert
    for (int result : results)
    {
        EXPECT_EQ(1, result); // [確認_異常系 回数=5] - 環境変数の設定により通常の syslog 送信を省略すること。
    }
}

// 有効な FD を write へ渡すことの確認
TEST(syslogFdTest, writes_to_valid_fd)
{
    // Arrange
    NiceMock<Mock_stdlib> mock_stdlib;
    NiceMock<Mock_unistd> mock_unistd;
    const char text[] = "x";

    // Pre-Assert
    EXPECT_CALL(mock_stdlib, getenv(_, _, _, StrEq("SYSLOG_TEST_FD"))).WillOnce(Return(const_cast<char *>("42")));
    // [Pre-Assert確認_正常系] - FD の環境変数を 1 回取得すること。
    // [Pre-Assert手順] - FD 42 を返す。
    EXPECT_CALL(mock_unistd, write(_, _, _, 42, text, 1)).WillOnce(Return(1));
    // [Pre-Assert確認_正常系] - FD 42 へ 1 バイト書き込むこと。
    // [Pre-Assert手順] - 1 バイトの送信成功を返す。

    // Act
    int result = cplat_syslog_test_fd_write(text, 1); // [手順] - テスト用 FD へ送信する。

    // Assert
    EXPECT_EQ(1, result); // [確認_正常系] - 通常の syslog 送信を省略すること。
}
#endif /* PLATFORM_LINUX */
