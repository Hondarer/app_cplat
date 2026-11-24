#include <testfw.h>
#include <mock_cplat.h>
#include <cplat/console/console.h>
#include <cplat/console/console_internal.h>

#if defined(PLATFORM_WINDOWS)
    #include <cplat/crt/temp_dir_internal.h>
    #include <mock_stdio.h>
    #include <mock_unistd.h>
    #include <mock_windows.h>

    #include <cwchar>
    #include <fstream>
    #include <sstream>
    #include <string>

    #include "console.inject.h"

namespace
{

// 診断ログ ファイルのパスを console.c と同じ方法 (%TEMP% 配下) で求める。
std::wstring diag_log_path()
{
    wchar_t temp_path[MAX_PATH + 1] = {};
    DWORD length = GetTempPathW((DWORD)(sizeof(temp_path) / sizeof(temp_path[0])), temp_path);
    if (length == 0 || length >= sizeof(temp_path) / sizeof(temp_path[0]))
    {
        return std::wstring();
    }

    return std::wstring(temp_path) + L"cplat_console_attach.log";
}

// 診断ログ ファイルの現在の大きさを返す。ファイルがない場合は 0 を返す。
std::streamoff diag_log_size()
{
    std::ifstream stream(diag_log_path(), std::ios::binary | std::ios::ate);
    if (!stream)
    {
        return 0;
    }

    return (std::streamoff)stream.tellg();
}

// 診断ログ ファイルの offset 以降に追記された内容を返す。
std::string diag_log_since(std::streamoff offset)
{
    std::ifstream stream(diag_log_path(), std::ios::binary);
    std::ostringstream content;

    if (!stream)
    {
        return std::string();
    }

    stream.seekg(offset);
    content << stream.rdbuf();
    return content.str();
}

bool is_console_device_name(LPCWSTR name)
{
    return name != nullptr && (wcscmp(name, L"CONOUT$") == 0 || wcscmp(name, L"CONIN$") == 0);
}

} // namespace

class consoleFailureTest : public Test
{
  protected:
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    std::streamoff log_offset = 0;

    // [サブ手順 名前=consoleFailureTest.SetUp]
    void SetUp() override
    {
        // console.c の単体テストでは shutdown.c を対象外とし、登録 API の呼び出しをフェイクする。
        ON_CALL(mock_cplat, cplat_shutdown_register(_, _)).WillByDefault(Return(CPLAT_OK));
        // [状態] - cplat_shutdown_register() から CPLAT_OK を返すように既定動作を設定する。
        test_console_reset_state();
        // [状態] - console.c の初期化の状態と終了処理の登録の once を初期値へ戻す。
        (void)SetEnvironmentVariableA(CPLAT_CONSOLE_ATTACH_DIAG_ENV, "1");
        // [状態] - 環境変数 CPLAT_CONSOLE_ATTACH_DIAG に 1 を設定し、診断ログを有効にする。
        log_offset = diag_log_size();
        // [状態] - 診断ログ ファイルの現在の大きさを記録する。
    }
    // [サブ手順終了]

    // [サブ手順 名前=consoleFailureTest.TearDown]
    void TearDown() override
    {
        (void)SetEnvironmentVariableA(CPLAT_CONSOLE_ATTACH_DIAG_ENV, nullptr);
        test_console_reset_state();
    }
    // [サブ手順終了]
};

// コンソールの設定の復元に失敗しても、診断ログへ残して残りの復元を続けることの確認
// [サブ手順参照 名前=consoleFailureTest.SetUp]
TEST_F(consoleFailureTest, dispose_logs_restore_failures_and_continues)
{
    // Arrange
    test_console_set_initialized_state(932U, 932U, 0x3UL, 0x3UL); // [状態] - 復元する元の設定を持つ初期化済みの状態にする。

    // Pre-Assert
    EXPECT_CALL(mock_windows, SetConsoleMode(_, _, _, _, 0x3UL))
        .Times(2)
        .WillRepeatedly(Return(FALSE)); // [Pre-Assert確認_異常系] - SetConsoleMode() が元のモードを指定して 2 回呼び出されること。
                                        // [Pre-Assert手順] - SetConsoleMode() から FALSE を返す。
    EXPECT_CALL(mock_windows, SetConsoleCP(_, _, _, 932U))
        .WillOnce(Return(FALSE)); // [Pre-Assert確認_異常系] - SetConsoleCP() が元のコード ページを指定して 1 回呼び出されること。
                                  // [Pre-Assert手順] - SetConsoleCP() から FALSE を返す。
    EXPECT_CALL(mock_windows, SetConsoleOutputCP(_, _, _, 932U))
        .WillOnce(Return(FALSE)); // [Pre-Assert確認_異常系] - SetConsoleOutputCP() が元のコード ページを指定して 1 回呼び出されること。
                                  // [Pre-Assert手順] - SetConsoleOutputCP() から FALSE を返す。

    // Act
    cplat_console_dispose(); // [手順] - 復元がすべて失敗する状態で cplat_console_dispose() を呼び出す。

    // Assert
    std::string log = diag_log_since(log_offset);
    EXPECT_THAT(log, HasSubstr("console_dispose SetConsoleMode stdout failed")); // [確認_異常系] - 診断ログに stdout のモードの復元の失敗が追記されること。
    EXPECT_THAT(log, HasSubstr("console_dispose SetConsoleMode stderr failed")); // [確認_異常系] - 診断ログに stderr のモードの復元の失敗が追記されること。
    EXPECT_THAT(log, HasSubstr("console_dispose SetConsoleCP failed")); // [確認_異常系] - 診断ログに入力のコード ページの復元の失敗が追記されること。
    EXPECT_THAT(log, HasSubstr("console_dispose SetConsoleOutputCP failed")); // [確認_異常系] - 診断ログに出力のコード ページの復元の失敗が追記されること。
    EXPECT_EQ(0, test_console_get_initialized()); // [確認_異常系] - 初期化の状態が 0 に戻ること。
}
// [サブ手順参照 名前=consoleFailureTest.TearDown]

// 終了時のフラッシュに失敗しても、診断ログへ残して処理を続けることの確認
// [サブ手順参照 名前=consoleFailureTest.SetUp]
TEST_F(consoleFailureTest, dispose_on_shutdown_logs_flush_failures)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    cplat_shutdown_event event = {CPLAT_SHUTDOWN_REASON_NORMAL_EXIT, CPLAT_SHUTDOWN_CODE_KIND_NONE,
                                  0}; // [状態] - 通常終了イベントを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_stdio, fflush(_, _, _, stdout))
        .WillOnce(Return(EOF)); // [Pre-Assert確認_異常系] - fflush() が stdout を指定して 1 回呼び出されること。
                                // [Pre-Assert手順] - fflush() から EOF を返す。
    EXPECT_CALL(mock_stdio, fflush(_, _, _, stderr))
        .WillOnce(Return(EOF)); // [Pre-Assert確認_異常系] - fflush() が stderr を指定して 1 回呼び出されること。
                                // [Pre-Assert手順] - fflush() から EOF を返す。

    // Act
    cplat_internal_console_dispose_on_shutdown(
        &event, nullptr); // [手順] - フラッシュが失敗する状態で cplat_internal_console_dispose_on_shutdown() を呼び出す。

    // Assert
    std::string log = diag_log_since(log_offset);
    EXPECT_THAT(log, HasSubstr("dispose_on_shutdown fflush stdout failed")); // [確認_異常系] - 診断ログに stdout のフラッシュの失敗が追記されること。
    EXPECT_THAT(log, HasSubstr("dispose_on_shutdown fflush stderr failed")); // [確認_異常系] - 診断ログに stderr のフラッシュの失敗が追記されること。
    EXPECT_THAT(log, HasSubstr("dispose_on_shutdown end")); // [確認_異常系] - 診断ログに終了処理の最後の行が追記されること。(失敗後も処理を続けることの確認)
}
// [サブ手順参照 名前=consoleFailureTest.TearDown]

// コード ページを UTF-8 にできなくても、診断ログへ残して初期化を続けることの確認
// [サブ手順参照 名前=consoleFailureTest.SetUp]
TEST_F(consoleFailureTest, init_logs_code_page_failures)
{
    // Arrange
    ON_CALL(mock_cplat, cplat_isatty(_)).WillByDefault(Return(1)); // [状態] - 標準出力と標準エラー出力をコンソールとする。
    ON_CALL(mock_windows, GetConsoleMode(_, _, _, _, _))
        .WillByDefault(Return(FALSE)); // [状態] - 実際のコンソール モードを変えないよう、GetConsoleMode() から FALSE を返す。

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetConsoleCP(_, _, _))
        .WillOnce(Return(932U)); // [Pre-Assert確認_異常系] - GetConsoleCP() が 1 回呼び出されること。
                                 // [Pre-Assert手順] - GetConsoleCP() から UTF-8 以外のコード ページを返す。
    EXPECT_CALL(mock_windows, GetConsoleOutputCP(_, _, _))
        .WillOnce(Return(932U)); // [Pre-Assert確認_異常系] - GetConsoleOutputCP() が 1 回呼び出されること。
                                 // [Pre-Assert手順] - GetConsoleOutputCP() から UTF-8 以外のコード ページを返す。
    EXPECT_CALL(mock_windows, SetConsoleCP(_, _, _, CP_UTF8))
        .WillOnce(Return(FALSE)); // [Pre-Assert確認_異常系] - SetConsoleCP() が CP_UTF8 を指定して 1 回呼び出されること。
                                  // [Pre-Assert手順] - SetConsoleCP() から FALSE を返す。
    EXPECT_CALL(mock_windows, SetConsoleOutputCP(_, _, _, CP_UTF8))
        .WillOnce(Return(FALSE)); // [Pre-Assert確認_異常系] - SetConsoleOutputCP() が CP_UTF8 を指定して 1 回呼び出されること。
                                  // [Pre-Assert手順] - SetConsoleOutputCP() から FALSE を返す。

    // Act
    cplat_console_init(); // [手順] - コード ページの設定が失敗する状態で cplat_console_init() を呼び出す。

    // Assert
    std::string log = diag_log_since(log_offset);
    EXPECT_THAT(log, HasSubstr("console_init SetConsoleCP failed")); // [確認_異常系] - 診断ログに入力のコード ページの設定の失敗が追記されること。
    EXPECT_THAT(log, HasSubstr("console_init SetConsoleOutputCP failed")); // [確認_異常系] - 診断ログに出力のコード ページの設定の失敗が追記されること。
    EXPECT_THAT(log, HasSubstr("console_init GetConsoleMode stdout failed")); // [確認_異常系] - 診断ログにモードの取得の行が追記されること。(失敗後も初期化を続けることの確認)
    EXPECT_EQ(1, test_console_get_initialized()); // [確認_異常系] - 初期化の状態が 1 であること。
}
// [サブ手順参照 名前=consoleFailureTest.TearDown]

// 標準ハンドルを付け替えられない場合に、親コンソールへの接続は維持したまま失敗を返し、診断ログへ残して処理を続けることの確認
// [サブ手順参照 名前=consoleFailureTest.SetUp]
TEST_F(consoleFailureTest, attach_parent_returns_failure_when_set_std_handle_fails)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    NiceMock<Mock_unistd> mock_unistd;
    HANDLE fake_console = (HANDLE)(uintptr_t)0x1234U;
    char program[] = "consoleFailureTest";
    char handover[] = CPLAT_CONSOLE_HANDOVER_FLAG "=4242";
    char *argv[] = {program, handover, nullptr};
    int argc = 2;
    int attached = 0;

    ON_CALL(mock_windows, FreeConsole(_, _, _)).WillByDefault(Return(TRUE)); // [状態] - FreeConsole() から TRUE を返す。
    ON_CALL(mock_windows, AttachConsole(_, _, _, 4242UL))
        .WillByDefault(Return(TRUE)); // [状態] - AttachConsole() から TRUE を返す。
    ON_CALL(mock_windows, CreateFileW(_, _, _, Truly(is_console_device_name), _, _, _, _, _, _))
        .WillByDefault(Return(fake_console)); // [状態] - CONOUT$ と CONIN$ を開く CreateFileW() から偽のハンドルを返す。
    ON_CALL(mock_stdio, _fileno(_, _, _, _)).WillByDefault(Return(100)); // [状態] - _fileno() から 100 を返す。
    ON_CALL(mock_unistd, _close(_, _, _, _)).WillByDefault(Return(0)); // [状態] - 実際の標準ストリームを閉じないよう、_close() から 0 を返す。
    ON_CALL(mock_unistd, _open_osfhandle(_, _, _, _, _))
        .WillByDefault(Return(100)); // [状態] - 実際の fd を作らないよう、_open_osfhandle() から 100 を返す。
    ON_CALL(mock_stdio, setvbuf(_, _, _, _, _, _, _))
        .WillByDefault(Return(0)); // [状態] - 実際のバッファリングを変えないよう、setvbuf() から 0 を返す。

    // Pre-Assert
    EXPECT_CALL(mock_windows, SetStdHandle(_, _, _, _, fake_console))
        .Times(3)
        .WillRepeatedly(Return(FALSE)); // [Pre-Assert確認_異常系] - SetStdHandle() が 3 回呼び出されること。
                                        // [Pre-Assert手順] - SetStdHandle() から FALSE を返す。
    EXPECT_CALL(mock_unistd, _open_osfhandle(_, _, _, _, _))
        .Times(3); // [Pre-Assert確認_異常系] - _open_osfhandle() が 3 回呼び出されること。(付け替えの失敗後も再接続を続けることの確認)

    // Act
    int result = cplat_console_attach_parent(
        &argc, argv, &attached); // [手順] - SetStdHandle() が失敗する状態で cplat_console_attach_parent() を呼び出す。

    // Assert
    std::string log = diag_log_since(log_offset);
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, result); // [確認_異常系] - cplat_console_attach_parent() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(1, attached);               // [確認_異常系] - attached が 1 であること。
    EXPECT_THAT(log, HasSubstr("SetStdHandle stdout failed")); // [確認_異常系] - 診断ログに stdout の付け替えの失敗が追記されること。
    EXPECT_THAT(log, HasSubstr("SetStdHandle stderr failed")); // [確認_異常系] - 診断ログに stderr の付け替えの失敗が追記されること。
    EXPECT_THAT(log, HasSubstr("SetStdHandle stdin failed")); // [確認_異常系] - 診断ログに stdin の付け替えの失敗が追記されること。
    EXPECT_THAT(log, Not(HasSubstr("SetStdHandle stdout success"))); // [確認_異常系] - 診断ログに stdout の付け替えの成功が追記されないこと。
}
// [サブ手順参照 名前=consoleFailureTest.TearDown]

// すべての段階が成功する場合に CPLAT_OK と attached 1 を返すことの確認
// [サブ手順参照 名前=consoleFailureTest.SetUp]
TEST_F(consoleFailureTest, attach_parent_succeeds_when_all_streams_are_reconnected)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    NiceMock<Mock_unistd> mock_unistd;
    HANDLE fake_console = (HANDLE)(uintptr_t)0x1234U;
    char program[] = "consoleFailureTest";
    char handover[] = CPLAT_CONSOLE_HANDOVER_FLAG "=4242";
    char *argv[] = {program, handover, nullptr};
    int argc = 2;
    int attached = 0;

    ON_CALL(mock_windows, FreeConsole(_, _, _)).WillByDefault(Return(TRUE)); // [状態] - FreeConsole() から TRUE を返す。
    ON_CALL(mock_windows, AttachConsole(_, _, _, 4242UL))
        .WillByDefault(Return(TRUE)); // [状態] - AttachConsole() から TRUE を返す。
    ON_CALL(mock_windows, CreateFileW(_, _, _, Truly(is_console_device_name), _, _, _, _, _, _))
        .WillByDefault(Return(fake_console)); // [状態] - CONOUT$ と CONIN$ を開く CreateFileW() から偽のハンドルを返す。
    ON_CALL(mock_windows, SetStdHandle(_, _, _, _, fake_console))
        .WillByDefault(Return(TRUE)); // [状態] - SetStdHandle() から TRUE を返す。
    ON_CALL(mock_stdio, _fileno(_, _, _, _)).WillByDefault(Return(100)); // [状態] - _fileno() から 100 を返す。
    ON_CALL(mock_unistd, _close(_, _, _, _)).WillByDefault(Return(0)); // [状態] - _close() から 0 を返す。
    ON_CALL(mock_unistd, _open_osfhandle(_, _, _, _, _))
        .WillByDefault(Return(100)); // [状態] - _open_osfhandle() から 100 を返す。
    ON_CALL(mock_stdio, setvbuf(_, _, _, _, _, _, _))
        .WillByDefault(Return(0)); // [状態] - setvbuf() から 0 を返す。

    // Pre-Assert
    EXPECT_CALL(mock_stdio, setvbuf(_, _, _, stdout, nullptr, _IONBF, 0U))
        .Times(1); // [Pre-Assert確認_正常系] - stdout に対して setvbuf() が _IONBF で 1 回呼び出されること。
    EXPECT_CALL(mock_stdio, setvbuf(_, _, _, stderr, nullptr, _IONBF, 0U))
        .Times(1); // [Pre-Assert確認_正常系] - stderr に対して setvbuf() が _IONBF で 1 回呼び出されること。

    // Act
    int result = cplat_console_attach_parent(&argc, argv, &attached); // [手順] - 全段階が成功する状態で cplat_console_attach_parent() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, result); // [確認_正常系] - cplat_console_attach_parent() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1, attached);      // [確認_正常系] - attached が 1 であること。
}
// [サブ手順参照 名前=consoleFailureTest.TearDown]

// 開き直した fd 番号が元の番号と一致しない場合に CPLAT_ERR_UNKNOWN と attached 1 を返し、new_fd を閉じないことの確認
// [サブ手順参照 名前=consoleFailureTest.SetUp]
TEST_F(consoleFailureTest, attach_parent_returns_failure_when_fd_mismatch)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    NiceMock<Mock_unistd> mock_unistd;
    HANDLE fake_console = (HANDLE)(uintptr_t)0x1234U;
    char program[] = "consoleFailureTest";
    char handover[] = CPLAT_CONSOLE_HANDOVER_FLAG "=4242";
    char *argv[] = {program, handover, nullptr};
    int argc = 2;
    int attached = 0;

    ON_CALL(mock_windows, FreeConsole(_, _, _)).WillByDefault(Return(TRUE)); // [状態] - FreeConsole() から TRUE を返す。
    ON_CALL(mock_windows, AttachConsole(_, _, _, 4242UL))
        .WillByDefault(Return(TRUE)); // [状態] - AttachConsole() から TRUE を返す。
    ON_CALL(mock_windows, CreateFileW(_, _, _, Truly(is_console_device_name), _, _, _, _, _, _))
        .WillByDefault(Return(fake_console)); // [状態] - CONOUT$ と CONIN$ を開く CreateFileW() から偽のハンドルを返す。
    ON_CALL(mock_windows, SetStdHandle(_, _, _, _, fake_console))
        .WillByDefault(Return(TRUE)); // [状態] - SetStdHandle() から TRUE を返す。
    ON_CALL(mock_stdio, _fileno(_, _, _, _)).WillByDefault(Return(100)); // [状態] - _fileno() から 100 を返す。
    ON_CALL(mock_unistd, _close(_, _, _, _)).WillByDefault(Return(0)); // [状態] - _close() から 0 を返す。

    // Pre-Assert
    EXPECT_CALL(mock_unistd, _open_osfhandle(_, _, _, _, _))
        .Times(3)
        .WillRepeatedly(Return(101)); // [Pre-Assert確認_異常系] - _open_osfhandle() が 3 回呼び出されること。
                                      // [Pre-Assert手順] - _open_osfhandle() から元の 100 と異なる 101 を返す。
    EXPECT_CALL(mock_unistd, _close(_, _, _, 100))
        .Times(3); // [Pre-Assert確認_正常系] - 元の fd 100 を指定した _close() が 3 回呼び出されること。
    EXPECT_CALL(mock_unistd, _close(_, _, _, 101))
        .Times(0); // [Pre-Assert確認_異常系] - 不一致となった 101 を指定した _close() が呼び出されないこと。
    EXPECT_CALL(mock_stdio, setvbuf(_, _, _, _, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - setvbuf() が呼び出されないこと。

    // Act
    int result = cplat_console_attach_parent(&argc, argv, &attached); // [手順] - _open_osfhandle() が異なる番号を返す状態で cplat_console_attach_parent() を呼び出す。

    // Assert
    std::string log = diag_log_since(log_offset);
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, result); // [確認_異常系] - cplat_console_attach_parent() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(1, attached);               // [確認_異常系] - attached が 1 であること。
    EXPECT_THAT(log, HasSubstr("unexpected fd new_fd=101 expected_fd=100")); // [確認_異常系] - 診断ログに番号の不一致の行が追記されること。
}
// [サブ手順参照 名前=consoleFailureTest.TearDown]

// _open_osfhandle() が失敗した場合に CPLAT_ERR_UNKNOWN と attached 1 を返すことの確認
// [サブ手順参照 名前=consoleFailureTest.SetUp]
TEST_F(consoleFailureTest, attach_parent_returns_failure_when_open_osfhandle_fails)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    NiceMock<Mock_unistd> mock_unistd;
    HANDLE fake_console = (HANDLE)(uintptr_t)0x1234U;
    char program[] = "consoleFailureTest";
    char handover[] = CPLAT_CONSOLE_HANDOVER_FLAG "=4242";
    char *argv[] = {program, handover, nullptr};
    int argc = 2;
    int attached = 0;

    ON_CALL(mock_windows, FreeConsole(_, _, _)).WillByDefault(Return(TRUE)); // [状態] - FreeConsole() から TRUE を返す。
    ON_CALL(mock_windows, AttachConsole(_, _, _, 4242UL))
        .WillByDefault(Return(TRUE)); // [状態] - AttachConsole() から TRUE を返す。
    ON_CALL(mock_windows, CreateFileW(_, _, _, Truly(is_console_device_name), _, _, _, _, _, _))
        .WillByDefault(Return(fake_console)); // [状態] - CONOUT$ と CONIN$ を開く CreateFileW() から偽のハンドルを返す。
    ON_CALL(mock_windows, SetStdHandle(_, _, _, _, fake_console))
        .WillByDefault(Return(TRUE)); // [状態] - SetStdHandle() から TRUE を返す。
    ON_CALL(mock_stdio, _fileno(_, _, _, _)).WillByDefault(Return(100)); // [状態] - _fileno() から 100 を返す。
    ON_CALL(mock_unistd, _close(_, _, _, _)).WillByDefault(Return(0)); // [状態] - _close() から 0 を返す。

    // Pre-Assert
    EXPECT_CALL(mock_unistd, _open_osfhandle(_, _, _, _, _))
        .Times(3)
        .WillRepeatedly(Return(-1)); // [Pre-Assert確認_異常系] - _open_osfhandle() が 3 回呼び出されること。
                                     // [Pre-Assert手順] - _open_osfhandle() から -1 を返す。

    // Act
    int result = cplat_console_attach_parent(&argc, argv, &attached); // [手順] - _open_osfhandle() が失敗する状態で cplat_console_attach_parent() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, result); // [確認_異常系] - cplat_console_attach_parent() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(1, attached);               // [確認_異常系] - attached が 1 であること。
}
// [サブ手順参照 名前=consoleFailureTest.TearDown]

// CONOUT$ / CONIN$ を開けない場合に CPLAT_ERR_UNKNOWN と attached 1 を返すことの確認
// [サブ手順参照 名前=consoleFailureTest.SetUp]
TEST_F(consoleFailureTest, attach_parent_returns_failure_when_console_device_cannot_be_opened)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    NiceMock<Mock_unistd> mock_unistd;
    char program[] = "consoleFailureTest";
    char handover[] = CPLAT_CONSOLE_HANDOVER_FLAG "=4242";
    char *argv[] = {program, handover, nullptr};
    int argc = 2;
    int attached = 0;

    ON_CALL(mock_windows, FreeConsole(_, _, _)).WillByDefault(Return(TRUE)); // [状態] - FreeConsole() から TRUE を返す。
    ON_CALL(mock_windows, AttachConsole(_, _, _, 4242UL))
        .WillByDefault(Return(TRUE)); // [状態] - AttachConsole() から TRUE を返す。

    // Pre-Assert
    EXPECT_CALL(mock_windows, CreateFileW(_, _, _, _, _, _, _, _, _, _))
        .Times(AnyNumber()); // [Pre-Assert確認_正常系] - 診断ログ用の CreateFileW() が任意の回数呼び出されること。
    EXPECT_CALL(mock_windows, CreateFileW(_, _, _, Truly(is_console_device_name), _, _, _, _, _, _))
        .Times(3)
        .WillRepeatedly(Return(INVALID_HANDLE_VALUE)); // [Pre-Assert確認_異常系] - CONOUT$ と CONIN$ を開く CreateFileW() が 3 回呼び出されること。
                                                       // [Pre-Assert手順] - CreateFileW() から INVALID_HANDLE_VALUE を返す。

    // Act
    int result = cplat_console_attach_parent(&argc, argv, &attached); // [手順] - CONOUT$ と CONIN$ のオープンが失敗する状態で cplat_console_attach_parent() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, result); // [確認_異常系] - cplat_console_attach_parent() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(1, attached);               // [確認_異常系] - attached が 1 であること。
}
// [サブ手順参照 名前=consoleFailureTest.TearDown]

// stdout の開き直しだけが失敗した場合でも、stderr と stdin の開き直しを行うことの確認
// [サブ手順参照 名前=consoleFailureTest.SetUp]
TEST_F(consoleFailureTest, attach_parent_tries_all_streams_after_one_fails)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    NiceMock<Mock_unistd> mock_unistd;
    HANDLE fake_console = (HANDLE)(uintptr_t)0x1234U;
    char program[] = "consoleFailureTest";
    char handover[] = CPLAT_CONSOLE_HANDOVER_FLAG "=4242";
    char *argv[] = {program, handover, nullptr};
    int argc = 2;
    int attached = 0;

    ON_CALL(mock_windows, FreeConsole(_, _, _)).WillByDefault(Return(TRUE)); // [状態] - FreeConsole() から TRUE を返す。
    ON_CALL(mock_windows, AttachConsole(_, _, _, 4242UL))
        .WillByDefault(Return(TRUE)); // [状態] - AttachConsole() から TRUE を返す。
    ON_CALL(mock_windows, CreateFileW(_, _, _, Truly(is_console_device_name), _, _, _, _, _, _))
        .WillByDefault(Return(fake_console)); // [状態] - CONOUT$ と CONIN$ を開く CreateFileW() から偽のハンドルを返す。
    ON_CALL(mock_windows, SetStdHandle(_, _, _, _, fake_console))
        .WillByDefault(Return(TRUE)); // [状態] - SetStdHandle() から TRUE を返す。
    ON_CALL(mock_stdio, _fileno(_, _, _, _)).WillByDefault(Return(100)); // [状態] - _fileno() から 100 を返す。
    ON_CALL(mock_unistd, _close(_, _, _, _)).WillByDefault(Return(0)); // [状態] - _close() から 0 を返す。
    ON_CALL(mock_stdio, setvbuf(_, _, _, _, _, _, _))
        .WillByDefault(Return(0)); // [状態] - setvbuf() から 0 を返す。

    // Pre-Assert
    EXPECT_CALL(mock_unistd, _open_osfhandle(_, _, _, _, _))
        .WillOnce(Return(-1))
        .WillOnce(Return(100))
        .WillOnce(Return(100)); // [Pre-Assert確認_異常系] - _open_osfhandle() が 3 回呼び出されること。
                                // [Pre-Assert手順] - 1 回目 (stdout) は -1 を返し、2 回目 (stderr) と 3 回目 (stdin) は 100 を返す。
    EXPECT_CALL(mock_stdio, setvbuf(_, _, _, stdout, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - 失敗した stdout に対して setvbuf() が呼び出されないこと。
    EXPECT_CALL(mock_stdio, setvbuf(_, _, _, stderr, nullptr, _IONBF, 0U))
        .Times(1); // [Pre-Assert確認_正常系] - 成功した stderr に対して setvbuf() が 1 回呼び出されること。

    // Act
    int result = cplat_console_attach_parent(&argc, argv, &attached); // [手順] - stdout の開き直しだけが失敗する状態で cplat_console_attach_parent() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, result); // [確認_異常系] - cplat_console_attach_parent() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(1, attached);               // [確認_異常系] - attached が 1 であること。
}
// [サブ手順参照 名前=consoleFailureTest.TearDown]

// _fileno() が失敗した場合に CPLAT_ERR_UNKNOWN と attached 1 を返すことの確認
// [サブ手順参照 名前=consoleFailureTest.SetUp]
TEST_F(consoleFailureTest, attach_parent_returns_failure_when_fileno_fails)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    NiceMock<Mock_unistd> mock_unistd;
    HANDLE fake_console = (HANDLE)(uintptr_t)0x1234U;
    char program[] = "consoleFailureTest";
    char handover[] = CPLAT_CONSOLE_HANDOVER_FLAG "=4242";
    char *argv[] = {program, handover, nullptr};
    int argc = 2;
    int attached = 0;

    ON_CALL(mock_windows, FreeConsole(_, _, _)).WillByDefault(Return(TRUE)); // [状態] - FreeConsole() から TRUE を返す。
    ON_CALL(mock_windows, AttachConsole(_, _, _, 4242UL))
        .WillByDefault(Return(TRUE)); // [状態] - AttachConsole() から TRUE を返す。
    ON_CALL(mock_windows, CreateFileW(_, _, _, Truly(is_console_device_name), _, _, _, _, _, _))
        .WillByDefault(Return(fake_console)); // [状態] - CONOUT$ と CONIN$ を開く CreateFileW() から偽のハンドルを返す。
    ON_CALL(mock_windows, SetStdHandle(_, _, _, _, fake_console))
        .WillByDefault(Return(TRUE)); // [状態] - SetStdHandle() から TRUE を返す。

    // Pre-Assert
    EXPECT_CALL(mock_stdio, _fileno(_, _, _, _))
        .Times(3)
        .WillRepeatedly(Return(-1)); // [Pre-Assert確認_異常系] - _fileno() が 3 回呼び出されること。
                                     // [Pre-Assert手順] - _fileno() から -1 を返す。

    // Act
    int result = cplat_console_attach_parent(&argc, argv, &attached); // [手順] - _fileno() が失敗する状態で cplat_console_attach_parent() を呼び出す。

    // Assert
    std::string log = diag_log_since(log_offset);
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, result); // [確認_異常系] - cplat_console_attach_parent() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(1, attached);               // [確認_異常系] - attached が 1 であること。
    EXPECT_THAT(log, HasSubstr("reopen stdout _fileno failed")); // [確認_異常系] - 診断ログに _fileno 失敗の行が追記されること。
}
// [サブ手順参照 名前=consoleFailureTest.TearDown]

// AttachConsole() が 1 度も成功しない場合に CPLAT_ERR_UNKNOWN と attached 0 を返すことの確認
// [サブ手順参照 名前=consoleFailureTest.SetUp]
TEST_F(consoleFailureTest, attach_parent_returns_failure_when_attach_console_never_succeeds)
{
    // Arrange
    char program[] = "consoleFailureTest";
    char handover[] = CPLAT_CONSOLE_HANDOVER_FLAG "=4242";
    char *argv[] = {program, handover, nullptr};
    int argc = 2;
    int attached = 1; // [状態] - 未接続への書き換えを確認するため 1 で初期化する。

    ON_CALL(mock_windows, FreeConsole(_, _, _)).WillByDefault(Return(TRUE)); // [状態] - FreeConsole() から TRUE を返す。

    // Pre-Assert
    EXPECT_CALL(mock_windows, AttachConsole(_, _, _, 4242UL))
        .Times(CPLAT_CONSOLE_ATTACH_MAX_ATTEMPTS)
        .WillRepeatedly(Return(FALSE)); // [Pre-Assert確認_異常系] - AttachConsole() が上限回数呼び出されること。
                                        // [Pre-Assert手順] - AttachConsole() から FALSE を返す。

    // Act
    int result = cplat_console_attach_parent(&argc, argv, &attached); // [手順] - AttachConsole() がすべて失敗する状態で cplat_console_attach_parent() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, result); // [確認_異常系] - cplat_console_attach_parent() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(0, attached);               // [確認_異常系] - attached が 0 であること。
}
// [サブ手順参照 名前=consoleFailureTest.TearDown]

// 終了処理を登録できない場合に、診断ログへ残して初期化を続けることの確認
// [サブ手順参照 名前=consoleFailureTest.SetUp]
TEST_F(consoleFailureTest, init_logs_shutdown_register_failure)
{
    // Arrange
    ON_CALL(mock_cplat, cplat_isatty(_)).WillByDefault(Return(0)); // [状態] - 標準出力と標準エラー出力をコンソール以外とする。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_shutdown_register(_, _))
        .WillOnce(Return(CPLAT_ERR_OUT_OF_MEMORY)); // [Pre-Assert確認_異常系] - cplat_shutdown_register() が 1 回呼び出されること。
                                                    // [Pre-Assert手順] - cplat_shutdown_register() から CPLAT_ERR_OUT_OF_MEMORY を返す。

    // Act
    cplat_console_init(); // [手順] - 終了処理の登録が失敗する状態で cplat_console_init() を呼び出す。

    // Assert
    std::string log = diag_log_since(log_offset);
    EXPECT_THAT(log, HasSubstr("console_init cplat_shutdown_register failed result=")); // [確認_異常系] - 診断ログに終了処理の登録の失敗が追記されること。
    EXPECT_THAT(log, HasSubstr("console_init isatty=0 skip")); // [確認_異常系] - 診断ログに初期化の続きの行が追記されること。(登録の失敗後も初期化を続けることの確認)
}
// [サブ手順参照 名前=consoleFailureTest.TearDown]

namespace
{

DWORD WINAPI console_temp_resolver_failing(DWORD, LPWSTR)
{
    SetLastError(ERROR_ACCESS_DENIED);
    return 0U;
}

DWORD WINAPI console_temp_resolver_custom_system_temp(DWORD buffer_length, LPWSTR buffer)
{
    static const wchar_t kPath[] = L"C:\\CustomSystemTemp\\";
    const DWORD len = (DWORD)(sizeof(kPath) / sizeof(kPath[0]) - 1U);
    if (buffer != nullptr && buffer_length > len)
    {
        std::memcpy(buffer, kPath, sizeof(kPath));
    }
    return len;
}

DWORD WINAPI console_temp_resolver_default_system_temp(DWORD buffer_length, LPWSTR buffer)
{
    static const wchar_t kPath[] = L"C:\\Windows\\SystemTemp\\";
    const DWORD len = (DWORD)(sizeof(kPath) / sizeof(kPath[0]) - 1U);
    if (buffer != nullptr && buffer_length > len)
    {
        std::memcpy(buffer, kPath, sizeof(kPath));
    }
    return len;
}

} // namespace

// 一時ディレクトリの取得に失敗した場合に診断ログのパス構築が -1 を返し CreateFileW を呼ばないことの確認
// [サブ手順参照 名前=consoleFailureTest.SetUp]
// [サブ手順参照 名前=consoleFailureTest.TearDown]
TEST_F(consoleFailureTest, diag_log_skips_create_file_when_internal_temp_dir_fails)
{
    // Arrange
    wchar_t path_out[MAX_PATH] = {};
    ON_CALL(mock_cplat, cplat_isatty(_)).WillByDefault(Return(0));
    cplat_internal_temp_dir_set_resolver_for_test(console_temp_resolver_failing);

    // Pre-Assert
    EXPECT_CALL(mock_windows, CreateFileW(_, _, _, _, _, _, _, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - 一時ディレクトリ取得失敗時は CreateFileW() が呼ばれないこと。

    // Act
    int path_rc = test_console_build_diag_log_path(path_out, sizeof(path_out) / sizeof(path_out[0]));
    cplat_console_init(); // [手順] - 内部一時ディレクトリ取得失敗時に診断ログ出力付きの初期化を呼ぶ。

    // Assert
    EXPECT_EQ(-1, path_rc); // [確認_異常系] - build_console_diag_log_path() が -1 を返すこと。

    // Cleanup
    cplat_internal_temp_dir_set_resolver_for_test(nullptr);
}

// 診断ログのパス構築が SystemTemp 指定と既定 C:\Windows\SystemTemp を上書きせず使用することの確認
// [サブ手順参照 名前=consoleFailureTest.SetUp]
// [サブ手順参照 名前=consoleFailureTest.TearDown]
TEST_F(consoleFailureTest, diag_log_path_uses_system_temp_and_default_unmodified)
{
    // Arrange
    wchar_t custom_path[MAX_PATH] = {};
    wchar_t default_path[MAX_PATH] = {};

    // Pre-Assert

    // Act
    cplat_internal_temp_dir_set_resolver_for_test(console_temp_resolver_custom_system_temp);
    int rc_custom = test_console_build_diag_log_path(custom_path, sizeof(custom_path) / sizeof(custom_path[0]));
    cplat_internal_temp_dir_set_resolver_for_test(console_temp_resolver_default_system_temp);
    int rc_default = test_console_build_diag_log_path(default_path, sizeof(default_path) / sizeof(default_path[0]));

    // Assert
    EXPECT_EQ(0, rc_custom); // [確認_正常系] - custom 側が成功すること。
    EXPECT_STREQ(L"C:\\CustomSystemTemp\\cplat_console_attach.log", custom_path); // [確認_正常系] - SystemTemp パスが使われること。
    EXPECT_EQ(0, rc_default); // [確認_正常系] - default 側が成功すること。
    EXPECT_STREQ(L"C:\\Windows\\SystemTemp\\cplat_console_attach.log", default_path); // [確認_正常系] - 既定 SystemTemp パスが使われること。

    // Cleanup
    cplat_internal_temp_dir_set_resolver_for_test(nullptr);
}

// 再接続試行時の FreeConsole の戻り値と直後の GetLastError、および再接続後の GetStdHandle 読み戻し一致が記録されることの確認
// [サブ手順参照 名前=consoleFailureTest.SetUp]
// [サブ手順参照 名前=consoleFailureTest.TearDown]
TEST_F(consoleFailureTest, attach_parent_logs_free_console_result_and_std_handle_readback)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    NiceMock<Mock_unistd> mock_unistd;
    HANDLE fake_console = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x1234U));
    HANDLE mismatched_handle = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x5678U));
    char program[] = "consoleFailureTest";
    char handover[] = CPLAT_CONSOLE_HANDOVER_FLAG "=4242";
    char *argv[] = {program, handover, nullptr};
    int argc = 2;
    int attached = 0;

    ON_CALL(mock_windows, CreateFileW(_, _, _, Truly(is_console_device_name), _, _, _, _, _, _))
        .WillByDefault(Return(fake_console)); // [状態] - CONOUT$ と CONIN$ を開く CreateFileW() から偽のハンドルを返す。
    ON_CALL(mock_windows, SetStdHandle(_, _, _, _, fake_console))
        .WillByDefault(Return(TRUE)); // [状態] - SetStdHandle() から TRUE を返す。
    ON_CALL(mock_stdio, _fileno(_, _, _, _)).WillByDefault(Return(100)); // [状態] - _fileno() から 100 を返す。
    ON_CALL(mock_unistd, _close(_, _, _, _)).WillByDefault(Return(0)); // [状態] - _close() から 0 を返す。
    ON_CALL(mock_unistd, _open_osfhandle(_, _, _, _, _))
        .WillByDefault(Return(100)); // [状態] - _open_osfhandle() から 100 を返す。
    ON_CALL(mock_stdio, setvbuf(_, _, _, _, _, _, _))
        .WillByDefault(Return(0)); // [状態] - setvbuf() から 0 を返す。

    // Pre-Assert
    EXPECT_CALL(mock_windows, FreeConsole(_, _, _))
        .WillOnce(
            [](const char *, int, const char *)
            {
                ::SetLastError(ERROR_ACCESS_DENIED);
                return FALSE;
            })
        .WillOnce(Return(TRUE)); // [Pre-Assert確認_異常系] - FreeConsole() が 2 回呼び出されること。(1 回目は ERROR_ACCESS_DENIED で FALSE、2 回目は TRUE を返すことの確認)
    EXPECT_CALL(mock_windows, AttachConsole(_, _, _, 4242UL))
        .WillOnce(Return(FALSE))
        .WillOnce(Return(TRUE)); // [Pre-Assert確認_異常系] - AttachConsole() が 1 回目失敗・2 回目成功で呼び出されること。
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_OUTPUT_HANDLE))
        .WillOnce(Return(fake_console)); // [Pre-Assert確認_正常系] - stdout の読み戻しが fake_console と一致すること。
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_ERROR_HANDLE))
        .WillOnce(Return(fake_console)); // [Pre-Assert確認_正常系] - stderr の読み戻しが fake_console と一致すること。
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_INPUT_HANDLE))
        .WillOnce(Return(mismatched_handle)); // [Pre-Assert確認_異常系] - stdin の読み戻しが不一致となる値を返すこと。

    // Act
    int result = cplat_console_attach_parent(
        &argc, argv, &attached); // [手順] - FreeConsole() の失敗と成功を経て cplat_console_attach_parent() を呼び出す。

    // Assert
    std::string log = diag_log_since(log_offset);
    EXPECT_EQ(CPLAT_OK, result); // [確認_正常系] - cplat_console_attach_parent() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1, attached);      // [確認_正常系] - attached が 1 であること。
    EXPECT_THAT(log, HasSubstr("attempt=0 FreeConsole result=0 last_error=5")); // [確認_異常系] - FreeConsole() の FALSE と last_error=5 が記録されること。
    EXPECT_THAT(log, HasSubstr("attempt=1 FreeConsole result=1")); // [確認_正常系] - FreeConsole() の TRUE が result=1 として記録されること。
    EXPECT_THAT(log, HasSubstr("attach_parent GetStdHandle readback stdout=")); // [確認_正常系] - stdout の読み戻し行が記録されること。
    EXPECT_THAT(log, HasSubstr("match=1")); // [確認_正常系] - 一致した stdout / stderr の match=1 が記録されること。
    EXPECT_THAT(log, HasSubstr("match=0")); // [確認_異常系] - 不一致となった stdin の match=0 が記録されること。
}

// 終了時のドレインで GetStdHandle の無効値と GetConsoleScreenBufferInfo の失敗が記録されることの確認
// [サブ手順参照 名前=consoleFailureTest.SetUp]
// [サブ手順参照 名前=consoleFailureTest.TearDown]
TEST_F(consoleFailureTest, dispose_on_shutdown_logs_drain_handle_and_screen_buffer_info_result)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    HANDLE fake_out = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x2468U));
    cplat_shutdown_event event = {CPLAT_SHUTDOWN_REASON_NORMAL_EXIT, CPLAT_SHUTDOWN_CODE_KIND_NONE, 0};

    test_console_set_attached_parent(1); // [状態] - 親コンソールへ再接続済みの状態にする。

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetLastError(_, _, _))
        .Times(AnyNumber()); // [Pre-Assert確認_正常系] - 診断ログ用の GetLastError() を許容する。
    {
        InSequence seq;
        EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_OUTPUT_HANDLE))
            .WillOnce(Return(INVALID_HANDLE_VALUE)); // [Pre-Assert確認_異常系] - 1 回目は無効ハンドルを返すこと。
        EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_OUTPUT_HANDLE))
            .WillOnce(Return(fake_out)); // [Pre-Assert確認_異常系] - 2 回目は有効ハンドルを返すこと。
        EXPECT_CALL(mock_windows, GetConsoleScreenBufferInfo(_, _, _, fake_out, _))
            .WillOnce(Return(FALSE)); // [Pre-Assert確認_異常系] - GetConsoleScreenBufferInfo() が失敗を返すこと。
        EXPECT_CALL(mock_windows, GetLastError(_, _, _))
            .WillOnce(Return(ERROR_INVALID_HANDLE))
            .RetiresOnSaturation(); // [Pre-Assert確認_異常系] - GetConsoleScreenBufferInfo() 失敗直後に ERROR_INVALID_HANDLE (6) を取り込むこと。
    }

    // Act
    cplat_internal_console_dispose_on_shutdown(
        &event, nullptr); // [手順] - GetStdHandle() が INVALID_HANDLE_VALUE を返す状態で終了時ドレインを呼ぶ。
    cplat_internal_console_dispose_on_shutdown(
        &event, nullptr); // [手順] - GetConsoleScreenBufferInfo() が失敗する状態で終了時ドレインを呼ぶ。

    // Assert
    std::string log = diag_log_since(log_offset);
    EXPECT_THAT(log, HasSubstr("dispose_on_shutdown drain skipped GetStdHandle=0x")); // [確認_異常系] - 無効ハンドル時にスキップ行が記録されること。
    EXPECT_THAT(log, HasSubstr("dispose_on_shutdown drain GetConsoleScreenBufferInfo handle=0x")); // [確認_異常系] - GetConsoleScreenBufferInfo() の行が記録されること。
    EXPECT_THAT(log, HasSubstr("result=0 error=6")); // [確認_異常系] - 失敗結果と error=6 が記録されること。
}

#endif /* PLATFORM_WINDOWS */
