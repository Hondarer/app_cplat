#include <testfw.h>
#include <mock_cplat.h>
#include <cplat/prompt/prompt_internal.h>

#if defined(PLATFORM_WINDOWS)

    #include <mock_windows.h>

class promptWindowsTest : public Test
{
  protected:
    cplat_prompt handle_ = {};
};

/*
 * cplat_internal_prompt_platform_enter_raw / cplat_internal_prompt_platform_leave_raw
 */

// 端末でない標準入力では raw モードへ移行しないことの確認
TEST_F(promptWindowsTest, enter_raw_does_nothing_for_non_terminal)
{
    // Arrange
    Mock_windows mock_windows;
    HANDLE dummy_handle = (HANDLE)0x1234;

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_INPUT_HANDLE))
        .WillOnce(Return(
            dummy_handle)); // [Pre-Assert確認_正常系] - GetStdHandle() が STD_INPUT_HANDLE を指定して 1 回呼び出されること。
                            // [Pre-Assert手順] - GetStdHandle() からダミー ハンドルを返す。
    EXPECT_CALL(mock_windows, GetConsoleMode(_, _, _, dummy_handle, _))
        .WillOnce(Return(
            FALSE)); // [Pre-Assert確認_異常系] - GetConsoleMode() が dummy_handle を指定して 1 回呼び出されること。
                     // [Pre-Assert手順] - GetConsoleMode() から FALSE を返す。

    // Act
    cplat_internal_prompt_platform_enter_raw(
        &handle_); // [手順] - cplat_internal_prompt_platform_enter_raw() を呼び出す。

    // Assert
    EXPECT_EQ(
        0,
        handle_
            .raw_active); // [確認_異常系] - handle_.raw_active が 0 であること。(GetConsoleMode() の失敗で raw モードへ移行しないことの確認)
}

// 標準入力ハンドルが無効な場合に raw モードへ移行しないことの確認
TEST_F(promptWindowsTest, enter_raw_does_nothing_when_stdin_handle_invalid)
{
    // Arrange
    Mock_windows mock_windows;

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_INPUT_HANDLE))
        .WillOnce(Return(
            INVALID_HANDLE_VALUE)); // [Pre-Assert確認_異常系] - GetStdHandle() が STD_INPUT_HANDLE を指定して 1 回呼び出されること。
                                    // [Pre-Assert手順] - GetStdHandle() から INVALID_HANDLE_VALUE を返す。
    EXPECT_CALL(mock_windows, GetConsoleMode(_, _, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - GetConsoleMode() が呼び出されないこと。

    // Act
    cplat_internal_prompt_platform_enter_raw(
        &handle_); // [手順] - cplat_internal_prompt_platform_enter_raw() を呼び出す。

    // Assert
    EXPECT_EQ(
        0,
        handle_
            .raw_active); // [確認_異常系] - handle_.raw_active が 0 であること。(標準入力ハンドルが無効で raw モードへ移行しないことの確認)
}

// 端末に対して raw モードへ移行できることと渡されるモード値が正しいことの確認
TEST_F(promptWindowsTest, enter_raw_succeeds_and_changes_mode)
{
    // Arrange
    Mock_windows mock_windows;
    HANDLE dummy_handle = (HANDLE)0x1234;
    DWORD orig_mode = ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT;
    DWORD captured_mode = 0;

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_INPUT_HANDLE))
        .WillOnce(Return(
            dummy_handle)); // [Pre-Assert確認_正常系] - GetStdHandle() が STD_INPUT_HANDLE を指定して 1 回呼び出されること。
                            // [Pre-Assert手順] - GetStdHandle() からダミー ハンドルを返す。
    EXPECT_CALL(mock_windows, GetConsoleMode(_, _, _, dummy_handle, _))
        .WillOnce(DoAll(
            SetArgPointee<4>(orig_mode),
            Return(
                TRUE))); // [Pre-Assert確認_正常系] - GetConsoleMode() が dummy_handle を指定して 1 回呼び出されること。
                         // [Pre-Assert手順] - GetConsoleMode() から、元のコンソール モードを設定して TRUE を返す。
    EXPECT_CALL(mock_windows, SetConsoleMode(_, _, _, dummy_handle, _))
        .WillOnce(DoAll(
            SaveArg<4>(&captured_mode),
            Return(
                TRUE))); // [Pre-Assert確認_正常系] - SetConsoleMode() が dummy_handle を指定して 1 回呼び出されること。
                         // [Pre-Assert手順] - SetConsoleMode() から TRUE を返す。

    // Act
    cplat_internal_prompt_platform_enter_raw(
        &handle_); // [手順] - cplat_internal_prompt_platform_enter_raw() を呼び出す。

    // Assert
    EXPECT_EQ(
        1,
        handle_.raw_active); // [確認_正常系] - handle_.raw_active が 1 であること。(raw モードが有効になったことの確認)
    EXPECT_NE(
        0U,
        captured_mode &
            (DWORD)
                ENABLE_VIRTUAL_TERMINAL_INPUT); // [確認_正常系] - captured_mode & ENABLE_VIRTUAL_TERMINAL_INPUT が 0 以外であること。(仮想端末入力が有効化されたことの確認)
    EXPECT_EQ(
        0U,
        captured_mode &
            (DWORD)(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT |
                    ENABLE_PROCESSED_INPUT)); // [確認_正常系] - captured_mode & (ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT) が 0 であること。(エコー・行入力・Ctrl+C シグナル化が無効化されたことの確認)
}

// SetConsoleMode が失敗した場合に raw モードへ移行しないことの確認
TEST_F(promptWindowsTest, enter_raw_does_nothing_when_set_console_mode_fails)
{
    // Arrange
    Mock_windows mock_windows;
    HANDLE dummy_handle = (HANDLE)0x1234;
    DWORD orig_mode = 0;

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_INPUT_HANDLE))
        .WillOnce(Return(
            dummy_handle)); // [Pre-Assert確認_正常系] - GetStdHandle() が STD_INPUT_HANDLE を指定して 1 回呼び出されること。
                            // [Pre-Assert手順] - GetStdHandle() からダミー ハンドルを返す。
    EXPECT_CALL(mock_windows, GetConsoleMode(_, _, _, dummy_handle, _))
        .WillOnce(DoAll(
            SetArgPointee<4>(orig_mode),
            Return(
                TRUE))); // [Pre-Assert確認_正常系] - GetConsoleMode() が dummy_handle を指定して 1 回呼び出されること。
                         // [Pre-Assert手順] - GetConsoleMode() から、元のコンソール モードを設定して TRUE を返す。
    EXPECT_CALL(mock_windows, SetConsoleMode(_, _, _, dummy_handle, _))
        .WillOnce(Return(
            FALSE)); // [Pre-Assert確認_異常系] - SetConsoleMode() が dummy_handle を指定して 1 回呼び出されること。
                     // [Pre-Assert手順] - SetConsoleMode() から FALSE を返す。

    // Act
    cplat_internal_prompt_platform_enter_raw(
        &handle_); // [手順] - cplat_internal_prompt_platform_enter_raw() を呼び出す。

    // Assert
    EXPECT_EQ(
        0,
        handle_
            .raw_active); // [確認_異常系] - handle_.raw_active が 0 であること。(SetConsoleMode() の失敗で raw モードが有効にならないことの確認)
}

// raw モード中の再入で二重に移行しないことの確認
TEST_F(promptWindowsTest, enter_raw_is_ignored_while_already_raw)
{
    // Arrange
    handle_.raw_active = 1; // [状態] - すでに raw モードとして扱う。

    // Pre-Assert

    // Act
    cplat_internal_prompt_platform_enter_raw(
        &handle_); // [手順] - raw モード中に cplat_internal_prompt_platform_enter_raw() を呼び出す。

    // Assert
    EXPECT_EQ(
        1,
        handle_
            .raw_active); // [確認_正常系] - handle_.raw_active が 1 であること。(raw モードの状態が変化しないことの確認)

    // Cleanup
    handle_.raw_active = 0;
}

// raw モードでないときの復帰が何もしないことの確認
TEST_F(promptWindowsTest, leave_raw_is_ignored_when_not_raw)
{
    // Arrange
    handle_.raw_active = 0; // [状態] - raw モードでない状態とする。

    // Pre-Assert

    // Act
    cplat_internal_prompt_platform_leave_raw(
        &handle_); // [手順] - cplat_internal_prompt_platform_leave_raw() を呼び出す。

    // Assert
    EXPECT_EQ(
        0,
        handle_
            .raw_active); // [確認_正常系] - handle_.raw_active が 0 であること。(raw モードの状態が変化しないことの確認)
}

// raw モードからの復帰時に保存済みのコンソール モードで復元されることの確認
TEST_F(promptWindowsTest, leave_raw_restores_mode)
{
    // Arrange
    Mock_windows mock_windows;
    HANDLE dummy_handle = (HANDLE)0x1234;
    DWORD captured_mode = 0;
    handle_.raw_active = 1;
    handle_.stdin_handle = dummy_handle;
    handle_.orig_in_mode =
        ENABLE_PROCESSED_INPUT; // [状態] - raw モード中で、復元対象のモードを保持している状態にする。

    // Pre-Assert
    EXPECT_CALL(mock_windows, SetConsoleMode(_, _, _, dummy_handle, _))
        .WillOnce(DoAll(
            SaveArg<4>(&captured_mode),
            Return(
                TRUE))); // [Pre-Assert確認_正常系] - SetConsoleMode() が dummy_handle を指定して 1 回呼び出されること。
                         // [Pre-Assert手順] - SetConsoleMode() から TRUE を返す。

    // Act
    cplat_internal_prompt_platform_leave_raw(
        &handle_); // [手順] - cplat_internal_prompt_platform_leave_raw() を呼び出す。

    // Assert
    EXPECT_EQ(
        0,
        handle_.raw_active); // [確認_正常系] - handle_.raw_active が 0 であること。(raw モードが無効になったことの確認)
    EXPECT_EQ(handle_.orig_in_mode,
              captured_mode); // [確認_正常系] - captured_mode が handle_.orig_in_mode と一致すること。
}

/*
 * cplat_internal_prompt_platform_read_char
 */

// 標準入力から 1 バイトが読み取れることの確認
TEST_F(promptWindowsTest, read_char_returns_next_byte)
{
    // Arrange
    Mock_windows mock_windows;
    HANDLE dummy_handle = (HANDLE)0x1234;
    char byte_value = 'A';
    handle_.stdin_handle = dummy_handle;

    // Pre-Assert
    EXPECT_CALL(mock_windows, WaitForSingleObject(_, _, _, dummy_handle, 100U))
        .WillOnce(Return(
            WAIT_OBJECT_0)); // [Pre-Assert確認_正常系] - WaitForSingleObject() が dummy_handle、100 を指定して 1 回呼び出されること。
                             // [Pre-Assert手順] - WaitForSingleObject() から WAIT_OBJECT_0 を返す。
    EXPECT_CALL(mock_windows, ReadConsoleW(_, _, _, dummy_handle, _, 1U, _, _))
        .WillOnce(
            [byte_value](const char *, int, const char *, HANDLE, LPVOID buffer, DWORD, LPDWORD bytes_read,
                         PCONSOLE_READCONSOLE_CONTROL)
            {
                *static_cast<WCHAR *>(buffer) = byte_value;
                *bytes_read = 1UL;
                return TRUE;
            }); // [Pre-Assert確認_正常系] - ReadConsoleW() が dummy_handle、1 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - ReadConsoleW() から、出力バッファーへ 'A' を書き込み、読み取りバイト数に 1 を設定して TRUE を返す。

    // Act
    int actual_ret = cplat_internal_prompt_platform_read_char(
        &handle_); // [手順] - cplat_internal_prompt_platform_read_char() を呼び出す。

    // Assert
    EXPECT_EQ('A',
              actual_ret); // [確認_正常系] - cplat_internal_prompt_platform_read_char() の戻り値が 'A' であること。
}

// 標準入力が閉じられた場合に EOF が返ることの確認
TEST_F(promptWindowsTest, read_char_returns_minus1_at_eof)
{
    // Arrange
    Mock_windows mock_windows;
    HANDLE dummy_handle = (HANDLE)0x1234;
    handle_.stdin_handle = dummy_handle;

    // Pre-Assert
    EXPECT_CALL(mock_windows, WaitForSingleObject(_, _, _, dummy_handle, 100U))
        .WillOnce(Return(
            WAIT_OBJECT_0)); // [Pre-Assert確認_正常系] - WaitForSingleObject() が dummy_handle、100 を指定して 1 回呼び出されること。
                             // [Pre-Assert手順] - WaitForSingleObject() から WAIT_OBJECT_0 を返す。
    EXPECT_CALL(mock_windows, ReadConsoleW(_, _, _, dummy_handle, _, 1U, _, _))
        .WillOnce(DoAll(
            SetArgPointee<6>(0UL),
            Return(TRUE))); // [Pre-Assert確認_異常系] - ReadConsoleW() が dummy_handle、1 を指定して 1 回呼び出されること。
                            // [Pre-Assert手順] - ReadConsoleW() から、読み取りバイト数に 0 を設定して TRUE を返す。

    // Act
    int actual_ret = cplat_internal_prompt_platform_read_char(
        &handle_); // [手順] - cplat_internal_prompt_platform_read_char() を呼び出す。

    // Assert
    EXPECT_EQ(
        -1,
        actual_ret); // [確認_異常系] - cplat_internal_prompt_platform_read_char() の戻り値が -1 であること。(EOF であることの確認)
}

// 入力がない場合に待機がタイムアウトしリサイズ チェック通知が返ることの確認
TEST_F(promptWindowsTest, read_char_returns_minus2_on_timeout)
{
    // Arrange
    Mock_windows mock_windows;
    HANDLE dummy_handle = (HANDLE)0x1234;
    handle_.stdin_handle = dummy_handle;

    // Pre-Assert
    EXPECT_CALL(mock_windows, WaitForSingleObject(_, _, _, dummy_handle, 100U))
        .WillOnce(Return(
            WAIT_TIMEOUT)); // [Pre-Assert確認_正常系] - WaitForSingleObject() が dummy_handle、100 を指定して 1 回呼び出されること。
                            // [Pre-Assert手順] - WaitForSingleObject() から WAIT_TIMEOUT を返す。
    EXPECT_CALL(mock_windows, ReadConsoleW(_, _, _, _, _, _, _, _))
        .Times(0); // [Pre-Assert確認_正常系] - ReadConsoleW() が呼び出されないこと。

    // Act
    int actual_ret = cplat_internal_prompt_platform_read_char(
        &handle_); // [手順] - cplat_internal_prompt_platform_read_char() を呼び出す。

    // Assert
    EXPECT_EQ(
        -2,
        actual_ret); // [確認_正常系] - cplat_internal_prompt_platform_read_char() の戻り値が -2 であること。(リサイズ チェック用タイムアウトであることの確認)
}

// 待機に失敗した場合に読み取り失敗が返ることの確認
TEST_F(promptWindowsTest, read_char_returns_minus1_when_wait_fails)
{
    // Arrange
    Mock_windows mock_windows;
    HANDLE dummy_handle = (HANDLE)0x1234;
    handle_.stdin_handle = dummy_handle;

    // Pre-Assert
    EXPECT_CALL(mock_windows, WaitForSingleObject(_, _, _, dummy_handle, 100U))
        .WillOnce(Return(
            WAIT_FAILED)); // [Pre-Assert確認_異常系] - WaitForSingleObject() が dummy_handle、100 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - WaitForSingleObject() から、WAIT_TIMEOUT でも WAIT_OBJECT_0 でもない WAIT_FAILED を返す。
    EXPECT_CALL(mock_windows, ReadConsoleW(_, _, _, _, _, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - ReadConsoleW() が呼び出されないこと。

    // Act
    int actual_ret = cplat_internal_prompt_platform_read_char(
        &handle_); // [手順] - cplat_internal_prompt_platform_read_char() を呼び出す。

    // Assert
    EXPECT_EQ(
        -1,
        actual_ret); // [確認_異常系] - cplat_internal_prompt_platform_read_char() の戻り値が -1 であること。(読み取り失敗であることの確認)
}

/*
 * cplat_internal_prompt_platform_read_char_nb
 */

// 入力がある場合に 1 バイトが読み取れることの確認
TEST_F(promptWindowsTest, read_char_nb_returns_next_byte_when_available)
{
    // Arrange
    Mock_windows mock_windows;
    HANDLE dummy_handle = (HANDLE)0x1234;
    char byte_value = 'B';
    handle_.stdin_handle = dummy_handle;

    // Pre-Assert
    EXPECT_CALL(mock_windows, WaitForSingleObject(_, _, _, dummy_handle, 50U))
        .WillOnce(Return(
            WAIT_OBJECT_0)); // [Pre-Assert確認_正常系] - WaitForSingleObject() が dummy_handle、50 を指定して 1 回呼び出されること。
                             // [Pre-Assert手順] - WaitForSingleObject() から WAIT_OBJECT_0 を返す。
    EXPECT_CALL(mock_windows, WaitForSingleObject(_, _, _, dummy_handle, 100U))
        .WillOnce(Return(
            WAIT_OBJECT_0)); // [Pre-Assert確認_正常系] - WaitForSingleObject() が dummy_handle、100 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - WaitForSingleObject() から WAIT_OBJECT_0 を返す。
    EXPECT_CALL(mock_windows, ReadConsoleW(_, _, _, dummy_handle, _, 1U, _, _))
        .WillOnce(
            [byte_value](const char *, int, const char *, HANDLE, LPVOID buffer, DWORD, LPDWORD bytes_read,
                         PCONSOLE_READCONSOLE_CONTROL)
            {
                *static_cast<WCHAR *>(buffer) = byte_value;
                *bytes_read = 1UL;
                return TRUE;
            }); // [Pre-Assert確認_正常系] - ReadConsoleW() が dummy_handle、1 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - ReadConsoleW() から、出力バッファーへ 'B' を書き込み、読み取りバイト数に 1 を設定して TRUE を返す。

    // Act
    int actual_ret = cplat_internal_prompt_platform_read_char_nb(
        &handle_); // [手順] - cplat_internal_prompt_platform_read_char_nb() を呼び出す。

    // Assert
    EXPECT_EQ('B',
              actual_ret); // [確認_正常系] - cplat_internal_prompt_platform_read_char_nb() の戻り値が 'B' であること。
}

// 入力がない場合にタイムアウトすることの確認
TEST_F(promptWindowsTest, read_char_nb_returns_minus1_on_timeout)
{
    // Arrange
    Mock_windows mock_windows;
    HANDLE dummy_handle = (HANDLE)0x1234;
    handle_.stdin_handle = dummy_handle;

    // Pre-Assert
    EXPECT_CALL(mock_windows, WaitForSingleObject(_, _, _, dummy_handle, 50U))
        .WillOnce(Return(
            WAIT_TIMEOUT)); // [Pre-Assert確認_異常系] - WaitForSingleObject() が dummy_handle、50 を指定して 1 回呼び出されること。
                            // [Pre-Assert手順] - WaitForSingleObject() から WAIT_TIMEOUT を返す。
    EXPECT_CALL(mock_windows, WaitForSingleObject(_, _, _, dummy_handle, 100U))
        .Times(
            0); // [Pre-Assert確認_異常系] - dummy_handle、100 を指定した WaitForSingleObject() の呼び出しがないこと。
    EXPECT_CALL(mock_windows, ReadConsoleW(_, _, _, _, _, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - ReadConsoleW() が呼び出されないこと。

    // Act
    int actual_ret = cplat_internal_prompt_platform_read_char_nb(
        &handle_); // [手順] - cplat_internal_prompt_platform_read_char_nb() を呼び出す。

    // Assert
    EXPECT_EQ(
        -1,
        actual_ret); // [確認_異常系] - cplat_internal_prompt_platform_read_char_nb() の戻り値が -1 であること。(50 ミリ秒待っても入力がないことの確認)
}

// 出力端末の幅を取得し、取得失敗時は 80 桁を返すことの確認
TEST_F(promptWindowsTest, columns_use_console_window_and_fallback)
{
    // Arrange
    Mock_windows mock_windows;
    CONSOLE_SCREEN_BUFFER_INFO info = {};
    info.srWindow.Left = 10;
    info.srWindow.Right = 109;
    // Pre-Assert
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_OUTPUT_HANDLE)).WillRepeatedly(Return((HANDLE)0x1234));
    // [Pre-Assert確認_正常系] - 出力ハンドルを取得すること。
    EXPECT_CALL(mock_windows, GetConsoleScreenBufferInfo(_, _, _, _, _))
        .WillOnce(DoAll(SetArgPointee<4>(info), Return(TRUE)))
        .WillOnce(Return(FALSE))
        .WillOnce(Invoke(
            [](const char *, int, const char *, HANDLE, CONSOLE_SCREEN_BUFFER_INFO *out)
            {
                out->srWindow.Left = 2;
                out->srWindow.Right = 1;
                return TRUE;
            }));
    // [Pre-Assert確認_正常系] - サイズ取得を 3 回行うこと。
    // [Pre-Assert手順] - 正常値、失敗、逆順の座標を返す。
    // Act
    size_t columns = cplat_internal_prompt_platform_get_columns();  // [手順] - 正常な端末幅を取得する。
    size_t fallback = cplat_internal_prompt_platform_get_columns(); // [手順] - 取得失敗時の幅を取得する。
    size_t invalid = cplat_internal_prompt_platform_get_columns();  // [手順] - 逆順の座標で幅を取得する。
    // Assert
    EXPECT_EQ(100U, columns); // [確認_正常系] - ウィンドウ幅が 100 桁であること。
    EXPECT_EQ(80U, fallback); // [確認_異常系] - 取得失敗時は 80 桁であること。
    EXPECT_EQ(80U, invalid);  // [確認_異常系] - 不正な座標では 80 桁であること。
}

#endif /* PLATFORM_WINDOWS */

#if defined(PLATFORM_WINDOWS)
// 不完全な UTF-8 の後続バイトを待機せずに取得することの確認
TEST_F(promptWindowsTest, reads_surrogate_pair_and_drains_utf8)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    // Pre-Assert
    EXPECT_CALL(mock_windows, WaitForSingleObject(_, _, _, _, 100U)).Times(2).WillRepeatedly(Return(WAIT_OBJECT_0));
    // [Pre-Assert確認_正常系] - UTF-16 の入力ごとに 1 回だけ待機すること。
    EXPECT_CALL(mock_windows, ReadConsoleW(_, _, _, _, _, 1U, _, _))
        .WillOnce(
            [](const char *, int, const char *, HANDLE, LPVOID buf, DWORD, LPDWORD read, PCONSOLE_READCONSOLE_CONTROL)
            {
                *static_cast<WCHAR *>(buf) = 0xD83DU;
                *read = 1U;
                return TRUE;
            })
        .WillOnce(
            [](const char *, int, const char *, HANDLE, LPVOID buf, DWORD, LPDWORD read, PCONSOLE_READCONSOLE_CONTROL)
            {
                *static_cast<WCHAR *>(buf) = 0xDE00U;
                *read = 1U;
                return TRUE;
            });
    // [Pre-Assert確認_正常系] - ReadConsoleW() が 2 回呼び出されること。
    // [Pre-Assert手順] - サロゲートペアを返す。
    // Act
    int first = cplat_internal_prompt_platform_read_char(&handle_);     // [手順] - UTF-8 の先頭を読む。
    int second = cplat_internal_prompt_platform_read_char_nb(&handle_); // [手順] - バッファー内の続きを読む。
    int third = cplat_internal_prompt_platform_read_char(&handle_);     // [手順] - 3 バイト目を読む。
    int fourth = cplat_internal_prompt_platform_read_char(&handle_);    // [手順] - 4 バイト目を読む。
    // Assert
    EXPECT_EQ(0xF0, first);  // [確認_正常系] - 先頭を取得すること。
    EXPECT_EQ(0x9F, second); // [確認_正常系] - 2 バイト目を取得すること。
    EXPECT_EQ(0x98, third);  // [確認_正常系] - 3 バイト目を取得すること。
    EXPECT_EQ(0x80, fourth); // [確認_正常系] - 最終バイトを取得すること。
}
#endif
#if defined(PLATFORM_WINDOWS)
// ReadConsoleW mock が未注入時と既定動作で実 API へ委譲することの確認
TEST_F(promptWindowsTest, readconsole_mock_uses_real_delegate)
{
    // Arrange
    WCHAR character = 0;
    DWORD count = 0;
    // Pre-Assert
    // Act 1
    BOOL direct =
        mock_ReadConsoleW(__FILE__, __LINE__, __func__, INVALID_HANDLE_VALUE, &character, 1U, &count, nullptr);
    // [手順] - mock 未注入で無効ハンドルを渡す。
    // Assert 1
    EXPECT_FALSE(direct); // [確認_異常系] - 実 API が無効ハンドルを拒否すること。
    // Arrange 2
    NiceMock<Mock_windows> mock_windows;
    // Pre-Assert 2
    // Act 2
    BOOL injected =
        mock_ReadConsoleW(__FILE__, __LINE__, __func__, INVALID_HANDLE_VALUE, &character, 1U, &count, nullptr);
    // [手順] - 既定 delegate へ無効ハンドルを渡す。
    // Assert 2
    EXPECT_FALSE(injected); // [確認_異常系] - 既定動作でも実 API が無効ハンドルを拒否すること。
}
#endif
#if defined(PLATFORM_WINDOWS)
// 上位サロゲートの待機中にタイムアウトしても状態を失わないことの確認
TEST_F(promptWindowsTest, preserves_high_surrogate_across_timeout)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    // Pre-Assert
    EXPECT_CALL(mock_windows, WaitForSingleObject(_, _, _, _, 100U))
        .WillOnce(Return(WAIT_OBJECT_0))
        .WillOnce(Return(WAIT_TIMEOUT))
        .WillOnce(Return(WAIT_OBJECT_0));
    // [Pre-Assert確認_正常系] - 2 コード単位の間でタイムアウトを受け付けること。
    EXPECT_CALL(mock_windows, ReadConsoleW(_, _, _, _, _, 1U, _, _))
        .WillOnce(
            [](const char *, int, const char *, HANDLE, LPVOID p, DWORD, LPDWORD n, PCONSOLE_READCONSOLE_CONTROL)
            {
                *static_cast<WCHAR *>(p) = 0xD83DU;
                *n = 1U;
                return TRUE;
            })
        .WillOnce(
            [](const char *, int, const char *, HANDLE, LPVOID p, DWORD, LPDWORD n, PCONSOLE_READCONSOLE_CONTROL)
            {
                *static_cast<WCHAR *>(p) = 0xDE00U;
                *n = 1U;
                return TRUE;
            });
    // [Pre-Assert確認_正常系] - ReadConsoleW() が 2 回呼び出されること。
    // [Pre-Assert手順] - 上位・下位サロゲートを返す。
    // Act 1
    int timeout = cplat_internal_prompt_platform_read_char(&handle_); // [手順] - 上位サロゲートの後でタイムアウトする。
    // Assert 1
    EXPECT_EQ(-2, timeout);                                   // [確認_正常系] - タイムアウトを返すこと。
    EXPECT_EQ(0xD83DU, handle_.windows_input.high_surrogate); // [確認_正常系] - 上位サロゲートを保持すること。
    // Act 2
    int first = cplat_internal_prompt_platform_read_char(&handle_); // [手順] - 続く下位サロゲートを受け取る。
    // Assert 2
    EXPECT_EQ(0xF0, first);                      // [確認_正常系] - 完成した UTF-8 の先頭を返すこと。
    EXPECT_EQ(4U, handle_.windows_input.length); // [確認_正常系] - 4 バイトへ変換すること。
}

// 不正なサロゲートの直後に来た ASCII を消費し直すことの確認
TEST_F(promptWindowsTest, drains_deferred_unit_before_waiting)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    cplat_internal_prompt_edit_utf16_feed(&handle_.windows_input, 0xD800U);
    cplat_internal_prompt_edit_utf16_feed(&handle_.windows_input, 0x41U);
    handle_.windows_input.offset = handle_.windows_input.length;
    // Pre-Assert
    EXPECT_CALL(mock_windows, WaitForSingleObject(_, _, _, _, _)).Times(0);
    // [Pre-Assert確認_正常系] - 保留中の入力を待機せず処理すること。
    EXPECT_CALL(mock_windows, ReadConsoleW(_, _, _, _, _, _, _, _)).Times(0);
    // [Pre-Assert確認_正常系] - 保留中の入力より先にコンソールを読まないこと。
    // Act
    int actual = cplat_internal_prompt_platform_read_char_nb(&handle_); // [手順] - 保留したコード単位を読む。
    // Assert
    EXPECT_EQ('A', actual);                                // [確認_正常系] - 後続文字を失わないこと。
    EXPECT_EQ(0U, handle_.windows_input.has_pending_unit); // [確認_正常系] - 保留状態を解除すること。
}
#endif