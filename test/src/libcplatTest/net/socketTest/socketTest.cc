#include <testfw.h>

#include <mock_cplat.h>

#include <cplat/base/platform.h>
#include <cplat/base/error.h>
#include <cplat/base/result.h>
#include <cplat/net/socket.h>

#if defined(PLATFORM_LINUX)
    #include <mock_fcntl.h>
    #include <mock_poll.h>
    #include <mock_unistd.h>
    #include <sys/mock_socket.h>
    #include <sys/mock_eventfd.h>

    #include <errno.h>
    #include <fcntl.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <poll.h>
    #include <sys/socket.h>
    #include <unistd.h>
#elif defined(PLATFORM_WINDOWS)
    #include <cplat/net/socket_internal.h>
    #include <mock_winsock.h>

    #include <errno.h>
#endif /* PLATFORM_ */

#include <cstring>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits.h>
#include <map>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

using testing::_;
using testing::Assign;
using testing::DoAll;
using testing::Mock;
using testing::NiceMock;
using testing::Return;

namespace
{

const cplat_socket kSocket = (cplat_socket)7;

const cplat_ipv4_endpoint kEndpoint = {cplat_ipv4_addr_loopback(), cplat_hton16((uint16_t)12345U), 0U};

/* TCP_NODELAY の値は int で、長さは sizeof(int) です。
 * Linux の setsockopt() は const void * と socklen_t、Windows は const char * と int です。
 */
MATCHER_P(TcpNodelayValue, expected, "")
{
    int actual = 0;

    if (arg == nullptr)
    {
        *result_listener << "nullptr";
        return false;
    }

    std::memcpy(&actual, arg, sizeof(actual));
    if (actual != expected)
    {
        *result_listener << actual;
        return false;
    }

    return true;
}

// [サブ手順 名前=socketTest.expect_detail]
void expect_detail(const cplat_error &detail, const cplat_error_domain domain, const int result,
                   const unsigned long code)
{
    EXPECT_EQ(domain, detail.domain);
    // [確認_正常系] - detail.domain が domain と一致すること。
    EXPECT_EQ(result, detail.result);
    // [確認_正常系] - detail.result が result と一致すること。
    EXPECT_EQ(code, detail.code);
    // [確認_正常系] - detail.code が code と一致すること。
}
// [サブ手順終了]

} // namespace

// IP_MULTICAST_TTL の読み戻しには実ソケットを使用します。
#if defined(PLATFORM_WINDOWS)
namespace
{

class RealWinsockSession
{
  public:
    RealWinsockSession()
    {
        WSADATA data = {};
        started_ = delegate_real_WSAStartup(__FILE__, __LINE__, __func__, MAKEWORD(2, 2), &data) == 0;
    }

    ~RealWinsockSession()
    {
        if (started_)
        {
            /* このスコープで開始した Winsock の参照を解放します。 */
            (void)delegate_real_WSACleanup(__FILE__, __LINE__, __func__);
        }
    }

    bool started() const
    {
        return started_;
    }

  private:
    bool started_ = false;
};

} // namespace
#endif /* PLATFORM_WINDOWS */

class socketTest : public Test
{
  protected:
    NiceMock<Mock_cplat> mock_cplat_;

#if defined(PLATFORM_LINUX)
    NiceMock<Mock_sys_socket> mock_sys_socket_;
    NiceMock<Mock_fcntl> mock_fcntl_;
    NiceMock<Mock_poll> mock_poll_;
    NiceMock<Mock_unistd> mock_unistd_;
    NiceMock<Mock_eventfd> mock_eventfd_;
#elif defined(PLATFORM_WINDOWS)
    NiceMock<Mock_winsock> mock_winsock_;
#endif /* PLATFORM_ */

    /* プラットフォームごとにモック対象が異なるため、呼び出し期待の検証はヘルパーへ限定する。 */
    bool verifyExpectations()
    {
#if defined(PLATFORM_LINUX)
        return Mock::VerifyAndClearExpectations(&mock_cplat_) && Mock::VerifyAndClearExpectations(&mock_sys_socket_) &&
               Mock::VerifyAndClearExpectations(&mock_unistd_) && Mock::VerifyAndClearExpectations(&mock_eventfd_);
#elif defined(PLATFORM_WINDOWS)
        return Mock::VerifyAndClearExpectations(&mock_cplat_) && Mock::VerifyAndClearExpectations(&mock_winsock_);
#endif /* PLATFORM_ */
    }

    /* 下位のクローズ API が呼び出されないことを期待する。 */
    // [サブ手順 名前=socketTest.expectNoCloseCall]
    void expectNoCloseCall()
    {
#if defined(PLATFORM_LINUX)
        EXPECT_CALL(mock_cplat_, cplat_close(_, _)).Times(0);
        // [Pre-Assert確認_正常系] - cplat_close() が呼び出されないこと。
#elif defined(PLATFORM_WINDOWS)
        EXPECT_CALL(mock_winsock_, closesocket(_, _, _, _)).Times(0);
        // [Pre-Assert確認_正常系] - closesocket() が呼び出されないこと。
#endif /* PLATFORM_ */
    }
    // [サブ手順終了]

    /* 下位のシャットダウン API が呼び出されないことを期待する。 */
    // [サブ手順 名前=socketTest.expectNoShutdownCall]
    void expectNoShutdownCall()
    {
#if defined(PLATFORM_LINUX)
        EXPECT_CALL(mock_sys_socket_, shutdown(_, _, _, _, _)).Times(0);
        // [Pre-Assert確認_正常系] - shutdown() が呼び出されないこと。
#elif defined(PLATFORM_WINDOWS)
        EXPECT_CALL(mock_winsock_, shutdown(_, _, _, _, _)).Times(0);
        // [Pre-Assert確認_正常系] - shutdown() が呼び出されないこと。
#endif /* PLATFORM_ */
    }
    // [サブ手順終了]

    void SetUp() override
    {
#if defined(PLATFORM_WINDOWS)
        ON_CALL(mock_cplat_, cplat_call_once)
            .WillByDefault([](cplat_once_flag *, cplat_once_fn function) { function(); });
        ON_CALL(mock_winsock_, WSAStartup).WillByDefault(Return(0));
        ON_CALL(mock_winsock_, WSACleanup).WillByDefault(Return(0));
#endif /* PLATFORM_WINDOWS */
    }

#if defined(PLATFORM_WINDOWS)
    // [サブ手順 名前=socketTest.TearDown]
    void TearDown() override
    {
        cplat_internal_socket_cleanup();
    }
    // [サブ手順終了]
#endif /* PLATFORM_WINDOWS */
};

// ソケットの引数不正が拒否されることの確認
TEST_F(socketTest, open_rejects_invalid_arguments)
{
    // Arrange
    cplat_socket socket = kSocket;
    // 列挙範囲外の不正値を意図的に渡す (定数キャストは -Wconversion になるため変数経由)
    int invalid_kind_value = CPLAT_SOCKET_UDP + 1; // [状態] - 未定義の種別を表す値とする。

    // Pre-Assert

    // Act
    int actual_ret_null_output = cplat_socket_open(
        CPLAT_SOCKET_TCP, nullptr, nullptr); // [手順] - 出力先に nullptr を指定して cplat_socket_open() を呼び出す。
    int actual_ret_invalid_kind =
        cplat_socket_open(static_cast<cplat_socket_kind>(invalid_kind_value), &socket,
                          nullptr); // [手順] - 未定義の種別を指定して cplat_socket_open() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_output); // [確認_異常系] - 出力先が nullptr の cplat_socket_open() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_invalid_kind); // [確認_異常系] - 未定義の種別を指定した cplat_socket_open() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_INVALID_SOCKET,
              socket); // [確認_異常系] - 不正な種別の指定時の socket が CPLAT_INVALID_SOCKET であること。
}
// [サブ手順参照 名前=socketTest.TearDown]

// ソケットが TCP と UDP の種別に応じて生成されることの確認
TEST_F(socketTest, open_returns_socket_for_each_kind)
{
    // Arrange
    cplat_socket tcp_socket = CPLAT_INVALID_SOCKET;
    cplat_socket udp_socket = CPLAT_INVALID_SOCKET;

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_sys_socket_, socket(_, _, _, AF_INET, SOCK_STREAM, 0)).WillOnce(Return(7));
    // [Pre-Assert確認_正常系] - socket() が AF_INET、SOCK_STREAM、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - socket() から 7 を返す。
    EXPECT_CALL(mock_sys_socket_, socket(_, _, _, AF_INET, SOCK_DGRAM, 0)).WillOnce(Return(8));
    // [Pre-Assert確認_正常系] - socket() が AF_INET、SOCK_DGRAM、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - socket() から 8 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, socket(_, _, _, AF_INET, SOCK_STREAM, 0)).WillOnce(Return((SOCKET)7));
    // [Pre-Assert確認_正常系] - socket() が AF_INET、SOCK_STREAM、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - socket() から 7 を返す。
    EXPECT_CALL(mock_winsock_, socket(_, _, _, AF_INET, SOCK_DGRAM, 0)).WillOnce(Return((SOCKET)8));
    // [Pre-Assert確認_正常系] - socket() が AF_INET、SOCK_DGRAM、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - socket() から 8 を返す。
#endif /* PLATFORM_ */
    // [Pre-Assert手順] - socket() から TCP に 7、UDP に 8 のハンドルを返す。

    // Act
    int actual_ret_tcp = cplat_socket_open(CPLAT_SOCKET_TCP, &tcp_socket,
                                           nullptr); // [手順] - TCP を指定して cplat_socket_open() を呼び出す。
    int actual_ret_udp = cplat_socket_open(CPLAT_SOCKET_UDP, &udp_socket,
                                           nullptr); // [手順] - UDP を指定して cplat_socket_open() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret_tcp); // [確認_正常系] - TCP を指定した cplat_socket_open() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              actual_ret_udp); // [確認_正常系] - UDP を指定した cplat_socket_open() の戻り値が CPLAT_OK であること。
    EXPECT_EQ((cplat_socket)7,
              tcp_socket); // [確認_正常系] - tcp_socket が 7 であること。(TCP に返したハンドルであることの確認)
    EXPECT_EQ((cplat_socket)8,
              udp_socket); // [確認_正常系] - udp_socket が 8 であること。(UDP に返したハンドルであることの確認)
}
// [サブ手順参照 名前=socketTest.TearDown]

// OS のソケット生成失敗が通知されることの確認
TEST_F(socketTest, open_reports_socket_failure)
{
    // Arrange
    cplat_socket socket = kSocket;
    cplat_error detail = {};

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_sys_socket_, socket(_, _, _, AF_INET, SOCK_STREAM, 0))
        .WillOnce(DoAll(Assign(&errno, EMFILE), Return(-1)));
    // [Pre-Assert確認_異常系] - socket() が AF_INET、SOCK_STREAM、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - socket() から、errno に EMFILE を設定して -1 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, socket(_, _, _, AF_INET, SOCK_STREAM, 0)).WillOnce(Return(INVALID_SOCKET));
    // [Pre-Assert確認_異常系] - socket() が AF_INET、SOCK_STREAM、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - socket() から INVALID_SOCKET を返す。
    EXPECT_CALL(mock_winsock_, WSAGetLastError).WillOnce(Return(WSAEMFILE));
    // [Pre-Assert確認_異常系] - WSAGetLastError() が 1 回呼び出されること。
    // [Pre-Assert手順] - WSAGetLastError() から WSAEMFILE を返す。
#endif /* PLATFORM_ */
    // [Pre-Assert手順] - socket() から、Linux では errno に EMFILE を設定して -1、Windows では INVALID_SOCKET を返し、WSAGetLastError() から WSAEMFILE を返す。

    // Act
    int actual_ret = cplat_socket_open(CPLAT_SOCKET_TCP, &socket,
                                       &detail); // [手順] - socket() が失敗する状態で cplat_socket_open() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret); // [確認_異常系] - ソケット生成失敗時の cplat_socket_open() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(CPLAT_INVALID_SOCKET,
              socket); // [確認_異常系] - ソケット生成失敗時の socket が CPLAT_INVALID_SOCKET であること。
    // 詳細エラーに実行環境に応じた OS のエラー ドメインとエラー値が記録されること。
#if defined(PLATFORM_LINUX)
    // [サブ手順参照 名前=socketTest.expect_detail]
    expect_detail(detail, CPLAT_ERROR_DOMAIN_SOCKET_ERRNO, CPLAT_ERR_UNKNOWN, static_cast<unsigned long>(EMFILE));
    // 詳細エラーのドメイン、結果コード、OS コードが期待値と一致すること。
#elif defined(PLATFORM_WINDOWS)
    // [サブ手順参照 名前=socketTest.expect_detail]
    expect_detail(detail, CPLAT_ERROR_DOMAIN_WINSOCK, CPLAT_ERR_UNKNOWN, static_cast<unsigned long>(WSAEMFILE));
    // 詳細エラーのドメイン、結果コード、OS コードが期待値と一致すること。
#endif /* PLATFORM_ */
}
// [サブ手順参照 名前=socketTest.TearDown]

#if defined(PLATFORM_WINDOWS)
// Winsock の初期化失敗がソケット生成へ伝播することの確認
TEST_F(socketTest, open_propagates_startup_failure)
{
    // Arrange
    cplat_socket socket = kSocket;
    cplat_error detail = {};

    // Pre-Assert
    EXPECT_CALL(mock_winsock_, WSAStartup(_, _, _, _, _)).WillOnce(Return(WSASYSNOTREADY));
    // [Pre-Assert確認_異常系] - WSAStartup() が 1 回呼び出されること。
    // [Pre-Assert手順] - WSAStartup() から WSASYSNOTREADY を返す。

    // Act
    int actual_ret =
        cplat_socket_open(CPLAT_SOCKET_TCP, &socket,
                          &detail); // [手順] - Winsock の初期化が失敗する状態で cplat_socket_open() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret); // [確認_異常系] - 初期化失敗時の cplat_socket_open() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(CPLAT_INVALID_SOCKET,
              socket); // [確認_異常系] - 初期化失敗時の socket が CPLAT_INVALID_SOCKET であること。

    // 詳細エラーに Winsock ドメインと OS のエラー値が記録されること。
    // [サブ手順参照 名前=socketTest.expect_detail]
    expect_detail(detail, CPLAT_ERROR_DOMAIN_WINSOCK, CPLAT_ERR_UNKNOWN, static_cast<unsigned long>(WSASYSNOTREADY));

    // 詳細エラーのドメイン、結果コード、OS コードが期待値と一致すること。
}
// [サブ手順参照 名前=socketTest.TearDown]
#endif /* PLATFORM_WINDOWS */

// 無効なソケットを閉じる場合に OS API が呼ばれないことの確認
TEST_F(socketTest, close_ignores_invalid_socket)
{
    // Arrange

    // Pre-Assert
    // [サブ手順参照 名前=socketTest.expectNoCloseCall]
    expectNoCloseCall();

    // Act
    cplat_socket_close(CPLAT_INVALID_SOCKET); // [手順] - 無効なソケットを指定して cplat_socket_close() を呼び出す。

    // Assert
    EXPECT_TRUE(
        verifyExpectations()); // [確認_正常系] - verifyExpectations() の戻り値が true であること。(無効なソケットの指定時に下位のクローズ API が呼び出されないことの確認)
}
// [サブ手順参照 名前=socketTest.TearDown]

// 有効なソケットを閉じる場合に OS API が呼ばれることの確認
TEST_F(socketTest, close_calls_os_close)
{
    // Arrange

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_cplat_, cplat_close(kSocket, _)).WillOnce(Return(CPLAT_OK));
    // [Pre-Assert確認_正常系] - cplat_close() が kSocket を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_close() から CPLAT_OK を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, closesocket(_, _, _, (SOCKET)kSocket)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - closesocket() が kSocket を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - closesocket() から 0 を返す。
#endif /* PLATFORM_ */
    // [Pre-Assert手順] - cplat_close() から CPLAT_OK を返し、Windows では closesocket() から 0 を返す。

    // Act
    cplat_socket_close(kSocket); // [手順] - 有効なソケットを指定して cplat_socket_close() を呼び出す。

    // Assert
    EXPECT_TRUE(
        verifyExpectations()); // [確認_正常系] - verifyExpectations() の戻り値が true であること。(下位のクローズ API への呼び出し期待が満たされることの確認)
}
// [サブ手順参照 名前=socketTest.TearDown]

// クローズが呼び出し前の直前エラーを保存および復元することの確認
TEST_F(socketTest, close_preserves_last_error)
{
    // Arrange
    const cplat_error saved = {CPLAT_ERROR_DOMAIN_SOCKET_ERRNO, CPLAT_ERR_UNKNOWN,
                               static_cast<unsigned long>(ECONNRESET)};
    cplat_error actual = {};

    cplat_error_set_last(&saved); // [状態] - 呼び出し前の直前エラーを ECONNRESET とする。

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_cplat_, cplat_close(kSocket, _))
        .WillOnce(
            [](int, cplat_error *)
            {
                const cplat_error overwritten = {CPLAT_ERROR_DOMAIN_ERRNO, CPLAT_ERR_PERMISSION_DENIED,
                                                 static_cast<unsigned long>(EACCES)};
                cplat_error_set_last(&overwritten);
                return CPLAT_OK;
            });
    // [Pre-Assert確認_異常系] - cplat_close() が kSocket を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_close() から、cplat_error_set_last() で overwritten のエラー情報 (CPLAT_ERROR_DOMAIN_ERRNO、CPLAT_ERR_PERMISSION_DENIED、EACCES) を設定して CPLAT_OK を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, closesocket(_, _, _, (SOCKET)kSocket))
        .WillOnce(
            [](const char *, const int, const char *, SOCKET)
            {
                const cplat_error overwritten = {CPLAT_ERROR_DOMAIN_ERRNO, CPLAT_ERR_PERMISSION_DENIED,
                                                 static_cast<unsigned long>(EACCES)};
                cplat_error_set_last(&overwritten);
                return 0;
            });
    // [Pre-Assert確認_異常系] - closesocket() が kSocket を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - closesocket() から、cplat_error_set_last() で overwritten のエラー情報 (CPLAT_ERROR_DOMAIN_ERRNO、CPLAT_ERR_PERMISSION_DENIED、EACCES) を設定して 0 を返す。
#endif /* PLATFORM_ */
    // [Pre-Assert手順] - cplat_close() から、直前エラーを EACCES へ書き換えて CPLAT_OK を返し、Windows では closesocket() から、同じエラーを書き込んで 0 を返す。

    // Act
    cplat_socket_close(
        kSocket); // [手順] - 下位のクローズ API が直前エラーを書き換える状態で cplat_socket_close() を呼び出す。

    // Assert
    cplat_error_get_last(&actual);
    EXPECT_EQ(
        saved.domain,
        actual
            .domain); // [確認_正常系] - クローズ後の actual.domain が saved.domain と一致すること。(呼び出し前の直前エラーのドメインが残ることの確認)
    EXPECT_EQ(
        saved.result,
        actual
            .result); // [確認_正常系] - クローズ後の actual.result が saved.result と一致すること。(呼び出し前の直前エラーの結果コードが残ることの確認)
    EXPECT_EQ(
        saved.code,
        actual
            .code); // [確認_正常系] - クローズ後の actual.code が saved.code と一致すること。(呼び出し前の直前エラーのエラー値が残ることの確認)

    // Cleanup
    cplat_error_clear_last();
}
// [サブ手順参照 名前=socketTest.TearDown]

// 無効なソケットのシャットダウンが無視されることの確認
TEST_F(socketTest, shutdown_ignores_invalid_socket)
{
    // Arrange

    // Pre-Assert
    // [サブ手順参照 名前=socketTest.expectNoShutdownCall]
    expectNoShutdownCall();

    // Act
    cplat_socket_shutdown(
        CPLAT_INVALID_SOCKET); // [手順] - 無効なソケットを指定して cplat_socket_shutdown() を呼び出す。

    // Assert
    EXPECT_TRUE(
        verifyExpectations()); // [確認_正常系] - verifyExpectations() の戻り値が true であること。(無効なソケットの指定時に下位のシャットダウン API が呼び出されないことの確認)
}
// [サブ手順参照 名前=socketTest.TearDown]

// 有効なソケットのシャットダウンが OS API へ委譲されることの確認
TEST_F(socketTest, shutdown_calls_os_shutdown)
{
    // Arrange

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_sys_socket_, shutdown(_, _, _, (int)kSocket, SHUT_RDWR)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - shutdown() が kSocket、SHUT_RDWR を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - shutdown() から 0 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, shutdown(_, _, _, (SOCKET)kSocket, SD_BOTH)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - shutdown() が kSocket、SD_BOTH を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - shutdown() から 0 を返す。
#endif /* PLATFORM_ */
    // [Pre-Assert手順] - shutdown() から成功を表す 0 を返す。

    // Act
    cplat_socket_shutdown(kSocket); // [手順] - 有効なソケットを指定して cplat_socket_shutdown() を呼び出す。

    // Assert
    EXPECT_TRUE(
        verifyExpectations()); // [確認_正常系] - verifyExpectations() の戻り値が true であること。(下位のシャットダウン API への呼び出し期待が満たされることの確認)
}
// [サブ手順参照 名前=socketTest.TearDown]

// bind、listen、connect の引数不正が拒否されることの確認
TEST_F(socketTest, connection_operations_reject_invalid_arguments)
{
    // Arrange
    cplat_error detail = {};

    // Pre-Assert

    // Act
    int actual_ret_bind_socket =
        cplat_socket_bind(CPLAT_INVALID_SOCKET, &kEndpoint,
                          &detail); // [手順] - 無効なソケットを指定して cplat_socket_bind() を呼び出す。
    int actual_ret_bind_endpoint = cplat_socket_bind(
        kSocket, nullptr, &detail); // [手順] - エンドポイントに nullptr を指定して cplat_socket_bind() を呼び出す。
    int actual_ret_listen_socket = cplat_socket_listen(
        CPLAT_INVALID_SOCKET, 1, &detail); // [手順] - 無効なソケットを指定して cplat_socket_listen() を呼び出す。
    int actual_ret_listen_backlog = cplat_socket_listen(
        kSocket, -1, &detail); // [手順] - 負の待ち受け数を指定して cplat_socket_listen() を呼び出す。
    int actual_ret_connect_socket =
        cplat_socket_connect(CPLAT_INVALID_SOCKET, &kEndpoint,
                             &detail); // [手順] - 無効なソケットを指定して cplat_socket_connect() を呼び出す。
    int actual_ret_connect_endpoint = cplat_socket_connect(
        kSocket, nullptr, &detail); // [手順] - エンドポイントに nullptr を指定して cplat_socket_connect() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_bind_socket); // [確認_異常系] - 無効なソケットを指定した cplat_socket_bind() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_bind_endpoint); // [確認_異常系] - nullptr のエンドポイントを指定した cplat_socket_bind() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_listen_socket); // [確認_異常系] - 無効なソケットを指定した cplat_socket_listen() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_listen_backlog); // [確認_異常系] - 負の待ち受け数を指定した cplat_socket_listen() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_connect_socket); // [確認_異常系] - 無効なソケットを指定した cplat_socket_connect() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_connect_endpoint); // [確認_異常系] - nullptr のエンドポイントを指定した cplat_socket_connect() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}
// [サブ手順参照 名前=socketTest.TearDown]

// bind、listen、connect が成功することの確認
TEST_F(socketTest, connection_operations_succeed)
{
    // Arrange

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_sys_socket_, bind(_, _, _, (int)kSocket, _, _)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - bind() が kSocket を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - bind() から 0 を返す。
    EXPECT_CALL(mock_sys_socket_, listen(_, _, _, (int)kSocket, SOMAXCONN)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - listen() が kSocket、SOMAXCONN を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - listen() から 0 を返す。
    EXPECT_CALL(mock_sys_socket_, listen(_, _, _, (int)kSocket, 3)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - listen() が kSocket、3 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - listen() から 0 を返す。
    EXPECT_CALL(mock_sys_socket_, connect(_, _, _, (int)kSocket, _, _)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - connect() が kSocket を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - connect() から 0 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, bind(_, _, _, (SOCKET)kSocket, _, _)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - bind() が kSocket を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - bind() から 0 を返す。
    EXPECT_CALL(mock_winsock_, listen(_, _, _, (SOCKET)kSocket, SOMAXCONN)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - listen() が kSocket、SOMAXCONN を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - listen() から 0 を返す。
    EXPECT_CALL(mock_winsock_, listen(_, _, _, (SOCKET)kSocket, 3)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - listen() が kSocket、3 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - listen() から 0 を返す。
    EXPECT_CALL(mock_winsock_, connect(_, _, _, (SOCKET)kSocket, _, _)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - connect() が kSocket を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - connect() から 0 を返す。
#endif /* PLATFORM_ */
    // [Pre-Assert手順] - bind() から 0 を返し、listen() と connect() からも成功を表す 0 を返す。

    // Act
    int actual_ret_bind = cplat_socket_bind(
        kSocket, &kEndpoint, nullptr); // [手順] - bind() が成功する状態で cplat_socket_bind() を呼び出す。
    int actual_ret_listen_default =
        cplat_socket_listen(kSocket, CPLAT_SOCKET_BACKLOG_DEFAULT,
                            nullptr); // [手順] - 待ち受け数に既定値を指定して cplat_socket_listen() を呼び出す。
    int actual_ret_listen_explicit = cplat_socket_listen(
        kSocket, 3, nullptr); // [手順] - 明示した待ち受け数を指定して cplat_socket_listen() を呼び出す。
    int actual_ret_connect = cplat_socket_connect(
        kSocket, &kEndpoint, nullptr); // [手順] - connect() が成功する状態で cplat_socket_connect() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret_bind); // [確認_正常系] - cplat_socket_bind() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_listen_default); // [確認_正常系] - 既定の待ち受け数を指定した cplat_socket_listen() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_listen_explicit); // [確認_正常系] - 明示した待ち受け数を指定した cplat_socket_listen() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              actual_ret_connect); // [確認_正常系] - cplat_socket_connect() の戻り値が CPLAT_OK であること。
}
// [サブ手順参照 名前=socketTest.TearDown]

// bind、listen、connect の OS 失敗が通知されることの確認
TEST_F(socketTest, connection_operations_report_os_failures)
{
    // Arrange
    cplat_error detail = {};

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_sys_socket_, bind(_, _, _, (int)kSocket, _, _))
        .WillOnce(DoAll(Assign(&errno, EADDRINUSE), Return(-1)));
    // [Pre-Assert確認_異常系] - bind() が kSocket を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - bind() から、errno に EADDRINUSE を設定して -1 を返す。
    EXPECT_CALL(mock_sys_socket_, listen(_, _, _, (int)kSocket, _)).WillOnce(DoAll(Assign(&errno, EIO), Return(-1)));
    // [Pre-Assert確認_異常系] - listen() が kSocket を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - listen() から、errno に EIO を設定して -1 を返す。
    EXPECT_CALL(mock_sys_socket_, connect(_, _, _, (int)kSocket, _, _))
        .WillOnce(DoAll(Assign(&errno, ECONNREFUSED), Return(-1)));
    // [Pre-Assert確認_異常系] - connect() が kSocket を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - connect() から、errno に ECONNREFUSED を設定して -1 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, bind(_, _, _, (SOCKET)kSocket, _, _)).WillOnce(Return(SOCKET_ERROR));
    // [Pre-Assert確認_異常系] - bind() が kSocket を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - bind() から SOCKET_ERROR を返す。
    EXPECT_CALL(mock_winsock_, listen(_, _, _, (SOCKET)kSocket, _)).WillOnce(Return(SOCKET_ERROR));
    // [Pre-Assert確認_異常系] - listen() が kSocket を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - listen() から SOCKET_ERROR を返す。
    EXPECT_CALL(mock_winsock_, connect(_, _, _, (SOCKET)kSocket, _, _)).WillOnce(Return(SOCKET_ERROR));
    // [Pre-Assert確認_異常系] - connect() が kSocket を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - connect() から SOCKET_ERROR を返す。
    EXPECT_CALL(mock_winsock_, WSAGetLastError)
        .WillOnce(Return(WSAEADDRINUSE))
        .WillOnce(Return(WSAENETDOWN))
        .WillOnce(Return(WSAECONNREFUSED));
    // [Pre-Assert確認_異常系] - WSAGetLastError() が 3 回呼び出されること。
    // [Pre-Assert手順] - WSAGetLastError() から 1 回目は WSAEADDRINUSE、2 回目は WSAENETDOWN、3 回目は WSAECONNREFUSED を返す。
#endif /* PLATFORM_ */
    // [Pre-Assert手順] - bind() から、Linux では errno に EADDRINUSE を設定して -1、Windows では SOCKET_ERROR を返し、listen() と connect() からも失敗を返し、それぞれ errno または WSAGetLastError() で失敗要因を通知する。

    // Act
    int actual_ret_bind = cplat_socket_bind(
        kSocket, &kEndpoint, &detail); // [手順] - bind() が失敗する状態で cplat_socket_bind() を呼び出す。
    int actual_ret_listen = cplat_socket_listen(
        kSocket, 1, &detail); // [手順] - listen() が失敗する状態で cplat_socket_listen() を呼び出す。
    int actual_ret_connect = cplat_socket_connect(
        kSocket, &kEndpoint, &detail); // [手順] - connect() が失敗する状態で cplat_socket_connect() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_bind); // [確認_異常系] - OS 失敗時の cplat_socket_bind() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_listen); // [確認_異常系] - OS 失敗時の cplat_socket_listen() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_connect); // [確認_異常系] - OS 失敗時の cplat_socket_connect() の戻り値が CPLAT_ERR_UNKNOWN であること。
}
// [サブ手順参照 名前=socketTest.TearDown]

// 非ブロッキング connect の継続状態がプラットフォーム共通の結果コードになることの確認
TEST_F(socketTest, connect_reports_in_progress_for_nonblocking_completion)
{
    // Arrange
    cplat_error detail = {};

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_sys_socket_, connect(_, _, _, (int)kSocket, _, _))
        .WillOnce(
            DoAll(Assign(&errno, EINPROGRESS),
                  Return(-1))); // [Pre-Assert確認_正常系] - connect() が kSocket を指定して 1 回呼び出されること。
                                // [Pre-Assert手順] - connect() から、errno に EINPROGRESS を設定して -1 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, connect(_, _, _, (SOCKET)kSocket, _, _))
        .WillOnce(
            Return(SOCKET_ERROR)); // [Pre-Assert確認_正常系] - connect() が kSocket を指定して 1 回呼び出されること。
                                   // [Pre-Assert手順] - connect() から SOCKET_ERROR を返す。
    EXPECT_CALL(mock_winsock_, WSAGetLastError)
        .WillOnce(Return(WSAEWOULDBLOCK)); // [Pre-Assert確認_正常系] - WSAGetLastError() が 1 回呼び出されること。
                                           // [Pre-Assert手順] - WSAGetLastError() から WSAEWOULDBLOCK を返す。
#endif /* PLATFORM_ */

    // Act
    int result = cplat_socket_connect(
        kSocket, &kEndpoint,
        &detail); // [手順] - 非ブロッキング接続が継続中の状態で cplat_socket_connect() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_IN_PROGRESS,
              result); // [確認_正常系] - cplat_socket_connect() の戻り値が CPLAT_ERR_IN_PROGRESS であること。
#if defined(PLATFORM_LINUX)
    EXPECT_EQ(
        CPLAT_CAUSE_IN_PROGRESS,
        cplat_error_get_cause(
            &detail)); // [確認_正常系] - Linux の cplat_error_get_cause() の戻り値が CPLAT_CAUSE_IN_PROGRESS であること。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_EQ(
        CPLAT_CAUSE_WOULD_BLOCK,
        cplat_error_get_cause(
            &detail)); // [確認_正常系] - Windows の cplat_error_get_cause() の戻り値が CPLAT_CAUSE_WOULD_BLOCK であること。(生の詳細要因であることの確認)
#endif /* PLATFORM_ */
}
// [サブ手順参照 名前=socketTest.TearDown]

// accept が無効な引数を拒否することの確認
TEST_F(socketTest, accept_rejects_invalid_arguments)
{
    // Arrange
    cplat_socket accepted = kSocket;

    // Pre-Assert

    // Act
    int actual_ret_socket =
        cplat_socket_accept(CPLAT_INVALID_SOCKET, nullptr, &accepted,
                            nullptr); // [手順] - 無効な待ち受けソケットを指定して cplat_socket_accept() を呼び出す。
    int actual_ret_output = cplat_socket_accept(
        kSocket, nullptr, nullptr, nullptr); // [手順] - 出力先に nullptr を指定して cplat_socket_accept() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_socket); // [確認_異常系] - 無効なソケットを指定した cplat_socket_accept() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_output); // [確認_異常系] - 出力先が nullptr の cplat_socket_accept() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}
// [サブ手順参照 名前=socketTest.TearDown]

// accept が接続元エンドポイントと新しいソケットを返すことの確認
TEST_F(socketTest, accept_returns_peer_and_socket)
{
    // Arrange
    cplat_ipv4_endpoint peer = {};
    cplat_socket accepted = CPLAT_INVALID_SOCKET;

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_sys_socket_, accept(_, _, _, (int)kSocket, _, _))
        .WillRepeatedly(
            [](const char *, const int, const char *, int, struct sockaddr *address, socklen_t *length)
            {
                struct sockaddr_in *native = reinterpret_cast<struct sockaddr_in *>(address);
                native->sin_addr.s_addr = cplat_ipv4_addr_loopback();
                native->sin_port = cplat_hton16((uint16_t)54321U);
                *length = (socklen_t)sizeof(*native);
                return 8;
            });

    // [Pre-Assert確認_正常系] - accept() が kSocket を指定して任意の回数呼び出されること。
    // [Pre-Assert手順] - accept() から、address の IPv4 アドレスに cplat_ipv4_addr_loopback()、ポートに cplat_hton16() で変換した 54321U、*length に sizeof(*native) を設定して 8 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, accept(_, _, _, (SOCKET)kSocket, _, _))
        .WillRepeatedly(
            [](const char *, const int, const char *, SOCKET, struct sockaddr *address, int *length)
            {
                SOCKADDR_IN *native = reinterpret_cast<SOCKADDR_IN *>(address);
                native->sin_addr.S_un.S_addr = cplat_ipv4_addr_loopback();
                native->sin_port = cplat_hton16((uint16_t)54321U);
                *length = (int)sizeof(*native);
                return (SOCKET)8;
            });
    EXPECT_CALL(mock_winsock_, WSAEventSelect(_, _, _, (SOCKET)8, (WSAEVENT)nullptr, 0))
        .Times(2)
        .WillRepeatedly(Return(0));
    // [Pre-Assert確認_正常系] - 受け付けたソケットのイベント登録解除が 2 回成功すること。
    // [Pre-Assert手順] - WSAEventSelect() から 0 を返す。
    EXPECT_CALL(mock_winsock_, ioctlsocket(_, _, _, (SOCKET)8, FIONBIO, _))
        .Times(2)
        .WillRepeatedly([](const char *, const int, const char *, SOCKET, const long command, u_long *nonblocking) {
            EXPECT_EQ(FIONBIO, command);
            EXPECT_NE(nullptr, nonblocking);
            if (nonblocking == nullptr)
            {
                return SOCKET_ERROR;
            }
            EXPECT_EQ(0UL, *nonblocking);
            return 0;
        });
    // [Pre-Assert確認_正常系] - 受け付けたソケットのブロッキング化が 2 回成功すること。
    // [Pre-Assert手順] - ioctlsocket() から 0 を返す。

    // [Pre-Assert確認_正常系] - accept() が kSocket を指定して任意の回数呼び出されること。
    // [Pre-Assert手順] - accept() から、address の IPv4 アドレスに cplat_ipv4_addr_loopback()、ポートに cplat_hton16() で変換した 54321U、*length に sizeof(*native) を設定して 8 を返す。
#endif /* PLATFORM_ */
    // [Pre-Assert手順] - accept() から、接続元のエンドポイントを出力引数に設定して新しいソケットのハンドル 8 を返す。

    // Act
    int actual_ret = cplat_socket_accept(kSocket, &peer, &accepted,
                                         nullptr); // [手順] - 接続を受け付ける状態で cplat_socket_accept() を呼び出す。
    int actual_ret_without_peer = cplat_socket_accept(
        kSocket, nullptr, &accepted,
        nullptr); // [手順] - 接続元エンドポイントの出力先に nullptr を指定して cplat_socket_accept() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret); // [確認_正常系] - cplat_socket_accept() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_without_peer); // [確認_正常系] - 接続元エンドポイントを要求しない cplat_socket_accept() の戻り値が CPLAT_OK であること。
    EXPECT_EQ((cplat_socket)8,
              accepted); // [確認_正常系] - accepted が 8 であること。(受け付けたソケットのハンドルであることの確認)
    EXPECT_EQ(
        cplat_ipv4_addr_loopback(),
        peer.address); // [確認_正常系] - peer.address が cplat_ipv4_addr_loopback() であること。(接続元アドレスであることの確認)
    EXPECT_EQ(
        cplat_hton16((uint16_t)54321U),
        peer.port); // [確認_正常系] - peer.port が 54321U を変換した cplat_hton16() の戻り値と一致すること。(接続元ポートであることの確認)
}
// [サブ手順参照 名前=socketTest.TearDown]

// accept の OS 失敗が通知されることの確認
TEST_F(socketTest, accept_reports_os_failure)
{
    // Arrange
    cplat_socket accepted = kSocket;
    cplat_error detail = {};

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_sys_socket_, accept(_, _, _, (int)kSocket, _, _)).WillOnce(DoAll(Assign(&errno, EIO), Return(-1)));
    // [Pre-Assert確認_異常系] - accept() が kSocket を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - accept() から、errno に EIO を設定して -1 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, accept(_, _, _, (SOCKET)kSocket, _, _)).WillOnce(Return(INVALID_SOCKET));
    // [Pre-Assert確認_異常系] - accept() が kSocket を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - accept() から INVALID_SOCKET を返す。
    EXPECT_CALL(mock_winsock_, WSAGetLastError).WillOnce(Return(WSAENETDOWN));
    // [Pre-Assert確認_異常系] - WSAGetLastError() が 1 回呼び出されること。
    // [Pre-Assert手順] - WSAGetLastError() から WSAENETDOWN を返す。
#endif /* PLATFORM_ */
    // [Pre-Assert手順] - accept() から、Linux では errno に EIO を設定して -1、Windows では INVALID_SOCKET を返し、WSAGetLastError() から WSAENETDOWN を返す。

    // Act
    int actual_ret = cplat_socket_accept(
        kSocket, nullptr, &accepted, &detail); // [手順] - accept() が失敗する状態で cplat_socket_accept() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret); // [確認_異常系] - OS 失敗時の cplat_socket_accept() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(CPLAT_INVALID_SOCKET,
              accepted); // [確認_異常系] - OS 失敗時の accepted が CPLAT_INVALID_SOCKET であること。
}
// [サブ手順参照 名前=socketTest.TearDown]

// 保留エラーの取得結果が分類されることの確認
TEST_F(socketTest, pending_error_reports_empty_pending_and_failure)
{
    // Arrange
    cplat_error detail = {};

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_sys_socket_, getsockopt(_, _, _, (int)kSocket, SOL_SOCKET, SO_ERROR, _, _))
        .WillOnce(
            [](const char *, const int, const char *, int, int, int, void *value, socklen_t *)
            {
                *static_cast<int *>(value) = 0;
                return 0;
            })
        .WillOnce(
            [](const char *, const int, const char *, int, int, int, void *value, socklen_t *)
            {
                *static_cast<int *>(value) = ECONNREFUSED;
                return 0;
            })
        .WillOnce(DoAll(Assign(&errno, EBADF), Return(-1)));
    // [Pre-Assert確認_異常系] - getsockopt() が kSocket、SOL_SOCKET、SO_ERROR を指定して 3 回呼び出されること。
    // [Pre-Assert手順] - getsockopt() から、1 回目は value が指す int に 0 を設定して 0、2 回目は value が指す int に ECONNREFUSED を設定して 0、3 回目は errno に EBADF を設定して -1 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, getsockopt(_, _, _, (SOCKET)kSocket, SOL_SOCKET, SO_ERROR, _, _))
        .WillOnce(
            [](const char *, const int, const char *, SOCKET, int, int, char *value, int *)
            {
                *reinterpret_cast<int *>(value) = 0;
                return 0;
            })
        .WillOnce(
            [](const char *, const int, const char *, SOCKET, int, int, char *value, int *)
            {
                *reinterpret_cast<int *>(value) = WSAECONNREFUSED;
                return 0;
            })
        .WillOnce(Return(SOCKET_ERROR));
    // [Pre-Assert確認_異常系] - getsockopt() が kSocket、SOL_SOCKET、SO_ERROR を指定して 3 回呼び出されること。
    // [Pre-Assert手順] - getsockopt() から、1 回目は value が指す int に 0 を設定して 0、2 回目は value が指す int に WSAECONNREFUSED を設定して 0、3 回目は SOCKET_ERROR を返す。
    EXPECT_CALL(mock_winsock_, WSAGetLastError).WillOnce(Return(WSAENOTSOCK));
    // [Pre-Assert確認_異常系] - WSAGetLastError() が 1 回呼び出されること。
    // [Pre-Assert手順] - WSAGetLastError() から WSAENOTSOCK を返す。
#endif /* PLATFORM_ */
    // [Pre-Assert手順] - getsockopt() から、1 回目は保留エラーなし、2 回目は保留エラーありを出力引数に設定して 0 を返し、3 回目は取得失敗を表す -1 (Windows では SOCKET_ERROR) を返す。

    // Act
    int actual_ret_invalid = cplat_socket_get_pending_error(
        CPLAT_INVALID_SOCKET,
        &detail); // [手順] - 無効なソケットを指定して cplat_socket_get_pending_error() を呼び出す。
    int actual_ret_empty = cplat_socket_get_pending_error(
        kSocket, &detail); // [手順] - 保留エラーがない状態で cplat_socket_get_pending_error() を呼び出す。
    int actual_ret_pending = cplat_socket_get_pending_error(
        kSocket, &detail); // [手順] - 保留エラーがある状態で cplat_socket_get_pending_error() を呼び出す。
    int actual_ret_failure = cplat_socket_get_pending_error(
        kSocket, &detail); // [手順] - getsockopt() が失敗する状態で cplat_socket_get_pending_error() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_invalid); // [確認_異常系] - 無効なソケットを指定した cplat_socket_get_pending_error() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_empty); // [確認_正常系] - 保留エラーがない場合の cplat_socket_get_pending_error() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_pending); // [確認_異常系] - 保留エラーがある場合の cplat_socket_get_pending_error() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_failure); // [確認_異常系] - getsockopt() 失敗時の cplat_socket_get_pending_error() の戻り値が CPLAT_ERR_UNKNOWN であること。
}
// [サブ手順参照 名前=socketTest.TearDown]

// 受信用の領域の大きさの取得結果が分類されることの確認
TEST_F(socketTest, receive_buffer_size_reports_value_zero_and_failure)
{
    // Arrange
    cplat_error detail = {};
    size_t size_valid = 0U;
    size_t size_zero = 1U;
    size_t size_negative = 1U;
    size_t size_failure = 1U;

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_sys_socket_, getsockopt(_, _, _, (int)kSocket, SOL_SOCKET, SO_RCVBUF, _, _))
        .WillOnce(
            [](const char *, const int, const char *, int, int, int, void *value, socklen_t *)
            {
                *static_cast<int *>(value) = 425984;
                return 0;
            })
        .WillOnce(
            [](const char *, const int, const char *, int, int, int, void *value, socklen_t *)
            {
                *static_cast<int *>(value) = 0;
                return 0;
            })
        .WillOnce(
            [](const char *, const int, const char *, int, int, int, void *value, socklen_t *)
            {
                *static_cast<int *>(value) = -1;
                return 0;
            })
        .WillOnce(DoAll(Assign(&errno, EBADF), Return(-1)));
    // [Pre-Assert確認_異常系] - getsockopt() が kSocket、SOL_SOCKET、SO_RCVBUF を指定して 4 回呼び出されること。
    // [Pre-Assert手順] - getsockopt() から、1 回目は value が指す int に 425984、2 回目は 0、3 回目は -1 を設定して 0、4 回目は errno に EBADF を設定して -1 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, getsockopt(_, _, _, (SOCKET)kSocket, SOL_SOCKET, SO_RCVBUF, _, _))
        .WillOnce(
            [](const char *, const int, const char *, SOCKET, int, int, char *value, int *)
            {
                *reinterpret_cast<int *>(value) = 425984;
                return 0;
            })
        .WillOnce(
            [](const char *, const int, const char *, SOCKET, int, int, char *value, int *)
            {
                *reinterpret_cast<int *>(value) = 0;
                return 0;
            })
        .WillOnce(
            [](const char *, const int, const char *, SOCKET, int, int, char *value, int *)
            {
                *reinterpret_cast<int *>(value) = -1;
                return 0;
            })
        .WillOnce(Return(SOCKET_ERROR));
    // [Pre-Assert確認_異常系] - getsockopt() が kSocket、SOL_SOCKET、SO_RCVBUF を指定して 4 回呼び出されること。
    // [Pre-Assert手順] - getsockopt() から、1 回目は value が指す int に 425984、2 回目は 0、3 回目は -1 を設定して 0、4 回目は SOCKET_ERROR を返す。
    EXPECT_CALL(mock_winsock_, WSAGetLastError).WillOnce(Return(WSAENOTSOCK));
    // [Pre-Assert確認_異常系] - WSAGetLastError() が 1 回呼び出されること。
    // [Pre-Assert手順] - WSAGetLastError() から WSAENOTSOCK を返す。
#endif /* PLATFORM_ */

    // Act
    int actual_ret_invalid_socket = cplat_socket_get_receive_buffer_size(
        CPLAT_INVALID_SOCKET, &size_valid,
        &detail); // [手順] - 無効なソケットを指定して cplat_socket_get_receive_buffer_size() を呼び出す。
    int actual_ret_null_output = cplat_socket_get_receive_buffer_size(
        kSocket, nullptr, &detail); // [手順] - 格納先に nullptr を指定して cplat_socket_get_receive_buffer_size() を呼び出す。
    int actual_ret_valid = cplat_socket_get_receive_buffer_size(
        kSocket, &size_valid, &detail); // [手順] - 正の値を取得できる状態で cplat_socket_get_receive_buffer_size() を呼び出す。
    int actual_ret_zero = cplat_socket_get_receive_buffer_size(
        kSocket, &size_zero, &detail); // [手順] - 0 を取得できる状態で cplat_socket_get_receive_buffer_size() を呼び出す。
    int actual_ret_negative = cplat_socket_get_receive_buffer_size(
        kSocket, &size_negative, &detail); // [手順] - 負の値を取得できる状態で cplat_socket_get_receive_buffer_size() を呼び出す。
    int actual_ret_failure = cplat_socket_get_receive_buffer_size(
        kSocket, &size_failure, &detail); // [手順] - getsockopt() が失敗する状態で cplat_socket_get_receive_buffer_size() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_invalid_socket); // [確認_異常系] - 無効なソケットを指定した cplat_socket_get_receive_buffer_size() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_output); // [確認_異常系] - 格納先が nullptr の cplat_socket_get_receive_buffer_size() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_valid); // [確認_正常系] - 正の値を取得した cplat_socket_get_receive_buffer_size() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(425984U,
              size_valid); // [確認_正常系] - 正の値を取得した場合の size_valid が取得値そのままの 425984 であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_zero); // [確認_正常系] - 0 を取得した cplat_socket_get_receive_buffer_size() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0U, size_zero); // [確認_正常系] - 0 を取得した場合の size_zero が 0 であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_negative); // [確認_異常系] - 負の値を取得した cplat_socket_get_receive_buffer_size() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(1U, size_negative); // [確認_異常系] - 負の値を取得した場合の size_negative が変更されないこと。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_failure); // [確認_異常系] - getsockopt() 失敗時の cplat_socket_get_receive_buffer_size() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(1U, size_failure); // [確認_異常系] - getsockopt() 失敗時の size_failure が変更されないこと。
}
// [サブ手順参照 名前=socketTest.TearDown]

// 受信用の領域の大きさの設定の成功、引数不正、OS 失敗が処理されることの確認
TEST_F(socketTest, set_receive_buffer_size_reports_success_invalid_and_failure)
{
    // Arrange
    cplat_error detail = {};
    constexpr size_t kRequestedSize = 4194304U;
    constexpr size_t kOversized = static_cast<size_t>(INT_MAX) + 1U;

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_sys_socket_, setsockopt(_, _, _, (int)kSocket, SOL_SOCKET, SO_RCVBUF, _, sizeof(int)))
        .WillOnce(
            [](const char *, const int, const char *, int, int, int, const void *value, socklen_t)
            {
                EXPECT_EQ(
                    4194304,
                    *static_cast<const int *>(value)); // [Pre-Assert確認_正常系] - *value が 4194304 であること。
                return 0;
            })
        .WillOnce(DoAll(Assign(&errno, EBADF), Return(-1)));
    // [Pre-Assert確認_異常系] - setsockopt() が kSocket、SOL_SOCKET、SO_RCVBUF、sizeof(int) を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - setsockopt() から、1 回目は *value が 4194304 であることを確認して 0、2 回目は errno に EBADF を設定して -1 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, setsockopt(_, _, _, (SOCKET)kSocket, SOL_SOCKET, SO_RCVBUF, _, (int)sizeof(int)))
        .WillOnce(
            [](const char *, const int, const char *, SOCKET, int, int, const char *value, int)
            {
                EXPECT_EQ(
                    4194304,
                    *reinterpret_cast<const int *>(value)); // [Pre-Assert確認_正常系] - *value が 4194304 であること。
                return 0;
            })
        .WillOnce(Return(SOCKET_ERROR));
    // [Pre-Assert確認_異常系] - setsockopt() が kSocket、SOL_SOCKET、SO_RCVBUF、sizeof(int) を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - setsockopt() から、1 回目は *value が 4194304 であることを確認して 0、2 回目は SOCKET_ERROR を返す。
    EXPECT_CALL(mock_winsock_, WSAGetLastError).WillOnce(Return(WSAENOTSOCK));
    // [Pre-Assert確認_異常系] - WSAGetLastError() が 1 回呼び出されること。
    // [Pre-Assert手順] - WSAGetLastError() から WSAENOTSOCK を返す。
#endif /* PLATFORM_ */

    // Act
    int actual_ret_invalid_socket = cplat_socket_set_receive_buffer_size(
        CPLAT_INVALID_SOCKET, kRequestedSize,
        &detail); // [手順] - 無効なソケットを指定して cplat_socket_set_receive_buffer_size() を呼び出す。
    int actual_ret_zero_size = cplat_socket_set_receive_buffer_size(
        kSocket, 0U, &detail); // [手順] - 大きさに 0 を指定して cplat_socket_set_receive_buffer_size() を呼び出す。
    int actual_ret_oversized = cplat_socket_set_receive_buffer_size(
        kSocket, kOversized,
        &detail); // [手順] - INT_MAX を超える大きさを指定して cplat_socket_set_receive_buffer_size() を呼び出す。
    int actual_ret_valid = cplat_socket_set_receive_buffer_size(
        kSocket, kRequestedSize,
        &detail); // [手順] - 有効な大きさを指定して setsockopt() が成功する状態で cplat_socket_set_receive_buffer_size() を呼び出す。
    int actual_ret_failure = cplat_socket_set_receive_buffer_size(
        kSocket, kRequestedSize,
        &detail); // [手順] - setsockopt() が失敗する状態で cplat_socket_set_receive_buffer_size() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_invalid_socket); // [確認_異常系] - 無効なソケットを指定した cplat_socket_set_receive_buffer_size() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_zero_size); // [確認_異常系] - 大きさに 0 を指定した cplat_socket_set_receive_buffer_size() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_oversized); // [確認_異常系] - INT_MAX を超える大きさを指定した cplat_socket_set_receive_buffer_size() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_valid); // [確認_正常系] - 有効な大きさを設定した cplat_socket_set_receive_buffer_size() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_failure); // [確認_異常系] - setsockopt() 失敗時の cplat_socket_set_receive_buffer_size() の戻り値が CPLAT_ERR_UNKNOWN であること。
}
// [サブ手順参照 名前=socketTest.TearDown]

// 非ブロッキング設定の引数不正と OS 失敗が処理されることの確認
TEST_F(socketTest, nonblocking_reports_invalid_and_os_failure)
{
    // Arrange
    cplat_error detail = {};

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_fcntl_, fcntl(_, _, _, (int)kSocket, F_GETFL, 0))
        .WillOnce(Return(O_RDONLY))
        .WillOnce(Return(O_NONBLOCK))
        .WillOnce(DoAll(Assign(&errno, EBADF), Return(-1)));
    // [Pre-Assert確認_異常系] - fcntl() が kSocket、F_GETFL、0 を指定して 3 回呼び出されること。
    // [Pre-Assert手順] - fcntl() から、1 回目は O_RDONLY、2 回目は O_NONBLOCK、3 回目は errno に EBADF を設定して -1 を返す。
    EXPECT_CALL(mock_fcntl_, fcntl(_, _, _, (int)kSocket, F_SETFL, O_RDONLY | O_NONBLOCK)).WillOnce(Return(0));
    // [Pre-Assert確認_異常系] - fcntl() が kSocket、F_SETFL、O_RDONLY | O_NONBLOCK を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - fcntl() から 0 を返す。
    EXPECT_CALL(mock_fcntl_, fcntl(_, _, _, (int)kSocket, F_SETFL, 0)).WillOnce(Return(0));
    // [Pre-Assert確認_異常系] - fcntl() が kSocket、F_SETFL、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - fcntl() から 0 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, ioctlsocket(_, _, _, (SOCKET)kSocket, FIONBIO, _))
        .WillOnce(
            [](const char *, const int, const char *, SOCKET, long, u_long *mode)
            {
                EXPECT_EQ(
                    1UL,
                    *mode); // [Pre-Assert確認_正常系] - *mode が 1UL であること。(非ブロッキング有効を表すことの確認)
                return 0;
            })
        .WillOnce(
            [](const char *, const int, const char *, SOCKET, long, u_long *mode)
            {
                EXPECT_EQ(
                    0UL,
                    *mode); // [Pre-Assert確認_正常系] - *mode が 0UL であること。(非ブロッキング無効を表すことの確認)
                return 0;
            })
        .WillOnce(Return(SOCKET_ERROR));
    // [Pre-Assert確認_異常系] - ioctlsocket() が kSocket、FIONBIO を指定して 3 回呼び出されること。
    // [Pre-Assert手順] - ioctlsocket() から 1 回目と 2 回目は 0、3 回目は SOCKET_ERROR を返す。
    EXPECT_CALL(mock_winsock_, WSAGetLastError).WillOnce(Return(WSAENETDOWN));
    // [Pre-Assert確認_異常系] - WSAGetLastError() が 1 回呼び出されること。
    // [Pre-Assert手順] - WSAGetLastError() から WSAENETDOWN を返す。
#endif /* PLATFORM_ */
    // [Pre-Assert手順] - fcntl() から、有効化と無効化に必要なフラグと設定成功の 0 を返したのち、errno に EBADF を設定して -1 を返し、Windows では ioctlsocket() から 1 回目と 2 回目は 0、3 回目は SOCKET_ERROR を返す。

    // Act
    int actual_ret_invalid = cplat_socket_set_nonblocking(
        CPLAT_INVALID_SOCKET, 1,
        &detail); // [手順] - 無効なソケットを指定して cplat_socket_set_nonblocking() を呼び出す。
    int actual_ret_enable = cplat_socket_set_nonblocking(
        kSocket, 1, &detail); // [手順] - 非ブロッキングの有効化を指定して cplat_socket_set_nonblocking() を呼び出す。
    int actual_ret_disable = cplat_socket_set_nonblocking(
        kSocket, 0, &detail); // [手順] - 非ブロッキングの無効化を指定して cplat_socket_set_nonblocking() を呼び出す。
    int actual_ret_failure = cplat_socket_set_nonblocking(
        kSocket, 1, &detail); // [手順] - 非ブロッキング設定が失敗する状態で cplat_socket_set_nonblocking() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_invalid); // [確認_異常系] - 無効なソケットを指定した cplat_socket_set_nonblocking() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_enable); // [確認_正常系] - 非ブロッキングを有効にした cplat_socket_set_nonblocking() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_disable); // [確認_正常系] - 非ブロッキングを無効にした cplat_socket_set_nonblocking() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_failure); // [確認_異常系] - 非ブロッキング設定失敗時の cplat_socket_set_nonblocking() の戻り値が CPLAT_ERR_UNKNOWN であること。
}
// [サブ手順参照 名前=socketTest.TearDown]

#if defined(PLATFORM_LINUX)
// F_SETFL の失敗が通知されることの確認
TEST_F(socketTest, nonblocking_reports_set_failure)
{
    // Arrange
    cplat_error detail = {};

    // Pre-Assert
    EXPECT_CALL(mock_fcntl_, fcntl(_, _, _, (int)kSocket, F_GETFL, 0)).WillOnce(Return(O_RDONLY));
    // [Pre-Assert確認_異常系] - fcntl() が kSocket、F_GETFL、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - fcntl() から、現在の設定の取得では O_RDONLY を返し、その後の設定の反映では errno に EIO を設定して -1 を返す。
    EXPECT_CALL(mock_fcntl_, fcntl(_, _, _, (int)kSocket, F_SETFL, O_NONBLOCK))
        .WillOnce(DoAll(
            Assign(&errno, EIO),
            Return(
                -1))); // [Pre-Assert確認_異常系] - fcntl() が kSocket、F_SETFL、O_NONBLOCK を指定して 1 回呼び出されること。
                       // [Pre-Assert手順] - fcntl() から、errno に EIO を設定して -1 を返す。

    // Act
    int actual_ret = cplat_socket_set_nonblocking(
        kSocket, 1, &detail); // [手順] - F_SETFL の設定が失敗する状態で cplat_socket_set_nonblocking() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret); // [確認_異常系] - F_SETFL 失敗時の cplat_socket_set_nonblocking() の戻り値が CPLAT_ERR_UNKNOWN であること。
}
// [サブ手順参照 名前=socketTest.TearDown]
#endif /* PLATFORM_LINUX */

// ソケット オプションの設定が成功することの確認
TEST_F(socketTest, socket_options_succeed)
{
    // Arrange
    cplat_error detail = {};

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_sys_socket_, setsockopt(_, _, _, (int)kSocket, SOL_SOCKET, SO_REUSEADDR, _, _))
        .WillOnce(Return(0))
        .WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - setsockopt() が kSocket、SOL_SOCKET、SO_REUSEADDR を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - setsockopt() から 0 を返す。
    EXPECT_CALL(mock_sys_socket_, setsockopt(_, _, _, (int)kSocket, SOL_SOCKET, SO_BROADCAST, _, _))
        .WillOnce(Return(0))
        .WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - setsockopt() が kSocket、SOL_SOCKET、SO_BROADCAST を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - setsockopt() から 0 を返す。
    EXPECT_CALL(mock_sys_socket_, setsockopt(_, _, _, (int)kSocket, IPPROTO_IP, IP_MULTICAST_IF, _, _))
        .WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - setsockopt() が kSocket、IPPROTO_IP、IP_MULTICAST_IF を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - setsockopt() から 0 を返す。
    EXPECT_CALL(mock_sys_socket_, setsockopt(_, _, _, (int)kSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, _, _))
        .WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - setsockopt() が kSocket、IPPROTO_IP、IP_ADD_MEMBERSHIP を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - setsockopt() から 0 を返す。
    EXPECT_CALL(mock_sys_socket_, setsockopt(_, _, _, (int)kSocket, IPPROTO_IP, IP_DROP_MEMBERSHIP, _, _))
        .WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - setsockopt() が kSocket、IPPROTO_IP、IP_DROP_MEMBERSHIP を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - setsockopt() から 0 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, setsockopt(_, _, _, (SOCKET)kSocket, SOL_SOCKET, SO_REUSEADDR, _, _))
        .WillOnce(Return(0))
        .WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - setsockopt() が kSocket、SOL_SOCKET、SO_REUSEADDR を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - setsockopt() から 0 を返す。
    EXPECT_CALL(mock_winsock_, setsockopt(_, _, _, (SOCKET)kSocket, SOL_SOCKET, SO_BROADCAST, _, _))
        .WillOnce(Return(0))
        .WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - setsockopt() が kSocket、SOL_SOCKET、SO_BROADCAST を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - setsockopt() から 0 を返す。
    EXPECT_CALL(mock_winsock_, setsockopt(_, _, _, (SOCKET)kSocket, IPPROTO_IP, IP_MULTICAST_IF, _, _))
        .WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - setsockopt() が kSocket、IPPROTO_IP、IP_MULTICAST_IF を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - setsockopt() から 0 を返す。
    EXPECT_CALL(mock_winsock_, setsockopt(_, _, _, (SOCKET)kSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, _, _))
        .WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - setsockopt() が kSocket、IPPROTO_IP、IP_ADD_MEMBERSHIP を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - setsockopt() から 0 を返す。
    EXPECT_CALL(mock_winsock_, setsockopt(_, _, _, (SOCKET)kSocket, IPPROTO_IP, IP_DROP_MEMBERSHIP, _, _))
        .WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - setsockopt() が kSocket、IPPROTO_IP、IP_DROP_MEMBERSHIP を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - setsockopt() から 0 を返す。
#endif /* PLATFORM_ */
    // [Pre-Assert手順] - setsockopt() から成功を表す 0 を返す。

    // Act
    int actual_ret_reuse = cplat_socket_set_reuse_address(
        kSocket, 1, &detail); // [手順] - アドレス再利用の有効化を指定して cplat_socket_set_reuse_address() を呼び出す。
    int actual_ret_reuse_disabled = cplat_socket_set_reuse_address(
        kSocket, 0, &detail); // [手順] - アドレス再利用の無効化を指定して cplat_socket_set_reuse_address() を呼び出す。
    int actual_ret_broadcast = cplat_socket_set_broadcast(
        kSocket, 0, &detail); // [手順] - ブロードキャストの無効化を指定して cplat_socket_set_broadcast() を呼び出す。
    int actual_ret_broadcast_enabled = cplat_socket_set_broadcast(
        kSocket, 1, &detail); // [手順] - ブロードキャストの有効化を指定して cplat_socket_set_broadcast() を呼び出す。
    int actual_ret_interface = cplat_socket_set_multicast_interface(
        kSocket, cplat_ipv4_addr_loopback(),
        &detail); // [手順] - マルチキャスト インターフェイスを指定して cplat_socket_set_multicast_interface() を呼び出す。
    int actual_ret_join = cplat_socket_join_multicast_group(
        kSocket, cplat_ipv4_addr_loopback(), CPLAT_IPV4_ADDR_ANY,
        &detail); // [手順] - 参加するマルチキャスト グループを指定して cplat_socket_join_multicast_group() を呼び出す。
    int actual_ret_leave = cplat_socket_leave_multicast_group(
        kSocket, cplat_ipv4_addr_loopback(), CPLAT_IPV4_ADDR_ANY,
        &detail); // [手順] - 離脱するマルチキャスト グループを指定して cplat_socket_leave_multicast_group() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_reuse); // [確認_正常系] - アドレス再利用を有効にした cplat_socket_set_reuse_address() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_reuse_disabled); // [確認_正常系] - アドレス再利用を無効にした cplat_socket_set_reuse_address() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_broadcast); // [確認_正常系] - ブロードキャストを無効にした cplat_socket_set_broadcast() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_broadcast_enabled); // [確認_正常系] - ブロードキャストを有効にした cplat_socket_set_broadcast() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_interface); // [確認_正常系] - cplat_socket_set_multicast_interface() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              actual_ret_join); // [確認_正常系] - cplat_socket_join_multicast_group() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_leave); // [確認_正常系] - cplat_socket_leave_multicast_group() の戻り値が CPLAT_OK であること。
}
// [サブ手順参照 名前=socketTest.TearDown]

// ソケット オプションの引数不正と OS 失敗が処理されることの確認
TEST_F(socketTest, socket_options_report_invalid_and_os_failure)
{
    // Arrange
    cplat_error detail = {};

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_sys_socket_, setsockopt(_, _, _, (int)kSocket, SOL_SOCKET, SO_REUSEADDR, _, _))
        .WillOnce(DoAll(Assign(&errno, ENOPROTOOPT), Return(-1)));
    // [Pre-Assert確認_異常系] - setsockopt() が kSocket、SOL_SOCKET、SO_REUSEADDR を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - setsockopt() から、errno に ENOPROTOOPT を設定して -1 を返す。
    EXPECT_CALL(mock_sys_socket_, setsockopt(_, _, _, (int)kSocket, IPPROTO_IP, IP_MULTICAST_IF, _, _))
        .WillOnce(DoAll(Assign(&errno, ENODEV), Return(-1)));
    // [Pre-Assert確認_異常系] - setsockopt() が kSocket、IPPROTO_IP、IP_MULTICAST_IF を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - setsockopt() から、errno に ENODEV を設定して -1 を返す。
    EXPECT_CALL(mock_sys_socket_, setsockopt(_, _, _, (int)kSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, _, _))
        .WillOnce(DoAll(Assign(&errno, ENODEV), Return(-1)));
    // [Pre-Assert確認_異常系] - setsockopt() が kSocket、IPPROTO_IP、IP_ADD_MEMBERSHIP を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - setsockopt() から、errno に ENODEV を設定して -1 を返す。
    EXPECT_CALL(mock_sys_socket_, setsockopt(_, _, _, (int)kSocket, IPPROTO_IP, IP_DROP_MEMBERSHIP, _, _))
        .WillOnce(DoAll(Assign(&errno, ENODEV), Return(-1)));
    // [Pre-Assert確認_異常系] - setsockopt() が kSocket、IPPROTO_IP、IP_DROP_MEMBERSHIP を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - setsockopt() から、errno に ENODEV を設定して -1 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, setsockopt(_, _, _, (SOCKET)kSocket, SOL_SOCKET, SO_REUSEADDR, _, _))
        .WillOnce(Return(SOCKET_ERROR));
    // [Pre-Assert確認_異常系] - setsockopt() が kSocket、SOL_SOCKET、SO_REUSEADDR を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - setsockopt() から SOCKET_ERROR を返す。
    EXPECT_CALL(mock_winsock_, setsockopt(_, _, _, (SOCKET)kSocket, IPPROTO_IP, IP_MULTICAST_IF, _, _))
        .WillOnce(Return(SOCKET_ERROR));
    // [Pre-Assert確認_異常系] - setsockopt() が kSocket、IPPROTO_IP、IP_MULTICAST_IF を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - setsockopt() から SOCKET_ERROR を返す。
    EXPECT_CALL(mock_winsock_, setsockopt(_, _, _, (SOCKET)kSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, _, _))
        .WillOnce(Return(SOCKET_ERROR));
    // [Pre-Assert確認_異常系] - setsockopt() が kSocket、IPPROTO_IP、IP_ADD_MEMBERSHIP を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - setsockopt() から SOCKET_ERROR を返す。
    EXPECT_CALL(mock_winsock_, setsockopt(_, _, _, (SOCKET)kSocket, IPPROTO_IP, IP_DROP_MEMBERSHIP, _, _))
        .WillOnce(Return(SOCKET_ERROR));
    // [Pre-Assert確認_異常系] - setsockopt() が kSocket、IPPROTO_IP、IP_DROP_MEMBERSHIP を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - setsockopt() から SOCKET_ERROR を返す。
    EXPECT_CALL(mock_winsock_, WSAGetLastError)
        .WillOnce(Return(WSAENOPROTOOPT))
        .WillOnce(Return(WSAENETDOWN))
        .WillOnce(Return(WSAENETDOWN))
        .WillOnce(Return(WSAENETDOWN));
    // [Pre-Assert確認_異常系] - WSAGetLastError() が 4 回呼び出されること。
    // [Pre-Assert手順] - WSAGetLastError() から 1 回目は WSAENOPROTOOPT、2 回目は WSAENETDOWN、3 回目は WSAENETDOWN、4 回目は WSAENETDOWN を返す。
#endif /* PLATFORM_ */
    // [Pre-Assert手順] - setsockopt() から、Linux では errno にそれぞれの失敗要因を設定して -1、Windows では SOCKET_ERROR を返し、WSAGetLastError() からそれぞれの失敗要因を返す。

    // Act
    int actual_ret_reuse_invalid = cplat_socket_set_reuse_address(
        CPLAT_INVALID_SOCKET, 1,
        &detail); // [手順] - 無効なソケットとアドレス再利用の有効化を指定して cplat_socket_set_reuse_address() を呼び出す。
    int actual_ret_broadcast_invalid = cplat_socket_set_broadcast(
        CPLAT_INVALID_SOCKET, 1,
        &detail); // [手順] - 無効なソケットとブロードキャストの有効化を指定して cplat_socket_set_broadcast() を呼び出す。
    int actual_ret_interface_invalid = cplat_socket_set_multicast_interface(
        CPLAT_INVALID_SOCKET, CPLAT_IPV4_ADDR_ANY,
        &detail); // [手順] - 無効なソケットとマルチキャスト インターフェイスを指定して cplat_socket_set_multicast_interface() を呼び出す。
    int actual_ret_join_invalid = cplat_socket_join_multicast_group(
        CPLAT_INVALID_SOCKET, cplat_ipv4_addr_loopback(), CPLAT_IPV4_ADDR_ANY,
        &detail); // [手順] - 無効なソケットと参加するマルチキャスト グループを指定して cplat_socket_join_multicast_group() を呼び出す。
    int actual_ret_leave_invalid = cplat_socket_leave_multicast_group(
        CPLAT_INVALID_SOCKET, cplat_ipv4_addr_loopback(), CPLAT_IPV4_ADDR_ANY,
        &detail); // [手順] - 無効なソケットと離脱するマルチキャスト グループを指定して cplat_socket_leave_multicast_group() を呼び出す。
    int actual_ret_failure = cplat_socket_set_reuse_address(
        kSocket, 1, &detail); // [手順] - オプション設定が失敗する状態で cplat_socket_set_reuse_address() を呼び出す。
    int actual_ret_interface_failure = cplat_socket_set_multicast_interface(
        kSocket, cplat_ipv4_addr_loopback(),
        &detail); // [手順] - インターフェイス設定が失敗する状態で cplat_socket_set_multicast_interface() を呼び出す。
    int actual_ret_join_failure = cplat_socket_join_multicast_group(
        kSocket, cplat_ipv4_addr_loopback(), CPLAT_IPV4_ADDR_ANY,
        &detail); // [手順] - グループ参加が失敗する状態で cplat_socket_join_multicast_group() を呼び出す。
    int actual_ret_leave_failure = cplat_socket_leave_multicast_group(
        kSocket, cplat_ipv4_addr_loopback(), CPLAT_IPV4_ADDR_ANY,
        &detail); // [手順] - グループ離脱が失敗する状態で cplat_socket_leave_multicast_group() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_reuse_invalid); // [確認_異常系] - 無効なソケットを指定した cplat_socket_set_reuse_address() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_broadcast_invalid); // [確認_異常系] - 無効なソケットを指定した cplat_socket_set_broadcast() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_interface_invalid); // [確認_異常系] - 無効なソケットを指定した cplat_socket_set_multicast_interface() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_join_invalid); // [確認_異常系] - 無効なソケットを指定した cplat_socket_join_multicast_group() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_failure); // [確認_異常系] - オプション設定失敗時の cplat_socket_set_reuse_address() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_interface_failure); // [確認_異常系] - インターフェイス設定失敗時の cplat_socket_set_multicast_interface() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_join_failure); // [確認_異常系] - グループ参加失敗時の cplat_socket_join_multicast_group() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_leave_invalid); // [確認_異常系] - 無効なソケットを指定した cplat_socket_leave_multicast_group() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_leave_failure); // [確認_異常系] - グループ離脱失敗時の cplat_socket_leave_multicast_group() の戻り値が CPLAT_ERR_UNKNOWN であること。
}
// [サブ手順参照 名前=socketTest.TearDown]

// IP_MULTICAST_TTL が UDP ソケットに指定した値を保持することの確認
TEST_F(socketTest, multicast_ttl_round_trips_on_real_udp_socket)
{
    // Arrange
#if defined(PLATFORM_WINDOWS)
    RealWinsockSession winsock_session;
    ASSERT_TRUE(winsock_session.started());
    const cplat_socket sock = (cplat_socket)::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    ASSERT_NE(INVALID_SOCKET, (SOCKET)sock);
#else
    const cplat_socket sock = (cplat_socket)::socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_NE(CPLAT_INVALID_SOCKET, sock);
#endif
    const unsigned int values[] = {0U, 1U, 32U, CPLAT_SOCKET_MULTICAST_TTL_MAX};

    // Act / Assert
    for (const unsigned int ttl : values)
    {
        cplat_error detail = {};
        const int actual_ret = cplat_socket_set_multicast_ttl(sock, ttl, &detail);
        ASSERT_EQ(CPLAT_OK, actual_ret);

#if defined(PLATFORM_WINDOWS)
        DWORD actual_ttl = 0U;
        int actual_len = (int)sizeof(actual_ttl);
        ASSERT_EQ(0, ::getsockopt((SOCKET)sock, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&actual_ttl, &actual_len));
        EXPECT_EQ((DWORD)ttl, actual_ttl);
        EXPECT_EQ((int)sizeof(actual_ttl), actual_len);
#else
        int actual_ttl = -1;
        socklen_t actual_len = (socklen_t)sizeof(actual_ttl);
        ASSERT_EQ(0, ::getsockopt((int)sock, IPPROTO_IP, IP_MULTICAST_TTL, &actual_ttl, &actual_len));
        EXPECT_EQ((int)ttl, actual_ttl);
        EXPECT_EQ((socklen_t)sizeof(actual_ttl), actual_len);
#endif
    }

#if defined(PLATFORM_WINDOWS)
    EXPECT_EQ(0, ::closesocket((SOCKET)sock));
#else
    EXPECT_EQ(0, ::close((int)sock));
#endif
// [確認_正常系] - 実 UDP ソケットへ 0、1、32、255 を設定し、getsockopt() で同じ値を取得できること。
}

// IP_MULTICAST_TTL の範囲外と無効なソケットが OS 呼び出し前に拒否されることの確認
TEST_F(socketTest, multicast_ttl_rejects_out_of_range_and_invalid_socket)
{
    // Arrange
    cplat_error detail = {};

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_sys_socket_, setsockopt(_, _, _, (int)kSocket, IPPROTO_IP, IP_MULTICAST_TTL, _, _)).Times(0);
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, setsockopt(_, _, _, (SOCKET)kSocket, IPPROTO_IP, IP_MULTICAST_TTL, _, _)).Times(0);
#endif /* PLATFORM_ */
    // [Pre-Assert確認_正常系] - 範囲外または無効なソケットでは setsockopt() が呼び出されないこと。

    // Act
    const int actual_ret_256 = cplat_socket_set_multicast_ttl(kSocket, 256U, &detail);
    const int actual_ret_max = cplat_socket_set_multicast_ttl(kSocket, UINT_MAX, &detail);
    const int actual_ret_invalid_socket = cplat_socket_set_multicast_ttl(CPLAT_INVALID_SOCKET, 0U, &detail);

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_256);
    EXPECT_EQ(CPLAT_ERROR_DOMAIN_ERRNO, detail.domain);
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, detail.result);
    EXPECT_EQ((unsigned long)EINVAL, detail.code);
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_max);
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret_invalid_socket);
// [確認_異常系] - 256、UINT_MAX、無効なソケットの結果が CPLAT_ERR_INVALID_ARGUMENT で、setsockopt() が呼ばれないこと。
}

// IP_MULTICAST_TTL の OS 失敗が結果コードと詳細エラーへ記録されることの確認
TEST_F(socketTest, multicast_ttl_reports_setsockopt_failure)
{
    // Arrange
    cplat_error detail = {};

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_sys_socket_, setsockopt(_, _, _, (int)kSocket, IPPROTO_IP, IP_MULTICAST_TTL, _, _))
        .WillOnce(DoAll(Assign(&errno, ENOPROTOOPT), Return(-1)));
    // [Pre-Assert確認_異常系] - setsockopt() が失敗し、errno に ENOPROTOOPT を設定すること。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, setsockopt(_, _, _, (SOCKET)kSocket, IPPROTO_IP, IP_MULTICAST_TTL, _, _))
        .WillOnce(Return(SOCKET_ERROR));
    EXPECT_CALL(mock_winsock_, WSAGetLastError).WillOnce(Return(WSAENOPROTOOPT));
    // [Pre-Assert確認_異常系] - setsockopt() が SOCKET_ERROR を返し、WSAGetLastError() が WSAENOPROTOOPT を返すこと。
#endif /* PLATFORM_ */

    // Act
    const int actual_ret = cplat_socket_set_multicast_ttl(kSocket, 32U, &detail);

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret);
#if defined(PLATFORM_LINUX)
    expect_detail(detail, CPLAT_ERROR_DOMAIN_SOCKET_ERRNO, CPLAT_ERR_UNKNOWN, (unsigned long)ENOPROTOOPT);
#elif defined(PLATFORM_WINDOWS)
    expect_detail(detail, CPLAT_ERROR_DOMAIN_WINSOCK, CPLAT_ERR_UNKNOWN, (unsigned long)WSAENOPROTOOPT);
#endif /* PLATFORM_ */
// [確認_異常系] - setsockopt() の失敗に対応する結果コードと詳細エラーが返ること。
}

// 解除の事前通知、リセット、待機中の通知、期限まで待つ動作の確認
TEST_F(socketTest, watch_cancel_is_persistent_resettable_and_interrupts_waiters)
{
    // Arrange
#if defined(PLATFORM_WINDOWS)
    RealWinsockSession winsock_session;
    ASSERT_TRUE(winsock_session.started());
    const cplat_socket sock = (cplat_socket)::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    ASSERT_NE(INVALID_SOCKET, (SOCKET)sock);
#else
    const cplat_socket sock = (cplat_socket)::socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_NE(CPLAT_INVALID_SOCKET, sock);
#endif
    cplat_socket_watch *watch = nullptr;
    cplat_socket_wait_cancel *cancel = nullptr;
    cplat_error detail = {};
    ASSERT_EQ(CPLAT_OK, cplat_socket_watch_attach(sock, &watch, &detail));
    ASSERT_NE(nullptr, watch);
    ASSERT_EQ(CPLAT_OK, cplat_socket_wait_cancel_create(&cancel, &detail));
    ASSERT_NE(nullptr, cancel);
    cplat_socket_watch *watches[] = {watch};
    unsigned char ready[] = {1U};

    // Act / Assert: 待機より前の通知は保持され、解除を優先して結果配列を 0 にします。
    ASSERT_EQ(CPLAT_OK, cplat_socket_wait_cancel_signal(cancel, &detail));
    EXPECT_EQ(CPLAT_ERR_CANCELED,
              cplat_socket_watch_wait_readable_multi(watches, 1U, cancel, CPLAT_WAIT_FOREVER, ready, &detail));
    EXPECT_EQ(0U, ready[0]);

    // リセット後は通知が解除され、指定した期限より早く戻りません。
    ASSERT_EQ(CPLAT_OK, cplat_socket_wait_cancel_reset(cancel, &detail));
    const auto timeout_started = std::chrono::steady_clock::now();
    EXPECT_EQ(CPLAT_ERR_TIMEOUT,
              cplat_socket_watch_wait_readable_multi(watches, 1U, cancel, 120, ready, &detail));
    const auto timeout_elapsed = std::chrono::steady_clock::now() - timeout_started;
    EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(timeout_elapsed).count(), 110);

    // 別スレッドの通知は、30 秒の待機を 1 秒以内に解除します。
    std::atomic<int> wait_result{CPLAT_ERR_UNKNOWN};
    std::atomic<bool> wait_started{false};
    std::thread waiter([&]() {
        unsigned char thread_ready[] = {1U};
        wait_started.store(true, std::memory_order_release);
        wait_result.store(cplat_socket_watch_wait_readable_multi(watches, 1U, cancel, 30000, thread_ready, nullptr),
                          std::memory_order_release);
    });
    while (!wait_started.load(std::memory_order_acquire))
    {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    const auto signal_started = std::chrono::steady_clock::now();
    const int signal_result = cplat_socket_wait_cancel_signal(cancel, &detail);
    waiter.join();
    const auto signal_elapsed = std::chrono::steady_clock::now() - signal_started;
    EXPECT_EQ(CPLAT_OK, signal_result);
    EXPECT_EQ(CPLAT_ERR_CANCELED, wait_result.load(std::memory_order_acquire));
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(signal_elapsed).count(), 1000);

    // 後片付け
    cplat_socket_wait_cancel_dispose(cancel);
    cplat_socket_watch_detach(watch);
#if defined(PLATFORM_WINDOWS)
    EXPECT_EQ(0, ::closesocket((SOCKET)sock));
#else
    EXPECT_EQ(0, ::close((int)sock));
#endif
// [確認_正常系] - 待機前の解除、解除状態の保持、リセット後の期限待ち、別スレッドからの解除を確認する。
}

// 登録前からあるデータと複数登録の選択が受信待機で検出されることの確認
TEST_F(socketTest, watch_readable_multi_detects_queued_data_for_matching_watch)
{
    // Arrange
#if defined(PLATFORM_WINDOWS)
    RealWinsockSession winsock_session;
    ASSERT_TRUE(winsock_session.started());
    const cplat_socket receiver_a = (cplat_socket)::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    const cplat_socket receiver_b = (cplat_socket)::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    const cplat_socket sender = (cplat_socket)::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    ASSERT_NE(INVALID_SOCKET, (SOCKET)receiver_a);
    ASSERT_NE(INVALID_SOCKET, (SOCKET)receiver_b);
    ASSERT_NE(INVALID_SOCKET, (SOCKET)sender);
#else
    const cplat_socket receiver_a = (cplat_socket)::socket(AF_INET, SOCK_DGRAM, 0);
    const cplat_socket receiver_b = (cplat_socket)::socket(AF_INET, SOCK_DGRAM, 0);
    const cplat_socket sender = (cplat_socket)::socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_NE(CPLAT_INVALID_SOCKET, receiver_a);
    ASSERT_NE(CPLAT_INVALID_SOCKET, receiver_b);
    ASSERT_NE(CPLAT_INVALID_SOCKET, sender);
#endif
    sockaddr_in address_a = {};
    sockaddr_in address_b = {};
    address_a.sin_family = AF_INET;
    address_a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address_a.sin_port = 0;
    address_b.sin_family = AF_INET;
    address_b.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address_b.sin_port = 0;
#if defined(PLATFORM_WINDOWS)
    ASSERT_EQ(0, ::bind((SOCKET)receiver_a, (const sockaddr *)&address_a, (int)sizeof(address_a)));
    ASSERT_EQ(0, ::bind((SOCKET)receiver_b, (const sockaddr *)&address_b, (int)sizeof(address_b)));
    int address_len = (int)sizeof(address_a);
#else
    ASSERT_EQ(0, ::bind((int)receiver_a, (const sockaddr *)&address_a, (socklen_t)sizeof(address_a)));
    ASSERT_EQ(0, ::bind((int)receiver_b, (const sockaddr *)&address_b, (socklen_t)sizeof(address_b)));
    socklen_t address_len = (socklen_t)sizeof(address_a);
#endif
#if defined(PLATFORM_WINDOWS)
    ASSERT_EQ(0, ::getsockname((SOCKET)receiver_a, (sockaddr *)&address_a, &address_len));
    address_len = (int)sizeof(address_b);
#else
    ASSERT_EQ(0, ::getsockname((int)receiver_a, (sockaddr *)&address_a, &address_len));
    address_len = (socklen_t)sizeof(address_b);
#endif
#if defined(PLATFORM_WINDOWS)
    ASSERT_EQ(0, ::getsockname((SOCKET)receiver_b, (sockaddr *)&address_b, &address_len));
#else
    ASSERT_EQ(0, ::getsockname((int)receiver_b, (sockaddr *)&address_b, &address_len));
#endif

    const char first_data = 'A';
#if defined(PLATFORM_WINDOWS)
    ASSERT_EQ(1, ::sendto((SOCKET)sender, &first_data, 1, 0, (const sockaddr *)&address_a,
                          (int)sizeof(address_a)));
#else
    ASSERT_EQ(1, ::sendto((int)sender, &first_data, 1, 0, (const sockaddr *)&address_a,
                          (socklen_t)sizeof(address_a)));
#endif
    cplat_socket_watch *watch_a = nullptr;
    cplat_socket_watch *watch_b = nullptr;
    cplat_error detail = {};
    ASSERT_EQ(CPLAT_OK, cplat_socket_watch_attach(receiver_a, &watch_a, &detail));
    ASSERT_EQ(CPLAT_OK, cplat_socket_watch_attach(receiver_b, &watch_b, &detail));
    cplat_socket_watch *watches[] = {watch_a, watch_b};
    unsigned char ready[] = {0U, 0U};

    // Act / Assert: 登録前に到着したデータは最初の待機で検出され、対応する要素だけが 1 になります。
    ASSERT_EQ(CPLAT_OK, cplat_socket_watch_wait_readable_multi(watches, 2U, nullptr, 1000, ready, &detail));
    EXPECT_EQ(1U, ready[0]);
    EXPECT_EQ(0U, ready[1]);
    char received = 0;
    size_t received_len = 0U;
    ASSERT_EQ(CPLAT_OK, cplat_socket_recv(receiver_a, &received, sizeof(received), &received_len, &detail));
    EXPECT_EQ(first_data, received);
    EXPECT_EQ(1U, received_len);
    received_len = 9U;
    const int would_block_result = cplat_socket_recv(receiver_a, &received, sizeof(received), &received_len, &detail);
    EXPECT_EQ(CPLAT_ERR_WOULD_BLOCK, would_block_result);
    EXPECT_EQ(CPLAT_CAUSE_WOULD_BLOCK, cplat_error_get_cause(&detail));
    EXPECT_EQ(0U, received_len);

    // 2 つ目の登録にだけデータを送ると、その登録だけが受信可能になります。
    const char second_data = 'B';
#if defined(PLATFORM_WINDOWS)
    ASSERT_EQ(1, ::sendto((SOCKET)sender, &second_data, 1, 0, (const sockaddr *)&address_b,
                          (int)sizeof(address_b)));
#else
    ASSERT_EQ(1, ::sendto((int)sender, &second_data, 1, 0, (const sockaddr *)&address_b,
                          (socklen_t)sizeof(address_b)));
#endif
    ready[0] = 1U;
    ready[1] = 0U;
    ASSERT_EQ(CPLAT_OK, cplat_socket_watch_wait_readable_multi(watches, 2U, nullptr, 1000, ready, &detail));
    EXPECT_EQ(0U, ready[0]);
    EXPECT_EQ(1U, ready[1]);

    cplat_socket_watch_detach(watch_b);
    cplat_socket_watch_detach(watch_a);
#if defined(PLATFORM_WINDOWS)
    EXPECT_EQ(0, ::closesocket((SOCKET)sender));
    EXPECT_EQ(0, ::closesocket((SOCKET)receiver_b));
    EXPECT_EQ(0, ::closesocket((SOCKET)receiver_a));
#else
    EXPECT_EQ(0, ::close((int)sender));
    EXPECT_EQ(0, ::close((int)receiver_b));
    EXPECT_EQ(0, ::close((int)receiver_a));
#endif
// [確認_正常系] - 登録前のデータと登録後のデータを、それぞれ該当する登録だけが受信可能として通知されること。
}

// 全量受信と全量送信が途中解除を返し、処理済みバイト数を保つことの確認
TEST_F(socketTest, watch_transfer_all_preserves_partial_counts_on_cancel)
{
    // Arrange
#if defined(PLATFORM_WINDOWS)
    RealWinsockSession winsock_session;
    ASSERT_TRUE(winsock_session.started());
    const cplat_socket listener = (cplat_socket)::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    const cplat_socket client = (cplat_socket)::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ASSERT_NE(INVALID_SOCKET, (SOCKET)listener);
    ASSERT_NE(INVALID_SOCKET, (SOCKET)client);
#else
    const cplat_socket listener = (cplat_socket)::socket(AF_INET, SOCK_STREAM, 0);
    const cplat_socket client = (cplat_socket)::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(CPLAT_INVALID_SOCKET, listener);
    ASSERT_NE(CPLAT_INVALID_SOCKET, client);
#endif
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
#if defined(PLATFORM_WINDOWS)
    ASSERT_EQ(0, ::bind((SOCKET)listener, (const sockaddr *)&address, (int)sizeof(address)));
    ASSERT_EQ(0, ::listen((SOCKET)listener, 1));
    int address_len = (int)sizeof(address);
    ASSERT_EQ(0, ::getsockname((SOCKET)listener, (sockaddr *)&address, &address_len));
    ASSERT_EQ(0, ::connect((SOCKET)client, (const sockaddr *)&address, (int)sizeof(address)));
    const cplat_socket accepted = (cplat_socket)::accept((SOCKET)listener, nullptr, nullptr);
    ASSERT_NE(INVALID_SOCKET, (SOCKET)accepted);
#else
    ASSERT_EQ(0, ::bind((int)listener, (const sockaddr *)&address, (socklen_t)sizeof(address)));
    ASSERT_EQ(0, ::listen((int)listener, 1));
    socklen_t address_len = (socklen_t)sizeof(address);
    ASSERT_EQ(0, ::getsockname((int)listener, (sockaddr *)&address, &address_len));
    ASSERT_EQ(0, ::connect((int)client, (const sockaddr *)&address, (socklen_t)sizeof(address)));
    const cplat_socket accepted = (cplat_socket)::accept((int)listener, nullptr, nullptr);
    ASSERT_NE(CPLAT_INVALID_SOCKET, accepted);
#endif

    cplat_socket_watch *client_watch = nullptr;
    cplat_socket_watch *accepted_watch = nullptr;
    cplat_socket_wait_cancel *cancel = nullptr;
    cplat_error detail = {};
    ASSERT_EQ(CPLAT_OK, cplat_socket_watch_attach(client, &client_watch, &detail));
    ASSERT_EQ(CPLAT_OK, cplat_socket_watch_attach(accepted, &accepted_watch, &detail));
    ASSERT_EQ(CPLAT_OK, cplat_socket_wait_cancel_create(&cancel, &detail));

    const char prefix[] = "ab";
    size_t sent_prefix = 0U;
    ASSERT_EQ(CPLAT_OK, cplat_socket_watch_send_all(client_watch, prefix, sizeof(prefix) - 1U, nullptr, 1000,
                                                   &sent_prefix, &detail));
    ASSERT_EQ(sizeof(prefix) - 1U, sent_prefix);
    char received_frame[4] = {};
    std::atomic<bool> receive_started{false};
    std::atomic<int> receive_result{CPLAT_ERR_UNKNOWN};
    std::atomic<size_t> received_count{0U};
    cplat_error receive_detail = {};
    std::thread receiver([&]() {
        size_t count = 0U;
        receive_started.store(true, std::memory_order_release);
        const int result = cplat_socket_watch_recv_all(accepted_watch, received_frame, sizeof(received_frame), cancel,
                                                       30000, &count, &receive_detail);
        received_count.store(count, std::memory_order_release);
        receive_result.store(result, std::memory_order_release);
    });
    while (!receive_started.load(std::memory_order_acquire))
    {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const auto receive_signal_started = std::chrono::steady_clock::now();
    EXPECT_EQ(CPLAT_OK, cplat_socket_wait_cancel_signal(cancel, &detail));
    receiver.join();
    const auto receive_signal_elapsed = std::chrono::steady_clock::now() - receive_signal_started;

    // Assert: フレーム先頭を受けた後の解除で部分長を返します。
    EXPECT_EQ(CPLAT_ERR_CANCELED, receive_result.load(std::memory_order_acquire));
    EXPECT_EQ(2U, received_count.load(std::memory_order_acquire));
    EXPECT_EQ('a', received_frame[0]);
    EXPECT_EQ('b', received_frame[1]);
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(receive_signal_elapsed).count(), 1000);
#if defined(PLATFORM_LINUX)
    expect_detail(receive_detail, CPLAT_ERROR_DOMAIN_SOCKET_ERRNO, CPLAT_ERR_CANCELED, (unsigned long)ECANCELED);
#else
    expect_detail(receive_detail, CPLAT_ERROR_DOMAIN_WINSOCK, CPLAT_ERR_CANCELED, (unsigned long)WSA_E_CANCELLED);
#endif

    ASSERT_EQ(CPLAT_OK, cplat_socket_wait_cancel_reset(cancel, &detail));
    const char suffix[] = "cd";
    size_t sent_suffix = 0U;
    ASSERT_EQ(CPLAT_OK, cplat_socket_watch_send_all(client_watch, suffix, sizeof(suffix) - 1U, nullptr, 1000,
                                                   &sent_suffix, &detail));
    EXPECT_EQ(sizeof(suffix) - 1U, sent_suffix);
    size_t resumed_count = 0U;
    ASSERT_EQ(CPLAT_OK, cplat_socket_watch_recv_all(accepted_watch, received_frame + 2, 2U, nullptr, 1000,
                                                   &resumed_count, &detail));
    EXPECT_EQ(2U, resumed_count);
    EXPECT_EQ('c', received_frame[2]);
    EXPECT_EQ('d', received_frame[3]);

    // Arrange / Act: 送信を一部だけ成功させて待機へ進め、解除までの送信済み量を記録します。
    const char send_payload[] = "abcdef";
    cplat_socket_watch *send_watch = client_watch;
#if defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, WSAEventSelect(_, _, _, (SOCKET)kSocket, _, _)).WillOnce(Return(0));
    ASSERT_EQ(CPLAT_OK, cplat_socket_watch_attach(kSocket, &send_watch, &detail));
    const cplat_socket send_sock = kSocket;
#else
    const cplat_socket send_sock = client;
#endif
    std::atomic<int> send_calls{0};
#if defined(PLATFORM_LINUX)
    std::atomic<bool> poll_wait_started{false};
#endif
    std::atomic<bool> cancel_requested{false};
    const size_t expected_partial_count = 2U;
#if defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, send(_, _, _, (SOCKET)send_sock, _, (int)(sizeof(send_payload) - 1U), 0))
        .WillOnce([&](const char *, const int, const char *, SOCKET, const char *, int, int) {
            send_calls.fetch_add(1, std::memory_order_release);
            return 2;
        });
    EXPECT_CALL(mock_winsock_, send(_, _, _, (SOCKET)send_sock, _, (int)(sizeof(send_payload) - 3U), 0))
        .WillOnce([&](const char *, const int, const char *, SOCKET, const char *, int, int) {
            send_calls.fetch_add(1, std::memory_order_release);
            return SOCKET_ERROR;
        });
    EXPECT_CALL(mock_winsock_, WSAGetLastError(_, _, _)).WillOnce(Return(WSAEWOULDBLOCK));
#else
    EXPECT_CALL(mock_sys_socket_, send(_, _, _, (int)send_sock, _, sizeof(send_payload) - 1U, MSG_NOSIGNAL))
        .WillOnce([&](const char *, const int, const char *, int, const void *, size_t, int) {
            send_calls.fetch_add(1, std::memory_order_release);
            return (ssize_t)2;
        });
    EXPECT_CALL(mock_sys_socket_, send(_, _, _, (int)send_sock, _, sizeof(send_payload) - 3U, MSG_NOSIGNAL))
        .WillOnce([&](const char *, const int, const char *, int, const void *, size_t, int) {
            errno = EAGAIN;
            send_calls.fetch_add(1, std::memory_order_release);
            return (ssize_t)-1;
        });
    EXPECT_CALL(mock_poll_, poll(_, _, _, _, 2, 50))
        .WillOnce([&](const char *, const int, const char *, struct pollfd *fds, nfds_t count, int) {
            EXPECT_EQ(2U, count);
            EXPECT_EQ(POLLIN, fds[0].events);
            EXPECT_EQ(POLLOUT, fds[1].events);
            poll_wait_started.store(true, std::memory_order_release);
            while (!cancel_requested.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }
            fds[0].revents = POLLIN;
            return 1;
        });
#endif
    std::atomic<bool> send_started{false};
    std::atomic<int> send_result{CPLAT_ERR_UNKNOWN};
    std::atomic<size_t> sent_count{0U};
    cplat_error send_detail = {};
    std::thread sender([&]() {
        size_t count = 0U;
        send_started.store(true, std::memory_order_release);
        const int result = cplat_socket_watch_send_all(send_watch, send_payload, sizeof(send_payload) - 1U, cancel,
                                                       CPLAT_WAIT_FOREVER, &count, &send_detail);
        sent_count.store(count, std::memory_order_release);
        send_result.store(result, std::memory_order_release);
    });
    while (!send_started.load(std::memory_order_acquire) || send_calls.load(std::memory_order_acquire) < 2)
    {
        std::this_thread::yield();
    }
#if defined(PLATFORM_LINUX)
    while (!poll_wait_started.load(std::memory_order_acquire))
    {
        std::this_thread::yield();
    }
#endif
    const auto send_signal_started = std::chrono::steady_clock::now();
    EXPECT_EQ(CPLAT_OK, cplat_socket_wait_cancel_signal(cancel, &detail));
    cancel_requested.store(true, std::memory_order_release);
    sender.join();
    const auto send_signal_elapsed = std::chrono::steady_clock::now() - send_signal_started;

    // Assert
    EXPECT_EQ(CPLAT_ERR_CANCELED, send_result.load(std::memory_order_acquire));
    EXPECT_EQ(expected_partial_count, sent_count.load(std::memory_order_acquire));
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(send_signal_elapsed).count(), 1000);
#if defined(PLATFORM_LINUX)
    expect_detail(send_detail, CPLAT_ERROR_DOMAIN_SOCKET_ERRNO, CPLAT_ERR_CANCELED, (unsigned long)ECANCELED);
    ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mock_sys_socket_));
    ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mock_poll_));
#else
    expect_detail(send_detail, CPLAT_ERROR_DOMAIN_WINSOCK, CPLAT_ERR_CANCELED, (unsigned long)WSA_E_CANCELLED);
    ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mock_winsock_));
    EXPECT_CALL(mock_winsock_, WSAEventSelect(_, _, _, (SOCKET)kSocket, (WSAEVENT)nullptr, 0)).WillOnce(Return(0));
    cplat_socket_watch_detach(send_watch);
    ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mock_winsock_));
#endif

    cplat_socket_wait_cancel_dispose(cancel);
    cplat_socket_watch_detach(accepted_watch);
    cplat_socket_watch_detach(client_watch);
#if defined(PLATFORM_WINDOWS)
    EXPECT_EQ(0, ::closesocket((SOCKET)accepted));
    EXPECT_EQ(0, ::closesocket((SOCKET)client));
    EXPECT_EQ(0, ::closesocket((SOCKET)listener));
#else
    EXPECT_EQ(0, ::close((int)accepted));
    EXPECT_EQ(0, ::close((int)client));
    EXPECT_EQ(0, ::close((int)listener));
#endif
// [確認_正常系] - 解除された全量受信と送信が、受けた量と送った量を保ち、受信は続きから再開できること。
}

#if defined(PLATFORM_LINUX)
// eventfd の生成失敗が出力を無効のままにして返ることの確認
TEST_F(socketTest, watch_cancel_reports_eventfd_creation_failure)
{
    // Arrange
    cplat_socket_wait_cancel *cancel = (cplat_socket_wait_cancel *)1;
    cplat_error detail = {};

    // Pre-Assert
    EXPECT_CALL(mock_eventfd_, eventfd(_, _, _, 0U, EFD_NONBLOCK | EFD_CLOEXEC))
        .WillOnce(DoAll(Assign(&errno, EMFILE), Return(-1)));
    // [Pre-Assert確認_異常系] - eventfd() が指定された flags で呼び出され、EMFILE を設定して失敗すること。

    // Act
    const int actual_ret = cplat_socket_wait_cancel_create(&cancel, &detail);

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret);
    EXPECT_EQ(nullptr, cancel);
    expect_detail(detail, CPLAT_ERROR_DOMAIN_SOCKET_ERRNO, CPLAT_ERR_UNKNOWN, (unsigned long)EMFILE);
// [確認_異常系] - eventfd() の生成失敗で cancel_out が NULL、結果が CPLAT_ERR_UNKNOWN になること。
}

// eventfd への通知失敗後も原子的な通知状態で待機を解除することの確認
TEST_F(socketTest, watch_cancel_signal_failure_is_detected_by_finite_poll)
{
    // Arrange
    const cplat_socket sock = (cplat_socket)::socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_NE(CPLAT_INVALID_SOCKET, sock);
    cplat_socket_watch *watch = nullptr;
    cplat_socket_wait_cancel *cancel = nullptr;
    cplat_error detail = {};
    ASSERT_EQ(CPLAT_OK, cplat_socket_watch_attach(sock, &watch, &detail));
    ASSERT_EQ(CPLAT_OK, cplat_socket_wait_cancel_create(&cancel, &detail));
    cplat_socket_watch *watches[] = {watch};
    std::atomic<int> wait_result{CPLAT_ERR_UNKNOWN};
    std::atomic<bool> wait_started{false};

    // Pre-Assert
    EXPECT_CALL(mock_unistd_, write(_, _, _, _, _, sizeof(uint64_t)))
        .WillOnce(DoAll(Assign(&errno, EAGAIN), Return((ssize_t)-1)))
        .WillOnce(DoAll(Assign(&errno, EINTR), Return((ssize_t)-1)))
        .WillOnce(DoAll(Assign(&errno, EIO), Return((ssize_t)-1)));
    // [Pre-Assert確認_異常系] - eventfd への write() が EAGAIN、EINTR、EIO で失敗すること。

    // Act
    std::thread waiter([&]() {
        unsigned char ready[] = {1U};
        wait_started.store(true, std::memory_order_release);
        wait_result.store(cplat_socket_watch_wait_readable_multi(watches, 1U, cancel, CPLAT_WAIT_FOREVER, ready,
                                                                 nullptr),
                          std::memory_order_release);
    });
    while (!wait_started.load(std::memory_order_acquire))
    {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    const int signal_result_eagain = cplat_socket_wait_cancel_signal(cancel, &detail);
    const int signal_result_eintr = cplat_socket_wait_cancel_signal(cancel, &detail);
    const int signal_result_eio = cplat_socket_wait_cancel_signal(cancel, &detail);
    waiter.join();

    // Assert
    EXPECT_EQ(CPLAT_OK, signal_result_eagain);
    EXPECT_EQ(CPLAT_OK, signal_result_eintr);
    EXPECT_EQ(CPLAT_OK, signal_result_eio);
    EXPECT_EQ(CPLAT_ERR_CANCELED, wait_result.load(std::memory_order_acquire));
    EXPECT_EQ(CPLAT_OK, cplat_socket_wait_cancel_reset(cancel, &detail));
    cplat_socket_wait_cancel_dispose(cancel);
    cplat_socket_watch_detach(watch);
    EXPECT_EQ(0, ::close((int)sock));
// [確認_正常系] - write() の失敗時も、50 ms の状態確認で無期限待機が解除されること。
}

// fcntl による既存モードの取得または非ブロッキング化が失敗した場合に登録を返さないことの確認
TEST_F(socketTest, watch_attach_reports_fcntl_failures)
{
    // Arrange
    cplat_socket_watch *watch = (cplat_socket_watch *)1;
    cplat_error detail = {};

    // Pre-Assert: F_GETFL の失敗
    EXPECT_CALL(mock_fcntl_, fcntl(_, _, _, (int)kSocket, F_GETFL, 0))
        .WillOnce(DoAll(Assign(&errno, EIO), Return(-1)));
    // [Pre-Assert確認_異常系] - F_GETFL が EIO を設定して失敗すること。
    EXPECT_CALL(mock_fcntl_, fcntl(_, _, _, (int)kSocket, F_SETFL, _)).Times(0);
    // [Pre-Assert確認_異常系] - F_GETFL の失敗後に F_SETFL を呼び出さないこと。

    // Act
    const int get_flags_result = cplat_socket_watch_attach(kSocket, &watch, &detail);

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, get_flags_result);
    EXPECT_EQ(nullptr, watch);
    expect_detail(detail, CPLAT_ERROR_DOMAIN_SOCKET_ERRNO, CPLAT_ERR_UNKNOWN, (unsigned long)EIO);

    ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mock_fcntl_));
    watch = (cplat_socket_watch *)1;
    detail = {};
    EXPECT_CALL(mock_fcntl_, fcntl(_, _, _, (int)kSocket, F_GETFL, 0)).WillOnce(Return(O_RDONLY));
    EXPECT_CALL(mock_fcntl_, fcntl(_, _, _, (int)kSocket, F_SETFL, O_RDONLY | O_NONBLOCK))
        .WillOnce(DoAll(Assign(&errno, EACCES), Return(-1)));

    const int set_flags_result = cplat_socket_watch_attach(kSocket, &watch, &detail);
    EXPECT_EQ(CPLAT_ERR_PERMISSION_DENIED, set_flags_result);
    EXPECT_EQ(nullptr, watch);
    expect_detail(detail, CPLAT_ERROR_DOMAIN_SOCKET_ERRNO, CPLAT_ERR_PERMISSION_DENIED, (unsigned long)EACCES);
// [確認_異常系] - F_GETFL または F_SETFL の失敗で登録を返さず、OS エラーを保持すること。
}
#endif /* PLATFORM_LINUX */

#if defined(PLATFORM_WINDOWS)
// 待機の解除オブジェクトのイベント生成失敗が返ることの確認
TEST_F(socketTest, watch_cancel_reports_event_creation_failure)
{
    // Arrange
    RealWinsockSession winsock_session;
    ASSERT_TRUE(winsock_session.started());
    cplat_socket_wait_cancel *cancel = (cplat_socket_wait_cancel *)1;
    cplat_error detail = {};

    // Pre-Assert
    EXPECT_CALL(mock_winsock_, WSACreateEvent(_, _, _)).WillOnce(Return(WSA_INVALID_EVENT));
    EXPECT_CALL(mock_winsock_, WSAGetLastError).WillOnce(Return(WSAENOBUFS));
    // [Pre-Assert確認_異常系] - WSACreateEvent() が失敗し、WSAGetLastError() が WSAENOBUFS を返すこと。

    // Act
    const int actual_ret = cplat_socket_wait_cancel_create(&cancel, &detail);

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY, actual_ret);
    EXPECT_EQ(nullptr, cancel);
    expect_detail(detail, CPLAT_ERROR_DOMAIN_WINSOCK, CPLAT_ERR_OUT_OF_MEMORY, (unsigned long)WSAENOBUFS);
// [確認_異常系] - イベント生成失敗で cancel_out が NULL、結果と詳細に WSAENOBUFS が記録されること。
}

// WSASetEvent の失敗後も原子的な通知状態で待機を解除することの確認
TEST_F(socketTest, watch_cancel_signal_failure_is_detected_by_finite_wait)
{
    // Arrange
    RealWinsockSession winsock_session;
    ASSERT_TRUE(winsock_session.started());
    const cplat_socket sock = (cplat_socket)::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    ASSERT_NE(INVALID_SOCKET, (SOCKET)sock);
    cplat_socket_watch *watch = nullptr;
    cplat_socket_wait_cancel *cancel = nullptr;
    cplat_error detail = {};
    ASSERT_EQ(CPLAT_OK, cplat_socket_watch_attach(sock, &watch, &detail));
    ASSERT_EQ(CPLAT_OK, cplat_socket_wait_cancel_create(&cancel, &detail));
    cplat_socket_watch *watches[] = {watch};
    std::atomic<int> wait_result{CPLAT_ERR_UNKNOWN};
    std::atomic<bool> wait_started{false};

    // Pre-Assert
    EXPECT_CALL(mock_winsock_, WSASetEvent(_, _, _, _)).WillOnce(Return(FALSE));
    EXPECT_CALL(mock_winsock_, WSAGetLastError).WillOnce(Return(WSAEINVAL));
    // [Pre-Assert確認_異常系] - WSASetEvent() が失敗し、WSAGetLastError() が WSAEINVAL を返すこと。

    // Act
    std::thread waiter([&]() {
        unsigned char ready[] = {1U};
        wait_started.store(true, std::memory_order_release);
        wait_result.store(cplat_socket_watch_wait_readable_multi(watches, 1U, cancel, CPLAT_WAIT_FOREVER, ready,
                                                                 nullptr),
                          std::memory_order_release);
    });
    while (!wait_started.load(std::memory_order_acquire))
    {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    const int signal_result = cplat_socket_wait_cancel_signal(cancel, &detail);
    waiter.join();

    // Assert
    EXPECT_EQ(CPLAT_OK, signal_result);
    EXPECT_EQ(CPLAT_ERR_CANCELED, wait_result.load(std::memory_order_acquire));
    EXPECT_EQ(CPLAT_OK, cplat_socket_wait_cancel_reset(cancel, &detail));
    cplat_socket_wait_cancel_dispose(cancel);
    cplat_socket_watch_detach(watch);
    EXPECT_EQ(0, ::closesocket((SOCKET)sock));
// [確認_正常系] - WSASetEvent() の失敗時も有限の状態確認で無期限待機が解除されること。
}

// watch 登録の各イベント生成段階で失敗すると準備済みのイベントを回収することの確認
TEST_F(socketTest, watch_attach_cleans_up_each_event_creation_failure)
{
    // Arrange
    RealWinsockSession winsock_session;
    ASSERT_TRUE(winsock_session.started());
    const cplat_socket sock = (cplat_socket)::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    ASSERT_NE(INVALID_SOCKET, (SOCKET)sock);

    // Act / Assert
    for (int failed_stage = 1; failed_stage <= 3; ++failed_stage)
    {
        ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mock_winsock_));
        cplat_socket_watch *watch = (cplat_socket_watch *)1;
        cplat_error detail = {};
        int create_calls = 0;
        EXPECT_CALL(mock_winsock_, WSACreateEvent(_, _, _))
            .Times(failed_stage)
            .WillRepeatedly([&](const char *file, const int line, const char *func) {
                ++create_calls;
                if (create_calls == failed_stage)
                {
                    return (WSAEVENT)WSA_INVALID_EVENT;
                }
                return delegate_real_WSACreateEvent(file, line, func);
            });
        EXPECT_CALL(mock_winsock_, WSAGetLastError).WillOnce(Return(WSAENOBUFS));
        if (failed_stage > 1)
        {
            EXPECT_CALL(mock_winsock_, WSACloseEvent(_, _, _, _))
                .Times(failed_stage - 1)
                .WillRepeatedly([](const char *file, const int line, const char *func, WSAEVENT event) {
                    return delegate_real_WSACloseEvent(file, line, func, event);
                });
        }
        EXPECT_CALL(mock_winsock_, WSAEventSelect(_, _, _, (SOCKET)sock, _, _)).Times(0);

        const int actual_ret = cplat_socket_watch_attach(sock, &watch, &detail);
        EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY, actual_ret);
        EXPECT_EQ(nullptr, watch);
        EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY, detail.result);
        EXPECT_EQ((unsigned long)WSAENOBUFS, detail.code);
        EXPECT_EQ(failed_stage, create_calls);
    }

    ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mock_winsock_));
    cplat_socket_watch *watch = (cplat_socket_watch *)1;
    cplat_error detail = {};
    EXPECT_CALL(mock_winsock_, WSACreateEvent(_, _, _))
        .Times(3)
        .WillRepeatedly([](const char *file, const int line, const char *func) {
            return delegate_real_WSACreateEvent(file, line, func);
        });
    EXPECT_CALL(mock_winsock_, WSAEventSelect(_, _, _, (SOCKET)sock, _,
                                               FD_READ | FD_ACCEPT | FD_CLOSE | FD_WRITE | FD_CONNECT))
        .WillOnce(Return(SOCKET_ERROR));
    EXPECT_CALL(mock_winsock_, WSAGetLastError).WillOnce(Return(WSAEINVAL));
    EXPECT_CALL(mock_winsock_, WSACloseEvent(_, _, _, _))
        .Times(3)
        .WillRepeatedly([](const char *file, const int line, const char *func, WSAEVENT event) {
            return delegate_real_WSACloseEvent(file, line, func, event);
        });
    const int registration_result = cplat_socket_watch_attach(sock, &watch, &detail);
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, registration_result);
    EXPECT_EQ(nullptr, watch);
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, detail.result);
    EXPECT_EQ((unsigned long)WSAEINVAL, detail.code);

    EXPECT_EQ(0, ::closesocket((SOCKET)sock));
// [確認_異常系] - E_s、E_r、E_w の各生成失敗で未公開イベントをすべて解放し、ソケット登録を行わないこと。
}

// 待機 API の失敗と WSAEnumNetworkEvents() の失敗が両方向へ保持されることの確認
TEST_F(socketTest, watch_wait_propagates_wait_and_enumeration_failures)
{
    // Arrange
    RealWinsockSession winsock_session;
    ASSERT_TRUE(winsock_session.started());
    cplat_socket_watch *watch = nullptr;
    cplat_error detail = {};
    EXPECT_CALL(mock_winsock_, WSAEventSelect(_, _, _, (SOCKET)kSocket, _, _)).WillOnce(Return(0));
    ASSERT_EQ(CPLAT_OK, cplat_socket_watch_attach(kSocket, &watch, &detail));
    cplat_socket_watch *watches[] = {watch};
    unsigned char ready[] = {1U};

    // Pre-Assert: WSAWaitForMultipleEvents() の失敗
    EXPECT_CALL(mock_winsock_, WSAWaitForMultipleEvents(_, _, _, 1U, _, FALSE, 0U, FALSE))
        .WillOnce(Return(WSA_WAIT_FAILED));
    EXPECT_CALL(mock_winsock_, WSAGetLastError(_, _, _)).WillOnce(Return(WSAENETDOWN));

    // Act
    const int wait_failure = cplat_socket_watch_wait_readable_multi(watches, 1U, nullptr, 1000, ready, &detail);

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, wait_failure);
    expect_detail(detail, CPLAT_ERROR_DOMAIN_WINSOCK, CPLAT_ERR_UNKNOWN, (unsigned long)WSAENETDOWN);

    ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mock_winsock_));
    detail = {};

    // Pre-Assert: 列挙失敗を保存し、後続の送信待機では再列挙しない。
    EXPECT_CALL(mock_winsock_, WSAWaitForMultipleEvents(_, _, _, 1U, _, FALSE, 0U, FALSE))
        .Times(2)
        .WillOnce(Return(WSA_WAIT_EVENT_0))
        .WillOnce(Return(WSA_WAIT_TIMEOUT));
    EXPECT_CALL(mock_winsock_, WSAEnumNetworkEvents(_, _, _, (SOCKET)kSocket, _, _)).WillOnce(Return(SOCKET_ERROR));
    EXPECT_CALL(mock_winsock_, WSAGetLastError(_, _, _)).WillOnce(Return(WSAENETDOWN));
    EXPECT_CALL(mock_winsock_, WSASetEvent(_, _, _, _)).Times(2).WillRepeatedly(Return(TRUE));

    // Act / Assert: 受信側に保存した列挙エラーを返します。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              cplat_socket_watch_wait_readable_multi(watches, 1U, nullptr, 1000, ready, &detail));
    expect_detail(detail, CPLAT_ERROR_DOMAIN_WINSOCK, CPLAT_ERR_UNKNOWN, (unsigned long)WSAENETDOWN);

    // Act / Assert: 送信側も同じエラーを受け、列挙を繰り返しません。
    detail = {};
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, cplat_socket_watch_wait_writable(watch, nullptr, 1000, &detail));
    expect_detail(detail, CPLAT_ERROR_DOMAIN_WINSOCK, CPLAT_ERR_UNKNOWN, (unsigned long)WSAENETDOWN);
    ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mock_winsock_));

    EXPECT_CALL(mock_winsock_, WSAEventSelect(_, _, _, (SOCKET)kSocket, (WSAEVENT)nullptr, 0)).WillOnce(Return(0));
    cplat_socket_watch_detach(watch);
    ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mock_winsock_));
// [確認_異常系] - 待機失敗を返し、列挙失敗を一度だけ保存して受信と送信の両方へ伝えること。
}

// FD_WRITE だけの通知で受信待機が起床しないことの確認
TEST_F(socketTest, watch_read_wait_ignores_write_only_notification)
{
    // Arrange
    RealWinsockSession winsock_session;
    ASSERT_TRUE(winsock_session.started());
    const WSAEVENT shared_event = reinterpret_cast<WSAEVENT>(static_cast<uintptr_t>(0x101U));
    const WSAEVENT read_event = reinterpret_cast<WSAEVENT>(static_cast<uintptr_t>(0x102U));
    const WSAEVENT write_event = reinterpret_cast<WSAEVENT>(static_cast<uintptr_t>(0x103U));
    int zero_timeout_calls = 0;
    cplat_socket_watch *watch = nullptr;
    cplat_error detail = {};
    EXPECT_CALL(mock_winsock_, WSACreateEvent(_, _, _))
        .Times(3)
        .WillOnce(Return(shared_event))
        .WillOnce(Return(read_event))
        .WillOnce(Return(write_event));
    EXPECT_CALL(mock_winsock_, WSAEventSelect(_, _, _, (SOCKET)kSocket, shared_event, _)).WillOnce(Return(0));
    ASSERT_EQ(CPLAT_OK, cplat_socket_watch_attach(kSocket, &watch, &detail));

    // Pre-Assert
    EXPECT_CALL(mock_winsock_, WSAWaitForMultipleEvents(_, _, _, _, _, FALSE, _, FALSE))
        .Times(AnyNumber())
        .WillRepeatedly([&](const char *, const int, const char *, const DWORD count, const WSAEVENT *events,
                            const BOOL wait_all, const DWORD timeout_ms, const BOOL alertable) -> DWORD {
            EXPECT_EQ(FALSE, wait_all);
            EXPECT_EQ(FALSE, alertable);
            if (count == 1U)
            {
                EXPECT_EQ(shared_event, events[0]);
                EXPECT_EQ(0U, timeout_ms);
                ++zero_timeout_calls;
                if (zero_timeout_calls == 1)
                {
                    return WSA_WAIT_EVENT_0;
                }
                return WSA_WAIT_TIMEOUT;
            }
            EXPECT_EQ(2U, count);
            EXPECT_GT(timeout_ms, 0U);
            EXPECT_EQ(read_event, events[0]);
            EXPECT_EQ(shared_event, events[1]);
            EXPECT_NE(write_event, events[0]);
            EXPECT_NE(write_event, events[1]);
            std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
            return WSA_WAIT_TIMEOUT;
        });
    EXPECT_CALL(mock_winsock_, WSAEnumNetworkEvents(_, _, _, (SOCKET)kSocket, shared_event, _))
        .WillOnce([](const char *, const int, const char *, SOCKET, WSAEVENT, LPWSANETWORKEVENTS events) {
            events->lNetworkEvents = FD_WRITE;
            return 0;
        });
    EXPECT_CALL(mock_winsock_, WSASetEvent(_, _, _, write_event)).WillOnce(Return(TRUE));
    EXPECT_CALL(mock_winsock_, WSAResetEvent(_, _, _, read_event))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(Return(TRUE));

    // Act
    cplat_socket_watch *watches[] = {watch};
    unsigned char ready[] = {1U};
    const auto started = std::chrono::steady_clock::now();
    const int actual_ret = cplat_socket_watch_wait_readable_multi(watches, 1U, nullptr, 80, ready, &detail);
    const auto elapsed = std::chrono::steady_clock::now() - started;

    // Assert
    EXPECT_EQ(CPLAT_ERR_TIMEOUT, actual_ret);
    EXPECT_EQ(0U, ready[0]);
    EXPECT_GT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 0);
    ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mock_winsock_));

    EXPECT_CALL(mock_winsock_, WSAEventSelect(_, _, _, (SOCKET)kSocket, (WSAEVENT)nullptr, 0)).WillOnce(Return(0));
    EXPECT_CALL(mock_winsock_, WSACloseEvent(_, _, _, _)).Times(3).WillRepeatedly(Return(TRUE));
    cplat_socket_watch_detach(watch);
    ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mock_winsock_));
// [確認_正常系] - FD_WRITE だけが通知された場合に受信の待機イベントを待ち続け、期限で戻ること。
}

// 最大登録数の受信待機と方向別待機が上限内のイベント数を使うことの確認
TEST_F(socketTest, watch_wait_uses_bounded_event_arrays)
{
    // Arrange
    RealWinsockSession winsock_session;
    ASSERT_TRUE(winsock_session.started());
    cplat_socket_watch *watches[CPLAT_SOCKET_WATCH_WAIT_MAX] = {};
    unsigned char ready[CPLAT_SOCKET_WATCH_WAIT_MAX] = {};
    cplat_socket_wait_cancel *cancel = nullptr;
    cplat_error detail = {};
    uintptr_t next_event_id = 0x1000U;
    int cancel_read_count = 0;
    int no_cancel_read_count = 0;
    int writable_cancel_count = 0;

    EXPECT_CALL(mock_winsock_, WSACreateEvent(_, _, _))
        .Times(3 * CPLAT_SOCKET_WATCH_WAIT_MAX + 1)
        .WillRepeatedly([&](const char *, const int, const char *) {
            return reinterpret_cast<WSAEVENT>(next_event_id++);
        });
    EXPECT_CALL(mock_winsock_, WSAEventSelect(_, _, _, _, _, _))
        .Times(2 * CPLAT_SOCKET_WATCH_WAIT_MAX)
        .WillRepeatedly(Return(0));
    EXPECT_CALL(mock_winsock_, WSAResetEvent(_, _, _, _)).Times(AnyNumber()).WillRepeatedly(Return(TRUE));
    EXPECT_CALL(mock_winsock_, WSACloseEvent(_, _, _, _))
        .Times(3 * CPLAT_SOCKET_WATCH_WAIT_MAX + 1)
        .WillRepeatedly(Return(TRUE));
    EXPECT_CALL(mock_winsock_, WSAWaitForMultipleEvents(_, _, _, _, _, FALSE, _, FALSE))
        .Times(AnyNumber())
        .WillRepeatedly([&](const char *, const int, const char *, const DWORD count, const WSAEVENT *, const BOOL,
                            const DWORD timeout_ms, const BOOL) -> DWORD {
            if (count == 1U)
            {
                return WSA_WAIT_TIMEOUT;
            }
            if (count == 2U * CPLAT_SOCKET_WATCH_WAIT_MAX + 1U)
            {
                ++cancel_read_count;
            }
            else if (count == 2U * CPLAT_SOCKET_WATCH_WAIT_MAX)
            {
                ++no_cancel_read_count;
            }
            else if (count == 3U)
            {
                ++writable_cancel_count;
            }
            else
            {
                ADD_FAILURE() << "unexpected WSAWaitForMultipleEvents count=" << count;
                return WSA_WAIT_FAILED;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
            return WSA_WAIT_TIMEOUT;
        });
    EXPECT_CALL(mock_winsock_, send(_, _, _, (SOCKET)0x400U, _, 1, 0)).WillOnce(Return(SOCKET_ERROR));
    EXPECT_CALL(mock_winsock_, WSAGetLastError(_, _, _)).WillOnce(Return(WSAEWOULDBLOCK));
    const char send_buffer[] = "x";

    for (size_t index = 0U; index < CPLAT_SOCKET_WATCH_WAIT_MAX; ++index)
    {
        const cplat_socket sock = (cplat_socket)(0x400U + index);
        ASSERT_EQ(CPLAT_OK, cplat_socket_watch_attach(sock, &watches[index], &detail));
    }
    ASSERT_EQ(CPLAT_OK, cplat_socket_wait_cancel_create(&cancel, &detail));
    size_t sent = 0U;
    ASSERT_EQ(CPLAT_ERR_WOULD_BLOCK, cplat_socket_watch_send(watches[0], send_buffer, 1U, &sent, &detail));
    EXPECT_EQ(0U, sent);

    // Act
    const int read_with_cancel = cplat_socket_watch_wait_readable_multi(
        watches, CPLAT_SOCKET_WATCH_WAIT_MAX, cancel, 60, ready, &detail);
    const int read_without_cancel = cplat_socket_watch_wait_readable_multi(
        watches, CPLAT_SOCKET_WATCH_WAIT_MAX, nullptr, 60, ready, &detail);
    const int write_with_cancel = cplat_socket_watch_wait_writable(watches[0], cancel, 60, &detail);

    // Assert
    EXPECT_EQ(CPLAT_ERR_TIMEOUT, read_with_cancel);
    EXPECT_EQ(CPLAT_ERR_TIMEOUT, read_without_cancel);
    EXPECT_EQ(CPLAT_ERR_TIMEOUT, write_with_cancel);
    EXPECT_GT(cancel_read_count, 0);
    EXPECT_GT(no_cancel_read_count, 0);
    EXPECT_GT(writable_cancel_count, 0);

    cplat_socket_wait_cancel_dispose(cancel);
    for (size_t index = 0U; index < CPLAT_SOCKET_WATCH_WAIT_MAX; ++index)
    {
        cplat_socket_watch_detach(watches[index]);
    }
    ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mock_winsock_));
// [確認_正常系] - 受信待機の対象数が解除ありで 33、解除なしで 32、送信待機の解除ありで 3 となること。
}

namespace
{

/* 方向別の待機の競合を障壁で固定するため、Winsock のイベントを試験側で模擬します。
 * 製品のソースに差し込み口を設けず、mock の呼び出しの中で待ち合わせて順序を決めます。
 * 呼び出しの記録と障壁は、スレッドごとに設定する待機の役割で区別します。
 */
enum class watch_role
{
    MAIN,
    READER,
    WRITER,
    PROBE
};

thread_local watch_role s_current_watch_role = watch_role::MAIN;

enum class simulated_call_kind
{
    ZERO_WAIT,
    BLOCKING_WAIT,
    ENUMERATE,
    SET_EVENT,
    RESET_EVENT,
    SEND,
    FINISHED
};

enum class simulated_gate
{
    BEFORE_BLOCKING_WAIT,
    ZERO_WAIT,
    ENUMERATE
};

struct simulated_call
{
    simulated_call_kind kind = simulated_call_kind::FINISHED;
    watch_role role = watch_role::MAIN;
    WSAEVENT event = nullptr;
    std::vector<WSAEVENT> events;
    DWORD timeout_ms = 0U;
    DWORD result = WSA_WAIT_TIMEOUT;
    bool returned = false;
    bool timed_out = false;
    long network_events = 0;
};

using simulated_calls = std::vector<simulated_call>;

struct watch_events
{
    WSAEVENT shared = nullptr;
    WSAEVENT read = nullptr;
    WSAEVENT write = nullptr;
};

/* 製品の 50 [ms] の内部間隔を超えて待ち、失われた起床を時間切れの記録として検出します。 */
constexpr DWORD kSimulatedWaitGuardMs = 2000U;
constexpr auto kSynchronizationLimit = std::chrono::seconds(5);

class winsock_event_simulator
{
  public:
    winsock_event_simulator(Mock_winsock &mock_winsock, const bool honor_timeout) : honor_timeout_(honor_timeout)
    {
        ON_CALL(mock_winsock, WSACreateEvent(_, _, _))
            .WillByDefault([this](const char *, const int, const char *) { return create_event(); });
        ON_CALL(mock_winsock, WSACloseEvent(_, _, _, _)).WillByDefault(Return(TRUE));
        ON_CALL(mock_winsock, WSAEventSelect(_, _, _, _, _, _))
            .WillByDefault(
                [this](const char *, const int, const char *, SOCKET, WSAEVENT event, long)
                {
                    select_event(event);
                    return 0;
                });
        ON_CALL(mock_winsock, WSASetEvent(_, _, _, _))
            .WillByDefault(
                [this](const char *, const int, const char *, WSAEVENT event)
                {
                    change_event(simulated_call_kind::SET_EVENT, event, true);
                    return TRUE;
                });
        ON_CALL(mock_winsock, WSAResetEvent(_, _, _, _))
            .WillByDefault(
                [this](const char *, const int, const char *, WSAEVENT event)
                {
                    change_event(simulated_call_kind::RESET_EVENT, event, false);
                    return TRUE;
                });
        ON_CALL(mock_winsock, WSAWaitForMultipleEvents(_, _, _, _, _, _, _, _))
            .WillByDefault([this](const char *, const int, const char *, const DWORD count, const WSAEVENT *events,
                                  const BOOL, const DWORD timeout_ms, const BOOL)
                           { return wait_events(count, events, timeout_ms); });
        ON_CALL(mock_winsock, WSAEnumNetworkEvents(_, _, _, _, _, _))
            .WillByDefault([this](const char *, const int, const char *, SOCKET, WSAEVENT event,
                                  LPWSANETWORKEVENTS network_events) { return enumerate(event, network_events); });
        ON_CALL(mock_winsock, WSAGetLastError(_, _, _)).WillByDefault(Return(WSAEWOULDBLOCK));
    }

    /* 登録は E_s、E_r、E_w の順にイベントを生成するため、直近の 3 個を返します。 */
    watch_events latest_watch_events()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        watch_events events;
        const size_t count = created_.size();

        if (count >= 3U)
        {
            events.shared = created_[count - 3U];
            events.read = created_[count - 2U];
            events.write = created_[count - 1U];
        }

        return events;
    }

    /* OS が E_s へネットワーク イベントを記録した状態を作ります。 */
    void inject(const long network_events)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_network_events_ |= network_events;
        signaled_[shared_] = true;
        changed_.notify_all();
    }

    /* 障壁は 1 回だけ働きます。同じ障壁を再び置く場合も、前回の到着を消してから置きます。 */
    void arm(const simulated_gate gate, const watch_role role)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        gate_state &state = gates_[std::make_pair(gate, role)];

        state.armed = true;
        state.arrived = false;
        state.released = false;
    }

    bool wait_arrived(const simulated_gate gate, const watch_role role)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        gate_state &state = gates_[std::make_pair(gate, role)];

        return changed_.wait_for(lock, kSynchronizationLimit, [&]() { return state.arrived; });
    }

    void release(const simulated_gate gate, const watch_role role)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        gates_[std::make_pair(gate, role)].released = true;
        changed_.notify_all();
    }

    /* 失敗時に止まったスレッドを回収するため、すべての障壁を開けます。 */
    void open_all_gates()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        gates_opened_ = true;
        changed_.notify_all();
    }

    size_t mark()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        return calls_.size();
    }

    bool wait_until(const std::function<bool(const simulated_calls &)> &predicate)
    {
        std::unique_lock<std::mutex> lock(mutex_);

        return changed_.wait_for(lock, kSynchronizationLimit, [&]() { return predicate(calls_); });
    }

    simulated_calls snapshot()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        return calls_;
    }

    void record(const simulated_call_kind kind)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        simulated_call call;

        call.kind = kind;
        call.role = s_current_watch_role;
        call.returned = true;
        calls_.push_back(call);
        changed_.notify_all();
    }

  private:
    struct gate_state
    {
        bool armed = false;
        bool arrived = false;
        bool released = false;
    };

    WSAEVENT create_event()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const WSAEVENT event = reinterpret_cast<WSAEVENT>(static_cast<uintptr_t>(0x1000U + created_.size()));

        created_.push_back(event);
        signaled_[event] = false;

        return event;
    }

    void select_event(const WSAEVENT event)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (event != nullptr)
        {
            shared_ = event;
        }
    }

    void change_event(const simulated_call_kind kind, const WSAEVENT event, const bool signaled)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        simulated_call call;

        call.kind = kind;
        call.role = s_current_watch_role;
        call.event = event;
        call.returned = true;
        signaled_[event] = signaled;
        calls_.push_back(call);
        changed_.notify_all();
    }

    DWORD wait_events(const DWORD count, const WSAEVENT *events, const DWORD timeout_ms)
    {
        const watch_role role = s_current_watch_role;

        pass_gate(timeout_ms == 0U ? simulated_gate::ZERO_WAIT : simulated_gate::BEFORE_BLOCKING_WAIT, role);

        std::unique_lock<std::mutex> lock(mutex_);
        simulated_call call;
        const size_t index = calls_.size();

        call.kind = timeout_ms == 0U ? simulated_call_kind::ZERO_WAIT : simulated_call_kind::BLOCKING_WAIT;
        call.role = role;
        call.events.assign(events, events + count);
        call.timeout_ms = timeout_ms;
        calls_.push_back(call);
        changed_.notify_all();

        DWORD result = first_signaled_locked(count, events);

        if (result == WSA_WAIT_TIMEOUT && timeout_ms != 0U)
        {
            const DWORD limit_ms = honor_timeout_ ? timeout_ms : kSimulatedWaitGuardMs;
            const bool woke = changed_.wait_for(lock, std::chrono::milliseconds(limit_ms), [&]()
                                                { return first_signaled_locked(count, events) != WSA_WAIT_TIMEOUT; });

            result = woke ? first_signaled_locked(count, events) : WSA_WAIT_TIMEOUT;
            calls_[index].timed_out = !woke;
        }

        calls_[index].result = result;
        calls_[index].returned = true;
        changed_.notify_all();

        return result;
    }

    /* WSAEnumNetworkEvents と同じく、記録の取り出しと E_s のリセットを 1 回の呼び出しで行います。 */
    int enumerate(const WSAEVENT event, LPWSANETWORKEVENTS network_events)
    {
        const watch_role role = s_current_watch_role;

        pass_gate(simulated_gate::ENUMERATE, role);

        std::lock_guard<std::mutex> lock(mutex_);
        simulated_call call;

        std::memset(network_events, 0, sizeof(*network_events));
        network_events->lNetworkEvents = pending_network_events_;
        pending_network_events_ = 0;
        signaled_[event] = false;
        call.kind = simulated_call_kind::ENUMERATE;
        call.role = role;
        call.event = event;
        call.network_events = network_events->lNetworkEvents;
        call.returned = true;
        calls_.push_back(call);
        changed_.notify_all();

        return 0;
    }

    void pass_gate(const simulated_gate gate, const watch_role role)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        gate_state &state = gates_[std::make_pair(gate, role)];

        if (gates_opened_ || !state.armed)
        {
            return;
        }

        state.armed = false;
        state.arrived = true;
        changed_.notify_all();
        changed_.wait(lock, [&]() { return state.released || gates_opened_; });
        state.released = false;
    }

    DWORD first_signaled_locked(const DWORD count, const WSAEVENT *events)
    {
        for (DWORD index = 0U; index < count; ++index)
        {
            if (signaled_[events[index]])
            {
                return WSA_WAIT_EVENT_0 + index;
            }
        }

        return WSA_WAIT_TIMEOUT;
    }

    const bool honor_timeout_;
    std::mutex mutex_;
    std::condition_variable changed_;
    std::vector<WSAEVENT> created_;
    std::map<WSAEVENT, bool> signaled_;
    std::map<std::pair<simulated_gate, watch_role>, gate_state> gates_;
    bool gates_opened_ = false;
    WSAEVENT shared_ = nullptr;
    long pending_network_events_ = 0;
    simulated_calls calls_;
};

/* 待機のスレッドを役割付きで起動し、テストの途中で失敗しても障壁と解除で回収します。 */
class watch_thread_group
{
  public:
    watch_thread_group(winsock_event_simulator &simulator, std::vector<cplat_socket_wait_cancel *> cancels)
        : simulator_(simulator), cancels_(std::move(cancels))
    {
    }

    ~watch_thread_group()
    {
        bool running = false;

        for (const std::thread &thread : threads_)
        {
            running = running || thread.joinable();
        }
        if (running)
        {
            simulator_.open_all_gates();
            for (cplat_socket_wait_cancel *cancel : cancels_)
            {
                /* 回収のための通知で、有効なオブジェクトでは常に成功します。 */
                (void)cplat_socket_wait_cancel_signal(cancel, nullptr);
            }
        }
        join_all();
    }

    watch_thread_group(const watch_thread_group &) = delete;
    watch_thread_group &operator=(const watch_thread_group &) = delete;

    void start(const watch_role role, std::function<void()> body)
    {
        threads_.emplace_back(
            [this, role, body]()
            {
                s_current_watch_role = role;
                body();
                simulator_.record(simulated_call_kind::FINISHED);
            });
    }

    void join_all()
    {
        for (std::thread &thread : threads_)
        {
            if (thread.joinable())
            {
                thread.join();
            }
        }
    }

  private:
    winsock_event_simulator &simulator_;
    std::vector<cplat_socket_wait_cancel *> cancels_;
    std::vector<std::thread> threads_;
};

size_t count_calls(const simulated_calls &calls, const simulated_call_kind kind, const watch_role role,
                   const size_t from = 0U)
{
    size_t count = 0U;

    for (size_t index = from; index < calls.size(); ++index)
    {
        if (calls[index].kind == kind && calls[index].role == role)
        {
            ++count;
        }
    }

    return count;
}

size_t count_event_calls(const simulated_calls &calls, const simulated_call_kind kind, const watch_role role,
                         const WSAEVENT event, const size_t from = 0U)
{
    size_t count = 0U;

    for (size_t index = from; index < calls.size(); ++index)
    {
        if (calls[index].kind == kind && calls[index].role == role && calls[index].event == event)
        {
            ++count;
        }
    }

    return count;
}

/* 見つからない場合は calls.size() を返します。 */
size_t find_call(const simulated_calls &calls, const simulated_call_kind kind, const watch_role role,
                 const size_t from = 0U)
{
    for (size_t index = from; index < calls.size(); ++index)
    {
        if (calls[index].kind == kind && calls[index].role == role)
        {
            return index;
        }
    }

    return calls.size();
}

size_t find_event_call(const simulated_calls &calls, const simulated_call_kind kind, const watch_role role,
                       const WSAEVENT event, const size_t from = 0U)
{
    for (size_t index = from; index < calls.size(); ++index)
    {
        if (calls[index].kind == kind && calls[index].role == role && calls[index].event == event)
        {
            return index;
        }
    }

    return calls.size();
}

bool is_blocked(const simulated_calls &calls, const watch_role role)
{
    for (size_t index = calls.size(); index > 0U; --index)
    {
        const simulated_call &call = calls[index - 1U];

        if (call.kind == simulated_call_kind::BLOCKING_WAIT && call.role == role)
        {
            return !call.returned;
        }
    }

    return false;
}

bool has_finished(const simulated_calls &calls, const watch_role role, const size_t from = 0U)
{
    return find_call(calls, simulated_call_kind::FINISHED, role, from) < calls.size();
}

size_t count_guard_timeouts(const simulated_calls &calls)
{
    size_t count = 0U;

    for (const simulated_call &call : calls)
    {
        if (call.kind == simulated_call_kind::BLOCKING_WAIT && call.returned && call.timed_out)
        {
            ++count;
        }
    }

    return count;
}

/* 最初の待機を起こしたイベントを返します。待機がない場合と時間切れの場合は nullptr を返します。 */
WSAEVENT first_woken_event(const simulated_calls &calls, const watch_role role, const size_t from = 0U)
{
    const size_t index = find_call(calls, simulated_call_kind::BLOCKING_WAIT, role, from);

    if (index >= calls.size() || !calls[index].returned || calls[index].result == WSA_WAIT_TIMEOUT)
    {
        return nullptr;
    }

    const DWORD offset = calls[index].result - WSA_WAIT_EVENT_0;

    return offset < calls[index].events.size() ? calls[index].events[offset] : nullptr;
}

/* 最初の列挙が返したネットワーク イベントを返します。列挙がない場合は -1 を返します。 */
long first_network_events(const simulated_calls &calls, const watch_role role, const size_t from = 0U)
{
    const size_t index = find_call(calls, simulated_call_kind::ENUMERATE, role, from);

    return index < calls.size() ? calls[index].network_events : -1L;
}

/* 列挙のあとに、同じ役割の待機が始まって戻っていないことを判定します。 */
bool is_blocked_after_enumeration(const simulated_calls &calls, const watch_role role, const size_t from)
{
    const size_t enumeration = find_call(calls, simulated_call_kind::ENUMERATE, role, from);

    return enumeration < calls.size() &&
           find_call(calls, simulated_call_kind::BLOCKING_WAIT, role, enumeration) < calls.size() &&
           is_blocked(calls, role);
}

} // namespace

// 送信側が E_w をリセットして待機の直前で止まる間に受信側が FD_WRITE を取得しても、送信側が E_w で起床することの確認
TEST_F(socketTest, watch_writer_wakes_by_write_event_set_by_reader)
{
    // Arrange
    RealWinsockSession winsock_session;
    ASSERT_TRUE(winsock_session.started()); // [状態確認] - winsock_session.started() の戻り値が true であること。
    winsock_event_simulator simulator(mock_winsock_,
                                      false); // [状態] - Winsock のイベントを模擬し、障壁で待機を止められるようにする。
    cplat_socket_wait_cancel *reader_cancel = nullptr;
    cplat_socket_wait_cancel *writer_cancel = nullptr;
    ASSERT_EQ(CPLAT_OK,
              cplat_socket_wait_cancel_create(
                  &reader_cancel,
                  nullptr)); // [状態確認] - 受信側の cplat_socket_wait_cancel_create() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_socket_wait_cancel_create(
                  &writer_cancel,
                  nullptr)); // [状態確認] - 送信側の cplat_socket_wait_cancel_create() の戻り値が CPLAT_OK であること。
    cplat_socket_watch *watch = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_socket_watch_attach(
                            kSocket, &watch,
                            nullptr)); // [状態確認] - cplat_socket_watch_attach() の戻り値が CPLAT_OK であること。
    const watch_events events = simulator.latest_watch_events(); // [状態] - 登録の E_s、E_r、E_w を取り出す。
    cplat_socket_watch *watches[] = {watch};
    unsigned char ready[] = {0U};
    const char payload[] = "x";
    size_t sent = 0U;
    std::atomic<int> actual_ret_writer{CPLAT_ERR_UNKNOWN};
    std::atomic<int> actual_ret_reader{CPLAT_ERR_UNKNOWN};
    watch_thread_group threads(simulator, {reader_cancel, writer_cancel});

    // Pre-Assert
    EXPECT_CALL(mock_winsock_, send(_, _, _, (SOCKET)kSocket, _, 1, 0))
        .WillOnce(
            Return(SOCKET_ERROR)); // [Pre-Assert確認_正常系] - send() が kSocket と 1 を指定して 1 回呼び出されること。
                                   // [Pre-Assert手順] - send() から SOCKET_ERROR を返す。

    // Act
    const int actual_ret_send = cplat_socket_watch_send(
        watch, payload, 1U, &sent,
        nullptr); // [手順] - WSAEWOULDBLOCK で失敗する送信を指定して cplat_socket_watch_send() を呼び出す。
    simulator.arm(simulated_gate::BEFORE_BLOCKING_WAIT,
                  watch_role::WRITER); // [手順] - 送信側の待機の直前に障壁を置く。
    threads.start(watch_role::WRITER,
                  [&]()
                  {
                      actual_ret_writer.store(cplat_socket_watch_wait_writable(watch, writer_cancel, 10000, nullptr));
                  }); // [手順] - 送信側のスレッドで cplat_socket_watch_wait_writable() を呼び出す。
    ASSERT_TRUE(simulator.wait_arrived(
        simulated_gate::BEFORE_BLOCKING_WAIT,
        watch_role::
            WRITER)); // [状態確認] - 送信側の待機の直前の障壁を待つ simulator.wait_arrived() の戻り値が true であること。
    simulator.inject(FD_WRITE); // [手順] - E_s へ FD_WRITE を通知する。
    threads.start(watch_role::READER,
                  [&]()
                  {
                      actual_ret_reader.store(
                          cplat_socket_watch_wait_readable_multi(watches, 1U, reader_cancel, 10000, ready, nullptr));
                  }); // [手順] - 受信側のスレッドで cplat_socket_watch_wait_readable_multi() を呼び出す。
    ASSERT_TRUE(simulator.wait_until(
        [](const simulated_calls &calls)
        {
            return is_blocked_after_enumeration(calls, watch_role::READER, 0U);
        })); // [状態確認] - 受信側の列挙の後の再待機を待つ simulator.wait_until() の戻り値が true であること。
    simulator.release(simulated_gate::BEFORE_BLOCKING_WAIT, watch_role::WRITER); // [手順] - 送信側の待機を開始させる。
    ASSERT_TRUE(simulator.wait_until(
        [](const simulated_calls &calls)
        {
            return has_finished(calls, watch_role::WRITER);
        })); // [状態確認] - 送信側の終了を待つ simulator.wait_until() の戻り値が true であること。
    const simulated_calls calls = simulator.snapshot(); // [手順] - 受信側の待機中に呼び出しの記録を取り出す。
    const int actual_ret_signal = cplat_socket_wait_cancel_signal(
        reader_cancel,
        nullptr);       // [手順] - 受信側の解除のオブジェクトを指定して cplat_socket_wait_cancel_signal() を呼び出す。
    threads.join_all(); // [手順] - 両方のスレッドの終了を待つ。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_WOULD_BLOCK,
        actual_ret_send); // [確認_正常系] - cplat_socket_watch_send() の戻り値が CPLAT_ERR_WOULD_BLOCK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_writer
            .load()); // [確認_正常系] - 送信側の cplat_socket_watch_wait_writable() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        FD_WRITE,
        first_network_events(
            calls,
            watch_role::
                READER)); // [確認_正常系] - 受信側の first_network_events() の戻り値が FD_WRITE であること。(受信側が FD_WRITE を取得したことの確認)
    EXPECT_EQ(
        1U,
        count_event_calls(
            calls, simulated_call_kind::SET_EVENT, watch_role::READER,
            events
                .write)); // [確認_正常系] - 受信側が E_w を設定した回数を数える count_event_calls() の戻り値が 1 であること。
    EXPECT_EQ(
        0U,
        count_event_calls(
            calls, simulated_call_kind::RESET_EVENT, watch_role::READER,
            events
                .write)); // [確認_正常系] - 受信側が E_w をリセットした回数を数える count_event_calls() の戻り値が 0 であること。(相手のイベントをリセットしないことの確認)
    EXPECT_EQ(
        0U,
        count_event_calls(
            calls, simulated_call_kind::RESET_EVENT, watch_role::WRITER,
            events
                .read)); // [確認_正常系] - 送信側が E_r をリセットした回数を数える count_event_calls() の戻り値が 0 であること。(相手のイベントをリセットしないことの確認)
    EXPECT_EQ(1U,
              count_calls(
                  calls, simulated_call_kind::BLOCKING_WAIT,
                  watch_role::WRITER)); // [確認_正常系] - 送信側の待機を数える count_calls() の戻り値が 1 であること。
    EXPECT_EQ(
        events.write,
        first_woken_event(
            calls,
            watch_role::
                WRITER)); // [確認_正常系] - 送信側の first_woken_event() の戻り値が events.write と一致すること。(待機の開始直後に E_w で起床したことの確認)
    EXPECT_EQ(
        0U,
        count_calls(
            calls, simulated_call_kind::ENUMERATE,
            watch_role::
                WRITER)); // [確認_正常系] - 送信側の列挙を数える count_calls() の戻り値が 0 であること。(受信側が保存した送信可能で戻ったことの確認)
    EXPECT_TRUE(is_blocked(
        calls,
        watch_role::
            READER)); // [確認_正常系] - 受信側の is_blocked() の戻り値が true であること。(受信側が FD_WRITE の取得の後に再び待機したことの確認)
    EXPECT_EQ(
        0U,
        count_guard_timeouts(
            calls)); // [確認_正常系] - count_guard_timeouts() の戻り値が 0 であること。(起床を失って時間切れになった待機がないことの確認)
    EXPECT_EQ(CPLAT_OK,
              actual_ret_signal); // [確認_正常系] - cplat_socket_wait_cancel_signal() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_CANCELED,
        actual_ret_reader
            .load()); // [確認_正常系] - 受信側の cplat_socket_watch_wait_readable_multi() の戻り値が CPLAT_ERR_CANCELED であること。

    // Cleanup
    cplat_socket_watch_detach(watch);
    cplat_socket_wait_cancel_dispose(writer_cancel);
    cplat_socket_wait_cancel_dispose(reader_cancel);
}
// [サブ手順参照 名前=socketTest.TearDown]

// 受信側が E_r をリセットして待機の直前で止まる間に送信側が FD_READ または FD_ACCEPT を取得しても、受信側が E_r で起床することの確認
TEST_F(socketTest, watch_reader_wakes_by_read_event_set_by_writer)
{
    // Arrange
    RealWinsockSession winsock_session;
    ASSERT_TRUE(winsock_session.started()); // [状態確認] - winsock_session.started() の戻り値が true であること。
    winsock_event_simulator simulator(mock_winsock_,
                                      false); // [状態] - Winsock のイベントを模擬し、障壁で待機を止められるようにする。
    const long notifications[] = {FD_READ, FD_ACCEPT}; // [状態] - 受信側の通知として FD_READ と FD_ACCEPT を用意する。
    const char payload[] = "x";
    int actual_ret_send[2] = {0};
    int actual_ret_reader[2] = {0};
    int actual_ret_writer[2] = {0};
    int actual_ret_signal[2] = {0};
    unsigned char ready[2] = {0U};
    watch_events events[2];
    simulated_calls calls[2];

    // Pre-Assert
    EXPECT_CALL(mock_winsock_, send(_, _, _, (SOCKET)kSocket, _, 1, 0))
        .Times(2)
        .WillRepeatedly(
            Return(SOCKET_ERROR)); // [Pre-Assert確認_正常系] - send() が kSocket と 1 を指定して 2 回呼び出されること。
                                   // [Pre-Assert手順] - send() から SOCKET_ERROR を返す。

    // Act
    for (size_t index = 0U; index < 2U; ++index)
    {
        cplat_socket_wait_cancel *reader_cancel = nullptr;
        cplat_socket_wait_cancel *writer_cancel = nullptr;
        ASSERT_EQ(
            CPLAT_OK,
            cplat_socket_wait_cancel_create(
                &reader_cancel,
                nullptr)); // [状態確認] - 受信側の cplat_socket_wait_cancel_create() の戻り値が CPLAT_OK であること。
        ASSERT_EQ(
            CPLAT_OK,
            cplat_socket_wait_cancel_create(
                &writer_cancel,
                nullptr)); // [状態確認] - 送信側の cplat_socket_wait_cancel_create() の戻り値が CPLAT_OK であること。
        cplat_socket_watch *watch = nullptr;
        ASSERT_EQ(CPLAT_OK, cplat_socket_watch_attach(
                                kSocket, &watch,
                                nullptr)); // [状態確認] - cplat_socket_watch_attach() の戻り値が CPLAT_OK であること。
        events[index] = simulator.latest_watch_events(); // [手順] - 登録の E_s、E_r、E_w を取り出す。
        const size_t from = simulator.mark();            // [手順] - この反復の呼び出しの記録の開始位置を控える。
        cplat_socket_watch *watches[] = {watch};
        unsigned char ready_once[] = {0U};
        size_t sent = 0U;
        std::atomic<int> reader_ret{CPLAT_ERR_UNKNOWN};
        std::atomic<int> writer_ret{CPLAT_ERR_UNKNOWN};
        {
            watch_thread_group threads(simulator, {reader_cancel, writer_cancel});

            actual_ret_send[index] = cplat_socket_watch_send(
                watch, payload, 1U, &sent,
                nullptr); // [手順] - WSAEWOULDBLOCK で失敗する送信を指定して cplat_socket_watch_send() を呼び出す。
            simulator.arm(simulated_gate::BEFORE_BLOCKING_WAIT,
                          watch_role::READER); // [手順] - 受信側の待機の直前に障壁を置く。
            threads.start(watch_role::READER,
                          [&]()
                          {
                              reader_ret.store(cplat_socket_watch_wait_readable_multi(watches, 1U, reader_cancel, 10000,
                                                                                      ready_once, nullptr));
                          }); // [手順] - 受信側のスレッドで cplat_socket_watch_wait_readable_multi() を呼び出す。
            ASSERT_TRUE(simulator.wait_arrived(
                simulated_gate::BEFORE_BLOCKING_WAIT,
                watch_role::
                    READER)); // [状態確認] - 受信側の待機の直前の障壁を待つ simulator.wait_arrived() の戻り値が true であること。
            simulator.inject(notifications[index]); // [手順] - E_s へ FD_READ または FD_ACCEPT を通知する。
            threads.start(watch_role::WRITER,
                          [&]()
                          {
                              writer_ret.store(cplat_socket_watch_wait_writable(watch, writer_cancel, 10000, nullptr));
                          }); // [手順] - 送信側のスレッドで cplat_socket_watch_wait_writable() を呼び出す。
            ASSERT_TRUE(simulator.wait_until(
                [from](const simulated_calls &recorded)
                {
                    return is_blocked_after_enumeration(recorded, watch_role::WRITER, from);
                })); // [状態確認] - 送信側の列挙の後の再待機を待つ simulator.wait_until() の戻り値が true であること。
            simulator.release(simulated_gate::BEFORE_BLOCKING_WAIT,
                              watch_role::READER); // [手順] - 受信側の待機を開始させる。
            ASSERT_TRUE(simulator.wait_until(
                [from](const simulated_calls &recorded)
                {
                    return has_finished(recorded, watch_role::READER, from);
                })); // [状態確認] - 受信側の終了を待つ simulator.wait_until() の戻り値が true であること。
            const simulated_calls recorded =
                simulator.snapshot(); // [手順] - 送信側の待機中に呼び出しの記録を取り出す。
            calls[index].assign(recorded.begin() + static_cast<std::ptrdiff_t>(from),
                                recorded.end()); // [手順] - この反復の記録だけを残す。
            actual_ret_signal[index] = cplat_socket_wait_cancel_signal(
                writer_cancel,
                nullptr); // [手順] - 送信側の解除のオブジェクトを指定して cplat_socket_wait_cancel_signal() を呼び出す。
            threads.join_all(); // [手順] - 両方のスレッドの終了を待つ。
        }
        actual_ret_reader[index] = reader_ret.load(); // [手順] - 受信側の戻り値を残す。
        actual_ret_writer[index] = writer_ret.load(); // [手順] - 送信側の戻り値を残す。
        ready[index] = ready_once[0];                 // [手順] - 受信側の受信可能の出力を残す。
        cplat_socket_watch_detach(watch);             // [手順] - この反復の登録を cplat_socket_watch_detach() で解く。
        cplat_socket_wait_cancel_dispose(
            writer_cancel); // [手順] - 送信側の解除のオブジェクトを cplat_socket_wait_cancel_dispose() で解放する。
        cplat_socket_wait_cancel_dispose(
            reader_cancel); // [手順] - 受信側の解除のオブジェクトを cplat_socket_wait_cancel_dispose() で解放する。
    }

    // Assert
    for (size_t index = 0U; index < 2U; ++index)
    {
        EXPECT_EQ(CPLAT_ERR_WOULD_BLOCK,
                  actual_ret_send
                      [index]); // [確認_正常系 回数=2] - actual_ret_send[index] が CPLAT_ERR_WOULD_BLOCK であること。
        EXPECT_EQ(
            notifications[index],
            first_network_events(
                calls[index],
                watch_role::
                    WRITER)); // [確認_正常系 回数=2] - 送信側の first_network_events() の戻り値が notifications[index] と一致すること。(送信側が受信側の通知を取得したことの確認)
        EXPECT_EQ(
            1U,
            count_event_calls(
                calls[index], simulated_call_kind::SET_EVENT, watch_role::WRITER,
                events[index]
                    .read)); // [確認_正常系 回数=2] - 送信側が E_r を設定した回数を数える count_event_calls() の戻り値が 1 であること。
        EXPECT_EQ(
            0U,
            count_event_calls(
                calls[index], simulated_call_kind::RESET_EVENT, watch_role::WRITER,
                events[index]
                    .read)); // [確認_正常系 回数=2] - 送信側が E_r をリセットした回数を数える count_event_calls() の戻り値が 0 であること。(相手のイベントをリセットしないことの確認)
        EXPECT_EQ(
            0U,
            count_event_calls(
                calls[index], simulated_call_kind::RESET_EVENT, watch_role::READER,
                events[index]
                    .write)); // [確認_正常系 回数=2] - 受信側が E_w をリセットした回数を数える count_event_calls() の戻り値が 0 であること。(相手のイベントをリセットしないことの確認)
        EXPECT_EQ(CPLAT_OK,
                  actual_ret_reader[index]); // [確認_正常系 回数=2] - actual_ret_reader[index] が CPLAT_OK であること。
        EXPECT_EQ(
            1U,
            ready
                [index]); // [確認_正常系 回数=2] - ready[index] が 1 であること。(送信側が pending を消費しないことの確認)
        EXPECT_EQ(
            1U, count_calls(
                    calls[index], simulated_call_kind::BLOCKING_WAIT,
                    watch_role::
                        READER)); // [確認_正常系 回数=2] - 受信側の待機を数える count_calls() の戻り値が 1 であること。
        EXPECT_EQ(
            events[index].read,
            first_woken_event(
                calls[index],
                watch_role::
                    READER)); // [確認_正常系 回数=2] - 受信側の first_woken_event() の戻り値が events[index].read と一致すること。(待機の開始直後に E_r で起床したことの確認)
        EXPECT_EQ(
            0U,
            count_calls(
                calls[index], simulated_call_kind::ENUMERATE,
                watch_role::
                    READER)); // [確認_正常系 回数=2] - 受信側の列挙を数える count_calls() の戻り値が 0 であること。(送信側が保存した受信の通知で戻ったことの確認)
        EXPECT_TRUE(is_blocked(
            calls[index],
            watch_role::
                WRITER)); // [確認_正常系 回数=2] - 送信側の is_blocked() の戻り値が true であること。(送信側が通知の取得の後に再び待機したことの確認)
        EXPECT_EQ(
            0U,
            count_guard_timeouts(
                calls
                    [index])); // [確認_正常系 回数=2] - count_guard_timeouts() の戻り値が 0 であること。(起床を失って時間切れになった待機がないことの確認)
        EXPECT_EQ(CPLAT_OK,
                  actual_ret_signal[index]); // [確認_正常系 回数=2] - actual_ret_signal[index] が CPLAT_OK であること。
        EXPECT_EQ(CPLAT_ERR_CANCELED,
                  actual_ret_writer
                      [index]); // [確認_正常系 回数=2] - actual_ret_writer[index] が CPLAT_ERR_CANCELED であること。
    }
}
// [サブ手順参照 名前=socketTest.TearDown]

// 一方の方向だけが待機して他方向の通知を取得した後に、他方向が待機を始めても通知を失わないことの確認
TEST_F(socketTest, watch_later_direction_uses_notification_taken_by_other_direction)
{
    // Arrange
    RealWinsockSession winsock_session;
    ASSERT_TRUE(winsock_session.started()); // [状態確認] - winsock_session.started() の戻り値が true であること。
    winsock_event_simulator simulator(mock_winsock_,
                                      false); // [状態] - Winsock のイベントを模擬し、障壁で待機を止められるようにする。
    cplat_socket_wait_cancel *reader_cancel = nullptr;
    cplat_socket_wait_cancel *writer_cancel = nullptr;
    ASSERT_EQ(CPLAT_OK,
              cplat_socket_wait_cancel_create(
                  &reader_cancel,
                  nullptr)); // [状態確認] - 受信側の cplat_socket_wait_cancel_create() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_socket_wait_cancel_create(
                  &writer_cancel,
                  nullptr)); // [状態確認] - 送信側の cplat_socket_wait_cancel_create() の戻り値が CPLAT_OK であること。
    cplat_socket_watch *watch = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_socket_watch_attach(
                            kSocket, &watch,
                            nullptr)); // [状態確認] - cplat_socket_watch_attach() の戻り値が CPLAT_OK であること。
    const watch_events events = simulator.latest_watch_events(); // [状態] - 登録の E_s、E_r、E_w を取り出す。
    cplat_socket_watch *watches[] = {watch};
    unsigned char reader_ready[] = {0U};
    unsigned char later_ready[] = {0U};
    const char payload[] = "x";
    size_t sent = 0U;
    std::atomic<int> actual_ret_reader{CPLAT_ERR_UNKNOWN};
    std::atomic<int> actual_ret_writer{CPLAT_ERR_UNKNOWN};
    watch_thread_group threads(simulator, {reader_cancel, writer_cancel});

    // Pre-Assert
    EXPECT_CALL(mock_winsock_, send(_, _, _, (SOCKET)kSocket, _, 1, 0))
        .Times(2)
        .WillRepeatedly(
            Return(SOCKET_ERROR)); // [Pre-Assert確認_正常系] - send() が kSocket と 1 を指定して 2 回呼び出されること。
                                   // [Pre-Assert手順] - send() から SOCKET_ERROR を返す。

    // Act
    const int actual_ret_send_first = cplat_socket_watch_send(
        watch, payload, 1U, &sent,
        nullptr); // [手順] - WSAEWOULDBLOCK で失敗する送信を指定して cplat_socket_watch_send() を呼び出す。
    threads.start(watch_role::READER,
                  [&]()
                  {
                      actual_ret_reader.store(cplat_socket_watch_wait_readable_multi(watches, 1U, reader_cancel, 10000,
                                                                                     reader_ready, nullptr));
                  }); // [手順] - 受信側のスレッドで cplat_socket_watch_wait_readable_multi() を呼び出す。
    ASSERT_TRUE(simulator.wait_until(
        [](const simulated_calls &calls)
        {
            return is_blocked(calls, watch_role::READER);
        })); // [状態確認] - 受信側の待機を待つ simulator.wait_until() の戻り値が true であること。
    const size_t from_write = simulator.mark(); // [手順] - FD_WRITE の通知の前の記録の位置を控える。
    simulator.inject(FD_WRITE);                 // [手順] - E_s へ FD_WRITE を通知する。
    ASSERT_TRUE(simulator.wait_until(
        [from_write](const simulated_calls &calls)
        {
            return is_blocked_after_enumeration(calls, watch_role::READER, from_write);
        })); // [状態確認] - 受信側の列挙の後の再待機を待つ simulator.wait_until() の戻り値が true であること。
    const int actual_ret_writer_later = cplat_socket_watch_wait_writable(
        watch, nullptr, 1000,
        nullptr); // [手順] - 受信側が FD_WRITE を取得した後に、送信側として cplat_socket_watch_wait_writable() を呼び出す。
    const simulated_calls calls_write = simulator.snapshot(); // [手順] - 呼び出しの記録を取り出す。
    const int actual_ret_signal_reader = cplat_socket_wait_cancel_signal(
        reader_cancel,
        nullptr); // [手順] - 受信側の解除のオブジェクトを指定して cplat_socket_wait_cancel_signal() を呼び出す。
    ASSERT_TRUE(simulator.wait_until(
        [](const simulated_calls &calls)
        {
            return has_finished(calls, watch_role::READER);
        })); // [状態確認] - 受信側の終了を待つ simulator.wait_until() の戻り値が true であること。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_WOULD_BLOCK,
        actual_ret_send_first); // [確認_正常系] - 1 回目の cplat_socket_watch_send() の戻り値が CPLAT_ERR_WOULD_BLOCK であること。
    EXPECT_EQ(
        FD_WRITE,
        first_network_events(
            calls_write, watch_role::READER,
            from_write)); // [確認_正常系] - 受信側の first_network_events() の戻り値が FD_WRITE であること。(受信側が他方向の通知を取得したことの確認)
    EXPECT_EQ(
        1U,
        count_event_calls(
            calls_write, simulated_call_kind::SET_EVENT, watch_role::READER, events.write,
            from_write)); // [確認_正常系] - 受信側が E_w を設定した回数を数える count_event_calls() の戻り値が 1 であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_writer_later); // [確認_正常系] - 後から待機を始めた cplat_socket_watch_wait_writable() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        0U,
        count_calls(
            calls_write, simulated_call_kind::BLOCKING_WAIT, watch_role::MAIN,
            from_write)); // [確認_正常系] - 後から始めた送信側の待機を数える count_calls() の戻り値が 0 であること。(ロック内の状態の確認で待たずに戻ったことの確認)
    EXPECT_EQ(
        0U,
        count_calls(
            calls_write, simulated_call_kind::ENUMERATE, watch_role::MAIN,
            from_write)); // [確認_正常系] - 後から始めた送信側の列挙を数える count_calls() の戻り値が 0 であること。
    EXPECT_EQ(
        0U,
        count_guard_timeouts(
            calls_write)); // [確認_正常系] - count_guard_timeouts() の戻り値が 0 であること。(起床を失って時間切れになった待機がないことの確認)
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_signal_reader); // [確認_正常系] - 受信側の cplat_socket_wait_cancel_signal() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_CANCELED,
        actual_ret_reader
            .load()); // [確認_正常系] - 受信側の cplat_socket_watch_wait_readable_multi() の戻り値が CPLAT_ERR_CANCELED であること。

    // Act_2
    const int actual_ret_send_second = cplat_socket_watch_send(
        watch, payload, 1U, &sent,
        nullptr); // [手順] - WSAEWOULDBLOCK で失敗する送信を指定して、もう一度 cplat_socket_watch_send() を呼び出す。
    threads.start(watch_role::WRITER,
                  [&]()
                  {
                      actual_ret_writer.store(cplat_socket_watch_wait_writable(watch, writer_cancel, 10000, nullptr));
                  }); // [手順] - 送信側のスレッドで cplat_socket_watch_wait_writable() を呼び出す。
    ASSERT_TRUE(simulator.wait_until(
        [](const simulated_calls &calls)
        {
            return is_blocked(calls, watch_role::WRITER);
        })); // [状態確認] - 送信側の待機を待つ simulator.wait_until() の戻り値が true であること。
    const size_t from_read = simulator.mark(); // [手順] - FD_READ の通知の前の記録の位置を控える。
    simulator.inject(FD_READ);                 // [手順] - E_s へ FD_READ を通知する。
    ASSERT_TRUE(simulator.wait_until(
        [from_read](const simulated_calls &calls)
        {
            return is_blocked_after_enumeration(calls, watch_role::WRITER, from_read);
        })); // [状態確認] - 送信側の列挙の後の再待機を待つ simulator.wait_until() の戻り値が true であること。
    const int actual_ret_reader_later = cplat_socket_watch_wait_readable_multi(
        watches, 1U, nullptr, 1000, later_ready,
        nullptr); // [手順] - 送信側が FD_READ を取得した後に、受信側として cplat_socket_watch_wait_readable_multi() を呼び出す。
    const simulated_calls calls_read = simulator.snapshot(); // [手順] - 呼び出しの記録を取り出す。
    const int actual_ret_signal_writer = cplat_socket_wait_cancel_signal(
        writer_cancel,
        nullptr);       // [手順] - 送信側の解除のオブジェクトを指定して cplat_socket_wait_cancel_signal() を呼び出す。
    threads.join_all(); // [手順] - スレッドの終了を待つ。

    // Assert_2
    EXPECT_EQ(
        CPLAT_ERR_WOULD_BLOCK,
        actual_ret_send_second); // [確認_正常系] - 2 回目の cplat_socket_watch_send() の戻り値が CPLAT_ERR_WOULD_BLOCK であること。
    EXPECT_EQ(
        FD_READ,
        first_network_events(
            calls_read, watch_role::WRITER,
            from_read)); // [確認_正常系] - 送信側の first_network_events() の戻り値が FD_READ であること。(送信側が他方向の通知を取得したことの確認)
    EXPECT_EQ(
        1U,
        count_event_calls(
            calls_read, simulated_call_kind::SET_EVENT, watch_role::WRITER, events.read,
            from_read)); // [確認_正常系] - 送信側が E_r を設定した回数を数える count_event_calls() の戻り値が 1 であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_reader_later); // [確認_正常系] - 後から待機を始めた cplat_socket_watch_wait_readable_multi() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1U, later_ready[0]); // [確認_正常系] - later_ready[0] が 1 であること。
    EXPECT_EQ(
        0U,
        count_calls(
            calls_read, simulated_call_kind::BLOCKING_WAIT, watch_role::MAIN,
            from_read)); // [確認_正常系] - 後から始めた受信側の待機を数える count_calls() の戻り値が 0 であること。(ロック内の状態の確認で待たずに戻ったことの確認)
    EXPECT_EQ(
        0U, count_calls(
                calls_read, simulated_call_kind::ENUMERATE, watch_role::MAIN,
                from_read)); // [確認_正常系] - 後から始めた受信側の列挙を数える count_calls() の戻り値が 0 であること。
    EXPECT_EQ(
        0U,
        count_guard_timeouts(
            calls_read)); // [確認_正常系] - count_guard_timeouts() の戻り値が 0 であること。(起床を失って時間切れになった待機がないことの確認)
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_signal_writer); // [確認_正常系] - 送信側の cplat_socket_wait_cancel_signal() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_CANCELED,
        actual_ret_writer
            .load()); // [確認_正常系] - 送信側の cplat_socket_watch_wait_writable() の戻り値が CPLAT_ERR_CANCELED であること。

    // Cleanup
    cplat_socket_watch_detach(watch);
    cplat_socket_wait_cancel_dispose(writer_cancel);
    cplat_socket_wait_cancel_dispose(reader_cancel);
}
// [サブ手順参照 名前=socketTest.TearDown]

// 両方向が E_s で起床し、後続側の列挙が空になる順でも、後続側が待機を続けることの確認
TEST_F(socketTest, watch_follower_keeps_waiting_after_empty_enumeration)
{
    // Arrange
    RealWinsockSession winsock_session;
    ASSERT_TRUE(winsock_session.started()); // [状態確認] - winsock_session.started() の戻り値が true であること。
    winsock_event_simulator simulator(mock_winsock_,
                                      false); // [状態] - Winsock のイベントを模擬し、障壁で待機を止められるようにする。
    cplat_socket_wait_cancel *reader_cancel = nullptr;
    cplat_socket_wait_cancel *writer_cancel = nullptr;
    ASSERT_EQ(CPLAT_OK,
              cplat_socket_wait_cancel_create(
                  &reader_cancel,
                  nullptr)); // [状態確認] - 受信側の cplat_socket_wait_cancel_create() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_socket_wait_cancel_create(
                  &writer_cancel,
                  nullptr)); // [状態確認] - 送信側の cplat_socket_wait_cancel_create() の戻り値が CPLAT_OK であること。
    cplat_socket_watch *watch = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_socket_watch_attach(
                            kSocket, &watch,
                            nullptr)); // [状態確認] - cplat_socket_watch_attach() の戻り値が CPLAT_OK であること。
    const watch_events events = simulator.latest_watch_events(); // [状態] - 登録の E_s、E_r、E_w を取り出す。
    cplat_socket_watch *watches[] = {watch};
    unsigned char ready[] = {0U};
    const char payload[] = "x";
    size_t sent = 0U;
    std::atomic<int> actual_ret_reader{CPLAT_ERR_UNKNOWN};
    std::atomic<int> actual_ret_writer{CPLAT_ERR_UNKNOWN};
    watch_thread_group threads(simulator, {reader_cancel, writer_cancel});

    // Pre-Assert
    EXPECT_CALL(mock_winsock_, send(_, _, _, (SOCKET)kSocket, _, 1, 0))
        .WillOnce(
            Return(SOCKET_ERROR)); // [Pre-Assert確認_正常系] - send() が kSocket と 1 を指定して 1 回呼び出されること。
                                   // [Pre-Assert手順] - send() から SOCKET_ERROR を返す。

    // Act
    const int actual_ret_send = cplat_socket_watch_send(
        watch, payload, 1U, &sent,
        nullptr); // [手順] - WSAEWOULDBLOCK で失敗する送信を指定して cplat_socket_watch_send() を呼び出す。
    threads.start(watch_role::READER,
                  [&]()
                  {
                      actual_ret_reader.store(
                          cplat_socket_watch_wait_readable_multi(watches, 1U, reader_cancel, 10000, ready, nullptr));
                  }); // [手順] - 受信側のスレッドで cplat_socket_watch_wait_readable_multi() を呼び出す。
    threads.start(watch_role::WRITER,
                  [&]()
                  {
                      actual_ret_writer.store(cplat_socket_watch_wait_writable(watch, writer_cancel, 10000, nullptr));
                  }); // [手順] - 送信側のスレッドで cplat_socket_watch_wait_writable() を呼び出す。
    ASSERT_TRUE(simulator.wait_until(
        [](const simulated_calls &calls)
        {
            return is_blocked(calls, watch_role::READER) && is_blocked(calls, watch_role::WRITER);
        })); // [状態確認] - 両方向の待機を待つ simulator.wait_until() の戻り値が true であること。
    const size_t from = simulator.mark();                         // [手順] - 通知の前の記録の位置を控える。
    simulator.arm(simulated_gate::ZERO_WAIT, watch_role::READER); // [手順] - 受信側の E_s の確認に障壁を置く。
    simulator.arm(simulated_gate::ZERO_WAIT, watch_role::WRITER); // [手順] - 送信側の E_s の確認に障壁を置く。
    simulator.arm(simulated_gate::ENUMERATE, watch_role::READER); // [手順] - 受信側の列挙に障壁を置く。
    simulator.inject(FD_READ);                                    // [手順] - E_s へ FD_READ を通知する。
    ASSERT_TRUE(simulator.wait_arrived(
        simulated_gate::ZERO_WAIT,
        watch_role::
            READER)); // [状態確認] - 受信側が E_s で起床したことを待つ simulator.wait_arrived() の戻り値が true であること。
    ASSERT_TRUE(simulator.wait_arrived(
        simulated_gate::ZERO_WAIT,
        watch_role::
            WRITER)); // [状態確認] - 送信側が E_s で起床したことを待つ simulator.wait_arrived() の戻り値が true であること。
    simulator.release(simulated_gate::ZERO_WAIT, watch_role::READER); // [手順] - 受信側に E_s を確認させる。
    ASSERT_TRUE(simulator.wait_arrived(
        simulated_gate::ENUMERATE,
        watch_role::
            READER)); // [状態確認] - 受信側の列挙の障壁を待つ simulator.wait_arrived() の戻り値が true であること。
    simulator.release(simulated_gate::ZERO_WAIT,
                      watch_role::WRITER); // [手順] - 受信側の列挙の前に、送信側に E_s を確認させる。
    ASSERT_TRUE(simulator.wait_until(
        [from](const simulated_calls &calls)
        {
            const size_t index = find_call(calls, simulated_call_kind::ZERO_WAIT, watch_role::WRITER, from);
            return index < calls.size() && calls[index].returned;
        })); // [状態確認] - 送信側の E_s の確認を待つ simulator.wait_until() の戻り値が true であること。
    simulator.release(simulated_gate::ENUMERATE, watch_role::READER); // [手順] - 受信側の列挙を進める。
    ASSERT_TRUE(simulator.wait_until(
        [from](const simulated_calls &calls)
        {
            return has_finished(calls, watch_role::READER, from) &&
                   count_calls(calls, simulated_call_kind::ENUMERATE, watch_role::WRITER, from) == 1U &&
                   is_blocked(calls, watch_role::WRITER);
        })); // [状態確認] - 受信側の終了と送信側の再待機を待つ simulator.wait_until() の戻り値が true であること。
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // [手順] - 送信側が待機を続ける間、100 [ms] 待つ。
    const simulated_calls calls = simulator.snapshot();          // [手順] - 送信側の待機中に呼び出しの記録を取り出す。
    const int actual_ret_signal = cplat_socket_wait_cancel_signal(
        writer_cancel,
        nullptr);       // [手順] - 送信側の解除のオブジェクトを指定して cplat_socket_wait_cancel_signal() を呼び出す。
    threads.join_all(); // [手順] - 両方のスレッドの終了を待つ。

    // Assert
    const size_t writer_zero_wait = find_call(calls, simulated_call_kind::ZERO_WAIT, watch_role::WRITER, from);
    const size_t reader_enumeration = find_call(calls, simulated_call_kind::ENUMERATE, watch_role::READER, from);
    const size_t writer_enumeration = find_call(calls, simulated_call_kind::ENUMERATE, watch_role::WRITER, from);
    const size_t writer_wait = find_call(calls, simulated_call_kind::BLOCKING_WAIT, watch_role::WRITER, from);
    const bool writer_wait_found = writer_wait < calls.size();
    const bool writer_waits_own_events = writer_wait_found && calls[writer_wait].events.size() == 3U &&
                                         calls[writer_wait].events[1] == events.write &&
                                         calls[writer_wait].events[2] == events.shared;
    EXPECT_EQ(
        CPLAT_ERR_WOULD_BLOCK,
        actual_ret_send); // [確認_正常系] - cplat_socket_watch_send() の戻り値が CPLAT_ERR_WOULD_BLOCK であること。
    ASSERT_LT(writer_zero_wait, calls.size()); // [確認_正常系] - writer_zero_wait が calls.size() より小さいこと。
    EXPECT_EQ(
        WSA_WAIT_EVENT_0,
        calls[writer_zero_wait]
            .result); // [確認_正常系] - calls[writer_zero_wait].result が WSA_WAIT_EVENT_0 であること。(送信側も E_s の通知を観測したことの確認)
    EXPECT_LT(
        writer_zero_wait,
        reader_enumeration); // [確認_正常系] - writer_zero_wait が reader_enumeration より小さいこと。(送信側の確認が受信側の列挙より前であることの確認)
    ASSERT_LT(
        reader_enumeration,
        writer_enumeration); // [確認_正常系] - reader_enumeration が writer_enumeration より小さいこと。(列挙がロックで直列化されたことの確認)
    ASSERT_LT(writer_enumeration, calls.size()); // [確認_正常系] - writer_enumeration が calls.size() より小さいこと。
    EXPECT_EQ(FD_READ,
              calls[reader_enumeration]
                  .network_events); // [確認_正常系] - calls[reader_enumeration].network_events が FD_READ であること。
    EXPECT_EQ(
        0,
        calls[writer_enumeration]
            .network_events); // [確認_正常系] - calls[writer_enumeration].network_events が 0 であること。(後続側の列挙が空であることの確認)
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_reader
            .load()); // [確認_正常系] - 受信側の cplat_socket_watch_wait_readable_multi() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1U, ready[0]); // [確認_正常系] - ready[0] が 1 であること。
    EXPECT_EQ(
        1U,
        count_event_calls(
            calls, simulated_call_kind::RESET_EVENT, watch_role::WRITER, events.write,
            from)); // [確認_正常系] - 送信側が E_w をリセットした回数を数える count_event_calls() の戻り値が 1 であること。(空の列挙の後に自分のイベントをリセットしたことの確認)
    EXPECT_EQ(
        0U,
        count_event_calls(
            calls, simulated_call_kind::RESET_EVENT, watch_role::WRITER,
            events
                .read)); // [確認_正常系] - 送信側が E_r をリセットした回数を数える count_event_calls() の戻り値が 0 であること。(相手のイベントをリセットしないことの確認)
    EXPECT_EQ(
        1U,
        count_calls(
            calls, simulated_call_kind::BLOCKING_WAIT, watch_role::WRITER,
            from)); // [確認_正常系] - 通知の後の送信側の待機を数える count_calls() の戻り値が 1 であること。(空回りせずに待機を続けたことの確認)
    EXPECT_TRUE(
        writer_waits_own_events); // [確認_正常系] - writer_waits_own_events が true であること。(送信側が解除、E_w、E_s を待ち続けたことの確認)
    EXPECT_TRUE(
        is_blocked(calls, watch_role::WRITER)); // [確認_正常系] - 送信側の is_blocked() の戻り値が true であること。
    EXPECT_EQ(
        0U,
        count_guard_timeouts(
            calls)); // [確認_正常系] - count_guard_timeouts() の戻り値が 0 であること。(起床を失って時間切れになった待機がないことの確認)
    EXPECT_EQ(CPLAT_OK,
              actual_ret_signal); // [確認_正常系] - cplat_socket_wait_cancel_signal() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_CANCELED,
        actual_ret_writer
            .load()); // [確認_正常系] - 送信側の cplat_socket_watch_wait_writable() の戻り値が CPLAT_ERR_CANCELED であること。

    // Cleanup
    cplat_socket_watch_detach(watch);
    cplat_socket_wait_cancel_dispose(writer_cancel);
    cplat_socket_wait_cancel_dispose(reader_cancel);
}
// [サブ手順参照 名前=socketTest.TearDown]

// FD_READ だけの通知で、送信の待機がタイムアウト 0 の即時起床を反復しないことの確認
TEST_F(socketTest, watch_write_wait_ignores_read_only_notification)
{
    // Arrange
    RealWinsockSession winsock_session;
    ASSERT_TRUE(winsock_session.started()); // [状態確認] - winsock_session.started() の戻り値が true であること。
    winsock_event_simulator simulator(mock_winsock_,
                                      true); // [状態] - Winsock のイベントを模擬し、待機は指定の時間で時間切れにする。
    cplat_socket_watch *watch = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_socket_watch_attach(
                            kSocket, &watch,
                            nullptr)); // [状態確認] - cplat_socket_watch_attach() の戻り値が CPLAT_OK であること。
    const watch_events events = simulator.latest_watch_events(); // [状態] - 登録の E_s、E_r、E_w を取り出す。
    const char payload[] = "x";
    size_t sent = 0U;

    // Pre-Assert
    EXPECT_CALL(mock_winsock_, send(_, _, _, (SOCKET)kSocket, _, 1, 0))
        .WillOnce(
            Return(SOCKET_ERROR)); // [Pre-Assert確認_正常系] - send() が kSocket と 1 を指定して 1 回呼び出されること。
                                   // [Pre-Assert手順] - send() から SOCKET_ERROR を返す。

    // Act
    const int actual_ret_send = cplat_socket_watch_send(
        watch, payload, 1U, &sent,
        nullptr); // [手順] - WSAEWOULDBLOCK で失敗する送信を指定して cplat_socket_watch_send() を呼び出す。
    const size_t from = simulator.mark(); // [手順] - 通知の前の記録の位置を控える。
    simulator.inject(FD_READ);            // [手順] - E_s へ FD_READ を通知する。
    const int actual_ret_wait = cplat_socket_watch_wait_writable(
        watch, nullptr, 120, nullptr); // [手順] - 120 [ms] を指定して cplat_socket_watch_wait_writable() を呼び出す。
    const simulated_calls calls = simulator.snapshot(); // [手順] - 呼び出しの記録を取り出す。

    // Assert
    size_t blocking_waits = 0U;
    bool waits_own_events = true;
    bool waits_with_positive_timeout = true;
    bool waits_until_timeout = true;
    for (size_t index = from; index < calls.size(); ++index)
    {
        const simulated_call &call = calls[index];
        if (call.kind != simulated_call_kind::BLOCKING_WAIT)
        {
            continue;
        }
        ++blocking_waits;
        waits_own_events = waits_own_events && call.events.size() == 2U && call.events[0] == events.write &&
                           call.events[1] == events.shared;
        waits_with_positive_timeout = waits_with_positive_timeout && call.timeout_ms > 0U && call.timeout_ms <= 50U;
        waits_until_timeout = waits_until_timeout && call.timed_out;
    }
    const size_t zero_waits = count_calls(calls, simulated_call_kind::ZERO_WAIT, watch_role::MAIN, from);
    EXPECT_EQ(
        CPLAT_ERR_WOULD_BLOCK,
        actual_ret_send); // [確認_正常系] - cplat_socket_watch_send() の戻り値が CPLAT_ERR_WOULD_BLOCK であること。
    EXPECT_EQ(
        CPLAT_ERR_TIMEOUT,
        actual_ret_wait); // [確認_正常系] - cplat_socket_watch_wait_writable() の戻り値が CPLAT_ERR_TIMEOUT であること。
    EXPECT_EQ(
        1U,
        count_calls(
            calls, simulated_call_kind::ENUMERATE, watch_role::MAIN,
            from)); // [確認_正常系] - 列挙を数える count_calls() の戻り値が 1 であること。(新しい通知のない E_s を列挙し直さないことの確認)
    EXPECT_EQ(FD_READ,
              first_network_events(calls, watch_role::MAIN,
                                   from)); // [確認_正常系] - first_network_events() の戻り値が FD_READ であること。
    EXPECT_EQ(1U,
              count_event_calls(
                  calls, simulated_call_kind::SET_EVENT, watch_role::MAIN, events.read,
                  from)); // [確認_正常系] - E_r を設定した回数を数える count_event_calls() の戻り値が 1 であること。
    EXPECT_EQ(0U,
              count_event_calls(
                  calls, simulated_call_kind::SET_EVENT, watch_role::MAIN, events.write,
                  from)); // [確認_正常系] - E_w を設定した回数を数える count_event_calls() の戻り値が 0 であること。
    EXPECT_EQ(
        0U,
        count_event_calls(
            calls, simulated_call_kind::RESET_EVENT, watch_role::MAIN, events.read,
            from)); // [確認_正常系] - E_r をリセットした回数を数える count_event_calls() の戻り値が 0 であること。(相手のイベントをリセットしないことの確認)
    EXPECT_EQ(
        zero_waits,
        count_event_calls(
            calls, simulated_call_kind::RESET_EVENT, watch_role::MAIN, events.write,
            from)); // [確認_正常系] - E_w をリセットした回数を数える count_event_calls() の戻り値が zero_waits と一致すること。(各反復の条件確認で自分のイベントだけをリセットしたことの確認)
    EXPECT_GE(blocking_waits, 1U); // [確認_正常系] - blocking_waits が 1 以上であること。
    EXPECT_LE(
        blocking_waits,
        5U); // [確認_正常系] - blocking_waits が 5 以下であること。(120 [ms] の待機を最大 50 [ms] ずつに分けた回数で、即時起床の反復がないことの確認)
    EXPECT_EQ(
        blocking_waits + 1U,
        zero_waits); // [確認_正常系] - zero_waits が blocking_waits + 1 と一致すること。(E_s の確認が反復ごとに 1 回だけであることの確認)
    EXPECT_TRUE(
        waits_own_events); // [確認_正常系] - waits_own_events が true であること。(待機の対象が E_w と E_s だけであることの確認)
    EXPECT_TRUE(
        waits_with_positive_timeout); // [確認_正常系] - waits_with_positive_timeout が true であること。(待機の時間が 0 でなく 50 [ms] 以下であることの確認)
    EXPECT_TRUE(
        waits_until_timeout); // [確認_正常系] - waits_until_timeout が true であること。(どの待機もイベントで即時に起床しなかったことの確認)

    // Cleanup
    cplat_socket_watch_detach(watch);
}
// [サブ手順参照 名前=socketTest.TearDown]

// 全方向の列挙と状態の更新が同じ登録のロックの中で行われ、各方向が相手のイベントをリセットしないことの確認
TEST_F(socketTest, watch_enumeration_and_state_update_share_watch_lock)
{
    // Arrange
    RealWinsockSession winsock_session;
    ASSERT_TRUE(winsock_session.started()); // [状態確認] - winsock_session.started() の戻り値が true であること。
    winsock_event_simulator simulator(mock_winsock_,
                                      false); // [状態] - Winsock のイベントを模擬し、障壁で待機を止められるようにする。
    cplat_socket_wait_cancel *reader_cancel = nullptr;
    cplat_socket_wait_cancel *writer_cancel = nullptr;
    ASSERT_EQ(CPLAT_OK,
              cplat_socket_wait_cancel_create(
                  &reader_cancel,
                  nullptr)); // [状態確認] - 受信側の cplat_socket_wait_cancel_create() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_socket_wait_cancel_create(
                  &writer_cancel,
                  nullptr)); // [状態確認] - 送信側の cplat_socket_wait_cancel_create() の戻り値が CPLAT_OK であること。
    cplat_socket_watch *watch = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_socket_watch_attach(
                            kSocket, &watch,
                            nullptr)); // [状態確認] - cplat_socket_watch_attach() の戻り値が CPLAT_OK であること。
    const watch_events events = simulator.latest_watch_events(); // [状態] - 登録の E_s、E_r、E_w を取り出す。
    cplat_socket_watch *watches[] = {watch};
    unsigned char ready[] = {0U};
    const char payload[] = "x";
    size_t sent = 0U;
    size_t probe_sent = 0U;
    std::atomic<int> actual_ret_reader{CPLAT_ERR_UNKNOWN};
    std::atomic<int> actual_ret_writer{CPLAT_ERR_UNKNOWN};
    std::atomic<int> actual_ret_probe{CPLAT_ERR_UNKNOWN};
    watch_thread_group threads(simulator, {reader_cancel, writer_cancel});

    // Pre-Assert
    EXPECT_CALL(mock_winsock_, send(_, _, _, (SOCKET)kSocket, _, 1, 0))
        .WillOnce(Return(SOCKET_ERROR))
        .WillOnce(
            [&](const char *, const int, const char *, SOCKET, const char *, int, int)
            {
                simulator.record(simulated_call_kind::SEND);
                return 1;
            }); // [Pre-Assert確認_正常系] - send() が kSocket と 1 を指定して 2 回呼び出されること。
                // [Pre-Assert手順] - send() から 1 回目は SOCKET_ERROR、2 回目は呼び出しを記録して 1 を返す。

    // Act
    const int actual_ret_send = cplat_socket_watch_send(
        watch, payload, 1U, &sent,
        nullptr); // [手順] - WSAEWOULDBLOCK で失敗する送信を指定して cplat_socket_watch_send() を呼び出す。
    threads.start(watch_role::READER,
                  [&]()
                  {
                      actual_ret_reader.store(
                          cplat_socket_watch_wait_readable_multi(watches, 1U, reader_cancel, 10000, ready, nullptr));
                  }); // [手順] - 受信側のスレッドで cplat_socket_watch_wait_readable_multi() を呼び出す。
    threads.start(watch_role::WRITER,
                  [&]()
                  {
                      actual_ret_writer.store(cplat_socket_watch_wait_writable(watch, writer_cancel, 10000, nullptr));
                  }); // [手順] - 送信側のスレッドで cplat_socket_watch_wait_writable() を呼び出す。
    ASSERT_TRUE(simulator.wait_until(
        [](const simulated_calls &calls)
        {
            return is_blocked(calls, watch_role::READER) && is_blocked(calls, watch_role::WRITER);
        })); // [状態確認] - 両方向の待機を待つ simulator.wait_until() の戻り値が true であること。
    const size_t from = simulator.mark();                         // [手順] - 通知の前の記録の位置を控える。
    simulator.arm(simulated_gate::ZERO_WAIT, watch_role::WRITER); // [手順] - 送信側の E_s の確認に障壁を置く。
    simulator.arm(simulated_gate::ENUMERATE, watch_role::READER); // [手順] - 受信側の列挙に障壁を置く。
    simulator.inject(FD_READ | FD_WRITE);                         // [手順] - E_s へ FD_READ と FD_WRITE を通知する。
    ASSERT_TRUE(simulator.wait_arrived(
        simulated_gate::ENUMERATE,
        watch_role::
            READER)); // [状態確認] - 受信側の列挙の障壁を待つ simulator.wait_arrived() の戻り値が true であること。
    ASSERT_TRUE(simulator.wait_arrived(
        simulated_gate::ZERO_WAIT,
        watch_role::
            WRITER)); // [状態確認] - 送信側の E_s の確認の障壁を待つ simulator.wait_arrived() の戻り値が true であること。
    threads.start(
        watch_role::PROBE,
        [&]()
        {
            actual_ret_probe.store(cplat_socket_watch_send(watch, payload, 1U, &probe_sent, nullptr));
        }); // [手順] - 受信側が列挙の中で止まっている間に、別のスレッドで cplat_socket_watch_send() を呼び出す。
    std::this_thread::sleep_for(
        std::chrono::milliseconds(100)); // [手順] - 送信の関数が登録のロックへ達するまで、100 [ms] 待つ。
    const simulated_calls calls_during_enumeration =
        simulator.snapshot();                                         // [手順] - 列挙の途中の呼び出しの記録を取り出す。
    simulator.release(simulated_gate::ENUMERATE, watch_role::READER); // [手順] - 受信側の列挙を進める。
    ASSERT_TRUE(simulator.wait_until(
        [](const simulated_calls &calls)
        {
            return has_finished(calls, watch_role::READER) && has_finished(calls, watch_role::PROBE);
        })); // [状態確認] - 受信側と送信の関数の終了を待つ simulator.wait_until() の戻り値が true であること。
    simulator.release(simulated_gate::ZERO_WAIT, watch_role::WRITER); // [手順] - 送信側に E_s を確認させる。
    ASSERT_TRUE(simulator.wait_until(
        [](const simulated_calls &calls)
        {
            return has_finished(calls, watch_role::WRITER);
        }));            // [状態確認] - 送信側の終了を待つ simulator.wait_until() の戻り値が true であること。
    threads.join_all(); // [手順] - すべてのスレッドの終了を待つ。
    const simulated_calls calls = simulator.snapshot(); // [手順] - 呼び出しの記録を取り出す。

    // Assert
    const size_t reader_enumeration = find_call(calls, simulated_call_kind::ENUMERATE, watch_role::READER, from);
    const size_t read_set =
        find_event_call(calls, simulated_call_kind::SET_EVENT, watch_role::READER, events.read, from);
    const size_t write_set =
        find_event_call(calls, simulated_call_kind::SET_EVENT, watch_role::READER, events.write, from);
    const size_t probe_send = find_call(calls, simulated_call_kind::SEND, watch_role::PROBE, from);
    EXPECT_EQ(
        CPLAT_ERR_WOULD_BLOCK,
        actual_ret_send); // [確認_正常系] - 1 回目の cplat_socket_watch_send() の戻り値が CPLAT_ERR_WOULD_BLOCK であること。
    EXPECT_EQ(
        0U,
        count_calls(
            calls_during_enumeration, simulated_call_kind::SEND, watch_role::PROBE,
            from)); // [確認_正常系] - 列挙の途中の送信を数える count_calls() の戻り値が 0 であること。(列挙の間は登録のロックで送信の関数が待たされたことの確認)
    ASSERT_LT(reader_enumeration, calls.size()); // [確認_正常系] - reader_enumeration が calls.size() より小さいこと。
    EXPECT_EQ(
        FD_READ | FD_WRITE,
        calls[reader_enumeration]
            .network_events); // [確認_正常系] - calls[reader_enumeration].network_events が FD_READ | FD_WRITE であること。(全方向の通知を 1 回で列挙したことの確認)
    EXPECT_LT(reader_enumeration, read_set);  // [確認_正常系] - reader_enumeration が read_set より小さいこと。
    EXPECT_LT(reader_enumeration, write_set); // [確認_正常系] - reader_enumeration が write_set より小さいこと。
    EXPECT_LT(
        read_set,
        probe_send); // [確認_正常系] - read_set が probe_send より小さいこと。(E_r の設定までをロックの中で終えたことの確認)
    EXPECT_LT(
        write_set,
        probe_send); // [確認_正常系] - write_set が probe_send より小さいこと。(E_w の設定までをロックの中で終えたことの確認)
    EXPECT_LT(probe_send, calls.size()); // [確認_正常系] - probe_send が calls.size() より小さいこと。
    EXPECT_EQ(CPLAT_OK,
              actual_ret_probe
                  .load()); // [確認_正常系] - 別のスレッドの cplat_socket_watch_send() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1U, probe_sent); // [確認_正常系] - probe_sent が 1 であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_reader
            .load()); // [確認_正常系] - 受信側の cplat_socket_watch_wait_readable_multi() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1U, ready[0]); // [確認_正常系] - ready[0] が 1 であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_writer
            .load()); // [確認_正常系] - 送信側の cplat_socket_watch_wait_writable() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        0U,
        count_calls(
            calls, simulated_call_kind::ENUMERATE, watch_role::WRITER,
            from)); // [確認_正常系] - 送信側の列挙を数える count_calls() の戻り値が 0 であること。(受信側の列挙で保存した送信可能で戻ったことの確認)
    EXPECT_GE(
        count_event_calls(calls, simulated_call_kind::RESET_EVENT, watch_role::READER, events.read),
        1U); // [確認_正常系] - 受信側が E_r をリセットした回数を数える count_event_calls() の戻り値が 1 以上であること。
    EXPECT_GE(
        count_event_calls(calls, simulated_call_kind::RESET_EVENT, watch_role::WRITER, events.write),
        1U); // [確認_正常系] - 送信側が E_w をリセットした回数を数える count_event_calls() の戻り値が 1 以上であること。
    EXPECT_EQ(
        0U,
        count_event_calls(
            calls, simulated_call_kind::RESET_EVENT, watch_role::READER,
            events
                .write)); // [確認_正常系] - 受信側が E_w をリセットした回数を数える count_event_calls() の戻り値が 0 であること。(相手のイベントをリセットしないことの確認)
    EXPECT_EQ(
        0U,
        count_event_calls(
            calls, simulated_call_kind::RESET_EVENT, watch_role::WRITER,
            events
                .read)); // [確認_正常系] - 送信側が E_r をリセットした回数を数える count_event_calls() の戻り値が 0 であること。(相手のイベントをリセットしないことの確認)
    EXPECT_EQ(
        0U,
        count_guard_timeouts(
            calls)); // [確認_正常系] - count_guard_timeouts() の戻り値が 0 であること。(起床を失って時間切れになった待機がないことの確認)

    // Cleanup
    cplat_socket_watch_detach(watch);
    cplat_socket_wait_cancel_dispose(writer_cancel);
    cplat_socket_wait_cancel_dispose(reader_cancel);
}
// [サブ手順参照 名前=socketTest.TearDown]

// 両方向の待機の開始後に FD_CLOSE だけを通知し、先行側が取得した後に後続側を再開しても、両方が終了を検出することの確認
TEST_F(socketTest, watch_close_notification_reaches_both_directions)
{
    // Arrange
    RealWinsockSession winsock_session;
    ASSERT_TRUE(winsock_session.started()); // [状態確認] - winsock_session.started() の戻り値が true であること。
    winsock_event_simulator simulator(mock_winsock_,
                                      false); // [状態] - Winsock のイベントを模擬し、障壁で待機を止められるようにする。
    cplat_socket_wait_cancel *reader_cancel = nullptr;
    cplat_socket_wait_cancel *writer_cancel = nullptr;
    ASSERT_EQ(CPLAT_OK,
              cplat_socket_wait_cancel_create(
                  &reader_cancel,
                  nullptr)); // [状態確認] - 受信側の cplat_socket_wait_cancel_create() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_socket_wait_cancel_create(
                  &writer_cancel,
                  nullptr)); // [状態確認] - 送信側の cplat_socket_wait_cancel_create() の戻り値が CPLAT_OK であること。
    cplat_socket_watch *watch = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_socket_watch_attach(
                            kSocket, &watch,
                            nullptr)); // [状態確認] - cplat_socket_watch_attach() の戻り値が CPLAT_OK であること。
    const watch_events events = simulator.latest_watch_events(); // [状態] - 登録の E_s、E_r、E_w を取り出す。
    cplat_socket_watch *watches[] = {watch};
    unsigned char ready[] = {0U};
    unsigned char ready_again[] = {0U};
    const char payload[] = "x";
    size_t sent = 0U;
    std::atomic<int> actual_ret_reader{CPLAT_ERR_UNKNOWN};
    std::atomic<int> actual_ret_writer{CPLAT_ERR_UNKNOWN};
    watch_thread_group threads(simulator, {reader_cancel, writer_cancel});

    // Pre-Assert
    EXPECT_CALL(mock_winsock_, send(_, _, _, (SOCKET)kSocket, _, 1, 0))
        .WillOnce(
            Return(SOCKET_ERROR)); // [Pre-Assert確認_正常系] - send() が kSocket と 1 を指定して 1 回呼び出されること。
                                   // [Pre-Assert手順] - send() から SOCKET_ERROR を返す。

    // Act
    const int actual_ret_send = cplat_socket_watch_send(
        watch, payload, 1U, &sent,
        nullptr); // [手順] - WSAEWOULDBLOCK で失敗する送信を指定して cplat_socket_watch_send() を呼び出す。
    threads.start(watch_role::READER,
                  [&]()
                  {
                      actual_ret_reader.store(
                          cplat_socket_watch_wait_readable_multi(watches, 1U, reader_cancel, 10000, ready, nullptr));
                  }); // [手順] - 受信側のスレッドで cplat_socket_watch_wait_readable_multi() を呼び出す。
    threads.start(watch_role::WRITER,
                  [&]()
                  {
                      actual_ret_writer.store(cplat_socket_watch_wait_writable(watch, writer_cancel, 10000, nullptr));
                  }); // [手順] - 送信側のスレッドで cplat_socket_watch_wait_writable() を呼び出す。
    ASSERT_TRUE(simulator.wait_until(
        [](const simulated_calls &calls)
        {
            return is_blocked(calls, watch_role::READER) && is_blocked(calls, watch_role::WRITER);
        })); // [状態確認] - 両方向の待機を待つ simulator.wait_until() の戻り値が true であること。
    const size_t from = simulator.mark();                         // [手順] - 通知の前の記録の位置を控える。
    simulator.arm(simulated_gate::ZERO_WAIT, watch_role::WRITER); // [手順] - 送信側の E_s の確認に障壁を置く。
    simulator.inject(FD_CLOSE);                                   // [手順] - E_s へ FD_CLOSE だけを通知する。
    ASSERT_TRUE(simulator.wait_arrived(
        simulated_gate::ZERO_WAIT,
        watch_role::
            WRITER)); // [状態確認] - 送信側の E_s の確認の障壁を待つ simulator.wait_arrived() の戻り値が true であること。
    ASSERT_TRUE(simulator.wait_until(
        [from](const simulated_calls &calls)
        {
            return has_finished(calls, watch_role::READER, from);
        })); // [状態確認] - 先行側の受信側の終了を待つ simulator.wait_until() の戻り値が true であること。
    simulator.release(simulated_gate::ZERO_WAIT, watch_role::WRITER); // [手順] - 後続側の送信側を再開させる。
    ASSERT_TRUE(simulator.wait_until(
        [from](const simulated_calls &calls)
        {
            return has_finished(calls, watch_role::WRITER, from);
        }));            // [状態確認] - 後続側の送信側の終了を待つ simulator.wait_until() の戻り値が true であること。
    threads.join_all(); // [手順] - 両方のスレッドの終了を待つ。
    const simulated_calls calls = simulator.snapshot(); // [手順] - 呼び出しの記録を取り出す。

    // Assert
    const size_t writer_zero_wait = find_call(calls, simulated_call_kind::ZERO_WAIT, watch_role::WRITER, from);
    EXPECT_EQ(
        CPLAT_ERR_WOULD_BLOCK,
        actual_ret_send); // [確認_正常系] - cplat_socket_watch_send() の戻り値が CPLAT_ERR_WOULD_BLOCK であること。
    EXPECT_EQ(FD_CLOSE, first_network_events(
                            calls, watch_role::READER,
                            from)); // [確認_正常系] - 先行側の first_network_events() の戻り値が FD_CLOSE であること。
    EXPECT_EQ(
        1U,
        count_event_calls(
            calls, simulated_call_kind::SET_EVENT, watch_role::READER, events.read,
            from)); // [確認_正常系] - 先行側が E_r を設定した回数を数える count_event_calls() の戻り値が 1 であること。
    EXPECT_EQ(
        1U,
        count_event_calls(
            calls, simulated_call_kind::SET_EVENT, watch_role::READER, events.write,
            from)); // [確認_正常系] - 先行側が E_w を設定した回数を数える count_event_calls() の戻り値が 1 であること。(終了を両方向へ通知したことの確認)
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_reader
            .load()); // [確認_正常系] - 先行側の cplat_socket_watch_wait_readable_multi() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1U, ready[0]);                   // [確認_正常系] - ready[0] が 1 であること。
    ASSERT_LT(writer_zero_wait, calls.size()); // [確認_正常系] - writer_zero_wait が calls.size() より小さいこと。
    EXPECT_EQ(
        static_cast<DWORD>(WSA_WAIT_TIMEOUT),
        calls[writer_zero_wait]
            .result); // [確認_正常系] - calls[writer_zero_wait].result が WSA_WAIT_TIMEOUT であること。(後続側の再開時には E_s の通知が取得済みであることの確認)
    EXPECT_EQ(0U, count_calls(calls, simulated_call_kind::ENUMERATE, watch_role::WRITER,
                              from)); // [確認_正常系] - 後続側の列挙を数える count_calls() の戻り値が 0 であること。
    EXPECT_EQ(
        0U,
        count_calls(
            calls, simulated_call_kind::BLOCKING_WAIT, watch_role::WRITER,
            from)); // [確認_正常系] - 再開後の後続側の待機を数える count_calls() の戻り値が 0 であること。(保存した終了の状態で戻ったことの確認)
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_writer
            .load()); // [確認_正常系] - 後続側の cplat_socket_watch_wait_writable() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        0U,
        count_guard_timeouts(
            calls)); // [確認_正常系] - count_guard_timeouts() の戻り値が 0 であること。(起床を失って時間切れになった待機がないことの確認)

    // Act_2
    const size_t from_again = simulator.mark(); // [手順] - 再度の待機の前の記録の位置を控える。
    const int actual_ret_read_again = cplat_socket_watch_wait_readable_multi(
        watches, 1U, nullptr, 1000, ready_again,
        nullptr); // [手順] - 終了の取得後に、もう一度 cplat_socket_watch_wait_readable_multi() を呼び出す。
    const int actual_ret_write_again = cplat_socket_watch_wait_writable(
        watch, nullptr, 1000,
        nullptr); // [手順] - 終了の取得後に、もう一度 cplat_socket_watch_wait_writable() を呼び出す。
    const simulated_calls calls_again = simulator.snapshot(); // [手順] - 呼び出しの記録を取り出す。

    // Assert_2
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_read_again); // [確認_正常系] - 再度の cplat_socket_watch_wait_readable_multi() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1U, ready_again[0]); // [確認_正常系] - ready_again[0] が 1 であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_write_again); // [確認_正常系] - 再度の cplat_socket_watch_wait_writable() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        0U,
        count_calls(
            calls_again, simulated_call_kind::BLOCKING_WAIT, watch_role::MAIN,
            from_again)); // [確認_正常系] - 再度の待機の中の待機を数える count_calls() の戻り値が 0 であること。(終了の状態を片方の確認で消費しないことの確認)

    // Cleanup
    cplat_socket_watch_detach(watch);
    cplat_socket_wait_cancel_dispose(writer_cancel);
    cplat_socket_wait_cancel_dispose(reader_cancel);
}
// [サブ手順参照 名前=socketTest.TearDown]

// 受信の通知の取得後の一部の受信と、送信の WSAEWOULDBLOCK と FD_WRITE の取得の前後で、状態と世代を更新して再び待機することの確認
TEST_F(socketTest, watch_waits_rearm_after_partial_receive_and_write_generation_update)
{
    // Arrange
    RealWinsockSession winsock_session;
    ASSERT_TRUE(winsock_session.started()); // [状態確認] - winsock_session.started() の戻り値が true であること。
    winsock_event_simulator simulator(mock_winsock_,
                                      false); // [状態] - Winsock のイベントを模擬し、障壁で待機を止められるようにする。
    cplat_socket_wait_cancel *reader_cancel = nullptr;
    cplat_socket_wait_cancel *writer_cancel = nullptr;
    ASSERT_EQ(CPLAT_OK,
              cplat_socket_wait_cancel_create(
                  &reader_cancel,
                  nullptr)); // [状態確認] - 受信側の cplat_socket_wait_cancel_create() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_socket_wait_cancel_create(
                  &writer_cancel,
                  nullptr)); // [状態確認] - 送信側の cplat_socket_wait_cancel_create() の戻り値が CPLAT_OK であること。
    cplat_socket_watch *watch = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_socket_watch_attach(
                            kSocket, &watch,
                            nullptr)); // [状態確認] - cplat_socket_watch_attach() の戻り値が CPLAT_OK であること。
    const watch_events events = simulator.latest_watch_events(); // [状態] - 登録の E_s、E_r、E_w を取り出す。
    cplat_socket_watch *watches[] = {watch};
    unsigned char ready_first[] = {0U};
    unsigned char ready_second[] = {0U};
    char receive_buffer[4] = {0};
    size_t received = 0U;
    const char payload[] = "x";
    size_t sent = 0U;
    size_t from_race = 0U;
    bool writer_finished_in_send = false;
    std::atomic<int> actual_ret_reader{CPLAT_ERR_UNKNOWN};
    std::atomic<int> actual_ret_writer_first{CPLAT_ERR_UNKNOWN};
    std::atomic<int> actual_ret_writer_second{CPLAT_ERR_UNKNOWN};
    watch_thread_group threads(simulator, {reader_cancel, writer_cancel});

    // Pre-Assert
    EXPECT_CALL(mock_winsock_, recv(_, _, _, (SOCKET)kSocket, _, 4, 0))
        .WillOnce(Return(1)); // [Pre-Assert確認_正常系] - recv() が kSocket と 4 を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - recv() から 1 を返す。

    // Act
    simulator.inject(FD_READ); // [手順] - E_s へ FD_READ を通知する。
    const int actual_ret_read_first = cplat_socket_watch_wait_readable_multi(
        watches, 1U, nullptr, 1000, ready_first,
        nullptr); // [手順] - FD_READ の通知を取得する cplat_socket_watch_wait_readable_multi() を呼び出す。
    const int actual_ret_recv =
        cplat_socket_recv(kSocket, receive_buffer, sizeof(receive_buffer), &received,
                          nullptr); // [手順] - 4 [byte] のうち 1 [byte] だけを受信する cplat_socket_recv() を呼び出す。
    const size_t from_read = simulator.mark(); // [手順] - 再度の受信の待機の前の記録の位置を控える。
    threads.start(watch_role::READER,
                  [&]()
                  {
                      actual_ret_reader.store(cplat_socket_watch_wait_readable_multi(watches, 1U, reader_cancel, 10000,
                                                                                     ready_second, nullptr));
                  }); // [手順] - 受信側のスレッドで、もう一度 cplat_socket_watch_wait_readable_multi() を呼び出す。
    ASSERT_TRUE(simulator.wait_until(
        [](const simulated_calls &calls)
        {
            return is_blocked(calls, watch_role::READER);
        })); // [状態確認] - 受信側の再待機を待つ simulator.wait_until() の戻り値が true であること。
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // [手順] - 受信側が待機を続ける間、100 [ms] 待つ。
    const simulated_calls calls_before_notification =
        simulator.snapshot();  // [手順] - 次の通知の前の呼び出しの記録を取り出す。
    simulator.inject(FD_READ); // [手順] - 受信の関数の呼び出しで再開した FD_READ を E_s へ通知する。
    ASSERT_TRUE(simulator.wait_until(
        [](const simulated_calls &calls)
        {
            return has_finished(calls, watch_role::READER);
        })); // [状態確認] - 受信側の終了を待つ simulator.wait_until() の戻り値が true であること。
    const simulated_calls calls_read = simulator.snapshot(); // [手順] - 呼び出しの記録を取り出す。

    // Assert
    const size_t read_reset = find_event_call(calls_before_notification, simulated_call_kind::RESET_EVENT,
                                              watch_role::READER, events.read, from_read);
    const size_t read_wait =
        find_call(calls_before_notification, simulated_call_kind::BLOCKING_WAIT, watch_role::READER, from_read);
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_read_first); // [確認_正常系] - 1 回目の cplat_socket_watch_wait_readable_multi() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1U, ready_first[0]);        // [確認_正常系] - ready_first[0] が 1 であること。
    EXPECT_EQ(CPLAT_OK, actual_ret_recv); // [確認_正常系] - cplat_socket_recv() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1U, received);              // [確認_正常系] - received が 1 であること。
    EXPECT_LT(
        read_reset,
        read_wait); // [確認_正常系] - read_reset が read_wait より小さいこと。(取得済みの通知の後に残った E_r を待機の前にリセットしたことの確認)
    EXPECT_EQ(
        1U,
        count_calls(
            calls_before_notification, simulated_call_kind::BLOCKING_WAIT, watch_role::READER,
            from_read)); // [確認_正常系] - 次の通知の前の受信側の待機を数える count_calls() の戻り値が 1 であること。(消費した通知で空回りしないことの確認)
    EXPECT_TRUE(is_blocked(
        calls_before_notification,
        watch_role::READER)); // [確認_正常系] - 次の通知の前の受信側の is_blocked() の戻り値が true であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_reader
            .load()); // [確認_正常系] - 2 回目の cplat_socket_watch_wait_readable_multi() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1U, ready_second[0]); // [確認_正常系] - ready_second[0] が 1 であること。
    EXPECT_EQ(
        events.shared,
        first_woken_event(
            calls_read, watch_role::READER,
            from_read)); // [確認_正常系] - 受信側の first_woken_event() の戻り値が events.shared と一致すること。(再開した通知で起床したことの確認)
    EXPECT_EQ(
        1U,
        count_calls(calls_read, simulated_call_kind::ENUMERATE, watch_role::READER,
                    from_read)); // [確認_正常系] - 2 回目の受信側の列挙を数える count_calls() の戻り値が 1 であること。

    // Pre-Assert_2
    EXPECT_CALL(mock_winsock_, send(_, _, _, (SOCKET)kSocket, _, 1, 0))
        .WillOnce(Return(SOCKET_ERROR))
        .WillOnce(Return(SOCKET_ERROR))
        .WillOnce(
            [&](const char *, const int, const char *, SOCKET, const char *, int, int)
            {
                simulator.inject(FD_WRITE);
                writer_finished_in_send = simulator.wait_until(
                    [&](const simulated_calls &calls) { return has_finished(calls, watch_role::WRITER, from_race); });
                return SOCKET_ERROR;
            }); // [Pre-Assert確認_正常系] - send() が kSocket と 1 を指定して 3 回呼び出されること。
    // [Pre-Assert手順] - send() から 1 回目と 2 回目は SOCKET_ERROR を返し、3 回目は E_s へ FD_WRITE を通知して送信側の待機の終了を待ってから SOCKET_ERROR を返す。

    // Act_2
    const int actual_ret_send_first = cplat_socket_watch_send(
        watch, payload, 1U, &sent,
        nullptr); // [手順] - WSAEWOULDBLOCK で失敗する送信を指定して cplat_socket_watch_send() を呼び出す。
    const size_t from_write = simulator.mark(); // [手順] - 送信の待機の前の記録の位置を控える。
    threads.start(watch_role::WRITER,
                  [&]()
                  {
                      actual_ret_writer_first.store(
                          cplat_socket_watch_wait_writable(watch, writer_cancel, 10000, nullptr));
                  }); // [手順] - 送信側のスレッドで cplat_socket_watch_wait_writable() を呼び出す。
    ASSERT_TRUE(simulator.wait_until(
        [](const simulated_calls &calls)
        {
            return is_blocked(calls, watch_role::WRITER);
        }));                    // [状態確認] - 送信側の待機を待つ simulator.wait_until() の戻り値が true であること。
    simulator.inject(FD_WRITE); // [手順] - WSAEWOULDBLOCK の後の FD_WRITE を E_s へ通知する。
    ASSERT_TRUE(simulator.wait_until(
        [from_write](const simulated_calls &calls)
        {
            return has_finished(calls, watch_role::WRITER, from_write);
        })); // [状態確認] - 送信側の終了を待つ simulator.wait_until() の戻り値が true であること。
    const simulated_calls calls_write = simulator.snapshot(); // [手順] - 呼び出しの記録を取り出す。

    // Assert_2
    EXPECT_EQ(
        CPLAT_ERR_WOULD_BLOCK,
        actual_ret_send_first); // [確認_正常系] - 1 回目の cplat_socket_watch_send() の戻り値が CPLAT_ERR_WOULD_BLOCK であること。
    EXPECT_EQ(
        1U,
        count_calls(
            calls_write, simulated_call_kind::BLOCKING_WAIT, watch_role::WRITER,
            from_write)); // [確認_正常系] - 送信側の待機を数える count_calls() の戻り値が 1 であること。(WSAEWOULDBLOCK の後は送信可能を解除して待機したことの確認)
    EXPECT_EQ(FD_WRITE,
              first_network_events(
                  calls_write, watch_role::WRITER,
                  from_write)); // [確認_正常系] - 送信側の first_network_events() の戻り値が FD_WRITE であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_writer_first
            .load()); // [確認_正常系] - 1 回目の送信側の cplat_socket_watch_wait_writable() の戻り値が CPLAT_OK であること。

    // Act_3
    const size_t from_rewait = simulator.mark(); // [手順] - 再度の送信の前の記録の位置を控える。
    const int actual_ret_send_second = cplat_socket_watch_send(
        watch, payload, 1U, &sent,
        nullptr); // [手順] - FD_WRITE の取得の後に、WSAEWOULDBLOCK で失敗する cplat_socket_watch_send() を呼び出す。
    threads.start(watch_role::WRITER,
                  [&]()
                  {
                      actual_ret_writer_second.store(
                          cplat_socket_watch_wait_writable(watch, writer_cancel, 10000, nullptr));
                  }); // [手順] - 送信側のスレッドで、もう一度 cplat_socket_watch_wait_writable() を呼び出す。
    ASSERT_TRUE(simulator.wait_until(
        [from_rewait](const simulated_calls &calls)
        {
            return find_call(calls, simulated_call_kind::BLOCKING_WAIT, watch_role::WRITER, from_rewait) <
                       calls.size() &&
                   is_blocked(calls, watch_role::WRITER);
        })); // [状態確認] - 送信側の再待機を待つ simulator.wait_until() の戻り値が true であること。
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // [手順] - 送信側が待機を続ける間、100 [ms] 待つ。
    const simulated_calls calls_rewait = simulator.snapshot();   // [手順] - 再待機の間の呼び出しの記録を取り出す。
    from_race = simulator.mark(); // [手順] - 送信の途中の FD_WRITE の前の記録の位置を控える。
    const int actual_ret_send_third = cplat_socket_watch_send(
        watch, payload, 1U, &sent,
        nullptr); // [手順] - 送信の途中で FD_WRITE が取得される cplat_socket_watch_send() を呼び出す。
    const int actual_ret_write_after_race = cplat_socket_watch_wait_writable(
        watch, nullptr, 1000,
        nullptr); // [手順] - 送信の途中で FD_WRITE が取得された後に cplat_socket_watch_wait_writable() を呼び出す。
    const simulated_calls calls_race = simulator.snapshot(); // [手順] - 呼び出しの記録を取り出す。
    threads.join_all();                                      // [手順] - すべてのスレッドの終了を待つ。

    // Assert_3
    EXPECT_EQ(
        CPLAT_ERR_WOULD_BLOCK,
        actual_ret_send_second); // [確認_正常系] - 2 回目の cplat_socket_watch_send() の戻り値が CPLAT_ERR_WOULD_BLOCK であること。
    EXPECT_EQ(
        1U,
        count_event_calls(
            calls_rewait, simulated_call_kind::RESET_EVENT, watch_role::MAIN, events.write,
            from_rewait)); // [確認_正常系] - 2 回目の送信で E_w をリセットした回数を数える count_event_calls() の戻り値が 1 であること。(世代が変わらない WSAEWOULDBLOCK で送信可能を解除したことの確認)
    EXPECT_EQ(
        1U,
        count_calls(
            calls_rewait, simulated_call_kind::BLOCKING_WAIT, watch_role::WRITER,
            from_rewait)); // [確認_正常系] - 再待機の間の送信側の待機を数える count_calls() の戻り値が 1 であること。(古い FD_WRITE の後でも再び待機したことの確認)
    EXPECT_TRUE(is_blocked(
        calls_rewait,
        watch_role::WRITER)); // [確認_正常系] - 再待機の間の送信側の is_blocked() の戻り値が true であること。
    EXPECT_EQ(
        CPLAT_ERR_WOULD_BLOCK,
        actual_ret_send_third); // [確認_正常系] - 3 回目の cplat_socket_watch_send() の戻り値が CPLAT_ERR_WOULD_BLOCK であること。
    EXPECT_TRUE(
        writer_finished_in_send); // [確認_正常系] - writer_finished_in_send が true であること。(送信の途中で送信側が FD_WRITE を取得したことの確認)
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_writer_second
            .load()); // [確認_正常系] - 2 回目の送信側の cplat_socket_watch_wait_writable() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        0U,
        count_event_calls(
            calls_race, simulated_call_kind::RESET_EVENT, watch_role::MAIN, events.write,
            from_race)); // [確認_正常系] - 3 回目の送信で E_w をリセットした回数を数える count_event_calls() の戻り値が 0 であること。(世代が変わったため新しい通知を残したことの確認)
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_write_after_race); // [確認_正常系] - 3 回目の送信の後の cplat_socket_watch_wait_writable() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        0U,
        count_calls(
            calls_race, simulated_call_kind::BLOCKING_WAIT, watch_role::MAIN,
            from_race)); // [確認_正常系] - 3 回目の送信の後の待機を数える count_calls() の戻り値が 0 であること。(送信可能を保持したため待たずに戻ったことの確認)
    EXPECT_EQ(
        0U,
        count_guard_timeouts(
            calls_race)); // [確認_正常系] - count_guard_timeouts() の戻り値が 0 であること。(起床を失って時間切れになった待機がないことの確認)

    // Cleanup
    cplat_socket_watch_detach(watch);
    cplat_socket_wait_cancel_dispose(writer_cancel);
    cplat_socket_wait_cancel_dispose(reader_cancel);
}
// [サブ手順参照 名前=socketTest.TearDown]
#endif /* PLATFORM_WINDOWS */

// 登録済み UDP ソケットの送信待ちを結果コードで区別することの確認
TEST_F(socketTest, watch_sendto_returns_would_block)
{
    // Arrange
    cplat_socket_watch *watch = nullptr;
    cplat_error detail = {};
    const char payload[] = "x";
    size_t sent = 1U;

#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_fcntl_, fcntl(_, _, _, (int)kSocket, F_GETFL, (intptr_t)0)).WillOnce(Return(0));
    EXPECT_CALL(mock_fcntl_, fcntl(_, _, _, (int)kSocket, F_SETFL, (intptr_t)O_NONBLOCK)).WillOnce(Return(0));
    EXPECT_CALL(mock_sys_socket_, sendto(_, _, _, (int)kSocket, _, 1U, MSG_NOSIGNAL, _, _))
        .WillOnce(DoAll(Assign(&errno, EAGAIN), Return((ssize_t)-1)));
#elif defined(PLATFORM_WINDOWS)
    uintptr_t next_event_id = 0x2000U;
    EXPECT_CALL(mock_winsock_, WSACreateEvent(_, _, _))
        .Times(3)
        .WillRepeatedly([&](const char *, const int, const char *) {
            return reinterpret_cast<WSAEVENT>(next_event_id++);
        });
    EXPECT_CALL(mock_winsock_, WSAEventSelect(_, _, _, (SOCKET)kSocket, _, _)).WillOnce(Return(0));
    EXPECT_CALL(mock_winsock_, sendto(_, _, _, (SOCKET)kSocket, _, 1, 0, _, _)).WillOnce(Return(SOCKET_ERROR));
    EXPECT_CALL(mock_winsock_, WSAGetLastError(_, _, _)).WillOnce(Return(WSAEWOULDBLOCK));
    EXPECT_CALL(mock_winsock_, WSAResetEvent(_, _, _, _)).WillOnce(Return(TRUE));
#endif /* PLATFORM_ */

    // Act
    ASSERT_EQ(CPLAT_OK, cplat_socket_watch_attach(kSocket, &watch, &detail));
    const int result = cplat_socket_watch_sendto(watch, payload, 1U, &kEndpoint, &sent, &detail);

    // Assert
    EXPECT_EQ(CPLAT_ERR_WOULD_BLOCK, result);
    EXPECT_EQ(0U, sent);
    EXPECT_EQ(CPLAT_CAUSE_WOULD_BLOCK, cplat_error_get_cause(&detail));
#if defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, WSAEventSelect(_, _, _, (SOCKET)kSocket, (WSAEVENT)nullptr, 0)).WillOnce(Return(0));
    EXPECT_CALL(mock_winsock_, WSACloseEvent(_, _, _, _)).Times(3).WillRepeatedly(Return(TRUE));
#endif /* PLATFORM_WINDOWS */
    cplat_socket_watch_detach(watch);
#if defined(PLATFORM_LINUX)
    ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mock_fcntl_));
#endif /* PLATFORM_LINUX */
    ASSERT_TRUE(verifyExpectations());
// [確認_正常系] - 送信キュー満杯の OS エラーが CPLAT_ERR_WOULD_BLOCK と要因 CPLAT_CAUSE_WOULD_BLOCK になること。
}

// 受け付けたソケットがブロッキングかつ未登録であることの確認
TEST_F(socketTest, accept_from_watched_listener_returns_blocking_unregistered_socket)
{
    // Arrange
#if defined(PLATFORM_WINDOWS)
    RealWinsockSession winsock_session;
    ASSERT_TRUE(winsock_session.started());
    const cplat_socket listener = (cplat_socket)::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    const cplat_socket client = (cplat_socket)::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ASSERT_NE(INVALID_SOCKET, (SOCKET)listener);
    ASSERT_NE(INVALID_SOCKET, (SOCKET)client);
#else
    const cplat_socket listener = (cplat_socket)::socket(AF_INET, SOCK_STREAM, 0);
    const cplat_socket client = (cplat_socket)::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(CPLAT_INVALID_SOCKET, listener);
    ASSERT_NE(CPLAT_INVALID_SOCKET, client);
#endif
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
#if defined(PLATFORM_WINDOWS)
    ASSERT_EQ(0, ::bind((SOCKET)listener, (const sockaddr *)&address, (int)sizeof(address)));
#else
    ASSERT_EQ(0, ::bind((int)listener, (const sockaddr *)&address, (socklen_t)sizeof(address)));
#endif
#if defined(PLATFORM_WINDOWS)
    ASSERT_EQ(0, ::listen((SOCKET)listener, 1));
#else
    ASSERT_EQ(0, ::listen((int)listener, 1));
#endif
#if defined(PLATFORM_WINDOWS)
    int address_len = (int)sizeof(address);
    ASSERT_EQ(0, ::getsockname((SOCKET)listener, (sockaddr *)&address, &address_len));
#else
    socklen_t address_len = (socklen_t)sizeof(address);
    ASSERT_EQ(0, ::getsockname((int)listener, (sockaddr *)&address, &address_len));
#endif
    cplat_socket_watch *listener_watch = nullptr;
    cplat_socket_watch *accepted_watch = nullptr;
    cplat_error detail = {};
    ASSERT_EQ(CPLAT_OK, cplat_socket_watch_attach(listener, &listener_watch, &detail));
    cplat_socket no_connection = (cplat_socket)7;
    const int no_connection_result = cplat_socket_accept(listener, nullptr, &no_connection, &detail);
    EXPECT_EQ(CPLAT_ERR_WOULD_BLOCK, no_connection_result);
    EXPECT_EQ(CPLAT_INVALID_SOCKET, no_connection);
    EXPECT_EQ(CPLAT_CAUSE_WOULD_BLOCK, cplat_error_get_cause(&detail));
#if defined(PLATFORM_WINDOWS)
    ASSERT_EQ(0, ::connect((SOCKET)client, (const sockaddr *)&address, (int)sizeof(address)));
#else
    ASSERT_EQ(0, ::connect((int)client, (const sockaddr *)&address, (socklen_t)sizeof(address)));
#endif
    cplat_socket_watch *watches[] = {listener_watch};
    unsigned char ready[] = {0U};
    ASSERT_EQ(CPLAT_OK, cplat_socket_watch_wait_readable_multi(watches, 1U, nullptr, 1000, ready, &detail));
    ASSERT_EQ(1U, ready[0]);

#if defined(PLATFORM_WINDOWS)
    {
        testing::InSequence accept_normalization_sequence;
        EXPECT_CALL(mock_winsock_, WSAEventSelect(_, _, _, _, (WSAEVENT)nullptr, 0))
            .WillOnce([](const char *file, const int line, const char *func, SOCKET accepted, WSAEVENT event,
                         const long network_events) {
                EXPECT_EQ(nullptr, event);
                EXPECT_EQ(0L, network_events);
                return delegate_real_WSAEventSelect(file, line, func, accepted, event, network_events);
            });
        EXPECT_CALL(mock_winsock_, ioctlsocket(_, _, _, _, FIONBIO, _))
            .WillOnce([](const char *file, const int line, const char *func, SOCKET accepted, const long command,
                         u_long *nonblocking) {
                EXPECT_EQ(FIONBIO, command);
                EXPECT_NE(nullptr, nonblocking);
                if (nonblocking == nullptr)
                {
                    return SOCKET_ERROR;
                }
                EXPECT_EQ(0UL, *nonblocking);
                return delegate_real_ioctlsocket(file, line, func, accepted, command, nonblocking);
            });
    }
#endif

    // Act: 受け付けに成功し、受信データが来るまで cplat_socket_recv() が戻らないことを確認します。
    cplat_socket accepted = CPLAT_INVALID_SOCKET;
    ASSERT_EQ(CPLAT_OK, cplat_socket_accept(listener, nullptr, &accepted, &detail));
    ASSERT_NE(CPLAT_INVALID_SOCKET, accepted);
#if defined(PLATFORM_WINDOWS)
    ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mock_winsock_));
#endif
#if defined(PLATFORM_LINUX)
    const int accepted_flags = ::fcntl((int)accepted, F_GETFL);
    ASSERT_GE(accepted_flags, 0);
    EXPECT_EQ(0, accepted_flags & O_NONBLOCK);
#endif
    std::atomic<bool> receive_finished{false};
    std::atomic<int> receive_result{CPLAT_ERR_UNKNOWN};
    std::atomic<size_t> receive_count{0U};
    char receive_buffer = 0;
    std::thread receiver([&]() {
        size_t count = 0U;
        receive_result.store(cplat_socket_recv(accepted, &receive_buffer, sizeof(receive_buffer), &count, nullptr),
                             std::memory_order_release);
        receive_count.store(count, std::memory_order_release);
        receive_finished.store(true, std::memory_order_release);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    EXPECT_FALSE(receive_finished.load(std::memory_order_acquire));
    const char payload = 'W';
    bool client_is_open = true;
#if defined(PLATFORM_WINDOWS)
    const int sent_count = ::send((SOCKET)client, &payload, 1, 0);
#else
    const ssize_t sent_count = ::send((int)client, &payload, 1, 0);
#endif
    if (sent_count != 1)
    {
        /* 送信失敗でも相手のソケットを閉じて、ブロッキング受信スレッドを戻します。 */
#if defined(PLATFORM_WINDOWS)
        (void)::closesocket((SOCKET)client);
#else
        (void)::close((int)client);
#endif
        client_is_open = false;
    }
    receiver.join();

    // Assert
    EXPECT_EQ(1, sent_count);
    EXPECT_TRUE(receive_finished.load(std::memory_order_acquire));
    EXPECT_EQ(CPLAT_OK, receive_result.load(std::memory_order_acquire));
    EXPECT_EQ(1U, receive_count.load(std::memory_order_acquire));
    EXPECT_EQ(payload, receive_buffer);
    EXPECT_EQ(CPLAT_OK, cplat_socket_watch_attach(accepted, &accepted_watch, &detail));
    cplat_socket_watch *client_watch = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_socket_watch_attach(client, &client_watch, &detail));
    const char frame[] = "frame";
    char received_frame[sizeof(frame) - 1U] = {};
    size_t sent_frame_len = 0U;
    size_t received_frame_len = 0U;
    ASSERT_EQ(CPLAT_OK, cplat_socket_watch_send_all(client_watch, frame, sizeof(frame) - 1U, nullptr, 1000,
                                                   &sent_frame_len, &detail));
    EXPECT_EQ(sizeof(frame) - 1U, sent_frame_len);
    ASSERT_EQ(CPLAT_OK, cplat_socket_watch_recv_all(accepted_watch, received_frame, sizeof(received_frame), nullptr,
                                                   1000, &received_frame_len, &detail));
    EXPECT_EQ(sizeof(received_frame), received_frame_len);
    EXPECT_EQ(0, std::memcmp(frame, received_frame, sizeof(received_frame)));
    cplat_socket_watch_detach(client_watch);
    cplat_socket_watch_detach(accepted_watch);
    cplat_socket_watch_detach(listener_watch);
#if defined(PLATFORM_WINDOWS)
    EXPECT_EQ(0, ::closesocket((SOCKET)accepted));
    if (client_is_open)
    {
        EXPECT_EQ(0, ::closesocket((SOCKET)client));
    }
    EXPECT_EQ(0, ::closesocket((SOCKET)listener));
#else
    EXPECT_EQ(0, ::close((int)accepted));
    if (client_is_open)
    {
        EXPECT_EQ(0, ::close((int)client));
    }
    EXPECT_EQ(0, ::close((int)listener));
#endif
// [確認_正常系] - listener の watch 登録中に受け付けたソケットがブロッキングで、別の watch に登録できること。
}

#if defined(PLATFORM_WINDOWS)
// 受け付けたソケットの正規化失敗時に破棄して失敗を返すことの確認
TEST_F(socketTest, accept_closes_socket_when_normalization_fails)
{
    // Arrange
    cplat_socket accepted = (cplat_socket)7;
    cplat_error detail = {};

    // Pre-Assert / Act: WSAEventSelect による登録解除が失敗した場合
    {
        testing::InSequence sequence;
        EXPECT_CALL(mock_winsock_, accept(_, _, _, (SOCKET)kSocket, _, _)).WillOnce(Return((SOCKET)41));
        EXPECT_CALL(mock_winsock_, WSAEventSelect(_, _, _, (SOCKET)41, (WSAEVENT)nullptr, 0))
            .WillOnce(Return(SOCKET_ERROR));
        EXPECT_CALL(mock_winsock_, WSAGetLastError).WillOnce(Return(WSAEINVAL));
        EXPECT_CALL(mock_winsock_, closesocket(_, _, _, (SOCKET)41)).WillOnce(Return(0));
    }
    const int event_select_result = cplat_socket_accept(kSocket, nullptr, &accepted, &detail);

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, event_select_result);
    EXPECT_EQ(CPLAT_INVALID_SOCKET, accepted);
    expect_detail(detail, CPLAT_ERROR_DOMAIN_WINSOCK, CPLAT_ERR_INVALID_ARGUMENT, (unsigned long)WSAEINVAL);

    // Pre-Assert / Act: FIONBIO によるブロッキング化が失敗した場合
    ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mock_winsock_));
    accepted = (cplat_socket)7;
    {
        testing::InSequence sequence;
        EXPECT_CALL(mock_winsock_, accept(_, _, _, (SOCKET)kSocket, _, _)).WillOnce(Return((SOCKET)42));
        EXPECT_CALL(mock_winsock_, WSAEventSelect(_, _, _, (SOCKET)42, (WSAEVENT)nullptr, 0))
            .WillOnce(Return(0));
        EXPECT_CALL(mock_winsock_, ioctlsocket(_, _, _, (SOCKET)42, FIONBIO, _))
            .WillOnce([](const char *, const int, const char *, SOCKET, const long command, u_long *nonblocking) {
                EXPECT_EQ(FIONBIO, command);
                EXPECT_NE(nullptr, nonblocking);
                if (nonblocking == nullptr)
                {
                    return SOCKET_ERROR;
                }
                EXPECT_EQ(0UL, *nonblocking);
                return SOCKET_ERROR;
            });
        EXPECT_CALL(mock_winsock_, WSAGetLastError).WillOnce(Return(WSAEACCES));
        EXPECT_CALL(mock_winsock_, closesocket(_, _, _, (SOCKET)42)).WillOnce(Return(0));
    }
    const int ioctl_result = cplat_socket_accept(kSocket, nullptr, &accepted, &detail);

    // Assert
    EXPECT_EQ(CPLAT_ERR_PERMISSION_DENIED, ioctl_result);
    EXPECT_EQ(CPLAT_INVALID_SOCKET, accepted);
    expect_detail(detail, CPLAT_ERROR_DOMAIN_WINSOCK, CPLAT_ERR_PERMISSION_DENIED, (unsigned long)WSAEACCES);
// [確認_異常系] - 登録解除またはブロッキング化の失敗で accepted を閉じ、出力を無効値にすること。
}
#endif /* PLATFORM_WINDOWS */

// 登録済みのソケットで非ブロッキング接続の完了と拒否理由を取得することの確認
TEST_F(socketTest, watch_connect_reports_success_and_connection_refused)
{
    // Arrange
#if defined(PLATFORM_WINDOWS)
    RealWinsockSession winsock_session;
    ASSERT_TRUE(winsock_session.started());
    const cplat_socket listener = (cplat_socket)::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ASSERT_NE(INVALID_SOCKET, (SOCKET)listener);
#else
    const cplat_socket listener = (cplat_socket)::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(CPLAT_INVALID_SOCKET, listener);
#endif
    sockaddr_in listening_address = {};
    listening_address.sin_family = AF_INET;
    listening_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    listening_address.sin_port = 0;
#if defined(PLATFORM_WINDOWS)
    ASSERT_EQ(0, ::bind((SOCKET)listener, (const sockaddr *)&listening_address, (int)sizeof(listening_address)));
    ASSERT_EQ(0, ::listen((SOCKET)listener, 1));
    int address_len = (int)sizeof(listening_address);
    ASSERT_EQ(0, ::getsockname((SOCKET)listener, (sockaddr *)&listening_address, &address_len));
#else
    ASSERT_EQ(0, ::bind((int)listener, (const sockaddr *)&listening_address, (socklen_t)sizeof(listening_address)));
    ASSERT_EQ(0, ::listen((int)listener, 1));
    socklen_t address_len = (socklen_t)sizeof(listening_address);
    ASSERT_EQ(0, ::getsockname((int)listener, (sockaddr *)&listening_address, &address_len));
#endif
    cplat_ipv4_endpoint listening_endpoint = {cplat_ipv4_addr_loopback(), listening_address.sin_port, 0U};
#if defined(PLATFORM_WINDOWS)
    const cplat_socket client = (cplat_socket)::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ASSERT_NE(INVALID_SOCKET, (SOCKET)client);
#else
    const cplat_socket client = (cplat_socket)::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(CPLAT_INVALID_SOCKET, client);
#endif
    cplat_socket_watch *client_watch = nullptr;
    cplat_error detail = {};
    ASSERT_EQ(CPLAT_OK, cplat_socket_watch_attach(client, &client_watch, &detail));

    // Act / Assert: 待ち受け中のローカル ポートへの非ブロッキング接続が成功します。
    EXPECT_EQ(CPLAT_OK, cplat_socket_watch_connect(client_watch, &listening_endpoint, nullptr, 1000, &detail));
#if defined(PLATFORM_WINDOWS)
    const cplat_socket accepted = (cplat_socket)::accept((SOCKET)listener, nullptr, nullptr);
    ASSERT_NE(INVALID_SOCKET, (SOCKET)accepted);
#else
    const cplat_socket accepted = (cplat_socket)::accept((int)listener, nullptr, nullptr);
    ASSERT_NE(CPLAT_INVALID_SOCKET, accepted);
#endif

#if defined(PLATFORM_WINDOWS)
    // Windows の接続拒否は、FD_CONNECT の OS エラーを mock で決定的に注入します。
    cplat_socket_watch *refused_watch = nullptr;
    EXPECT_CALL(mock_winsock_, WSAEventSelect(_, _, _, (SOCKET)kSocket, _, _)).WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - WSAEventSelect() が kSocket を登録すること。
    // [Pre-Assert手順] - WSAEventSelect() から 0 を返す。
    ASSERT_EQ(CPLAT_OK, cplat_socket_watch_attach(kSocket, &refused_watch, &detail));

    {
        testing::InSequence sequence;
        EXPECT_CALL(mock_winsock_, connect(_, _, _, (SOCKET)kSocket, _, _)).WillOnce(Return(SOCKET_ERROR));
        // [Pre-Assert確認_異常系] - connect() が kSocket で失敗すること。
        // [Pre-Assert手順] - connect() から SOCKET_ERROR を返す。
        EXPECT_CALL(mock_winsock_, WSAGetLastError(_, _, _)).WillOnce(Return(WSAEWOULDBLOCK));
        // [Pre-Assert確認_正常系] - WSAGetLastError() が 1 回呼び出されること。
        // [Pre-Assert手順] - WSAGetLastError() から WSAEWOULDBLOCK を返す。
        EXPECT_CALL(mock_winsock_, WSAWaitForMultipleEvents(_, _, _, 1U, _, FALSE, 0U, FALSE))
            .WillOnce(Return(WSA_WAIT_EVENT_0));
        // [Pre-Assert確認_正常系] - 接続イベント 1 件を 0 ms で待機すること。
        // [Pre-Assert手順] - WSAWaitForMultipleEvents() から WSA_WAIT_EVENT_0 を返す。
        EXPECT_CALL(mock_winsock_, WSAEnumNetworkEvents(_, _, _, (SOCKET)kSocket, _, _))
            .WillOnce([](const char *, const int, const char *, SOCKET, WSAEVENT, LPWSANETWORKEVENTS events) {
                events->lNetworkEvents = FD_CONNECT;
                events->iErrorCode[FD_CONNECT_BIT] = WSAECONNREFUSED;
                return 0;
            });
        // [Pre-Assert確認_正常系] - WSAEnumNetworkEvents() が FD_CONNECT と WSAECONNREFUSED を渡すこと。
        // [Pre-Assert手順] - WSAEnumNetworkEvents() から FD_CONNECT と WSAECONNREFUSED を返す。
    }

    const int refused_result = cplat_socket_watch_connect(refused_watch, &kEndpoint, nullptr, 1000, &detail);
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, refused_result);
    expect_detail(detail, CPLAT_ERROR_DOMAIN_WINSOCK, CPLAT_ERR_UNKNOWN, (unsigned long)WSAECONNREFUSED);
    ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mock_winsock_));

    EXPECT_CALL(mock_winsock_, WSAEventSelect(_, _, _, (SOCKET)kSocket, (WSAEVENT)nullptr, 0)).WillOnce(Return(0));
    cplat_socket_watch_detach(refused_watch);
    ASSERT_TRUE(Mock::VerifyAndClearExpectations(&mock_winsock_));
#else
    // Linux は閉じたローカル ポートへの接続で拒否理由を実ソケットから取得します。
    const cplat_socket reservation = (cplat_socket)::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ASSERT_NE(CPLAT_INVALID_SOCKET, reservation);
    sockaddr_in refused_address = {};
    refused_address.sin_family = AF_INET;
    refused_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    refused_address.sin_port = 0;
    ASSERT_EQ(0, ::bind((int)reservation, (const sockaddr *)&refused_address, (socklen_t)sizeof(refused_address)));
    address_len = (socklen_t)sizeof(refused_address);
    ASSERT_EQ(0, ::getsockname((int)reservation, (sockaddr *)&refused_address, &address_len));
    EXPECT_EQ(0, ::close((int)reservation));
    const cplat_socket refused_client = (cplat_socket)::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_NE(CPLAT_INVALID_SOCKET, refused_client);
    cplat_ipv4_endpoint refused_endpoint = {cplat_ipv4_addr_loopback(), refused_address.sin_port, 0U};
    cplat_socket_watch *refused_watch = nullptr;
    ASSERT_EQ(CPLAT_OK, cplat_socket_watch_attach(refused_client, &refused_watch, &detail));
    const int refused_result = cplat_socket_watch_connect(refused_watch, &refused_endpoint, nullptr, 1000, &detail);
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, refused_result);
    expect_detail(detail, CPLAT_ERROR_DOMAIN_SOCKET_ERRNO, CPLAT_ERR_UNKNOWN, (unsigned long)ECONNREFUSED);

    cplat_socket_watch_detach(refused_watch);
#endif
    cplat_socket_watch_detach(client_watch);
#if defined(PLATFORM_WINDOWS)
    EXPECT_EQ(0, ::closesocket((SOCKET)accepted));
    EXPECT_EQ(0, ::closesocket((SOCKET)client));
    EXPECT_EQ(0, ::closesocket((SOCKET)listener));
#else
    EXPECT_EQ(0, ::close((int)refused_client));
    EXPECT_EQ(0, ::close((int)accepted));
    EXPECT_EQ(0, ::close((int)client));
    EXPECT_EQ(0, ::close((int)listener));
#endif
// [確認_正常系] - 登録ソケットの接続成功と拒否時の OS 詳細エラーを確認する。
}

// TCP_NODELAY の有効化、0 以外の 1 への正規化、無効化、detail_out の nullptr、引数不正、OS 失敗が処理されることの確認
TEST_F(socketTest, socket_set_nodelay_reports_enable_disable_and_failure)
{
    // Arrange
    cplat_error detail = {};

    // Pre-Assert
    /* 値 1 の成功 2 回の後に値 0、その後に値 1 の失敗が来る。後勝ちだと失敗期待が先の成功を取る。 */
    testing::InSequence sequence;
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_sys_socket_,
                setsockopt(_, _, _, (int)kSocket, IPPROTO_TCP, TCP_NODELAY, TcpNodelayValue(1), (socklen_t)sizeof(int)))
        .WillOnce(Return(0))
        .WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - setsockopt() が kSocket、IPPROTO_TCP、TCP_NODELAY、指す先が 1 であるポインター、sizeof(int) と等しい長さを指定して 2 回呼び出されること。(enable の 1 と -7 が TCP_NODELAY の 1 になることの確認)
    // [Pre-Assert手順] - setsockopt() から、1 回目は 0、2 回目は 0 を返す。
    EXPECT_CALL(mock_sys_socket_,
                setsockopt(_, _, _, (int)kSocket, IPPROTO_TCP, TCP_NODELAY, TcpNodelayValue(0), (socklen_t)sizeof(int)))
        .WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - setsockopt() が kSocket、IPPROTO_TCP、TCP_NODELAY、指す先が 0 であるポインター、sizeof(int) と等しい長さを指定して 1 回呼び出されること。(enable の 0 が TCP_NODELAY の 0 になることの確認)
    // [Pre-Assert手順] - setsockopt() から 0 を返す。
    EXPECT_CALL(mock_sys_socket_,
                setsockopt(_, _, _, (int)kSocket, IPPROTO_TCP, TCP_NODELAY, TcpNodelayValue(1), (socklen_t)sizeof(int)))
        .WillOnce(DoAll(Assign(&errno, ENOPROTOOPT), Return(-1)));
    // [Pre-Assert確認_異常系] - setsockopt() が kSocket、IPPROTO_TCP、TCP_NODELAY、指す先が 1 であるポインター、sizeof(int) と等しい長さを指定して 1 回呼び出されること。
    // [Pre-Assert手順] - setsockopt() から、errno に ENOPROTOOPT を設定して -1 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_,
                setsockopt(_, _, _, (SOCKET)kSocket, IPPROTO_TCP, TCP_NODELAY, TcpNodelayValue(1), (int)sizeof(int)))
        .WillOnce(Return(0))
        .WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - setsockopt() が kSocket、IPPROTO_TCP、TCP_NODELAY、指す先が 1 であるポインター、sizeof(int) と等しい長さを指定して 2 回呼び出されること。(enable の 1 と -7 が TCP_NODELAY の 1 になることの確認)
    // [Pre-Assert手順] - setsockopt() から、1 回目は 0、2 回目は 0 を返す。
    EXPECT_CALL(mock_winsock_,
                setsockopt(_, _, _, (SOCKET)kSocket, IPPROTO_TCP, TCP_NODELAY, TcpNodelayValue(0), (int)sizeof(int)))
        .WillOnce(Return(0));
    // [Pre-Assert確認_正常系] - setsockopt() が kSocket、IPPROTO_TCP、TCP_NODELAY、指す先が 0 であるポインター、sizeof(int) と等しい長さを指定して 1 回呼び出されること。(enable の 0 が TCP_NODELAY の 0 になることの確認)
    // [Pre-Assert手順] - setsockopt() から 0 を返す。
    EXPECT_CALL(mock_winsock_,
                setsockopt(_, _, _, (SOCKET)kSocket, IPPROTO_TCP, TCP_NODELAY, TcpNodelayValue(1), (int)sizeof(int)))
        .WillOnce(Return(SOCKET_ERROR));
    // [Pre-Assert確認_異常系] - setsockopt() が kSocket、IPPROTO_TCP、TCP_NODELAY、指す先が 1 であるポインター、sizeof(int) と等しい長さを指定して 1 回呼び出されること。
    // [Pre-Assert手順] - setsockopt() から SOCKET_ERROR を返す。
    EXPECT_CALL(mock_winsock_, WSAGetLastError).WillOnce(Return(WSAENOPROTOOPT));
    // [Pre-Assert確認_異常系] - WSAGetLastError() が 1 回呼び出されること。
    // [Pre-Assert手順] - WSAGetLastError() から WSAENOPROTOOPT を返す。
#endif /* PLATFORM_ */
    // [Pre-Assert手順] - 有効化の setsockopt() は成功を表す 0 を返す。失敗時は Linux では errno に ENOPROTOOPT を設定して -1、Windows では SOCKET_ERROR を返し、WSAGetLastError() から WSAENOPROTOOPT を返す。

    // Act
    int actual_ret_invalid = cplat_socket_set_nodelay(
        CPLAT_INVALID_SOCKET, 1,
        &detail); // [手順] - 無効なソケットと TCP_NODELAY の有効化を指定して cplat_socket_set_nodelay() を呼び出す。
    int actual_ret_enable = cplat_socket_set_nodelay(
        kSocket, 1, &detail); // [手順] - enable に 1 を指定して cplat_socket_set_nodelay() を呼び出す。
    int actual_ret_nonzero = cplat_socket_set_nodelay(
        kSocket, -7,
        nullptr); // [手順] - enable に -7、detail_out に nullptr を指定して cplat_socket_set_nodelay() を呼び出す。
    int actual_ret_disable = cplat_socket_set_nodelay(
        kSocket, 0, &detail); // [手順] - enable に 0 を指定して cplat_socket_set_nodelay() を呼び出す。
    int actual_ret_failure = cplat_socket_set_nodelay(
        kSocket, 1, &detail); // [手順] - TCP_NODELAY の設定が失敗する状態で cplat_socket_set_nodelay() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_invalid); // [確認_異常系] - 無効なソケットを指定した cplat_socket_set_nodelay() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_OK,
              actual_ret_enable); // [確認_正常系] - enable に 1 を指定した cplat_socket_set_nodelay() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_nonzero); // [確認_正常系] - enable に -7、detail_out に nullptr を指定した cplat_socket_set_nodelay() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              actual_ret_disable); // [確認_正常系] - enable に 0 を指定した cplat_socket_set_nodelay() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_failure); // [確認_異常系] - TCP_NODELAY の設定失敗時の cplat_socket_set_nodelay() の戻り値が CPLAT_ERR_UNKNOWN であること。
}
// [サブ手順参照 名前=socketTest.TearDown]

// 送受信の引数不正、成功、失敗が処理されることの確認
TEST_F(socketTest, send_and_recv_report_results)
{
    // Arrange
    char buffer[4] = {};
    size_t sent = 99U;
    size_t received = 99U;
    size_t sent_failure = 99U;
    size_t received_failure = 99U;
    cplat_error detail = {};

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_sys_socket_, send(_, _, _, (int)kSocket, _, 4U, MSG_NOSIGNAL))
        .WillOnce(Return((ssize_t)3))
        .WillOnce(DoAll(Assign(&errno, EPIPE), Return((ssize_t)-1)));
    // [Pre-Assert確認_正常系] - send() が kSocket、4U、MSG_NOSIGNAL を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - send() から、1 回目は 3、2 回目は errno に EPIPE を設定して -1 を返す。
    EXPECT_CALL(mock_sys_socket_, recv(_, _, _, (int)kSocket, _, 4U, 0))
        .WillOnce(Return((ssize_t)2))
        .WillOnce(DoAll(Assign(&errno, ECONNRESET), Return((ssize_t)-1)));
    // [Pre-Assert確認_正常系] - recv() が kSocket、4U、0 を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - recv() から、1 回目は 2、2 回目は errno に ECONNRESET を設定して -1 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, send(_, _, _, (SOCKET)kSocket, _, 4, 0))
        .WillOnce(Return(3))
        .WillOnce(Return(SOCKET_ERROR));
    // [Pre-Assert確認_正常系] - send() が kSocket、4、0 を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - send() から 1 回目は 3、2 回目は SOCKET_ERROR を返す。
    EXPECT_CALL(mock_winsock_, recv(_, _, _, (SOCKET)kSocket, _, 4, 0))
        .WillOnce(Return(2))
        .WillOnce(Return(SOCKET_ERROR));
    // [Pre-Assert確認_正常系] - recv() が kSocket、4、0 を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - recv() から 1 回目は 2、2 回目は SOCKET_ERROR を返す。
    EXPECT_CALL(mock_winsock_, WSAGetLastError).WillOnce(Return(WSAECONNRESET)).WillOnce(Return(WSAECONNRESET));
    // [Pre-Assert確認_正常系] - WSAGetLastError() が 2 回呼び出されること。
    // [Pre-Assert手順] - WSAGetLastError() から 1 回目は WSAECONNRESET、2 回目は WSAECONNRESET を返す。
#endif /* PLATFORM_ */
    // [Pre-Assert手順] - send() から 1 回目は送信バイト数 3、2 回目は失敗を返し、recv() から 1 回目は受信バイト数 2、2 回目は失敗を返し、それぞれ errno または WSAGetLastError() で失敗要因を通知する。

    // Act
    int actual_ret_invalid_send =
        cplat_socket_send(CPLAT_INVALID_SOCKET, buffer, sizeof(buffer), &sent,
                          &detail); // [手順] - 無効なソケットを指定して cplat_socket_send() を呼び出す。
    int actual_ret_invalid_send_buffer =
        cplat_socket_send(kSocket, nullptr, sizeof(buffer), &sent,
                          &detail); // [手順] - 送信バッファーに nullptr を指定して cplat_socket_send() を呼び出す。
    int actual_ret_invalid_send_output =
        cplat_socket_send(kSocket, buffer, sizeof(buffer), nullptr,
                          &detail); // [手順] - 送信バイト数出力先に nullptr を指定して cplat_socket_send() を呼び出す。
    int actual_ret_invalid_send_length =
        cplat_socket_send(kSocket, buffer, CPLAT_SOCKET_MAX_TRANSFER + (size_t)1U, &sent,
                          &detail); // [手順] - 最大転送量を超える送信長を指定して cplat_socket_send() を呼び出す。
    int actual_ret_invalid_recv_socket =
        cplat_socket_recv(CPLAT_INVALID_SOCKET, buffer, sizeof(buffer), &received,
                          &detail); // [手順] - 無効なソケットを指定して cplat_socket_recv() を呼び出す。
    int actual_ret_invalid_recv =
        cplat_socket_recv(kSocket, nullptr, sizeof(buffer), &received,
                          &detail); // [手順] - 受信バッファーに nullptr を指定して cplat_socket_recv() を呼び出す。
    int actual_ret_invalid_recv_output =
        cplat_socket_recv(kSocket, buffer, sizeof(buffer), nullptr,
                          &detail); // [手順] - 受信バイト数出力先に nullptr を指定して cplat_socket_recv() を呼び出す。
    int actual_ret_invalid_recv_length =
        cplat_socket_recv(kSocket, buffer, CPLAT_SOCKET_MAX_TRANSFER + (size_t)1U, &received,
                          &detail); // [手順] - 最大転送量を超える受信長を指定して cplat_socket_recv() を呼び出す。
    int actual_ret_send = cplat_socket_send(kSocket, buffer, sizeof(buffer), &sent,
                                            &detail); // [手順] - 送信が成功する状態で cplat_socket_send() を呼び出す。
    int actual_ret_recv = cplat_socket_recv(kSocket, buffer, sizeof(buffer), &received,
                                            &detail); // [手順] - 受信が成功する状態で cplat_socket_recv() を呼び出す。
    int actual_ret_send_failure =
        cplat_socket_send(kSocket, buffer, sizeof(buffer), &sent_failure,
                          &detail); // [手順] - 送信が失敗する状態で cplat_socket_send() を呼び出す。
    int actual_ret_recv_failure =
        cplat_socket_recv(kSocket, buffer, sizeof(buffer), &received_failure,
                          &detail); // [手順] - 受信が失敗する状態で cplat_socket_recv() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_invalid_send); // [確認_異常系] - 無効なソケットを指定した cplat_socket_send() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_invalid_send_buffer); // [確認_異常系] - nullptr の送信バッファーを指定した cplat_socket_send() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_invalid_send_output); // [確認_異常系] - nullptr の送信バイト数出力先を指定した cplat_socket_send() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_invalid_send_length); // [確認_異常系] - 最大転送量を超える長さを指定した cplat_socket_send() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_invalid_recv_socket); // [確認_異常系] - 無効なソケットを指定した cplat_socket_recv() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_invalid_recv); // [確認_異常系] - nullptr の受信バッファーを指定した cplat_socket_recv() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_invalid_recv_output); // [確認_異常系] - nullptr の受信バイト数出力先を指定した cplat_socket_recv() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_invalid_recv_length); // [確認_異常系] - 最大転送量を超える長さを指定した cplat_socket_recv() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_OK,
              actual_ret_send); // [確認_正常系] - 送信を注入した cplat_socket_send() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              actual_ret_recv); // [確認_正常系] - 受信を注入した cplat_socket_recv() の戻り値が CPLAT_OK であること。
    EXPECT_EQ((size_t)3,
              sent); // [確認_正常系] - sent が 3 であること。(下位の send() が返した送信バイト数であることの確認)
    EXPECT_EQ(
        (size_t)2,
        received); // [確認_正常系] - received が 2 であること。(下位の recv() が返した受信バイト数であることの確認)
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_send_failure); // [確認_異常系] - 送信失敗を注入した cplat_socket_send() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_recv_failure); // [確認_異常系] - 受信失敗を注入した cplat_socket_recv() の戻り値が CPLAT_ERR_UNKNOWN であること。
}
// [サブ手順参照 名前=socketTest.TearDown]

// sendto と recvfrom がエンドポイントを変換して送受信することの確認
TEST_F(socketTest, datagram_operations_succeed)
{
    // Arrange
    char buffer[4] = {};
    size_t transferred = 0U;
    cplat_ipv4_endpoint peer = {};

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_sys_socket_, sendto(_, _, _, (int)kSocket, _, 4U, 0, _, _)).WillOnce(Return((ssize_t)4));
    // [Pre-Assert確認_正常系] - sendto() が kSocket、4U、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - sendto() から 4 を返す。
    EXPECT_CALL(mock_sys_socket_, recvfrom(_, _, _, (int)kSocket, _, 4U, 0, _, _))
        .WillRepeatedly(
            [](const char *, const int, const char *, int, void *, size_t, int, struct sockaddr *address, socklen_t *)
            {
                struct sockaddr_in *native = reinterpret_cast<struct sockaddr_in *>(address);
                native->sin_addr.s_addr = cplat_ipv4_addr_loopback();
                native->sin_port = kEndpoint.port;
                return (ssize_t)2;
            });

    // [Pre-Assert確認_正常系] - recvfrom() が kSocket、4U、0 を指定して任意の回数呼び出されること。
    // [Pre-Assert手順] - recvfrom() から、address の IPv4 アドレスに cplat_ipv4_addr_loopback()、ポートに kEndpoint.port を設定して 2 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, sendto(_, _, _, (SOCKET)kSocket, _, 4, 0, _, _)).WillOnce(Return(4));

    // [Pre-Assert確認_正常系] - sendto() が kSocket、4、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - sendto() から 4 を返す。
    EXPECT_CALL(mock_winsock_, recvfrom(_, _, _, (SOCKET)kSocket, _, 4, 0, _, _))
        .WillRepeatedly(
            [](const char *, const int, const char *, SOCKET, char *, int, int, struct sockaddr *address, int *)
            {
                SOCKADDR_IN *native = reinterpret_cast<SOCKADDR_IN *>(address);
                native->sin_addr.S_un.S_addr = cplat_ipv4_addr_loopback();
                native->sin_port = kEndpoint.port;
                return 2;
            });

    // [Pre-Assert確認_正常系] - recvfrom() が kSocket、4、0 を指定して任意の回数呼び出されること。
    // [Pre-Assert手順] - recvfrom() から、address の IPv4 アドレスに cplat_ipv4_addr_loopback()、ポートに kEndpoint.port を設定して 2 を返す。
#endif /* PLATFORM_ */
    // [Pre-Assert手順] - sendto() から送信バイト数 4 を返し、recvfrom() から、送信元のエンドポイントを出力引数に設定して受信バイト数 2 を返す。

    // Act
    int actual_ret_send =
        cplat_socket_sendto(kSocket, buffer, sizeof(buffer), &kEndpoint, &transferred,
                            nullptr); // [手順] - 送信するデータグラムを指定して cplat_socket_sendto() を呼び出す。
    int actual_ret_recv =
        cplat_socket_recvfrom(kSocket, buffer, sizeof(buffer), &peer, &transferred,
                              nullptr); // [手順] - データグラムを受信する状態で cplat_socket_recvfrom() を呼び出す。
    int actual_ret_recv_without_peer = cplat_socket_recvfrom(
        kSocket, buffer, sizeof(buffer), nullptr, &transferred,
        nullptr); // [手順] - 接続元エンドポイントの出力先に nullptr を指定して cplat_socket_recvfrom() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret_send); // [確認_正常系] - cplat_socket_sendto() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              actual_ret_recv); // [確認_正常系] - cplat_socket_recvfrom() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_recv_without_peer); // [確認_正常系] - 接続元エンドポイントを要求しない cplat_socket_recvfrom() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        (size_t)2,
        transferred); // [確認_正常系] - transferred が 2 であること。(最後の recvfrom() が返した受信バイト数であることの確認)
    EXPECT_EQ(
        kEndpoint.address,
        peer.address); // [確認_正常系] - peer.address が kEndpoint.address と一致すること。(受信した接続元アドレスであることの確認)
    EXPECT_EQ(
        kEndpoint.port,
        peer.port); // [確認_正常系] - peer.port が kEndpoint.port と一致すること。(受信した接続元ポートであることの確認)
}
// [サブ手順参照 名前=socketTest.TearDown]

// sendto と recvfrom の引数不正および OS 失敗が処理されることの確認
TEST_F(socketTest, datagram_operations_report_invalid_and_os_failure)
{
    // Arrange
    char buffer[4] = {};
    size_t transferred = 0U;
    cplat_error detail = {};

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_sys_socket_, sendto(_, _, _, (int)kSocket, _, 4U, 0, _, _))
        .WillOnce(DoAll(Assign(&errno, ENETUNREACH), Return((ssize_t)-1)));
    // [Pre-Assert確認_異常系] - sendto() が kSocket、4U、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - sendto() から、errno に ENETUNREACH を設定して -1 を返す。
    EXPECT_CALL(mock_sys_socket_, recvfrom(_, _, _, (int)kSocket, _, 4U, 0, _, _))
        .WillOnce(DoAll(Assign(&errno, ECONNREFUSED), Return((ssize_t)-1)));
    // [Pre-Assert確認_異常系] - recvfrom() が kSocket、4U、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - recvfrom() から、errno に ECONNREFUSED を設定して -1 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, sendto(_, _, _, (SOCKET)kSocket, _, 4, 0, _, _)).WillOnce(Return(SOCKET_ERROR));
    // [Pre-Assert確認_異常系] - sendto() が kSocket、4、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - sendto() から SOCKET_ERROR を返す。
    EXPECT_CALL(mock_winsock_, recvfrom(_, _, _, (SOCKET)kSocket, _, 4, 0, _, _)).WillOnce(Return(SOCKET_ERROR));
    // [Pre-Assert確認_異常系] - recvfrom() が kSocket、4、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - recvfrom() から SOCKET_ERROR を返す。
    EXPECT_CALL(mock_winsock_, WSAGetLastError).WillOnce(Return(WSAENETUNREACH)).WillOnce(Return(WSAECONNREFUSED));
    // [Pre-Assert確認_異常系] - WSAGetLastError() が 2 回呼び出されること。
    // [Pre-Assert手順] - WSAGetLastError() から 1 回目は WSAENETUNREACH、2 回目は WSAECONNREFUSED を返す。
#endif /* PLATFORM_ */
    // [Pre-Assert手順] - sendto() から、Linux では errno に ENETUNREACH を設定して -1、Windows では SOCKET_ERROR を返し、recvfrom() からも失敗を返し、それぞれ errno または WSAGetLastError() で失敗要因を通知する。

    // Act
    int actual_ret_send_invalid_socket =
        cplat_socket_sendto(CPLAT_INVALID_SOCKET, buffer, sizeof(buffer), &kEndpoint, &transferred,
                            &detail); // [手順] - 無効なソケットを指定して cplat_socket_sendto() を呼び出す。
    int actual_ret_send_invalid_buffer =
        cplat_socket_sendto(kSocket, nullptr, sizeof(buffer), &kEndpoint, &transferred,
                            &detail); // [手順] - 送信バッファーに nullptr を指定して cplat_socket_sendto() を呼び出す。
    int actual_ret_send_invalid_output = cplat_socket_sendto(
        kSocket, buffer, sizeof(buffer), &kEndpoint, nullptr,
        &detail); // [手順] - 送信バイト数出力先に nullptr を指定して cplat_socket_sendto() を呼び出す。
    int actual_ret_send_invalid_length =
        cplat_socket_sendto(kSocket, buffer, CPLAT_SOCKET_MAX_TRANSFER + (size_t)1U, &kEndpoint, &transferred,
                            &detail); // [手順] - 最大転送量を超える長さを指定して cplat_socket_sendto() を呼び出す。
    int actual_ret_send_invalid =
        cplat_socket_sendto(kSocket, buffer, sizeof(buffer), nullptr, &transferred,
                            &detail); // [手順] - エンドポイントに nullptr を指定して cplat_socket_sendto() を呼び出す。
    int actual_ret_recv_invalid_socket =
        cplat_socket_recvfrom(CPLAT_INVALID_SOCKET, buffer, sizeof(buffer), nullptr, &transferred,
                              &detail); // [手順] - 無効なソケットを指定して cplat_socket_recvfrom() を呼び出す。
    int actual_ret_recv_invalid_buffer = cplat_socket_recvfrom(
        kSocket, nullptr, sizeof(buffer), nullptr, &transferred,
        &detail); // [手順] - 受信バッファーに nullptr を指定して cplat_socket_recvfrom() を呼び出す。
    int actual_ret_recv_invalid_length = cplat_socket_recvfrom(
        kSocket, buffer, CPLAT_SOCKET_MAX_TRANSFER + (size_t)1U, nullptr, &transferred,
        &detail); // [手順] - 最大転送量を超える長さを指定して cplat_socket_recvfrom() を呼び出す。
    int actual_ret_recv_invalid = cplat_socket_recvfrom(
        kSocket, buffer, sizeof(buffer), nullptr, nullptr,
        &detail); // [手順] - 受信バイト数の出力先に nullptr を指定して cplat_socket_recvfrom() を呼び出す。
    int actual_ret_send_failure =
        cplat_socket_sendto(kSocket, buffer, sizeof(buffer), &kEndpoint, &transferred,
                            &detail); // [手順] - sendto() が失敗する状態で cplat_socket_sendto() を呼び出す。
    int actual_ret_recv_failure =
        cplat_socket_recvfrom(kSocket, buffer, sizeof(buffer), nullptr, &transferred,
                              &detail); // [手順] - recvfrom() が失敗する状態で cplat_socket_recvfrom() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_send_invalid_socket); // [確認_異常系] - 無効なソケットを指定した cplat_socket_sendto() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_send_invalid_buffer); // [確認_異常系] - nullptr の送信バッファーを指定した cplat_socket_sendto() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_send_invalid_output); // [確認_異常系] - nullptr の送信バイト数出力先を指定した cplat_socket_sendto() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_send_invalid_length); // [確認_異常系] - 最大転送量を超える長さを指定した cplat_socket_sendto() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_send_invalid); // [確認_異常系] - nullptr のエンドポイントを指定した cplat_socket_sendto() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_recv_invalid_socket); // [確認_異常系] - 無効なソケットを指定した cplat_socket_recvfrom() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_recv_invalid_buffer); // [確認_異常系] - nullptr の受信バッファーを指定した cplat_socket_recvfrom() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_recv_invalid_length); // [確認_異常系] - 最大転送量を超える長さを指定した cplat_socket_recvfrom() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_recv_invalid); // [確認_異常系] - nullptr の出力先を指定した cplat_socket_recvfrom() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_send_failure); // [確認_異常系] - 失敗を注入した cplat_socket_sendto() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_recv_failure); // [確認_異常系] - 失敗を注入した cplat_socket_recvfrom() の戻り値が CPLAT_ERR_UNKNOWN であること。
}
// [サブ手順参照 名前=socketTest.TearDown]

// send_all の全送信、部分送信、ゼロ送信、失敗が処理されることの確認
TEST_F(socketTest, send_all_reports_results)
{
    // Arrange
    const char buffer[4] = {'a', 'b', 'c', 'd'};
    cplat_error detail = {};

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_sys_socket_, send(_, _, _, (int)kSocket, _, 4U, MSG_NOSIGNAL))
        .WillOnce(Return((ssize_t)4))
        .WillOnce(Return((ssize_t)2))
        .WillOnce(Return((ssize_t)0))
        .WillOnce(DoAll(Assign(&errno, EPIPE), Return((ssize_t)-1)));
    // [Pre-Assert確認_正常系] - send() が kSocket、4U、MSG_NOSIGNAL を指定して 4 回呼び出されること。
    // [Pre-Assert手順] - send() から、1 回目は 4、2 回目は 2、3 回目は 0、4 回目は errno に EPIPE を設定して -1 を返す。
    EXPECT_CALL(mock_sys_socket_, send(_, _, _, (int)kSocket, _, 2U, MSG_NOSIGNAL)).WillOnce(Return((ssize_t)2));
    // [Pre-Assert確認_正常系] - send() が kSocket、2U、MSG_NOSIGNAL を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - send() から 2 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, send(_, _, _, (SOCKET)kSocket, _, 4, 0))
        .WillOnce(Return(4))
        .WillOnce(Return(2))
        .WillOnce(Return(0))
        .WillOnce(Return(SOCKET_ERROR));
    // [Pre-Assert確認_正常系] - send() が kSocket、4、0 を指定して 4 回呼び出されること。
    // [Pre-Assert手順] - send() から 1 回目は 4、2 回目は 2、3 回目は 0、4 回目は SOCKET_ERROR を返す。
    EXPECT_CALL(mock_winsock_, send(_, _, _, (SOCKET)kSocket, _, 2, 0)).WillOnce(Return(2));
    // [Pre-Assert確認_正常系] - send() が kSocket、2、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - send() から 2 を返す。
    EXPECT_CALL(mock_winsock_, WSAGetLastError).WillOnce(Return(WSAECONNRESET));
    // [Pre-Assert確認_正常系] - WSAGetLastError() が 1 回呼び出されること。
    // [Pre-Assert手順] - WSAGetLastError() から WSAECONNRESET を返す。
#endif /* PLATFORM_ */
    // [Pre-Assert手順] - send() から、全量送信の 4、部分送信の 2、0 バイト送信の 0、失敗を表す -1 (Windows では SOCKET_ERROR) の順に返す。

    // Act
    int actual_ret_empty = cplat_socket_send_all(
        kSocket, buffer, 0U, &detail); // [手順] - 長さ 0 のデータを指定して cplat_socket_send_all() を呼び出す。
    int actual_ret_full =
        cplat_socket_send_all(kSocket, buffer, sizeof(buffer),
                              &detail); // [手順] - 全送信が成功する状態で cplat_socket_send_all() を呼び出す。
    int actual_ret_partial = cplat_socket_send_all(
        kSocket, buffer, sizeof(buffer), &detail); // [手順] - 部分送信になる状態で cplat_socket_send_all() を呼び出す。
    int actual_ret_zero =
        cplat_socket_send_all(kSocket, buffer, sizeof(buffer),
                              &detail); // [手順] - 0 バイト送信になる状態で cplat_socket_send_all() を呼び出す。
    int actual_ret_failure = cplat_socket_send_all(
        kSocket, buffer, sizeof(buffer), &detail); // [手順] - 送信が失敗する状態で cplat_socket_send_all() を呼び出す。
    int actual_ret_invalid =
        cplat_socket_send_all(CPLAT_INVALID_SOCKET, buffer, sizeof(buffer),
                              &detail); // [手順] - 無効なソケットを指定して cplat_socket_send_all() を呼び出す。
    int actual_ret_invalid_buffer = cplat_socket_send_all(
        kSocket, nullptr, sizeof(buffer),
        &detail); // [手順] - 送信バッファーに nullptr を指定して cplat_socket_send_all() を呼び出す。
    int actual_ret_invalid_length = cplat_socket_send_all(
        kSocket, buffer, CPLAT_SOCKET_MAX_TRANSFER + (size_t)1U,
        &detail); // [手順] - 最大転送量を超える送信長を指定して cplat_socket_send_all() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_empty); // [確認_正常系] - 長さ 0 を指定した cplat_socket_send_all() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_full); // [確認_正常系] - 全量送信を注入した cplat_socket_send_all() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_partial); // [確認_正常系] - 部分送信を注入した cplat_socket_send_all() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_zero); // [確認_異常系] - 0 バイト送信を注入した cplat_socket_send_all() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_failure); // [確認_異常系] - 送信失敗を注入した cplat_socket_send_all() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_invalid); // [確認_異常系] - 無効なソケットを指定した cplat_socket_send_all() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_invalid_buffer); // [確認_異常系] - nullptr の送信バッファーを指定した cplat_socket_send_all() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_invalid_length); // [確認_異常系] - 最大転送量を超える長さを指定した cplat_socket_send_all() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}
// [サブ手順参照 名前=socketTest.TearDown]

// recv_all の全受信、部分受信、EOF、失敗が処理されることの確認
TEST_F(socketTest, recv_all_reports_results)
{
    // Arrange
    char buffer[4] = {};
    cplat_error detail = {};

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_sys_socket_, recv(_, _, _, (int)kSocket, _, 4U, 0))
        .WillOnce(Return((ssize_t)4))
        .WillOnce(Return((ssize_t)2))
        .WillOnce(Return((ssize_t)0))
        .WillOnce(DoAll(Assign(&errno, ECONNRESET), Return((ssize_t)-1)));
    // [Pre-Assert確認_正常系] - recv() が kSocket、4U、0 を指定して 4 回呼び出されること。
    // [Pre-Assert手順] - recv() から、1 回目は 4、2 回目は 2、3 回目は 0、4 回目は errno に ECONNRESET を設定して -1 を返す。
    EXPECT_CALL(mock_sys_socket_, recv(_, _, _, (int)kSocket, _, 2U, 0)).WillOnce(Return((ssize_t)2));
    // [Pre-Assert確認_正常系] - recv() が kSocket、2U、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - recv() から 2 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, recv(_, _, _, (SOCKET)kSocket, _, 4, 0))
        .WillOnce(Return(4))
        .WillOnce(Return(2))
        .WillOnce(Return(0))
        .WillOnce(Return(SOCKET_ERROR));
    // [Pre-Assert確認_正常系] - recv() が kSocket、4、0 を指定して 4 回呼び出されること。
    // [Pre-Assert手順] - recv() から 1 回目は 4、2 回目は 2、3 回目は 0、4 回目は SOCKET_ERROR を返す。
    EXPECT_CALL(mock_winsock_, recv(_, _, _, (SOCKET)kSocket, _, 2, 0)).WillOnce(Return(2));
    // [Pre-Assert確認_正常系] - recv() が kSocket、2、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - recv() から 2 を返す。
    EXPECT_CALL(mock_winsock_, WSAGetLastError).WillOnce(Return(WSAECONNRESET));
    // [Pre-Assert確認_正常系] - WSAGetLastError() が 1 回呼び出されること。
    // [Pre-Assert手順] - WSAGetLastError() から WSAECONNRESET を返す。
#endif /* PLATFORM_ */
    // [Pre-Assert手順] - recv() から、全量受信の 4、部分受信の 2、0 バイト受信の 0、失敗を表す -1 (Windows では SOCKET_ERROR) の順に返す。

    // Act
    int actual_ret_empty = cplat_socket_recv_all(
        kSocket, buffer, 0U, &detail); // [手順] - 長さ 0 のデータを指定して cplat_socket_recv_all() を呼び出す。
    int actual_ret_full =
        cplat_socket_recv_all(kSocket, buffer, sizeof(buffer),
                              &detail); // [手順] - 全受信が成功する状態で cplat_socket_recv_all() を呼び出す。
    int actual_ret_partial = cplat_socket_recv_all(
        kSocket, buffer, sizeof(buffer), &detail); // [手順] - 部分受信になる状態で cplat_socket_recv_all() を呼び出す。
    int actual_ret_eof = cplat_socket_recv_all(
        kSocket, buffer, sizeof(buffer), &detail); // [手順] - EOF が返される状態で cplat_socket_recv_all() を呼び出す。
    int actual_ret_failure = cplat_socket_recv_all(
        kSocket, buffer, sizeof(buffer), &detail); // [手順] - 受信が失敗する状態で cplat_socket_recv_all() を呼び出す。
    int actual_ret_invalid =
        cplat_socket_recv_all(CPLAT_INVALID_SOCKET, buffer, sizeof(buffer),
                              &detail); // [手順] - 無効なソケットを指定して cplat_socket_recv_all() を呼び出す。
    int actual_ret_invalid_buffer = cplat_socket_recv_all(
        kSocket, nullptr, sizeof(buffer),
        &detail); // [手順] - 受信バッファーに nullptr を指定して cplat_socket_recv_all() を呼び出す。
    int actual_ret_invalid_length = cplat_socket_recv_all(
        kSocket, buffer, CPLAT_SOCKET_MAX_TRANSFER + (size_t)1U,
        &detail); // [手順] - 最大転送量を超える受信長を指定して cplat_socket_recv_all() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_empty); // [確認_正常系] - 長さ 0 を指定した cplat_socket_recv_all() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_full); // [確認_正常系] - 全量受信を注入した cplat_socket_recv_all() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_partial); // [確認_正常系] - 部分受信を注入した cplat_socket_recv_all() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_EOF,
        actual_ret_eof); // [確認_異常系] - EOF を注入した cplat_socket_recv_all() の戻り値が CPLAT_ERR_EOF であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_failure); // [確認_異常系] - 受信失敗を注入した cplat_socket_recv_all() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_invalid); // [確認_異常系] - 無効なソケットを指定した cplat_socket_recv_all() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_invalid_buffer); // [確認_異常系] - nullptr の受信バッファーを指定した cplat_socket_recv_all() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_invalid_length); // [確認_異常系] - 最大転送量を超える長さを指定した cplat_socket_recv_all() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}
// [サブ手順参照 名前=socketTest.TearDown]

// 下位 API が要求量を超える転送量を返した場合に、単発の送受信が異常を返し出力値を 0 のままにすることの確認
TEST_F(socketTest, send_and_recv_reject_transfer_exceeding_request)
{
    // Arrange
    unsigned char buffer[4] = {0U, 0U, 0U, 0U};
    cplat_ipv4_endpoint peer = {};
    size_t sent = 9U;
    size_t received = 9U;
    size_t sent_to = 9U;
    size_t received_from = 9U;
    cplat_error detail = {};

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_sys_socket_, send(_, _, _, (int)kSocket, _, 4U, MSG_NOSIGNAL)).WillOnce(Return((ssize_t)5));
    // [Pre-Assert確認_異常系] - send() が kSocket、4U、MSG_NOSIGNAL を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - send() から 5 を返す。
    EXPECT_CALL(mock_sys_socket_, recv(_, _, _, (int)kSocket, _, 4U, 0)).WillOnce(Return((ssize_t)5));
    // [Pre-Assert確認_異常系] - recv() が kSocket、4U、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - recv() から 5 を返す。
    EXPECT_CALL(mock_sys_socket_, sendto(_, _, _, (int)kSocket, _, 4U, 0, _, _)).WillOnce(Return((ssize_t)5));
    // [Pre-Assert確認_異常系] - sendto() が kSocket、4U、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - sendto() から 5 を返す。
    EXPECT_CALL(mock_sys_socket_, recvfrom(_, _, _, (int)kSocket, _, 4U, 0, _, _)).WillOnce(Return((ssize_t)5));
    // [Pre-Assert確認_異常系] - recvfrom() が kSocket、4U、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - recvfrom() から 5 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, send(_, _, _, (SOCKET)kSocket, _, 4, 0)).WillOnce(Return(5));
    // [Pre-Assert確認_異常系] - send() が kSocket、4、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - send() から 5 を返す。
    EXPECT_CALL(mock_winsock_, recv(_, _, _, (SOCKET)kSocket, _, 4, 0)).WillOnce(Return(5));
    // [Pre-Assert確認_異常系] - recv() が kSocket、4、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - recv() から 5 を返す。
    EXPECT_CALL(mock_winsock_, sendto(_, _, _, (SOCKET)kSocket, _, 4, 0, _, _)).WillOnce(Return(5));
    // [Pre-Assert確認_異常系] - sendto() が kSocket、4、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - sendto() から 5 を返す。
    EXPECT_CALL(mock_winsock_, recvfrom(_, _, _, (SOCKET)kSocket, _, 4, 0, _, _)).WillOnce(Return(5));
    // [Pre-Assert確認_異常系] - recvfrom() が kSocket、4、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - recvfrom() から 5 を返す。
#endif /* PLATFORM_ */
    // [Pre-Assert手順] - send() から要求量を超える転送バイト数 5 を返し、recv()、sendto()、recvfrom() からも同じ値を返す。

    // Act
    int actual_ret_send =
        cplat_socket_send(kSocket, buffer, sizeof(buffer), &sent,
                          &detail); // [手順] - 送信長に 4 バイトを指定して cplat_socket_send() を呼び出す。
    int actual_ret_recv =
        cplat_socket_recv(kSocket, buffer, sizeof(buffer), &received,
                          &detail); // [手順] - 受信長に 4 バイトを指定して cplat_socket_recv() を呼び出す。
    int actual_ret_sendto = cplat_socket_sendto(
        kSocket, buffer, sizeof(buffer), &kEndpoint, &sent_to,
        &detail); // [手順] - 送信長に 4 バイトと送信先エンドポイントを指定して cplat_socket_sendto() を呼び出す。
    int actual_ret_recvfrom =
        cplat_socket_recvfrom(kSocket, buffer, sizeof(buffer), &peer, &received_from,
                              &detail); // [手順] - 受信長に 4 バイトを指定して cplat_socket_recvfrom() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_send); // [確認_異常系] - 要求量超過時の cplat_socket_send() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ((size_t)0, sent); // [確認_異常系] - 要求量超過時の sent が 0 であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_recv); // [確認_異常系] - 要求量超過時の cplat_socket_recv() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ((size_t)0, received); // [確認_異常系] - 要求量超過時の received が 0 であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_sendto); // [確認_異常系] - 要求量超過時の cplat_socket_sendto() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ((size_t)0, sent_to); // [確認_異常系] - 要求量超過時の sent_to が 0 であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_recvfrom); // [確認_異常系] - 要求量超過時の cplat_socket_recvfrom() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ((size_t)0,
              received_from); // [確認_異常系] - 要求量超過時の received_from が 0 であること。
}
// [サブ手順参照 名前=socketTest.TearDown]

// 下位 API が残量を超える転送量を返した場合に、全量送受信が異常を返すことの確認
TEST_F(socketTest, send_all_and_recv_all_reject_transfer_exceeding_remaining)
{
    // Arrange
    unsigned char buffer[4] = {0U, 0U, 0U, 0U};
    cplat_error detail = {};

    // Pre-Assert
    // 下位の send / recv API が、4 バイトの要求ののち残量 2 バイトの要求で呼び出されること。
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_sys_socket_, send(_, _, _, (int)kSocket, _, 4U, MSG_NOSIGNAL)).WillOnce(Return((ssize_t)2));
    // [Pre-Assert確認_異常系] - send() が kSocket、4U、MSG_NOSIGNAL を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - send() から 2 を返す。
    EXPECT_CALL(mock_sys_socket_, send(_, _, _, (int)kSocket, _, 2U, MSG_NOSIGNAL)).WillOnce(Return((ssize_t)3));
    // [Pre-Assert確認_異常系] - send() が kSocket、2U、MSG_NOSIGNAL を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - send() から 3 を返す。
    EXPECT_CALL(mock_sys_socket_, recv(_, _, _, (int)kSocket, _, 4U, 0)).WillOnce(Return((ssize_t)2));
    // [Pre-Assert確認_異常系] - recv() が kSocket、4U、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - recv() から 2 を返す。
    EXPECT_CALL(mock_sys_socket_, recv(_, _, _, (int)kSocket, _, 2U, 0)).WillOnce(Return((ssize_t)3));
    // [Pre-Assert確認_異常系] - recv() が kSocket、2U、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - recv() から 3 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, send(_, _, _, (SOCKET)kSocket, _, 4, 0)).WillOnce(Return(2));
    // [Pre-Assert確認_異常系] - send() が kSocket、4、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - send() から 2 を返す。
    EXPECT_CALL(mock_winsock_, send(_, _, _, (SOCKET)kSocket, _, 2, 0)).WillOnce(Return(3));
    // [Pre-Assert確認_異常系] - send() が kSocket、2、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - send() から 3 を返す。
    EXPECT_CALL(mock_winsock_, recv(_, _, _, (SOCKET)kSocket, _, 4, 0)).WillOnce(Return(2));
    // [Pre-Assert確認_異常系] - recv() が kSocket、4、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - recv() から 2 を返す。
    EXPECT_CALL(mock_winsock_, recv(_, _, _, (SOCKET)kSocket, _, 2, 0)).WillOnce(Return(3));
    // [Pre-Assert確認_異常系] - recv() が kSocket、2、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - recv() から 3 を返す。
#endif /* PLATFORM_ */
    // [Pre-Assert手順] - send() から 1 回目は転送バイト数 2、2 回目は残量を超える 3 を返し、recv() からも同じ順に返す。

    // Act
    int actual_ret_send_all = cplat_socket_send_all(
        kSocket, buffer, sizeof(buffer),
        &detail); // [手順] - 全量送信するデータに 4 バイトを指定して cplat_socket_send_all() を呼び出す。
    int actual_ret_recv_all = cplat_socket_recv_all(
        kSocket, buffer, sizeof(buffer),
        &detail); // [手順] - 全量受信する長さに 4 バイトを指定して cplat_socket_recv_all() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_send_all); // [確認_異常系] - 残量超過時の cplat_socket_send_all() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_recv_all); // [確認_異常系] - 残量超過時の cplat_socket_recv_all() の戻り値が CPLAT_ERR_UNKNOWN であること。
}
// [サブ手順参照 名前=socketTest.TearDown]

// 単一ソケット待機の引数不正、タイムアウト、準備完了、失敗が処理されることの確認
TEST_F(socketTest, wait_single_reports_results)
{
    // Arrange
    cplat_error detail = {};
    cplat_error timeout_detail = {};
    cplat_error not_ready_detail = {};

    // Pre-Assert
    // 下位の待機 API が 1 個のソケットとタイムアウト 0 を指定して呼び出されること。
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_poll_, poll(_, _, _, _, 1, 0))
        .WillOnce(Return(0))
        .WillOnce(
            [](const char *, const int, const char *, struct pollfd *fds, nfds_t, int)
            {
                fds->revents = 0;
                return 1;
            })
        .WillOnce(
            [](const char *, const int, const char *, struct pollfd *fds, nfds_t, int)
            {
                fds->revents = fds->events;
                return 1;
            })
        .WillOnce(DoAll(Assign(&errno, EBADF), Return(-1)));
    // [Pre-Assert確認_正常系] - poll() が 1、0 を指定して 4 回呼び出されること。
    // [Pre-Assert手順] - poll() から、1 回目は 0、2 回目は fds->revents に 0 を設定して 1、3 回目は fds->revents に fds->events を設定して 1、4 回目は errno に EBADF を設定して -1 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, WSAPoll(_, _, _, _, 1, 0))
        .WillOnce(Return(0))
        .WillOnce(
            [](const char *, const int, const char *, LPWSAPOLLFD fds, ULONG, INT)
            {
                fds->revents = 0;
                return 1;
            })
        .WillOnce(
            [](const char *, const int, const char *, LPWSAPOLLFD fds, ULONG, INT)
            {
                fds->revents = POLLWRNORM;
                return 1;
            })
        .WillOnce(Return(SOCKET_ERROR));
    // [Pre-Assert確認_正常系] - WSAPoll() が 1、0 を指定して 4 回呼び出されること。
    // [Pre-Assert手順] - WSAPoll() から、1 回目は 0、2 回目は fds->revents に 0 を設定して 1、3 回目は fds->revents に POLLWRNORM を設定して 1、4 回目は SOCKET_ERROR を返す。
    EXPECT_CALL(mock_winsock_, WSAGetLastError).WillOnce(Return(WSAEBADF));
    // [Pre-Assert確認_正常系] - WSAGetLastError() が 1 回呼び出されること。
    // [Pre-Assert手順] - WSAGetLastError() から WSAEBADF を返す。
#endif /* PLATFORM_ */
    // [Pre-Assert手順] - poll() から 1 回目はタイムアウトの 0、2 回目と 3 回目は条件成立の 1、4 回目は失敗の -1 を返し、Windows では WSAPoll() から 0、1、1、SOCKET_ERROR の順に返す。

    // Act
    int actual_ret_invalid_socket = cplat_socket_timed_wait_readable(
        CPLAT_INVALID_SOCKET, 0,
        &detail); // [手順] - 無効なソケットを指定して cplat_socket_timed_wait_readable() を呼び出す。
    int actual_ret_negative_timeout = cplat_socket_timed_wait_readable(
        kSocket, -1,
        &detail); // [手順] - 負の待ち時間を指定して cplat_socket_timed_wait_readable() を呼び出す。
    int actual_ret_timeout = cplat_socket_timed_wait_readable(
        kSocket, 0, &timeout_detail); // [手順] - 待機がタイムアウトする状態で cplat_socket_timed_wait_readable() を呼び出す。
    int actual_ret_not_ready = cplat_socket_timed_wait_readable(
        kSocket, 0,
        &not_ready_detail); // [手順] - 待機イベントが一致しない状態で cplat_socket_timed_wait_readable() を呼び出す。
    int actual_ret_ready = cplat_socket_timed_wait_writable(
        kSocket, 0, &detail); // [手順] - 書き込み可能な状態で cplat_socket_timed_wait_writable() を呼び出す。
    int actual_ret_failure = cplat_socket_timed_wait_readable(
        kSocket, 0, &detail); // [手順] - 待機 API が失敗する状態で cplat_socket_timed_wait_readable() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_invalid_socket); // [確認_異常系] - 無効なソケットを指定した cplat_socket_timed_wait_readable() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_negative_timeout); // [確認_異常系] - 負の待ち時間を指定した cplat_socket_timed_wait_readable() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_TIMEOUT,
        actual_ret_timeout); // [確認_正常系] - タイムアウト時の cplat_socket_timed_wait_readable() の戻り値が CPLAT_ERR_TIMEOUT であること。
    EXPECT_EQ(
        CPLAT_ERR_TIMEOUT,
        actual_ret_not_ready); // [確認_正常系] - イベント不一致時の cplat_socket_timed_wait_readable() の戻り値が CPLAT_ERR_TIMEOUT であること。
#if defined(PLATFORM_LINUX)
    // [サブ手順参照 名前=socketTest.expect_detail]
    expect_detail(timeout_detail, CPLAT_ERROR_DOMAIN_SOCKET_ERRNO, CPLAT_ERR_TIMEOUT,
                  static_cast<unsigned long>(ETIMEDOUT));
    // [サブ手順参照 名前=socketTest.expect_detail]
    expect_detail(not_ready_detail, CPLAT_ERROR_DOMAIN_SOCKET_ERRNO, CPLAT_ERR_TIMEOUT,
                  static_cast<unsigned long>(ETIMEDOUT));
#elif defined(PLATFORM_WINDOWS)
    // [サブ手順参照 名前=socketTest.expect_detail]
    expect_detail(timeout_detail, CPLAT_ERROR_DOMAIN_WINSOCK, CPLAT_ERR_TIMEOUT,
                  static_cast<unsigned long>(WSAETIMEDOUT));
    // [サブ手順参照 名前=socketTest.expect_detail]
    expect_detail(not_ready_detail, CPLAT_ERROR_DOMAIN_WINSOCK, CPLAT_ERR_TIMEOUT,
                  static_cast<unsigned long>(WSAETIMEDOUT));
#endif
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_ready); // [確認_正常系] - 準備完了時の cplat_socket_timed_wait_writable() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_failure); // [確認_異常系] - 待機 API 失敗時の cplat_socket_timed_wait_readable() の戻り値が CPLAT_ERR_UNKNOWN であること。
}
// [サブ手順参照 名前=socketTest.TearDown]

// 複数ソケット待機の入力検証と全無効ソケット処理が行われることの確認
TEST_F(socketTest, wait_multi_rejects_invalid_and_waits_without_valid_socket)
{
    // Arrange
    const cplat_socket socks[1] = {CPLAT_INVALID_SOCKET};
    unsigned char ready[1] = {9U};
    cplat_error detail = {};
    cplat_error wait_detail = {};

    // Pre-Assert
    // 有効なソケットがない場合に cplat_sleep_ms がタイムアウト時間を引数として 1 回呼び出されること。
    EXPECT_CALL(mock_cplat_, cplat_sleep_ms(5)).Times(1);
    // [Pre-Assert確認_異常系] - cplat_sleep_ms() が 5 を指定して 1 回呼び出されること。

    // Act
    int actual_ret_null_socks = cplat_socket_timed_wait_readable_multi(
        nullptr, 1U, 0, ready,
        &detail); // [手順] - ソケット配列に nullptr を指定して cplat_socket_timed_wait_readable_multi() を呼び出す。
    int actual_ret_zero_count = cplat_socket_timed_wait_readable_multi(
        socks, 0U, 0, ready,
        &detail); // [手順] - 要素数に 0 を指定して cplat_socket_timed_wait_readable_multi() を呼び出す。
    int actual_ret_large_count = cplat_socket_timed_wait_readable_multi(
        socks, CPLAT_SOCKET_WAIT_MAX + 1U, 0, ready,
        &detail); // [手順] - 最大数を超える要素数を指定して cplat_socket_timed_wait_readable_multi() を呼び出す。
    int actual_ret_null_ready = cplat_socket_timed_wait_readable_multi(
        socks, 1U, 0, nullptr,
        &detail); // [手順] - 結果配列に nullptr を指定して cplat_socket_timed_wait_readable_multi() を呼び出す。
    int actual_ret_negative_timeout = cplat_socket_timed_wait_readable_multi(
        socks, 1U, -1, ready,
        &detail); // [手順] - 負の待ち時間を指定して cplat_socket_timed_wait_readable_multi() を呼び出す。
    int actual_ret_indefinite_no_valid = cplat_socket_wait_readable_multi(
        socks, 1U, ready,
        &detail); // [手順] - すべてのソケットが無効な状態で無期限の cplat_socket_wait_readable_multi() を呼び出す。
    int actual_ret_wait = cplat_socket_timed_wait_readable_multi(
        socks, 1U, 5, ready,
        &wait_detail); // [手順] - すべてのソケットが無効な状態で有限の待ち時間を指定して cplat_socket_timed_wait_readable_multi() を呼び出す。
    int actual_ret_wait_without_timeout = cplat_socket_timed_wait_readable_multi(
        socks, 1U, 0, ready,
        &detail); // [手順] - すべてのソケットが無効な状態で待機時間に 0 を指定して cplat_socket_timed_wait_readable_multi() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_socks); // [確認_異常系] - nullptr のソケット配列を指定した cplat_socket_timed_wait_readable_multi() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_zero_count); // [確認_異常系] - 要素数 0 を指定した cplat_socket_timed_wait_readable_multi() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_large_count); // [確認_異常系] - 最大数を超える要素数を指定した cplat_socket_timed_wait_readable_multi() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_ready); // [確認_異常系] - nullptr の結果配列を指定した cplat_socket_timed_wait_readable_multi() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_negative_timeout); // [確認_異常系] - 負の待ち時間を指定した cplat_socket_timed_wait_readable_multi() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_indefinite_no_valid); // [確認_異常系] - 全無効ソケットを指定した無期限の cplat_socket_wait_readable_multi() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_TIMEOUT,
        actual_ret_wait); // [確認_正常系] - 全無効ソケットへ有限の待ち時間を指定した cplat_socket_timed_wait_readable_multi() の戻り値が CPLAT_ERR_TIMEOUT であること。
#if defined(PLATFORM_LINUX)
    // [サブ手順参照 名前=socketTest.expect_detail]
    expect_detail(wait_detail, CPLAT_ERROR_DOMAIN_SOCKET_ERRNO, CPLAT_ERR_TIMEOUT,
                  static_cast<unsigned long>(ETIMEDOUT));
#elif defined(PLATFORM_WINDOWS)
    // [サブ手順参照 名前=socketTest.expect_detail]
    expect_detail(wait_detail, CPLAT_ERROR_DOMAIN_WINSOCK, CPLAT_ERR_TIMEOUT,
                  static_cast<unsigned long>(WSAETIMEDOUT));
#endif
    EXPECT_EQ(
        CPLAT_ERR_TIMEOUT,
        actual_ret_wait_without_timeout); // [確認_正常系] - 全無効ソケットで待機時間 0 を指定した cplat_socket_timed_wait_readable_multi() の戻り値が CPLAT_ERR_TIMEOUT であること。
    EXPECT_EQ(0U,
              ready[0]); // [確認_正常系] - ready[0] が 0U であること。(無効ソケットは準備完了にならないことの確認)
}
// [サブ手順参照 名前=socketTest.TearDown]

// 複数ソケット待機のタイムアウト、準備完了、失敗が処理されることの確認
TEST_F(socketTest, wait_multi_reports_results)
{
    // Arrange
    const cplat_socket socks[3] = {kSocket, CPLAT_INVALID_SOCKET, (cplat_socket)8};
    unsigned char ready[3] = {9U, 9U, 9U};
    unsigned char ready_after_ready[3] = {0U, 0U, 0U};
    cplat_error detail = {};

    // Pre-Assert
    // 下位の待機 API が 2 個のソケットとタイムアウト 0 を指定して呼び出されること。
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_poll_, poll(_, _, _, _, 2, 0))
        .WillOnce(
            [](const char *, const int, const char *, struct pollfd *fds, nfds_t, int)
            {
                fds[0].revents = 0;
                fds[1].revents = POLLERR;
                return 0;
            })
        .WillOnce(
            [](const char *, const int, const char *, struct pollfd *fds, nfds_t, int)
            {
                fds[0].revents = POLLIN;
                fds[1].revents = POLLHUP;
                return 2;
            })
        .WillOnce(DoAll(Assign(&errno, EBADF), Return(-1)));
    // [Pre-Assert確認_正常系] - poll() が 2、0 を指定して 3 回呼び出されること。
    // [Pre-Assert手順] - poll() から、1 回目は fds[0].revents に 0、fds[1].revents に POLLERR を設定して 0、2 回目は fds[0].revents に POLLIN、fds[1].revents に POLLHUP を設定して 2、3 回目は errno に EBADF を設定して -1 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, WSAPoll(_, _, _, _, 2, 0))
        .WillOnce(
            [](const char *, const int, const char *, LPWSAPOLLFD fds, ULONG, INT)
            {
                fds[0].revents = 0;
                fds[1].revents = POLLERR;
                return 0;
            })
        .WillOnce(
            [](const char *, const int, const char *, LPWSAPOLLFD fds, ULONG, INT)
            {
                fds[0].revents = POLLRDNORM;
                fds[1].revents = POLLHUP;
                return 2;
            })
        .WillOnce(Return(SOCKET_ERROR));
    // [Pre-Assert確認_正常系] - WSAPoll() が 2、0 を指定して 3 回呼び出されること。
    // [Pre-Assert手順] - WSAPoll() から、1 回目は fds[0].revents に 0、fds[1].revents に POLLERR を設定して 0、2 回目は fds[0].revents に POLLRDNORM、fds[1].revents に POLLHUP を設定して 2、3 回目は SOCKET_ERROR を返す。
    EXPECT_CALL(mock_winsock_, WSAGetLastError).WillOnce(Return(WSAENETDOWN));
    // [Pre-Assert確認_正常系] - WSAGetLastError() が 1 回呼び出されること。
    // [Pre-Assert手順] - WSAGetLastError() から WSAENETDOWN を返す。
#endif /* PLATFORM_ */
    // [Pre-Assert手順] - poll() から、一部のソケットの受信可能または非受信を出力引数に設定して 1 回目は 0、2 回目は 2、3 回目は -1 を返し、Windows では WSAPoll() から同様に設定して 0、2、SOCKET_ERROR を返す。

    // Act
    int actual_ret_timeout = cplat_socket_timed_wait_readable_multi(
        socks, 3U, 0, ready,
        &detail); // [手順] - 待機がタイムアウトする状態で cplat_socket_timed_wait_readable_multi() を呼び出す。
    int actual_ret_ready = cplat_socket_timed_wait_readable_multi(
        socks, 3U, 0, ready,
        &detail); // [手順] - 複数ソケットが準備完了になる状態で cplat_socket_timed_wait_readable_multi() を呼び出す。
    std::memcpy(ready_after_ready, ready, sizeof(ready_after_ready));
    int actual_ret_failure = cplat_socket_timed_wait_readable_multi(
        socks, 3U, 0, ready,
        &detail); // [手順] - 待機が失敗する状態で cplat_socket_timed_wait_readable_multi() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_TIMEOUT,
        actual_ret_timeout); // [確認_正常系] - タイムアウト時の cplat_socket_timed_wait_readable_multi() の戻り値が CPLAT_ERR_TIMEOUT であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_ready); // [確認_正常系] - 準備完了時の cplat_socket_timed_wait_readable_multi() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        1U,
        ready_after_ready
            [0]); // [確認_正常系] - ready_after_ready[0] が 1U であること。(1 番目のソケットが準備完了になることの確認)
    EXPECT_EQ(
        0U,
        ready_after_ready
            [1]); // [確認_正常系] - ready_after_ready[1] が 0U であること。(無効ソケットは準備完了にならないことの確認)
    EXPECT_EQ(
        1U,
        ready_after_ready
            [2]); // [確認_正常系] - ready_after_ready[2] が 1U であること。(3 番目のソケットが準備完了になることの確認)
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_failure); // [確認_異常系] - 待機失敗時の cplat_socket_timed_wait_readable_multi() の戻り値が CPLAT_ERR_UNKNOWN であること。
}
// [サブ手順参照 名前=socketTest.TearDown]

#if defined(PLATFORM_LINUX)

// 単一ソケット待機がシグナル中断後に再待機することの確認
TEST_F(socketTest, wait_single_retries_after_interrupt)
{
    // Arrange
    cplat_error detail = {};

    // Pre-Assert
    // 下位の待機 API が 1 個のソケットとタイムアウト 0 を指定して 2 回呼び出されること。
    EXPECT_CALL(mock_poll_, poll(_, _, _, _, 1, 0))
        .WillOnce(DoAll(Assign(&errno, EINTR), Return(-1)))
        .WillOnce(
            [](const char *, const int, const char *, struct pollfd *fds, nfds_t, int)
            {
                fds->revents = fds->events;
                return 1;
            });
    // [Pre-Assert確認_正常系] - poll() が 1、0 を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - poll() から、1 回目は errno に EINTR を設定して -1、2 回目は fds->revents に fds->events を設定して条件成立の 1 を返す。

    // Act
    int actual_ret = cplat_socket_timed_wait_readable(
        kSocket, 0,
        &detail); // [手順] - 受信可能になるまでの待機のタイムアウトに 0 を指定して cplat_socket_timed_wait_readable() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret); // [確認_正常系] - cplat_socket_timed_wait_readable() の戻り値が CPLAT_OK であること。
}
// [サブ手順参照 名前=socketTest.TearDown]

// 無期限の単一ソケット待機がシグナル中断後に期限を計算せず再待機することの確認
TEST_F(socketTest, wait_single_retries_without_deadline_after_interrupt)
{
    // Arrange
    cplat_error detail = {};

    // Pre-Assert
    // 無期限待機では単調時刻が取得されないこと。
    EXPECT_CALL(mock_cplat_, cplat_clock_get_monotonic_ms(_)).Times(0);

    // [Pre-Assert確認_正常系] - cplat_clock_get_monotonic_ms() が呼び出されないこと。
    EXPECT_CALL(mock_poll_, poll(_, _, _, _, 1, -1))
        .WillOnce(DoAll(Assign(&errno, EINTR), Return(-1)))
        .WillOnce(
            [](const char *, const int, const char *, struct pollfd *fds, nfds_t, int)
            {
                fds->revents = fds->events;
                return 1;
            });
    // [Pre-Assert確認_正常系] - poll() が 1、-1 を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - poll() から、1 回目は errno に EINTR を設定して -1、2 回目は fds->revents に fds->events を設定して条件成立の 1 を返す。

    // Act
    int actual_ret = cplat_socket_wait_readable(
        kSocket,
        &detail); // [手順] - 無期限の受信待機として cplat_socket_wait_readable() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret); // [確認_正常系] - 無期限の cplat_socket_wait_readable() の戻り値が CPLAT_OK であること。
}
// [サブ手順参照 名前=socketTest.TearDown]

// 期限付きの単一ソケット待機がシグナル中断後に残り時間で再待機することの確認
TEST_F(socketTest, wait_single_recomputes_remaining_after_interrupt)
{
    // Arrange
    cplat_error detail = {};

    // Pre-Assert
    // 単調時刻が待機の開始時と中断時の 2 回取得されること。
    EXPECT_CALL(mock_cplat_, cplat_clock_get_monotonic_ms(_))
        .WillOnce(DoAll(SetArgPointee<0>(uint64_t{1000U}), Return(CPLAT_OK)))
        .WillOnce(DoAll(SetArgPointee<0>(uint64_t{1040U}), Return(CPLAT_OK)));

    // [Pre-Assert確認_正常系] - cplat_clock_get_monotonic_ms() が 2 回呼び出されること。
    // [Pre-Assert手順] - cplat_clock_get_monotonic_ms() から開始時に 1000 ms、中断時に 1040 ms を返す。
    EXPECT_CALL(mock_poll_, poll(_, _, _, _, 1, 100)).WillOnce(DoAll(Assign(&errno, EINTR), Return(-1)));

    // [Pre-Assert確認_正常系] - poll() が 1、100 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - poll() から、errno に EINTR を設定して -1 を返す。
    EXPECT_CALL(mock_poll_, poll(_, _, _, _, 1, 60))
        .WillOnce(
            [](const char *, const int, const char *, struct pollfd *fds, nfds_t, int)
            {
                fds->revents = fds->events;
                return 1;
            });
    // [Pre-Assert確認_正常系] - poll() が 1、60 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - poll() から、fds->revents に fds->events を設定して条件成立の 1 を返す。

    // Act
    int actual_ret = cplat_socket_timed_wait_readable(
        kSocket, 100, &detail); // [手順] - タイムアウトに 100 ms を指定して cplat_socket_timed_wait_readable() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - タイムアウト 100 ms の cplat_socket_timed_wait_readable() の戻り値が CPLAT_OK であること。
}
// [サブ手順参照 名前=socketTest.TearDown]

// 期限付きの単一ソケット待機がシグナル中断で期限を過ぎた場合にタイムアウトとなることの確認
TEST_F(socketTest, wait_single_reports_not_ready_when_deadline_expires_after_interrupt)
{
    // Arrange
    cplat_error detail = {};

    // Pre-Assert
    // 単調時刻が待機の開始時と中断時の 2 回取得されること。
    EXPECT_CALL(mock_cplat_, cplat_clock_get_monotonic_ms(_))
        .WillOnce(DoAll(SetArgPointee<0>(uint64_t{1000U}), Return(CPLAT_OK)))
        .WillOnce(DoAll(SetArgPointee<0>(uint64_t{1100U}), Return(CPLAT_OK)));

    // [Pre-Assert確認_正常系] - cplat_clock_get_monotonic_ms() が 2 回呼び出されること。
    // [Pre-Assert手順] - cplat_clock_get_monotonic_ms() から開始時に 1000 ms、中断時に期限を過ぎた 1100 ms を返す。
    EXPECT_CALL(mock_poll_, poll(_, _, _, _, 1, 50)).WillOnce(DoAll(Assign(&errno, EINTR), Return(-1)));

    // [Pre-Assert確認_正常系] - poll() が 1、50 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - poll() から、errno に EINTR を設定して -1 を返す。

    // Act
    int actual_ret = cplat_socket_timed_wait_readable(
        kSocket, 50, &detail); // [手順] - タイムアウトに 50 ms を指定して cplat_socket_timed_wait_readable() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_TIMEOUT,
        actual_ret); // [確認_正常系] - 期限超過後の cplat_socket_timed_wait_readable() の戻り値が CPLAT_ERR_TIMEOUT であること。
}
// [サブ手順参照 名前=socketTest.TearDown]

// 複数ソケット待機がシグナル中断後に再待機することの確認
TEST_F(socketTest, wait_multi_retries_after_interrupt)
{
    // Arrange
    const cplat_socket socks[2] = {kSocket, (cplat_socket)8};
    unsigned char ready[2] = {9U, 9U};
    cplat_error detail = {};

    // Pre-Assert
    // 下位の待機 API が 2 個のソケットとタイムアウト 0 を指定して 2 回呼び出されること。
    EXPECT_CALL(mock_poll_, poll(_, _, _, _, 2, 0))
        .WillOnce(DoAll(Assign(&errno, EINTR), Return(-1)))
        .WillOnce(
            [](const char *, const int, const char *, struct pollfd *fds, nfds_t, int)
            {
                fds[0].revents = POLLIN;
                fds[1].revents = 0;
                return 1;
            });
    // [Pre-Assert確認_正常系] - poll() が 2、0 を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - poll() から、1 回目は errno に EINTR を設定して -1、2 回目は fds[0].revents に POLLIN、fds[1].revents に 0 を設定して 1 番目のソケットの受信可能を表す 1 を返す。

    // Act
    int actual_ret = cplat_socket_timed_wait_readable_multi(
        socks, 2U, 0, ready,
        &detail); // [手順] - 受信可能になるまで待機するソケットを 2 個指定して cplat_socket_timed_wait_readable_multi() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret); // [確認_正常系] - cplat_socket_timed_wait_readable_multi() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        1U,
        ready[0]); // [確認_正常系] - ready[0] が 1U であること。(再待機で 1 番目のソケットが準備完了になることの確認)
    EXPECT_EQ(0U,
              ready[1]); // [確認_正常系] - ready[1] が 0U であること。(2 番目のソケットは準備完了にならないことの確認)
}
// [サブ手順参照 名前=socketTest.TearDown]

// 接続受け付けがシグナル中断後に再試行することの確認
TEST_F(socketTest, accept_retries_after_interrupt)
{
    // Arrange
    cplat_socket accepted = CPLAT_INVALID_SOCKET;
    cplat_error detail = {};

    // Pre-Assert
    // 下位の accept API が待ち受けソケットを引数として 2 回呼び出されること。
    EXPECT_CALL(mock_sys_socket_, accept(_, _, _, (int)kSocket, _, _))
        .WillOnce(DoAll(Assign(&errno, EINTR), Return(-1)))
        .WillOnce(Return(8));
    // [Pre-Assert確認_正常系] - accept() が kSocket を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - accept() から、1 回目は errno に EINTR を設定して -1、2 回目は新しいソケットのハンドル 8 を返す。

    // Act
    int actual_ret = cplat_socket_accept(kSocket, nullptr, &accepted,
                                         &detail); // [手順] - 接続を受け付ける状態で cplat_socket_accept() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret); // [確認_正常系] - cplat_socket_accept() の戻り値が CPLAT_OK であること。
    EXPECT_EQ((cplat_socket)8,
              accepted); // [確認_正常系] - accepted が 8 であること。(再試行で受け付けたソケットが返されることの確認)
}
// [サブ手順参照 名前=socketTest.TearDown]

// 送信と受信がシグナル中断後に再試行することの確認
TEST_F(socketTest, send_and_recv_retry_after_interrupt)
{
    // Arrange
    unsigned char buffer[4] = {0U, 0U, 0U, 0U};
    size_t sent = 0U;
    size_t received = 0U;
    cplat_error detail = {};

    // Pre-Assert
    // 下位の send API が MSG_NOSIGNAL と 4 バイトを指定して 2 回呼び出されること。
    EXPECT_CALL(mock_sys_socket_, send(_, _, _, (int)kSocket, _, 4U, MSG_NOSIGNAL))
        .WillOnce(DoAll(Assign(&errno, EINTR), Return((ssize_t)-1)))
        .WillOnce(Return((ssize_t)4));
    // [Pre-Assert確認_正常系] - send() が kSocket、4U、MSG_NOSIGNAL を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - send() から、1 回目は errno に EINTR を設定して -1、2 回目は転送バイト数 4 を返す。
    EXPECT_CALL(mock_sys_socket_, recv(_, _, _, (int)kSocket, _, 4U, 0))
        .WillOnce(DoAll(Assign(&errno, EINTR), Return((ssize_t)-1)))
        .WillOnce(Return((ssize_t)4));
    // [Pre-Assert確認_正常系] - recv() が kSocket、4U、0 を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - recv() から、1 回目は errno に EINTR を設定して -1、2 回目は転送バイト数 4 を返す。

    // Act
    int actual_ret_send =
        cplat_socket_send(kSocket, buffer, sizeof(buffer), &sent,
                          &detail); // [手順] - 送信長に 4 バイトを指定して cplat_socket_send() を呼び出す。
    int actual_ret_recv =
        cplat_socket_recv(kSocket, buffer, sizeof(buffer), &received,
                          &detail); // [手順] - 受信長に 4 バイトを指定して cplat_socket_recv() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret_send); // [確認_正常系] - cplat_socket_send() の戻り値が CPLAT_OK であること。
    EXPECT_EQ((size_t)4,
              sent); // [確認_正常系] - 再試行後の sent が 4 であること。
    EXPECT_EQ(CPLAT_OK,
              actual_ret_recv); // [確認_正常系] - cplat_socket_recv() の戻り値が CPLAT_OK であること。
    EXPECT_EQ((size_t)4,
              received); // [確認_正常系] - 再試行後の received が 4 であること。
}
// [サブ手順参照 名前=socketTest.TearDown]

// データグラムの送信と受信がシグナル中断後に再試行することの確認
TEST_F(socketTest, sendto_and_recvfrom_retry_after_interrupt)
{
    // Arrange
    unsigned char buffer[4] = {0U, 0U, 0U, 0U};
    cplat_ipv4_endpoint peer = {};
    size_t sent = 0U;
    size_t received = 0U;
    cplat_error detail = {};

    // Pre-Assert
    // 下位の sendto API が 4 バイトを指定して 2 回呼び出されること。
    EXPECT_CALL(mock_sys_socket_, sendto(_, _, _, (int)kSocket, _, 4U, 0, _, _))
        .WillOnce(DoAll(Assign(&errno, EINTR), Return((ssize_t)-1)))
        .WillOnce(Return((ssize_t)4));
    // [Pre-Assert確認_正常系] - sendto() が kSocket、4U、0 を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - sendto() から、1 回目は errno に EINTR を設定して -1、2 回目は転送バイト数 4 を返す。
    EXPECT_CALL(mock_sys_socket_, recvfrom(_, _, _, (int)kSocket, _, 4U, 0, _, _))
        .WillOnce(DoAll(Assign(&errno, EINTR), Return((ssize_t)-1)))
        .WillOnce(Return((ssize_t)4));
    // [Pre-Assert確認_正常系] - recvfrom() が kSocket、4U、0 を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - recvfrom() から、1 回目は errno に EINTR を設定して -1、2 回目は転送バイト数 4 を返す。

    // Act
    int actual_ret_sendto = cplat_socket_sendto(
        kSocket, buffer, sizeof(buffer), &kEndpoint, &sent,
        &detail); // [手順] - 送信長に 4 バイトと送信先エンドポイントを指定して cplat_socket_sendto() を呼び出す。
    int actual_ret_recvfrom =
        cplat_socket_recvfrom(kSocket, buffer, sizeof(buffer), &peer, &received,
                              &detail); // [手順] - 受信長に 4 バイトを指定して cplat_socket_recvfrom() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret_sendto); // [確認_正常系] - cplat_socket_sendto() の戻り値が CPLAT_OK であること。
    EXPECT_EQ((size_t)4,
              sent); // [確認_正常系] - 再試行後の sent が 4 であること。
    EXPECT_EQ(CPLAT_OK,
              actual_ret_recvfrom); // [確認_正常系] - cplat_socket_recvfrom() の戻り値が CPLAT_OK であること。
    EXPECT_EQ((size_t)4,
              received); // [確認_正常系] - 再試行後の received が 4 であること。
}
// [サブ手順参照 名前=socketTest.TearDown]

// 全量送信と全量受信がシグナル中断後に再試行することの確認
TEST_F(socketTest, send_all_and_recv_all_retry_after_interrupt)
{
    // Arrange
    unsigned char buffer[4] = {0U, 0U, 0U, 0U};
    cplat_error detail = {};

    // Pre-Assert
    // 下位の send API が MSG_NOSIGNAL と 4 バイトを指定して 2 回呼び出されること。
    EXPECT_CALL(mock_sys_socket_, send(_, _, _, (int)kSocket, _, 4U, MSG_NOSIGNAL))
        .WillOnce(DoAll(Assign(&errno, EINTR), Return((ssize_t)-1)))
        .WillOnce(Return((ssize_t)4));
    // [Pre-Assert確認_正常系] - send() が kSocket、4U、MSG_NOSIGNAL を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - send() から、1 回目は errno に EINTR を設定して -1、2 回目は転送バイト数 4 を返す。
    EXPECT_CALL(mock_sys_socket_, recv(_, _, _, (int)kSocket, _, 4U, 0))
        .WillOnce(DoAll(Assign(&errno, EINTR), Return((ssize_t)-1)))
        .WillOnce(Return((ssize_t)4));
    // [Pre-Assert確認_正常系] - recv() が kSocket、4U、0 を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - recv() から、1 回目は errno に EINTR を設定して -1、2 回目は転送バイト数 4 を返す。

    // Act
    int actual_ret_send_all = cplat_socket_send_all(
        kSocket, buffer, sizeof(buffer),
        &detail); // [手順] - 全量送信するデータに 4 バイトを指定して cplat_socket_send_all() を呼び出す。
    int actual_ret_recv_all = cplat_socket_recv_all(
        kSocket, buffer, sizeof(buffer),
        &detail); // [手順] - 全量受信する長さに 4 バイトを指定して cplat_socket_recv_all() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret_send_all); // [確認_正常系] - cplat_socket_send_all() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              actual_ret_recv_all); // [確認_正常系] - cplat_socket_recv_all() の戻り値が CPLAT_OK であること。
}
// [サブ手順参照 名前=socketTest.TearDown]

// ブロッキング接続がシグナル中断後に完了を待って成功を確定することの確認
TEST_F(socketTest, connect_completes_after_interrupt)
{
    // Arrange
    cplat_error detail = {};

    // Pre-Assert
    // 下位の connect API が 1 回だけ呼び出されること。
    EXPECT_CALL(mock_sys_socket_, connect(_, _, _, (int)kSocket, _, _))
        .WillOnce(DoAll(Assign(&errno, EINTR), Return(-1)));
    // [Pre-Assert確認_正常系] - connect() が kSocket を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - connect() から、errno に EINTR を設定して -1 を返す。
    EXPECT_CALL(mock_poll_, poll(_, _, _, _, 1, -1))
        .WillOnce(
            [](const char *, const int, const char *, struct pollfd *fds, nfds_t, int)
            {
                fds->revents = fds->events;
                return 1;
            });
    // [Pre-Assert確認_正常系] - poll() が 1、-1 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - poll() から、fds->revents に fds->events を設定して書き込み可能を表す 1 を返す。
    EXPECT_CALL(mock_sys_socket_, getsockopt(_, _, _, (int)kSocket, SOL_SOCKET, SO_ERROR, _, _))
        .WillOnce(
            [](const char *, const int, const char *, int, int, int, void *value, socklen_t *)
            {
                *static_cast<int *>(value) = 0;
                return 0;
            });
    // [Pre-Assert確認_正常系] - getsockopt() が kSocket、SOL_SOCKET、SO_ERROR を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - getsockopt() から、value が指す int に保留エラーなしを表す 0 を設定して 0 を返す。

    // Act
    int actual_ret = cplat_socket_connect(
        kSocket, &kEndpoint, &detail); // [手順] - ブロッキング モードの状態で cplat_socket_connect() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret); // [確認_正常系] - 中断後の cplat_socket_connect() の戻り値が CPLAT_OK であること。
}
// [サブ手順参照 名前=socketTest.TearDown]

// ブロッキング接続がシグナル中断後の完了確認で保留エラーを検出することの確認
TEST_F(socketTest, connect_reports_pending_error_after_interrupt)
{
    // Arrange
    cplat_error detail = {};

    // Pre-Assert
    // 下位の connect API が 1 回だけ呼び出されること。
    EXPECT_CALL(mock_sys_socket_, connect(_, _, _, (int)kSocket, _, _))
        .WillOnce(DoAll(Assign(&errno, EINTR), Return(-1)));
    // [Pre-Assert確認_異常系] - connect() が kSocket を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - connect() から、errno に EINTR を設定して -1 を返す。
    EXPECT_CALL(mock_poll_, poll(_, _, _, _, 1, -1))
        .WillOnce(
            [](const char *, const int, const char *, struct pollfd *fds, nfds_t, int)
            {
                fds->revents = fds->events;
                return 1;
            });
    // [Pre-Assert確認_異常系] - poll() が 1、-1 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - poll() から、fds->revents に fds->events を設定して書き込み可能を表す 1 を返す。
    EXPECT_CALL(mock_sys_socket_, getsockopt(_, _, _, (int)kSocket, SOL_SOCKET, SO_ERROR, _, _))
        .WillOnce(
            [](const char *, const int, const char *, int, int, int, void *value, socklen_t *)
            {
                *static_cast<int *>(value) = ECONNREFUSED;
                return 0;
            });
    // [Pre-Assert確認_異常系] - getsockopt() が kSocket、SOL_SOCKET、SO_ERROR を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - getsockopt() から、value が指す int に接続拒否の保留エラー ECONNREFUSED を設定して 0 を返す。

    // Act
    int actual_ret = cplat_socket_connect(
        kSocket, &kEndpoint, &detail); // [手順] - ブロッキング モードの状態で cplat_socket_connect() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret); // [確認_異常系] - 保留エラーがある場合の cplat_socket_connect() の戻り値が CPLAT_ERR_UNKNOWN であること。
    // [サブ手順参照 名前=socketTest.expect_detail 区分=異常系]
    expect_detail(detail, CPLAT_ERROR_DOMAIN_SOCKET_ERRNO, CPLAT_ERR_UNKNOWN,
                  (unsigned long)ECONNREFUSED); // 詳細に接続拒否が記録されること。
}
// [サブ手順参照 名前=socketTest.TearDown]

// ブロッキング接続がシグナル中断後の待機失敗を通知することの確認
TEST_F(socketTest, connect_reports_wait_failure_after_interrupt)
{
    // Arrange
    cplat_error detail = {};

    // Pre-Assert
    // 下位の connect API が 1 回だけ呼び出されること。
    EXPECT_CALL(mock_sys_socket_, connect(_, _, _, (int)kSocket, _, _))
        .WillOnce(DoAll(Assign(&errno, EINTR), Return(-1)));
    // [Pre-Assert確認_異常系] - connect() が kSocket を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - connect() から、errno に EINTR を設定して -1 を返す。
    EXPECT_CALL(mock_poll_, poll(_, _, _, _, 1, -1)).WillOnce(DoAll(Assign(&errno, EBADF), Return(-1)));
    // [Pre-Assert確認_異常系] - poll() が 1、-1 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - poll() から、errno に EBADF を設定して -1 を返す。
    EXPECT_CALL(mock_sys_socket_, getsockopt(_, _, _, _, _, _, _, _)).Times(0);
    // [Pre-Assert確認_異常系] - getsockopt() が呼び出されないこと。

    // Act
    int actual_ret = cplat_socket_connect(
        kSocket, &kEndpoint, &detail); // [手順] - ブロッキング モードの状態で cplat_socket_connect() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret); // [確認_異常系] - 待機に失敗した場合の cplat_socket_connect() の戻り値が CPLAT_ERR_UNKNOWN であること。
    // [サブ手順参照 名前=socketTest.expect_detail 区分=異常系]
    expect_detail(detail, CPLAT_ERROR_DOMAIN_SOCKET_ERRNO, CPLAT_ERR_UNKNOWN,
                  (unsigned long)EBADF); // 詳細に待機失敗の要因が記録されること。
}
// [サブ手順参照 名前=socketTest.TearDown]

// ブロッキング接続がシグナル中断後の待機で条件不成立となった場合にタイムアウトを通知することの確認
TEST_F(socketTest, connect_reports_timeout_when_not_writable_after_interrupt)
{
    // Arrange
    cplat_error detail = {};

    // Pre-Assert
    // 下位の connect API が 1 回だけ呼び出されること。
    EXPECT_CALL(mock_sys_socket_, connect(_, _, _, (int)kSocket, _, _))
        .WillOnce(DoAll(Assign(&errno, EINTR), Return(-1)));
    // [Pre-Assert確認_異常系] - connect() が kSocket を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - connect() から、errno に EINTR を設定して -1 を返す。
    EXPECT_CALL(mock_poll_, poll(_, _, _, _, 1, -1)).WillOnce(Return(0));
    // [Pre-Assert確認_異常系] - poll() が 1、-1 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - poll() から条件不成立を表す 0 を返す。
    EXPECT_CALL(mock_sys_socket_, getsockopt(_, _, _, _, _, _, _, _)).Times(0);
    // [Pre-Assert確認_正常系] - getsockopt() が呼び出されないこと。

    // Act
    int actual_ret = cplat_socket_connect(
        kSocket, &kEndpoint, &detail); // [手順] - ブロッキング モードの状態で cplat_socket_connect() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_TIMEOUT,
        actual_ret); // [確認_異常系] - 条件不成立の場合の cplat_socket_connect() の戻り値が CPLAT_ERR_TIMEOUT であること。
}
// [サブ手順参照 名前=socketTest.TearDown]

#endif /* PLATFORM_LINUX */

#if defined(PLATFORM_LINUX)
// 時計失敗時に期限付き待機を開始しないことの確認
TEST_F(socketTest, clock_failure_does_not_poll)
{
    // Arrange
    cplat_error detail = {};

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_clock_get_monotonic_ms(_)).WillOnce(Return(CPLAT_ERR_UNKNOWN));

    // [Pre-Assert確認_異常系] - 開始時刻を 1 回取得すること。
    // [Pre-Assert手順] - 時計取得失敗を返す。
    EXPECT_CALL(mock_poll_, poll(_, _, _, _, _, _)).Times(0);

    // [Pre-Assert確認_異常系] - 不正な期限で poll() を呼び出さないこと。

    // Act
    int result = cplat_socket_timed_wait_readable(kSocket, 100, &detail);

    // [手順] - 時計が取得できない状態で期限付き待機を要求する。

    // Assert
    EXPECT_NE(CPLAT_OK, result); // [確認_異常系] - 待機失敗を返すこと。
}
#endif

// 単一ソケット待機が POLLHUP のみで準備完了を返すことの確認
TEST_F(socketTest, wait_readable_returns_ready_on_pollhup)
{
    // Arrange
    cplat_error detail = {};

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_poll_, poll(_, _, _, _, 1, 0))
        .WillOnce(
            [](const char *, const int, const char *, struct pollfd *fds, nfds_t, int)
            {
                fds[0].revents = POLLHUP;
                return 1;
            });
    // [Pre-Assert確認_正常系] - poll() が 1、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - poll() から fds[0].revents に POLLHUP のみを設定して 1 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, WSAPoll(_, _, _, _, 1, 0))
        .WillOnce(
            [](const char *, const int, const char *, LPWSAPOLLFD fds, ULONG, INT)
            {
                fds[0].revents = POLLHUP;
                return 1;
            });
    // [Pre-Assert確認_正常系] - WSAPoll() が 1、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - WSAPoll() から fds[0].revents に POLLHUP のみを設定して 1 を返す。
#endif /* PLATFORM_ */

    // Act
    int actual_ret = cplat_socket_timed_wait_readable(
        kSocket, 0, &detail); // [手順] - POLLHUP のみが返る状態で cplat_socket_timed_wait_readable() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - POLLHUP のみでも戻り値が CPLAT_OK であること。
}
// [サブ手順参照 名前=socketTest.TearDown]

// 単一ソケット待機が POLLERR のみで準備完了を返すことの確認
TEST_F(socketTest, wait_readable_returns_ready_on_pollerr)
{
    // Arrange
    cplat_error detail = {};

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_poll_, poll(_, _, _, _, 1, 0))
        .WillOnce(
            [](const char *, const int, const char *, struct pollfd *fds, nfds_t, int)
            {
                fds[0].revents = POLLERR;
                return 1;
            });
    // [Pre-Assert確認_正常系] - poll() が 1、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - poll() から fds[0].revents に POLLERR のみを設定して 1 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, WSAPoll(_, _, _, _, 1, 0))
        .WillOnce(
            [](const char *, const int, const char *, LPWSAPOLLFD fds, ULONG, INT)
            {
                fds[0].revents = POLLERR;
                return 1;
            });
    // [Pre-Assert確認_正常系] - WSAPoll() が 1、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - WSAPoll() から fds[0].revents に POLLERR のみを設定して 1 を返す。
#endif /* PLATFORM_ */

    // Act
    int actual_ret = cplat_socket_timed_wait_readable(
        kSocket, 0, &detail); // [手順] - POLLERR のみが返る状態で cplat_socket_timed_wait_readable() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - POLLERR のみでも戻り値が CPLAT_OK であること。
}
// [サブ手順参照 名前=socketTest.TearDown]

// 無期限待機および CPLAT_WAIT_FOREVER が下位の待機 API へ -1 を渡すことの確認
TEST_F(socketTest, indefinite_and_wait_forever_pass_negative_one_to_native_poll)
{
    // Arrange
    const cplat_socket socks[2] = {kSocket, (cplat_socket)8};
    unsigned char ready[2] = {0U, 0U};
    cplat_error detail = {};

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_poll_, poll(_, _, _, _, 1, -1))
        .Times(2)
        .WillRepeatedly(
            [](const char *, const int, const char *, struct pollfd *fds, nfds_t, int)
            {
                fds[0].revents = fds[0].events;
                return 1;
            });
    // [Pre-Assert確認_正常系] - 単一ソケットの無期限待機で poll() が 1、-1 を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - poll() から fds[0].revents に fds[0].events を設定して 1 を返す。
    EXPECT_CALL(mock_poll_, poll(_, _, _, _, 2, -1))
        .Times(2)
        .WillRepeatedly(
            [](const char *, const int, const char *, struct pollfd *fds, nfds_t, int)
            {
                fds[0].revents = POLLIN;
                fds[1].revents = 0;
                return 1;
            });
    // [Pre-Assert確認_正常系] - 複数ソケットの無期限待機で poll() が 2、-1 を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - poll() から fds[0].revents に POLLIN、fds[1].revents に 0 を設定して 1 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, WSAPoll(_, _, _, _, 1, -1))
        .Times(2)
        .WillRepeatedly(
            [](const char *, const int, const char *, LPWSAPOLLFD fds, ULONG, INT)
            {
                fds[0].revents = fds[0].events;
                return 1;
            });
    // [Pre-Assert確認_正常系] - 単一ソケットの無期限待機で WSAPoll() が 1、-1 を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - WSAPoll() から fds[0].revents に fds[0].events を設定して 1 を返す。
    EXPECT_CALL(mock_winsock_, WSAPoll(_, _, _, _, 2, -1))
        .Times(2)
        .WillRepeatedly(
            [](const char *, const int, const char *, LPWSAPOLLFD fds, ULONG, INT)
            {
                fds[0].revents = POLLRDNORM;
                fds[1].revents = 0;
                return 1;
            });
    // [Pre-Assert確認_正常系] - 複数ソケットの無期限待機で WSAPoll() が 2、-1 を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - WSAPoll() から fds[0].revents に POLLRDNORM、fds[1].revents に 0 を設定して 1 を返す。
#endif /* PLATFORM_ */

    // Act
    int actual_ret_writable =
        cplat_socket_wait_writable(kSocket, &detail); // [手順] - 無期限の書き込み待機を呼び出す。
    int actual_ret_timed_forever = cplat_socket_timed_wait_readable(
        kSocket, CPLAT_WAIT_FOREVER, &detail); // [手順] - CPLAT_WAIT_FOREVER の受信待機を呼び出す。
    int actual_ret_multi = cplat_socket_wait_readable_multi(
        socks, 2U, ready, &detail); // [手順] - 無期限の複数ソケット受信待機を呼び出す。
    int actual_ret_multi_forever = cplat_socket_timed_wait_readable_multi(
        socks, 2U, CPLAT_WAIT_FOREVER, ready, &detail); // [手順] - CPLAT_WAIT_FOREVER の複数ソケット受信待機を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret_writable); // [確認_正常系] - cplat_socket_wait_writable() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              actual_ret_timed_forever); // [確認_正常系] - CPLAT_WAIT_FOREVER の受信待機が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              actual_ret_multi); // [確認_正常系] - cplat_socket_wait_readable_multi() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              actual_ret_multi_forever); // [確認_正常系] - CPLAT_WAIT_FOREVER の複数待機が CPLAT_OK であること。
    EXPECT_EQ(1U, ready[0]);             // [確認_正常系] - ready[0] が 1U であること。
    EXPECT_EQ(0U, ready[1]);             // [確認_正常系] - ready[1] が 0U であること。
}
// [サブ手順参照 名前=socketTest.TearDown]
