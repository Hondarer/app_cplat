/**
 *******************************************************************************
 *  @file           trace_eventlog.c
 *  @brief          Windows イベント ログへ出力するシンクを実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/06/14
 *  @version        1.0.0
 *
 *  Windows のアプリケーション イベント ログへの書き込みと、共通イベント
 *  ソースのレジストリ登録/削除を提供します。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/base/platform.h>
#include <cplat/crt/stdlib.h>

#if defined(PLATFORM_WINDOWS)

    #include <cplat/base/result.h>
    #include <cplat/base/windows_sdk.h>
    #include <cplat/crt/path.h>
    #include <cplat/crt/stdio.h>
    #include <cplat/crt/wchar_conv.h>
    #include <cplat/win32/wchar_conv_internal.h>
    #include <cplat/runtime/process.h>
    #include <cplat/sync/sync.h>
    #include <cplat/trace/eventlog.h>
    #include <cplat/trace/tracer.h>
    #include <assert.h>
    #include <inttypes.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <wchar.h>

    #include <cplat/trace/backends/eventlog/eventlog_internal.h>

/** イベント ソース登録キーのレジストリ パス接頭辞 (HKLM 配下)。 */
    #define EVENTLOG_KEY_PREFIX L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\"

/**
 *  イベント ID の基底値。
 *
 *  EventMessageFile と CategoryMessageFile が同一ファイル (libcplat_eventlog_messages.dll) を
 *  指すため、メッセージ テーブルの ID 空間は 1 つに統合されます。カテゴリは Windows 仕様上
 *  1..CategoryCount に固定配置するため、イベント ID は重複を避けて 0x1000 番台に配置します。
 *  cplat_eventlog_messages/eventlog_messages.mc の MessageId (0x1001..0x1036) と一致させます。
 */
    #define EVENTLOG_EVENT_ID_NO_IDENTIFIER       0x1001U
    #define EVENTLOG_EVENT_ID_FILE_IDENTIFIER     0x1011U
    #define EVENTLOG_EVENT_ID_INSTANCE_IDENTIFIER 0x1021U
    #define EVENTLOG_EVENT_ID_BOTH_IDENTIFIERS    0x1031U

/**
 *  EventLog に渡す置換文字列数。
 */
    #define EVENTLOG_STRING_COUNT 5

/*
 * int64_t の識別子を 10 進文字列へ書式化する作業配列の大きさ [byte] (終端 NUL を含む)。
 * int64_t の 10 進表記は符号を含めて最大 20 文字であり、終端 NUL を含めて最小 21 [byte] を要する。
 * 制約: 21 以上とすること。変更時は作業配列のスタック消費量だけが変わる。
 */
    #define EVENTLOG_IDENTIFIER_TEXT_SIZE 32U

static_assert(EVENTLOG_IDENTIFIER_TEXT_SIZE >= 21U, "identifier text buffer must fit int64_t and NUL");

    /** レジストリ キー パスの作業配列長 [UTF-16] です。 */
    #define EVENTLOG_KEY_PATH_CHARS 512U

/**
 *  @brief  EventLog シンク ハンドル構造体 (内部定義) です。
 */
struct cplat_eventlog_sink
{
    /** RegisterEventSourceW が返すイベント ソース ハンドル。 */
    HANDLE source;
};

static void map_level(const int level, const int has_file_identifier, const int has_instance_identifier, WORD *type,
                      WORD *category, DWORD *event_id);
static void copy_wstr(wchar_t *dst, const size_t dst_count, const wchar_t *src);
static void init_executable_path_cache(void);
static const wchar_t *cached_executable_path(void);
static void init_user_sid_cache(void);
static PSID cached_user_sid(void);
static int format_identifier_string(const int64_t identifier, char *buf, const size_t buf_size);
static int build_source_key_path(const char *source_name, wchar_t *key_path, const size_t key_count);
static int registry_status(const LONG rc);

static cplat_once_flag s_executable_path_once = {0};
static wchar_t s_executable_path[PLATFORM_PATH_MAX] = L"";

static cplat_once_flag s_user_sid_once = {0};

/*
 * TOKEN_USER 本体に続けて可変長 SID を格納するため、最大 SID サイズ分を確保する。
 * union により TOKEN_USER の境界整合を保証する。
 */
static union
{
    TOKEN_USER token_user;
    unsigned char bytes[sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE];
} s_user_sid_buffer;
static PSID s_user_sid = NULL;

/* Doxygen コメントは、ヘッダーに記載 */

cplat_eventlog_sink *cplat_eventlog_sink_create(const char *source_name)
{
    cplat_eventlog_sink *handle;
    wchar_t *wsource;
    HANDLE source;

    if (source_name == NULL)
    {
        return NULL;
    }

    if (cplat_utf8_to_wstr_alloc(&wsource, source_name) != CPLAT_OK)
    {
        return NULL;
    }

    source = RegisterEventSourceW(NULL, wsource);
    cplat_free(wsource);

    if (source == NULL)
    {
        return NULL;
    }

    handle = (cplat_eventlog_sink *)cplat_malloc(sizeof(cplat_eventlog_sink));
    if (handle == NULL)
    {
        (void)DeregisterEventSource(source); /* 確保の失敗で NULL を返す経路の後始末のため、戻り値は明示的に破棄する */
        return NULL;
    }

    handle->source = source;

    return handle;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_eventlog_sink_write(cplat_eventlog_sink *handle, const int level, const int64_t file_identifier,
                              const char *instance_name, const int64_t instance_identifier, const char *message)
{
    WORD type;
    WORD category;
    DWORD event_id;
    wchar_t *wmsg = NULL;
    wchar_t *wfile_id = NULL;
    wchar_t *winstance = NULL;
    wchar_t *winstance_id = NULL;
    LPCWSTR strings[EVENTLOG_STRING_COUNT];
    char file_id[EVENTLOG_IDENTIFIER_TEXT_SIZE];
    char instance_id[EVENTLOG_IDENTIFIER_TEXT_SIZE];
    int has_file_identifier;
    int has_instance_identifier;
    BOOL ok;
    int conversion_result;

    if (handle == NULL || handle->source == NULL || message == NULL)
    {
        return CPLAT_OK;
    }

    has_file_identifier = format_identifier_string(file_identifier, file_id, sizeof(file_id));
    has_instance_identifier = format_identifier_string(instance_identifier, instance_id, sizeof(instance_id));
    map_level(level, has_file_identifier, has_instance_identifier, &type, &category, &event_id);

    conversion_result = cplat_utf8_to_wstr_alloc(&wmsg, message);
    if (conversion_result == CPLAT_OK)
    {
        conversion_result = cplat_utf8_to_wstr_alloc(&wfile_id, file_id);
    }

    if (conversion_result == CPLAT_OK && instance_name != NULL)
    {
        conversion_result = cplat_utf8_to_wstr_alloc(&winstance, instance_name);
    }
    else if (conversion_result == CPLAT_OK)
    {
        conversion_result = cplat_utf8_to_wstr_alloc(&winstance, "");
    }

    if (conversion_result == CPLAT_OK)
    {
        conversion_result = cplat_utf8_to_wstr_alloc(&winstance_id, instance_id);
    }

    if (conversion_result != CPLAT_OK)
    {
        cplat_free(wmsg);
        cplat_free(wfile_id);
        cplat_free(winstance);
        cplat_free(winstance_id);
        return conversion_result;
    }

    strings[0] = wmsg;
    strings[1] = cached_executable_path();
    strings[2] = wfile_id;
    strings[3] = winstance;
    strings[4] = winstance_id;
    ok = ReportEventW(handle->source, type, category, event_id, cached_user_sid(), EVENTLOG_STRING_COUNT, 0, strings,
                      NULL);
    cplat_free(wmsg);
    cplat_free(wfile_id);
    cplat_free(winstance);
    cplat_free(winstance_id);

    if (!ok)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_eventlog_sink_dispose(cplat_eventlog_sink *handle)
{
    if (handle == NULL)
    {
        return;
    }

    if (handle->source != NULL)
    {
        /* 破棄の後に失敗を受けても、呼び出し元が取れる行動がないため、戻り値は明示的に破棄する。 */
        (void)DeregisterEventSource(handle->source);
    }

    cplat_free(handle);
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_eventlog_sink_dispose_on_shutdown(cplat_eventlog_sink *handle, const cplat_shutdown_event *event)
{
    if (handle == NULL)
    {
        return;
    }

    if (handle->source != NULL && (event == NULL || event->reason == CPLAT_SHUTDOWN_REASON_NORMAL_EXIT))
    {
        /* プロセスの終了時の破棄であり、失敗を受けて取れる行動がないため、戻り値は明示的に破棄する。 */
        (void)DeregisterEventSource(handle->source);
    }

    cplat_free(handle);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_eventlog_register_source(const char *source_name, const char *message_file_path)
{
    wchar_t key_path[EVENTLOG_KEY_PATH_CHARS];
    HKEY hkey;
    LONG rc;
    DWORD disposition;
    DWORD types_supported;
    DWORD category_count;

    if (source_name == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (build_source_key_path(source_name, key_path, sizeof(key_path) / sizeof(key_path[0])) != 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    rc = RegCreateKeyExW(HKEY_LOCAL_MACHINE, key_path, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &hkey,
                         &disposition);
    if (rc != ERROR_SUCCESS)
    {
        return registry_status(rc);
    }

    types_supported = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;
    rc = RegSetValueExW(hkey, L"TypesSupported", 0, REG_DWORD, (const BYTE *)&types_supported,
                        (DWORD)sizeof(types_supported));
    if (rc == ERROR_SUCCESS)
    {
        category_count = CPLAT_EVENTLOG_LEVEL_COUNT;
        rc = RegSetValueExW(hkey, L"CategoryCount", 0, REG_DWORD, (const BYTE *)&category_count,
                            (DWORD)sizeof(category_count));
    }

    /*
     * メッセージ リソース (MESSAGETABLE) を持つファイルの絶対パスが渡された場合は、
     * EventMessageFile / CategoryMessageFile に登録して Event Viewer に解決させる。
     */
    if (rc == ERROR_SUCCESS && message_file_path != NULL)
    {
        wchar_t *wpath;
        int conversion_result;

        conversion_result = cplat_utf8_to_wstr_alloc(&wpath, message_file_path);
        if (conversion_result != CPLAT_OK)
        {
            cplat_internal_win32_set_last_error_from_result(conversion_result);
            rc = (LONG)GetLastError();
        }
        else
        {
            DWORD bytes;
            wchar_t *p;

            /*
             * イベント表示側 (Event Viewer 等) が LoadLibraryEx() でメッセージ ファイルを読み込む。
             * LoadLibraryExW() のパス仕様に合わせ、区切り文字の '/' を '\' へ変換する。
             * see: https://learn.microsoft.com/en-us/windows/win32/eventlog/message-files
             * see: https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryexw
             */
            for (p = wpath; *p != L'\0'; p++)
            {
                if (*p == L'/')
                {
                    *p = L'\\';
                }
            }

            bytes = (DWORD)((wcslen(wpath) + 1) * sizeof(wchar_t));
            rc = RegSetValueExW(hkey, L"EventMessageFile", 0, REG_SZ, (const BYTE *)wpath, bytes);
            if (rc == ERROR_SUCCESS)
            {
                rc = RegSetValueExW(hkey, L"CategoryMessageFile", 0, REG_SZ, (const BYTE *)wpath, bytes);
            }

            cplat_free(wpath);
        }
    }

    /* 登録の成否は値の書き込みの結果 rc で確定しており、閉じる失敗で結果が変わらないため、戻り値は明示的に破棄する。 */
    (void)RegCloseKey(hkey);

    return registry_status(rc);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_eventlog_unregister_source(const char *source_name)
{
    wchar_t key_path[EVENTLOG_KEY_PATH_CHARS];
    LONG rc;

    if (source_name == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (build_source_key_path(source_name, key_path, sizeof(key_path) / sizeof(key_path[0])) != 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    rc = RegDeleteKeyW(HKEY_LOCAL_MACHINE, key_path);
    if (rc == ERROR_FILE_NOT_FOUND)
    {
        return CPLAT_OK;
    }

    return registry_status(rc);
}

/**
 *  @brief          トレース レベルをイベント タイプ・カテゴリ・イベント ID に写像します。
 *  @param[in]      level     トレース レベル (0=CRITICAL 〜 5=DEBUG)。範囲外は Information 扱い。
 *  @param[out]     type      EventLog イベント タイプ (EVENTLOG_*_TYPE)。
 *  @param[out]     category  イベント カテゴリ (レベル毎に分離)。
 *  @param[in]      has_file_identifier      ファイル識別子がある場合は非 0。
 *  @param[in]      has_instance_identifier  インスタンス識別子がある場合は非 0。
 *  @param[out]     event_id                 イベント ID (レベル・表示形式毎に分離)。
 *
 *  分析性を高めるため、同一ソースでもレベル毎にカテゴリとイベント ID を
 *  分けて割り当てます。\n
 *  @p level が 0 から 5 の範囲にない場合は、イベント タイプを Information、カテゴリを 0、
 *  イベント ID を 0 にします。\n
 *  メッセージ定義 (eventlog_messages.mc) に、ID が 0 のカテゴリとイベントはありません。\n
 *  強制出力のレベルは、呼び出し元の tracer.c が通常のレベルへ変換してから渡します。
 */
static void map_level(const int level, const int has_file_identifier, const int has_instance_identifier, WORD *type,
                      WORD *category, DWORD *event_id)
{
    DWORD base_id;

    switch (level)
    {
    case CPLAT_TRACE_LEVEL_CRITICAL:
        *type = EVENTLOG_ERROR_TYPE;
        break;
    case CPLAT_TRACE_LEVEL_ERROR:
        *type = EVENTLOG_ERROR_TYPE;
        break;
    case CPLAT_TRACE_LEVEL_WARNING:
        *type = EVENTLOG_WARNING_TYPE;
        break;
    case CPLAT_TRACE_LEVEL_INFO:
        *type = EVENTLOG_INFORMATION_TYPE;
        break;
    case CPLAT_TRACE_LEVEL_VERBOSE:
        *type = EVENTLOG_INFORMATION_TYPE;
        break;
    case CPLAT_TRACE_LEVEL_DEBUG:
        *type = EVENTLOG_INFORMATION_TYPE;
        break;
    default:
        *type = EVENTLOG_INFORMATION_TYPE;
        break;
    }

    if (level >= 0 && level < (int)CPLAT_EVENTLOG_LEVEL_COUNT)
    {
        *category = (WORD)(level + 1);

        if (has_file_identifier && has_instance_identifier)
        {
            base_id = EVENTLOG_EVENT_ID_BOTH_IDENTIFIERS;
        }
        else if (has_file_identifier)
        {
            base_id = EVENTLOG_EVENT_ID_FILE_IDENTIFIER;
        }
        else if (has_instance_identifier)
        {
            base_id = EVENTLOG_EVENT_ID_INSTANCE_IDENTIFIER;
        }
        else
        {
            base_id = EVENTLOG_EVENT_ID_NO_IDENTIFIER;
        }

        *event_id = base_id + (DWORD)level;
    }
    else
    {
        *category = 0;
        *event_id = 0;
    }
}

/**
 *  @brief          キャッシュ用のワイド文字列へ安全にコピーします。
 *  @param[out]     dst        コピー先。
 *  @param[in]      dst_count  コピー先の要素数。
 *  @param[in]      src        コピー元。
 */
static void copy_wstr(wchar_t *dst, const size_t dst_count, const wchar_t *src)
{
    size_t i;

    if (dst == NULL || dst_count == 0)
    {
        return;
    }

    if (src == NULL)
    {
        dst[0] = L'\0';
        return;
    }

    i = 0;
    while (i + 1 < dst_count && src[i] != L'\0')
    {
        dst[i] = src[i];
        i++;
    }

    dst[i] = L'\0';
}

/**
 *  @brief          実行ファイル パスを初回だけ解決し、EventLog 用の UTF-16 文字列としてキャッシュします。
 *
 *  パス取得または変換に失敗した場合は空文字列をキャッシュし、以後は再試行しません。
 */
static void init_executable_path_cache(void)
{
    char path[PLATFORM_PATH_UTF8_MAX];
    char *p;
    wchar_t *wpath;

    s_executable_path[0] = L'\0';

    if (cplat_process_get_executable_path(path, sizeof(path)) != CPLAT_OK)
    {
        return;
    }

    /*
     * イベント表示側 (Event Viewer 等) は登録されたパスを LoadLibraryEx() で読み込む。
     * LoadLibraryExW() はパス区切りとして '\' の使用を想定するため、cplat 内部共通の '/' を '\' へ戻す。
     * see: https://learn.microsoft.com/en-us/windows/win32/eventlog/message-files
     * see: https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryexw
     */
    for (p = path; *p != '\0'; p++)
    {
        if (*p == '/')
        {
            *p = '\\';
        }
    }

    if (cplat_utf8_to_wstr_alloc(&wpath, path) != CPLAT_OK)
    {
        return;
    }

    copy_wstr(s_executable_path, sizeof(s_executable_path) / sizeof(s_executable_path[0]), wpath);
    cplat_free(wpath);
}

/**
 *  @brief          キャッシュ済みの実行ファイル パスを取得します。
 *  @return         実行ファイル パス。取得失敗時は空文字列。
 */
static const wchar_t *cached_executable_path(void)
{
    cplat_call_once(&s_executable_path_once, init_executable_path_cache);
    return s_executable_path;
}

/**
 *  @brief          現在のプロセスのユーザー SID を初回だけ取得し、キャッシュします。
 *
 *  プロセス トークンから TokenUser 情報を取得し、SID をキャッシュ バッファーへ
 *  格納します。取得に失敗した場合は s_user_sid を NULL のままとし、以後は再試行しません。
 */
static void init_user_sid_cache(void)
{
    HANDLE token = NULL;
    DWORD needed = 0;

    s_user_sid = NULL;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
    {
        return;
    }

    if (GetTokenInformation(token, TokenUser, &s_user_sid_buffer, (DWORD)sizeof(s_user_sid_buffer), &needed))
    {
        s_user_sid = s_user_sid_buffer.token_user.User.Sid;
    }

    (void)CloseHandle(token); /* SID の取得を終えた後の解放で結果が変わらないため、戻り値は明示的に破棄する */
}

/**
 *  @brief          キャッシュ済みのプロセス ユーザー SID を取得します。
 *  @return         ユーザー SID。取得失敗時は NULL。
 *
 *  NULL を ReportEventW の lpUserSid に渡すと、Event Viewer の「ユーザー」列は
 *  未設定 (N/A) となり、従来動作と一致します。
 */
static PSID cached_user_sid(void)
{
    cplat_call_once(&s_user_sid_once, init_user_sid_cache);
    return s_user_sid;
}

/**
 *  @brief          識別子を EventData 用文字列に変換します。
 *  @param[in]      identifier  変換対象の識別子。
 *  @param[out]     buf         変換先。
 *  @param[in]      buf_size    変換先のバイト数。
 *  @return         0 以外の場合は識別子あり、0 の場合は識別子なし。
 */
static int format_identifier_string(const int64_t identifier, char *buf, const size_t buf_size)
{
    if (buf == NULL || buf_size == 0)
    {
        return 0;
    }

    if (identifier == 0)
    {
        buf[0] = '\0';
        return 0;
    }

    if (cplat_snprintf(buf, buf_size, "%" PRId64, identifier) != CPLAT_OK)
    {
        return 0;
    }

    return 1;
}

/**
 *  @brief          イベント ソース名からレジストリ キー パスを組み立てます。
 *  @param[in]      source_name  イベント ソース名 (UTF-8)。
 *  @param[out]     key_path     書き込み先のワイド文字列バッファー。
 *  @param[in]      key_count    key_path の要素数。
 *  @return         成功 0 / 失敗 -1。
 */
static int build_source_key_path(const char *source_name, wchar_t *key_path, const size_t key_count)
{
    wchar_t *wsource;
    int written;

    if (cplat_utf8_to_wstr_alloc(&wsource, source_name) != CPLAT_OK)
    {
        return -1;
    }

    written = swprintf(key_path, key_count, L"%ls%ls", EVENTLOG_KEY_PREFIX, wsource);
    cplat_free(wsource);

    if (written < 0)
    {
        return -1;
    }

    return 0;
}

/**
 *  @brief          RegCreateKeyExW / RegDeleteKeyW の戻り値をステータス コードに変換します。
 *  @param[in]      rc  Win32 レジストリ API の戻り値。
 *  @return         CPLAT_OK / CPLAT_ERR_PERMISSION_DENIED / CPLAT_ERR_UNKNOWN。
 */
static int registry_status(const LONG rc)
{
    if (rc == ERROR_SUCCESS)
    {
        return CPLAT_OK;
    }

    if (rc == ERROR_ACCESS_DENIED)
    {
        return CPLAT_ERR_PERMISSION_DENIED;
    }

    return CPLAT_ERR_UNKNOWN;
}

#endif /* PLATFORM_WINDOWS */
