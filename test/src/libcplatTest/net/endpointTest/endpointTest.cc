#include <testfw.h>

#include <mock_cplat.h>

#include <cplat/base/platform.h>
#include <cplat/base/result.h>
#include <cplat/net/endpoint.h>
#include <errno.h>

#if defined(PLATFORM_LINUX)
    #include <arpa/mock_inet.h>
    #include <mock_netdb.h>

    #include <arpa/inet.h>
    #include <netdb.h>
    #include <netinet/in.h>
#elif defined(PLATFORM_WINDOWS)
    #include <cplat/base/windows_sdk.h>
    #include <cplat/base/error_internal.h>
    #include <cplat/net/socket_internal.h>
    #include <mock_winsock.h>
#endif /* PLATFORM_ */

#include <cstring>

using testing::_;
using testing::Assign;
using testing::DoAll;
using testing::NiceMock;
using testing::Return;

namespace
{

#if defined(PLATFORM_WINDOWS)
class Mock_socket_internal
{
  public:
    MOCK_METHOD(int, startup, (cplat_error *));
};

Mock_socket_internal *s_mock_socket_internal = nullptr;
#endif /* PLATFORM_WINDOWS */

// [サブ手順 名前=endpointTest.expect_detail]
void expect_detail(const cplat_error &detail, const cplat_error_domain domain, const int result,
                   const unsigned long code)
{
    EXPECT_EQ(domain, detail.domain);
    // [確認_正常系] - `detail.domain` の値が `domain` であること。
    EXPECT_EQ(result, detail.result);
    // [確認_正常系] - `detail.result` の値が `result` であること。
    EXPECT_EQ(code, detail.code);
    // [確認_正常系] - `detail.code` の値が `code` であること。
}
// [サブ手順終了]

} // namespace

#if defined(PLATFORM_WINDOWS)
extern "C" int cplat_internal_socket_startup(cplat_error *detail_out)
{
    if (s_mock_socket_internal == nullptr)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    return s_mock_socket_internal->startup(detail_out);
}
#endif /* PLATFORM_WINDOWS */

class endpointTest : public Test
{
  protected:
#if defined(PLATFORM_LINUX)
    NiceMock<Mock_arpa_inet> mock_arpa_inet_;
    NiceMock<Mock_netdb> mock_netdb_;
#elif defined(PLATFORM_WINDOWS)
    NiceMock<Mock_socket_internal> mock_socket_internal_;
    NiceMock<Mock_winsock> mock_winsock_;

    // [サブ手順 名前=endpointTest.SetUp]
    void SetUp() override
    {
        s_mock_socket_internal = &mock_socket_internal_;
        ON_CALL(mock_socket_internal_, startup(_))
            .WillByDefault([](cplat_error *detail_out) { return cplat_internal_error_report_success(detail_out); });
        // [状態] - `startup` の既定動作を設定する。
        ON_CALL(mock_winsock_, WSAGetLastError).WillByDefault(Return(0));
        // [状態] - `WSAGetLastError` の既定動作を設定する。
    }
    // [サブ手順終了]

    // [サブ手順 名前=endpointTest.TearDown]
    void TearDown() override
    {
        s_mock_socket_internal = nullptr;
    }
    // [サブ手順終了]
#endif /* PLATFORM_WINDOWS */
};

// IPv4 の解析が NULL 引数を拒否することの確認
// [サブ手順参照 名前=endpointTest.SetUp]
TEST_F(endpointTest, parse_rejects_null_arguments)
{
    // Arrange
    uint32_t address = 0U;

    // Pre-Assert

    // Act
    int actual_ret_null_text = cplat_ipv4_parse(NULL, &address);      // [手順] - text に NULL を指定して解析する。
    int actual_ret_null_output = cplat_ipv4_parse("192.0.2.1", NULL); // [手順] - address_out に NULL を指定して解析する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_text); // [確認_異常系] - text が NULL の cplat_ipv4_parse の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_output); // [確認_異常系] - address_out が NULL の cplat_ipv4_parse の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}
// [サブ手順参照 名前=endpointTest.TearDown]

// 不正な IPv4 文字列が拒否されることの確認
// [サブ手順参照 名前=endpointTest.SetUp]
TEST_F(endpointTest, parse_rejects_malformed_text)
{
    // Arrange
    uint32_t address = 0xA5A5A5A5U;

    // Pre-Assert
    // inet_pton が 1 回呼び出されること。
    // [Pre-Assert手順] - inet_pton から形式不正を示す 0 を返却する。
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_arpa_inet_, inet_pton(_, _, _, _, _, _))
        .WillOnce(Return(0));
    // [Pre-Assert確認_異常系] - mock_arpa_inet_ の inet_pton(_, _, _, _, _, _) が登録した呼び出し期待を満たすこと。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, inet_pton(_, _, _, _, _, _))
        .WillOnce(Return(0));
    // [Pre-Assert確認_異常系] - mock_winsock_ の inet_pton(_, _, _, _, _, _) が登録した呼び出し期待を満たすこと。
#endif /* PLATFORM_ */

    // Act
    int actual_ret = cplat_ipv4_parse("not-an-ip", &address); // [手順] - 不正な IPv4 文字列を解析する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret); // [確認_異常系] - 不正な文字列を指定した cplat_ipv4_parse の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(0xA5A5A5A5U,
              address); // [確認_異常系] - 解析に失敗して address_out が変更されないこと。
}
// [サブ手順参照 名前=endpointTest.TearDown]

// 正しい IPv4 文字列がネットワークバイトオーダーの値へ変換されることの確認
// [サブ手順参照 名前=endpointTest.SetUp]
TEST_F(endpointTest, parse_converts_valid_text)
{
    // Arrange
    uint32_t address = 0U;
    const uint32_t expected = CPLAT_IPV4_ADDR_LOOPBACK;

    // Pre-Assert
    // inet_pton が 1 回呼び出されること。
    // [Pre-Assert手順] - inet_pton からループバック アドレスを格納し、成功を示す 1 を返却する。
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_arpa_inet_, inet_pton(_, _, _, _, _, _))
        .WillOnce(
            [expected](const char *, const int, const char *, int, const char *, void *dst)
            {
                static_cast<struct in_addr *>(dst)->s_addr = expected;
                return 1;
            });
    // [Pre-Assert確認_正常系] - mock_arpa_inet_ の inet_pton(_, _, _, _, _, _) が登録した呼び出し期待を満たすこと。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, inet_pton(_, _, _, _, _, _))
        .WillOnce(
            [expected](const char *, const int, const char *, INT, PCSTR, PVOID dst)
            {
                static_cast<IN_ADDR *>(dst)->S_un.S_addr = expected;
                return 1;
            });
    // [Pre-Assert確認_正常系] - mock_winsock_ の inet_pton(_, _, _, _, _, _) が登録した呼び出し期待を満たすこと。
#endif /* PLATFORM_ */

    // Act
    int actual_ret = cplat_ipv4_parse("127.0.0.1", &address); // [手順] - ループバックアドレスを解析する。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - 正しい IPv4 文字列を指定した cplat_ipv4_parse の戻り値が CPLAT_OK であること。
    EXPECT_EQ(expected,
              address); // [確認_正常系] - cplat_ipv4_parse がネットワークバイトオーダーのアドレスを返すこと。
}
// [サブ手順参照 名前=endpointTest.TearDown]

#if defined(PLATFORM_WINDOWS)
// Winsock の初期化に失敗した場合、IPv4 解析が不正引数として終了することの確認
// [サブ手順参照 名前=endpointTest.SetUp]
TEST_F(endpointTest, parse_returns_invalid_when_startup_fails)
{
    // Arrange
    uint32_t address = 0U;

    // Pre-Assert
    // cplat_internal_socket_startup が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_internal_socket_startup が WSASYSNOTREADY を Winsock エラーとして返却する。
    EXPECT_CALL(mock_socket_internal_, startup(_))
        .WillOnce([](cplat_error *detail_out)
                  { return cplat_internal_error_report_winsock_error(detail_out, WSASYSNOTREADY); });
    // [Pre-Assert確認_異常系] - mock_socket_internal_ の startup(_) が登録した呼び出し期待を満たすこと。

    // Act
    int actual_ret = cplat_ipv4_parse("127.0.0.1", &address); // [手順] - 初期化失敗を注入して IPv4 を解析する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret); // [確認_異常系] - 初期化に失敗した cplat_ipv4_parse の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}
// [サブ手順参照 名前=endpointTest.TearDown]
#endif /* PLATFORM_WINDOWS */

// 名前解決が NULL 引数を拒否することの確認
// [サブ手順参照 名前=endpointTest.SetUp]
TEST_F(endpointTest, resolve_rejects_null_arguments)
{
    // Arrange
    uint32_t address = 0U;
    cplat_error detail = {};

    // Pre-Assert

    // Act
    int actual_ret_null_text =
        cplat_ipv4_resolve(NULL, &address, &detail); // [手順] - text に NULL を指定して名前解決する。
    int actual_ret_null_output =
        cplat_ipv4_resolve("localhost", NULL, &detail); // [手順] - address_out に NULL を指定して名前解決する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_text); // [確認_異常系] - text が NULL の cplat_ipv4_resolve の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_output); // [確認_異常系] - address_out が NULL の cplat_ipv4_resolve の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    // 詳細エラーに errno ドメインと EINVAL が記録されること。
    // [サブ手順参照 名前=endpointTest.expect_detail]
    expect_detail(detail, CPLAT_ERROR_DOMAIN_ERRNO, CPLAT_ERR_INVALID_ARGUMENT,
                  static_cast<unsigned long>(EINVAL));
    // 詳細エラーのドメイン、結果コード、OS コードが期待値と一致すること。
}
// [サブ手順参照 名前=endpointTest.TearDown]

// 名前解決の失敗時に GAI エラーが返されることの確認
// [サブ手順参照 名前=endpointTest.SetUp]
TEST_F(endpointTest, resolve_reports_lookup_failure)
{
    // Arrange
    uint32_t address = 0U;
    cplat_error detail = {};

    // Pre-Assert
    // getaddrinfo が 1 回呼び出されること。
    // [Pre-Assert手順] - getaddrinfo から解決結果へ NULL を格納し、EAI_AGAIN を返却する。
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_netdb_, getaddrinfo(_, _, _, _, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, const char *, const char *, const struct addrinfo *,
               struct addrinfo **result)
            {
                *result = NULL;
                return EAI_AGAIN;
            });
    // [Pre-Assert確認_異常系] - mock_netdb_ の getaddrinfo(_, _, _, _, _, _, _) が登録した呼び出し期待を満たすこと。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, getaddrinfo(_, _, _, _, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, PCSTR, PCSTR, const ADDRINFOA *, PADDRINFOA *result)
            {
                *result = NULL;
                return EAI_AGAIN;
            });
    // [Pre-Assert確認_異常系] - mock_winsock_ の getaddrinfo(_, _, _, _, _, _, _) が登録した呼び出し期待を満たすこと。
#endif /* PLATFORM_ */

    // Act
    int actual_ret = cplat_ipv4_resolve("example.invalid", &address, &detail); // [手順] - 名前解決失敗を注入する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret); // [確認_異常系] - 名前解決に失敗した cplat_ipv4_resolve の戻り値が CPLAT_ERR_UNKNOWN であること。
    // 詳細エラーに getaddrinfo ドメインとエラー コードが記録されること。
    // [サブ手順参照 名前=endpointTest.expect_detail]
    expect_detail(detail, CPLAT_ERROR_DOMAIN_GAI, CPLAT_ERR_UNKNOWN,
#if defined(PLATFORM_LINUX)
                  static_cast<unsigned long>(EAI_AGAIN));
    // 詳細エラーのドメイン、結果コード、OS コードが期待値と一致すること。
#else
                  static_cast<unsigned long>(EAI_AGAIN));
#endif /* PLATFORM_ */
}
// [サブ手順参照 名前=endpointTest.TearDown]

// 名前解決の失敗時に返された解決結果が解放されることの確認
// [サブ手順参照 名前=endpointTest.SetUp]
TEST_F(endpointTest, resolve_releases_result_when_lookup_fails)
{
    // Arrange
    uint32_t address = 0U;
    cplat_error detail = {};

#if defined(PLATFORM_LINUX)
    struct addrinfo resolved = {};
#elif defined(PLATFORM_WINDOWS)
    ADDRINFOA resolved = {};
#endif /* PLATFORM_ */

    // Pre-Assert
    // getaddrinfo が 1 回呼び出されること。
    // [Pre-Assert手順] - getaddrinfo から解決結果を格納したうえで EAI_FAIL を返却する。
    // freeaddrinfo が getaddrinfo の格納した解決結果を引数として 1 回呼び出されること。
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_netdb_, getaddrinfo(_, _, _, _, _, _, _))
        .WillOnce(
            [&resolved](const char *, const int, const char *, const char *, const char *, const struct addrinfo *,
                        struct addrinfo **result)
            {
                *result = &resolved;
                return EAI_FAIL;
            });
    // [Pre-Assert確認_異常系] - mock_netdb_ の getaddrinfo(_, _, _, _, _, _, _) が登録した呼び出し期待を満たすこと。
    EXPECT_CALL(mock_netdb_, freeaddrinfo(_, _, _, _))
        .WillOnce(
            [&resolved](const char *, const int, const char *, struct addrinfo *actual)
            {
                EXPECT_EQ(&resolved, actual);
                // [確認_正常系] - `freeaddrinfo` に渡す解決結果が `resolved` のアドレスであること。
            });
    // [Pre-Assert確認_異常系] - mock_netdb_ の freeaddrinfo(_, _, _, _) が登録した呼び出し期待を満たすこと。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, getaddrinfo(_, _, _, _, _, _, _))
        .WillOnce(
            [&resolved](const char *, const int, const char *, PCSTR, PCSTR, const ADDRINFOA *, PADDRINFOA *result)
            {
                *result = &resolved;
                return EAI_FAIL;
            });
    // [Pre-Assert確認_異常系] - mock_winsock_ の getaddrinfo(_, _, _, _, _, _, _) が登録した呼び出し期待を満たすこと。
    EXPECT_CALL(mock_winsock_, freeaddrinfo(_, _, _, _))
        .WillOnce(
            [&resolved](const char *, const int, const char *, PADDRINFOA actual)
            {
                EXPECT_EQ(&resolved, actual);
                // [確認_正常系] - `freeaddrinfo` に渡す解決結果が `resolved` のアドレスであること。
            });
    // [Pre-Assert確認_異常系] - mock_winsock_ の freeaddrinfo(_, _, _, _) が登録した呼び出し期待を満たすこと。
#endif /* PLATFORM_ */

    // Act
    int actual_ret = cplat_ipv4_resolve("example.invalid", &address, &detail); // [手順] - 解決結果を残す失敗を注入する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret); // [確認_異常系] - 解決結果を残した cplat_ipv4_resolve の戻り値が CPLAT_ERR_UNKNOWN であること。
}
// [サブ手順参照 名前=endpointTest.TearDown]

// 名前解決結果が NULL の場合に失敗として扱われることの確認
// [サブ手順参照 名前=endpointTest.SetUp]
TEST_F(endpointTest, resolve_rejects_empty_result)
{
    // Arrange
    uint32_t address = 0U;
    cplat_error detail = {};

    // Pre-Assert
    // getaddrinfo が 1 回呼び出されること。
    // [Pre-Assert手順] - getaddrinfo から解決結果へ NULL を格納し、成功を示す 0 を返却する。
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_netdb_, getaddrinfo(_, _, _, _, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, const char *, const char *, const struct addrinfo *,
               struct addrinfo **result)
            {
                *result = NULL;
                return 0;
            });
    // [Pre-Assert確認_異常系] - mock_netdb_ の getaddrinfo(_, _, _, _, _, _, _) が登録した呼び出し期待を満たすこと。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, getaddrinfo(_, _, _, _, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, PCSTR, PCSTR, const ADDRINFOA *, PADDRINFOA *result)
            {
                *result = NULL;
                return 0;
            });
    // [Pre-Assert確認_異常系] - mock_winsock_ の getaddrinfo(_, _, _, _, _, _, _) が登録した呼び出し期待を満たすこと。
#endif /* PLATFORM_ */

    // Act
    int actual_ret = cplat_ipv4_resolve("localhost", &address, &detail); // [手順] - NULL の名前解決結果を処理する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_NOT_FOUND,
        actual_ret); // [確認_異常系] - 結果が NULL の cplat_ipv4_resolve の戻り値が CPLAT_ERR_NOT_FOUND であること。
}
// [サブ手順参照 名前=endpointTest.TearDown]

// 名前解決結果の先頭 IPv4 アドレスが返されることの確認
// [サブ手順参照 名前=endpointTest.SetUp]
TEST_F(endpointTest, resolve_returns_first_ipv4_address)
{
    // Arrange
    uint32_t address = 0U;
    cplat_error detail = {};
    const uint32_t expected = CPLAT_IPV4_ADDR_LOOPBACK;

#if defined(PLATFORM_LINUX)
    struct sockaddr_in native = {};
    struct addrinfo resolved = {};
    native.sin_addr.s_addr = expected;
    resolved.ai_addr = reinterpret_cast<struct sockaddr *>(&native);
#elif defined(PLATFORM_WINDOWS)
    SOCKADDR_IN native = {};
    ADDRINFOA resolved = {};
    native.sin_addr.S_un.S_addr = expected;
    resolved.ai_addr = reinterpret_cast<sockaddr *>(&native);
#endif /* PLATFORM_ */

    // Pre-Assert
    // getaddrinfo が 1 回呼び出されること。
    // [Pre-Assert手順] - getaddrinfo から有効な IPv4 の解決結果を格納し、成功を示す 0 を返却する。
    // freeaddrinfo が getaddrinfo の格納した解決結果を引数として 1 回呼び出されること。
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_netdb_, getaddrinfo(_, _, _, _, _, _, _))
        .WillOnce(
            [&resolved](const char *, const int, const char *, const char *, const char *, const struct addrinfo *,
                        struct addrinfo **result)
            {
                *result = &resolved;
                return 0;
            });
    // [Pre-Assert確認_正常系] - mock_netdb_ の getaddrinfo(_, _, _, _, _, _, _) が登録した呼び出し期待を満たすこと。
    EXPECT_CALL(mock_netdb_, freeaddrinfo(_, _, _, _))
        .WillOnce(
            [&resolved](const char *, const int, const char *, struct addrinfo *actual)
            { EXPECT_EQ(&resolved, actual); });
            // [確認_正常系] - `freeaddrinfo` に渡す解決結果が `resolved` のアドレスであること。
    // [Pre-Assert確認_正常系] - mock_netdb_ の freeaddrinfo(_, _, _, _) が登録した呼び出し期待を満たすこと。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, getaddrinfo(_, _, _, _, _, _, _))
        .WillOnce(
            [&resolved](const char *, const int, const char *, PCSTR, PCSTR, const ADDRINFOA *, PADDRINFOA *result)
            {
                *result = &resolved;
                return 0;
            });
    // [Pre-Assert確認_正常系] - mock_winsock_ の getaddrinfo(_, _, _, _, _, _, _) が登録した呼び出し期待を満たすこと。
    EXPECT_CALL(mock_winsock_, freeaddrinfo(_, _, _, _))
        .WillOnce(
            [&resolved](const char *, const int, const char *, PADDRINFOA actual)
            { EXPECT_EQ(&resolved, actual); });
            // [確認_正常系] - `freeaddrinfo` に渡す解決結果が `resolved` のアドレスであること。
    // [Pre-Assert確認_正常系] - mock_winsock_ の freeaddrinfo(_, _, _, _) が登録した呼び出し期待を満たすこと。
#endif /* PLATFORM_ */

    // Act
    int actual_ret = cplat_ipv4_resolve("localhost", &address, &detail); // [手順] - 有効な IPv4 解決結果を処理する。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret); // [確認_正常系] - 有効な結果を指定した cplat_ipv4_resolve の戻り値が CPLAT_OK であること。
    EXPECT_EQ(expected,
              address); // [確認_正常系] - cplat_ipv4_resolve が先頭の IPv4 アドレスを返すこと。
    // 詳細エラーが記録されないこと。
    // [サブ手順参照 名前=endpointTest.expect_detail]
    expect_detail(detail, CPLAT_ERROR_DOMAIN_NONE, CPLAT_OK, 0UL);
    // 詳細エラーのドメイン、結果コード、OS コードが期待値と一致すること。
}
// [サブ手順参照 名前=endpointTest.TearDown]

#if defined(PLATFORM_LINUX)
// 名前解決がシグナル中断後に再試行することの確認
// [サブ手順参照 名前=endpointTest.SetUp]
TEST_F(endpointTest, resolve_retries_after_interrupt)
{
    // Arrange
    uint32_t address = 0U;
    cplat_error detail = {};
    const uint32_t expected = CPLAT_IPV4_ADDR_LOOPBACK;
    struct sockaddr_in native = {};
    struct addrinfo resolved = {};
    native.sin_addr.s_addr = expected;
    resolved.ai_addr = reinterpret_cast<struct sockaddr *>(&native);

    // Pre-Assert
    // getaddrinfo が 2 回呼び出されること。
    // [Pre-Assert手順] - getaddrinfo から、errno に EINTR を設定した EAI_SYSTEM ののち、有効な IPv4 の解決結果と成功を示す 0 を返却する。
    EXPECT_CALL(mock_netdb_, getaddrinfo(_, _, _, _, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, const char *, const char *, const struct addrinfo *,
               struct addrinfo **result)
            {
                *result = NULL;
                errno = EINTR;
                return EAI_SYSTEM;
            })
        .WillOnce(
            [&resolved](const char *, const int, const char *, const char *, const char *, const struct addrinfo *,
                        struct addrinfo **result)
            {
                *result = &resolved;
                return 0;
            });
    // [Pre-Assert確認_正常系] - freeaddrinfo が getaddrinfo の格納した解決結果を引数として 1 回呼び出されること。
    EXPECT_CALL(mock_netdb_, freeaddrinfo(_, _, _, _))
        .WillOnce(
            [&resolved](const char *, const int, const char *, struct addrinfo *actual)
            { EXPECT_EQ(&resolved, actual); });
            // [確認_正常系] - `freeaddrinfo` に渡す解決結果が `resolved` のアドレスであること。
    // [Pre-Assert確認_正常系] - mock_netdb_ の freeaddrinfo(_, _, _, _) が登録した呼び出し期待を満たすこと。

    // Act
    int actual_ret = cplat_ipv4_resolve("localhost", &address, &detail); // [手順] - 中断ののち成功する名前解決を実行する。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret); // [確認_正常系] - 中断後に成功した cplat_ipv4_resolve の戻り値が CPLAT_OK であること。
    EXPECT_EQ(expected,
              address); // [確認_正常系] - 再試行で解決した IPv4 アドレスが返されること。
    // 詳細エラーが記録されないこと。
    // [サブ手順参照 名前=endpointTest.expect_detail]
    expect_detail(detail, CPLAT_ERROR_DOMAIN_NONE, CPLAT_OK, 0UL);
    // 詳細エラーのドメイン、結果コード、OS コードが期待値と一致すること。
}
// [サブ手順参照 名前=endpointTest.TearDown]

// 名前解決の EAI_SYSTEM が中断以外の errno では再試行されないことの確認
// [サブ手順参照 名前=endpointTest.SetUp]
TEST_F(endpointTest, resolve_reports_system_error_without_retry)
{
    // Arrange
    uint32_t address = 0U;
    cplat_error detail = {};

    // Pre-Assert
    // getaddrinfo が 1 回だけ呼び出されること。
    // [Pre-Assert手順] - getaddrinfo から、errno に ENOMEM を設定した EAI_SYSTEM を返却する。
    EXPECT_CALL(mock_netdb_, getaddrinfo(_, _, _, _, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, const char *, const char *, const struct addrinfo *,
               struct addrinfo **result)
            {
                *result = NULL;
                errno = ENOMEM;
                return EAI_SYSTEM;
            });
    // [Pre-Assert確認_異常系] - mock_netdb_ の getaddrinfo(_, _, _, _, _, _, _) が登録した呼び出し期待を満たすこと。

    // Act
    int actual_ret = cplat_ipv4_resolve("localhost", &address, &detail); // [手順] - 中断以外の EAI_SYSTEM を注入する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret); // [確認_異常系] - EAI_SYSTEM を返した cplat_ipv4_resolve の戻り値が CPLAT_ERR_UNKNOWN であること。
    // 詳細エラーに getaddrinfo ドメインと EAI_SYSTEM が記録されること。
    // [サブ手順参照 名前=endpointTest.expect_detail]
    expect_detail(detail, CPLAT_ERROR_DOMAIN_GAI, CPLAT_ERR_UNKNOWN, static_cast<unsigned long>(EAI_SYSTEM));
    // 詳細エラーのドメイン、結果コード、OS コードが期待値と一致すること。
}
// [サブ手順参照 名前=endpointTest.TearDown]
#endif /* PLATFORM_LINUX */

#if defined(PLATFORM_WINDOWS)
// Winsock の初期化に失敗した場合、名前解決が失敗することの確認
// [サブ手順参照 名前=endpointTest.SetUp]
TEST_F(endpointTest, resolve_propagates_startup_failure)
{
    // Arrange
    uint32_t address = 0U;
    cplat_error detail = {};

    // Pre-Assert
    // cplat_internal_socket_startup が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_internal_socket_startup が WSASYSNOTREADY を Winsock エラーとして返却する。
    EXPECT_CALL(mock_socket_internal_, startup(_))
        .WillOnce([](cplat_error *detail_out)
                  { return cplat_internal_error_report_winsock_error(detail_out, WSASYSNOTREADY); });
    // [Pre-Assert確認_異常系] - mock_socket_internal_ の startup(_) が登録した呼び出し期待を満たすこと。

    // Act
    int actual_ret = cplat_ipv4_resolve("localhost", &address, &detail); // [手順] - 初期化失敗を注入して名前解決する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret); // [確認_異常系] - 初期化に失敗した cplat_ipv4_resolve の戻り値が CPLAT_ERR_UNKNOWN であること。
    // 詳細エラーに Winsock ドメインと OS のエラー値が記録されること。
    // [サブ手順参照 名前=endpointTest.expect_detail]
    expect_detail(detail, CPLAT_ERROR_DOMAIN_WINSOCK, CPLAT_ERR_UNKNOWN,
                  static_cast<unsigned long>(WSASYSNOTREADY));
    // 詳細エラーのドメイン、結果コード、OS コードが期待値と一致すること。
}
// [サブ手順参照 名前=endpointTest.TearDown]
#endif /* PLATFORM_WINDOWS */

// IPv4 文字列出力が NULL 引数を拒否することの確認
// [サブ手順参照 名前=endpointTest.SetUp]
TEST_F(endpointTest, to_string_rejects_null_or_zero_sized_buffer)
{
    // Arrange
    char buffer[CPLAT_IPV4_ADDR_STRLEN] = {};
    cplat_error detail = {};

    // Pre-Assert

    // Act
    int actual_ret_null_buffer = cplat_ipv4_to_string(CPLAT_IPV4_ADDR_LOOPBACK, NULL, sizeof(buffer),
                                                  &detail); // [手順] - buffer に NULL を指定して文字列化する。
    int actual_ret_zero_size = cplat_ipv4_to_string(CPLAT_IPV4_ADDR_LOOPBACK, buffer, 0U,
                                                &detail); // [手順] - buffer_size に 0 を指定して文字列化する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_buffer); // [確認_異常系] - buffer が NULL の cplat_ipv4_to_string の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_zero_size); // [確認_異常系] - buffer_size が 0 の cplat_ipv4_to_string の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}
// [サブ手順参照 名前=endpointTest.TearDown]

// IPv4 文字列出力が小さいバッファーを拒否することの確認
// [サブ手順参照 名前=endpointTest.SetUp]
TEST_F(endpointTest, to_string_rejects_small_buffer)
{
    // Arrange
    char buffer[CPLAT_IPV4_ADDR_STRLEN] = {};
    cplat_error detail = {};

    // Pre-Assert

    // Act
    int actual_ret = cplat_ipv4_to_string(CPLAT_IPV4_ADDR_LOOPBACK, buffer, CPLAT_IPV4_ADDR_STRLEN - 1U,
                                      &detail); // [手順] - 必要長未満のバッファーで文字列化する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_BUFFER_TOO_SMALL,
        actual_ret); // [確認_異常系] - 小さいバッファーを指定した cplat_ipv4_to_string の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    // 詳細エラーに errno ドメインと ERANGE が記録されること。
    // [サブ手順参照 名前=endpointTest.expect_detail]
    expect_detail(detail, CPLAT_ERROR_DOMAIN_ERRNO, CPLAT_ERR_BUFFER_TOO_SMALL,
                  static_cast<unsigned long>(ERANGE));
    // 詳細エラーのドメイン、結果コード、OS コードが期待値と一致すること。
}
// [サブ手順参照 名前=endpointTest.TearDown]

// OS の IPv4 文字列化に失敗した場合にエラーが返されることの確認
// [サブ手順参照 名前=endpointTest.SetUp]
TEST_F(endpointTest, to_string_reports_conversion_failure)
{
    // Arrange
    char buffer[CPLAT_IPV4_ADDR_STRLEN] = {};
    cplat_error detail = {};

    // Pre-Assert
    // inet_ntop が 1 回呼び出されること。
    // [Pre-Assert手順] - inet_ntop から変換失敗を示す NULL を返却する。
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_arpa_inet_, inet_ntop(_, _, _, _, _, _, _))
        .WillOnce(DoAll(Assign(&errno, EINVAL), Return(static_cast<const char *>(NULL))));
    // [Pre-Assert確認_異常系] - mock_arpa_inet_ の inet_ntop(_, _, _, _, _, _, _) が登録した呼び出し期待を満たすこと。

    // [Pre-Assert手順] - inet_ntop の失敗時に errno を EINVAL へ設定する。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, inet_ntop(_, _, _, _, _, _, _))
        .WillOnce(Return(static_cast<PCSTR>(NULL)));
    // [Pre-Assert確認_異常系] - WSAGetLastError が 1 回呼び出されること。
    // [Pre-Assert手順] - WSAGetLastError から WSAEINVAL を返却する。
    EXPECT_CALL(mock_winsock_, WSAGetLastError)
        .WillOnce(Return(WSAEINVAL));
    // [Pre-Assert確認_異常系] - mock_winsock_ の WSAGetLastError が登録した呼び出し期待を満たすこと。
#endif /* PLATFORM_ */

    // Act
    int actual_ret = cplat_ipv4_to_string(CPLAT_IPV4_ADDR_LOOPBACK, buffer, sizeof(buffer),
                                      &detail); // [手順] - IPv4 文字列化の失敗を注入する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret); // [確認_異常系] - 文字列化に失敗した cplat_ipv4_to_string の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}
// [サブ手順参照 名前=endpointTest.TearDown]

// IPv4 アドレスがドット区切り文字列へ変換されることの確認
// [サブ手順参照 名前=endpointTest.SetUp]
TEST_F(endpointTest, to_string_converts_address)
{
    // Arrange
    char buffer[CPLAT_IPV4_ADDR_STRLEN] = {};
    cplat_error detail = {};

    // Pre-Assert
    // inet_ntop が 1 回呼び出されること。
    // [Pre-Assert手順] - inet_ntop からループバック アドレスの文字列を格納して返却する。
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_arpa_inet_, inet_ntop(_, _, _, _, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, int, const void *, char *dst, socklen_t)
            {
                std::memcpy(dst, "127.0.0.1", sizeof("127.0.0.1"));
                return static_cast<const char *>(dst);
            });
    // [Pre-Assert確認_正常系] - mock_arpa_inet_ の inet_ntop(_, _, _, _, _, _, _) が登録した呼び出し期待を満たすこと。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, inet_ntop(_, _, _, _, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, INT, const void *, PSTR dst, size_t)
            {
                std::memcpy(dst, "127.0.0.1", sizeof("127.0.0.1"));
                return static_cast<PCSTR>(dst);
            });
    // [Pre-Assert確認_正常系] - mock_winsock_ の inet_ntop(_, _, _, _, _, _, _) が登録した呼び出し期待を満たすこと。
#endif /* PLATFORM_ */

    // Act
    int actual_ret = cplat_ipv4_to_string(CPLAT_IPV4_ADDR_LOOPBACK, buffer, sizeof(buffer),
                                      &detail); // [手順] - IPv4 アドレスを文字列化する。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret); // [確認_正常系] - 正常に文字列化した cplat_ipv4_to_string の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("127.0.0.1",
                 buffer); // [確認_正常系] - cplat_ipv4_to_string がドット区切りの IPv4 文字列を返すこと。
    // 詳細エラーが記録されないこと。
    // [サブ手順参照 名前=endpointTest.expect_detail]
    expect_detail(detail, CPLAT_ERROR_DOMAIN_NONE, CPLAT_OK, 0UL);
    // 詳細エラーのドメイン、結果コード、OS コードが期待値と一致すること。
}
// [サブ手順参照 名前=endpointTest.TearDown]

#if defined(PLATFORM_WINDOWS)
// Winsock の初期化に失敗した場合、IPv4 文字列化が失敗することの確認
// [サブ手順参照 名前=endpointTest.SetUp]
TEST_F(endpointTest, to_string_propagates_startup_failure)
{
    // Arrange
    char buffer[CPLAT_IPV4_ADDR_STRLEN] = {};
    cplat_error detail = {};

    // Pre-Assert
    // cplat_internal_socket_startup が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_internal_socket_startup が WSASYSNOTREADY を Winsock エラーとして返却する。
    EXPECT_CALL(mock_socket_internal_, startup(_))
        .WillOnce([](cplat_error *detail_out)
                  { return cplat_internal_error_report_winsock_error(detail_out, WSASYSNOTREADY); });
    // [Pre-Assert確認_異常系] - mock_socket_internal_ の startup(_) が登録した呼び出し期待を満たすこと。

    // Act
    int actual_ret = cplat_ipv4_to_string(CPLAT_IPV4_ADDR_LOOPBACK, buffer, sizeof(buffer),
                                      &detail); // [手順] - 初期化失敗を注入して IPv4 を文字列化する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret); // [確認_異常系] - 初期化に失敗した cplat_ipv4_to_string の戻り値が CPLAT_ERR_UNKNOWN であること。
    // 詳細エラーに Winsock ドメインと OS のエラー値が記録されること。
    // [サブ手順参照 名前=endpointTest.expect_detail]
    expect_detail(detail, CPLAT_ERROR_DOMAIN_WINSOCK, CPLAT_ERR_UNKNOWN,
                  static_cast<unsigned long>(WSASYSNOTREADY));
    // 詳細エラーのドメイン、結果コード、OS コードが期待値と一致すること。
}
// [サブ手順参照 名前=endpointTest.TearDown]
#endif /* PLATFORM_WINDOWS */
