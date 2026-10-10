#include <cplat/base/platform.h>
#include <mock_cplat.h>

#if defined(PLATFORM_LINUX)

    #include <cstdlib>
    #include <cstdio>
    #include <ctime>
    #include <string>
    #include <unistd.h>
    #include <syslog.h>
    #include <testfw.h>
    #include <cplat/trace/syslog.h>

class trace_syslogTest : public Test
{
};

// プロバイダーを初期化し、有効なハンドルが返されることの確認
TEST_F(trace_syslogTest, test_init_and_dispose)
{
    // Arrange

    // Pre-Assert

    // Act
    cplat_syslog_sink *handle = cplat_syslog_sink_create(
        "syslog_test", LOG_USER);      // [手順] - ident "syslog_test"、facility LOG_USER で syslog sink を初期化する。
    cplat_syslog_sink_dispose(handle); // [手順] - syslog sink を破棄する。

    // Assert
    EXPECT_NE((cplat_syslog_sink *)NULL, handle); // [確認_正常系] - ハンドルが NULL でないこと。
}

// syslog sink の識別子を変更できることの確認
TEST_F(trace_syslogTest, rename_succeeds)
{
    // Arrange
    cplat_syslog_sink *handle =
        cplat_syslog_sink_create("before", LOG_USER); // [状態] - 変更前の識別子を "before" とする。
    ASSERT_NE((cplat_syslog_sink *)NULL, handle);     // [状態確認] - ハンドルが非 NULL であること。

    // Pre-Assert

    // Act
    int result = cplat_syslog_sink_rename(handle, "after"); // [手順] - syslog sink の識別子を "after" へ変更する。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_syslog_sink_rename の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_syslog_sink_dispose(handle);
}

// syslog sink の識別子変更が不正引数を拒否することの確認
TEST_F(trace_syslogTest, rename_rejects_invalid_arguments)
{
    // Arrange
    cplat_syslog_sink *handle =
        cplat_syslog_sink_create("syslog_test", LOG_USER); // [状態] - 初期化済みの syslog sink を用意する。
    ASSERT_NE((cplat_syslog_sink *)NULL, handle);          // [状態確認] - ハンドルが非 NULL であること。

    // Pre-Assert

    // Act
    int null_handle_result =
        cplat_syslog_sink_rename(NULL, "after"); // [手順] - handle に NULL を渡して識別子を変更する。
    int null_ident_result =
        cplat_syslog_sink_rename(handle, NULL); // [手順] - new_ident に NULL を渡して識別子を変更する。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_handle_result); // [確認_異常系] - handle が NULL の呼び出しの戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_ident_result); // [確認_異常系] - new_ident が NULL の呼び出しの戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。

    // Cleanup
    cplat_syslog_sink_dispose(handle);
}

// INFO レベルでメッセージを書き込み、成功することの確認
TEST_F(trace_syslogTest, test_write_returns_zero)
{
    // Arrange
    cplat_syslog_sink *handle =
        cplat_syslog_sink_create("syslog_test", LOG_USER); // [状態] - 初期化済みの syslog sink を用意する。
    ASSERT_NE((cplat_syslog_sink *)NULL, handle);          // [状態確認] - ハンドルが非 NULL であること。

    // Pre-Assert

    // Act
    int result = cplat_syslog_sink_write(handle, LOG_INFO, NULL,
                                         "test message"); // [手順] - LOG_INFO レベルで "test message" を書き込む。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_syslog_sink_write の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_syslog_sink_dispose(handle);
}

// 全レベルで書き込みが成功することの確認
TEST_F(trace_syslogTest, test_write_all_levels)
{
    // Arrange
    cplat_syslog_sink *handle =
        cplat_syslog_sink_create("syslog_test", LOG_USER); // [状態] - 初期化済みの syslog sink を用意する。
    ASSERT_NE((cplat_syslog_sink *)NULL, handle);          // [状態確認] - ハンドルが非 NULL であること。

    // Pre-Assert

    // Act
    // [手順] - CRIT、ERR、WARNING、INFO、DEBUG の各レベルで書き込む。
    int actual_ret_critical = cplat_syslog_sink_write(handle, LOG_CRIT, NULL, "critical");
    int actual_ret_error = cplat_syslog_sink_write(handle, LOG_ERR, NULL, "error");
    int actual_ret_warning = cplat_syslog_sink_write(handle, LOG_WARNING, NULL, "warning");
    int actual_ret_info = cplat_syslog_sink_write(handle, LOG_INFO, NULL, "info");
    int actual_ret_debug = cplat_syslog_sink_write(handle, LOG_DEBUG, NULL, "debug");

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret_critical); // [確認_正常系] - CRIT レベルで書き込めること。
    EXPECT_EQ(CPLAT_OK, actual_ret_error);    // [確認_正常系] - ERR レベルで書き込めること。
    EXPECT_EQ(CPLAT_OK, actual_ret_warning);  // [確認_正常系] - WARNING レベルで書き込めること。
    EXPECT_EQ(CPLAT_OK, actual_ret_info);     // [確認_正常系] - INFO レベルで書き込めること。
    EXPECT_EQ(CPLAT_OK, actual_ret_debug);    // [確認_正常系] - DEBUG レベルで書き込めること。

    // Cleanup
    cplat_syslog_sink_dispose(handle);
}

// SYSLOG_TEST_FD 経路ではタイムスタンプ付き 1 行が書き込まれることの確認
TEST_F(trace_syslogTest, test_write_to_test_fd_prefixes_timestamp)
{
    // Arrange
    int pipe_fds[2];
    char fd_text[32];
    char actual[256];
    char expected[256];
    ssize_t nread;
    const char *saved_tz = getenv("TZ");
    const char *saved_fd = getenv("SYSLOG_TEST_FD");
    std::string saved_tz_value;
    std::string saved_fd_value;
    if (saved_tz != NULL)
    {
        saved_tz_value = saved_tz;
    }
    if (saved_fd != NULL)
    {
        saved_fd_value = saved_fd;
    }
    cplat_timespec timestamp = {
        1412916640LL, 0}; // [状態] - 明示タイムスタンプを 2014-10-10T13:50:40+09:00 相当の {1412916640, 0} とする。

    ASSERT_EQ(0, pipe(pipe_fds));                          // [状態] - pipe を生成する。
                                                           // [状態確認] - pipe の生成が成功すること。
    ASSERT_EQ(0, setenv("TZ", "Asia/Tokyo", 1));           // [状態] - TZ を Asia/Tokyo とする。
                                                           // [状態確認] - TZ の setenv の戻り値が 0 であること。
    tzset();                                               // [状態] - タイム ゾーンを Asia/Tokyo とする。
    snprintf(fd_text, sizeof(fd_text), "%d", pipe_fds[1]); // [状態] - テスト用 FD をパイプの書き込み側とする。
    ASSERT_EQ(0, setenv("SYSLOG_TEST_FD", fd_text,
                        1)); // [状態] - SYSLOG_TEST_FD をパイプの書き込み側とする。
                             // [状態確認] - SYSLOG_TEST_FD の setenv の戻り値が 0 であること。

    cplat_syslog_sink *handle =
        cplat_syslog_sink_create("syslog_test", LOG_USER); // [状態] - 初期化済みの syslog sink を用意する。
    ASSERT_NE((cplat_syslog_sink *)NULL, handle);          // [状態確認] - ハンドルが非 NULL であること。

    // Pre-Assert

    // Act
    int actual_ret_syslog_sink_write = cplat_syslog_sink_write(
        handle, LOG_INFO, &timestamp, "test message"); // [手順] - 明示タイムスタンプ付きで "test message" を書き込む。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret_syslog_sink_write); // [確認_正常系] - cplat_syslog_sink_write の戻り値が CPLAT_OK であること。

    close(pipe_fds[1]);
    pipe_fds[1] = -1;
    nread = read(pipe_fds[0], actual, sizeof(actual) - 1); // [手順] - pipe から書き込まれた 1 行を読み取る。
    ASSERT_GT(nread, 0);
    // [確認_正常系] - `nread` が `0` より大きいこと。
    actual[nread] = '\0';

    snprintf(expected, sizeof(expected), "2014-10-10T13:50:40.000+09:00 <14>syslog_test[%d]: test message\n",
             (int)getpid());
    EXPECT_STREQ(expected, actual); // [確認_正常系] - ISO 8601 タイムスタンプ付きの 1 行が書き込まれていること。

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
    if (saved_tz != NULL)
    {
        setenv("TZ", saved_tz_value.c_str(), 1);
    }
    else
    {
        unsetenv("TZ");
    }
    tzset();
}

// 不正な明示タイムスタンプ指定時に現在時刻へ代替して出力しつつ -1 を返すことの確認
TEST_F(trace_syslogTest, test_write_to_test_fd_falls_back_from_invalid_explicit_timestamp)
{
    // Arrange
    int pipe_fds[2];
    char fd_text[32];
    char actual[256];
    char expected[256];
    ssize_t nread;
    const char *saved_tz = getenv("TZ");
    const char *saved_fd = getenv("SYSLOG_TEST_FD");
    std::string saved_tz_value;
    std::string saved_fd_value;
    if (saved_tz != NULL)
    {
        saved_tz_value = saved_tz;
    }
    if (saved_fd != NULL)
    {
        saved_fd_value = saved_fd;
    }
    cplat_timespec invalid_timestamp = {1714100645LL,
                                        1000000000}; // [状態] - nsec が 10 億の不正な明示タイムスタンプを用意する。

    ASSERT_EQ(0, pipe(pipe_fds));                          // [状態] - pipe を生成する。
                                                           // [状態確認] - pipe の生成が成功すること。
    ASSERT_EQ(0, setenv("TZ", "Asia/Tokyo", 1));           // [状態] - TZ を Asia/Tokyo とする。
                                                           // [状態確認] - TZ の setenv の戻り値が 0 であること。
    tzset();                                               // [状態] - タイム ゾーンを Asia/Tokyo とする。
    snprintf(fd_text, sizeof(fd_text), "%d", pipe_fds[1]); // [状態] - テスト用 FD をパイプの書き込み側とする。
    ASSERT_EQ(0, setenv("SYSLOG_TEST_FD", fd_text,
                        1)); // [状態] - SYSLOG_TEST_FD をパイプの書き込み側とする。
                             // [状態確認] - SYSLOG_TEST_FD の setenv の戻り値が 0 であること。

    cplat_syslog_sink *handle =
        cplat_syslog_sink_create("syslog_test", LOG_USER); // [状態] - 初期化済みの syslog sink を用意する。
    ASSERT_NE((cplat_syslog_sink *)NULL, handle);          // [状態確認] - ハンドルが非 NULL であること。

    // Pre-Assert

    // Act
    int actual_ret_syslog_sink_write =
        cplat_syslog_sink_write(handle, LOG_INFO, &invalid_timestamp,
                                "invalid ts"); // [手順] - 不正な明示タイムスタンプ付きで "invalid ts" を書き込む。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret_syslog_sink_write); // [確認_異常系] - cplat_syslog_sink_write の戻り値が CPLAT_ERR_UNKNOWN であること。

    close(pipe_fds[1]);
    pipe_fds[1] = -1;
    nread = read(pipe_fds[0], actual, sizeof(actual) - 1); // [手順] - pipe から書き込まれた 1 行を読み取る。
    ASSERT_GT(nread, 0);
    // [確認_異常系] - `nread` が `0` より大きいこと。
    actual[nread] = '\0';

    snprintf(expected, sizeof(expected), "<14>syslog_test[%d]: invalid ts\n", (int)getpid());
    EXPECT_NE(std::string::npos, std::string(actual).find(expected)); // [確認_異常系] - syslog 本文が出力されること。
    EXPECT_NE('<', actual[0]); // [確認_異常系] - 先頭に現在時刻のタイムスタンプが付与されること。

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
    if (saved_tz != NULL)
    {
        setenv("TZ", saved_tz_value.c_str(), 1);
    }
    else
    {
        unsetenv("TZ");
    }
    tzset();
}

// NULL 引数が安全に無視されることの確認
TEST_F(trace_syslogTest, test_null_arguments_are_safe)
{
    // Arrange
    cplat_syslog_sink *handle =
        cplat_syslog_sink_create("syslog_test", LOG_USER); // [状態] - 初期化済みの syslog sink を用意する。

    // Pre-Assert

    // Act
    cplat_syslog_sink_dispose(NULL); // [手順] - NULL ハンドルで dispose を呼び出す。

    // Assert
    EXPECT_EQ(
        (cplat_syslog_sink *)NULL,
        cplat_syslog_sink_create(
            NULL,
            LOG_USER)); // [確認_異常系] - cplat_syslog_sink_create の戻り値から、NULL ident で create が失敗したと判断できること。
    EXPECT_EQ(
        CPLAT_OK,
        cplat_syslog_sink_write(
            NULL, LOG_INFO, NULL,
            "test message")); // [確認_異常系] - NULL ハンドルに対する cplat_syslog_sink_write の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        cplat_syslog_sink_write(
            handle, LOG_INFO, NULL,
            NULL)); // [確認_異常系] - NULL message に対する cplat_syslog_sink_write の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_syslog_sink_dispose(handle);
}

// SYSLOG_TEST_FD は生成時に確定し、生成後の環境変数の変更が書き込み先に影響しないことの確認
TEST_F(trace_syslogTest, test_fd_is_fixed_at_create)
{
    // Arrange
    int pipe_fds[2];
    char fd_text[32];
    char actual[256];
    char expected[256];
    ssize_t nread;
    const char *saved_fd = getenv("SYSLOG_TEST_FD");
    std::string saved_fd_value;
    if (saved_fd != NULL)
    {
        saved_fd_value = saved_fd;
    }

    ASSERT_EQ(0, pipe(pipe_fds));                          // [状態] - pipe を生成する。
                                                           // [状態確認] - pipe の生成が成功すること。
    snprintf(fd_text, sizeof(fd_text), "%d", pipe_fds[1]); // [状態] - テスト用 FD をパイプの書き込み側とする。
    ASSERT_EQ(0, setenv("SYSLOG_TEST_FD", fd_text,
                        1)); // [状態] - SYSLOG_TEST_FD をパイプの書き込み側とする。
                             // [状態確認] - SYSLOG_TEST_FD の setenv の戻り値が 0 であること。

    cplat_syslog_sink *handle =
        cplat_syslog_sink_create("syslog_test", LOG_USER); // [状態] - 初期化済みの syslog sink を用意する。
    ASSERT_NE((cplat_syslog_sink *)NULL, handle);          // [状態確認] - ハンドルが非 NULL であること。
    ASSERT_EQ(0, setenv("SYSLOG_TEST_FD", "-1",
                        1)); // [状態] - 生成後に SYSLOG_TEST_FD を書き込み先のない値へ変更する。
                             // [状態確認] - SYSLOG_TEST_FD の setenv の戻り値が 0 であること。

    // Pre-Assert

    // Act
    int actual_ret_syslog_sink_write = cplat_syslog_sink_write(
        handle, LOG_INFO, NULL, "fixed message"); // [手順] - タイムスタンプなしで "fixed message" を書き込む。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret_syslog_sink_write); // [確認_正常系] - cplat_syslog_sink_write の戻り値が CPLAT_OK であること。

    close(pipe_fds[1]);
    pipe_fds[1] = -1;
    nread = read(pipe_fds[0], actual, sizeof(actual) - 1); // [手順] - pipe から書き込まれた 1 行を読み取る。
    ASSERT_GT(nread, 0);
    // [確認_正常系] - `nread` が `0` より大きいこと。
    actual[nread] = '\0';

    snprintf(expected, sizeof(expected), "<14>syslog_test[%d]: fixed message\n", (int)getpid());
    EXPECT_STREQ(expected, actual); // [確認_正常系] - 生成時の FD へ 1 行が書き込まれていること。

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

// SYSLOG_TEST_FD を FD として解釈できない場合も、書き込みが成功扱いになることの確認
TEST_F(trace_syslogTest, test_fd_invalid_value_is_ignored)
{
    // Arrange
    const char *saved_fd = getenv("SYSLOG_TEST_FD");
    std::string saved_fd_value;
    if (saved_fd != NULL)
    {
        saved_fd_value = saved_fd;
    }

    ASSERT_EQ(0, setenv("SYSLOG_TEST_FD", "not-a-number",
                        1)); // [状態] - SYSLOG_TEST_FD を FD として解釈できない値とする。
                             // [状態確認] - SYSLOG_TEST_FD の setenv の戻り値が 0 であること。

    cplat_syslog_sink *handle =
        cplat_syslog_sink_create("syslog_test", LOG_USER); // [状態] - 初期化済みの syslog sink を用意する。
    ASSERT_NE((cplat_syslog_sink *)NULL, handle);          // [状態確認] - ハンドルが非 NULL であること。

    // Pre-Assert

    // Act
    int actual_ret_syslog_sink_write =
        cplat_syslog_sink_write(handle, LOG_INFO, NULL, "ignored message"); // [手順] - "ignored message" を書き込む。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret_syslog_sink_write); // [確認_正常系] - cplat_syslog_sink_write の戻り値が CPLAT_OK であること。

    // Cleanup
    cplat_syslog_sink_dispose(handle);
    if (saved_fd != NULL)
    {
        setenv("SYSLOG_TEST_FD", saved_fd_value.c_str(), 1);
    }
    else
    {
        unsetenv("SYSLOG_TEST_FD");
    }
}

#endif /* PLATFORM_ */
