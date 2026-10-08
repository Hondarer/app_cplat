/**
 *  @file           console.c
 *  @brief          Windows コンソールを設定するヘルパー機能を実装します。
 *
 *  Windows 環境: 接続先コンソールの入出力コード ページを UTF-8 に設定し、
 *  stdout / stderr の Virtual Terminal Processing を有効化します。\n
 *  Linux 環境: cplat_console_init / cplat_console_dispose は no-op です。
 */

#include <cplat/base/result.h>
#include <cplat/console/console.h>
#include <cplat/sync/atomic.h>
#include <cplat/console/console_internal.h>
#include <cplat/crt/path.h>
#include <cplat/crt/stdio.h>
#include <cplat/crt/unistd.h>
#include <cplat/runtime/shutdown.h>

/* ===== Windows 実装 ===== */

#if defined(PLATFORM_WINDOWS)

    #include <cplat/base/windows_sdk.h>
    #include <cplat/sync/sync.h>
    #include <cplat/win32/win32.h>
    #include <errno.h>
    #include <fcntl.h>
    #include <io.h>
    #include <stdint.h> /* uintptr_t */
    #include <stdarg.h> /* va_list */
    #include <stdio.h>  /* stdout, stderr */
    #include <stdlib.h> /* strtoul, strtoull */
    #include <string.h> /* strncmp, strlen */

/* 初期化前のコンソール状態を保存 */
static UINT s_orig_output_cp = 0;
static UINT s_orig_input_cp = 0;
static DWORD s_orig_stdout_mode = 0;
static DWORD s_orig_stderr_mode = 0;
/* 初期化状態と親コンソールへの再接続の有無。ロックを取らずに複数のスレッドから参照するため、アトミック型で保持する */
static cplat_atomic_i32 s_initialized = CPLAT_ATOMIC_INIT(0);
static cplat_atomic_i32 s_attached_parent = CPLAT_ATOMIC_INIT(0);
static cplat_once_flag s_console_shutdown_once = {0};

static void console_diag_logf(const char *fmt, ...);

static void register_console_shutdown_callback(void)
{
    (void)cplat_shutdown_register(cplat_internal_console_dispose_on_shutdown, NULL);
}

/**
 *  @brief          ストリームの fd を、いったん閉じてから同じ番号へ直接再オープンします。
 *  @param[in]      handle      再接続先の Win32 ハンドル (CONOUT$ / CONIN$)。
 *  @param[in]      stream      対象ストリーム (stdout / stderr / stdin)。
 *  @param[in]      name        診断ログに記録するストリーム名。
 *  @param[in]      open_flags  `_open_osfhandle` へ渡すフラグ (`_O_TEXT` に加え `_O_RDWR` 等)。
 *  @return         成功時は 0、失敗時は -1。
 *
 *  `freopen` (内部で fd の閉じ直しを行う) や `_dup2` (別の一時 fd を経由する) は、
 *  いずれも実機調査で原因不明の失敗 (`EACCES` / 2 本目以降の `EBADF`) が再現した。
 *  本関数は `_close()` で対象 fd を解放した直後に、その場で `_open_osfhandle()` を
 *  呼ぶだけで `_dup2` を経由しない。fd 解放直後に呼ぶため、CRT の fd 割り当てが
 *  最小番号から再利用する前提で、解放した番号がそのまま戻ることを期待し、
 *  異なる番号が返った場合は診断ログに記録する。\n
 *  @p handle は `DuplicateHandle()` で複製せず、そのまま `_open_osfhandle()` に渡す
 *  (複製したハンドルでは書き込みが `EACCES` で拒否される事象が実機調査で再現したため)。
 *  そのため呼び出し後は fd がこのハンドルの所有権を持つことになり、@p handle を
 *  呼び出し元で別用途に使い続けてはならない。\n
 *  なお、対象 fd の解放を呼び出し元側でこの関数より早いタイミングに前倒しする変更を
 *  実機検証したところ、`CreateFileW(CONOUT$)` 自体が全試行で `ERROR_INVALID_HANDLE` に
 *  なる、より重い回帰が再現したため、解放はこの関数内でこのタイミングのまま行います。
 */
static int reopen_crt_std_fd(HANDLE handle, FILE *stream, const char *name, int open_flags)
{
    int expected_fd;
    int new_fd;

    if (handle == NULL || handle == INVALID_HANDLE_VALUE || stream == NULL || name == NULL)
    {
        return -1;
    }

    expected_fd = _fileno(stream);
    if (expected_fd < 0)
    {
        console_diag_logf("reopen %s _fileno failed", name);
        return -1;
    }

    _close(expected_fd);

    new_fd = _open_osfhandle((intptr_t)handle, open_flags);
    if (new_fd < 0)
    {
        console_diag_logf("reopen %s _open_osfhandle failed errno=%d expected_fd=%d", name, errno, expected_fd);
        return -1;
    }
    if (new_fd != expected_fd)
    {
        console_diag_logf("reopen %s unexpected fd new_fd=%d expected_fd=%d", name, new_fd, expected_fd);
    }

    clearerr(stream);
    console_diag_logf("reopen %s success fd=%d", name, new_fd);
    return 0;
}

static int console_diag_enabled(void)
{
    char value[8];
    DWORD len;

    len = GetEnvironmentVariableA(CPLAT_CONSOLE_ATTACH_DIAG_ENV, value, (DWORD)sizeof(value));
    if (len == 0 || len >= sizeof(value))
    {
        return 0;
    }
    if (len == 1 && value[0] == '0')
    {
        return 0;
    }
    return 1;
}

static int build_console_diag_log_path(wchar_t *path_out, size_t path_len)
{
    static const wchar_t file_name[] = L"cplat_console_attach.log";
    wchar_t temp_path[PLATFORM_PATH_MAX];
    DWORD temp_len;
    errno_t err;

    if (path_out == NULL || path_len == 0)
    {
        return -1;
    }

    /* 取得したパスは以降 CreateFileW へワイドのまま渡すため、*U ラッパーを経由しない */
    temp_len = GetTempPathW((DWORD)(sizeof(temp_path) / sizeof(temp_path[0])), temp_path);
    if (temp_len == 0 || temp_len >= sizeof(temp_path) / sizeof(temp_path[0]))
    {
        return -1;
    }

    err = _snwprintf_s(path_out, path_len, _TRUNCATE, L"%ls%ls", temp_path, file_name);
    if (err < 0)
    {
        return -1;
    }
    return 0;
}

/**
 *  @brief          `%TEMP%` 配下の診断ログへ 1 行追記します。
 *  @param[in]      fmt `printf` 互換の書式文字列です。
 *
 *  `CPLAT_CONSOLE_ATTACH_DIAG` が未設定、空文字、または `"0"` の場合は何もしません。
 */
static void console_diag_logf(const char *fmt, ...)
{
    wchar_t path[PLATFORM_PATH_MAX];
    char line[1024];
    char message[896];
    SYSTEMTIME now;
    HANDLE handle;
    DWORD written;
    int prefix_len;
    int message_len;
    va_list args;

    if (!console_diag_enabled() || fmt == NULL)
    {
        return;
    }
    if (build_console_diag_log_path(path, sizeof(path) / sizeof(path[0])) != 0)
    {
        return;
    }

    GetLocalTime(&now);
    if (cplat_snprintf(line, sizeof(line), "%04u-%02u-%02u %02u:%02u:%02u.%03u pid=%lu ", (unsigned int)now.wYear,
                       (unsigned int)now.wMonth, (unsigned int)now.wDay, (unsigned int)now.wHour,
                       (unsigned int)now.wMinute, (unsigned int)now.wSecond, (unsigned int)now.wMilliseconds,
                       (unsigned long)GetCurrentProcessId()) != CPLAT_OK)
    {
        return;
    }
    prefix_len = (int)strlen(line);

    va_start(args, fmt);
    message_len = vsnprintf(message, sizeof(message), fmt, args); /* 置換対象外: 意図的な切り詰め */
    va_end(args);
    if (message_len < 0)
    {
        return;
    }

    if (prefix_len + message_len + 2 >= (int)sizeof(line))
    {
        message_len = (int)sizeof(line) - prefix_len - 2;
        if (message_len < 0)
        {
            return;
        }
    }
    memcpy(line + prefix_len, message, (size_t)message_len);
    line[prefix_len + message_len] = '\n';
    line[prefix_len + message_len + 1] = '\0';

    /* path は GetTempPathW 由来のワイド文字列であり UTF-8 が関与しないため CreateFileU を使わない */
    handle = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == INVALID_HANDLE_VALUE)
    {
        return;
    }
    (void)WriteFile(handle, line, (DWORD)(prefix_len + message_len + 1), &written, NULL);
    CloseHandle(handle);
}

/* ===== 公開 API ===== */

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_console_init(void)
{
    HANDLE h;
    DWORD mode = 0;
    UINT current_cp;

    cplat_call_once(&s_console_shutdown_once, register_console_shutdown_callback);

    /* 二重初期化を防ぐ */
    {
        int32_t expected = 0;

        if (!cplat_atomic_compare_exchange_i32(&s_initialized, &expected, 1, CPLAT_MEMORY_ORDER_SEQ_CST))
            return;
    }

    /* stdout / stderr のいずれもコンソール (TTY) でなければ何もしない。
       stdout だけをリダイレクトした場合も stderr のコンソール出力に設定が要るため、
       コンソール全体で共有するコード ページを UTF-8 へ変更する (利用側の制約とする) */
    if (!cplat_isatty(CPLAT_STREAM_STDOUT) && !cplat_isatty(CPLAT_STREAM_STDERR))
    {
        console_diag_logf("console_init isatty=0 skip");
        cplat_atomic_store_i32(&s_initialized, 0, CPLAT_MEMORY_ORDER_SEQ_CST);
        return;
    }

    /* コンソールの入出力コード ページを UTF-8 に設定 (すでに UTF-8 なら変更しない) */
    current_cp = GetConsoleCP();
    if (current_cp != CP_UTF8)
    {
        s_orig_input_cp = current_cp;
        SetConsoleCP(CP_UTF8);
    }

    current_cp = GetConsoleOutputCP();
    if (current_cp != CP_UTF8)
    {
        s_orig_output_cp = current_cp;
        SetConsoleOutputCP(CP_UTF8);
    }

    /* Virtual Terminal Processing を有効化 (ANSI エスケープ シーケンス対応) */
    h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleMode(h, &mode))
    {
        if (!(mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING))
        {
            s_orig_stdout_mode = mode;
            if (!SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
            {
                console_diag_logf("console_init SetConsoleMode stdout failed handle=0x%p error=%lu", (void *)h,
                                  (unsigned long)GetLastError());
            }
        }
    }
    else
    {
        console_diag_logf("console_init GetConsoleMode stdout failed handle=0x%p error=%lu", (void *)h,
                          (unsigned long)GetLastError());
    }

    h = GetStdHandle(STD_ERROR_HANDLE);
    if (GetConsoleMode(h, &mode))
    {
        if (!(mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING))
        {
            s_orig_stderr_mode = mode;
            if (!SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
            {
                console_diag_logf("console_init SetConsoleMode stderr failed handle=0x%p error=%lu", (void *)h,
                                  (unsigned long)GetLastError());
            }
        }
    }
    else
    {
        console_diag_logf("console_init GetConsoleMode stderr failed handle=0x%p error=%lu", (void *)h,
                          (unsigned long)GetLastError());
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_console_dispose(void)
{
    HANDLE h;

    /* initialized を 1 → 0 に変更。変更できなければ元々未初期化なので何もしない。 */
    {
        int32_t expected = 1;

        if (!cplat_atomic_compare_exchange_i32(&s_initialized, &expected, 0, CPLAT_MEMORY_ORDER_SEQ_CST))
            return;
    }

    /* コンソール モードを元に戻す */
    if (s_orig_stdout_mode != 0)
    {
        h = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleMode(h, s_orig_stdout_mode);
        s_orig_stdout_mode = 0;
    }

    if (s_orig_stderr_mode != 0)
    {
        h = GetStdHandle(STD_ERROR_HANDLE);
        SetConsoleMode(h, s_orig_stderr_mode);
        s_orig_stderr_mode = 0;
    }

    /* コード ページを元に戻す */
    if (s_orig_input_cp != 0)
    {
        SetConsoleCP(s_orig_input_cp);
        s_orig_input_cp = 0;
    }

    if (s_orig_output_cp != 0)
    {
        SetConsoleOutputCP(s_orig_output_cp);
        s_orig_output_cp = 0;
    }
}

/**
 *  @brief          argv から親コンソール引き継ぎフラグを取り出して除去します。
 *  @param[in,out]  argc        引数の数へのポインター。
 *  @param[in,out]  argv        引数配列。
 *  @param[out]     pid_out     取り出した親プロセス ID の格納先。
 *  @param[out]     window_out  取り出した親コンソール window ハンドルの格納先 (省略時は NULL)。
 *  @return         有効なフラグを検出した場合は 1、そうでない場合は 0 を返します。
 *
 *  フラグの値は `{PID}` または `{PID}:{HWND}` 形式です。@p window_out には HWND を
 *  復元して格納し、HWND が無い・不正な場合は NULL を格納します。\n
 *  フラグを検出した場合は、値の解析可否にかかわらず @p argv から取り除き、
 *  @p argc を 1 減らします。PID が不正な場合は 0 を返します。
 */
static int extract_handover_args(int *argc, char **argv, DWORD *pid_out, HWND *window_out)
{
    const char *prefix = CPLAT_CONSOLE_HANDOVER_FLAG "=";
    size_t prefix_len;
    int n;
    int i;
    int found;
    DWORD pid;
    HWND window;

    if (argc == NULL || argv == NULL || pid_out == NULL || window_out == NULL)
    {
        return 0;
    }

    prefix_len = strlen(prefix);
    n = *argc;
    found = 0;
    pid = 0;
    window = NULL;

    for (i = 1; i < n; i++)
    {
        char *endp;
        unsigned long value;
        int j;

        if (argv[i] == NULL || strncmp(argv[i], prefix, prefix_len) != 0)
        {
            continue;
        }

        endp = NULL;
        value = strtoul(argv[i] + prefix_len, &endp, 10);
        if (endp != argv[i] + prefix_len && (*endp == '\0' || *endp == ':') && value != 0)
        {
            pid = (DWORD)value;
            found = 1;

            /* 区切り ':' に続く HWND を任意で解析する (旧形式の PID のみでも受理する) */
            if (*endp == ':')
            {
                const char *window_text = endp + 1;
                char *window_endp = NULL;
                unsigned long long window_value;

                window_value = strtoull(window_text, &window_endp, 10);
                if (window_endp != window_text && *window_endp == '\0')
                {
                    window = (HWND)(uintptr_t)window_value;
                }
            }
        }

        /* フラグを取り除いて後続を前へ詰める (不正値でも除去する) */
        for (j = i; j < n - 1; j++)
        {
            argv[j] = argv[j + 1];
        }
        argv[n - 1] = NULL;
        *argc = n - 1;
        break;
    }

    if (found)
    {
        *pid_out = pid;
        *window_out = window;
    }
    return found;
}

static int extract_diag_arg(int *argc, char **argv)
{
    int n;
    int i;

    if (argc == NULL || argv == NULL)
    {
        return 0;
    }

    n = *argc;
    for (i = 1; i < n; i++)
    {
        int j;

        if (argv[i] == NULL || strcmp(argv[i], CPLAT_CONSOLE_ATTACH_DIAG_FLAG) != 0)
        {
            continue;
        }

        for (j = i; j < n - 1; j++)
        {
            argv[j] = argv[j + 1];
        }
        argv[n - 1] = NULL;
        *argc = n - 1;
        return 1;
    }
    return 0;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_console_attach_parent(int *argc, char **argv, int *attached_out)
{
    DWORD parent_pid;
    HWND parent_window;
    HANDLE h_out;
    HANDLE h_err;
    HANDLE h_in;
    int attached;
    int attached_once;
    int attempt;
    int argc_value;

    if (attached_out != NULL)
    {
        *attached_out = 0;
    }

    parent_pid = 0;
    parent_window = NULL;
    if (extract_diag_arg(argc, argv) != 0)
    {
        (void)SetEnvironmentVariableA(CPLAT_CONSOLE_ATTACH_DIAG_ENV, "1");
    }
    if (!extract_handover_args(argc, argv, &parent_pid, &parent_window))
    {
        return CPLAT_OK;
    }
    argc_value = -1;
    if (argc != NULL)
    {
        argc_value = *argc;
    }

    /* 昇格時に割り当てられた一時コンソールを切り離し、親コンソールへ接続する。
       昇格直後は子の一時コンソール (conhost) 割り当てが非同期に進むため、
       自前コンソールへ繋がったままだと AttachConsole が ERROR_ACCESS_DENIED で
       失敗することがある。割り当てが落ち着くまで有界リトライする。\n
       さらに、AttachConsole が成功しても親コンソールの window ハンドルが取得できる
       状態になるまでには間がある。親 HWND が判っている場合は GetConsoleWindow() が
       親 HWND に一致するまで待ち、一時コンソールへ出力が吸われるのを防ぐ。
       全試行で一致しない場合は、素の AttachConsole 成功を受理してフォールバックする。 */
    attached = 0;
    attached_once = 0;
    console_diag_logf("attach_parent begin parent_pid=%lu parent_hwnd=0x%p argc=%d", (unsigned long)parent_pid,
                      (void *)parent_window, argc_value);
    for (attempt = 0; attempt < CPLAT_CONSOLE_ATTACH_MAX_ATTEMPTS; attempt++)
    {
        if (attached_once == 0)
        {
            DWORD free_error;

            SetLastError(ERROR_SUCCESS);
            (void)FreeConsole();
            free_error = GetLastError();
            console_diag_logf("attempt=%d FreeConsole last_error=%lu", attempt, (unsigned long)free_error);

            if (AttachConsole(parent_pid))
            {
                attached_once = 1;
                console_diag_logf("attempt=%d AttachConsole success hwnd=0x%p", attempt, (void *)GetConsoleWindow());
                if (parent_window == NULL)
                {
                    attached = 1;
                    break;
                }
            }
            else
            {
                DWORD attach_error = GetLastError();

                console_diag_logf("attempt=%d AttachConsole failed error=%lu", attempt, (unsigned long)attach_error);
            }
        }
        else
        {
            HWND current_window = GetConsoleWindow();

            console_diag_logf("attempt=%d attached_once current_hwnd=0x%p parent_hwnd=0x%p", attempt,
                              (void *)current_window, (void *)parent_window);
            if (current_window == parent_window)
            {
                attached = 1;
                break;
            }
        }
        Sleep(CPLAT_CONSOLE_ATTACH_RETRY_INTERVAL_MS);
    }
    if (attached == 0 && attached_once == 0)
    {
        /* AttachConsole 自体が一度も成功しなかった場合のみ失敗とする。 */
        console_diag_logf("attach_parent failed attached=%d attached_once=%d final_hwnd=0x%p", attached, attached_once,
                          (void *)GetConsoleWindow());
        return CPLAT_ERR_UNKNOWN;
    }
    console_diag_logf("attach_parent proceed attached=%d attached_once=%d final_hwnd=0x%p", attached, attached_once,
                      (void *)GetConsoleWindow());

    /* Win32 レベルの標準ハンドルを親コンソールへ付け替える
       (GetStdHandle / WriteConsole 系や tracer の stderr sink が参照する) */
    /* 次の 3 件はワイド文字列リテラルを渡すため、CreateFileU を使わない */
    h_out = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        OPEN_EXISTING, 0, NULL);
    if (h_out != INVALID_HANDLE_VALUE)
    {
        SetStdHandle(STD_OUTPUT_HANDLE, h_out);
        console_diag_logf("SetStdHandle stdout success handle=0x%p", (void *)h_out);
    }
    else
    {
        DWORD open_error = GetLastError();

        console_diag_logf("SetStdHandle stdout skipped CreateFileW failed error=%lu", (unsigned long)open_error);
    }
    h_err = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        OPEN_EXISTING, 0, NULL);
    if (h_err != INVALID_HANDLE_VALUE)
    {
        SetStdHandle(STD_ERROR_HANDLE, h_err);
        console_diag_logf("SetStdHandle stderr success handle=0x%p", (void *)h_err);
    }
    else
    {
        DWORD open_error = GetLastError();

        console_diag_logf("SetStdHandle stderr skipped CreateFileW failed error=%lu", (unsigned long)open_error);
    }
    h_in = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                       0, NULL);
    if (h_in != INVALID_HANDLE_VALUE)
    {
        SetStdHandle(STD_INPUT_HANDLE, h_in);
        console_diag_logf("SetStdHandle stdin success handle=0x%p", (void *)h_in);
    }
    else
    {
        DWORD open_error = GetLastError();

        console_diag_logf("SetStdHandle stdin skipped CreateFileW failed error=%lu", (unsigned long)open_error);
    }

    /* CRT レベルの標準ストリームを親コンソールへ再接続する (fgets 等の入力で参照する)。
       printf/fprintf (FILE* 経由) は再接続後に書き込みを拒否することがあるため
       (README.md 参照)、再接続後の出力には本ファイル末尾の cplat_console_write()
       (Win32 の WriteConsoleU を呼ぶ) を使うこと。本関数による fd の再接続は、
       fgets 等の入力経路向けに維持している。
       CONOUT$ / CONIN$ は GENERIC_READ | GENERIC_WRITE で開いているため `_O_RDWR` を渡す。 */
    if (reopen_crt_std_fd(h_out, stdout, "stdout", _O_TEXT | _O_RDWR) == 0)
    {
        /* 失敗しても Win32 ハンドルは付け替え済みのため処理を継続する */
        (void)setvbuf(stdout, NULL, _IONBF, 0);
    }
    if (reopen_crt_std_fd(h_err, stderr, "stderr", _O_TEXT | _O_RDWR) == 0)
    {
        /* 失敗しても Win32 ハンドルは付け替え済みのため処理を継続する */
        (void)setvbuf(stderr, NULL, _IONBF, 0);
    }
    (void)reopen_crt_std_fd(h_in, stdin, "stdin", _O_TEXT | _O_RDWR);

    /* 親コンソールへ再接続したことを記録する。終了時のフラッシュ後に conhost が
       書き込みを処理し終えるのを待ち合わせるために参照する。 */
    cplat_atomic_store_i32(&s_attached_parent, 1, CPLAT_MEMORY_ORDER_SEQ_CST);
    console_diag_logf("attach_parent end attached=%d attached_once=%d", attached, attached_once);

    if (attached_out != NULL)
    {
        *attached_out = 1;
    }
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_console_dispose_on_shutdown(const cplat_shutdown_event *event, void *context)
{
    (void)context;
    if (event == NULL || event->reason != CPLAT_SHUTDOWN_REASON_NORMAL_EXIT)
        return;

    /* stdout/stderr をフラッシュしてからコンソール状態を戻す */
    fflush(stdout);
    fflush(stderr);
    console_diag_logf("dispose_on_shutdown after fflush attached_parent=%ld",
                      (long)cplat_atomic_load_i32(&s_attached_parent, CPLAT_MEMORY_ORDER_SEQ_CST));

    /* 昇格時に親コンソールへ再接続していた場合、終了時フラッシュで書き込んだ内容を
       conhost が処理し終える前にプロセスが終了すると、内容が画面に出ないことがある。
       コンソールへの同期 API を 1 度呼び出して直前の書き込みが処理されたことを保証する。 */
    if (cplat_atomic_load_i32(&s_attached_parent, CPLAT_MEMORY_ORDER_SEQ_CST) != 0)
    {
        HANDLE h_out;
        CONSOLE_SCREEN_BUFFER_INFO info;

        h_out = GetStdHandle(STD_OUTPUT_HANDLE);
        if (h_out != NULL && h_out != INVALID_HANDLE_VALUE)
        {
            (void)GetConsoleScreenBufferInfo(h_out, &info);
            console_diag_logf("dispose_on_shutdown drain GetConsoleScreenBufferInfo handle=0x%p", (void *)h_out);
        }
    }

    cplat_console_dispose();
    console_diag_logf("dispose_on_shutdown end");
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_console_write(cplat_stream stream, const char *text)
{
    DWORD std_handle;
    HANDLE h;
    DWORD len;
    DWORD written;

    if (text == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (stream == CPLAT_STREAM_STDOUT)
    {
        std_handle = STD_OUTPUT_HANDLE;
    }
    else if (stream == CPLAT_STREAM_STDERR)
    {
        std_handle = STD_ERROR_HANDLE;
    }
    else
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    h = GetStdHandle(std_handle);
    if (h == NULL || h == INVALID_HANDLE_VALUE)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    len = (DWORD)strlen(text);
    if (WriteConsoleU(h, text, len, &written, NULL))
    {
        return CPLAT_OK;
    }
    /* リダイレクト先がコンソールでない場合 WriteConsoleU は失敗するため WriteFile にフォールバックする。
       リダイレクト先はコード ページ変換を行わないため、UTF-8 のまま書き込む */
    if (WriteFile(h, text, len, &written, NULL))
    {
        return CPLAT_OK;
    }

    console_diag_logf("console_write failed error=%lu", (unsigned long)GetLastError());
    return CPLAT_ERR_UNKNOWN;
}

#elif defined(PLATFORM_LINUX)

    #include <errno.h>  /* errno, EINTR */
    #include <string.h> /* strlen */
    #include <unistd.h> /* write, STDOUT_FILENO, STDERR_FILENO */

/* ===== Linux 実装 (no-op) ===== */

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_console_init(void) {}
/* Doxygen コメントは、ヘッダーに記載 */

void cplat_console_dispose(void) {}
/* Doxygen コメントは、ヘッダーに記載 */

int cplat_console_attach_parent(int *argc, char **argv, int *attached_out)
{
    (void)argc;
    (void)argv;
    if (attached_out != NULL)
    {
        *attached_out = 0;
    }
    return CPLAT_OK;
}
/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_console_dispose_on_shutdown(const cplat_shutdown_event *event, void *context)
{
    (void)event;
    (void)context;
}
/* Doxygen コメントは、ヘッダーに記載 */

int cplat_console_write(cplat_stream stream, const char *text)
{
    int fd;
    size_t len;
    const char *cursor;

    if (text == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (stream == CPLAT_STREAM_STDOUT)
    {
        fd = STDOUT_FILENO;
    }
    else if (stream == CPLAT_STREAM_STDERR)
    {
        fd = STDERR_FILENO;
    }
    else
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    cursor = text;
    len = strlen(text);
    while (len > 0)
    {
        ssize_t written;

        /* 端末は低速デバイスであり、シグナルで write が中断されうる。中断を吸収する。 */
        do
        {
            written = write(fd, cursor, len);
        } while ((written < 0) && (errno == EINTR));

        if (written < 0)
        {
            return CPLAT_ERR_UNKNOWN;
        }
        cursor += written;
        len -= (size_t)written;
    }
    return CPLAT_OK;
}

#endif /* PLATFORM_ */
