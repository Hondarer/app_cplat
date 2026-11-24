/**
 *******************************************************************************
 *  @file           etw-viewer.c
 *  @brief          cplat ETW provider のイベントをリアルタイムに表示するコマンドを実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/05/01
 *  @version        1.0.0
 *
 *  Windows 上で cplat の ETW provider を購読し、受信イベントを stdout に表示します。
 *  既定では cplat.tracer の標準 provider GUID を購読し、Ctrl+C で終了します。
 *
 *  終了コードは次のとおりです。
 *
 *  - 0: 正常終了。
 *  - 1: 引数の誤りや Windows 以外での実行など、下記以外の失敗。
 *  - 2: ETW session の開始権限がない。必要なグループと対処方法を標準エラー出力へ書き出します。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include "etw-viewer.h"

#include <cplat/argparser/argparser.h>
#include <cplat/base/platform.h>
#include <cplat/base/result.h>
#include <cplat/clock/clock.h>
#include <cplat/console/console.h>
#include <cplat/crt/stdio.h>
#include <cplat/crt/stdlib.h>
#include <cplat/crt/string.h>
#include <cplat/runtime/shutdown.h>
#include <cplat/sync/sync.h>
#include <signal.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if defined(PLATFORM_WINDOWS)

    #include <cplat/base/windows_sdk.h>
    #include <evntrace.h>
    #include <cplat/trace/etw.h>
    #include <cplat/trace/tracer.h>

/** @brief 終了要求を確認する待機間隔 [ms] です。 */
    #define ETW_VIEWER_WAIT_MS     200
    #define ETW_VIEWER_DEFAULT_TAG CPLAT_TRACER_DEFAULT_PROVIDER_NAME

/** @brief ETW セッションの開始権限がない場合の終了コードです。 */
    #define EXIT_ACCESS_DENIED 2

/** FILETIME の起点から Unix epoch までの 100 [ns] 単位数です。 */
/* see: https://learn.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-filetime */
    #define ETW_VIEWER_FILETIME_UNIX_EPOCH_100NS 116444736000000000LL

    /** 1 [s] あたりの 100 [ns] 単位数です。 */
    #define ETW_VIEWER_100NS_PER_SEC 10000000LL

    /** 100 [ns] 単位から [ns] への換算係数です。 */
    #define ETW_VIEWER_NS_PER_100NS 100LL

    /** ISO 8601 時刻文字列を格納する配列の大きさ [byte] (終端を含む) です。 */
    #define ETW_VIEWER_TIMESTAMP_TEXT_SIZE 64U

static volatile sig_atomic_t g_stop_requested = 0;

static void etw_viewer_shutdown_request_callback(const cplat_shutdown_event *event, void *context)
{
    (void)event;
    (void)context;
    g_stop_requested = 1;
}

static int parse_process_id_arg(const char *text, uint32_t *process_id_out)
{
    uint64_t value;
    int parse_result;

    if (text == NULL || process_id_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (text[0] == '\0')
    {
        return CPLAT_ERR_INVALID_INTEGER;
    }

    /* プロセス ID は Windows DWORD のため、コーディング規範の例外として UINT32_MAX 上限を維持する。 */
    parse_result = cplat_parse_uint64(&value, text, 10);
    if (parse_result != CPLAT_OK)
    {
        return parse_result;
    }

    if (value > UINT32_MAX)
    {
        return CPLAT_ERR_OUT_OF_RANGE;
    }

    *process_id_out = (uint32_t)value;

    return CPLAT_OK;
}

void etw_viewer_options_init(etw_viewer_options *options)
{
    if (options == NULL)
    {
        return;
    }

    options->process_id_filter = 0U;
    options->has_process_id_filter = 0;
    options->need_help = 0;
}

int etw_viewer_build_default_session_name(unsigned long process_id, char *buffer, size_t buffer_size)
{
    if (buffer == NULL || buffer_size == 0U)
    {
        return -1;
    }

    if (cplat_snprintf(buffer, buffer_size, "etw-viewer_%lu", process_id) != CPLAT_OK)
    {
        return -1;
    }

    return 0;
}

const char *etw_viewer_level_name(int level)
{
    switch (level)
    {
    case TRACE_LEVEL_CRITICAL:
        return "CRITICAL";
    case TRACE_LEVEL_ERROR:
        return "ERROR";
    case TRACE_LEVEL_WARNING:
        return "WARNING";
    case TRACE_LEVEL_INFORMATION:
        return "INFO";
    case TRACE_LEVEL_VERBOSE:
        return "VERBOSE";
    default:
        return "UNKNOWN";
    }
}

/*
 * ETW イベント レベルを優先度文字 (C, E, W, I, V, U) へ変換する。
 */
static char etw_viewer_priority_letter(int level)
{
    switch (level)
    {
    case TRACE_LEVEL_CRITICAL:
        return 'C';
    case TRACE_LEVEL_ERROR:
        return 'E';
    case TRACE_LEVEL_WARNING:
        return 'W';
    case TRACE_LEVEL_INFORMATION:
        return 'I';
    case TRACE_LEVEL_VERBOSE:
        return 'V';
    default:
        return 'U';
    }
}

int etw_viewer_format_timestamp_utc(int64_t timestamp_100ns, char *buffer, size_t buffer_size)
{
    int64_t unix_100ns;
    cplat_timespec ts;

    if (buffer == NULL || buffer_size == 0U)
    {
        return -1;
    }

    if (timestamp_100ns < ETW_VIEWER_FILETIME_UNIX_EPOCH_100NS)
    {
        return -1;
    }

    unix_100ns = timestamp_100ns - ETW_VIEWER_FILETIME_UNIX_EPOCH_100NS;
    ts.tv_sec = (time_t)(unix_100ns / ETW_VIEWER_100NS_PER_SEC);
    ts.tv_nsec = (unix_100ns % ETW_VIEWER_100NS_PER_SEC) * ETW_VIEWER_NS_PER_100NS;

    return cplat_clock_format_realtime_iso8601_local(buffer, buffer_size, &ts);
}

void etw_viewer_handle_event(const cplat_etw_event *event, void *context)
{
    char timestamp_text[ETW_VIEWER_TIMESTAMP_TEXT_SIZE];
    const char *timestamp_value;
    const char *tag;
    const char *message;
    const etw_viewer_context *viewer_context = (const etw_viewer_context *)context;

    if (event == NULL)
    {
        return;
    }

    /* cplat の Trace イベント以外のイベントは無視する。 */
    if (event->event_name == NULL || strcmp(event->event_name, "Trace") != 0)
    {
        return;
    }

    if (event->message == NULL)
    {
        return;
    }

    /* プロセス ID フィルターが指定されている場合、一致しないプロセスのイベントを除外する。 */
    if (viewer_context != NULL && viewer_context->has_process_id_filter &&
        event->process_id != viewer_context->process_id_filter)
    {
        return;
    }

    if (event->service != NULL && event->service[0] != '\0')
    {
        tag = event->service;
    }
    else
    {
        tag = ETW_VIEWER_DEFAULT_TAG;
    }

    message = event->message;

    if (etw_viewer_format_timestamp_utc(event->timestamp_100ns, timestamp_text, sizeof(timestamp_text)) != 0)
    {
        timestamp_value = "(invalid-timestamp)";
    }
    else
    {
        timestamp_value = timestamp_text;
    }

    /*
     * イベントの表示の失敗は標準出力のストリームに残り、main が終了の直前にまとめて検査するため、
     * この関数の printf と fflush の戻り値は明示的に破棄する。
     */
    (void)printf("%s <%c>%s[%" PRIu32 "]: %s\n", timestamp_value, etw_viewer_priority_letter(event->level), tag,
                 event->process_id, message);
    (void)fflush(stdout);
}

static void print_access_error(void)
{
    /* 診断の出力先がほかにないため、この関数の fprintf と fflush の戻り値は明示的に破棄する。 */
    (void)fprintf(stderr, "ETW session の開始権限がありません。Administrators または "
                          "\"Performance Log Users\" が必要です。\n"
                          "対処方法: net localgroup \"Performance Log Users\" %%USERNAME%% /add\n"
                          "          この操作後にサインアウト/サインインが必要です。\n");
    (void)fflush(stderr);
}

static void print_start_error(int ret, const char *session_name)
{
    /* 診断の出力先がほかにないため、この関数の fprintf の戻り値は明示的に破棄する。 */
    if (ret == CPLAT_ERR_PERMISSION_DENIED)
    {
        print_access_error();
    }
    else if (ret == CPLAT_ERR_INVALID_ARGUMENT)
    {
        (void)fprintf(stderr, "ETW session の開始に失敗しました。内部パラメーターが不正です。\n");
    }
    else
    {
        (void)fprintf(stderr, "ETW session の開始に失敗しました。session=\"%s\" provider=\"%s\"\n", session_name,
                      CPLAT_TRACER_DEFAULT_PROVIDER_GUID_STR);
    }
}

int main(int argc, char *argv[])
{
    etw_viewer_options options;
    etw_viewer_context viewer_context;
    cplat_etw_session *session;
    int ret;
    char session_name[ETW_VIEWER_SESSION_NAME_MAX];
    const char *pid_value = NULL;

    cplat_console_init();
    g_stop_requested = 0;

    etw_viewer_options_init(&options);

    cplat_argparser_init(argc, argv, "ETW の cplat.tracer を表示します。");
    /*
     * 登録の失敗はパーサーに記録され、直後の cplat_argparser_get_register_error_count() でまとめて検査するため、
     * 登録の関数の戻り値は明示的に破棄する。
     */
    (void)cplat_argparser_register_flag("-h", "--help", "ヘルプを表示します。", &options.need_help);
    (void)cplat_argparser_register_option_string(NULL, "--pid", "process-id", "表示するプロセス ID。", 0,
                                                 &pid_value);

    if (cplat_argparser_get_register_error_count() > 0)
    {
        /* 終了コードはすでに失敗に決まっているため、この 2 つの表示の関数の戻り値は明示的に破棄する。 */
        (void)cplat_argparser_print_register_error_messages(stderr);
        (void)cplat_argparser_print_usage(stderr);
        return EXIT_FAILURE;
    }

    int parse_result = cplat_argparser_parse();

    if (options.need_help != 0)
    {
        if (cplat_argparser_print_usage(stdout) != CPLAT_OK || fflush(stdout) != 0 || ferror(stdout) != 0)
        {
            (void)fprintf(stderr, "標準出力へ書き込めませんでした。\n"); /* 診断の出力先がほかにないため、戻り値は明示的に破棄する */
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }

    if (parse_result != CPLAT_OK)
    {
        /* 終了コードはすでに失敗に決まっているため、この 2 つの表示の関数の戻り値は明示的に破棄する。 */
        (void)cplat_argparser_print_error_messages(stderr);
        (void)cplat_argparser_print_usage(stderr);
        return EXIT_FAILURE;
    }

    if (pid_value != NULL)
    {
        int pid_result = parse_process_id_arg(pid_value, &options.process_id_filter);
        if (pid_result != CPLAT_OK)
        {
            if (cplat_argparser_set_validation_error(pid_result, "--pid", NULL) == CPLAT_ERR_INVALID_ARGUMENT)
            {
                /* 記録失敗時も終了コードはすでに失敗に決まっており、エラー表示を続けるため戻り値は明示的に破棄する。 */
                (void)cplat_argparser_set_validation_error(CPLAT_ERR_INVALID_INTEGER, "--pid", NULL);
            }
            /* 終了コードはすでに失敗に決まっているため、この 2 つの表示の関数の戻り値は明示的に破棄する。 */
            (void)cplat_argparser_print_error_messages(stderr);
            (void)cplat_argparser_print_usage(stderr);
            return EXIT_FAILURE;
        }

        options.has_process_id_filter = 1;
    }

    if (etw_viewer_build_default_session_name((unsigned long)GetCurrentProcessId(), session_name,
                                              sizeof(session_name)) != 0)
    {
        /* 診断の出力先がほかにないため、戻り値は明示的に破棄する。 */
        (void)fprintf(stderr, "既定 session 名の生成に失敗しました。\n");
        return EXIT_FAILURE;
    }

    viewer_context.process_id_filter = options.process_id_filter;
    viewer_context.has_process_id_filter = options.has_process_id_filter;

    /* ETW セッションの開始に必要な権限があるか確認する。 */
    ret = cplat_etw_session_check_access();
    if (ret == CPLAT_ERR_PERMISSION_DENIED)
    {
        print_access_error();
        return EXIT_ACCESS_DENIED;
    }

    if (ret != CPLAT_OK)
    {
        /* 診断の出力先がほかにないため、戻り値は明示的に破棄する。 */
        (void)fprintf(stderr, "ETW session の権限確認に失敗しました。\n");
        return EXIT_FAILURE;
    }

    /* Ctrl+C などの終了要求を受け取るコールバックを登録する。 */
    if (cplat_shutdown_request_register(etw_viewer_shutdown_request_callback, NULL) != CPLAT_OK)
    {
        /* 診断の出力先がほかにないため、戻り値は明示的に破棄する。 */
        (void)fprintf(stderr, "終了要求 callback の登録に失敗しました。\n");
        return EXIT_FAILURE;
    }

    /* ETW セッションを開始し、イベントの受信を開始する。 */
    ret = cplat_etw_session_start(session_name, CPLAT_TRACER_DEFAULT_PROVIDER_GUID_STR, etw_viewer_handle_event,
                                  &viewer_context, &session);
    if (ret != CPLAT_OK)
    {
        int exit_code = EXIT_FAILURE;

        if (ret == CPLAT_ERR_PERMISSION_DENIED)
        {
            exit_code = EXIT_ACCESS_DENIED;
        }

        print_start_error(ret, session_name);
        return exit_code;
    }

    /*
     * 開始時の案内の出力の失敗は標準出力のストリームに残り、終了の直前にまとめて検査するため、
     * この 3 つの printf と fflush の戻り値は明示的に破棄する。
     */
    (void)printf("session=%s provider=%s\n", session_name, CPLAT_TRACER_DEFAULT_PROVIDER_GUID_STR);

    if (options.has_process_id_filter)
    {
        (void)printf("filter.pid=%" PRIu32 "\n", options.process_id_filter);
    }

    (void)printf("Ctrl+C で終了します。\n");
    (void)fflush(stdout);

    /* 終了要求を受け取るまで待機ループを継続する。 */
    while (!g_stop_requested)
    {
        cplat_sleep_ms(ETW_VIEWER_WAIT_MS);
    }

    cplat_etw_session_stop(session);

    if (fflush(stdout) != 0 || ferror(stdout) != 0)
    {
        (void)fprintf(stderr, "標準出力へ書き込めませんでした。\n"); /* 診断の出力先がほかにないため、戻り値は明示的に破棄する */
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

#elif defined(PLATFORM_LINUX)

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    cplat_console_init();

    /* 診断の出力先がほかにないため、戻り値は明示的に破棄する。 */
    (void)fprintf(stderr, "ETW viewer is supported only on Windows.\n");

    return EXIT_FAILURE;
}

#endif /* PLATFORM_ */
