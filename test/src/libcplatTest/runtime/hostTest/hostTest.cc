#include <testfw.h>
#include <mock_unistd.h>

#include <cplat/base/platform.h>
#include <cplat/base/result.h>
#include <cplat/runtime/host.h>

#include <errno.h>
#include <limits.h>
#include <string.h>

using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::StrEq;

#if defined(PLATFORM_WINDOWS)
    #include <mock_cplat.h>
    #include <mock_windows.h>
#endif /* PLATFORM_WINDOWS */

class hostTest : public Test
{
};

// ホスト名取得が空でない UTF-8 文字列を返すことの確認
TEST_F(hostTest, GetsHostnameSuccess)
{
    // Arrange
    char name[CPLAT_HOST_NAME_MAX];

    memset(name, 'x', sizeof(name)); // [状態] - 出力先を非空で埋める。
    name[sizeof(name) - 1] = '\0';

    // Pre-Assert

    // Act
    int actual_ret = cplat_host_get_name(name, sizeof(name)); // [手順] - 十分な容量のバッファーへホスト名を取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_host_get_name の戻り値が CPLAT_OK であること。
    EXPECT_NE('\0', name[0]);        // [確認_正常系] - 取得したホスト名が空文字列でないこと。
}

// ホスト名取得が不正な出力引数を拒否することの確認
TEST_F(hostTest, RejectsInvalidOutputArguments)
{
    // Arrange
    char name[CPLAT_HOST_NAME_MAX] = {'x'};

    // Pre-Assert

    // Act
    int actual_ret_null =
        cplat_host_get_name(NULL, sizeof(name));        // [手順] - 出力先に NULL を渡してホスト名を取得する。
    int actual_ret_zero = cplat_host_get_name(name, 0); // [手順] - 出力先サイズに 0 を渡してホスト名を取得する。
    int actual_ret_too_big = cplat_host_get_name(
        name, (size_t)INT_MAX + 1u); // [手順] - 出力先サイズに INT_MAX を超える値を渡してホスト名を取得する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_null); // [確認_異常系] - 出力先が NULL の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret_zero); // [確認_異常系] - 出力先サイズが 0 の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_too_big); // [確認_異常系] - 出力先サイズが INT_MAX 超えの戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ('x', name[0]); // [確認_異常系] - 不正引数の呼び出しで出力先が変更されないこと。
}

// ホスト名取得がバッファー不足を報告することの確認
TEST_F(hostTest, ReportsSmallBuffer)
{
    // Arrange
    char name[1] = {'x'};

    // Pre-Assert

    // Act
    int actual_ret = cplat_host_get_name(name, sizeof(name)); // [手順] - 1 バイトの出力先へホスト名を取得する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              actual_ret);    // [確認_異常系] - cplat_host_get_name の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_EQ('\0', name[0]); // [確認_異常系] - バッファー不足時に出力先が空文字列であること。
}

#if defined(PLATFORM_LINUX)
// mock 化した gethostname の成功値が呼び出し側へ写されることの確認
TEST_F(hostTest, CopiesMockedGethostname)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    char name[CPLAT_HOST_NAME_MAX] = {};

    // Pre-Assert
    EXPECT_CALL(mock_unistd, gethostname(_, _, _, _, _))
        .WillOnce(Invoke(
            [](const char *, int, const char *, char *buf, size_t len)
            {
                const char *host = "testhost";
                size_t host_len = strlen(host);

                if (len <= host_len)
                {
                    return -1;
                }
                memcpy(buf, host, host_len + 1u);
                return 0;
            })); // [Pre-Assert確認_正常系] - gethostname が 1 回呼び出されること。
                 // [Pre-Assert手順] - "testhost" を書き込み 0 を返却する。

    // Act
    int actual_ret =
        cplat_host_get_name(name, sizeof(name)); // [手順] - mock 化した gethostname からホスト名を取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_host_get_name の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("testhost", name);  // [確認_正常系] - 取得したホスト名が testhost であること。
}

// gethostname の ENAMETOOLONG がバッファー不足へ変換されることの確認
TEST_F(hostTest, ReportsGethostnameNameTooLong)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    char name[CPLAT_HOST_NAME_MAX] = {'x'};

    errno = ENAMETOOLONG; // [状態] - gethostname 失敗時の errno を ENAMETOOLONG にする。

    // Pre-Assert
    EXPECT_CALL(mock_unistd, gethostname(_, _, _, _, _))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - gethostname が 1 回呼び出されること。
                               // [Pre-Assert手順] - gethostname から -1 を返却する。

    // Act
    int actual_ret =
        cplat_host_get_name(name, sizeof(name)); // [手順] - ENAMETOOLONG で失敗する状態でホスト名を取得する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              actual_ret);    // [確認_異常系] - ENAMETOOLONG が BUFFER_TOO_SMALL へ変換されること。
    EXPECT_EQ('\0', name[0]); // [確認_異常系] - 失敗時に出力先が空文字列であること。
}

// gethostname のその他の OS エラーが共通結果へ変換されることの確認
TEST_F(hostTest, ReportsGethostnameOsError)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    char name[CPLAT_HOST_NAME_MAX] = {'x'};

    errno = EACCES; // [状態] - gethostname 失敗時の errno を EACCES にする。

    // Pre-Assert
    EXPECT_CALL(mock_unistd, gethostname(_, _, _, _, _))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - gethostname が 1 回呼び出されること。
                               // [Pre-Assert手順] - gethostname から -1 を返却する。

    // Act
    int actual_ret = cplat_host_get_name(name, sizeof(name)); // [手順] - EACCES で失敗する状態でホスト名を取得する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_PERMISSION_DENIED,
              actual_ret);    // [確認_異常系] - EACCES が PERMISSION_DENIED へ変換されること。
    EXPECT_EQ('\0', name[0]); // [確認_異常系] - 失敗時に出力先が空文字列であること。
}
#elif defined(PLATFORM_WINDOWS)
// mock 化した GetComputerNameExW の成功値が UTF-8 で写されることの確認
TEST_F(hostTest, CopiesMockedComputerName)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    char name[CPLAT_HOST_NAME_MAX] = {};

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetComputerNameExW(_, _, _, ComputerNameDnsHostname, _, _))
        .WillOnce(Invoke(
            [](const char *, int, const char *, COMPUTER_NAME_FORMAT, LPWSTR buffer, LPDWORD size)
            {
                const wchar_t kName[] = L"testhost";
                DWORD needed = (DWORD)(sizeof(kName) / sizeof(kName[0]));

                if (buffer == nullptr || size == nullptr || *size < needed)
                {
                    if (size != nullptr)
                    {
                        *size = needed;
                    }
                    return FALSE;
                }
                memcpy(buffer, kName, sizeof(kName));
                *size = needed - 1u;
                return TRUE;
            })); // [Pre-Assert確認_正常系] - GetComputerNameExW が DNS ホスト名を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - L"testhost" を書き込み成功を返却する。
    EXPECT_CALL(mock_cplat, cplat_wstr_to_utf8(_, sizeof(name), StrEq(L"testhost")))
        .WillOnce(Invoke(
            [](char *dest, size_t dest_size, const wchar_t *)
            {
                const char kName[] = "testhost";

                if (dest_size < sizeof(kName))
                {
                    return 0;
                }
                memcpy(dest, kName, sizeof(kName));
                return (int)sizeof(kName);
            })); // [Pre-Assert確認_正常系] - UTF-16 のホスト名が UTF-8 変換に 1 回渡されること。
                 // [Pre-Assert手順] - "testhost" を出力し、変換後のバイト数を返却する。

    // Act
    int actual_ret =
        cplat_host_get_name(name, sizeof(name)); // [手順] - mock 化した GetComputerNameExW からホスト名を取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_host_get_name の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("testhost", name);  // [確認_正常系] - 取得したホスト名が testhost であること。
}

// UTF-8 変換の OS エラーが共通結果へ変換されることの確認
TEST_F(hostTest, ReportsWstrToUtf8OsError)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    char name[CPLAT_HOST_NAME_MAX] = {'x'};

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetComputerNameExW(_, _, _, ComputerNameDnsHostname, _, _))
        .WillOnce(Invoke(
            [](const char *, int, const char *, COMPUTER_NAME_FORMAT, LPWSTR buffer, LPDWORD size)
            {
                const wchar_t kName[] = L"testhost";

                memcpy(buffer, kName, sizeof(kName));
                *size = (DWORD)(sizeof(kName) / sizeof(kName[0])) - 1u;
                return TRUE;
            })); // [Pre-Assert確認_異常系] - GetComputerNameExW が DNS ホスト名を指定して 1 回呼び出されること。
                 // [Pre-Assert手順] - L"testhost" を書き込み成功を返却する。
    EXPECT_CALL(mock_cplat, cplat_wstr_to_utf8(_, sizeof(name), StrEq(L"testhost")))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - UTF-16 のホスト名が UTF-8 変換に 1 回渡されること。
                              // [Pre-Assert手順] - UTF-8 変換から 0 を返却する。
    EXPECT_CALL(mock_windows, GetLastError(_, _, _))
        .WillOnce(Return(
            ERROR_ACCESS_DENIED)); // [Pre-Assert確認_異常系] - 変換失敗時に GetLastError が 1 回呼び出されること。
                                   // [Pre-Assert手順] - ERROR_ACCESS_DENIED を返却する。

    // Act
    int actual_ret = cplat_host_get_name(name, sizeof(name)); // [手順] - UTF-8 変換が失敗する状態でホスト名を取得する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_PERMISSION_DENIED,
              actual_ret);    // [確認_異常系] - ERROR_ACCESS_DENIED が PERMISSION_DENIED へ変換されること。
    EXPECT_EQ('\0', name[0]); // [確認_異常系] - UTF-8 変換失敗時に出力先が空文字列であること。
}

// GetComputerNameExW の ERROR_MORE_DATA がバッファー不足へ変換されることの確認
TEST_F(hostTest, ReportsComputerNameMoreData)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    char name[CPLAT_HOST_NAME_MAX] = {'x'};

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetComputerNameExW(_, _, _, ComputerNameDnsHostname, _, _))
        .WillOnce(Return(FALSE)); // [Pre-Assert確認_異常系] - GetComputerNameExW が 1 回呼び出されること。
                                  // [Pre-Assert手順] - FALSE を返却する。
    EXPECT_CALL(mock_windows, GetLastError(_, _, _))
        .WillOnce(Return(ERROR_MORE_DATA)); // [Pre-Assert確認_異常系] - GetLastError が 1 回呼び出されること。
                                            // [Pre-Assert手順] - ERROR_MORE_DATA を返却する。

    // Act
    int actual_ret =
        cplat_host_get_name(name, sizeof(name)); // [手順] - ERROR_MORE_DATA で失敗する状態でホスト名を取得する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              actual_ret);    // [確認_異常系] - ERROR_MORE_DATA が BUFFER_TOO_SMALL へ変換されること。
    EXPECT_EQ('\0', name[0]); // [確認_異常系] - 失敗時に出力先が空文字列であること。
}

// GetComputerNameExW のその他の OS エラーが共通結果へ変換されることの確認
TEST_F(hostTest, ReportsComputerNameOsError)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    char name[CPLAT_HOST_NAME_MAX] = {'x'};

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetComputerNameExW(_, _, _, ComputerNameDnsHostname, _, _))
        .WillOnce(Return(FALSE)); // [Pre-Assert確認_異常系] - GetComputerNameExW が 1 回呼び出されること。
                                  // [Pre-Assert手順] - FALSE を返却する。
    EXPECT_CALL(mock_windows, GetLastError(_, _, _))
        .WillOnce(Return(ERROR_ACCESS_DENIED)); // [Pre-Assert確認_異常系] - GetLastError が 1 回呼び出されること。
                                                // [Pre-Assert手順] - ERROR_ACCESS_DENIED を返却する。

    // Act
    int actual_ret =
        cplat_host_get_name(name, sizeof(name)); // [手順] - ERROR_ACCESS_DENIED で失敗する状態でホスト名を取得する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_PERMISSION_DENIED,
              actual_ret);    // [確認_異常系] - ERROR_ACCESS_DENIED が PERMISSION_DENIED へ変換されること。
    EXPECT_EQ('\0', name[0]); // [確認_異常系] - 失敗時に出力先が空文字列であること。
}
#endif /* PLATFORM_ */
