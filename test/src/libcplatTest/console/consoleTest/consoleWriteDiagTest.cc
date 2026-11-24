#include <cplat/base/platform.h>

#if defined(PLATFORM_WINDOWS)

    #include <cplat/base/result.h>
    #include <cplat/base/windows_sdk.h>
    #include <cplat/console/console.h>
    #include <cplat/console/console_internal.h>
    #include <mock_cplat.h>
    #include <mock_windows.h>
    #include <testfw.h>

    #include <cstring>
    #include <fstream>
    #include <sstream>
    #include <string>

    #include "console.inject.h"

namespace
{

std::wstring diag_log_path()
{
    wchar_t temp_path[MAX_PATH + 1] = {};
    DWORD length = GetTempPathW(static_cast<DWORD>(sizeof(temp_path) / sizeof(temp_path[0])), temp_path);
    if (length == 0 || length >= sizeof(temp_path) / sizeof(temp_path[0]))
    {
        return std::wstring();
    }

    return std::wstring(temp_path) + L"cplat_console_attach.log";
}

std::streamoff diag_log_size()
{
    std::ifstream stream(diag_log_path(), std::ios::binary | std::ios::ate);
    if (!stream)
    {
        return 0;
    }

    return static_cast<std::streamoff>(stream.tellg());
}

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

std::wstring make_temp_test_file_path(const wchar_t *suffix)
{
    wchar_t temp_path[MAX_PATH + 1] = {};
    DWORD length = GetTempPathW(static_cast<DWORD>(sizeof(temp_path) / sizeof(temp_path[0])), temp_path);
    if (length == 0 || length >= sizeof(temp_path) / sizeof(temp_path[0]))
    {
        return std::wstring();
    }

    return std::wstring(temp_path) + L"cplat_console_write_diag_" + std::to_wstring(GetCurrentProcessId()) + L"_" +
           suffix;
}

} // namespace

class consoleWriteDiagTest : public Test
{
  protected:
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    std::streamoff log_offset = 0;

    // [サブ手順 名前=consoleWriteDiagTest.SetUp]
    void SetUp() override
    {
        test_console_reset_state();
        (void)SetEnvironmentVariableA(CPLAT_CONSOLE_ATTACH_DIAG_ENV, "1");
        log_offset = diag_log_size();
    }
    // [サブ手順終了]

    // [サブ手順 名前=consoleWriteDiagTest.TearDown]
    void TearDown() override
    {
        (void)SetEnvironmentVariableA(CPLAT_CONSOLE_ATTACH_DIAG_ENV, nullptr);
        test_console_reset_state();
    }
    // [サブ手順終了]
};

// GetStdHandle が INVALID_HANDLE_VALUE を返す場合に CPLAT_ERR_UNKNOWN を返し診断ログへ記録することの確認
// [サブ手順参照 名前=consoleWriteDiagTest.SetUp]
// [サブ手順参照 名前=consoleWriteDiagTest.TearDown]
TEST_F(consoleWriteDiagTest, logs_invalid_std_handle_and_returns_unknown)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_OUTPUT_HANDLE))
        .WillOnce(Return(INVALID_HANDLE_VALUE)); // [Pre-Assert確認_異常系] - GetStdHandle() が 1 回呼び出され INVALID_HANDLE_VALUE を返すこと。

    // Act
    int actual_ret = cplat_console_write(
        CPLAT_STREAM_STDOUT,
        "diag invalid handle"); // [手順] - GetStdHandle() が無効ハンドルを返す状態で cplat_console_write() を呼ぶ。

    // Assert
    std::string log = diag_log_since(log_offset);
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret); // [確認_異常系] - 戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_THAT(log, HasSubstr("GetStdHandle=")); // [確認_異常系] - 診断ログに GetStdHandle= の行が含まれること。
}

// WriteConsoleU が失敗して WriteFile が成功する場合に CPLAT_OK を返し両方の結果を記録することの確認
// [サブ手順参照 名前=consoleWriteDiagTest.SetUp]
// [サブ手順参照 名前=consoleWriteDiagTest.TearDown]
TEST_F(consoleWriteDiagTest, logs_write_console_failure_and_write_file_success)
{
    // Arrange
    const std::wstring file_path = make_temp_test_file_path(L"ok.txt");
    ASSERT_FALSE(file_path.empty()); // [状態確認] - 一時ファイル パスを構築できること。
    HANDLE file_handle =
        CreateFileW(file_path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    ASSERT_NE(INVALID_HANDLE_VALUE, file_handle); // [状態確認] - 書き込み用の一時ファイルを開けること。

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_OUTPUT_HANDLE))
        .WillOnce(Return(file_handle)); // [Pre-Assert確認_正常系] - GetStdHandle() が書き込み用ハンドルを返すこと。
    EXPECT_CALL(mock_cplat, WriteConsoleU(file_handle, _, _, _, nullptr))
        .WillOnce(Return(FALSE)); // [Pre-Assert確認_異常系] - WriteConsoleU() が FALSE を返すこと。

    // Act
    int actual_ret = cplat_console_write(
        CPLAT_STREAM_STDOUT,
        "fallback ok\n"); // [手順] - WriteConsoleU() 失敗後に実ファイル ハンドルへ WriteFile() が成功する状態で書き込む。

    // Assert
    std::string log = diag_log_since(log_offset);
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - フォールバック成功で CPLAT_OK を返すこと。
    EXPECT_THAT(log, HasSubstr("WriteConsoleU result=0")); // [確認_異常系] - ログに WriteConsoleU result=0 が含まれること。
    EXPECT_THAT(log, HasSubstr("WriteFile result=1")); // [確認_正常系] - ログに WriteFile result=1 が含まれること。

    // Cleanup
    (void)CloseHandle(file_handle);
    (void)DeleteFileW(file_path.c_str());
}

// WriteConsoleU と WriteFile の両方が失敗する場合に直後の GetLastError を取り込み記録することの確認
// [サブ手順参照 名前=consoleWriteDiagTest.SetUp]
// [サブ手順参照 名前=consoleWriteDiagTest.TearDown]
TEST_F(consoleWriteDiagTest, logs_both_failures_and_captures_last_error_immediately)
{
    // Arrange
    const std::wstring file_path = make_temp_test_file_path(L"ro.txt");
    ASSERT_FALSE(file_path.empty()); // [状態確認] - 一時ファイル パスを構築できること。
    HANDLE create_handle =
        CreateFileW(file_path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    ASSERT_NE(INVALID_HANDLE_VALUE, create_handle); // [状態確認] - 一時ファイルを作成できること。
    (void)CloseHandle(create_handle);

    HANDLE readonly_handle =
        CreateFileW(file_path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    ASSERT_NE(INVALID_HANDLE_VALUE, readonly_handle); // [状態確認] - 読み取り専用で一時ファイルを開けること。

    // Pre-Assert
    EXPECT_CALL(mock_windows, WriteFile(_, _, _, Ne(readonly_handle), _, _, _, _))
        .Times(AnyNumber())
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_正常系] - 診断ログ ファイル自身の WriteFile() 呼び出しは実ファイルへ通すこと。
    {
        InSequence seq;
        EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_OUTPUT_HANDLE))
            .WillOnce(Return(readonly_handle)); // [Pre-Assert確認_正常系] - 読み取り専用ハンドルを返すこと。
        EXPECT_CALL(mock_cplat, WriteConsoleU(readonly_handle, _, _, _, nullptr))
            .WillOnce(
                [](HANDLE, const char *, DWORD, DWORD *, LPVOID)
                {
                    ::SetLastError(ERROR_INVALID_FUNCTION);
                    return FALSE;
                }); // [Pre-Assert確認_異常系] - WriteConsoleU() が ERROR_INVALID_FUNCTION (1) で FALSE を返すこと。
        EXPECT_CALL(mock_windows, WriteFile(_, _, _, readonly_handle, _, _, _, _))
            .WillOnce(DoDefault()); // [Pre-Assert確認_異常系] - 実 WriteFile() が読み取り専用ハンドルで ERROR_ACCESS_DENIED (5) により失敗すること。
    }

    // Act
    int actual_ret = cplat_console_write(
        CPLAT_STREAM_STDOUT,
        "both fail\n"); // [手順] - WriteConsoleU() と WriteFile() が両方失敗する状態で書き込む。

    // Assert
    std::string log = diag_log_since(log_offset);
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret); // [確認_異常系] - 戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_THAT(log, HasSubstr("WriteConsoleU result=0")); // [確認_異常系] - WriteConsoleU の失敗行が記録されること。
    EXPECT_THAT(log, HasSubstr("error=1")); // [確認_異常系] - WriteConsoleU 失敗直後の ERROR_INVALID_FUNCTION (1) が記録されること。
    EXPECT_THAT(log, HasSubstr("WriteFile result=0")); // [確認_異常系] - WriteFile の失敗行が記録されること。
    EXPECT_THAT(log, HasSubstr("error=5")); // [確認_異常系] - WriteFile 失敗直後の ERROR_ACCESS_DENIED (5) が記録されること。
    EXPECT_THAT(log, HasSubstr("console_write failed")); // [確認_異常系] - 最終失敗の行が記録されること。

    // Cleanup
    (void)CloseHandle(readonly_handle);
    (void)DeleteFileW(file_path.c_str());
}

// 診断が無効な場合にログ ファイルの大きさが変わらず戻り値も変わらないことの確認
// [サブ手順参照 名前=consoleWriteDiagTest.SetUp]
// [サブ手順参照 名前=consoleWriteDiagTest.TearDown]
TEST_F(consoleWriteDiagTest, does_not_write_log_when_diag_is_disabled)
{
    // Arrange
    (void)SetEnvironmentVariableA(CPLAT_CONSOLE_ATTACH_DIAG_ENV, nullptr);
    const std::streamoff size_before = diag_log_size();

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_OUTPUT_HANDLE))
        .WillOnce(Return(INVALID_HANDLE_VALUE)); // [Pre-Assert確認_異常系] - 無効ハンドルを 1 回返すこと。

    // Act
    int actual_ret = cplat_console_write(
        CPLAT_STREAM_STDOUT,
        "diag disabled"); // [手順] - 診断無効の状態で失敗する cplat_console_write() を呼ぶ。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret); // [確認_異常系] - 戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(size_before, diag_log_size()); // [確認_正常系] - ログ ファイルの大きさが増えないこと。
}

// 1 回の書き込みで複数行を記録する経路でも先頭の判定結果だけを使い環境変数を再照会しないことの確認
// [サブ手順参照 名前=consoleWriteDiagTest.SetUp]
// [サブ手順参照 名前=consoleWriteDiagTest.TearDown]
TEST_F(consoleWriteDiagTest, queries_diag_env_once_per_write_for_both_enabled_and_disabled)
{
    // Arrange
    const std::wstring file_path = make_temp_test_file_path(L"once.txt");
    ASSERT_FALSE(file_path.empty()); // [状態確認] - 一時ファイル パスを構築できること。
    HANDLE create_handle =
        CreateFileW(file_path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    ASSERT_NE(INVALID_HANDLE_VALUE, create_handle); // [状態確認] - 一時ファイルを作成できること。
    (void)CloseHandle(create_handle);

    HANDLE readonly_handle =
        CreateFileW(file_path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    ASSERT_NE(INVALID_HANDLE_VALUE, readonly_handle); // [状態確認] - 読み取り専用で一時ファイルを開けること。

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_OUTPUT_HANDLE))
        .Times(2)
        .WillRepeatedly(Return(readonly_handle)); // [Pre-Assert確認_異常系] - 2 回の書き込みで読み取り専用ハンドルを返すこと。
    EXPECT_CALL(mock_cplat, WriteConsoleU(readonly_handle, _, _, _, nullptr))
        .WillOnce(
            [](HANDLE, const char *, DWORD, DWORD *, LPVOID)
            {
                (void)SetEnvironmentVariableA(CPLAT_CONSOLE_ATTACH_DIAG_ENV, "0");
                return FALSE;
            })
        .WillOnce(
            [](HANDLE, const char *, DWORD, DWORD *, LPVOID)
            {
                (void)SetEnvironmentVariableA(CPLAT_CONSOLE_ATTACH_DIAG_ENV, "1");
                return FALSE;
            }); // [Pre-Assert確認_異常系] - 1 回目は途中で環境変数を "0" へ、2 回目は途中で "1" へ書き換えても、各回とも最初の判定のまま振る舞うこと。

    // Act
    (void)SetEnvironmentVariableA(CPLAT_CONSOLE_ATTACH_DIAG_ENV, "1");
    int ret_enabled = cplat_console_write(
        CPLAT_STREAM_STDOUT,
        "env enabled"); // [手順] - 呼び出し開始時に診断有効の状態で失敗経路を実行する (途中で無効へ変更)。
    const std::streamoff size_after_enabled = diag_log_size();
    const std::string log_enabled = diag_log_since(log_offset);

    (void)SetEnvironmentVariableA(CPLAT_CONSOLE_ATTACH_DIAG_ENV, nullptr);
    int ret_disabled = cplat_console_write(
        CPLAT_STREAM_STDOUT,
        "env disabled"); // [手順] - 呼び出し開始時に診断無効の状態で失敗経路を実行する (途中で有効へ変更)。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, ret_enabled); // [確認_異常系] - 有効時の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_THAT(log_enabled, HasSubstr("GetStdHandle=")); // [確認_正常系] - 同じ有効状態で GetStdHandle 行が出力されること。
    EXPECT_THAT(log_enabled, HasSubstr("WriteConsoleU result=0")); // [確認_正常系] - 同じ有効状態で WriteConsoleU 行が出力されること。
    EXPECT_THAT(log_enabled, HasSubstr("WriteFile result=0")); // [確認_正常系] - 同じ有効状態で WriteFile 行が出力されること。
    EXPECT_THAT(log_enabled, HasSubstr("console_write failed")); // [確認_正常系] - 同じ有効状態で最終失敗行が出力されること。
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, ret_disabled); // [確認_異常系] - 無効時の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(size_after_enabled, diag_log_size()); // [確認_正常系] - 無効時は途中で環境変数を変えても診断行が追記されないこと。

    // Cleanup
    (void)CloseHandle(readonly_handle);
    (void)DeleteFileW(file_path.c_str());
}

// 診断が有効でも失敗後の実 GetLastError が失敗 API の値のまま保存・復元されることの確認
// [サブ手順参照 名前=consoleWriteDiagTest.SetUp]
// [サブ手順参照 名前=consoleWriteDiagTest.TearDown]
TEST_F(consoleWriteDiagTest, preserves_and_restores_get_last_error_when_diag_is_enabled)
{
    // Arrange
    HANDLE dummy_handle = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x1234U));

    // Pre-Assert
    EXPECT_CALL(mock_windows, WriteFile(_, _, _, Ne(dummy_handle), _, _, _, _))
        .Times(AnyNumber())
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_正常系] - 診断ログ ファイル自身の WriteFile() 呼び出しは実ファイルへ通すこと。
    EXPECT_CALL(mock_windows, GetStdHandle(_, _, _, STD_OUTPUT_HANDLE))
        .WillOnce(Return(dummy_handle)); // [Pre-Assert確認_正常系] - GetStdHandle() がダミー ハンドルを返すこと。
    EXPECT_CALL(mock_cplat, WriteConsoleU(dummy_handle, _, _, _, nullptr))
        .WillOnce(Return(FALSE)); // [Pre-Assert確認_異常系] - WriteConsoleU() が FALSE を返すこと。
    EXPECT_CALL(mock_windows, WriteFile(_, _, _, dummy_handle, _, _, _, _))
        .WillOnce(Return(FALSE)); // [Pre-Assert確認_異常系] - WriteFile() が FALSE を返すこと。
    ON_CALL(mock_windows, GetLastError(_, _, _))
        .WillByDefault(Return(ERROR_ACCESS_DENIED)); // [Pre-Assert手順] - 失敗後の GetLastError() が 5 (ERROR_ACCESS_DENIED) を返すようにする。

    // Act
    SetLastError(ERROR_SUCCESS);
    int actual_ret = cplat_console_write(
        CPLAT_STREAM_STDOUT,
        "preserve last error"); // [手順] - 診断有効かつ GetLastError() が 5 を返す状態で失敗させる。
    DWORD actual_last_error = ::GetLastError();

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret); // [確認_異常系] - 戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(static_cast<DWORD>(ERROR_ACCESS_DENIED),
              actual_last_error); // [確認_異常系] - テスト翻訳単位の実 GetLastError() が 5 のままであること。
}

#endif /* PLATFORM_WINDOWS */
