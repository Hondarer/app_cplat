#include <cplat/base/platform.h>
#if defined(PLATFORM_WINDOWS)
    // testfw.h が using namespace std; を行う前に Windows SDK の rpcndr.h (byte typedef) を取り込む。
    #include <cplat/base/windows_sdk.h>
    #include <shellapi.h>
    #include <aclapi.h>
#endif /* PLATFORM_WINDOWS */

#include <testfw.h>
#include <mock_cplat.h>
#include <mock_unistd.h>
#include <cplat/base/result.h>
#include <cplat/runtime/elevated_process.h>

using testing::_;
using testing::NiceMock;
using testing::Return;

#if defined(PLATFORM_WINDOWS)
    #include <cplat/runtime/process_internal.h>
    #include <mock_shellapi.h>
    #include <mock_stdio.h>
    #include <mock_windows.h>
    #include <atomic>
    #include <chrono>
    #include <cstring>
    #include <string>
    #include <thread>
    #include <vector>

// このテストでは process_internal の実体を持たないため、リンク用の fake として非 null の偽ポインターを返す。
extern "C" cplat_process *cplat_internal_process_adopt_native(intptr_t native_handle)
{
    (void)native_handle;
    static int dummy_process = 0;
    return reinterpret_cast<cplat_process *>(&dummy_process);
}
#endif /* PLATFORM_WINDOWS */

// 昇格結果報告先が未検出の場合に出力フラグが 0 になることの確認
TEST(elevatedProcessTest, elevated_result_target_initializes_output)
{
    // Arrange
    int argc = 1;
    char program[] = "resultBehaviorTest";
    char *argv[] = {program, nullptr};
    int detected = 1;

    // Pre-Assert

    // Act
    int result = cplat_elevated_process_extract_result_target(
        &argc, argv,
        &detected); // [手順] - 結果報告先フラグのない引数を指定して cplat_elevated_process_extract_result_target() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        result); // [確認_正常系] - cplat_elevated_process_extract_result_target() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, detected); // [確認_正常系] - detected が 0 であること。(結果報告先が未検出であることの確認)
}

#if defined(PLATFORM_LINUX)
// Linux の root 判定が geteuid の結果を elevated へ反映することの確認
TEST(elevatedProcessTest, is_elevated_reflects_effective_user_id)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    int elevated = -1;

    // Pre-Assert
    EXPECT_CALL(mock_unistd, geteuid(_, _, _))
        .WillOnce(Return(static_cast<uid_t>(0))); // [Pre-Assert確認_正常系] - geteuid() が 1 回呼び出されること。
                                                  // [Pre-Assert手順] - geteuid() から実効ユーザー ID 0 を返す。

    // Act
    int root_result = cplat_elevated_process_is_elevated(
        &elevated); // [手順] - geteuid() が 0 を返す状態で cplat_elevated_process_is_elevated() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              root_result); // [確認_正常系] - cplat_elevated_process_is_elevated() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        1, elevated); // [確認_正常系] - elevated が 1 であること。(実効ユーザー ID 0 が昇格状態と判定されることの確認)
}

// Linux の非 root 判定が elevated を 0 にすることの確認
TEST(elevatedProcessTest, is_elevated_rejects_non_root_as_not_elevated)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    int elevated = -1;

    // Pre-Assert
    EXPECT_CALL(mock_unistd, geteuid(_, _, _))
        .WillOnce(Return(static_cast<uid_t>(1000))); // [Pre-Assert確認_正常系] - geteuid() が 1 回呼び出されること。
                                                     // [Pre-Assert手順] - geteuid() から実効ユーザー ID 1000 を返す。

    // Act
    int result = cplat_elevated_process_is_elevated(
        &elevated); // [手順] - geteuid() が 1000 を返す状態で cplat_elevated_process_is_elevated() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_elevated_process_is_elevated() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        0,
        elevated); // [確認_正常系] - elevated が 0 であること。(実効ユーザー ID 1000 が非昇格状態と判定されることの確認)
}
#endif /* PLATFORM_LINUX */

// 昇格判定 API が NULL 出力を拒否することの確認
TEST(elevatedProcessTest, elevated_apis_reject_null_outputs)
{
    // Arrange
    int exit_code = -1;
    int handled = -1;

    // Pre-Assert

    // Act
    int is_elevated_result = cplat_elevated_process_is_elevated(
        nullptr); // [手順] - 昇格状態の出力先に nullptr を指定して cplat_elevated_process_is_elevated() を呼び出す。
    int run_result = cplat_elevated_process_run_if_needed(
        nullptr, nullptr,
        &handled); // [手順] - exit_code に nullptr を指定して cplat_elevated_process_run_if_needed() を呼び出す。
    int run_handled_result = cplat_elevated_process_run_if_needed(
        nullptr, &exit_code,
        nullptr); // [手順] - handled に nullptr を指定して cplat_elevated_process_run_if_needed() を呼び出す。
    int result_run_with_result = cplat_elevated_process_run_with_result(
        nullptr, &exit_code, nullptr, nullptr,
        0U); // [手順] - handled に nullptr を指定して cplat_elevated_process_run_with_result() を呼び出す。
    int result_run_with_null_exit = cplat_elevated_process_run_with_result(
        nullptr, nullptr, &handled, nullptr,
        0U); // [手順] - exit_code に nullptr を指定して cplat_elevated_process_run_with_result() を呼び出す。
    int report_null_result = cplat_elevated_process_report_result(
        nullptr); // [手順] - message に nullptr を指定して cplat_elevated_process_report_result() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        is_elevated_result); // [確認_異常系] - cplat_elevated_process_is_elevated() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        run_result); // [確認_異常系] - exit_code が nullptr の cplat_elevated_process_run_if_needed() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        run_handled_result); // [確認_異常系] - handled が nullptr の cplat_elevated_process_run_if_needed() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        result_run_with_result); // [確認_異常系] - handled が nullptr の cplat_elevated_process_run_with_result() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        result_run_with_null_exit); // [確認_異常系] - exit_code が nullptr の cplat_elevated_process_run_with_result() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        report_null_result); // [確認_異常系] - cplat_elevated_process_report_result() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// run_piped が NULL 出力を拒否することの確認
TEST(elevatedProcessTest, run_piped_rejects_null_outputs)
{
    // Arrange
    int exit_code = -1;
    int handled = -1;

    // Pre-Assert

    // Act
    int null_exit_result = cplat_elevated_process_run_piped(
        nullptr, nullptr, nullptr, nullptr,
        &handled); // [手順] - exit_code に nullptr を指定して cplat_elevated_process_run_piped() を呼び出す。
    int null_handled_result = cplat_elevated_process_run_piped(
        nullptr, nullptr, nullptr, &exit_code,
        nullptr); // [手順] - handled に nullptr を指定して cplat_elevated_process_run_piped() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_exit_result); // [確認_異常系] - exit_code が nullptr の cplat_elevated_process_run_piped() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_handled_result); // [確認_異常系] - handled が nullptr の cplat_elevated_process_run_piped() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// 出力パイプ フラグがない場合に attach_output_pipes が何もしないことの確認
TEST(elevatedProcessTest, attach_output_pipes_without_flag_keeps_arguments)
{
    // Arrange
    char program[] = "pipeTest";
    char command[] = "install";
    char *argv[] = {program, command, nullptr};
    int argc = 2;
    int attached = -1;

    // Pre-Assert

    // Act
    int result = cplat_elevated_process_attach_output_pipes(
        &argc, argv,
        &attached); // [手順] - 出力パイプ フラグのない引数を指定して cplat_elevated_process_attach_output_pipes() を呼び出す。
    int null_result = cplat_elevated_process_attach_output_pipes(
        nullptr, nullptr,
        nullptr); // [手順] - すべての引数に nullptr を指定して cplat_elevated_process_attach_output_pipes() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        result); // [確認_正常系] - フラグがない場合の cplat_elevated_process_attach_output_pipes() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, attached);           // [確認_正常系] - attached が 0 であること。(パイプが接続されないことの確認)
    EXPECT_EQ(2, argc);               // [確認_正常系] - argc が 2 であること。(引数の個数が変更されないことの確認)
    EXPECT_STREQ("install", argv[1]); // [確認_正常系] - argv[1] が "install" であること。
    EXPECT_EQ(
        CPLAT_OK,
        null_result); // [確認_正常系] - nullptr 引数の cplat_elevated_process_attach_output_pipes() の戻り値が CPLAT_OK であること。
}

#if defined(PLATFORM_WINDOWS)
// 出力パイプ フラグの値が不正な場合にフラグを取り除いて失敗することの確認
TEST(elevatedProcessTest, attach_output_pipes_removes_malformed_flag_and_fails)
{
    // Arrange
    char program[] = "pipeTest";
    char flag[] = "--cplat-output-pipes=123-xyz";
    char command[] = "install";
    char *argv[] = {program, flag, command, nullptr};
    int argc = 3;
    int attached = -1;

    // Pre-Assert

    // Act
    int result = cplat_elevated_process_attach_output_pipes(
        &argc, argv,
        &attached); // [手順] - 不正な文字を含むトークンのフラグを指定して cplat_elevated_process_attach_output_pipes() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        result); // [確認_異常系] - cplat_elevated_process_attach_output_pipes() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(0, attached); // [確認_異常系] - attached が 0 であること。(パイプが接続されないことの確認)
    EXPECT_EQ(2, argc); // [確認_異常系] - argc が 2 であること。(フラグの除去で引数の個数が 3 から 1 減ることの確認)
    EXPECT_STREQ("install",
                 argv[1]); // [確認_異常系] - argv[1] が "install" であること。(後続の引数が前へ詰められることの確認)
    EXPECT_EQ(nullptr, argv[2]); // [確認_異常系] - argv[2] が nullptr であること。
}

// 出力パイプ フラグから開いたオブジェクトがパイプ以外の場合に付け替えないことの確認
TEST(elevatedProcessTest, attach_output_pipes_rejects_non_pipe_object)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    HANDLE fake_stdout = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x210U));
    HANDLE fake_stderr = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x310U));
    std::string flag_text = "--cplat-output-pipes=" + std::to_string(GetCurrentProcessId()) +
                            "-0123456789abcdef0123456789abcdef";
    char program[] = "pipeTest";
    char *argv[] = {program, &flag_text[0], nullptr};
    int argc = 2;
    int attached = -1;
    HANDLE stdout_before = GetStdHandle(STD_OUTPUT_HANDLE);

    // Pre-Assert
    EXPECT_CALL(mock_windows, CreateFileW(_, _, _, _, GENERIC_WRITE, 0U, nullptr, OPEN_EXISTING,
                                          SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS, nullptr))
        .WillOnce(Return(fake_stdout))
        .WillOnce(Return(fake_stderr));
    // [Pre-Assert確認_異常系] - CreateFileW() が 2 回呼び出されること。
    // [Pre-Assert手順] - CreateFileW() から偽の stdout と stderr ハンドルを返す。
    EXPECT_CALL(mock_windows, GetFileType(_, _, _, fake_stdout))
        .WillOnce(Return(static_cast<DWORD>(FILE_TYPE_DISK)));
    // [Pre-Assert確認_異常系] - GetFileType() が fake_stdout を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - GetFileType() から FILE_TYPE_DISK を返す。
    EXPECT_CALL(mock_windows, CloseHandle(_, _, _, fake_stdout)).WillOnce(Return(TRUE));
    // [Pre-Assert確認_異常系] - 後始末で fake_stdout の CloseHandle() が 1 回呼び出されること。
    // [Pre-Assert手順] - CloseHandle() から TRUE を返す。
    EXPECT_CALL(mock_windows, CloseHandle(_, _, _, fake_stderr)).WillOnce(Return(TRUE));
    // [Pre-Assert確認_異常系] - 後始末で fake_stderr の CloseHandle() が 1 回呼び出されること。
    // [Pre-Assert手順] - CloseHandle() から TRUE を返す。
    EXPECT_CALL(mock_windows, SetStdHandle(_, _, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - SetStdHandle() が呼び出されないこと。

    // Act
    int result = cplat_elevated_process_attach_output_pipes(
        &argc, argv,
        &attached); // [手順] - GetFileType() が FILE_TYPE_DISK を返す状態で cplat_elevated_process_attach_output_pipes() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        result); // [確認_異常系] - cplat_elevated_process_attach_output_pipes() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(0, attached); // [確認_異常系] - attached が 0 であること。(パイプが接続されないことの確認)
    EXPECT_EQ(1, argc); // [確認_異常系] - argc が 1 であること。(フラグの除去で引数の個数が 2 から 1 減ることの確認)
    EXPECT_EQ(
        stdout_before,
        GetStdHandle(
            STD_OUTPUT_HANDLE)); // [確認_異常系] - 標準出力に対する GetStdHandle() の戻り値が stdout_before と一致すること。(標準出力ハンドルが変更されないことの確認)
}

namespace
{

// 付け替えの検証で使う偽のハンドル。実際の OS のハンドルとして閉じないよう、CloseHandle() も差し替える。
HANDLE const k_stdout_pipe = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x200U));
HANDLE const k_stderr_pipe = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x300U));
HANDLE const k_stdin_null = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x400U));

bool ends_with_wide(const wchar_t *text, const wchar_t *suffix)
{
    if (text == nullptr || suffix == nullptr)
    {
        return false;
    }

    const size_t text_len = wcslen(text);
    const size_t suffix_len = wcslen(suffix);
    if (text_len < suffix_len)
    {
        return false;
    }

    return wcscmp(text + text_len - suffix_len, suffix) == 0;
}

// [サブ手順 名前=elevatedProcessTest.setup_attach_output_pipes_mocks]
// 付け替えの最後 (setvbuf) まで到達するよう、途中の OS と C ランタイムの関数をすべて成功させる。
void setup_attach_output_pipes_mocks(Mock_windows &mock_windows, Mock_stdio &mock_stdio, Mock_unistd &mock_unistd)
{
    ON_CALL(mock_windows, CreateFileW(_, _, _, _, _, _, _, _, _, _))
        .WillByDefault(
            [](const char *, const int, const char *, LPCWSTR file_name, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD,
               DWORD, HANDLE) -> HANDLE
            {
                if (file_name != nullptr && wcscmp(file_name, L"NUL") == 0)
                {
                    return k_stdin_null;
                }
                if (ends_with_wide(file_name, L"-out"))
                {
                    return k_stdout_pipe;
                }
                if (ends_with_wide(file_name, L"-err"))
                {
                    return k_stderr_pipe;
                }
                return INVALID_HANDLE_VALUE;
            });
    // [状態] - CreateFileW() が対象名 (-out、-err、NUL) に応じて別の偽のハンドルを返すように既定動作を設定する。
    ON_CALL(mock_windows, CloseHandle(_, _, _, _)).WillByDefault(Return(TRUE));
    // [状態] - 偽のハンドルを閉じないよう、CloseHandle() から TRUE を返すように既定動作を設定する。
    ON_CALL(mock_windows, GetFileType(_, _, _, _)).WillByDefault(Return(static_cast<DWORD>(FILE_TYPE_PIPE)));
    // [状態] - GetFileType() から FILE_TYPE_PIPE を返すように既定動作を設定する。
    ON_CALL(mock_windows, SetStdHandle(_, _, _, _, _)).WillByDefault(Return(TRUE));
    // [状態] - 実際の標準ハンドルを変えないよう、SetStdHandle() から TRUE を返すように既定動作を設定する。
    ON_CALL(mock_stdio, _fileno(_, _, _, _)).WillByDefault(Return(7));
    // [状態] - _fileno() から 7 を返すように既定動作を設定する。
    ON_CALL(mock_stdio, fflush(_, _, _, _)).WillByDefault(Return(0));
    // [状態] - fflush() から 0 を返すように既定動作を設定する。
    ON_CALL(mock_stdio, setvbuf(_, _, _, _, _, _, _)).WillByDefault(Return(0));
    // [状態] - 実際のバッファリングを変えないよう、setvbuf() から 0 を返すように既定動作を設定する。
    ON_CALL(mock_unistd, _close(_, _, _, _)).WillByDefault(Return(0));
    // [状態] - 実際の標準ストリームを閉じないよう、_close() から 0 を返すように既定動作を設定する。
    ON_CALL(mock_unistd, _open_osfhandle(_, _, _, _, _)).WillByDefault(Return(7));
    // [状態] - _open_osfhandle() から _fileno() と同じ 7 を返すように既定動作を設定する。
    ON_CALL(mock_unistd, _get_osfhandle(_, _, _, _)).WillByDefault(Return(static_cast<intptr_t>(0x500U)));
    // [状態] - _get_osfhandle() から偽のハンドル値を返すように既定動作を設定する。
}
// [サブ手順終了]

// [サブ手順 名前=elevatedProcessTest.setup_run_piped_base_mocks]
// 未昇格・対話セッションの状態と、TokenUser の実委譲および偽プロセス破棄を設定する。
void setup_run_piped_base_mocks(Mock_cplat &mock_cplat, Mock_windows &mock_windows)
{
    ON_CALL(mock_windows, GetTokenInformation(_, _, _, _, _, _, _, _))
        .WillByDefault(
            [](const char *file, const int line, const char *func, HANDLE token_handle,
               TOKEN_INFORMATION_CLASS info_class, LPVOID info, DWORD info_len, PDWORD return_len) -> BOOL
            {
                if (info_class == TokenElevation)
                {
                    if (info != nullptr && info_len >= sizeof(TOKEN_ELEVATION))
                    {
                        static_cast<TOKEN_ELEVATION *>(info)->TokenIsElevated = 0;
                    }
                    if (return_len != nullptr)
                    {
                        *return_len = static_cast<DWORD>(sizeof(TOKEN_ELEVATION));
                    }
                    return TRUE;
                }
                return delegate_real_GetTokenInformation(file, line, func, token_handle, info_class, info, info_len,
                                                         return_len);
            });
    // [状態] - GetTokenInformation() は TokenElevation で未昇格 (0) を返し、TokenUser 等は実物へ委譲するように既定動作を設定する。
    ON_CALL(mock_windows, ProcessIdToSessionId(_, _, _, _, _))
        .WillByDefault(DoAll(SetArgPointee<4>(1U), Return(TRUE)));
    // [状態] - ProcessIdToSessionId() から対話セッション ID 1 を設定して TRUE を返すように既定動作を設定する。
    ON_CALL(mock_cplat, cplat_process_dispose(_)).WillByDefault(Return());
    // [状態] - リンク用の fake ポインターを解放しないよう、cplat_process_dispose() を無効化する既定動作を設定する。
}
// [サブ手順終了]

std::wstring extract_pipe_token_from_wide_params(LPCWSTR params)
{
    if (params == nullptr)
    {
        return L"";
    }

    const wchar_t prefix[] = L"--cplat-output-pipes=";
    const wchar_t *found = wcsstr(params, prefix);
    if (found == nullptr)
    {
        return L"";
    }

    const wchar_t *start = found + (sizeof(prefix) / sizeof(prefix[0]) - 1U);
    const wchar_t *end = start;
    while (*end != L'\0' && *end != L' ')
    {
        end++;
    }

    return std::wstring(start, static_cast<size_t>(end - start));
}

} // namespace

// 出力パイプへの付け替えがすべて成功した場合に attached_out が 1 になることの確認
TEST(elevatedProcessTest, attach_output_pipes_succeeds_when_all_steps_succeed)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    NiceMock<Mock_stdio> mock_stdio;
    NiceMock<Mock_unistd> mock_unistd;
    // [サブ手順参照 名前=elevatedProcessTest.setup_attach_output_pipes_mocks]
    setup_attach_output_pipes_mocks(mock_windows, mock_stdio, mock_unistd);
    const std::string token = std::to_string(GetCurrentProcessId()) + "-0123456789abcdef0123456789abcdef";
    const std::wstring wide_token(token.begin(), token.end());
    const std::wstring expected_out = L"\\\\.\\pipe\\cplat-elevated-" + wide_token + L"-out";
    const std::wstring expected_err = L"\\\\.\\pipe\\cplat-elevated-" + wide_token + L"-err";
    std::string flag_text = "--cplat-output-pipes=" + token;
    char program[] = "pipeTest";
    char *argv[] = {program, &flag_text[0], nullptr};
    int argc = 2;
    int attached = -1;

    // Pre-Assert
    EXPECT_CALL(mock_windows, CreateFileW(_, _, _, StrEq(expected_out.c_str()), GENERIC_WRITE, 0U, nullptr,
                                          OPEN_EXISTING, SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS, nullptr))
        .Times(1)
        .WillOnce(Return(k_stdout_pipe));
    // [Pre-Assert確認_正常系] - CreateFileW() が期待する stdout パイプ名と匿名偽装フラグで 1 回呼び出されること。
    // [Pre-Assert手順] - CreateFileW() から偽の stdout パイプを返す。
    EXPECT_CALL(mock_windows, CreateFileW(_, _, _, StrEq(expected_err.c_str()), GENERIC_WRITE, 0U, nullptr,
                                          OPEN_EXISTING, SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS, nullptr))
        .Times(1)
        .WillOnce(Return(k_stderr_pipe));
    // [Pre-Assert確認_正常系] - CreateFileW() が期待する stderr パイプ名と匿名偽装フラグで 1 回呼び出されること。
    // [Pre-Assert手順] - CreateFileW() から偽の stderr パイプを返す。
    EXPECT_CALL(mock_windows, CreateFileW(_, _, _, StrEq(L"NUL"), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                          nullptr, OPEN_EXISTING, 0U, nullptr))
        .Times(1)
        .WillOnce(Return(k_stdin_null));
    // [Pre-Assert確認_正常系] - CreateFileW() が NUL デバイスで 1 回呼び出されること。
    // [Pre-Assert手順] - CreateFileW() から偽の NUL ハンドルを返す。
    EXPECT_CALL(mock_windows, SetStdHandle(_, _, _, _, _))
        .Times(3); // [Pre-Assert確認_正常系] - SetStdHandle() が 3 回呼び出されること。(stdout、stderr、stdin の付け替えの確認)
    EXPECT_CALL(mock_stdio, setvbuf(_, _, _, stdout, nullptr, _IONBF, 0U))
        .Times(1); // [Pre-Assert確認_正常系] - setvbuf() が stdout と _IONBF を指定して 1 回呼び出されること。

    // Act
    int result = cplat_elevated_process_attach_output_pipes(
        &argc, argv, &attached); // [手順] - 付け替えがすべて成功する状態で cplat_elevated_process_attach_output_pipes() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_elevated_process_attach_output_pipes() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1, attached); // [確認_正常系] - attached が 1 であること。
    EXPECT_EQ(1, argc);     // [確認_正常系] - argc が 1 であること。(フラグが取り除かれることの確認)
}

// 標準ハンドルの付け替えに失敗した場合に CPLAT_ERR_UNKNOWN を返すことの確認
TEST(elevatedProcessTest, attach_output_pipes_fails_when_set_std_handle_fails)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    NiceMock<Mock_stdio> mock_stdio;
    NiceMock<Mock_unistd> mock_unistd;
    // [サブ手順参照 名前=elevatedProcessTest.setup_attach_output_pipes_mocks]
    setup_attach_output_pipes_mocks(mock_windows, mock_stdio, mock_unistd);
    std::string flag_text = "--cplat-output-pipes=" + std::to_string(GetCurrentProcessId()) +
                            "-0123456789abcdef0123456789abcdef";
    char program[] = "pipeTest";
    char *argv[] = {program, &flag_text[0], nullptr};
    int argc = 2;
    int attached = -1;

    // Pre-Assert
    EXPECT_CALL(mock_windows, SetStdHandle(_, _, _, STD_OUTPUT_HANDLE, _))
        .WillOnce(Return(FALSE)); // [Pre-Assert確認_異常系] - SetStdHandle() が STD_OUTPUT_HANDLE を指定して 1 回呼び出されること。
                                  // [Pre-Assert手順] - SetStdHandle() から FALSE を返す。
    EXPECT_CALL(mock_windows, CloseHandle(_, _, _, k_stderr_pipe))
        .Times(1); // [Pre-Assert確認_異常系] - 失敗の後始末で stderr パイプの CloseHandle() が 1 回呼び出されること。
    EXPECT_CALL(mock_windows, CloseHandle(_, _, _, k_stdin_null))
        .Times(1); // [Pre-Assert確認_異常系] - 失敗の後始末で NUL デバイスの CloseHandle() が 1 回呼び出されること。
    EXPECT_CALL(mock_stdio, setvbuf(_, _, _, _, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - setvbuf() が呼び出されないこと。

    // Act
    int result = cplat_elevated_process_attach_output_pipes(
        &argc, argv, &attached); // [手順] - SetStdHandle() が失敗する状態で cplat_elevated_process_attach_output_pipes() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        result); // [確認_異常系] - cplat_elevated_process_attach_output_pipes() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(0, attached); // [確認_異常系] - attached が 0 であること。
}

// stdout をバッファーなしにできない場合に CPLAT_ERR_UNKNOWN を返すことの確認
TEST(elevatedProcessTest, attach_output_pipes_fails_when_setvbuf_fails)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    NiceMock<Mock_stdio> mock_stdio;
    NiceMock<Mock_unistd> mock_unistd;
    // [サブ手順参照 名前=elevatedProcessTest.setup_attach_output_pipes_mocks]
    setup_attach_output_pipes_mocks(mock_windows, mock_stdio, mock_unistd);
    std::string flag_text = "--cplat-output-pipes=" + std::to_string(GetCurrentProcessId()) +
                            "-0123456789abcdef0123456789abcdef";
    char program[] = "pipeTest";
    char *argv[] = {program, &flag_text[0], nullptr};
    int argc = 2;
    int attached = -1;

    // Pre-Assert
    EXPECT_CALL(mock_stdio, setvbuf(_, _, _, stdout, nullptr, _IONBF, 0U))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - setvbuf() が stdout と _IONBF を指定して 1 回呼び出されること。
                               // [Pre-Assert手順] - setvbuf() から -1 を返す。

    // Act
    int result = cplat_elevated_process_attach_output_pipes(
        &argc, argv, &attached); // [手順] - setvbuf() が失敗する状態で cplat_elevated_process_attach_output_pipes() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        result); // [確認_異常系] - cplat_elevated_process_attach_output_pipes() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(0, attached); // [確認_異常系] - attached が 0 であること。
}

// stdout パイプを開けない場合にフラグを取り除いて失敗することの確認
TEST(elevatedProcessTest, attach_output_pipes_fails_when_pipe_cannot_be_opened)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    std::string flag_text = "--cplat-output-pipes=" + std::to_string(GetCurrentProcessId()) +
                            "-0123456789abcdef0123456789abcdef";
    char program[] = "pipeTest";
    char *argv[] = {program, &flag_text[0], nullptr};
    int argc = 2;
    int attached = -1;

    // Pre-Assert
    EXPECT_CALL(mock_windows, CreateFileW(_, _, _, _, _, _, _, _, _, _))
        .WillOnce(Return(INVALID_HANDLE_VALUE));
    // [Pre-Assert確認_異常系] - stdout パイプの CreateFileW() が 1 回呼び出されること。
    // [Pre-Assert手順] - CreateFileW() から INVALID_HANDLE_VALUE を返す。
    EXPECT_CALL(mock_windows, SetStdHandle(_, _, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - SetStdHandle() が呼び出されないこと。

    // Act
    int actual_ret = cplat_elevated_process_attach_output_pipes(
        &argc, argv, &attached); // [手順] - stdout パイプを開けない状態で cplat_elevated_process_attach_output_pipes() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret); // [確認_異常系] - cplat_elevated_process_attach_output_pipes() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(0, attached); // [確認_異常系] - attached が 0 であること。
    EXPECT_EQ(1, argc);     // [確認_異常系] - argc が 1 であること。(フラグが取り除かれることの確認)
}

// stderr パイプを開けない場合に stdout パイプを閉じて失敗することの確認
TEST(elevatedProcessTest, attach_output_pipes_closes_stdout_pipe_when_stderr_pipe_cannot_be_opened)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    std::string flag_text = "--cplat-output-pipes=" + std::to_string(GetCurrentProcessId()) +
                            "-0123456789abcdef0123456789abcdef";
    char program[] = "pipeTest";
    char *argv[] = {program, &flag_text[0], nullptr};
    int argc = 2;
    int attached = -1;

    // Pre-Assert
    EXPECT_CALL(mock_windows, CreateFileW(_, _, _, _, _, _, _, _, _, _))
        .WillOnce(Return(k_stdout_pipe))
        .WillOnce(Return(INVALID_HANDLE_VALUE));
    // [Pre-Assert確認_異常系] - CreateFileW() が 2 回呼び出されること。
    // [Pre-Assert手順] - 1 回目 (-out) は偽の stdout ハンドル、2 回目 (-err) は INVALID_HANDLE_VALUE を返す。
    EXPECT_CALL(mock_windows, CloseHandle(_, _, _, k_stdout_pipe))
        .Times(1)
        .WillOnce(Return(TRUE));
    // [Pre-Assert確認_異常系] - 開いた stdout ハンドルの CloseHandle() が 1 回呼び出されること。
    // [Pre-Assert手順] - CloseHandle() から TRUE を返す。

    // Act
    int actual_ret = cplat_elevated_process_attach_output_pipes(
        &argc, argv, &attached); // [手順] - stderr パイプだけ開けない状態で cplat_elevated_process_attach_output_pipes() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret); // [確認_異常系] - cplat_elevated_process_attach_output_pipes() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(0, attached); // [確認_異常系] - attached が 0 であること。
}

// パス区切りやコロンなど不正な文字を含むトークンを拒否し CreateFileW を呼ばないことの確認
TEST(elevatedProcessTest, attach_output_pipes_rejects_flag_value_with_path_characters)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    char program[] = "pipeTest";
    char flag[] = "--cplat-output-pipes=123\\../a:b";
    char *argv[] = {program, flag, nullptr};
    int argc = 2;
    int attached = -1;

    // Pre-Assert
    EXPECT_CALL(mock_windows, CreateFileW(_, _, _, _, _, _, _, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - CreateFileW() が呼び出されないこと。

    // Act
    int actual_ret = cplat_elevated_process_attach_output_pipes(
        &argc, argv, &attached); // [手順] - パス文字を含むトークンを指定して cplat_elevated_process_attach_output_pipes() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret); // [確認_異常系] - cplat_elevated_process_attach_output_pipes() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(0, attached); // [確認_異常系] - attached が 0 であること。
    EXPECT_EQ(1, argc);     // [確認_異常系] - argc が 1 であること。(フラグが取り除かれることの確認)
}
#endif /* PLATFORM_WINDOWS */

#if defined(PLATFORM_LINUX)
// Linux では root の場合だけ run_piped が完了扱いにすることの確認
TEST(elevatedProcessTest, run_piped_reports_linux_elevation_state)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    int root_exit_code = -1;
    int root_handled = -1;
    int user_exit_code = 0;
    int user_handled = -1;

    // Pre-Assert
    EXPECT_CALL(mock_unistd, geteuid(_, _, _))
        .WillOnce(Return(static_cast<uid_t>(0)))
        .WillOnce(Return(static_cast<uid_t>(1000))); // [Pre-Assert確認_正常系] - geteuid() が 2 回呼び出されること。
    // [Pre-Assert手順] - geteuid() から 1 回目は 0、2 回目は 1000 を返す。

    // Act
    int root_result = cplat_elevated_process_run_piped(
        "--test", nullptr, nullptr, &root_exit_code,
        &root_handled); // [手順] - geteuid() が 0 を返す状態で cplat_elevated_process_run_piped() を呼び出す。
    int user_result = cplat_elevated_process_run_piped(
        "--test", nullptr, nullptr, &user_exit_code,
        &user_handled); // [手順] - geteuid() が 1000 を返す状態で cplat_elevated_process_run_piped() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        root_result); // [確認_正常系] - root 状態の cplat_elevated_process_run_piped() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, root_exit_code); // [確認_正常系] - root_exit_code が 0 であること。
    EXPECT_EQ(0,
              root_handled); // [確認_正常系] - root_handled が 0 であること。(Linux では昇格処理を実行しないことの確認)
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        user_result); // [確認_異常系] - 非 root 状態の cplat_elevated_process_run_piped() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(EXIT_FAILURE, user_exit_code); // [確認_異常系] - user_exit_code が EXIT_FAILURE であること。
    EXPECT_EQ(0,
              user_handled); // [確認_異常系] - user_handled が 0 であること。(Linux では昇格処理を実行しないことの確認)
}
#endif /* PLATFORM_LINUX */

// Linux 固有 API が省略可能な出力と結果報告を処理することの確認
TEST(elevatedProcessTest, linux_helpers_accept_optional_output_and_report_unavailable_target)
{
    // Arrange
    int argc = 1;
    char program[] = "resultBehaviorTest";
    char *argv[] = {program, nullptr};

    // Pre-Assert

    // Act
    int extract_result = cplat_elevated_process_extract_result_target(
        &argc, argv,
        nullptr); // [手順] - detected_out に nullptr を指定して cplat_elevated_process_extract_result_target() を呼び出す。
#if defined(PLATFORM_LINUX)
    int report_result = cplat_elevated_process_report_result(
        "result"); // [手順] - Linux で結果メッセージ "result" を指定して cplat_elevated_process_report_result() を呼び出す。
#elif defined(PLATFORM_WINDOWS)
    (void)cplat_elevated_process_report_result(
        "result"); // [手順] - Windows でも戻り値を使わずに結果メッセージ "result" を指定して cplat_elevated_process_report_result() を呼び出す。
#endif /* PLATFORM_ */

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        extract_result); // [確認_正常系] - detected_out を省略した cplat_elevated_process_extract_result_target() の戻り値が CPLAT_OK であること。
#if defined(PLATFORM_LINUX)
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        report_result); // [確認_異常系] - Linux の cplat_elevated_process_report_result() の戻り値が CPLAT_ERR_UNKNOWN であること。
#endif                  /* PLATFORM_LINUX */
}

#if defined(PLATFORM_LINUX)
// Linux では root の場合だけ昇格処理を完了扱いにすることの確認
TEST(elevatedProcessTest, run_apis_report_linux_elevation_state)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    int exit_code = -1;
    int handled = -1;
    char result_message[16] = "stale";

    // Pre-Assert
    EXPECT_CALL(mock_unistd, geteuid(_, _, _))
        .Times(2)
        .WillRepeatedly(Return(static_cast<uid_t>(0))); // [Pre-Assert確認_正常系] - geteuid() が 2 回呼び出されること。
                                                        // [Pre-Assert手順] - geteuid() から実効ユーザー ID 0 を返す。

    // Act
    int run_result = cplat_elevated_process_run_if_needed(
        "--test", &exit_code,
        &handled); // [手順] - root 状態で cplat_elevated_process_run_if_needed() を呼び出す。
    int result_run_with_result = cplat_elevated_process_run_with_result(
        "--test", &exit_code, &handled, result_message,
        sizeof(result_message)); // [手順] - root 状態で cplat_elevated_process_run_with_result() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        run_result); // [確認_正常系] - root 状態の cplat_elevated_process_run_if_needed() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, exit_code); // [確認_正常系] - exit_code が 0 であること。
    EXPECT_EQ(0, handled);   // [確認_正常系] - handled が 0 であること。(Linux では昇格処理を実行しないことの確認)
    EXPECT_EQ(
        CPLAT_OK,
        result_run_with_result); // [確認_正常系] - root 状態の cplat_elevated_process_run_with_result() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, handled); // [確認_正常系] - handled が 0 であること。(Linux では昇格処理を実行しないことの確認)
    EXPECT_STREQ("", result_message); // [確認_正常系] - result_message が "" であること。
}

// Linux の結果バッファー初期化が NULL とサイズ 0 を受理することの確認
TEST(elevatedProcessTest, run_with_result_accepts_absent_or_zero_sized_message_buffer)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    int exit_code = -1;
    int handled = -1;
    char result_message[2] = "x";

    // Pre-Assert
    EXPECT_CALL(mock_unistd, geteuid(_, _, _))
        .Times(2)
        .WillRepeatedly(Return(static_cast<uid_t>(0))); // [Pre-Assert確認_正常系] - geteuid() が 2 回呼び出されること。
                                                        // [Pre-Assert手順] - geteuid() から実効ユーザー ID 0 を返す。

    // Act
    int null_buffer_result = cplat_elevated_process_run_with_result(
        nullptr, &exit_code, &handled, nullptr,
        1U); // [手順] - result_message に nullptr を指定して cplat_elevated_process_run_with_result() を呼び出す。
    int zero_size_result = cplat_elevated_process_run_with_result(
        nullptr, &exit_code, &handled, result_message,
        0U); // [手順] - result_message_size に 0 を指定して cplat_elevated_process_run_with_result() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        null_buffer_result); // [確認_正常系] - nullptr バッファー指定時の cplat_elevated_process_run_with_result() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        zero_size_result); // [確認_正常系] - サイズ 0 指定時の cplat_elevated_process_run_with_result() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "x",
        result_message); // [確認_正常系] - result_message が "x" であること。(サイズ 0 のバッファー内容が変更されないことの確認)
}

// Linux の非 root 状態では昇格 API が失敗を報告することの確認
TEST(elevatedProcessTest, run_apis_reject_linux_non_root)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    int exit_code = 0;
    int handled = 0;

    // Pre-Assert
    EXPECT_CALL(mock_unistd, geteuid(_, _, _))
        .Times(2)
        .WillRepeatedly(
            Return(static_cast<uid_t>(1000))); // [Pre-Assert確認_異常系] - geteuid() が 2 回呼び出されること。
                                               // [Pre-Assert手順] - geteuid() から実効ユーザー ID 1000 を返す。

    // Act
    int run_result = cplat_elevated_process_run_if_needed(
        nullptr, &exit_code, &handled); // [手順] - 非 root 状態で cplat_elevated_process_run_if_needed() を呼び出す。
    int result_run_with_result = cplat_elevated_process_run_with_result(
        nullptr, &exit_code, &handled, nullptr,
        0U); // [手順] - 非 root 状態で cplat_elevated_process_run_with_result() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        run_result); // [確認_異常系] - 非 root 状態の cplat_elevated_process_run_if_needed() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(EXIT_FAILURE, exit_code); // [確認_異常系] - exit_code が EXIT_FAILURE であること。
    EXPECT_EQ(0, handled); // [確認_異常系] - handled が 0 であること。(Linux では昇格処理を実行しないことの確認)
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        result_run_with_result); // [確認_異常系] - 非 root 状態の cplat_elevated_process_run_with_result() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(EXIT_FAILURE, exit_code); // [確認_異常系] - exit_code が EXIT_FAILURE であること。
}
#endif /* PLATFORM_LINUX */

#if defined(PLATFORM_WINDOWS)
// 長すぎる報告先を拒否したときに引数配列を変更しないことの確認
TEST(elevatedProcessTest, long_result_target_preserves_arguments)
{
    // Arrange
    std::string argument = "--cplat-result-file=" + std::string(32768, 'x');
    char program[] = "program";
    char *argv[] = {program, &argument[0], nullptr};
    char *original = argv[1];
    int argc = 2;
    int detected = 1;

    // Pre-Assert

    // Act
    int result = cplat_elevated_process_extract_result_target(&argc, argv, &detected);

    // [手順] - 保持先へ収まらない長さの結果報告先を指定する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL, result); // [確認_異常系] - 容量不足を返すこと。
    EXPECT_EQ(0, detected);                        // [確認_異常系] - 未検出の状態を返すこと。
    EXPECT_EQ(2, argc);                            // [確認_異常系] - 引数の数を変更しないこと。
    EXPECT_EQ(original, argv[1]);                  // [確認_異常系] - 引数配列を変更しないこと。
}

namespace
{

struct pipe_test_outputs
{
    std::string out;
    std::string err;
    int call_count;
};

void collect_stream_output(cplat_elevated_process_stream stream, const char *data, size_t size, void *context)
{
    auto *outputs = static_cast<pipe_test_outputs *>(context);
    outputs->call_count++;
    if (stream == CPLAT_ELEVATED_PROCESS_STREAM_STDOUT)
    {
        outputs->out.append(data, size);
    }
    else if (stream == CPLAT_ELEVATED_PROCESS_STREAM_STDERR)
    {
        outputs->err.append(data, size);
    }
}

} // namespace

// 名前付きパイプ経由で昇格プロセスの標準出力と標準エラー出力を受信できることの確認
TEST(elevatedProcessTest, run_piped_delivers_output_through_named_pipes)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    NiceMock<Mock_shellapi> mock_shellapi;
    // [サブ手順参照 名前=elevatedProcessTest.setup_run_piped_base_mocks]
    setup_run_piped_base_mocks(mock_cplat, mock_windows);
    pipe_test_outputs outputs = {};
    int exit_code = -1;
    int handled = 0;
    std::wstring captured_verb;
    int captured_show = -1;
    std::wstring captured_params;

    // Pre-Assert
    EXPECT_CALL(mock_shellapi, ShellExecuteExW(_, _, _, _))
        .WillOnce(
            [&captured_verb, &captured_show, &captured_params](const char *, const int, const char *,
                                                               SHELLEXECUTEINFOW *exec_info) -> BOOL
            {
                captured_verb = exec_info->lpVerb != nullptr ? exec_info->lpVerb : L"";
                captured_show = exec_info->nShow;
                captured_params = exec_info->lpParameters != nullptr ? exec_info->lpParameters : L"";
                exec_info->hProcess = ::GetCurrentProcess();

                const std::wstring token = extract_pipe_token_from_wide_params(exec_info->lpParameters);
                const std::wstring out_name = L"\\\\.\\pipe\\cplat-elevated-" + token + L"-out";
                const std::wstring err_name = L"\\\\.\\pipe\\cplat-elevated-" + token + L"-err";

                HANDLE out_pipe = ::CreateFileW(out_name.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                                SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS, nullptr);
                HANDLE err_pipe = ::CreateFileW(err_name.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                                SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS, nullptr);
                if (out_pipe != INVALID_HANDLE_VALUE)
                {
                    const char msg[] = "stdout-line\n";
                    DWORD written = 0;
                    ::WriteFile(out_pipe, msg, static_cast<DWORD>(sizeof(msg) - 1U), &written, nullptr);
                    ::CloseHandle(out_pipe);
                }
                if (err_pipe != INVALID_HANDLE_VALUE)
                {
                    const char msg[] = "stderr-line\n";
                    DWORD written = 0;
                    ::WriteFile(err_pipe, msg, static_cast<DWORD>(sizeof(msg) - 1U), &written, nullptr);
                    ::CloseHandle(err_pipe);
                }
                return TRUE;
            });
    // [Pre-Assert確認_正常系] - ShellExecuteExW() が 1 回呼び出されること。
    // [Pre-Assert手順] - ShellExecuteExW() のモック内で実パイプへ書き込み、テスト自身のプロセス ハンドルを返す。
    EXPECT_CALL(mock_cplat, cplat_process_timed_wait(_, _))
        .WillOnce(Return(CPLAT_ERR_TIMEOUT))
        .WillOnce(Return(CPLAT_ERR_TIMEOUT))
        .WillOnce(Return(CPLAT_OK));
    // [Pre-Assert確認_正常系] - cplat_process_timed_wait() が 3 回呼び出されること。
    // [Pre-Assert手順] - cplat_process_timed_wait() から CPLAT_ERR_TIMEOUT を 2 回の後 CPLAT_OK を返す。
    EXPECT_CALL(mock_cplat, cplat_process_get_exit_code(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(0), Return(CPLAT_OK)));
    // [Pre-Assert確認_正常系] - cplat_process_get_exit_code() が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_process_get_exit_code() から終了コード 0 を設定して CPLAT_OK を返す。

    // Act
    int actual_ret = cplat_elevated_process_run_piped(
        "--install", collect_stream_output, &outputs, &exit_code,
        &handled); // [手順] - 昇格プロセスが名前付きパイプへ出力する状態で cplat_elevated_process_run_piped() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_elevated_process_run_piped() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1, handled);           // [確認_正常系] - handled が 1 であること。
    EXPECT_EQ(0, exit_code);         // [確認_正常系] - exit_code が 0 であること。
    EXPECT_STREQ("stdout-line\n", outputs.out.c_str()); // [確認_正常系] - stdout の出力内容が一致すること。
    EXPECT_STREQ("stderr-line\n", outputs.err.c_str()); // [確認_正常系] - stderr の出力内容が一致すること。
    EXPECT_EQ(L"runas", captured_verb);                 // [確認_正常系] - lpVerb が L"runas" であること。
    EXPECT_EQ(SW_HIDE, captured_show);                  // [確認_正常系] - nShow が SW_HIDE であること。
    EXPECT_NE(std::wstring::npos, captured_params.find(L"--cplat-output-pipes="));
    // [確認_正常系] - lpParameters が --cplat-output-pipes= フラグを含むこと。
}

// 同じ名前のパイプがすでに存在する場合に UAC を出さず失敗することの確認
TEST(elevatedProcessTest, run_piped_fails_when_pipe_name_already_exists)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    NiceMock<Mock_shellapi> mock_shellapi;
    // [サブ手順参照 名前=elevatedProcessTest.setup_run_piped_base_mocks]
    setup_run_piped_base_mocks(mock_cplat, mock_windows);
    const std::string expected_token = std::to_string(GetCurrentProcessId()) + "-11111111111111111111111111111111";
    const std::wstring wide_token(expected_token.begin(), expected_token.end());
    const std::wstring existing_name = L"\\\\.\\pipe\\cplat-elevated-" + wide_token + L"-out";
    HANDLE existing_pipe = ::CreateNamedPipeW(
        existing_name.c_str(), PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1U, 0U, 4096U, 0U, nullptr);
    ASSERT_NE(INVALID_HANDLE_VALUE, existing_pipe); // [状態確認] - 既存のパイプを先に作成できていること。
    int exit_code = 0;
    int handled = -1;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_random_bytes(_, 16U))
        .WillOnce(
            [](void *dest, size_t dest_size) -> int
            {
                std::memset(dest, 0x11, dest_size);
                return CPLAT_OK;
            });
    // [Pre-Assert確認_異常系] - cplat_random_bytes() が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_random_bytes() から全バイト 0x11 の固定値を返す。
    EXPECT_CALL(mock_shellapi, ShellExecuteExW(_, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - ShellExecuteExW() が呼び出されないこと。

    // Act
    int actual_ret = cplat_elevated_process_run_piped(
        nullptr, nullptr, nullptr, &exit_code,
        &handled); // [手順] - 同名のパイプが先取りされた状態で cplat_elevated_process_run_piped() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret); // [確認_異常系] - cplat_elevated_process_run_piped() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(0, handled);              // [確認_異常系] - handled が 0 であること。
    EXPECT_EQ(EXIT_FAILURE, exit_code); // [確認_異常系] - exit_code が EXIT_FAILURE であること。

    // Cleanup
    ::CloseHandle(existing_pipe);
}

// UAC が取り消された場合に待機せず即座に失敗することの確認
TEST(elevatedProcessTest, run_piped_fails_without_waiting_when_elevation_is_cancelled)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    NiceMock<Mock_shellapi> mock_shellapi;
    // [サブ手順参照 名前=elevatedProcessTest.setup_run_piped_base_mocks]
    setup_run_piped_base_mocks(mock_cplat, mock_windows);
    int exit_code = 0;
    int handled = 0;

    // Pre-Assert
    EXPECT_CALL(mock_shellapi, ShellExecuteExW(_, _, _, _))
        .WillOnce(Return(FALSE)); // [Pre-Assert確認_異常系] - ShellExecuteExW() が 1 回呼び出されること。
                                  // [Pre-Assert手順] - ShellExecuteExW() から FALSE を返す。
    EXPECT_CALL(mock_cplat, cplat_process_timed_wait(_, _))
        .Times(0); // [Pre-Assert確認_異常系] - cplat_process_timed_wait() が呼び出されないこと。

    // Act
    int actual_ret = cplat_elevated_process_run_piped(
        nullptr, nullptr, nullptr, &exit_code,
        &handled); // [手順] - ShellExecuteExW() が失敗する状態で cplat_elevated_process_run_piped() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret); // [確認_異常系] - cplat_elevated_process_run_piped() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(1, handled);              // [確認_異常系] - handled が 1 であること。
    EXPECT_EQ(EXIT_FAILURE, exit_code); // [確認_異常系] - exit_code が EXIT_FAILURE であること。
}

// 昇格プロセスがパイプへ接続せず終了した場合に失敗を返すことの確認
TEST(elevatedProcessTest, run_piped_reports_child_that_exits_without_connecting)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    NiceMock<Mock_shellapi> mock_shellapi;
    // [サブ手順参照 名前=elevatedProcessTest.setup_run_piped_base_mocks]
    setup_run_piped_base_mocks(mock_cplat, mock_windows);
    pipe_test_outputs outputs = {};
    int exit_code = 0;
    int handled = 0;

    // Pre-Assert
    EXPECT_CALL(mock_shellapi, ShellExecuteExW(_, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, SHELLEXECUTEINFOW *exec_info) -> BOOL
            {
                exec_info->hProcess = ::GetCurrentProcess();
                return TRUE;
            });
    // [Pre-Assert確認_異常系] - ShellExecuteExW() が 1 回呼び出されること。
    // [Pre-Assert手順] - パイプを開かずにテスト自身のプロセス ハンドルを返す。
    EXPECT_CALL(mock_cplat, cplat_process_timed_wait(_, _))
        .WillOnce(Return(CPLAT_OK)); // [Pre-Assert確認_異常系] - cplat_process_timed_wait() が 1 回呼び出されること。
                                     // [Pre-Assert手順] - cplat_process_timed_wait() から CPLAT_OK を返す。

    // Act
    int actual_ret = cplat_elevated_process_run_piped(
        nullptr, collect_stream_output, &outputs, &exit_code,
        &handled); // [手順] - 子プロセスがパイプに接続せずに終了する状態で cplat_elevated_process_run_piped() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret); // [確認_異常系] - cplat_elevated_process_run_piped() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(EXIT_FAILURE, exit_code); // [確認_異常系] - exit_code が EXIT_FAILURE であること。
    EXPECT_EQ(0, outputs.call_count);   // [確認_異常系] - コールバックが 1 回も呼ばれないこと。
}

// 終了直前に大量出力を書き込んだ場合も残りを読み切って受信できることの確認
TEST(elevatedProcessTest, run_piped_receives_output_written_just_before_exit)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    NiceMock<Mock_shellapi> mock_shellapi;
    // [サブ手順参照 名前=elevatedProcessTest.setup_run_piped_base_mocks]
    setup_run_piped_base_mocks(mock_cplat, mock_windows);
    const size_t payload_size = 200U * 1024U;
    std::string expected_payload(payload_size, '\0');
    for (size_t i = 0; i < payload_size; i++)
    {
        expected_payload[i] = static_cast<char>('A' + static_cast<int>(i % 26U));
    }
    pipe_test_outputs outputs = {};
    std::atomic<bool> writer_done(false);
    std::thread writer_thread;
    const auto test_start = std::chrono::steady_clock::now();
    int exit_code = -1;
    int handled = 0;

    // Pre-Assert
    EXPECT_CALL(mock_shellapi, ShellExecuteExW(_, _, _, _))
        .WillOnce(
            [&expected_payload, &writer_done, &writer_thread](const char *, const int, const char *,
                                                              SHELLEXECUTEINFOW *exec_info) -> BOOL
            {
                exec_info->hProcess = ::GetCurrentProcess();
                const std::wstring token = extract_pipe_token_from_wide_params(exec_info->lpParameters);
                const std::wstring out_name = L"\\\\.\\pipe\\cplat-elevated-" + token + L"-out";
                const std::wstring err_name = L"\\\\.\\pipe\\cplat-elevated-" + token + L"-err";

                writer_thread = std::thread(
                    [out_name, err_name, &expected_payload, &writer_done]()
                    {
                        HANDLE out_pipe = ::CreateFileW(out_name.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                                        SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS, nullptr);
                        HANDLE err_pipe = ::CreateFileW(err_name.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                                        SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS, nullptr);
                        if (out_pipe != INVALID_HANDLE_VALUE)
                        {
                            size_t offset = 0;
                            while (offset < expected_payload.size())
                            {
                                DWORD chunk = static_cast<DWORD>(expected_payload.size() - offset);
                                DWORD written = 0;
                                if (!::WriteFile(out_pipe, expected_payload.data() + offset, chunk, &written, nullptr) ||
                                    written == 0)
                                {
                                    break;
                                }
                                offset += static_cast<size_t>(written);
                            }
                            ::CloseHandle(out_pipe);
                        }
                        if (err_pipe != INVALID_HANDLE_VALUE)
                        {
                            ::CloseHandle(err_pipe);
                        }
                        writer_done.store(true);
                    });
                return TRUE;
            });
    // [Pre-Assert確認_正常系] - ShellExecuteExW() が 1 回呼び出されること。
    // [Pre-Assert手順] - 別スレッドで 200 KiB を実パイプへ書き込んで閉じるように設定する。
    EXPECT_CALL(mock_cplat, cplat_process_timed_wait(_, _))
        .WillRepeatedly(
            [&writer_done, test_start](cplat_process *, int) -> int
            {
                if (writer_done.load())
                {
                    return CPLAT_OK;
                }
                const auto elapsed = std::chrono::steady_clock::now() - test_start;
                if (elapsed > std::chrono::seconds(5))
                {
                    ADD_FAILURE() << "timed out waiting for writer thread";
                    return CPLAT_OK;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                return CPLAT_ERR_TIMEOUT;
            });
    // [Pre-Assert確認_正常系] - cplat_process_timed_wait() が 1 回以上呼び出されること。
    // [Pre-Assert手順] - 別スレッドの書き込み完了まで CPLAT_ERR_TIMEOUT、完了後は CPLAT_OK を返す。
    EXPECT_CALL(mock_cplat, cplat_process_get_exit_code(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(0), Return(CPLAT_OK)));
    // [Pre-Assert確認_正常系] - cplat_process_get_exit_code() が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_process_get_exit_code() から終了コード 0 を設定して CPLAT_OK を返す。

    // Act
    int actual_ret = cplat_elevated_process_run_piped(
        nullptr, collect_stream_output, &outputs, &exit_code,
        &handled); // [手順] - 別スレッドが 200 KiB を書いて閉じる状態で cplat_elevated_process_run_piped() を呼び出す。

    if (writer_thread.joinable())
    {
        writer_thread.join();
    }

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_elevated_process_run_piped() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, exit_code);         // [確認_正常系] - exit_code が 0 であること。
    EXPECT_EQ(payload_size, outputs.out.size()); // [確認_正常系] - 受信バイト数が 200 KiB と一致すること。
    EXPECT_EQ(expected_payload, outputs.out);    // [確認_正常系] - 受信内容が書き込んだデータと一致すること。
}

// 乱数生成に失敗した場合に UAC を出さず失敗することの確認
TEST(elevatedProcessTest, run_piped_fails_when_random_bytes_cannot_be_generated)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    NiceMock<Mock_shellapi> mock_shellapi;
    // [サブ手順参照 名前=elevatedProcessTest.setup_run_piped_base_mocks]
    setup_run_piped_base_mocks(mock_cplat, mock_windows);
    int exit_code = 0;
    int handled = -1;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_random_bytes(_, 16U))
        .WillOnce(Return(CPLAT_ERR_UNKNOWN));
    // [Pre-Assert確認_異常系] - cplat_random_bytes() が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_random_bytes() から CPLAT_ERR_UNKNOWN を返す。
    EXPECT_CALL(mock_shellapi, ShellExecuteExW(_, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - ShellExecuteExW() が呼び出されないこと。

    // Act
    int actual_ret = cplat_elevated_process_run_piped(
        nullptr, nullptr, nullptr, &exit_code,
        &handled); // [手順] - 乱数生成が失敗する状態で cplat_elevated_process_run_piped() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret); // [確認_異常系] - cplat_elevated_process_run_piped() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(0, handled);              // [確認_異常系] - handled が 0 であること。
    EXPECT_EQ(EXIT_FAILURE, exit_code); // [確認_異常系] - exit_code が EXIT_FAILURE であること。
}

// 名前付きパイプに BUILTIN\Administrators への書き込み許可 DACL が設定されることの確認
TEST(elevatedProcessTest, run_piped_grants_write_access_to_administrators)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    NiceMock<Mock_shellapi> mock_shellapi;
    // [サブ手順参照 名前=elevatedProcessTest.setup_run_piped_base_mocks]
    setup_run_piped_base_mocks(mock_cplat, mock_windows);
    bool has_admin_write_ace = false;
    int exit_code = -1;
    int handled = 0;

    // Pre-Assert
    EXPECT_CALL(mock_shellapi, ShellExecuteExW(_, _, _, _))
        .WillOnce(
            [&has_admin_write_ace](const char *, const int, const char *, SHELLEXECUTEINFOW *exec_info) -> BOOL
            {
                exec_info->hProcess = ::GetCurrentProcess();
                const std::wstring token = extract_pipe_token_from_wide_params(exec_info->lpParameters);
                const std::wstring out_name = L"\\\\.\\pipe\\cplat-elevated-" + token + L"-out";
                const std::wstring err_name = L"\\\\.\\pipe\\cplat-elevated-" + token + L"-err";

                HANDLE out_pipe = ::CreateFileW(out_name.c_str(), GENERIC_WRITE | READ_CONTROL, 0, nullptr,
                                                OPEN_EXISTING, SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS, nullptr);
                HANDLE err_pipe = ::CreateFileW(err_name.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                                SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS, nullptr);
                if (out_pipe != INVALID_HANDLE_VALUE)
                {
                    PACL dacl = nullptr;
                    PSECURITY_DESCRIPTOR sd = nullptr;
                    if (::GetSecurityInfo(out_pipe, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr,
                                          &dacl, nullptr, &sd) == ERROR_SUCCESS &&
                        dacl != nullptr)
                    {
                        BYTE admins_sid_buf[SECURITY_MAX_SID_SIZE];
                        DWORD admins_sid_size = static_cast<DWORD>(sizeof(admins_sid_buf));
                        if (::CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr,
                                                 static_cast<PSID>(admins_sid_buf), &admins_sid_size))
                        {
                            ACL_SIZE_INFORMATION acl_info = {};
                            if (::GetAclInformation(dacl, &acl_info, sizeof(acl_info), AclSizeInformation))
                            {
                                for (DWORD i = 0; i < acl_info.AceCount; i++)
                                {
                                    LPVOID ace_ptr = nullptr;
                                    if (::GetAce(dacl, i, &ace_ptr))
                                    {
                                        const auto *header = static_cast<const ACE_HEADER *>(ace_ptr);
                                        if (header->AceType == ACCESS_ALLOWED_ACE_TYPE)
                                        {
                                            const auto *allowed = static_cast<const ACCESS_ALLOWED_ACE *>(ace_ptr);
                                            const PSID ace_sid =
                                                reinterpret_cast<PSID>(const_cast<DWORD *>(&allowed->SidStart));
                                            if (::EqualSid(ace_sid, static_cast<PSID>(admins_sid_buf)) &&
                                                (allowed->Mask & FILE_GENERIC_WRITE) == FILE_GENERIC_WRITE)
                                            {
                                                has_admin_write_ace = true;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    if (sd != nullptr)
                    {
                        ::LocalFree(sd);
                    }
                    ::CloseHandle(out_pipe);
                }
                if (err_pipe != INVALID_HANDLE_VALUE)
                {
                    ::CloseHandle(err_pipe);
                }
                return TRUE;
            });
    // [Pre-Assert確認_正常系] - ShellExecuteExW() が 1 回呼び出されること。
    // [Pre-Assert手順] - パイプを開いて GetSecurityInfo() で DACL を検証する。
    EXPECT_CALL(mock_cplat, cplat_process_timed_wait(_, _))
        .WillOnce(Return(CPLAT_OK)); // [Pre-Assert確認_正常系] - cplat_process_timed_wait() が 1 回呼び出されること。
                                     // [Pre-Assert手順] - cplat_process_timed_wait() から CPLAT_OK を返す。
    EXPECT_CALL(mock_cplat, cplat_process_get_exit_code(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(0), Return(CPLAT_OK)));
    // [Pre-Assert確認_正常系] - cplat_process_get_exit_code() が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_process_get_exit_code() から終了コード 0 を設定して CPLAT_OK を返す。

    // Act
    int actual_ret = cplat_elevated_process_run_piped(
        nullptr, nullptr, nullptr, &exit_code,
        &handled); // [手順] - DACL を検証する状態で cplat_elevated_process_run_piped() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_elevated_process_run_piped() の戻り値が CPLAT_OK であること。
    EXPECT_TRUE(has_admin_write_ace); // [確認_正常系] - BUILTIN\Administrators へ書き込み許可の ACE があること。
}

// 起動した昇格プロセス以外の PID からのパイプ接続を拒否することの確認
TEST(elevatedProcessTest, run_piped_rejects_client_other_than_launched_process)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    NiceMock<Mock_shellapi> mock_shellapi;
    // [サブ手順参照 名前=elevatedProcessTest.setup_run_piped_base_mocks]
    setup_run_piped_base_mocks(mock_cplat, mock_windows);
    pipe_test_outputs outputs = {};
    int exit_code = 0;
    int handled = 0;
    const ULONG unexpected_pid = static_cast<ULONG>(GetCurrentProcessId()) + 1UL;

    // Pre-Assert
    EXPECT_CALL(mock_shellapi, ShellExecuteExW(_, _, _, _))
        .WillOnce(
            [](const char *, const int, const char *, SHELLEXECUTEINFOW *exec_info) -> BOOL
            {
                exec_info->hProcess = ::GetCurrentProcess();
                const std::wstring token = extract_pipe_token_from_wide_params(exec_info->lpParameters);
                const std::wstring out_name = L"\\\\.\\pipe\\cplat-elevated-" + token + L"-out";
                const std::wstring err_name = L"\\\\.\\pipe\\cplat-elevated-" + token + L"-err";

                HANDLE out_pipe = ::CreateFileW(out_name.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                                SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS, nullptr);
                HANDLE err_pipe = ::CreateFileW(err_name.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                                SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS, nullptr);
                if (out_pipe != INVALID_HANDLE_VALUE)
                {
                    const char msg[] = "should-not-be-read";
                    DWORD written = 0;
                    ::WriteFile(out_pipe, msg, static_cast<DWORD>(sizeof(msg) - 1U), &written, nullptr);
                    ::CloseHandle(out_pipe);
                }
                if (err_pipe != INVALID_HANDLE_VALUE)
                {
                    ::CloseHandle(err_pipe);
                }
                return TRUE;
            });
    // [Pre-Assert確認_異常系] - ShellExecuteExW() が 1 回呼び出されること。
    // [Pre-Assert手順] - パイプへ接続して文字列を書き込むが、クライアント PID は別プロセスとして返す。
    EXPECT_CALL(mock_windows, GetNamedPipeClientProcessId(_, _, _, _, _))
        .WillOnce(DoAll(SetArgPointee<4>(unexpected_pid), Return(TRUE)));
    // [Pre-Assert確認_異常系] - GetNamedPipeClientProcessId() が 1 回呼び出されること。
    // [Pre-Assert手順] - 起動したプロセスと異なる PID を設定して TRUE を返す。
    EXPECT_CALL(mock_cplat, cplat_process_timed_wait(_, _))
        .WillOnce(Return(CPLAT_OK)); // [Pre-Assert確認_異常系] - cplat_process_timed_wait() が 1 回呼び出されること。
                                     // [Pre-Assert手順] - cplat_process_timed_wait() から CPLAT_OK を返す。

    // Act
    int actual_ret = cplat_elevated_process_run_piped(
        nullptr, collect_stream_output, &outputs, &exit_code,
        &handled); // [手順] - 別 PID のクライアントが接続した状態で cplat_elevated_process_run_piped() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        actual_ret); // [確認_異常系] - cplat_elevated_process_run_piped() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(EXIT_FAILURE, exit_code); // [確認_異常系] - exit_code が EXIT_FAILURE であること。
    EXPECT_EQ(0, outputs.call_count);   // [確認_異常系] - コールバックが 1 回も呼ばれないこと。
}
#endif
