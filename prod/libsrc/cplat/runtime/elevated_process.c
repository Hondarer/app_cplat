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

#include <cplat/base/error_internal.h>
#include <cplat/base/result.h>
#include <cplat/crt/stdlib.h>
#include <cplat/crt/stdio.h>
#include <cplat/crt/string.h>
#include <cplat/runtime/elevated_process.h>
#include <cplat/runtime/process.h>
#include <cplat/runtime/process_internal.h>
#include <cplat/console/console_internal.h>
#include <cplat/crypto/random.h>
#include <cplat/crt/path.h>
#include <cplat/crt/temp_dir_internal.h>
#include <cplat/crt/wchar_conv.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** 追加引数の作業配列サイズ [byte] です。 */
#define CPLAT_PROCESS_EXTRA_ARGUMENT_SIZE 80U

/** PID の 10 進表記の最大桁数です。 */
#define CPLAT_PROCESS_PID_DIGITS_MAX 10U

/** ハンドルの 10 進表記の最大桁数です。 */
#define CPLAT_PROCESS_HANDLE_DIGITS_MAX 20U

/** 引数の区切り空白の長さ [byte] です。 */
#define CPLAT_PROCESS_ARGUMENT_SPACE_BYTES 1U

/** 引数内の '=' または ':' の長さ [byte] です。 */
#define CPLAT_PROCESS_ARGUMENT_SEPARATOR_BYTES 1U

/** 引数の終端 NUL の長さ [byte] です。 */
#define CPLAT_PROCESS_ARGUMENT_NUL_BYTES 1U

/** コンソール引き継ぎ引数の作業領域の余裕 [byte] です。 */
#define CPLAT_PROCESS_HANDOVER_MARGIN_BYTES 15U

/** コンソール引き継ぎ引数のフラグ以外の作業領域 [byte] です。 */
#define CPLAT_PROCESS_HANDOVER_EXTRA_BYTES \
    (CPLAT_PROCESS_ARGUMENT_SPACE_BYTES + CPLAT_PROCESS_ARGUMENT_SEPARATOR_BYTES + CPLAT_PROCESS_PID_DIGITS_MAX + \
     CPLAT_PROCESS_ARGUMENT_SEPARATOR_BYTES + CPLAT_PROCESS_HANDLE_DIGITS_MAX + CPLAT_PROCESS_ARGUMENT_NUL_BYTES + \
     CPLAT_PROCESS_HANDOVER_MARGIN_BYTES)

static_assert(CPLAT_PROCESS_HANDOVER_EXTRA_BYTES == 49U, "コンソール引き継ぎ引数の余裕を維持すること");

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
    #define CPLAT_PROCESS_RESULT_TARGET_FLAG       "--cplat-result-file"

static char s_result_target_path[PLATFORM_PATH_UTF8_MAX] = {0};

/**
 *  @brief          昇格プロセスへ出力パイプのトークンを引き継ぐ内部フラグです。
 *
 *  cplat_elevated_process_run_piped() が昇格プロセスのコマンド ラインへ
 *  `{FLAG}={トークン}` の形式で付与し、cplat_elevated_process_attach_output_pipes() が
 *  これを検出して名前付きパイプを開きます。
 */
    #define CPLAT_PROCESS_OUTPUT_PIPES_FLAG        "--cplat-output-pipes"

/**
 *  @brief          昇格プロセスの終了を待つ間に出力パイプを確認する間隔 [ms] です。
 */
    #define CPLAT_PROCESS_OUTPUT_POLL_INTERVAL_MS  10

/**
 *  @brief          出力パイプから 1 回に読み取る最大バイト数です。
 */
    #define CPLAT_PROCESS_OUTPUT_CHUNK_SIZE        4096

/* パイプのトークンに付与する暗号論的乱数のバイト数 [byte] (16 進数で 32 桁)。 */
    #define CPLAT_ELEVATED_PIPE_TOKEN_RANDOM_BYTES 16U

/* Windows の DWORD (32 bit 符号なし整数) の 10 進表記の最大桁数。 */
    #define CPLAT_ELEVATED_PIPE_PID_MAX_DIGITS     10U

/* トークン文字列 ("{PID}-{乱数 32 桁}") の最大文字数。 */
    #define CPLAT_ELEVATED_PIPE_TOKEN_MAX_LEN \
        (CPLAT_ELEVATED_PIPE_PID_MAX_DIGITS + 1U + (CPLAT_ELEVATED_PIPE_TOKEN_RANDOM_BYTES * 2U))

/* 名前付きパイプの作成時に指定する受信バッファーの助言サイズ [byte]。 */
    #define CPLAT_ELEVATED_PIPE_IN_BUFFER_BYTES    65536U

/* 名前付きパイプの作成時に指定する送信バッファーの助言サイズ [byte] (片方向受信のため 0)。 */
    #define CPLAT_ELEVATED_PIPE_OUT_BUFFER_BYTES   0U

/* 名前付きパイプの最大インスタンス数 (先取りの検出と後続の接続拒否のため 1 に固定)。 */
    #define CPLAT_ELEVATED_PIPE_MAX_INSTANCES      1U

/* 名前付きパイプの接頭辞と接尾辞 (ワイド文字列)。 */
    #define CPLAT_ELEVATED_PIPE_NAME_PREFIX        L"\\\\.\\pipe\\cplat-elevated-"
    #define CPLAT_ELEVATED_PIPE_STDOUT_SUFFIX      L"-out"
    #define CPLAT_ELEVATED_PIPE_STDERR_SUFFIX      L"-err"

/* CreateNamedPipeW が許すパイプ名全体の最大文字数 (終端 NUL を含む上限の目安)。 */
    #define CPLAT_ELEVATED_PIPE_NAME_API_MAX_CHARS 256U

/* パイプ名バッファーの要素数 [wchar_t] (接頭辞 + トークン + 接尾辞 + 終端 NUL)。 */
    #define CPLAT_ELEVATED_PIPE_NAME_CAP \
        ((sizeof(CPLAT_ELEVATED_PIPE_NAME_PREFIX) / sizeof(wchar_t) - 1U) + CPLAT_ELEVATED_PIPE_TOKEN_MAX_LEN + \
         (sizeof(CPLAT_ELEVATED_PIPE_STDERR_SUFFIX) / sizeof(wchar_t)))

static_assert(sizeof(CPLAT_ELEVATED_PIPE_STDOUT_SUFFIX) == sizeof(CPLAT_ELEVATED_PIPE_STDERR_SUFFIX),
              "pipe suffix sizes must match");
static_assert(CPLAT_ELEVATED_PIPE_NAME_CAP <= CPLAT_ELEVATED_PIPE_NAME_API_MAX_CHARS,
              "pipe name must fit within CreateNamedPipeW limit");

/**
 *  @brief          現在のプロセスが対話セッションで動作しているかを確認します。
 *
 *  Windows のサービス プロセスはセッション 0 に隔離され、UAC (User Account Control)
 *  ダイアログを表示できる対話デスクトップを持ちません。セッション 0 で未昇格のまま
 *  `ShellExecuteExW` の `runas` verb を呼び出すと、対話デスクトップが無いために
 *  昇格が失敗または応答不能になるおそれがあるため、事前にセッション ID で判定します。\n
 *  判定は、セッション ID が 0 かどうかだけで行います。
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
 *  @param[in]      arguments      実行ファイルへ渡す引数文字列 (UTF-8)。NULL を渡してはなりません。
 *  @param[out]     child_out      起動したプロセスの格納先。失敗時は NULL を格納します。
 *  @param[out]     child_pid_out  起動したプロセスの ID の格納先。失敗時は 0 を格納します。
 *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_OUT_OF_MEMORY 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
 */
static int start_elevated_self(const char *arguments, cplat_process **child_out, DWORD *child_pid_out)
{
    SHELLEXECUTEINFOW exec_info;
    char exe_path[PLATFORM_PATH_UTF8_MAX];
    wchar_t *wide_exe_path;
    wchar_t *wide_arguments;
    DWORD child_pid;
    BOOL executed;
    int rc;

    *child_out = NULL;
    *child_pid_out = 0;

    if (cplat_process_get_executable_path(exe_path, sizeof(exe_path)) != CPLAT_OK)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    rc = cplat_utf8_to_wstr_alloc(&wide_exe_path, exe_path);
    if (rc != CPLAT_OK)
    {
        return rc;
    }

    rc = cplat_utf8_to_wstr_alloc(&wide_arguments, arguments);
    if (rc != CPLAT_OK)
    {
        cplat_free(wide_exe_path);
        return rc;
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

    child_pid = GetProcessId(exec_info.hProcess);
    if (child_pid == 0)
    {
        (void)CloseHandle(exec_info.hProcess); /* PID 取得失敗の後始末のため、戻り値は明示的に破棄する */
        return CPLAT_ERR_UNKNOWN;
    }

    *child_out = cplat_internal_process_adopt_native((intptr_t)exec_info.hProcess);
    if (*child_out == NULL)
    {
        (void)CloseHandle(exec_info.hProcess); /* 引き取りの失敗を返す後始末のため、戻り値は明示的に破棄する */
        return CPLAT_ERR_UNKNOWN;
    }

    *child_pid_out = child_pid;
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

    /*
     * このコールバックは戻り値を持たず、書き込みの失敗を伝える先がないため、
     * fwrite と fflush の戻り値は明示的に破棄する。
     */
    (void)fwrite(data, 1, size, fp);
    (void)fflush(fp);
}

/**
 *  @brief          名前付きパイプへ設定するアクセス許可です。
 */
typedef struct elevated_pipe_security
{
    SECURITY_ATTRIBUTES attributes; /**< CreateNamedPipeW() へ渡す属性です。descriptor を指します。 */
    SECURITY_DESCRIPTOR descriptor; /**< dacl を持つセキュリティ記述子です。 */
    PACL dacl;                      /**< 確保した DACL です。未確保の場合は NULL です。 */
} elevated_pipe_security;

/**
 *  @brief          昇格プロセスの出力を受け取る名前付きパイプ 1 本分の状態です。
 */
typedef struct elevated_pipe_channel
{
    HANDLE pipe;           /**< パイプのサーバー側のハンドルです。未作成の場合は INVALID_HANDLE_VALUE です。 */
    HANDLE event;          /**< 重複 I/O の完了を受け取る手動リセットのイベントです。未作成の場合は NULL です。 */
    OVERLAPPED overlapped; /**< ConnectNamedPipe() の重複 I/O の状態です。 */
    int connected;         /**< クライアントの接続が完了している場合は 1 です。 */
    int pending_connect;   /**< ConnectNamedPipe() の完了を待っている場合は 1 です。 */
    int verified;          /**< クライアントのプロセス ID を照合済みの場合は 1 です。 */
    int eof;               /**< クライアントがパイプを閉じ、終端まで読み取った場合は 1 です。 */
    cplat_elevated_process_stream stream; /**< コールバックへ渡すストリームの種別です。 */
} elevated_pipe_channel;

/**
 *  @brief          名前付きパイプのトークンが仕様の文字種と長さ上限を満たすかを検証します。
 */
static int is_valid_pipe_token(const char *token)
{
    size_t len = 0;

    if (token == NULL || token[0] == '\0')
    {
        return 0;
    }

    while (token[len] != '\0')
    {
        const char ch = token[len];
        const int is_digit = (ch >= '0' && ch <= '9');
        const int is_lower_hex = (ch >= 'a' && ch <= 'f');
        const int is_hyphen = (ch == '-');

        if (!is_digit && !is_lower_hex && !is_hyphen)
        {
            return 0;
        }

        len++;
        if (len > CPLAT_ELEVATED_PIPE_TOKEN_MAX_LEN)
        {
            return 0;
        }
    }

    return 1;
}

/**
 *  @brief          呼び出し元 PID と暗号論的乱数からパイプ名のトークンを生成します。
 */
static int generate_pipe_token(char *token_out, const size_t token_out_size)
{
    static const char hex_digits[] = "0123456789abcdef";
    uint8_t random_bytes[CPLAT_ELEVATED_PIPE_TOKEN_RANDOM_BYTES];
    char random_hex[(CPLAT_ELEVATED_PIPE_TOKEN_RANDOM_BYTES * 2U) + 1U];
    size_t i;

    if (cplat_random_bytes(random_bytes, sizeof(random_bytes)) != CPLAT_OK)
    {
        return -1;
    }

    for (i = 0; i < sizeof(random_bytes); i++)
    {
        random_hex[i * 2U] = hex_digits[(random_bytes[i] >> 4U) & 0x0FU];
        random_hex[(i * 2U) + 1U] = hex_digits[random_bytes[i] & 0x0FU];
    }

    random_hex[sizeof(random_hex) - 1U] = '\0';

    if (cplat_snprintf(token_out, token_out_size, "%lu-%s", (unsigned long)GetCurrentProcessId(), random_hex) !=
        CPLAT_OK)
    {
        return -1;
    }

    return 0;
}

/**
 *  @brief          トークンと接尾辞から名前付きパイプのワイド文字列パスを組み立てます。
 */
static int build_pipe_name(wchar_t *pipe_name_out, const size_t pipe_name_cap, const char *token, const wchar_t *suffix,
                           const size_t suffix_chars_with_nul)
{
    const size_t prefix_chars = (sizeof(CPLAT_ELEVATED_PIPE_NAME_PREFIX) / sizeof(wchar_t)) - 1U;
    size_t pos;
    size_t i;

    if (!is_valid_pipe_token(token))
    {
        return -1;
    }

    pos = prefix_chars;
    for (i = 0; token[i] != '\0'; i++)
    {
        pos++;
    }

    if (pos + suffix_chars_with_nul > pipe_name_cap)
    {
        return -1;
    }

    memcpy(pipe_name_out, CPLAT_ELEVATED_PIPE_NAME_PREFIX, prefix_chars * sizeof(wchar_t));
    pos = prefix_chars;

    for (i = 0; token[i] != '\0'; i++)
    {
        pipe_name_out[pos] = (wchar_t)(unsigned char)token[i];
        pos++;
    }

    memcpy(pipe_name_out + pos, suffix, suffix_chars_with_nul * sizeof(wchar_t));

    return 0;
}

/**
 *  @brief          名前付きパイプに設定する DACL と SECURITY_ATTRIBUTES を破棄します。
 */
static void dispose_pipe_security(elevated_pipe_security *sec)
{
    if (sec->dacl != NULL)
    {
        cplat_free(sec->dacl);
        sec->dacl = NULL;
    }
}

/**
 *  @brief          LocalSystem、Administrators、呼び出し元ユーザーへ許可を与える DACL を構築します。
 */
static int init_pipe_security(elevated_pipe_security *sec)
{
    BYTE system_sid_buf[SECURITY_MAX_SID_SIZE];
    BYTE admins_sid_buf[SECURITY_MAX_SID_SIZE];
    DWORD system_sid_size = (DWORD)sizeof(system_sid_buf);
    DWORD admins_sid_size = (DWORD)sizeof(admins_sid_buf);
    HANDLE token = NULL;
    DWORD user_info_size = 0;
    TOKEN_USER *user_info = NULL;
    PSID system_sid = (PSID)system_sid_buf;
    PSID admins_sid = (PSID)admins_sid_buf;
    PSID user_sid;
    DWORD acl_size;
    PACL dacl;

    ZeroMemory(sec, sizeof(*sec));

    if (!CreateWellKnownSid(WinLocalSystemSid, NULL, system_sid, &system_sid_size) ||
        !CreateWellKnownSid(WinBuiltinAdministratorsSid, NULL, admins_sid, &admins_sid_size))
    {
        return -1;
    }

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
    {
        return -1;
    }

    if (GetTokenInformation(token, TokenUser, NULL, 0, &user_info_size) ||
        GetLastError() != ERROR_INSUFFICIENT_BUFFER || user_info_size == 0)
    {
        (void)CloseHandle(token); /* TokenUser サイズ取得の失敗の後始末のため、戻り値は明示的に破棄する */
        return -1;
    }

    user_info = (TOKEN_USER *)cplat_malloc((size_t)user_info_size);
    if (user_info == NULL)
    {
        (void)CloseHandle(token); /* メモリ確保失敗の後始末のため、戻り値は明示的に破棄する */
        return -1;
    }

    if (!GetTokenInformation(token, TokenUser, user_info, user_info_size, &user_info_size))
    {
        cplat_free(user_info);
        (void)CloseHandle(token); /* TokenUser 取得失敗の後始末のため、戻り値は明示的に破棄する */
        return -1;
    }

    (void)CloseHandle(token); /* SID の取得を終えた後の解放で結果が変わらないため、戻り値は明示的に破棄する */

    user_sid = user_info->User.Sid;
    if (user_sid == NULL || !IsValidSid(user_sid))
    {
        cplat_free(user_info);
        return -1;
    }

    acl_size = (DWORD)sizeof(ACL) + (3U * ((DWORD)sizeof(ACCESS_ALLOWED_ACE) - (DWORD)sizeof(DWORD))) +
               GetLengthSid(system_sid) + GetLengthSid(admins_sid) + GetLengthSid(user_sid);
    dacl = (PACL)cplat_malloc((size_t)acl_size);
    if (dacl == NULL)
    {
        cplat_free(user_info);
        return -1;
    }

    if (!InitializeAcl(dacl, acl_size, ACL_REVISION) ||
        !AddAccessAllowedAce(dacl, ACL_REVISION, GENERIC_ALL, system_sid) ||
        !AddAccessAllowedAce(dacl, ACL_REVISION, FILE_GENERIC_WRITE, admins_sid) ||
        !AddAccessAllowedAce(dacl, ACL_REVISION, GENERIC_ALL, user_sid) ||
        !InitializeSecurityDescriptor(&sec->descriptor, SECURITY_DESCRIPTOR_REVISION) ||
        !SetSecurityDescriptorDacl(&sec->descriptor, TRUE, dacl, FALSE))
    {
        cplat_free(dacl);
        cplat_free(user_info);
        return -1;
    }

    cplat_free(user_info);
    sec->dacl = dacl;
    sec->attributes.nLength = (DWORD)sizeof(sec->attributes);
    sec->attributes.lpSecurityDescriptor = &sec->descriptor;
    sec->attributes.bInheritHandle = FALSE;

    return 0;
}

/**
 *  @brief          重複 I/O の取り消しと完了待機を行ってから、パイプとイベントを閉じます。
 */
static void cancel_and_close_pipe_channel(elevated_pipe_channel *ch)
{
    if (ch->pipe != INVALID_HANDLE_VALUE && ch->pending_connect != 0 && ch->connected == 0)
    {
        DWORD transferred = 0;

        /*
         * 重複 I/O を取り消した後、完了を待機してから OVERLAPPED とイベントを破棄する。
         * 完了前に破棄するとカーネルが解放済みの構造体へ書き込むおそれがあるため。
         * 後始末の経路であり返す結果を変えないため、戻り値は明示的に破棄する。
         */
        if (CancelIoEx(ch->pipe, &ch->overlapped) || GetLastError() != ERROR_NOT_FOUND)
        {
            (void)GetOverlappedResult(ch->pipe, &ch->overlapped, &transferred, TRUE);
        }

        ch->pending_connect = 0;
    }

    if (ch->pipe != INVALID_HANDLE_VALUE)
    {
        (void)CloseHandle(ch->pipe); /* パイプの後始末のため、戻り値は明示的に破棄する */
        ch->pipe = INVALID_HANDLE_VALUE;
    }

    if (ch->event != NULL)
    {
        (void)CloseHandle(ch->event); /* イベントの後始末のため、戻り値は明示的に破棄する */
        ch->event = NULL;
    }
}

/**
 *  @brief          名前付きパイプと重複 I/O 用イベントを作成し、非同期で接続待機を開始します。
 */
static int init_pipe_channel(elevated_pipe_channel *ch, const cplat_elevated_process_stream stream,
                             const wchar_t *pipe_name, LPSECURITY_ATTRIBUTES sec_attr)
{
    ZeroMemory(ch, sizeof(*ch));
    ch->pipe = INVALID_HANDLE_VALUE;
    ch->stream = stream;

    /* パイプ名はワイド文字列で組み立てるため CreateNamedPipeU を使わない。 */
    ch->pipe = CreateNamedPipeW(pipe_name, PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED,
                                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                                CPLAT_ELEVATED_PIPE_MAX_INSTANCES, CPLAT_ELEVATED_PIPE_OUT_BUFFER_BYTES,
                                CPLAT_ELEVATED_PIPE_IN_BUFFER_BYTES, 0, sec_attr);
    if (ch->pipe == INVALID_HANDLE_VALUE)
    {
        return -1;
    }

    ch->event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (ch->event == NULL)
    {
        (void)CloseHandle(ch->pipe); /* イベント作成失敗の後始末のため、戻り値は明示的に破棄する */
        ch->pipe = INVALID_HANDLE_VALUE;
        return -1;
    }

    ch->overlapped.hEvent = ch->event;
    if (ConnectNamedPipe(ch->pipe, &ch->overlapped))
    {
        ch->connected = 1;
    }
    else
    {
        DWORD err = GetLastError();

        if (err == ERROR_IO_PENDING)
        {
            ch->pending_connect = 1;
        }
        else if (err == ERROR_PIPE_CONNECTED)
        {
            ch->connected = 1;
        }
        else
        {
            (void)CloseHandle(ch->event); /* ConnectNamedPipe 失敗の後始末のため、戻り値は明示的に破棄する */
            ch->event = NULL;
            (void)CloseHandle(ch->pipe); /* ConnectNamedPipe 失敗の後始末のため、戻り値は明示的に破棄する */
            ch->pipe = INVALID_HANDLE_VALUE;
            return -1;
        }
    }

    return 0;
}

/**
 *  @brief          接続したパイプのクライアント PID が起動した昇格プロセスの PID と一致するか照合します。
 */
static int verify_pipe_client_pid(elevated_pipe_channel *ch, const DWORD child_pid)
{
    ULONG client_pid = 0;

    if (ch->verified != 0)
    {
        return 0;
    }

    if (!GetNamedPipeClientProcessId(ch->pipe, &client_pid) || (DWORD)client_pid != child_pid)
    {
        return -1;
    }

    ch->verified = 1;
    return 0;
}

/**
 *  @brief          出力パイプから、現時点で読み取れる内容をすべて読み取ってコールバックへ渡します。
 *
 *  読み取れる量を PeekNamedPipe で確認してから OVERLAPPED を指定して ReadFile を呼ぶため、
 *  パイプが空でも待機しません。PeekNamedPipe または ReadFile が ERROR_BROKEN_PIPE を返した
 *  場合は終端として正常に終了します。それ以外のエラーは異常として -1 を返します。
 *  see: https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-peeknamedpipe
 */
static int pump_output_pipe(elevated_pipe_channel *ch, cplat_elevated_process_output_fn output_fn, void *context)
{
    char buffer[CPLAT_PROCESS_OUTPUT_CHUNK_SIZE];

    if (ch->eof != 0)
    {
        return 0;
    }

    for (;;)
    {
        OVERLAPPED read_ov;
        DWORD available = 0;
        DWORD to_read;
        DWORD bytes_read = 0;

        if (!PeekNamedPipe(ch->pipe, NULL, 0, NULL, &available, NULL))
        {
            if (GetLastError() == ERROR_BROKEN_PIPE)
            {
                ch->eof = 1;
                return 0;
            }

            return -1;
        }

        if (available == 0)
        {
            return 0;
        }

        to_read = (DWORD)sizeof(buffer);
        if (available < to_read)
        {
            to_read = available;
        }

        /* ReadFile の重複 I/O 完了通知に同じイベントを再利用するため、呼び出し前に非シグナルへ戻す。 */
        if (!ResetEvent(ch->event))
        {
            return -1;
        }

        ZeroMemory(&read_ov, sizeof(read_ov));
        read_ov.hEvent = ch->event;

        if (!ReadFile(ch->pipe, buffer, to_read, &bytes_read, &read_ov))
        {
            DWORD err = GetLastError();

            if (err == ERROR_IO_PENDING)
            {
                if (!GetOverlappedResult(ch->pipe, &read_ov, &bytes_read, TRUE))
                {
                    if (GetLastError() == ERROR_BROKEN_PIPE)
                    {
                        ch->eof = 1;
                        return 0;
                    }

                    return -1;
                }
            }
            else if (err == ERROR_BROKEN_PIPE)
            {
                ch->eof = 1;
                return 0;
            }
            else
            {
                return -1;
            }
        }

        if (bytes_read == 0)
        {
            return 0;
        }

        output_fn(ch->stream, buffer, (size_t)bytes_read, context);
    }
}

/**
 *  @brief          出力パイプの接続完了確認、クライアント PID 照合、読み取りを行います。
 */
static int process_pipe_channel(elevated_pipe_channel *ch, const DWORD child_pid,
                                cplat_elevated_process_output_fn output_fn, void *context)
{
    if (ch->connected == 0)
    {
        DWORD dummy = 0;

        if (!GetOverlappedResult(ch->pipe, &ch->overlapped, &dummy, FALSE))
        {
            if (GetLastError() == ERROR_IO_INCOMPLETE)
            {
                return 0;
            }

            return -1;
        }

        ch->pending_connect = 0;
        ch->connected = 1;
    }

    if (verify_pipe_client_pid(ch, child_pid) != 0)
    {
        return -1;
    }

    return pump_output_pipe(ch, output_fn, context);
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
        (void)CloseHandle(handle); /* すでに -1 を返す経路の後始末のため、戻り値は明示的に破棄する */
        return -1;
    }

    /*
     * プログラムの開始直後に呼ぶ契約のため、未出力のデータはない。
     * 付け替え前の出力先 (非表示のコンソール) は捨てる対象であり、失敗しても結果は変わらないため、
     * 戻り値は明示的に破棄する。
     */
    (void)fflush(stream);

    /*
     * _close の失敗は fd がすでに無効な場合 (EBADF) だけである。
     * 閉じられずに残った場合も、後続の _dup2 が複製先を閉じてから複製するため、戻り値は明示的に破棄する。
     * see: https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/close
     * see: https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/dup-dup2
     */
    (void)_close(expected_fd);

    new_fd = _open_osfhandle((intptr_t)handle, open_flags);
    if (new_fd < 0)
    {
        (void)CloseHandle(handle); /* すでに -1 を返す経路の後始末のため、戻り値は明示的に破棄する */
        return -1;
    }

    if (new_fd != expected_fd)
    {
        if (_dup2(new_fd, expected_fd) != 0)
        {
            /* _dup2 の失敗の後始末であり、すでに -1 を返す経路のため、戻り値は明示的に破棄する。 */
            (void)_close(new_fd);
            return -1;
        }

        /* 付け替え先の fd は成立済みで、失敗しても余分な fd が 1 つ残るだけのため、戻り値は明示的に破棄する。 */
        (void)_close(new_fd);
    }

    clearerr(stream);
    if (!SetStdHandle(std_handle_id, (HANDLE)_get_osfhandle(expected_fd)))
    {
        return -1;
    }

    return 0;
}

/**
 *  @brief          argv から出力パイプ フラグを取り除き、トークン文字列を検証して取り出します。
 *  @param[in,out]  argc            引数の数へのポインター。
 *  @param[in,out]  argv            引数配列。
 *  @param[out]     token_out       検証済みトークン文字列の格納先。
 *  @param[in]      token_out_size  @p token_out のバイト数。
 *  @return         フラグがない場合は 0、値が正しい場合は 1、値が不正な場合は -1 を返します。
 *
 *  値が不正な場合も、フラグは argv から取り除きます。
 */
static int extract_output_pipes_arg(int *argc, char **argv, char *token_out, const size_t token_out_size)
{
    const char *prefix = CPLAT_PROCESS_OUTPUT_PIPES_FLAG "=";
    const char *value;
    size_t prefix_len;
    int n;
    int i;
    int j;

    if (token_out == NULL || token_out_size == 0)
    {
        return -1;
    }

    token_out[0] = '\0';

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

    if (!is_valid_pipe_token(value))
    {
        return -1;
    }

    if (cplat_strcpy(token_out, token_out_size, value) != 0)
    {
        token_out[0] = '\0';
        return -1;
    }

    return 1;
}
#endif /* PLATFORM_ */

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_elevated_process_is_elevated(int *elevated_out)
{
    if (elevated_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    *elevated_out = 0;

#if defined(PLATFORM_LINUX)
    if (geteuid() == 0)
    {
        *elevated_out = 1;
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
                    *elevated_out = 1;
                }

                rc = CPLAT_OK;
            }

            (void)CloseHandle(token); /* 昇格状態は rc に確定済みのため、戻り値は明示的に破棄する */
        }

        return rc;
    }
#else
    return CPLAT_ERR_UNSUPPORTED;
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_elevated_process_run_if_needed(const char *arguments, int *exit_code_out, int *handled_out)
{
    if (exit_code_out == NULL || handled_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    *exit_code_out = 0;
    *handled_out = 0;

#if defined(PLATFORM_WINDOWS)
    {
        SHELLEXECUTEINFOW exec_info;
        char exe_path[PLATFORM_PATH_UTF8_MAX];
        char *combined_arguments = NULL;
        const char *effective_arguments;
        wchar_t *wide_arguments = NULL;
        wchar_t *wide_exe_path;
        cplat_process *child_process;
        int child_exit_code;
        int elevated;
        int inherit_console;
        int diag_enabled;
        int conversion_result;

        if (cplat_elevated_process_is_elevated(&elevated) != CPLAT_OK)
        {
            *exit_code_out = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }

        if (elevated != 0)
        {
            return CPLAT_OK;
        }

        /*
         * セッション 0 にはユーザーが操作できるデスクトップがないため、
         * 昇格を試みない。
         */
        if (is_current_session_interactive() == 0)
        {
            *exit_code_out = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }

        if (cplat_process_get_executable_path(exe_path, sizeof(exe_path)) != CPLAT_OK)
        {
            *exit_code_out = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }

        /*
         * 親にコンソールがある場合のみ、昇格プロセスへ親コンソールを引き継ぐ。
         * 昇格プロセス側は cplat_console_attach_parent() で再接続する。
         */
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
            char extra[CPLAT_PROCESS_EXTRA_ARGUMENT_SIZE];
            int ret;

            parent_pid = 0;

            /*
             * 親コンソールの window ハンドルを引き継ぎ、子側が一時コンソールではなく
             * 親コンソールへ確実に再接続できたかを確認できるようにする。
             */
            parent_console_window = 0;
            arg_len = 0;

            if (arguments != NULL)
            {
                arg_len = strlen(arguments);
            }

            /* 区切り空白 + フラグ + '=' + PID (最大 10 桁) + ':' + HWND (最大 20 桁) + 終端の余裕を確保する。 */
            buf_sz = arg_len + 1;

            if (diag_enabled != 0)
            {
                buf_sz += strlen(CPLAT_CONSOLE_ATTACH_DIAG_FLAG) + 1;
            }

            if (inherit_console != 0)
            {
                parent_pid = GetCurrentProcessId();
                parent_console_window = (unsigned long long)(uintptr_t)GetConsoleWindow();
                buf_sz += strlen(CPLAT_CONSOLE_HANDOVER_FLAG) + CPLAT_PROCESS_HANDOVER_EXTRA_BYTES;
            }

            combined_arguments = (char *)cplat_malloc(buf_sz);
            if (combined_arguments == NULL)
            {
                *exit_code_out = EXIT_FAILURE;
                return CPLAT_ERR_OUT_OF_MEMORY;
            }

            if (arg_len > 0)
            {
                ret = cplat_snprintf(combined_arguments, buf_sz, "%s", arguments);
                if (ret != CPLAT_OK)
                {
                    cplat_free(combined_arguments);
                    *exit_code_out = EXIT_FAILURE;
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
                    *exit_code_out = EXIT_FAILURE;
                    return CPLAT_ERR_UNKNOWN;
                }

                ret = cplat_strcat(combined_arguments, buf_sz, extra);
                if (ret != CPLAT_OK)
                {
                    cplat_free(combined_arguments);
                    *exit_code_out = EXIT_FAILURE;
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
                    *exit_code_out = EXIT_FAILURE;
                    return CPLAT_ERR_UNKNOWN;
                }

                ret = cplat_strcat(combined_arguments, buf_sz, extra);
                if (ret != CPLAT_OK)
                {
                    cplat_free(combined_arguments);
                    *exit_code_out = EXIT_FAILURE;
                    return CPLAT_ERR_UNKNOWN;
                }
            }

            effective_arguments = combined_arguments;
        }

        conversion_result = cplat_utf8_to_wstr_alloc(&wide_exe_path, exe_path);
        if (conversion_result != CPLAT_OK)
        {
            cplat_free(combined_arguments);
            *exit_code_out = EXIT_FAILURE;
            return conversion_result;
        }

        if (effective_arguments != NULL)
        {
            conversion_result = cplat_utf8_to_wstr_alloc(&wide_arguments, effective_arguments);
            if (conversion_result != CPLAT_OK)
            {
                cplat_free(combined_arguments);
                cplat_free(wide_exe_path);
                *exit_code_out = EXIT_FAILURE;
                return conversion_result;
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

        /*
         * コンソール引き継ぎ時は昇格プロセスの一時コンソールを隠す。
         * 引き継がない場合は従来どおり通常表示とする。
         */
        if (inherit_console != 0)
        {
            exec_info.nShow = SW_HIDE;
        }
        else
        {
            exec_info.nShow = SW_SHOWNORMAL;
        }

        *handled_out = 1;

        if (!ShellExecuteExW(&exec_info))
        {
            cplat_free(wide_arguments);
            cplat_free(wide_exe_path);
            *exit_code_out = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }

        cplat_free(wide_arguments);
        cplat_free(wide_exe_path);

        child_process = cplat_internal_process_adopt_native((intptr_t)exec_info.hProcess);
        if (child_process == NULL)
        {
            (void)CloseHandle(exec_info.hProcess); /* 引き取りの失敗を返す後始末のため、戻り値は明示的に破棄する */
            *exit_code_out = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }

        if (cplat_process_wait(child_process) != CPLAT_OK ||
            cplat_process_get_exit_code(child_process, &child_exit_code) != CPLAT_OK)
        {
            cplat_process_dispose(child_process);
            *exit_code_out = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }

        cplat_process_dispose(child_process);

        *exit_code_out = child_exit_code;
        return CPLAT_OK;
    }
#elif defined(PLATFORM_LINUX)
    {
        int elevated;

        /* Linux では別プロセスを起動しないため、引数は使わない。 */
        (void)arguments;

        /*
         * Linux の実装は常に成功する geteuid() の結果を局所変数へ格納するだけで、失敗しないため、
         * 戻り値は明示的に破棄する。
         * see: https://man7.org/linux/man-pages/man2/geteuid.2.html
         */
        (void)cplat_elevated_process_is_elevated(&elevated);

        if (elevated == 0)
        {
            *exit_code_out = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }

        return CPLAT_OK;
    }
#else
    (void)arguments;
    *exit_code_out = EXIT_FAILURE;

    return CPLAT_ERR_UNSUPPORTED;
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_elevated_process_run_with_result(const char *arguments, int *exit_code_out, int *handled_out,
                                           char *result_message, size_t result_message_size)
{
    if (exit_code_out == NULL || handled_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    *exit_code_out = 0;
    *handled_out = 0;

    if (result_message != NULL && result_message_size > 0)
    {
        result_message[0] = '\0';
    }

#if defined(PLATFORM_WINDOWS)
    {
        SHELLEXECUTEINFOW exec_info;
        char exe_path[PLATFORM_PATH_UTF8_MAX];
        char result_path[PLATFORM_PATH_UTF8_MAX];
        wchar_t *wide_temp_dir = NULL;
        wchar_t wide_result_path[PLATFORM_PATH_MAX];
        char *combined_arguments;
        wchar_t *wide_arguments;
        wchar_t *wide_exe_path;
        cplat_process *child_process;
        int child_exit_code;
        int elevated;
        size_t arg_len;
        size_t buf_sz;
        int conversion_result;

        if (cplat_elevated_process_is_elevated(&elevated) != CPLAT_OK)
        {
            *exit_code_out = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }

        if (elevated != 0)
        {
            return CPLAT_OK;
        }

        /*
         * セッション 0 にはユーザーが操作できるデスクトップがないため、
         * 昇格を試みない。
         */
        if (is_current_session_interactive() == 0)
        {
            *exit_code_out = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }

        if (cplat_process_get_executable_path(exe_path, sizeof(exe_path)) != CPLAT_OK)
        {
            *exit_code_out = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }

        /*
         * 昇格プロセスが結果メッセージを書き込む一時ファイルを確保する。GetTempFileNameW は
         * 一意な空ファイルを作成するため、並行する別プロセスとの衝突を避けられる。
         * GetTempFileNameW はディレクトリ部が MAX_PATH - 14 を超えると失敗する。
         * see: https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-gettempfilenamew
         */
        /*
         * GetTempPath2W / GetTempPathW の実行時選択と十分な領域の確保を内部の共通関数へ集約し、
         * 以降の GetTempFileNameW へワイドのまま渡すため、*U ラッパーを経由しない。
         */
        conversion_result = cplat_internal_get_temp_dir_w(&wide_temp_dir, NULL, NULL);
        if (conversion_result != CPLAT_OK)
        {
            *exit_code_out = EXIT_FAILURE;
            return conversion_result;
        }

        if (GetTempFileNameW(wide_temp_dir, L"cur", 0, wide_result_path) == 0)
        {
            const DWORD error_code = GetLastError();

            cplat_free(wide_temp_dir);
            *exit_code_out = EXIT_FAILURE;
            return cplat_internal_error_report_windows_error(NULL, error_code);
        }
        cplat_free(wide_temp_dir);

        /*
         * 一時ファイルの削除は、結果を読み終えた後か失敗を返す経路の後始末であり、削除できなくても返す結果が
         * 変わらないため、この関数の DeleteFileW の戻り値は明示的に破棄する。
         */
        conversion_result = cplat_wpath_to_utf8(result_path, sizeof(result_path), NULL, wide_result_path);
        if (conversion_result != CPLAT_OK)
        {
            (void)DeleteFileW(wide_result_path);
            *exit_code_out = EXIT_FAILURE;
            return conversion_result;
        }

        arg_len = 0;

        if (arguments != NULL)
        {
            arg_len = strlen(arguments);
        }

        /* 区切り空白 + フラグ + '=' + '"' + パス + '"' + 終端の余裕を確保する。 */
        buf_sz = arg_len + 1 + strlen(CPLAT_PROCESS_RESULT_TARGET_FLAG) + 1 + 1 + strlen(result_path) + 1 + 1;
        combined_arguments = (char *)cplat_malloc(buf_sz);
        if (combined_arguments == NULL)
        {
            *exit_code_out = EXIT_FAILURE;
            return CPLAT_ERR_OUT_OF_MEMORY;
        }

        /*
         * result_path はユーザー プロファイル配下 (ユーザー名に空白を含みうる) から生成されるため、
         * クォートしないと CRT のコマンド ライン分割で複数トークンとなり、
         * cplat_elevated_process_extract_result_target() でのフラグ抽出に失敗する。
         * クォート区間内の空白はトークン区切りにならず、クォート文字自体は結果の argv から除去される。
         * see: https://learn.microsoft.com/en-us/cpp/c-language/parsing-c-command-line-arguments
         */
        /* buf_sz は展開後の長さに合わせて計算しており、切り詰めが起きないため、次の戻り値は明示的に破棄する。 */
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

        conversion_result = cplat_utf8_to_wstr_alloc(&wide_exe_path, exe_path);
        if (conversion_result != CPLAT_OK)
        {
            cplat_free(combined_arguments);
            (void)DeleteFileW(wide_result_path);
            *exit_code_out = EXIT_FAILURE;
            return conversion_result;
        }

        conversion_result = cplat_utf8_to_wstr_alloc(&wide_arguments, combined_arguments);
        cplat_free(combined_arguments);

        if (conversion_result != CPLAT_OK)
        {
            cplat_free(wide_exe_path);
            (void)DeleteFileW(wide_result_path);
            *exit_code_out = EXIT_FAILURE;
            return conversion_result;
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

        *handled_out = 1;

        if (!ShellExecuteExW(&exec_info))
        {
            cplat_free(wide_arguments);
            cplat_free(wide_exe_path);
            (void)DeleteFileW(wide_result_path);
            *exit_code_out = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }

        cplat_free(wide_arguments);
        cplat_free(wide_exe_path);

        child_process = cplat_internal_process_adopt_native((intptr_t)exec_info.hProcess);
        if (child_process == NULL)
        {
            (void)CloseHandle(exec_info.hProcess); /* 引き取りの失敗を返す後始末のため、戻り値は明示的に破棄する */
            (void)DeleteFileW(wide_result_path);
            *exit_code_out = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }

        if (cplat_process_wait(child_process) != CPLAT_OK ||
            cplat_process_get_exit_code(child_process, &child_exit_code) != CPLAT_OK)
        {
            cplat_process_dispose(child_process);
            (void)DeleteFileW(wide_result_path);
            *exit_code_out = EXIT_FAILURE;
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

                (void)CloseHandle(h); /* 結果メッセージは読み取り済みのため、戻り値は明示的に破棄する */
            }
        }

        (void)DeleteFileW(wide_result_path);

        *exit_code_out = child_exit_code;
        return CPLAT_OK;
    }
#elif defined(PLATFORM_LINUX)
    {
        int elevated;

        /* 別プロセスを起動しないため、引数は使わず、報告の余地がない。 */
        (void)result_message;
        (void)result_message_size;
        (void)arguments;

        /*
         * Linux の実装は常に成功する geteuid() の結果を局所変数へ格納するだけで、失敗しないため、
         * 戻り値は明示的に破棄する。
         * see: https://man7.org/linux/man-pages/man2/geteuid.2.html
         */
        (void)cplat_elevated_process_is_elevated(&elevated);

        if (elevated == 0)
        {
            *exit_code_out = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }

        return CPLAT_OK;
    }
#else
    (void)arguments;
    (void)result_message;
    (void)result_message_size;
    *exit_code_out = EXIT_FAILURE;

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
            return CPLAT_ERR_BUFFER_TOO_SMALL;
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
            (void)CloseHandle(h); /* 書き込みの失敗を返す後始末のため、戻り値は明示的に破棄する */
            return CPLAT_ERR_UNKNOWN;
        }

        /* 書き込みは WriteFile で成功しており、解放の失敗で返す結果を変えないため、戻り値は明示的に破棄する。 */
        (void)CloseHandle(h);
        return CPLAT_OK;
    }
#else
    (void)message;

    return CPLAT_ERR_UNKNOWN;
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_elevated_process_run_piped(const char *arguments, cplat_elevated_process_output_fn output_fn, void *context,
                                     int *exit_code_out, int *handled_out)
{
    if (exit_code_out == NULL || handled_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    *exit_code_out = 0;
    *handled_out = 0;

#if defined(PLATFORM_WINDOWS)
    {
        cplat_elevated_process_output_fn effective_output_fn;
        elevated_pipe_security pipe_security;
        elevated_pipe_channel stdout_ch;
        elevated_pipe_channel stderr_ch;
        char token[CPLAT_ELEVATED_PIPE_TOKEN_MAX_LEN + 1U];
        wchar_t stdout_pipe_name[CPLAT_ELEVATED_PIPE_NAME_CAP];
        wchar_t stderr_pipe_name[CPLAT_ELEVATED_PIPE_NAME_CAP];
        char *combined_arguments;
        const char *base_arguments;
        const char *separator;
        cplat_process *child_process;
        DWORD child_pid;
        int child_exit_code;
        int elevated;
        int wait_result;
        int io_failed;
        int rc;
        size_t buf_sz;

        if (cplat_elevated_process_is_elevated(&elevated) != CPLAT_OK)
        {
            *exit_code_out = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }

        if (elevated != 0)
        {
            return CPLAT_OK;
        }

        /*
         * セッション 0 にはユーザーが操作できるデスクトップがないため、
         * 昇格を試みない。
         */
        if (is_current_session_interactive() == 0)
        {
            *exit_code_out = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }

        effective_output_fn = output_fn;
        if (effective_output_fn == NULL)
        {
            effective_output_fn = write_to_own_std_stream;
        }

        if (generate_pipe_token(token, sizeof(token)) != 0)
        {
            *exit_code_out = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }

        if (build_pipe_name(stdout_pipe_name, sizeof(stdout_pipe_name) / sizeof(stdout_pipe_name[0]), token,
                            CPLAT_ELEVATED_PIPE_STDOUT_SUFFIX,
                            sizeof(CPLAT_ELEVATED_PIPE_STDOUT_SUFFIX) / sizeof(wchar_t)) != 0 ||
            build_pipe_name(stderr_pipe_name, sizeof(stderr_pipe_name) / sizeof(stderr_pipe_name[0]), token,
                            CPLAT_ELEVATED_PIPE_STDERR_SUFFIX,
                            sizeof(CPLAT_ELEVATED_PIPE_STDERR_SUFFIX) / sizeof(wchar_t)) != 0)
        {
            *exit_code_out = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }

        /*
         * 昇格プロセスは、別の管理者アカウントで動く場合がある。パイプの既定の ACL に頼らず、
         * Administrators グループへ書き込みを許可する。
         * see: https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipe-security-and-access-rights
         */
        if (init_pipe_security(&pipe_security) != 0)
        {
            *exit_code_out = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }

        /*
         * ShellExecuteExW() へ渡す SHELLEXECUTEINFOW には、ハンドルの継承を指定するメンバーがない。
         * see: https://learn.microsoft.com/en-us/windows/win32/api/shellapi/ns-shellapi-shellexecuteinfow
         * そのため、名前付きパイプを作成してトークンを内部フラグで渡し、昇格プロセス側が名前で開く。
         * 整合性レベルが低い主体は、既定では、整合性レベルが高いオブジェクトへ書き込めない。
         * パイプは未昇格の本プロセスが作成し、昇格プロセスが書き込むため、この制限を受けない。
         * see: https://learn.microsoft.com/en-us/windows/win32/secauthz/mandatory-integrity-control
         * 同じ名前のパイプが先に存在する場合は作成を失敗させ、インスタンスを 1 つに限り、
         * リモートからの接続を拒否する。
         * see: https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-createnamedpipew
         */
        if (init_pipe_channel(&stdout_ch, CPLAT_ELEVATED_PROCESS_STREAM_STDOUT, stdout_pipe_name,
                              &pipe_security.attributes) != 0)
        {
            dispose_pipe_security(&pipe_security);
            *exit_code_out = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }

        if (init_pipe_channel(&stderr_ch, CPLAT_ELEVATED_PROCESS_STREAM_STDERR, stderr_pipe_name,
                              &pipe_security.attributes) != 0)
        {
            cancel_and_close_pipe_channel(&stdout_ch);
            dispose_pipe_security(&pipe_security);
            *exit_code_out = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }

        dispose_pipe_security(&pipe_security);

        base_arguments = "";
        separator = "";

        if (arguments != NULL && arguments[0] != '\0')
        {
            base_arguments = arguments;
            separator = " ";
        }

        /* 区切り空白 + フラグ + '=' + トークン + 終端 */
        buf_sz = strlen(base_arguments) + 1U + strlen(CPLAT_PROCESS_OUTPUT_PIPES_FLAG) + 1U + strlen(token) + 1U;
        combined_arguments = (char *)cplat_malloc(buf_sz);
        if (combined_arguments == NULL)
        {
            cancel_and_close_pipe_channel(&stdout_ch);
            cancel_and_close_pipe_channel(&stderr_ch);
            *exit_code_out = EXIT_FAILURE;
            return CPLAT_ERR_OUT_OF_MEMORY;
        }

        rc = cplat_snprintf(combined_arguments, buf_sz, "%s%s%s=%s", base_arguments, separator,
                            CPLAT_PROCESS_OUTPUT_PIPES_FLAG, token);
        child_process = NULL;
        child_pid = 0;

        if (rc == CPLAT_OK)
        {
            *handled_out = 1;
            rc = start_elevated_self(combined_arguments, &child_process, &child_pid);
        }

        cplat_free(combined_arguments);

        if (rc != CPLAT_OK)
        {
            cancel_and_close_pipe_channel(&stdout_ch);
            cancel_and_close_pipe_channel(&stderr_ch);
            *exit_code_out = EXIT_FAILURE;

            if (rc == CPLAT_ERR_OUT_OF_MEMORY)
            {
                return CPLAT_ERR_OUT_OF_MEMORY;
            }

            return CPLAT_ERR_UNKNOWN;
        }

        io_failed = 0;

        /*
         * パイプの容量を超える出力で昇格プロセスが書き込み待ちにならないよう、
         * 終了を待つ間も接続の確認と出力の読み取りを続ける。
         */
        do
        {
            wait_result = cplat_process_timed_wait(child_process, CPLAT_PROCESS_OUTPUT_POLL_INTERVAL_MS);
            if (process_pipe_channel(&stdout_ch, child_pid, effective_output_fn, context) != 0 ||
                process_pipe_channel(&stderr_ch, child_pid, effective_output_fn, context) != 0)
            {
                io_failed = 1;
                break;
            }
        } while (wait_result == CPLAT_ERR_TIMEOUT);

        /*
         * 子プロセスの終了後に未接続のパイプがある場合は失敗とし、
         * 接続済みのパイプは残りのデータを読み切ってから閉じる。
         */
        if (io_failed == 0)
        {
            if (stdout_ch.connected == 0 || stderr_ch.connected == 0)
            {
                io_failed = 1;
            }
            else if (pump_output_pipe(&stdout_ch, effective_output_fn, context) != 0 ||
                     pump_output_pipe(&stderr_ch, effective_output_fn, context) != 0)
            {
                io_failed = 1;
            }
        }

        cancel_and_close_pipe_channel(&stdout_ch);
        cancel_and_close_pipe_channel(&stderr_ch);

        if (io_failed != 0 || wait_result != CPLAT_OK ||
            cplat_process_get_exit_code(child_process, &child_exit_code) != CPLAT_OK)
        {
            cplat_process_dispose(child_process);
            *exit_code_out = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }

        cplat_process_dispose(child_process);

        *exit_code_out = child_exit_code;
        return CPLAT_OK;
    }
#elif defined(PLATFORM_LINUX)
    {
        int elevated;

        /* Linux では別プロセスを起動しないため、引数と出力のコールバックは使わない。 */
        (void)arguments;
        (void)output_fn;
        (void)context;

        /*
         * Linux の実装は常に成功する geteuid() の結果を局所変数へ格納するだけで、失敗しないため、
         * 戻り値は明示的に破棄する。
         * see: https://man7.org/linux/man-pages/man2/geteuid.2.html
         */
        (void)cplat_elevated_process_is_elevated(&elevated);

        if (elevated == 0)
        {
            *exit_code_out = EXIT_FAILURE;
            return CPLAT_ERR_UNKNOWN;
        }

        return CPLAT_OK;
    }
#else
    (void)arguments;
    (void)output_fn;
    (void)context;
    *exit_code_out = EXIT_FAILURE;

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
        HANDLE stdout_handle;
        HANDLE stderr_handle;
        HANDLE stdin_handle;
        char token[CPLAT_ELEVATED_PIPE_TOKEN_MAX_LEN + 1U];
        wchar_t stdout_pipe_name[CPLAT_ELEVATED_PIPE_NAME_CAP];
        wchar_t stderr_pipe_name[CPLAT_ELEVATED_PIPE_NAME_CAP];
        int found;

        found = extract_output_pipes_arg(argc, argv, token, sizeof(token));
        if (found == 0)
        {
            return CPLAT_OK;
        }

        if (found < 0)
        {
            return CPLAT_ERR_UNKNOWN;
        }

        if (build_pipe_name(stdout_pipe_name, sizeof(stdout_pipe_name) / sizeof(stdout_pipe_name[0]), token,
                            CPLAT_ELEVATED_PIPE_STDOUT_SUFFIX,
                            sizeof(CPLAT_ELEVATED_PIPE_STDOUT_SUFFIX) / sizeof(wchar_t)) != 0 ||
            build_pipe_name(stderr_pipe_name, sizeof(stderr_pipe_name) / sizeof(stderr_pipe_name[0]), token,
                            CPLAT_ELEVATED_PIPE_STDERR_SUFFIX,
                            sizeof(CPLAT_ELEVATED_PIPE_STDERR_SUFFIX) / sizeof(wchar_t)) != 0)
        {
            return CPLAT_ERR_UNKNOWN;
        }

        /*
         * パイプのサーバー (未昇格の呼び出し元) が本プロセスになりすませないよう、偽装レベルを指定して開く。
         * 指定しない場合、サーバーは SecurityImpersonation のレベルで偽装できる。
         * see: https://learn.microsoft.com/en-us/windows/win32/ipc/impersonating-a-named-pipe-client
         * フラグの値は未昇格の側が決めるため、トークンの文字を検査し、パスは本関数で組み立てる。
         * パイプ名はワイド文字列で組み立てるため CreateFileU を使わない。
         */
        stdout_handle = CreateFileW(stdout_pipe_name, GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                                    SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS, NULL);
        if (stdout_handle == INVALID_HANDLE_VALUE)
        {
            return CPLAT_ERR_UNKNOWN;
        }

        stderr_handle = CreateFileW(stderr_pipe_name, GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                                    SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS, NULL);
        if (stderr_handle == INVALID_HANDLE_VALUE)
        {
            (void)CloseHandle(stdout_handle); /* stderr オープン失敗の後始末のため、戻り値は明示的に破棄する */
            return CPLAT_ERR_UNKNOWN;
        }

        /*
         * 開いたオブジェクトがパイプ以外を指す場合は付け替えない。
         * 失敗を返す経路の後始末は返す結果を変えないため、
         * この関数の失敗の経路の CloseHandle の戻り値は明示的に破棄する。
         */
        if (GetFileType(stdout_handle) != FILE_TYPE_PIPE || GetFileType(stderr_handle) != FILE_TYPE_PIPE)
        {
            (void)CloseHandle(stdout_handle);
            (void)CloseHandle(stderr_handle);
            return CPLAT_ERR_UNKNOWN;
        }

        /*
         * 昇格プロセスのコンソールは非表示のため、標準入力の読み取りで応答不能にならないよう
         * NUL デバイスへ付け替える。ワイド文字列リテラルを渡すため、CreateFileU を使わない。
         */
        stdin_handle =
            CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (stdin_handle == INVALID_HANDLE_VALUE)
        {
            (void)CloseHandle(stdout_handle);
            (void)CloseHandle(stderr_handle);
            return CPLAT_ERR_UNKNOWN;
        }

        /* 改行コードは呼び出し元で変換するため、バイナリ モードで開く。 */
        if (reopen_std_stream(stdout, stdout_handle, _O_BINARY | _O_WRONLY, STD_OUTPUT_HANDLE) != 0)
        {
            (void)CloseHandle(stderr_handle);
            (void)CloseHandle(stdin_handle);
            return CPLAT_ERR_UNKNOWN;
        }

        if (reopen_std_stream(stderr, stderr_handle, _O_BINARY | _O_WRONLY, STD_ERROR_HANDLE) != 0)
        {
            (void)CloseHandle(stdin_handle);
            return CPLAT_ERR_UNKNOWN;
        }

        if (reopen_std_stream(stdin, stdin_handle, _O_BINARY | _O_RDONLY, STD_INPUT_HANDLE) != 0)
        {
            return CPLAT_ERR_UNKNOWN;
        }

        /* 呼び出し元での表示が昇格プロセスの終了まで遅れないよう、stdout をバッファーなしにする。 */
        if (setvbuf(stdout, NULL, _IONBF, 0) != 0)
        {
            return CPLAT_ERR_UNKNOWN;
        }

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
