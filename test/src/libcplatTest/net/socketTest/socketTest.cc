#include <testfw.h>

#include <mock_cplat.h>

#include <cplat/base/platform.h>
#include <cplat/base/result.h>
#include <cplat/net/socket.h>

#if defined(PLATFORM_LINUX)
    #include <mock_fcntl.h>
    #include <mock_poll.h>
    #include <sys/mock_socket.h>

    #include <errno.h>
    #include <fcntl.h>
    #include <netinet/in.h>
    #include <poll.h>
    #include <sys/socket.h>
#elif defined(PLATFORM_WINDOWS)
    #include <cplat/net/socket_internal.h>
    #include <mock_winsock.h>

    #include <errno.h>
#endif /* PLATFORM_ */

#include <cstring>

using testing::_;
using testing::Assign;
using testing::DoAll;
using testing::Mock;
using testing::NiceMock;
using testing::Return;

namespace
{

const cplat_socket kSocket = (cplat_socket)7;

const cplat_ipv4_endpoint kEndpoint = {CPLAT_IPV4_ADDR_LOOPBACK, cplat_hton16((uint16_t)12345U), 0U};

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

class socketTest : public Test
{
  protected:
    NiceMock<Mock_cplat> mock_cplat_;

#if defined(PLATFORM_LINUX)
    NiceMock<Mock_sys_socket> mock_sys_socket_;
    NiceMock<Mock_fcntl> mock_fcntl_;
    NiceMock<Mock_poll> mock_poll_;
#elif defined(PLATFORM_WINDOWS)
    NiceMock<Mock_winsock> mock_winsock_;
#endif /* PLATFORM_ */

    /* プラットフォームごとにモック対象が異なるため、呼び出し期待の検証はヘルパーへ限定する。 */
    bool verifyExpectations()
    {
#if defined(PLATFORM_LINUX)
        return Mock::VerifyAndClearExpectations(&mock_cplat_) && Mock::VerifyAndClearExpectations(&mock_sys_socket_);
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

#if defined(PLATFORM_WINDOWS)
    void SetUp() override
    {
        ON_CALL(mock_cplat_, cplat_call_once)
            .WillByDefault([](cplat_once_flag *, cplat_once_fn function) { function(); });
        ON_CALL(mock_winsock_, WSAStartup).WillByDefault(Return(0));
        ON_CALL(mock_winsock_, WSACleanup).WillByDefault(Return(0));
        ON_CALL(mock_winsock_, WSAGetLastError).WillByDefault(Return(0));
    }

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
    expect_detail(detail, CPLAT_ERROR_DOMAIN_WINSOCK, CPLAT_ERR_UNKNOWN,
                  static_cast<unsigned long>(WSASYSNOTREADY));
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
                native->sin_addr.s_addr = CPLAT_IPV4_ADDR_LOOPBACK;
                native->sin_port = cplat_hton16((uint16_t)54321U);
                *length = (socklen_t)sizeof(*native);
                return 8;
            });
    // [Pre-Assert確認_正常系] - accept() が kSocket を指定して任意の回数呼び出されること。
    // [Pre-Assert手順] - accept() から、address の IPv4 アドレスに CPLAT_IPV4_ADDR_LOOPBACK、ポートに cplat_hton16() で変換した 54321U、*length に sizeof(*native) を設定して 8 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, accept(_, _, _, (SOCKET)kSocket, _, _))
        .WillRepeatedly(
            [](const char *, const int, const char *, SOCKET, struct sockaddr *address, int *length)
            {
                SOCKADDR_IN *native = reinterpret_cast<SOCKADDR_IN *>(address);
                native->sin_addr.S_un.S_addr = CPLAT_IPV4_ADDR_LOOPBACK;
                native->sin_port = cplat_hton16((uint16_t)54321U);
                *length = (int)sizeof(*native);
                return (SOCKET)8;
            });
    // [Pre-Assert確認_正常系] - accept() が kSocket を指定して任意の回数呼び出されること。
    // [Pre-Assert手順] - accept() から、address の IPv4 アドレスに CPLAT_IPV4_ADDR_LOOPBACK、ポートに cplat_hton16() で変換した 54321U、*length に sizeof(*native) を設定して 8 を返す。
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
        CPLAT_IPV4_ADDR_LOOPBACK,
        peer.address); // [確認_正常系] - peer.address が CPLAT_IPV4_ADDR_LOOPBACK であること。(接続元アドレスであることの確認)
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
        kSocket, CPLAT_IPV4_ADDR_LOOPBACK,
        &detail); // [手順] - マルチキャスト インターフェイスを指定して cplat_socket_set_multicast_interface() を呼び出す。
    int actual_ret_join = cplat_socket_join_multicast_group(
        kSocket, CPLAT_IPV4_ADDR_LOOPBACK, CPLAT_IPV4_ADDR_ANY,
        &detail); // [手順] - 参加するマルチキャスト グループを指定して cplat_socket_join_multicast_group() を呼び出す。
    int actual_ret_leave = cplat_socket_leave_multicast_group(
        kSocket, CPLAT_IPV4_ADDR_LOOPBACK, CPLAT_IPV4_ADDR_ANY,
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
        CPLAT_INVALID_SOCKET, CPLAT_IPV4_ADDR_LOOPBACK, CPLAT_IPV4_ADDR_ANY,
        &detail); // [手順] - 無効なソケットと参加するマルチキャスト グループを指定して cplat_socket_join_multicast_group() を呼び出す。
    int actual_ret_leave_invalid = cplat_socket_leave_multicast_group(
        CPLAT_INVALID_SOCKET, CPLAT_IPV4_ADDR_LOOPBACK, CPLAT_IPV4_ADDR_ANY,
        &detail); // [手順] - 無効なソケットと離脱するマルチキャスト グループを指定して cplat_socket_leave_multicast_group() を呼び出す。
    int actual_ret_failure = cplat_socket_set_reuse_address(
        kSocket, 1, &detail); // [手順] - オプション設定が失敗する状態で cplat_socket_set_reuse_address() を呼び出す。
    int actual_ret_interface_failure = cplat_socket_set_multicast_interface(
        kSocket, CPLAT_IPV4_ADDR_LOOPBACK,
        &detail); // [手順] - インターフェイス設定が失敗する状態で cplat_socket_set_multicast_interface() を呼び出す。
    int actual_ret_join_failure = cplat_socket_join_multicast_group(
        kSocket, CPLAT_IPV4_ADDR_LOOPBACK, CPLAT_IPV4_ADDR_ANY,
        &detail); // [手順] - グループ参加が失敗する状態で cplat_socket_join_multicast_group() を呼び出す。
    int actual_ret_leave_failure = cplat_socket_leave_multicast_group(
        kSocket, CPLAT_IPV4_ADDR_LOOPBACK, CPLAT_IPV4_ADDR_ANY,
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
                native->sin_addr.s_addr = CPLAT_IPV4_ADDR_LOOPBACK;
                native->sin_port = kEndpoint.port;
                return (ssize_t)2;
            });
    // [Pre-Assert確認_正常系] - recvfrom() が kSocket、4U、0 を指定して任意の回数呼び出されること。
    // [Pre-Assert手順] - recvfrom() から、address の IPv4 アドレスに CPLAT_IPV4_ADDR_LOOPBACK、ポートに kEndpoint.port を設定して 2 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, sendto(_, _, _, (SOCKET)kSocket, _, 4, 0, _, _)).WillOnce(Return(4));
    // [Pre-Assert確認_正常系] - sendto() が kSocket、4、0 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - sendto() から 4 を返す。
    EXPECT_CALL(mock_winsock_, recvfrom(_, _, _, (SOCKET)kSocket, _, 4, 0, _, _))
        .WillRepeatedly(
            [](const char *, const int, const char *, SOCKET, char *, int, int, struct sockaddr *address, int *)
            {
                SOCKADDR_IN *native = reinterpret_cast<SOCKADDR_IN *>(address);
                native->sin_addr.S_un.S_addr = CPLAT_IPV4_ADDR_LOOPBACK;
                native->sin_port = kEndpoint.port;
                return 2;
            });
    // [Pre-Assert確認_正常系] - recvfrom() が kSocket、4、0 を指定して任意の回数呼び出されること。
    // [Pre-Assert手順] - recvfrom() から、address の IPv4 アドレスに CPLAT_IPV4_ADDR_LOOPBACK、ポートに kEndpoint.port を設定して 2 を返す。
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
    int ready = 9;
    int ready_after_not_ready = 9;
    cplat_error detail = {};

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
                fds->revents = POLLRDNORM;
                return 1;
            })
        .WillOnce(Return(SOCKET_ERROR));
    // [Pre-Assert確認_正常系] - WSAPoll() が 1、0 を指定して 4 回呼び出されること。
    // [Pre-Assert手順] - WSAPoll() から、1 回目は 0、2 回目は fds->revents に 0 を設定して 1、3 回目は fds->revents に POLLRDNORM を設定して 1、4 回目は SOCKET_ERROR を返す。
    EXPECT_CALL(mock_winsock_, WSAGetLastError).WillOnce(Return(WSAEBADF));
    // [Pre-Assert確認_正常系] - WSAGetLastError() が 1 回呼び出されること。
    // [Pre-Assert手順] - WSAGetLastError() から WSAEBADF を返す。
#endif /* PLATFORM_ */
    // [Pre-Assert手順] - poll() から 1 回目はタイムアウトの 0、2 回目と 3 回目は条件成立の 1、4 回目は失敗の -1 を返し、Windows では WSAPoll() から 0、1、1、SOCKET_ERROR の順に返す。

    // Act
    int actual_ret_invalid_socket = cplat_socket_wait_readable(
        CPLAT_INVALID_SOCKET, 0, &ready,
        &detail); // [手順] - 無効なソケットを指定して cplat_socket_wait_readable() を呼び出す。
    int actual_ret_invalid_output = cplat_socket_wait_readable(
        kSocket, 0, nullptr,
        &detail); // [手順] - 待機結果の出力先に nullptr を指定して cplat_socket_wait_readable() を呼び出す。
    int actual_ret_timeout = cplat_socket_wait_readable(
        kSocket, 0, &ready, &detail); // [手順] - 待機がタイムアウトする状態で cplat_socket_wait_readable() を呼び出す。
    int actual_ret_not_ready = cplat_socket_wait_readable(
        kSocket, 0, &ready,
        &detail); // [手順] - 待機イベントが一致しない状態で cplat_socket_wait_readable() を呼び出す。
    ready_after_not_ready = ready;
    int actual_ret_ready = cplat_socket_wait_writable(
        kSocket, 0, &ready, &detail); // [手順] - 書き込み可能な状態で cplat_socket_wait_writable() を呼び出す。
    int actual_ret_failure = cplat_socket_wait_readable(
        kSocket, 0, &ready, &detail); // [手順] - 待機 API が失敗する状態で cplat_socket_wait_readable() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_invalid_socket); // [確認_異常系] - 無効なソケットを指定した cplat_socket_wait_readable() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_invalid_output); // [確認_異常系] - nullptr の出力先を指定した cplat_socket_wait_readable() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_timeout); // [確認_正常系] - タイムアウト時の cplat_socket_wait_readable() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_not_ready); // [確認_正常系] - イベント不一致時の cplat_socket_wait_readable() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        0,
        ready_after_not_ready); // [確認_正常系] - ready_after_not_ready が 0 であること。(イベント不一致時は準備完了にならないことの確認)
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_ready); // [確認_正常系] - 準備完了時の cplat_socket_wait_writable() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_failure); // [確認_異常系] - 待機 API 失敗時の cplat_socket_wait_readable() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(0,
              ready); // [確認_正常系] - ready が 0 であること。(最後の待機失敗で準備完了にならないことの確認)
}
// [サブ手順参照 名前=socketTest.TearDown]

// 複数ソケット待機の入力検証と全無効ソケット処理が行われることの確認
TEST_F(socketTest, wait_multi_rejects_invalid_and_waits_without_valid_socket)
{
    // Arrange
    const cplat_socket socks[1] = {CPLAT_INVALID_SOCKET};
    unsigned char ready[1] = {9U};
    cplat_error detail = {};

    // Pre-Assert
    // 有効なソケットがない場合に cplat_sleep_ms がタイムアウト時間を引数として 1 回呼び出されること。
    EXPECT_CALL(mock_cplat_, cplat_sleep_ms(5)).Times(1);
    // [Pre-Assert確認_異常系] - cplat_sleep_ms() が 5 を指定して 1 回呼び出されること。

    // Act
    int actual_ret_null_socks = cplat_socket_wait_readable_multi(
        nullptr, 1U, 0, ready,
        &detail); // [手順] - ソケット配列に nullptr を指定して cplat_socket_wait_readable_multi() を呼び出す。
    int actual_ret_zero_count = cplat_socket_wait_readable_multi(
        socks, 0U, 0, ready, &detail); // [手順] - 要素数に 0 を指定して cplat_socket_wait_readable_multi() を呼び出す。
    int actual_ret_large_count = cplat_socket_wait_readable_multi(
        socks, CPLAT_SOCKET_WAIT_MAX + 1U, 0, ready,
        &detail); // [手順] - 最大数を超える要素数を指定して cplat_socket_wait_readable_multi() を呼び出す。
    int actual_ret_null_ready = cplat_socket_wait_readable_multi(
        socks, 1U, 0, nullptr,
        &detail); // [手順] - 結果配列に nullptr を指定して cplat_socket_wait_readable_multi() を呼び出す。
    int actual_ret_wait = cplat_socket_wait_readable_multi(
        socks, 1U, 5, ready,
        &detail); // [手順] - すべてのソケットが無効な状態で cplat_socket_wait_readable_multi() を呼び出す。
    int actual_ret_wait_without_timeout = cplat_socket_wait_readable_multi(
        socks, 1U, 0, ready,
        &detail); // [手順] - すべてのソケットが無効な状態で待機時間に 0 を指定して cplat_socket_wait_readable_multi() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_socks); // [確認_異常系] - nullptr のソケット配列を指定した cplat_socket_wait_readable_multi() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_zero_count); // [確認_異常系] - 要素数 0 を指定した cplat_socket_wait_readable_multi() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_large_count); // [確認_異常系] - 最大数を超える要素数を指定した cplat_socket_wait_readable_multi() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_ready); // [確認_異常系] - nullptr の結果配列を指定した cplat_socket_wait_readable_multi() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_wait); // [確認_正常系] - 全無効ソケットを指定した cplat_socket_wait_readable_multi() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_wait_without_timeout); // [確認_正常系] - 全無効ソケットで待機時間 0 を指定した cplat_socket_wait_readable_multi() の戻り値が CPLAT_OK であること。
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
    int actual_ret_timeout = cplat_socket_wait_readable_multi(
        socks, 3U, 0, ready,
        &detail); // [手順] - 待機がタイムアウトする状態で cplat_socket_wait_readable_multi() を呼び出す。
    int actual_ret_ready = cplat_socket_wait_readable_multi(
        socks, 3U, 0, ready,
        &detail); // [手順] - 複数ソケットが準備完了になる状態で cplat_socket_wait_readable_multi() を呼び出す。
    std::memcpy(ready_after_ready, ready, sizeof(ready_after_ready));
    int actual_ret_failure = cplat_socket_wait_readable_multi(
        socks, 3U, 0, ready, &detail); // [手順] - 待機が失敗する状態で cplat_socket_wait_readable_multi() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_timeout); // [確認_正常系] - タイムアウト時の cplat_socket_wait_readable_multi() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_ready); // [確認_正常系] - 準備完了時の cplat_socket_wait_readable_multi() の戻り値が CPLAT_OK であること。
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
        actual_ret_failure); // [確認_異常系] - 待機失敗時の cplat_socket_wait_readable_multi() の戻り値が CPLAT_ERR_UNKNOWN であること。
}
// [サブ手順参照 名前=socketTest.TearDown]

#if defined(PLATFORM_LINUX)

// 単一ソケット待機がシグナル中断後に再待機することの確認
TEST_F(socketTest, wait_single_retries_after_interrupt)
{
    // Arrange
    int ready = 9;
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
    int actual_ret = cplat_socket_wait_readable(
        kSocket, 0, &ready,
        &detail); // [手順] - 受信可能になるまでの待機のタイムアウトに 0 を指定して cplat_socket_wait_readable() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret); // [確認_正常系] - cplat_socket_wait_readable() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1,
              ready); // [確認_正常系] - ready が 1 であること。(再待機で条件が成立することの確認)
}
// [サブ手順参照 名前=socketTest.TearDown]

// 無期限の単一ソケット待機がシグナル中断後に期限を計算せず再待機することの確認
TEST_F(socketTest, wait_single_retries_without_deadline_after_interrupt)
{
    // Arrange
    int ready = 9;
    cplat_error detail = {};

    // Pre-Assert
    // 無期限待機では単調時刻が取得されないこと。
    EXPECT_CALL(mock_cplat_, cplat_clock_get_monotonic_ms()).Times(0);
    // [Pre-Assert確認_正常系] - cplat_clock_get_monotonic_ms() が呼び出されないこと。
    EXPECT_CALL(mock_poll_, poll(_, _, _, _, 1, CPLAT_SOCKET_WAIT_FOREVER))
        .WillOnce(DoAll(Assign(&errno, EINTR), Return(-1)))
        .WillOnce(
            [](const char *, const int, const char *, struct pollfd *fds, nfds_t, int)
            {
                fds->revents = fds->events;
                return 1;
            });
    // [Pre-Assert確認_正常系] - poll() が 1、CPLAT_SOCKET_WAIT_FOREVER を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - poll() から、1 回目は errno に EINTR を設定して -1、2 回目は fds->revents に fds->events を設定して条件成立の 1 を返す。

    // Act
    int actual_ret = cplat_socket_wait_readable(
        kSocket, CPLAT_SOCKET_WAIT_FOREVER, &ready,
        &detail); // [手順] - 受信可能になるまでの待機時間に無期限を指定して cplat_socket_wait_readable() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret); // [確認_正常系] - 無期限指定の cplat_socket_wait_readable() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1,
              ready); // [確認_正常系] - ready が 1 であること。(再待機で条件が成立することの確認)
}
// [サブ手順参照 名前=socketTest.TearDown]

// 期限付きの単一ソケット待機がシグナル中断後に残り時間で再待機することの確認
TEST_F(socketTest, wait_single_recomputes_remaining_after_interrupt)
{
    // Arrange
    int ready = 9;
    cplat_error detail = {};

    // Pre-Assert
    // 単調時刻が待機の開始時と中断時の 2 回取得されること。
    EXPECT_CALL(mock_cplat_, cplat_clock_get_monotonic_ms())
        .WillOnce(Return((uint64_t)1000U))
        .WillOnce(Return((uint64_t)1040U));
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
    int actual_ret = cplat_socket_wait_readable(
        kSocket, 100, &ready,
        &detail); // [手順] - タイムアウトに 100 ms を指定して cplat_socket_wait_readable() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - タイムアウト 100 ms の cplat_socket_wait_readable() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1,
              ready); // [確認_正常系] - ready が 1 であること。(残り時間での再待機で条件が成立することの確認)
}
// [サブ手順参照 名前=socketTest.TearDown]

// 期限付きの単一ソケット待機がシグナル中断で期限を過ぎた場合に条件不成立となることの確認
TEST_F(socketTest, wait_single_reports_not_ready_when_deadline_expires_after_interrupt)
{
    // Arrange
    int ready = 9;
    cplat_error detail = {};

    // Pre-Assert
    // 単調時刻が待機の開始時と中断時の 2 回取得されること。
    EXPECT_CALL(mock_cplat_, cplat_clock_get_monotonic_ms())
        .WillOnce(Return((uint64_t)1000U))
        .WillOnce(Return((uint64_t)1100U));
    // [Pre-Assert確認_正常系] - cplat_clock_get_monotonic_ms() が 2 回呼び出されること。
    // [Pre-Assert手順] - cplat_clock_get_monotonic_ms() から開始時に 1000 ms、中断時に期限を過ぎた 1100 ms を返す。
    EXPECT_CALL(mock_poll_, poll(_, _, _, _, 1, 50)).WillOnce(DoAll(Assign(&errno, EINTR), Return(-1)));
    // [Pre-Assert確認_正常系] - poll() が 1、50 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - poll() から、errno に EINTR を設定して -1 を返す。

    // Act
    int actual_ret = cplat_socket_wait_readable(
        kSocket, 50, &ready,
        &detail); // [手順] - タイムアウトに 50 ms を指定して cplat_socket_wait_readable() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret); // [確認_正常系] - 期限超過後の cplat_socket_wait_readable() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0,
              ready); // [確認_正常系] - ready が 0 であること。(期限を過ぎたため準備完了にならないことの確認)
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
    int actual_ret = cplat_socket_wait_readable_multi(
        socks, 2U, 0, ready,
        &detail); // [手順] - 受信可能になるまで待機するソケットを 2 個指定して cplat_socket_wait_readable_multi() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret); // [確認_正常系] - cplat_socket_wait_readable_multi() の戻り値が CPLAT_OK であること。
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
    EXPECT_CALL(mock_poll_, poll(_, _, _, _, 1, CPLAT_SOCKET_WAIT_FOREVER))
        .WillOnce(
            [](const char *, const int, const char *, struct pollfd *fds, nfds_t, int)
            {
                fds->revents = fds->events;
                return 1;
            });
    // [Pre-Assert確認_正常系] - poll() が 1、CPLAT_SOCKET_WAIT_FOREVER を指定して 1 回呼び出されること。
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
    EXPECT_CALL(mock_poll_, poll(_, _, _, _, 1, CPLAT_SOCKET_WAIT_FOREVER))
        .WillOnce(
            [](const char *, const int, const char *, struct pollfd *fds, nfds_t, int)
            {
                fds->revents = fds->events;
                return 1;
            });
    // [Pre-Assert確認_異常系] - poll() が 1、CPLAT_SOCKET_WAIT_FOREVER を指定して 1 回呼び出されること。
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
    EXPECT_CALL(mock_poll_, poll(_, _, _, _, 1, CPLAT_SOCKET_WAIT_FOREVER))
        .WillOnce(DoAll(Assign(&errno, EBADF), Return(-1)));
    // [Pre-Assert確認_異常系] - poll() が 1、CPLAT_SOCKET_WAIT_FOREVER を指定して 1 回呼び出されること。
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
    EXPECT_CALL(mock_poll_, poll(_, _, _, _, 1, CPLAT_SOCKET_WAIT_FOREVER)).WillOnce(Return(0));
    // [Pre-Assert確認_異常系] - poll() が 1、CPLAT_SOCKET_WAIT_FOREVER を指定して 1 回呼び出されること。
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

// 受信停止の引数不正、成功、失敗が処理されることの確認
TEST_F(socketTest, shutdown_receive_reports_results)
{
    // Arrange
    cplat_socket socket = kSocket;
    cplat_socket failure_socket = kSocket;
    cplat_socket invalid_socket = CPLAT_INVALID_SOCKET;
    cplat_error detail = {};

    // Pre-Assert
    // 下位の受信停止 API が対象のソケットを引数として 2 回呼び出されること。
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_sys_socket_, shutdown(_, _, _, (int)kSocket, SHUT_RD))
        .WillOnce(Return(0))
        .WillOnce(DoAll(Assign(&errno, EBADF), Return(-1)));
    // [Pre-Assert確認_正常系] - shutdown() が kSocket、SHUT_RD を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - shutdown() から、1 回目は 0、2 回目は errno に EBADF を設定して -1 を返す。
#elif defined(PLATFORM_WINDOWS)
    EXPECT_CALL(mock_winsock_, closesocket(_, _, _, (SOCKET)kSocket))
        .WillOnce(Return(0))
        .WillOnce(Return(SOCKET_ERROR));
    // [Pre-Assert確認_正常系] - closesocket() が kSocket を指定して 2 回呼び出されること。
    // [Pre-Assert手順] - closesocket() から 1 回目は 0、2 回目は SOCKET_ERROR を返す。
    EXPECT_CALL(mock_winsock_, WSAGetLastError).WillOnce(Return(WSAEBADF));
    // [Pre-Assert確認_正常系] - WSAGetLastError() が 1 回呼び出されること。
    // [Pre-Assert手順] - WSAGetLastError() から WSAEBADF を返す。
#endif /* PLATFORM_ */
    // [Pre-Assert手順] - shutdown() から 1 回目は 0、2 回目は errno に EBADF を設定して -1 を返し、Windows では closesocket() から 1 回目は 0、2 回目は SOCKET_ERROR を返して WSAGetLastError() から失敗要因を返す。

    // Act
    int actual_ret_null = cplat_socket_shutdown_receive(
        nullptr, &detail); // [手順] - ソケット出力先に nullptr を指定して cplat_socket_shutdown_receive() を呼び出す。
    int actual_ret_invalid = cplat_socket_shutdown_receive(
        &invalid_socket, &detail); // [手順] - 無効なソケットを指定して cplat_socket_shutdown_receive() を呼び出す。
    int actual_ret_success = cplat_socket_shutdown_receive(
        &socket, &detail); // [手順] - 受信停止が成功する状態で cplat_socket_shutdown_receive() を呼び出す。
    int actual_ret_failure = cplat_socket_shutdown_receive(
        &failure_socket, &detail); // [手順] - 受信停止が失敗する状態で cplat_socket_shutdown_receive() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null); // [確認_異常系] - nullptr の出力先を指定した cplat_socket_shutdown_receive() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_invalid); // [確認_異常系] - 無効なソケットを指定した cplat_socket_shutdown_receive() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_success); // [確認_正常系] - 受信停止成功時の cplat_socket_shutdown_receive() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_failure); // [確認_異常系] - 受信停止失敗時の cplat_socket_shutdown_receive() の戻り値が CPLAT_ERR_UNKNOWN であること。
#if defined(PLATFORM_WINDOWS)
    EXPECT_EQ(CPLAT_INVALID_SOCKET,
              socket); // [確認_正常系] - Windows の受信停止成功時の socket が CPLAT_INVALID_SOCKET であること。
#endif                 /* PLATFORM_WINDOWS */
}
// [サブ手順参照 名前=socketTest.TearDown]
