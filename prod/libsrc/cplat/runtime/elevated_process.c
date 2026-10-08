/**
 *******************************************************************************
 *  @file           elevated_process.c
 *  @brief          管理者権限の確認と昇格プロセスの起動を実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/06/20
 *  @version        1.0.0
 *
 *  管理者/root 権限の確認と、必要に応じた昇格プロセスの起動を提供します。
 *  プロセスの待機・終了コード取得・破棄はプロセス起動ユーティリティ (process.c) を
 *  再利用します。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/base/result.h>
#include <cplat/crt/stdlib.h>
#include <cplat/crt/stdio.h>
#include <cplat/crt/string.h>
#include <cplat/runtime/elevated_process.h>
#include <cplat/runtime/process.h>
#include <cplat/runtime/process_internal.h>
#include <cplat/console/console_internal.h>
#include <cplat/crt/path.h>
#include <cplat/crt/wchar_conv.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(PLATFORM_LINUX)
    #include <unistd.h>
#elif defined(PLATFORM_WINDOWS)
    #include <cplat/base/windows_sdk.h>
    #include <cplat/win32/win32.h>
    #include <shellapi.h>
    #include <wchar.h>
    #include <fcntl.h>
    #include <io.h>
    #pragma comment(lib, "Shell32.lib")

/**
 *  @brief          昇格プロセスへ結果報告先の一時ファイル パスを引き継ぐ内部フラグです。
 *
 *  cplat_elevated_process_run_with_result() が昇格プロセスのコマンド ラインへ
 *  `{FLAG}={一時ファイルの UTF-8 パス}` の形式で付与し、
 *  cplat_elevated_process_extract_result_target() がこれを検出して報告先を保持します。
 */
    #define CPLAT_PROCESS_RESULT_TARGET_FLAG      "--cplat-result-file"

static char s_result_target_path[PLATFORM_PATH_MAX] = {0};

/**
 *  @brief          昇格プロセスへ出力パイプの複製元を引き継ぐ内部フラグです。
 *
 *  cplat_elevated_process_run_piped() が昇格プロセスのコマンド ラインへ
 *  `{FLAG}={親 PID}:{stdout パイプの書き込みハンドル値}:{stderr パイプの書き込みハンドル値}`
 *  の形式 (いずれも 10 進数) で付与し、cplat_elevated_process_attach_output_pipes() が
 *  これを検出してパイプを複製します。
 */
    #define CPLAT_PROCESS_OUTPUT_PIPES_FLAG       "--cplat-output-pipes"

/**
 *  @brief          昇格プロセスの終了を待つ間に出力パイプを確認する間隔 [ms] です。
 */
    #define CPLAT_PROCESS_OUTPUT_POLL_INTERVAL_MS 10

/**
 *  @brief          出力パイプから 1 回に読み取る最大バイト数です。
 */
    #define CPLAT_PROCESS_OUTPUT_CHUNK_SIZE       4096

/**
 *  @brief          現在のプロセスが対話セッションで動作しているかを確認します。
 *
 *  Windows のサービス プロセスはセッション 0 に隔離され、UAC (User Account Control)
 *  ダイアログを表示できる対話デスクトップを持ちません。セッション 0 で未昇格のまま
 *  `ShellExecuteExW` の `runas` verb を呼び出すと、対話デスクトップが無いために
 *  昇格が失敗または応答不能になるおそれがあるため、事前にセッション ID で判定します。
 *  see: https://learn.microsoft.com/windows/win32/services/interactive-services
 *
 *  @return         対話セッションの場合は非 0、非対話セッション (セッション 0) の場合は 0、
 *                  判定に失敗した場合は -1 を返します。
 */
static int is_current_session_interactive(void)
{
    DWORD session_id = 0;

    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session_id))
    {
        return -1;
    }
    return (session_id != 0);
}

/**
 *  @brief          UAC を要求して、現在の実行ファイルを非表示で起動します。
 *  @param[in]      arguments  実行ファイルへ渡す引数文字列 (UTF-8)。NULL を渡してはなりません。
 *  @param[out]     child_out  起動したプロセスの格納先。失敗時は NULL を格納します。
 *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_OUT_OF_MEMORY 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
 */
static int start_elevated_self(const char *arguments, cplat_process **child_out)
{
    SHELLEXECUTEINFOW exec_info;
    char exe_path[PLATFORM_PATH_MAX];
    wchar_t *wide_exe_path;
    wchar_t *wide_arguments;
    BOOL executed;

    *child_out = NULL;

    if (cplat_process_get_executable_path(exe_path, sizeof(exe_path)) != CPLAT_OK)
    {
        return CPLAT_ERR_UNKNOWN;
    }
    wide_exe_path = cplat_utf8_to_wstr_alloc(exe_path);
    if (wide_exe_path == NULL)
    {
        return CPLAT_ERR_OUT_OF_MEMORY;
    }
    wide_arguments = cplat_utf8_to_wstr_alloc(arguments);
    if (wide_arguments == NULL)
    {
        cplat_free(wide_exe_path);
        return CPLAT_ERR_OUT_OF_MEMORY;
    }

    ZeroMemory(&exec_info, sizeof(exec_info));
    exec_info.cbSize = sizeof(exec_info);
    exec_info.fMask = SEE_MASK_NOCLOSEPROCESS;
    exec_info.hwnd = NULL;
    exec_info.lpVerb = L"runas";
    exec_info.lpFile = wide_exe_path;
    exec_info.lpParameters = wide_arguments;
    exec_info.nShow = SW_HIDE;

    executed = ShellExecuteExW(&exec_info);
    cplat_free(wide_arguments);
    cplat_free(wide_exe_path);
    if (!executed)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    *child_out = cplat_internal_process_adopt_native((intptr_t)exec_info.hProcess);
    if (*child_out == NULL)
    {
        CloseHandle(exec_info.hProcess);
        return CPLAT_ERR_UNKNOWN;
    }
    return CPLAT_OK;
}

/**
 *  @brief          昇格プロセスの出力を、呼び出し元の標準出力または標準エラー出力へ書き込みます。
 *
 *  cplat_elevated_process_run_piped() に @p output_fn として NULL を渡した場合の既定の処理です。
 *  昇格プロセス側はバイナリ モードで書き込むため、改行コードの変換は呼び出し元の CRT に任せます。
 */
static void write_to_own_std_stream(cplat_elevated_process_stream stream, const char *data, size_t size, void *context)
{
    FILE *fp;

    (void)context;

    fp = stdout;
    if (stream == CPLAT_ELEVATED_PROCESS_STREAM_STDERR)
    {
        fp = stderr;
    }
    (void)fwrite(data, 1, size, fp);
    (void)fflush(fp);
}

/**
 *  @brief          出力パイプから、現時点で読み取れる内容をすべて読み取ってコールバックへ渡します。
 *
 *  読み取れる量を PeekNamedPipe で確認してから ReadFile を呼ぶため、パイプが空でも待機しません。
 *  PeekNamedPipe は無名パイプの読み取りハンドルにも使えます。
 *  see: https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-peeknamedpipe
 */
static void pump_output_pipe(HANDLE read_handle, cplat_elevated_process_stream stream,
                             cplat_elevated_process_output_fn output_fn, void *context)
{
    char buffer[CPLAT_PROCESS_OUTPUT_CHUNK_SIZE];

    for (;;)
    {
        DWORD available;
        DWORD to_read;
        DWORD bytes_read;

        available = 0;
        if (!PeekNamedPipe(read_handle, NULL, 0, NULL, &available, NULL) || available == 0)
        {
            return;
        }
        to_read = (DWORD)sizeof(buffer);
        if (available < to_read)
        {
            to_read = available;
        }
        bytes_read = 0;
        if (!ReadFile(read_handle, buffer, to_read, &bytes_read, NULL) || bytes_read == 0)
        {
            return;
        }
        output_fn(stream, buffer, (size_t)bytes_read, context);
    }
}

/**
 *  @brief          CRT の標準ストリームと Win32 の標準ハンドルを、指定したハンドルへ付け替えます。
 *  @param[in]      stream         対象ストリーム (stdin / stdout / stderr)。
 *  @param[in]      handle         付け替え先のハンドル。成否にかかわらず本関数が所有権を持ちます。
 *  @param[in]      open_flags     `_open_osfhandle` へ渡すフラグ。
 *  @param[in]      std_handle_id  `SetStdHandle` へ渡す標準ハンドルの種別。
 *  @return         成功時は 0、失敗時は -1 を返します。
 *
 *  付け替え後の Win32 標準ハンドルには、fd が所有するハンドルをそのまま設定します。
 *  fd を閉じるとハンドルも閉じられるため、別に複製したハンドルを設定しません。
 */
static int reopen_std_stream(FILE *stream, HANDLE handle, int open_flags, DWORD std_handle_id)
{
    int expected_fd;
    int new_fd;

    expected_fd = _fileno(stream);
    if (expected_fd < 0)
    {
        CloseHandle(handle);
        return -1;
    }

    (void)fflush(stream);
    (void)_close(expected_fd);

    new_fd = _open_osfhandle((intptr_t)handle, open_flags);
    if (new_fd < 0)
    {
        CloseHandle(handle);
        return -1;
    }
    if (new_fd != expected_fd)
    {
        if (_dup2(new_fd, expected_fd) != 0)
        {
            (void)_close(new_fd);
            return -1;
        }
        (void)_close(new_fd);
    }

    clearerr(stream);
    (void)SetStdHandle(std_handle_id, (HANDLE)_get_osfhandle(expected_fd));
    return 0;
}

/**
 *  @brief          argv から出力パイプ フラグを取り除き、その値を解析します。
 *  @param[in,out]  argc           引数の数へのポインター。
 *  @param[in,out]  argv           引数配列。
 *  @param[out]     parent_pid     親プロセス ID の格納先。
 *  @param[out]     stdout_handle  親プロセス内の stdout パイプ書き込みハンドル値の格納先。
 *  @param[out]     stderr_handle  親プロセス内の stderr パイプ書き込みハンドル値の格納先。
 *  @return         フラグがない場合は 0、値が正しい場合は 1、値が不正な場合は -1 を返します。
 *
 *  値が不正な場合も、フラグは argv から取り除きます。
 */
static int extract_output_pipes_arg(int *argc, char **argv, DWORD *parent_pid, unsigned long long *stdout_handle,
                                    unsigned long long *stderr_handle)
{
    const char *prefix = CPLAT_PROCESS_OUTPUT_PIPES_FLAG "=";
    const char *value;
    char *endp;
    size_t prefix_len;
    unsigned long pid;
    int n;
    int i;
    int j;

    if (argc == NULL || argv == NULL)
    {
        return 0;
    }

    prefix_len = strlen(prefix);
    value = NULL;
    n = *argc;
    for (i = 1; i < n; i++)
    {
        if (argv[i] == NULL || strncmp(argv[i], prefix, prefix_len) != 0)
        {
            continue;
        }

        value = argv[i] + prefix_len;
        for (j = i; j < n - 1; j++)
        {
            argv[j] = argv[j + 1];
        }
        argv[n - 1] = NULL;
        *argc = n - 1;
        break;
    }
    if (value == NULL)
    {
        return 0;
    }

    endp = NULL;
    pid = strtoul(value, &endp, 10);
    if (endp == value || *endp != ':' || pid == 0)
    {
        return -1;
    }
    value = endp + 1;
    *stdout_handle = strtoull(value, &endp, 10);
    if (endp == value || *endp != ':' || *stdout_handle == 0)
    {
        return -1;
    }
    value = endp + 1;
    *stderr_handle = strtoull(value, &endp, 10);
    if (endp == value || *endp != '\0' || *stderr_handle == 0)
    {
        return -1;
    }
    *parent_pid = (DWORD)pid;
    return 1;
}
#endif /* PLATFORM_ */

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_elevated_process_is_elevated(int *elevated)
{
    if (elevated == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    *elevated = 0;

#if defined(PLATFORM_LINUX)
    if (geteuid() == 0)
    {
        *elevated = 1;
    }
    return CPLAT_OK;
#elif defined(PLATFORM_WINDOWS)
    {
        HANDLE token = NULL;
        TOKEN_ELEVATION elevation;
        DWORD returned_length;
        int rc;

        rc = CPLAT_ERR_UNKNOWN;
        returned_length = 0;

        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        {
            if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &returned_length))
            {
                if (elevation.TokenIsElevated != 0)
                {
                    *elevated = 1;
                }
                rc = CPLAT_OK;
            }
            CloseHandle(token);
        }

        return rc;
    }
#else
    return CPLAT_ERR_UNSUPPORTED;
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_elevated_process_run_if_needed(const char *arguments, int *exit_code, int *handled)
{
    if (exit_code == NULL || handled == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    *exit_code = 0;
    *handled = 0;

#if defined(PLATFORM_WINDOWS)
    {
        SHELLEXECUTEINFOW exec_info;
        char exe_path[PLATFORM_PATH_MAX];
        char *combined_arguments = NULL;
        const char *effective_arguments;
        wchar_t *wide_arguments = NULL;
        wchar_t *wide_exe_path;
        cplat_process *child_process;
        int child_exit_code;
        int elevated;
        int inherit_console;
        int diag_enabled;

        if (cplat_elevated_process_is_elevated(&elevated) != CPLAT_OK)
        {
            *exit_code = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }
        if (elevated != 0)
        {
            return CPLAT_OK;
        }

        /* セッション 0 (非対話セッション) では UAC ダイアログを表示できないため、
           昇格を試みず即座に失敗させる。 */
        if (is_current_session_interactive() == 0)
        {
            *exit_code = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }

        if (cplat_process_get_executable_path(exe_path, sizeof(exe_path)) != CPLAT_OK)
        {
            *exit_code = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }

        /* 親にコンソールがある場合のみ、昇格プロセスへ親コンソールを引き継ぐ。
           昇格プロセス側は cplat_console_attach_parent() で再接続する。 */
        inherit_console = (GetConsoleWindow() != NULL);
        diag_enabled = 0;
        if (GetEnvironmentVariableA(CPLAT_CONSOLE_ATTACH_DIAG_ENV, NULL, 0) > 0)
        {
            diag_enabled = 1;
        }

        effective_arguments = arguments;
        if (inherit_console != 0 || diag_enabled != 0)
        {
            DWORD parent_pid;
            unsigned long long parent_console_window;
            size_t arg_len;
            size_t buf_sz;
            char extra[80];
            int ret;

            parent_pid = 0;
            /* 親コンソールの window ハンドルを引き継ぎ、子側が一時コンソールではなく
               親コンソールへ確実に再接続できたかを確認できるようにする。 */
            parent_console_window = 0;
            arg_len = 0;
            if (arguments != NULL)
            {
                arg_len = strlen(arguments);
            }
            /* 区切り空白 + フラグ + '=' + PID (最大 10 桁) + ':' + HWND (最大 20 桁) + 終端の余裕を確保する */
            buf_sz = arg_len + 1;
            if (diag_enabled != 0)
            {
                buf_sz += strlen(CPLAT_CONSOLE_ATTACH_DIAG_FLAG) + 1;
            }
            if (inherit_console != 0)
            {
                parent_pid = GetCurrentProcessId();
                parent_console_window = (unsigned long long)(uintptr_t)GetConsoleWindow();
                buf_sz += strlen(CPLAT_CONSOLE_HANDOVER_FLAG) + 49;
            }
            combined_arguments = (char *)cplat_malloc(buf_sz);
            if (combined_arguments == NULL)
            {
                *exit_code = EXIT_FAILURE;
                return CPLAT_ERR_OUT_OF_MEMORY;
            }
            if (arg_len > 0)
            {
                ret = cplat_snprintf(combined_arguments, buf_sz, "%s", arguments);
                if (ret != CPLAT_OK)
                {
                    cplat_free(combined_arguments);
                    *exit_code = EXIT_FAILURE;
                    return CPLAT_ERR_UNKNOWN;
                }
            }
            else
            {
                combined_arguments[0] = '\0';
            }
            if (diag_enabled != 0)
            {
                const char *separator;

                separator = "";
                if (combined_arguments[0] != '\0')
                {
                    separator = " ";
                }
                ret = cplat_snprintf(extra, sizeof(extra), "%s%s", separator, CPLAT_CONSOLE_ATTACH_DIAG_FLAG);
                if (ret != CPLAT_OK)
                {
                    cplat_free(combined_arguments);
                    *exit_code = EXIT_FAILURE;
                    return CPLAT_ERR_UNKNOWN;
                }
                ret = cplat_strcat(combined_arguments, buf_sz, extra);
                if (ret != CPLAT_OK)
                {
                    cplat_free(combined_arguments);
                    *exit_code = EXIT_FAILURE;
                    return CPLAT_ERR_UNKNOWN;
                }
            }
            if (inherit_console != 0)
            {
                const char *separator;

                separator = "";
                if (combined_arguments[0] != '\0')
                {
                    separator = " ";
                }
                ret = cplat_snprintf(extra, sizeof(extra), "%s%s=%lu:%llu", separator, CPLAT_CONSOLE_HANDOVER_FLAG,
                                     (unsigned long)parent_pid, parent_console_window);
                if (ret != CPLAT_OK)
                {
                    cplat_free(combined_arguments);
                    *exit_code = EXIT_FAILURE;
                    return CPLAT_ERR_UNKNOWN;
                }
                ret = cplat_strcat(combined_arguments, buf_sz, extra);
                if (ret != CPLAT_OK)
                {
                    cplat_free(combined_arguments);
                    *exit_code = EXIT_FAILURE;
                    return CPLAT_ERR_UNKNOWN;
                }
            }
            effective_arguments = combined_arguments;
        }

        wide_exe_path = cplat_utf8_to_wstr_alloc(exe_path);
        if (wide_exe_path == NULL)
        {
            cplat_free(combined_arguments);
            *exit_code = EXIT_FAILURE;
            return CPLAT_ERR_OUT_OF_MEMORY;
        }
        if (effective_arguments != NULL)
        {
            wide_arguments = cplat_utf8_to_wstr_alloc(effective_arguments);
            if (wide_arguments == NULL)
            {
                cplat_free(combined_arguments);
                cplat_free(wide_exe_path);
                *exit_code = EXIT_FAILURE;
                return CPLAT_ERR_OUT_OF_MEMORY;
            }
        }
        cplat_free(combined_arguments);
        combined_arguments = NULL;

        ZeroMemory(&exec_info, sizeof(exec_info));
        exec_info.cbSize = sizeof(exec_info);
        exec_info.fMask = SEE_MASK_NOCLOSEPROCESS;
        exec_info.hwnd = NULL;
        exec_info.lpVerb = L"runas";
        exec_info.lpFile = wide_exe_path;
        exec_info.lpParameters = wide_arguments;
        /* コンソール引き継ぎ時は昇格プロセスの一時コンソールを隠す。
           引き継がない場合は従来どおり通常表示とする。 */
        if (inherit_console != 0)
        {
            exec_info.nShow = SW_HIDE;
        }
        else
        {
            exec_info.nShow = SW_SHOWNORMAL;
        }

        *handled = 1;

        if (!ShellExecuteExW(&exec_info))
        {
            cplat_free(wide_arguments);
            cplat_free(wide_exe_path);
            *exit_code = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }
        cplat_free(wide_arguments);
        cplat_free(wide_exe_path);

        child_process = cplat_internal_process_adopt_native((intptr_t)exec_info.hProcess);
        if (child_process == NULL)
        {
            CloseHandle(exec_info.hProcess);
            *exit_code = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }
        if (cplat_process_wait(child_process, CPLAT_PROCESS_WAIT_FOREVER) != CPLAT_OK ||
            cplat_process_get_exit_code(child_process, &child_exit_code) != CPLAT_OK)
        {
            cplat_process_dispose(child_process);
            *exit_code = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }
        cplat_process_dispose(child_process);

        *exit_code = child_exit_code;
        return CPLAT_OK;
    }
#elif defined(PLATFORM_LINUX)
    {
        int elevated;

        (void)arguments;
        (void)cplat_elevated_process_is_elevated(&elevated);
        if (elevated == 0)
        {
            *exit_code = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }
        return CPLAT_OK;
    }
#else
    (void)arguments;
    *exit_code = EXIT_FAILURE;
    return CPLAT_ERR_UNSUPPORTED;
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_elevated_process_run_with_result(const char *arguments, int *exit_code, int *handled, char *result_message,
                                           size_t result_message_size)
{
    if (exit_code == NULL || handled == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    *exit_code = 0;
    *handled = 0;
    if (result_message != NULL && result_message_size > 0)
    {
        result_message[0] = '\0';
    }

#if defined(PLATFORM_WINDOWS)
    {
        SHELLEXECUTEINFOW exec_info;
        char exe_path[PLATFORM_PATH_MAX];
        char result_path[PLATFORM_PATH_MAX];
        wchar_t wide_temp_dir[PLATFORM_PATH_MAX];
        wchar_t wide_result_path[PLATFORM_PATH_MAX];
        char *combined_arguments;
        wchar_t *wide_arguments;
        wchar_t *wide_exe_path;
        cplat_process *child_process;
        int child_exit_code;
        int elevated;
        size_t arg_len;
        size_t buf_sz;
        DWORD temp_len;

        if (cplat_elevated_process_is_elevated(&elevated) != CPLAT_OK)
        {
            *exit_code = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }
        if (elevated != 0)
        {
            return CPLAT_OK;
        }

        /* セッション 0 (非対話セッション) では UAC ダイアログを表示できないため、
           昇格を試みず即座に失敗させる。 */
        if (is_current_session_interactive() == 0)
        {
            *exit_code = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }

        if (cplat_process_get_executable_path(exe_path, sizeof(exe_path)) != CPLAT_OK)
        {
            *exit_code = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }

        /* 昇格プロセスが結果メッセージを書き込む一時ファイルを確保する。GetTempFileNameW は
           一意な空ファイルを作成するため、並行する別プロセスとの衝突を避けられる。 */
        /* 本関数内の一時ファイル操作はワイドのまま完結するため、*U ラッパーを経由しない */
        temp_len = GetTempPathW((DWORD)(sizeof(wide_temp_dir) / sizeof(wide_temp_dir[0])), wide_temp_dir);
        if (temp_len == 0 || temp_len >= sizeof(wide_temp_dir) / sizeof(wide_temp_dir[0]))
        {
            *exit_code = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }
        if (GetTempFileNameW(wide_temp_dir, L"cur", 0, wide_result_path) == 0)
        {
            *exit_code = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }
        if (cplat_wpath_to_utf8(result_path, sizeof(result_path), wide_result_path) < 0)
        {
            DeleteFileW(wide_result_path);
            *exit_code = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }

        arg_len = 0;
        if (arguments != NULL)
        {
            arg_len = strlen(arguments);
        }
        /* 区切り空白 + フラグ + '=' + '"' + パス + '"' + 終端の余裕を確保する */
        buf_sz = arg_len + 1 + strlen(CPLAT_PROCESS_RESULT_TARGET_FLAG) + 1 + 1 + strlen(result_path) + 1 + 1;
        combined_arguments = (char *)cplat_malloc(buf_sz);
        if (combined_arguments == NULL)
        {
            *exit_code = EXIT_FAILURE;
            return CPLAT_ERR_OUT_OF_MEMORY;
        }
        /* result_path はユーザー プロファイル配下 (ユーザー名に空白を含みうる) から生成されるため、
           クォートしないと CRT のコマンド ライン分割で複数トークンとなり、
           cplat_elevated_process_extract_result_target() でのフラグ抽出に失敗する。
           クォート区間内の空白はトークン区切りにならず、クォート文字自体は結果の argv から除去される。
           see: https://learn.microsoft.com/en-us/cpp/c-language/parsing-c-command-line-arguments */
        if (arg_len > 0)
        {
            (void)cplat_snprintf(combined_arguments, buf_sz, "%s %s=\"%s\"", arguments,
                                 CPLAT_PROCESS_RESULT_TARGET_FLAG, result_path);
        }
        else
        {
            (void)cplat_snprintf(combined_arguments, buf_sz, "%s=\"%s\"", CPLAT_PROCESS_RESULT_TARGET_FLAG,
                                 result_path);
        }

        wide_exe_path = cplat_utf8_to_wstr_alloc(exe_path);
        if (wide_exe_path == NULL)
        {
            cplat_free(combined_arguments);
            DeleteFileW(wide_result_path);
            *exit_code = EXIT_FAILURE;
            return CPLAT_ERR_OUT_OF_MEMORY;
        }
        wide_arguments = cplat_utf8_to_wstr_alloc(combined_arguments);
        cplat_free(combined_arguments);
        if (wide_arguments == NULL)
        {
            cplat_free(wide_exe_path);
            DeleteFileW(wide_result_path);
            *exit_code = EXIT_FAILURE;
            return CPLAT_ERR_OUT_OF_MEMORY;
        }

        ZeroMemory(&exec_info, sizeof(exec_info));
        exec_info.cbSize = sizeof(exec_info);
        exec_info.fMask = SEE_MASK_NOCLOSEPROCESS;
        exec_info.hwnd = NULL;
        exec_info.lpVerb = L"runas";
        exec_info.lpFile = wide_exe_path;
        exec_info.lpParameters = wide_arguments;
        /* コンソールを一切引き継がないため、昇格プロセスは常に非表示で起動する。 */
        exec_info.nShow = SW_HIDE;

        *handled = 1;

        if (!ShellExecuteExW(&exec_info))
        {
            cplat_free(wide_arguments);
            cplat_free(wide_exe_path);
            DeleteFileW(wide_result_path);
            *exit_code = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }
        cplat_free(wide_arguments);
        cplat_free(wide_exe_path);

        child_process = cplat_internal_process_adopt_native((intptr_t)exec_info.hProcess);
        if (child_process == NULL)
        {
            CloseHandle(exec_info.hProcess);
            DeleteFileW(wide_result_path);
            *exit_code = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }
        if (cplat_process_wait(child_process, CPLAT_PROCESS_WAIT_FOREVER) != CPLAT_OK ||
            cplat_process_get_exit_code(child_process, &child_exit_code) != CPLAT_OK)
        {
            cplat_process_dispose(child_process);
            DeleteFileW(wide_result_path);
            *exit_code = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }
        cplat_process_dispose(child_process);

        if (result_message != NULL && result_message_size > 0)
        {
            HANDLE h;

            h = CreateFileW(wide_result_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                            NULL);
            if (h != INVALID_HANDLE_VALUE)
            {
                DWORD bytes_read = 0;
                DWORD to_read = (DWORD)(result_message_size - 1);

                if (ReadFile(h, result_message, to_read, &bytes_read, NULL))
                {
                    result_message[bytes_read] = '\0';
                }
                CloseHandle(h);
            }
        }
        DeleteFileW(wide_result_path);

        *exit_code = child_exit_code;
        return CPLAT_OK;
    }
#elif defined(PLATFORM_LINUX)
    {
        int elevated;

        /* 別プロセスを起動しないため、報告の余地がない。 */
        (void)result_message;
        (void)result_message_size;
        (void)arguments;
        (void)cplat_elevated_process_is_elevated(&elevated);
        if (elevated == 0)
        {
            *exit_code = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }
        return CPLAT_OK;
    }
#else
    (void)arguments;
    (void)result_message;
    (void)result_message_size;
    *exit_code = EXIT_FAILURE;
    return CPLAT_ERR_UNSUPPORTED;
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_elevated_process_extract_result_target(int *argc, char **argv, int *detected_out)
{
#if defined(PLATFORM_WINDOWS)
    const char *prefix = CPLAT_PROCESS_RESULT_TARGET_FLAG "=";
    size_t prefix_len;
    int n;
    int i;

    if (detected_out != NULL)
    {
        *detected_out = 0;
    }

    if (argc == NULL || argv == NULL)
    {
        return CPLAT_OK;
    }

    prefix_len = strlen(prefix);
    n = *argc;
    for (i = 1; i < n; i++)
    {
        int j;
        size_t value_len;

        if (argv[i] == NULL || strncmp(argv[i], prefix, prefix_len) != 0)
        {
            continue;
        }

        value_len = strlen(argv[i] + prefix_len);
        if (value_len >= sizeof(s_result_target_path))
        {
            value_len = sizeof(s_result_target_path) - 1;
        }
        memcpy(s_result_target_path, argv[i] + prefix_len, value_len);
        s_result_target_path[value_len] = '\0';

        for (j = i; j < n - 1; j++)
        {
            argv[j] = argv[j + 1];
        }
        argv[n - 1] = NULL;
        *argc = n - 1;
        if (detected_out != NULL)
        {
            *detected_out = 1;
        }
        return CPLAT_OK;
    }
    return CPLAT_OK;
#else
    (void)argc;
    (void)argv;
    if (detected_out != NULL)
    {
        *detected_out = 0;
    }
    return CPLAT_OK;
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_elevated_process_report_result(const char *message)
{
    if (message == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

#if defined(PLATFORM_WINDOWS)
    {
        HANDLE h;
        DWORD written;
        size_t len;

        if (s_result_target_path[0] == '\0')
        {
            return CPLAT_ERR_UNKNOWN;
        }
        h = CreateFileU(s_result_target_path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE)
        {
            return CPLAT_ERR_UNKNOWN;
        }
        len = strlen(message);
        if (!WriteFile(h, message, (DWORD)len, &written, NULL))
        {
            CloseHandle(h);
            return CPLAT_ERR_UNKNOWN;
        }
        CloseHandle(h);
        return CPLAT_OK;
    }
#else
    (void)message;
    return CPLAT_ERR_UNKNOWN;
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_elevated_process_run_piped(const char *arguments, cplat_elevated_process_output_fn output_fn, void *context,
                                     int *exit_code, int *handled)
{
    if (exit_code == NULL || handled == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    *exit_code = 0;
    *handled = 0;

#if defined(PLATFORM_WINDOWS)
    {
        cplat_elevated_process_output_fn effective_output_fn;
        HANDLE stdout_read;
        HANDLE stdout_write;
        HANDLE stderr_read;
        HANDLE stderr_write;
        char *combined_arguments;
        const char *base_arguments;
        const char *separator;
        cplat_process *child_process;
        int child_exit_code;
        int elevated;
        int wait_result;
        int rc;
        size_t buf_sz;

        if (cplat_elevated_process_is_elevated(&elevated) != CPLAT_OK)
        {
            *exit_code = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }
        if (elevated != 0)
        {
            return CPLAT_OK;
        }

        /* セッション 0 (非対話セッション) では UAC ダイアログを表示できないため、
           昇格を試みず即座に失敗させる。 */
        if (is_current_session_interactive() == 0)
        {
            *exit_code = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }

        effective_output_fn = output_fn;
        if (effective_output_fn == NULL)
        {
            effective_output_fn = write_to_own_std_stream;
        }

        /* UAC 昇格 (ShellExecuteExW の runas) ではハンドルを継承できないため、継承不可の
           無名パイプを作成し、昇格プロセス側が OpenProcess(PROCESS_DUP_HANDLE) と
           DuplicateHandle で書き込み側を自分へ複製する。高い整合性レベルのプロセスは、
           同じユーザーの低い整合性レベルのプロセスをこの権限で開ける。
           see: https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-duplicatehandle */
        if (!CreatePipe(&stdout_read, &stdout_write, NULL, 0))
        {
            *exit_code = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }
        if (!CreatePipe(&stderr_read, &stderr_write, NULL, 0))
        {
            CloseHandle(stdout_read);
            CloseHandle(stdout_write);
            *exit_code = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }

        base_arguments = "";
        separator = "";
        if (arguments != NULL && arguments[0] != '\0')
        {
            base_arguments = arguments;
            separator = " ";
        }
        /* 区切り空白 + フラグ + '=' + PID (最大 10 桁) + ':' と ハンドル値 (最大 20 桁) の 2 組 + 終端 */
        buf_sz = strlen(base_arguments) + 1 + strlen(CPLAT_PROCESS_OUTPUT_PIPES_FLAG) + 1 + 10 + 2 * (1 + 20) + 1;
        combined_arguments = (char *)cplat_malloc(buf_sz);
        if (combined_arguments == NULL)
        {
            CloseHandle(stdout_read);
            CloseHandle(stdout_write);
            CloseHandle(stderr_read);
            CloseHandle(stderr_write);
            *exit_code = EXIT_FAILURE;
            return CPLAT_ERR_OUT_OF_MEMORY;
        }
        rc = cplat_snprintf(combined_arguments, buf_sz, "%s%s%s=%lu:%llu:%llu", base_arguments, separator,
                            CPLAT_PROCESS_OUTPUT_PIPES_FLAG, (unsigned long)GetCurrentProcessId(),
                            (unsigned long long)(uintptr_t)stdout_write, (unsigned long long)(uintptr_t)stderr_write);
        child_process = NULL;
        if (rc == CPLAT_OK)
        {
            *handled = 1;
            rc = start_elevated_self(combined_arguments, &child_process);
        }
        cplat_free(combined_arguments);
        if (rc != CPLAT_OK)
        {
            CloseHandle(stdout_read);
            CloseHandle(stdout_write);
            CloseHandle(stderr_read);
            CloseHandle(stderr_write);
            *exit_code = EXIT_FAILURE;
            if (rc == CPLAT_ERR_OUT_OF_MEMORY)
            {
                return CPLAT_ERR_OUT_OF_MEMORY;
            }
            return CPLAT_ERR_UNKNOWN;
        }

        /* パイプの容量を超える出力で昇格プロセスが書き込み待ちにならないよう、
           終了を待つ間も出力を読み取り続ける。 */
        do
        {
            wait_result = cplat_process_wait(child_process, CPLAT_PROCESS_OUTPUT_POLL_INTERVAL_MS);
            pump_output_pipe(stdout_read, CPLAT_ELEVATED_PROCESS_STREAM_STDOUT, effective_output_fn, context);
            pump_output_pipe(stderr_read, CPLAT_ELEVATED_PROCESS_STREAM_STDERR, effective_output_fn, context);
        } while (wait_result == CPLAT_ERR_TIMEOUT);

        /* 昇格プロセスの書き込みは終了時点でパイプへ格納済みのため、残りを読み切る。
           昇格プロセスが起動した孫プロセスが書き込み側を保持していても待機しない。 */
        CloseHandle(stdout_write);
        CloseHandle(stderr_write);
        pump_output_pipe(stdout_read, CPLAT_ELEVATED_PROCESS_STREAM_STDOUT, effective_output_fn, context);
        pump_output_pipe(stderr_read, CPLAT_ELEVATED_PROCESS_STREAM_STDERR, effective_output_fn, context);
        CloseHandle(stdout_read);
        CloseHandle(stderr_read);

        if (wait_result != CPLAT_OK || cplat_process_get_exit_code(child_process, &child_exit_code) != CPLAT_OK)
        {
            cplat_process_dispose(child_process);
            *exit_code = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }
        cplat_process_dispose(child_process);

        *exit_code = child_exit_code;
        return CPLAT_OK;
    }
#elif defined(PLATFORM_LINUX)
    {
        int elevated;

        (void)arguments;
        (void)output_fn;
        (void)context;
        (void)cplat_elevated_process_is_elevated(&elevated);
        if (elevated == 0)
        {
            *exit_code = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }
        return CPLAT_OK;
    }
#else
    (void)arguments;
    (void)output_fn;
    (void)context;
    *exit_code = EXIT_FAILURE;
    return CPLAT_ERR_UNSUPPORTED;
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_elevated_process_attach_output_pipes(int *argc, char **argv, int *attached_out)
{
    if (attached_out != NULL)
    {
        *attached_out = 0;
    }

#if defined(PLATFORM_WINDOWS)
    {
        HANDLE parent_process;
        HANDLE stdout_handle;
        HANDLE stderr_handle;
        HANDLE stdin_handle;
        DWORD parent_pid;
        unsigned long long stdout_value;
        unsigned long long stderr_value;
        BOOL stdout_duplicated;
        BOOL stderr_duplicated;
        int found;

        parent_pid = 0;
        stdout_value = 0;
        stderr_value = 0;
        found = extract_output_pipes_arg(argc, argv, &parent_pid, &stdout_value, &stderr_value);
        if (found == 0)
        {
            return CPLAT_OK;
        }
        if (found < 0)
        {
            return CPLAT_ERR_UNKNOWN;
        }

        parent_process = OpenProcess(PROCESS_DUP_HANDLE, FALSE, parent_pid);
        if (parent_process == NULL)
        {
            return CPLAT_ERR_UNKNOWN;
        }
        stdout_handle = NULL;
        stderr_handle = NULL;
        stdout_duplicated = DuplicateHandle(parent_process, (HANDLE)(uintptr_t)stdout_value, GetCurrentProcess(),
                                            &stdout_handle, 0, FALSE, DUPLICATE_SAME_ACCESS);
        stderr_duplicated = DuplicateHandle(parent_process, (HANDLE)(uintptr_t)stderr_value, GetCurrentProcess(),
                                            &stderr_handle, 0, FALSE, DUPLICATE_SAME_ACCESS);
        CloseHandle(parent_process);

        /* 引数で受け取ったハンドル値がパイプ以外を指す場合は付け替えない。 */
        if (!stdout_duplicated || !stderr_duplicated || GetFileType(stdout_handle) != FILE_TYPE_PIPE ||
            GetFileType(stderr_handle) != FILE_TYPE_PIPE)
        {
            if (stdout_duplicated)
            {
                CloseHandle(stdout_handle);
            }
            if (stderr_duplicated)
            {
                CloseHandle(stderr_handle);
            }
            return CPLAT_ERR_UNKNOWN;
        }

        /* 昇格プロセスのコンソールは非表示のため、標準入力の読み取りで応答不能にならないよう
           NUL デバイスへ付け替える。ワイド文字列リテラルを渡すため、CreateFileU を使わない。 */
        stdin_handle =
            CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (stdin_handle == INVALID_HANDLE_VALUE)
        {
            CloseHandle(stdout_handle);
            CloseHandle(stderr_handle);
            return CPLAT_ERR_UNKNOWN;
        }

        /* 改行コードは呼び出し元で変換するため、バイナリ モードで開く。 */
        if (reopen_std_stream(stdout, stdout_handle, _O_BINARY | _O_WRONLY, STD_OUTPUT_HANDLE) != 0)
        {
            CloseHandle(stderr_handle);
            CloseHandle(stdin_handle);
            return CPLAT_ERR_UNKNOWN;
        }
        if (reopen_std_stream(stderr, stderr_handle, _O_BINARY | _O_WRONLY, STD_ERROR_HANDLE) != 0)
        {
            CloseHandle(stdin_handle);
            return CPLAT_ERR_UNKNOWN;
        }
        if (reopen_std_stream(stdin, stdin_handle, _O_BINARY | _O_RDONLY, STD_INPUT_HANDLE) != 0)
        {
            return CPLAT_ERR_UNKNOWN;
        }

        /* 呼び出し元での表示が昇格プロセスの終了まで遅れないよう、stdout をバッファーなしにする。 */
        (void)setvbuf(stdout, NULL, _IONBF, 0);

        if (attached_out != NULL)
        {
            *attached_out = 1;
        }
        return CPLAT_OK;
    }
#else
    (void)argc;
    (void)argv;
    return CPLAT_OK;
#endif /* PLATFORM_ */
}
