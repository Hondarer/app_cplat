#include <testfw.h>
#include <cplat/base/error.h>
#include <cplat/base/error_message_internal.h>
#include <cplat/base/result.h>
#include <mock_cplat.h>
#include <mock_string.h>

#include <errno.h>

#if defined(PLATFORM_LINUX)
    #include <netdb.h>
#endif

#include <cstring>
#include <set>
#include <string>
#include <vector>

/* result.h が定義するエラー コードの一覧 (CPLAT_OK を除く)。 */
static std::vector<int> all_error_codes()
{
    return std::vector<int>{CPLAT_ERR_UNKNOWN,
                            CPLAT_ERR_INVALID_ARGUMENT,
                            CPLAT_ERR_UNSUPPORTED,
                            CPLAT_ERR_PERMISSION_DENIED,
                            CPLAT_ERR_DUPLICATE_DEFINITION,
                            CPLAT_ERR_DUPLICATE_KEY,
                            CPLAT_ERR_OUT_OF_MEMORY,
                            CPLAT_ERR_BUSY,
                            CPLAT_ERR_TIMEOUT,
                            CPLAT_ERR_LIMIT_EXCEEDED,
                            CPLAT_ERR_BUFFER_TOO_SMALL,
                            CPLAT_ERR_CORRUPT_DESCRIPTOR,
                            CPLAT_ERR_STORAGE_FULL,
                            CPLAT_ERR_UNKNOWN_OPTION,
                            CPLAT_ERR_MISSING_VALUE,
                            CPLAT_ERR_UNEXPECTED_VALUE,
                            CPLAT_ERR_INVALID_INTEGER,
                            CPLAT_ERR_OUT_OF_RANGE,
                            CPLAT_ERR_MISSING_REQUIRED,
                            CPLAT_ERR_DUPLICATE_OPTION,
                            CPLAT_ERR_TOO_MANY_ARGUMENTS,
                            CPLAT_ERR_TOO_MANY_OCCURRENCES,
                            CPLAT_ERR_INVALID_PATTERN,
                            CPLAT_ERR_INVALID_ENCODING,
                            CPLAT_ERR_MALFORMED_DEFINITION,
                            CPLAT_ERR_EOF,
                            CPLAT_ERR_CANCELED,
                            CPLAT_ERR_IN_PROGRESS,
                            CPLAT_ERR_SIGNATURE_MISMATCH,
                            CPLAT_ERR_VERSION_MISMATCH};
}

class errorMessageTest : public Test
{
};

// 既知の結果コードが固有の文字列へ変換されることの確認
TEST_F(errorMessageTest, known_codes_map_to_distinct_texts)
{
    // Arrange
    const std::vector<int> codes = all_error_codes(); // [状態] - CPLAT_OK を除く全エラー コードを列挙する。
    std::set<std::string> texts;

    // Pre-Assert

    // Act
    for (int code : codes)
    {
        texts.insert(std::string(cplat_result_to_string(code))); // [手順] - 各コードを文字列化して集合へ挿入する。
    }

    // Assert
    EXPECT_EQ(codes.size(),
              texts.size()); // [確認_正常系] - 文字列が重複せず、集合の要素数が列挙したコード数と一致すること。
    EXPECT_STREQ("success",
                 cplat_result_to_string(CPLAT_OK)); // [確認_正常系] - CPLAT_OK が "success" になること。
}

// 未知の値が既定の文字列になることの確認
TEST_F(errorMessageTest, unknown_code_maps_to_default_text)
{
    // Arrange

    // Pre-Assert

    // Act
    const char *text = cplat_result_to_string(-9999); // [手順] - 定義されていない値 -9999 を文字列化する。

    // Assert
    EXPECT_STREQ("unknown result code",
                 text); // [確認_異常系] - cplat_result_to_string の戻り値が "unknown result code" であること。
}

// errno がメッセージへ変換されることの確認
TEST_F(errorMessageTest, errno_is_converted_to_message)
{
    // Arrange
    char buf[256] = {0}; // [状態] - 256 byte の格納先を 0 で初期化する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_internal_errno_message(buf, sizeof(buf), ENOENT); // [手順] - ENOENT を指定して呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_internal_errno_message の戻り値が CPLAT_OK であること。
    EXPECT_LT(0U, strlen(buf));         // [確認_正常系] - 空でないメッセージが格納されること。
}

// 引数不正の場合に CPLAT_ERR_INVALID_ARGUMENT を返すことの確認
TEST_F(errorMessageTest, invalid_arguments_are_rejected)
{
    // Arrange
    char buf[16] = {0}; // [状態] - 16 byte の格納先を 0 で初期化する。

    // Pre-Assert

    // Act
    int actual_ret_null_buf =
        cplat_internal_errno_message(NULL, sizeof(buf), ENOENT);              // [手順] - 格納先に NULL を指定して呼び出す。
    int actual_ret_zero_size = cplat_internal_errno_message(buf, 0U, ENOENT); // [手順] - サイズに 0 を指定して呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null_buf); // [確認_異常系] - 格納先が NULL の場合に cplat_internal_errno_message の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_zero_size); // [確認_異常系] - サイズが 0 の場合に cplat_internal_errno_message の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// error_message がドメインごとに文字列化することの確認
TEST_F(errorMessageTest, error_message_dispatches_by_domain)
{
    // Arrange
    char buf[256] = {0}; // [状態] - 文字列の格納先を 0 で初期化する。
    cplat_error error;

    // Pre-Assert

    // Act
    cplat_error_clear(&error); // [手順] - 空の詳細エラーを文字列化する。
    const int none_result = cplat_error_message(buf, sizeof(buf), &error);
    const std::string none_message(buf);
    cplat_error_capture_errno(&error, ENOENT); // [手順] - errno ドメインの詳細エラーを文字列化する。
    const int errno_result = cplat_error_message(buf, sizeof(buf), &error);
    const std::string errno_message(buf);
    error.domain = CPLAT_ERROR_DOMAIN_SOCKET_ERRNO;
    const int socket_errno_result =
        cplat_error_message(buf, sizeof(buf), &error); // [手順] - socket errno ドメインの詳細エラーを文字列化する。
    const std::string socket_errno_message(buf);

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        none_result); // [確認_正常系] - 空の詳細エラーに対する cplat_error_message の戻り値が CPLAT_OK であること。
    EXPECT_EQ("no error", none_message); // [確認_正常系] - 空の詳細エラーのメッセージが "no error" であること。
    EXPECT_EQ(
        CPLAT_OK,
        errno_result); // [確認_正常系] - errno ドメインに対する cplat_error_message の戻り値が CPLAT_OK であること。
    EXPECT_FALSE(errno_message.empty()); // [確認_正常系] - errno ドメインのメッセージが空でないこと。
    EXPECT_EQ(CPLAT_OK,
              socket_errno_result); // [確認_正常系] - socket errno ドメインの戻り値が CPLAT_OK であること。
    EXPECT_FALSE(socket_errno_message.empty()); // [確認_正常系] - socket errno ドメインのメッセージが空でないこと。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        cplat_error_message(NULL, sizeof(buf),
                               &error)); // [確認_異常系] - NULL の格納先が CPLAT_ERR_INVALID_ARGUMENT になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              cplat_error_message(buf, 0U,
                                     &error)); // [確認_異常系] - サイズ 0 が CPLAT_ERR_INVALID_ARGUMENT になること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        cplat_error_message(buf, sizeof(buf),
                               NULL)); // [確認_異常系] - NULL の詳細エラーが CPLAT_ERR_INVALID_ARGUMENT になること。
}

// 空の詳細エラーが小さいバッファーへ切り詰めて格納されることの確認
TEST_F(errorMessageTest, empty_error_message_is_truncated_to_buffer)
{
    // Arrange
    char buf[4] = {'X', 'X', 'X', 'X'};
    cplat_error error;

    cplat_error_clear(&error); // [状態] - 空の詳細エラーを用意する。

    // Pre-Assert

    // Act
    int result = cplat_error_message(buf, sizeof(buf),
                                        &error); // [手順] - 4 バイトのバッファーへ空の詳細エラーを文字列化する。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        result); // [確認_正常系] - 小さいバッファーでも cplat_error_message の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("no ", buf); // [確認_正常系] - バッファーへ終端付きで "no " が格納されること。
}

#if defined(PLATFORM_LINUX)

// Windows ドメインが Linux では不正引数として拒否されることの確認
TEST_F(errorMessageTest, windows_error_domain_is_rejected_on_linux)
{
    // Arrange
    char windows_buf[32] = {'X'};
    char winsock_buf[32] = {'X'};
    const cplat_error windows_error = {CPLAT_ERROR_DOMAIN_WINDOWS, CPLAT_ERR_UNKNOWN, 1UL};
    const cplat_error winsock_error = {CPLAT_ERROR_DOMAIN_WINSOCK, CPLAT_ERR_UNKNOWN, 1UL};

    // Pre-Assert

    // Act
    const int windows_result = cplat_error_message(
        windows_buf, sizeof(windows_buf), &windows_error); // [手順] - Windows ドメインを持つ詳細エラーを文字列化する。
    const int winsock_result = cplat_error_message(
        winsock_buf, sizeof(winsock_buf), &winsock_error); // [手順] - Winsock ドメインを持つ詳細エラーを文字列化する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        windows_result); // [確認_異常系] - Windows ドメインに対する戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_STREQ("", windows_buf); // [確認_異常系] - Windows ドメインでは出力バッファーが空になること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        winsock_result); // [確認_異常系] - Winsock ドメインに対する戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_STREQ("", winsock_buf); // [確認_異常系] - Winsock ドメインでは出力バッファーが空になること。
}

// GAI ドメインが Linux の gai_strerror() で文字列化されることの確認
TEST_F(errorMessageTest, gai_domain_is_converted_to_message_on_linux)
{
    // Arrange
    char buf[128] = {};
    const cplat_error error = {CPLAT_ERROR_DOMAIN_GAI, CPLAT_ERR_UNKNOWN,
                                  static_cast<unsigned long>(EAI_NONAME)};

    // Pre-Assert

    // Act
    int result = cplat_error_message(buf, sizeof(buf), &error); // [手順] - GAI ドメインを文字列化する。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result);       // [確認_正常系] - GAI ドメインの文字列化が成功すること。
    EXPECT_NE('\0', buf[0]); // [確認_正常系] - GAI エラー メッセージが格納されること。
}

// 未知のエラー ドメインが不正引数として拒否されることの確認
TEST_F(errorMessageTest, unknown_error_domain_is_rejected)
{
    // Arrange
    char buf[32] = {'X'};
    cplat_error error = {CPLAT_ERROR_DOMAIN_NONE, CPLAT_ERR_UNKNOWN, 1UL};
    const int invalid_domain_value = 99;

    std::memcpy(&error.domain, &invalid_domain_value,
                sizeof(error.domain)); // [状態] - 未知のドメイン値を持つ不正な詳細エラーを用意する。

    // Pre-Assert

    // Act
    int result =
        cplat_error_message(buf, sizeof(buf), &error); // [手順] - 未知のドメインを持つ詳細エラーを文字列化する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              result);     // [確認_異常系] - 未知のドメインに対する戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_STREQ("", buf); // [確認_異常系] - 未知のドメインでは出力バッファーが空になること。
}

#endif /* PLATFORM_LINUX */

#if defined(PLATFORM_LINUX)

// errno 文字列の取得に失敗した場合に拒否されることの確認
// Windows の cplat_internal_errno_message は strerror_s を使うため、この失敗経路は Linux のみに存在する
TEST_F(errorMessageTest, errno_message_returns_unknown_when_strerror_r_fails)
{
    // Arrange
    NiceMock<Mock_string> mock_string;
    char buf[64];

    std::memset(buf, 'X', sizeof(buf)); // [状態] - 出力バッファーを 'X' で埋めておく。

    // Pre-Assert
    EXPECT_CALL(mock_string, strerror_r(_, _, _, EACCES, buf, sizeof(buf)))
        .WillOnce(Return(
            EINVAL)); // [Pre-Assert確認_異常系] - strerror_r が errno 値 EACCES と 64 バイトのバッファーを指定して 1 回呼び出されること。
                      // [Pre-Assert手順] - strerror_r から EINVAL を返却する。

    // Act
    int actual_ret = cplat_internal_errno_message(buf, sizeof(buf), EACCES); // [手順] - cplat_internal_errno_message を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_internal_errno_message の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_STREQ("", buf); // [確認_異常系] - 出力バッファーが空文字列に初期化されること。
}

// 切り詰めを表す ERANGE が成功として扱われることの確認
// Windows の cplat_internal_errno_message は strerror_s を使うため、この分岐は Linux のみに存在する
TEST_F(errorMessageTest, errno_message_treats_erange_as_success)
{
    // Arrange
    NiceMock<Mock_string> mock_string;
    char buf[8] = {0}; // [状態] - 8 バイトの出力バッファーを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_string, strerror_r(_, _, _, EACCES, buf, sizeof(buf)))
        .WillOnce(DoAll(
            SetArrayArgument<4>("trunc", "trunc" + 6),
            Return(
                ERANGE))); // [Pre-Assert確認_正常系] - strerror_r が errno 値 EACCES と 8 バイトのバッファーを指定して 1 回呼び出されること。
    // [Pre-Assert手順] - バッファーへ切り詰め済みの文字列を書き込み、strerror_r から ERANGE を返却する。

    // Act
    int actual_ret = cplat_internal_errno_message(buf, sizeof(buf), EACCES); // [手順] - cplat_internal_errno_message を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 切り詰めは成功として扱われ CPLAT_OK が返ること。
    EXPECT_STREQ("trunc", buf);         // [確認_正常系] - 書き込まれた文字列が保持されること。
}

#endif /* PLATFORM_LINUX */
