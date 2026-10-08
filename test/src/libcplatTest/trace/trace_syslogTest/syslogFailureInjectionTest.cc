#include <cplat/base/platform.h>

#if defined(PLATFORM_LINUX)

    #include <testfw.h>
    #include <mock_cplat.h>
    #include <mock_stdio.h>
    #include <sys/mock_socket.h>

    #include <cplat/base/result.h>
    #include <cplat/trace/backends/syslog/syslog_internal.h>

    #include <errno.h>
    #include <string>
    #include <syslog.h>

using testing::_;
using testing::Assign;
using testing::DoAll;
using testing::DoDefault;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;

class syslogFailureInjectionTest : public Test
{
  protected:
    NiceMock<Mock_cplat> mock_cplat_;
};

// ハンドルの確保に失敗した場合に生成が失敗することの確認
TEST_F(syslogFailureInjectionTest, create_returns_null_when_handle_allocation_fails)
{
    // Arrange
    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_malloc(_))
        .WillOnce(Return(nullptr))
        .WillRepeatedly(
            DoDefault()); // [Pre-Assert確認_異常系] - cplat_malloc がハンドル確保のために 1 回目に呼び出されること。
                          // [Pre-Assert手順] - 1 回目は NULL を返却し、以降は本物へ委譲する。

    // Act
    cplat_syslog_sink *handle = cplat_syslog_sink_create("syslogFailureInjectionTest",
                                                         LOG_USER); // [手順] - cplat_syslog_sink_create を呼び出す。

    // Assert
    EXPECT_EQ((cplat_syslog_sink *)NULL,
              handle); // [確認_異常系] - cplat_syslog_sink_create の戻り値が NULL であること。
}

// 識別子の複製に失敗した場合に生成が失敗することの確認
TEST_F(syslogFailureInjectionTest, create_returns_null_when_ident_duplication_fails)
{
    // Arrange
    // Pre-Assert
    /* 1 回目はハンドル、2 回目が識別子の複製になる */
    EXPECT_CALL(mock_cplat_, cplat_malloc(_))
        .WillOnce(DoDefault())
        .WillOnce(Return(nullptr))
        .WillRepeatedly(
            DoDefault()); // [Pre-Assert確認_異常系] - cplat_malloc が識別子の複製のために 2 回目に呼び出されること。
                          // [Pre-Assert手順] - 2 回目は NULL を返却し、他は本物へ委譲する。

    // Act
    cplat_syslog_sink *handle = cplat_syslog_sink_create("syslogFailureInjectionTest",
                                                         LOG_USER); // [手順] - cplat_syslog_sink_create を呼び出す。

    // Assert
    EXPECT_EQ((cplat_syslog_sink *)NULL,
              handle); // [確認_異常系] - cplat_syslog_sink_create の戻り値が NULL であること。
}

// ソケットの作成に失敗しても生成が成功することの確認
// Windows は ETW / イベント ログを使うため、この経路は Linux のみに存在する
TEST_F(syslogFailureInjectionTest, create_succeeds_when_socket_creation_fails)
{
    // Arrange
    NiceMock<Mock_sys_socket> mock_sys_socket;

    // Pre-Assert
    EXPECT_CALL(mock_sys_socket, socket(_, _, _, _, _, _))
        .WillOnce(DoAll(Assign(&errno, EMFILE), Return(-1)))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - socket が 1 回目に呼び出されること。
                                      // [Pre-Assert手順] - errno に EMFILE を設定し、1 回目は -1 を返却する。

    // Act
    cplat_syslog_sink *handle = cplat_syslog_sink_create("syslogFailureInjectionTest",
                                                         LOG_USER); // [手順] - cplat_syslog_sink_create を呼び出す。

    // Assert
    EXPECT_NE((cplat_syslog_sink *)NULL,
              handle); // [確認_正常系] - 初回接続に失敗してもハンドルが生成されること。

    // Cleanup
    cplat_syslog_sink_dispose(handle);
}

// バックオフ経過後の再接続に成功した場合に送信を再開することの確認
TEST_F(syslogFailureInjectionTest, write_reconnects_after_backoff_elapsed)
{
    // Arrange
    int realtime_call = 0;
    NiceMock<Mock_sys_socket> mock_sys_socket;
    ON_CALL(mock_cplat_, cplat_clock_get_realtime(_))
        .WillByDefault(Invoke(
            [&realtime_call](cplat_timespec *timestamp)
            {
                if (realtime_call == 0)
                {
                    timestamp->tv_sec = 0;
                }
                else
                {
                    timestamp->tv_sec = 10;
                }
                timestamp->tv_nsec = 0;
                ++realtime_call;
            })); // [状態] - cplat_clock_get_realtime が呼び出された際に初回は 0 秒、以降は 10 秒を返すようにモックを設定する。

    // Pre-Assert
    EXPECT_CALL(mock_sys_socket, socket(_, _, _, _, _, _))
        .WillOnce(Return(-1))
        .WillOnce(
            Return(123)); // [Pre-Assert確認_正常系] - 初回接続とバックオフ経過後の再接続で socket を呼び出すこと。
                          // [Pre-Assert手順] - 初回は失敗し、再接続時は fd 123 を返却する。
    EXPECT_CALL(mock_sys_socket, sendto(_, _, _, 123, _, _, _, _, _))
        .WillOnce(Return(1)); // [Pre-Assert確認_正常系] - 再接続した fd へメッセージを送信すること。
                              // [Pre-Assert手順] - sendto から 1 を返却する。

    // Act
    cplat_syslog_sink *handle = cplat_syslog_sink_create("syslogFailureInjectionTest",
                                                         LOG_USER); // [手順] - 初回接続が失敗する sink を生成する。
    int result = cplat_syslog_sink_write(handle, CPLAT_TRACE_LEVEL_INFO, NULL,
                                         "message"); // [手順] - バックオフ経過後の時刻でメッセージを書き込む。

    // Assert
    ASSERT_NE((cplat_syslog_sink *)NULL,
              handle); // [確認_正常系] - cplat_syslog_sink_create の戻り値が NULL でないこと。
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_syslog_sink_write の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_syslog_sink_dispose(handle);
}

// 送信に失敗した場合に書き込みが破棄されることの確認
// Windows は ETW / イベント ログを使うため、この経路は Linux のみに存在する
TEST_F(syslogFailureInjectionTest, write_drops_message_when_sendto_fails)
{
    // Arrange
    cplat_syslog_sink *handle = cplat_syslog_sink_create("syslogFailureInjectionTest", LOG_USER);

    ASSERT_NE((cplat_syslog_sink *)NULL, handle); // [状態確認] - ハンドルが非 NULL であること。

    NiceMock<Mock_sys_socket> mock_sys_socket;

    // Pre-Assert
    EXPECT_CALL(mock_sys_socket, sendto(_, _, _, _, _, _, _, _, _))
        .WillOnce(DoAll(Assign(&errno, ECONNREFUSED), Return(-1)))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - sendto が 1 回目に呼び出されること。
                                      // [Pre-Assert手順] - errno に ECONNREFUSED を設定し、1 回目は -1 を返却する。

    // Act
    int actual_ret = cplat_syslog_sink_write(handle, CPLAT_TRACE_LEVEL_INFO, NULL,
                                             "message"); // [手順] - cplat_syslog_sink_write を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - 送信に失敗しても破棄扱いとして cplat_syslog_sink_write は CPLAT_OK を返すこと。

    // Cleanup
    cplat_syslog_sink_dispose(handle);
}

// 送信バッファーが満杯の場合に書き込みが破棄されることの確認
// Windows は ETW / イベント ログを使うため、この経路は Linux のみに存在する
TEST_F(syslogFailureInjectionTest, write_drops_message_when_send_buffer_is_full)
{
    // Arrange
    cplat_syslog_sink *handle = cplat_syslog_sink_create("syslogFailureInjectionTest", LOG_USER);

    ASSERT_NE((cplat_syslog_sink *)NULL, handle); // [状態確認] - ハンドルが非 NULL であること。

    NiceMock<Mock_sys_socket> mock_sys_socket;

    // Pre-Assert
    EXPECT_CALL(mock_sys_socket, sendto(_, _, _, _, _, _, _, _, _))
        .WillOnce(DoAll(Assign(&errno, EAGAIN), Return(-1)))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - sendto が 1 回目に呼び出されること。
                                      // [Pre-Assert手順] - errno に EAGAIN を設定し、1 回目は -1 を返却する。

    // Act
    int actual_ret = cplat_syslog_sink_write(handle, CPLAT_TRACE_LEVEL_INFO, NULL,
                                             "message"); // [手順] - cplat_syslog_sink_write を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret); // [確認_正常系] - 送信バッファー満杯は再接続を伴わない破棄として CPLAT_OK を返すこと。

    // Cleanup
    cplat_syslog_sink_dispose(handle);
}

// 再接続ロックの生成に失敗した場合に生成が失敗することの確認
TEST_F(syslogFailureInjectionTest, create_returns_null_when_reconnect_lock_creation_fails)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_local_lock_create(_))
        .WillOnce(Return(CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_異常系] - 再接続ロックの生成が失敗すること。
    // [Pre-Assert手順] - cplat_local_lock_create から CPLAT_ERR_UNKNOWN を返却する。

    // Act
    cplat_syslog_sink *handle =
        cplat_syslog_sink_create("syslogFailureInjectionTest",
                                 LOG_USER); // [手順] - 再接続ロック生成失敗を注入して sink を生成する。

    // Assert
    EXPECT_EQ((cplat_syslog_sink *)NULL,
              handle); // [確認_異常系] - ロック生成失敗時の戻り値が NULL であること。
}

// 現在時刻の解決にも失敗した場合に書き込みが失敗することの確認
TEST_F(syslogFailureInjectionTest, write_returns_unknown_when_fallback_timestamp_is_invalid)
{
    // Arrange
    cplat_timespec invalid_timestamp = {1, 1000000000L};
    cplat_syslog_sink *handle =
        cplat_syslog_sink_create("syslogFailureInjectionTest", LOG_USER); // [状態] - syslog sink を生成する。
    ASSERT_NE((cplat_syslog_sink *)NULL, handle);                         // [状態確認] - ハンドルが非 NULL であること。
    ON_CALL(mock_cplat_, cplat_clock_get_realtime(_))
        .WillByDefault(Invoke(
            [](cplat_timespec *timestamp)
            {
                timestamp->tv_sec = 1;
                timestamp->tv_nsec = 1000000000L;
            })); // [状態] - cplat_clock_get_realtime が呼び出された際に不正なナノ秒値 1000000000 を返すようにモックを設定する。

    // Pre-Assert

    // Act
    int result = cplat_syslog_sink_write(handle, CPLAT_TRACE_LEVEL_INFO, &invalid_timestamp,
                                         "message"); // [手順] - 不正な時刻で書き込みを実行する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - 時刻解決失敗時の戻り値が CPLAT_ERR_UNKNOWN であること。

    // Cleanup
    cplat_syslog_sink_dispose(handle);
}

// 送信後の再接続抑制期間中はメッセージを破棄することの確認
TEST_F(syslogFailureInjectionTest, write_drops_message_during_reconnect_backoff)
{
    // Arrange
    cplat_syslog_sink *handle =
        cplat_syslog_sink_create("syslogFailureInjectionTest", LOG_USER); // [状態] - syslog sink を生成する。
    ASSERT_NE((cplat_syslog_sink *)NULL, handle);                         // [状態確認] - ハンドルが非 NULL であること。
    NiceMock<Mock_sys_socket> mock_sys_socket;

    // Pre-Assert
    EXPECT_CALL(mock_sys_socket, sendto(_, _, _, _, _, _, _, _, _))
        .WillOnce(DoAll(Assign(&errno, ECONNREFUSED), Return(-1)))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - 初回送信を ECONNREFUSED で失敗させること。
                                      // [Pre-Assert手順] - errno に ECONNREFUSED を設定し、1 回目は -1 を返却する。

    // Act
    int first_result = cplat_syslog_sink_write(handle, CPLAT_TRACE_LEVEL_INFO, NULL,
                                               "first"); // [手順] - 初回送信を実行する。
    int backoff_result = cplat_syslog_sink_write(handle, CPLAT_TRACE_LEVEL_INFO, NULL,
                                                 "backoff"); // [手順] - 再接続抑制期間中に再送信する。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              first_result); // [確認_正常系] - 初回送信失敗が破棄扱いになること。
    EXPECT_EQ(CPLAT_OK,
              backoff_result); // [確認_正常系] - 再接続抑制期間中も破棄扱いになること。

    // Cleanup
    cplat_syslog_sink_dispose(handle);
}

// ソケット送信が成功した場合にバックオフを初期値へ戻して成功することの確認
TEST_F(syslogFailureInjectionTest, write_returns_ok_when_send_succeeds)
{
    // Arrange
    cplat_syslog_sink *handle =
        cplat_syslog_sink_create("syslogFailureInjectionTest", LOG_USER); // [状態] - syslog sink を生成する。
    ASSERT_NE((cplat_syslog_sink *)NULL, handle);                         // [状態確認] - ハンドルが非 NULL であること。
    NiceMock<Mock_sys_socket> mock_sys_socket;

    // Pre-Assert
    EXPECT_CALL(mock_sys_socket, sendto(_, _, _, _, _, _, _, _, _))
        .WillOnce(Return(1)); // [Pre-Assert確認_正常系] - syslog ソケットへの送信が成功すること。
                              // [Pre-Assert手順] - sendto から 1 を返却する。

    // Act
    int result = cplat_syslog_sink_write(handle, CPLAT_TRACE_LEVEL_INFO, NULL,
                                         "message"); // [手順] - メッセージを送信する。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - 送信成功時の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_syslog_sink_dispose(handle);
}

// 不正時刻の代替後に通常ソケット送信が成功しても UNKNOWN を返すことの確認
TEST_F(syslogFailureInjectionTest, write_reports_unknown_after_fallback_timestamp_with_successful_send)
{
    // Arrange
    cplat_timespec invalid_timestamp = {1, 1000000000L};
    cplat_syslog_sink *handle =
        cplat_syslog_sink_create("syslogFailureInjectionTest", LOG_USER); // [状態] - syslog sink を生成する。
    ASSERT_NE((cplat_syslog_sink *)NULL, handle);                         // [状態確認] - ハンドルが非 NULL であること。
    NiceMock<Mock_sys_socket> mock_sys_socket;

    // Pre-Assert
    EXPECT_CALL(mock_sys_socket, sendto(_, _, _, _, _, _, _, _, _))
        .WillOnce(Return(1)); // [Pre-Assert確認_正常系] - 代替時刻での送信が成功すること。
                              // [Pre-Assert手順] - sendto から 1 を返却する。

    // Act
    int result = cplat_syslog_sink_write(handle, CPLAT_TRACE_LEVEL_INFO, &invalid_timestamp,
                                         "message"); // [手順] - 不正時刻を指定して送信する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - 代替時刻を使用した場合も UNKNOWN が保持されること。

    // Cleanup
    cplat_syslog_sink_dispose(handle);
}

// テスト用 FD への長大なメッセージが切り詰められることの確認
TEST_F(syslogFailureInjectionTest, write_truncates_message_for_test_fd)
{
    // Arrange
    int pipe_fds[2];
    char actual[2200];
    std::string message(3000, 'x');
    const char *saved_fd = getenv("SYSLOG_TEST_FD");
    std::string saved_fd_value;
    if (saved_fd != NULL)
    {
        saved_fd_value = saved_fd;
    }
    ASSERT_EQ(0, pipe(pipe_fds));                               // [状態] - pipe を生成する。
                                                                // [状態確認] - pipe の生成が成功すること。
    std::string fd_text = std::to_string(pipe_fds[1]);          // [状態] - テスト用 FD をパイプの書き込み側とする。
    ASSERT_EQ(0, setenv("SYSLOG_TEST_FD", fd_text.c_str(), 1)); // [状態] - SYSLOG_TEST_FD をパイプの書き込み側とする。
    // [状態確認] - SYSLOG_TEST_FD の setenv の戻り値が 0 であること。
    cplat_syslog_sink *handle =
        cplat_syslog_sink_create("syslogFailureInjectionTest", LOG_USER); // [状態] - syslog sink を生成する。
    ASSERT_NE((cplat_syslog_sink *)NULL, handle);                         // [状態確認] - ハンドルが非 NULL であること。

    // Pre-Assert

    // Act
    int result = cplat_syslog_sink_write(handle, CPLAT_TRACE_LEVEL_INFO, NULL,
                                         message.c_str()); // [手順] - 長大なメッセージをテスト用 FD へ書き込む。
    close(pipe_fds[1]);
    pipe_fds[1] = -1;
    ssize_t read_size = read(pipe_fds[0], actual, sizeof(actual) - 1); // [手順] - 切り詰め後のメッセージを読み取る。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result);       // [確認_正常系] - 長大なメッセージの書き込みが成功扱いになること。
    EXPECT_GT(read_size, 0); // [確認_正常系] - 切り詰め後のメッセージが出力されること。
    EXPECT_LE(read_size,
              static_cast<ssize_t>(sizeof(actual) - 1)); // [確認_正常系] - 出力が受信バッファー内に収まること。

    // Cleanup
    cplat_syslog_sink_dispose(handle);
    close(pipe_fds[0]);
    if (saved_fd != NULL)
    {
        setenv("SYSLOG_TEST_FD", saved_fd_value.c_str(), 1);
    }
    else
    {
        unsetenv("SYSLOG_TEST_FD");
    }
}

// テスト用時刻整形に失敗した場合に本文だけを書き込むことの確認
TEST_F(syslogFailureInjectionTest, write_uses_plain_line_when_test_timestamp_format_fails)
{
    // Arrange
    int pipe_fds[2];
    char actual[256];
    cplat_timespec timestamp = {1, 0};
    const char *saved_fd = getenv("SYSLOG_TEST_FD");
    std::string saved_fd_value;
    if (saved_fd != NULL)
    {
        saved_fd_value = saved_fd;
    }
    ASSERT_EQ(0, pipe(pipe_fds));                               // [状態] - pipe を生成する。
                                                                // [状態確認] - pipe の生成が成功すること。
    std::string fd_text = std::to_string(pipe_fds[1]);          // [状態] - テスト用 FD をパイプの書き込み側とする。
    ASSERT_EQ(0, setenv("SYSLOG_TEST_FD", fd_text.c_str(), 1)); // [状態] - SYSLOG_TEST_FD をパイプの書き込み側とする。
    // [状態確認] - SYSLOG_TEST_FD の setenv の戻り値が 0 であること。
    cplat_syslog_sink *handle =
        cplat_syslog_sink_create("syslogFailureInjectionTest", LOG_USER); // [状態] - syslog sink を生成する。
    ASSERT_NE((cplat_syslog_sink *)NULL, handle);                         // [状態確認] - ハンドルが非 NULL であること。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_clock_format_realtime_iso8601_local(_, _, _))
        .WillOnce(Return(CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_異常系] - 時刻整形を失敗させること。
    // [Pre-Assert手順] - cplat_clock_format_realtime_iso8601_local から CPLAT_ERR_UNKNOWN を返却する。

    // Act
    int result = cplat_syslog_sink_write(handle, CPLAT_TRACE_LEVEL_INFO, &timestamp,
                                         "message"); // [手順] - 時刻整形失敗時の書き込みを実行する。
    close(pipe_fds[1]);
    pipe_fds[1] = -1;
    ssize_t read_size = read(pipe_fds[0], actual, sizeof(actual) - 1); // [手順] - 本文だけの出力を読み取る。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result);       // [確認_正常系] - 時刻整形失敗でも本文出力が成功扱いになること。
    EXPECT_GT(read_size, 0); // [確認_正常系] - 本文が出力されること。

    // Cleanup
    cplat_syslog_sink_dispose(handle);
    close(pipe_fds[0]);
    if (saved_fd != NULL)
    {
        setenv("SYSLOG_TEST_FD", saved_fd_value.c_str(), 1);
    }
    else
    {
        unsetenv("SYSLOG_TEST_FD");
    }
}

// 識別子複製用メモリの確保に失敗した場合に OUT_OF_MEMORY が返ることの確認
TEST_F(syslogFailureInjectionTest, rename_reports_out_of_memory_when_duplication_fails)
{
    // Arrange
    cplat_syslog_sink *handle =
        cplat_syslog_sink_create("syslogFailureInjectionTest", LOG_USER); // [状態] - syslog sink を生成する。
    ASSERT_NE((cplat_syslog_sink *)NULL, handle);                         // [状態確認] - ハンドルが非 NULL であること。
    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_malloc(_))
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - 新しい識別子の確保を失敗させること。
                                    // [Pre-Assert手順] - cplat_malloc から NULL を返却する。

    // Act
    int result = cplat_syslog_sink_rename(handle, "renamed"); // [手順] - 識別子を変更する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY,
              result); // [確認_異常系] - 識別子確保失敗時の戻り値が OUT_OF_MEMORY であること。

    // Cleanup
    cplat_syslog_sink_dispose(handle);
}

// 識別子変更時のロック失敗を呼び出し元へ返すことの確認
TEST_F(syslogFailureInjectionTest, rename_reports_lock_failure)
{
    // Arrange
    cplat_syslog_sink *handle =
        cplat_syslog_sink_create("syslogFailureInjectionTest", LOG_USER); // [状態] - syslog sink を生成する。
    ASSERT_NE((cplat_syslog_sink *)NULL, handle);                         // [状態確認] - ハンドルが非 NULL であること。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_local_lock_lock(_, CPLAT_SYNC_WAIT_FOREVER))
        .WillOnce(Return(CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_異常系] - 識別子変更前のロック取得を失敗させること。
    // [Pre-Assert手順] - cplat_local_lock_lock から CPLAT_ERR_UNKNOWN を返却する。

    // Act
    int result = cplat_syslog_sink_rename(handle, "renamed"); // [手順] - ロック失敗を注入して識別子を変更する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - ロック失敗時の戻り値が CPLAT_ERR_UNKNOWN であること。

    // Cleanup
    cplat_syslog_sink_dispose(handle);
}

// shutdown 用破棄が NULL と有効なハンドルを処理することの確認
TEST_F(syslogFailureInjectionTest, dispose_on_shutdown_handles_null_and_active_sink)
{
    // Arrange
    cplat_syslog_sink *handle =
        cplat_syslog_sink_create("syslogFailureInjectionTest", LOG_USER); // [状態] - syslog sink を生成する。
    ASSERT_NE((cplat_syslog_sink *)NULL, handle);                         // [状態確認] - ハンドルが非 NULL であること。

    // Pre-Assert

    // Act
    cplat_internal_syslog_sink_dispose_on_shutdown(NULL);   // [手順] - NULL ハンドルを shutdown 破棄する。
    cplat_internal_syslog_sink_dispose_on_shutdown(handle); // [手順] - 有効なハンドルを shutdown 破棄する。

    // Assert
    SUCCEED(); // [確認_正常系] - NULL と有効なハンドルの shutdown 破棄が完了すること。
}

// ソケット未接続の sink を shutdown 時に破棄できることの確認
TEST_F(syslogFailureInjectionTest, dispose_on_shutdown_handles_disconnected_sink)
{
    // Arrange
    NiceMock<Mock_sys_socket> mock_sys_socket;

    // Pre-Assert
    EXPECT_CALL(mock_sys_socket, socket(_, _, _, _, _, _))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - 初回のソケット生成を 1 回試みること。
                               // [Pre-Assert手順] - socket から -1 を返却する。

    // Act
    cplat_syslog_sink *handle =
        cplat_syslog_sink_create("syslogFailureInjectionTest", LOG_USER); // [手順] - ソケット未接続の sink を生成する。
    cplat_internal_syslog_sink_dispose_on_shutdown(
        handle); // [手順] - ソケット未接続の sink を shutdown 経路で破棄する。

    // Assert
    EXPECT_NE((cplat_syslog_sink *)NULL,
              handle); // [確認_正常系] - ソケット生成に失敗しても cplat_syslog_sink_create の戻り値が NULL でないこと。
}

// syslog 本文の書式化に失敗した場合に成功扱いで戻ることの確認
TEST_F(syslogFailureInjectionTest, write_returns_ok_when_message_formatting_fails)
{
    // Arrange
    cplat_syslog_sink *handle =
        cplat_syslog_sink_create("syslogFailureInjectionTest", LOG_USER); // [状態] - syslog sink を生成する。
    ASSERT_NE((cplat_syslog_sink *)NULL, handle);                         // [状態確認] - ハンドルが非 NULL であること。
    NiceMock<Mock_stdio> mock_stdio;

    // Pre-Assert
    EXPECT_CALL(mock_stdio, snprintf(_, _, _, _, _, _))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - syslog 本文の書式化を失敗させること。
                               // [Pre-Assert手順] - snprintf から -1 を返却する。

    // Act
    int result = cplat_syslog_sink_write(handle, CPLAT_TRACE_LEVEL_INFO, NULL,
                                         "message"); // [手順] - 書式化失敗時の書き込みを実行する。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - 書式化失敗時の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_syslog_sink_dispose(handle);
}

// テスト用 FD の時刻付き行の書式化に失敗した場合に UNKNOWN を返すことの確認
TEST_F(syslogFailureInjectionTest, write_returns_unknown_when_test_timestamp_formatting_fails)
{
    // Arrange
    int pipe_fds[2];
    cplat_timespec timestamp = {1, 0};
    const char *saved_fd = getenv("SYSLOG_TEST_FD");
    std::string saved_fd_value;
    if (saved_fd != NULL)
    {
        saved_fd_value = saved_fd;
    }
    ASSERT_EQ(0, pipe(pipe_fds));                               // [状態] - pipe を生成する。
                                                                // [状態確認] - pipe の生成が成功すること。
    std::string fd_text = std::to_string(pipe_fds[1]);          // [状態] - テスト用 FD をパイプの書き込み側とする。
    ASSERT_EQ(0, setenv("SYSLOG_TEST_FD", fd_text.c_str(), 1)); // [状態] - SYSLOG_TEST_FD をパイプの書き込み側とする。
    // [状態確認] - SYSLOG_TEST_FD の setenv の戻り値が 0 であること。
    cplat_syslog_sink *handle =
        cplat_syslog_sink_create("syslogFailureInjectionTest", LOG_USER); // [状態] - syslog sink を生成する。
    ASSERT_NE((cplat_syslog_sink *)NULL, handle);                         // [状態確認] - ハンドルが非 NULL であること。
    NiceMock<Mock_stdio> mock_stdio;

    // Pre-Assert
    EXPECT_CALL(mock_stdio, snprintf(_, _, _, _, _, _))
        .WillOnce(DoDefault())
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - 時刻付き行の書式化を失敗させること。
                               // [Pre-Assert手順] - 1 回目は既定動作、2 回目は -1 を返却する。

    // Act
    int result = cplat_syslog_sink_write(handle, CPLAT_TRACE_LEVEL_INFO, &timestamp,
                                         "message"); // [手順] - 時刻付き行の書式化失敗を実行する。
    close(pipe_fds[1]);
    pipe_fds[1] = -1;

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - 時刻付き行の書式化失敗時の戻り値が UNKNOWN であること。

    // Cleanup
    cplat_syslog_sink_dispose(handle);
    close(pipe_fds[0]);
    if (saved_fd != NULL)
    {
        setenv("SYSLOG_TEST_FD", saved_fd_value.c_str(), 1);
    }
    else
    {
        unsetenv("SYSLOG_TEST_FD");
    }
}

// テスト用 FD の時刻付き行が長い場合に末尾を改行へ補正することの確認
TEST_F(syslogFailureInjectionTest, write_truncates_test_timestamp_line)
{
    // Arrange
    int pipe_fds[2];
    char actual[2200];
    cplat_timespec timestamp = {1, 0};
    const char *saved_fd = getenv("SYSLOG_TEST_FD");
    std::string saved_fd_value;
    if (saved_fd != NULL)
    {
        saved_fd_value = saved_fd;
    }
    ASSERT_EQ(0, pipe(pipe_fds));                               // [状態] - pipe を生成する。
                                                                // [状態確認] - pipe の生成が成功すること。
    std::string fd_text = std::to_string(pipe_fds[1]);          // [状態] - テスト用 FD をパイプの書き込み側とする。
    ASSERT_EQ(0, setenv("SYSLOG_TEST_FD", fd_text.c_str(), 1)); // [状態] - SYSLOG_TEST_FD をパイプの書き込み側とする。
    // [状態確認] - SYSLOG_TEST_FD の setenv の戻り値が 0 であること。
    cplat_syslog_sink *handle =
        cplat_syslog_sink_create("syslogFailureInjectionTest", LOG_USER); // [状態] - syslog sink を生成する。
    ASSERT_NE((cplat_syslog_sink *)NULL, handle);                         // [状態確認] - ハンドルが非 NULL であること。
    NiceMock<Mock_stdio> mock_stdio;

    // Pre-Assert
    EXPECT_CALL(mock_stdio, snprintf(_, _, _, _, _, _))
        .WillOnce(DoDefault())
        .WillOnce(Return(3000)); // [Pre-Assert確認_異常系] - 時刻付き行の長さをバッファーより大きくすること。
                                 // [Pre-Assert手順] - 1 回目は既定動作、2 回目は 3000 を返却する。

    // Act
    int result = cplat_syslog_sink_write(handle, CPLAT_TRACE_LEVEL_INFO, &timestamp,
                                         "message"); // [手順] - 長い時刻付き行を書き込む。
    close(pipe_fds[1]);
    pipe_fds[1] = -1;
    ssize_t read_size = read(pipe_fds[0], actual, sizeof(actual) - 1); // [手順] - 補正後の行を読み取る。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result);       // [確認_正常系] - 長い時刻付き行の書き込みが成功扱いになること。
    EXPECT_GT(read_size, 0); // [確認_正常系] - 補正後の行が出力されること。

    // Cleanup
    cplat_syslog_sink_dispose(handle);
    close(pipe_fds[0]);
    if (saved_fd != NULL)
    {
        setenv("SYSLOG_TEST_FD", saved_fd_value.c_str(), 1);
    }
    else
    {
        unsetenv("SYSLOG_TEST_FD");
    }
}

// 代替時刻を用いた送信が EAGAIN の場合に UNKNOWN を返すことの確認
TEST_F(syslogFailureInjectionTest, write_reports_unknown_when_fallback_send_buffer_is_full)
{
    // Arrange
    cplat_timespec invalid_timestamp = {1, 1000000000L};
    cplat_syslog_sink *handle =
        cplat_syslog_sink_create("syslogFailureInjectionTest", LOG_USER); // [状態] - syslog sink を生成する。
    ASSERT_NE((cplat_syslog_sink *)NULL, handle);                         // [状態確認] - ハンドルが非 NULL であること。
    NiceMock<Mock_sys_socket> mock_sys_socket;

    // Pre-Assert
    EXPECT_CALL(mock_sys_socket, sendto(_, _, _, _, _, _, _, _, _))
        .WillOnce(DoAll(Assign(&errno, EAGAIN),
                        Return(-1))); // [Pre-Assert確認_異常系] - 代替時刻での送信を EAGAIN にすること。
                                      // [Pre-Assert手順] - errno に EAGAIN を設定し、sendto から -1 を返却する。

    // Act
    int result = cplat_syslog_sink_write(handle, CPLAT_TRACE_LEVEL_INFO, &invalid_timestamp,
                                         "message"); // [手順] - 代替時刻で送信する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - EAGAIN 発生時の戻り値が UNKNOWN であること。

    // Cleanup
    cplat_syslog_sink_dispose(handle);
}

// 代替時刻を用いた送信が一般エラーの場合に UNKNOWN を返すことの確認
TEST_F(syslogFailureInjectionTest, write_reports_unknown_when_fallback_send_fails)
{
    // Arrange
    cplat_timespec invalid_timestamp = {1, 1000000000L};
    cplat_syslog_sink *handle =
        cplat_syslog_sink_create("syslogFailureInjectionTest", LOG_USER); // [状態] - syslog sink を生成する。
    ASSERT_NE((cplat_syslog_sink *)NULL, handle);                         // [状態確認] - ハンドルが非 NULL であること。
    NiceMock<Mock_sys_socket> mock_sys_socket;

    // Pre-Assert
    EXPECT_CALL(mock_sys_socket, sendto(_, _, _, _, _, _, _, _, _))
        .WillOnce(DoAll(Assign(&errno, ECONNREFUSED),
                        Return(-1))); // [Pre-Assert確認_異常系] - 代替時刻での送信を一般エラーにすること。
                                      // [Pre-Assert手順] - errno に ECONNREFUSED を設定し、sendto から -1 を返却する。

    // Act
    int result = cplat_syslog_sink_write(handle, CPLAT_TRACE_LEVEL_INFO, &invalid_timestamp,
                                         "message"); // [手順] - 代替時刻で送信する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - 一般送信エラー時の戻り値が UNKNOWN であること。

    // Cleanup
    cplat_syslog_sink_dispose(handle);
}

// 代替時刻の送信先ソケットが未接続の場合に UNKNOWN を返すことの確認
TEST_F(syslogFailureInjectionTest, write_reports_unknown_when_fallback_socket_is_unavailable)
{
    // Arrange
    cplat_timespec invalid_timestamp = {1, 1000000000L};
    NiceMock<Mock_sys_socket> mock_sys_socket;
    EXPECT_CALL(mock_sys_socket, socket(_, _, _, _, _, _))
        .WillOnce(Return(-1)); // [状態確認] - create 時に socket が 1 回呼び出されること。
    // [Pre-Assert確認_異常系] - mock_sys_socket の socket(_, _, _, _, _, _) が登録した呼び出し期待を満たすこと。
    cplat_syslog_sink *handle =
        cplat_syslog_sink_create("syslogFailureInjectionTest", LOG_USER); // [状態] - syslog sink を生成する。
    ASSERT_NE((cplat_syslog_sink *)NULL, handle);                         // [状態確認] - ハンドルが非 NULL であること。

    // Pre-Assert

    // Act
    int result = cplat_syslog_sink_write(handle, CPLAT_TRACE_LEVEL_INFO, &invalid_timestamp,
                                         "message"); // [手順] - 再接続抑制期間中に代替時刻で送信する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - ソケット未接続時の戻り値が UNKNOWN であること。

    // Cleanup
    cplat_syslog_sink_dispose(handle);
}

// 連続するソケット生成失敗でバックオフ上限が適用されることの確認
TEST_F(syslogFailureInjectionTest, socket_failure_caps_backoff_interval)
{
    // Arrange
    int realtime_call = 0;
    NiceMock<Mock_sys_socket> mock_sys_socket;
    ON_CALL(mock_cplat_, cplat_clock_get_realtime(_))
        .WillByDefault(Invoke(
            [&realtime_call](cplat_timespec *timestamp)
            {
                if (realtime_call == 0)
                {
                    timestamp->tv_sec = 0;
                }
                else if (realtime_call == 1)
                {
                    timestamp->tv_sec = 10;
                }
                else if (realtime_call == 2)
                {
                    timestamp->tv_sec = 20;
                }
                else
                {
                    timestamp->tv_sec = 40;
                }
                timestamp->tv_nsec = 0;
                ++realtime_call;
            })); // [状態] - cplat_clock_get_realtime が呼び出された際に 0 秒、10 秒、20 秒、40 秒を順に返すようにモックを設定する。

    // Pre-Assert
    EXPECT_CALL(mock_sys_socket, socket(_, _, _, _, _, _))
        .WillOnce(Return(-1))
        .WillOnce(Return(-1))
        .WillOnce(Return(-1))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - 連続するソケット生成を失敗させること。
                               // [Pre-Assert手順] - socket から 4 回とも -1 を返却する。

    // Act
    cplat_syslog_sink *handle =
        cplat_syslog_sink_create("syslogFailureInjectionTest", LOG_USER); // [手順] - 初回接続を試行する。
    int first_retry = cplat_syslog_sink_write(handle, CPLAT_TRACE_LEVEL_INFO, NULL,
                                              "first"); // [手順] - 1 回目の再接続を試行する。
    int second_retry = cplat_syslog_sink_write(handle, CPLAT_TRACE_LEVEL_INFO, NULL,
                                               "second"); // [手順] - 2 回目の再接続を試行する。
    int third_retry = cplat_syslog_sink_write(handle, CPLAT_TRACE_LEVEL_INFO, NULL,
                                              "third"); // [手順] - 3 回目の再接続を試行する。

    // Assert
    ASSERT_NE((cplat_syslog_sink *)NULL, handle); // [確認_正常系] - 接続失敗後もハンドルが維持されること。
    EXPECT_EQ(CPLAT_OK, first_retry);             // [確認_正常系] - 1 回目の再接続失敗が破棄扱いになること。
    EXPECT_EQ(CPLAT_OK, second_retry);            // [確認_正常系] - 2 回目の再接続失敗が破棄扱いになること。
    EXPECT_EQ(CPLAT_OK, third_retry);             // [確認_正常系] - 3 回目の再接続失敗が破棄扱いになること。

    // Cleanup
    cplat_syslog_sink_dispose(handle);
}

#endif /* PLATFORM_LINUX */
