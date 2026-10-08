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
    #include <cplat/trace/etw.h>
    #include <cplat/trace/tracer.h>

    #define ETW_VIEWER_WAIT_MS     200
    #define ETW_VIEWER_DEFAULT_TAG CPLAT_TRACER_DEFAULT_PROVIDER_NAME

    #define EXIT_ACCESS_DENIED 2

static volatile sig_atomic_t g_stop_requested = 0;

static void etw_viewer_shutdown_request_callback(const cplat_shutdown_event *event, void *context)
{
    (void)event;
    (void)context;
    g_stop_requested = 1;
}

static int parse_process_id_arg(const char *text, uint32_t *process_id_out)
{
    char *endptr;
    unsigned long value;

    if (text == NULL || process_id_out == NULL || text[0] == '\0')
    {
        return -1;
    }

    errno = 0;
    value = strtoul(text, &endptr, 10);
    /* プロセス ID は Windows DWORD のため、コーディング規範の例外として UINT32_MAX 上限を維持する。 */
    if (errno != 0 || endptr == text || *endptr != '\0' || value > UINT32_MAX)
    {
        return -1;
    }

    *process_id_out = (uint32_t)value;
    return 0;
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
    case 1:
        return "CRITICAL";
    case 2:
        return "ERROR";
    case 3:
        return "WARNING";
    case 4:
        return "INFO";
    case 5:
        return "VERBOSE";
    default:
        return "UNKNOWN";
    }
}

static char etw_viewer_priority_letter(int level)
{
    switch (level)
    {
    case 1:
        return 'C';
    case 2:
        return 'E';
    case 3:
        return 'W';
    case 4:
        return 'I';
    case 5:
        return 'V';
    default:
        return 'U';
    }
}

int etw_viewer_format_timestamp_utc(int64_t timestamp_100ns, char *buffer, size_t buffer_size)
{
    static const int64_t filetime_unix_epoch_100ns = 116444736000000000LL;
    int64_t unix_100ns;
    cplat_timespec ts;

    if (buffer == NULL || buffer_size == 0U)
    {
        return -1;
    }
    if (timestamp_100ns < filetime_unix_epoch_100ns)
    {
        return -1;
    }

    unix_100ns = timestamp_100ns - filetime_unix_epoch_100ns;
    ts.tv_sec = (time_t)(unix_100ns / 10000000LL);
    ts.tv_nsec = (unix_100ns % 10000000LL) * 100LL;
    return cplat_clock_format_realtime_iso8601_local(buffer, buffer_size, &ts);
}

void etw_viewer_handle_event(const cplat_etw_event *event, void *context)
{
    char timestamp_text[64];
    const char *timestamp_value;
    const char *tag;
    const char *message;
    const etw_viewer_context *viewer_context = (const etw_viewer_context *)context;

    if (event == NULL)
    {
        return;
    }
    if (event->event_name == NULL || strcmp(event->event_name, "Trace") != 0)
    {
        return;
    }
    if (event->message == NULL)
    {
        return;
    }
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

    printf("%s <%c>%s[%" PRIu32 "]: %s\n", timestamp_value, etw_viewer_priority_letter(event->level), tag,
           event->process_id, message);
    fflush(stdout);
}

static void print_access_error(void)
{
    fprintf(stderr, "ETW session の開始権限がありません。Administrators または "
                    "\"Performance Log Users\" が必要です。\n"
                    "対処方法: net localgroup \"Performance Log Users\" %%USERNAME%% /add\n"
                    "          この操作後にサインアウト/サインインが必要です。\n");
    fflush(stderr);
}

static void print_start_error(int ret, const char *session_name)
{
    if (ret == CPLAT_ERR_PERMISSION_DENIED)
    {
        print_access_error();
    }
    else if (ret == CPLAT_ERR_INVALID_ARGUMENT)
    {
        fprintf(stderr, "ETW session の開始に失敗しました。内部パラメーターが不正です。\n");
    }
    else
    {
        fprintf(stderr, "ETW session の開始に失敗しました。session=\"%s\" provider=\"%s\"\n", session_name,
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
    cplat_argparser_register_flag("-h", "--help", "ヘルプを表示します。", &options.need_help);
    cplat_argparser_register_option_string(NULL, "--pid", "process-id", "表示するプロセス ID。", 0, &pid_value);

    if (cplat_argparser_get_register_error_count() > 0)
    {
        cplat_argparser_print_register_error_messages(stderr);
        cplat_argparser_print_usage(stderr);
        return EXIT_FAILURE;
    }

    int parse_result = cplat_argparser_parse();

    if (options.need_help != 0)
    {
        cplat_argparser_print_usage(stdout);
        return EXIT_SUCCESS;
    }

    if (parse_result != CPLAT_OK)
    {
        cplat_argparser_print_error_messages(stderr);
        cplat_argparser_print_usage(stderr);
        return EXIT_FAILURE;
    }

    if (pid_value != NULL)
    {
        if (parse_process_id_arg(pid_value, &options.process_id_filter) != 0)
        {
            cplat_argparser_print_error_messages(stderr);
            cplat_argparser_print_usage(stderr);
            return EXIT_FAILURE;
        }
        options.has_process_id_filter = 1;
    }

    if (etw_viewer_build_default_session_name((unsigned long)GetCurrentProcessId(), session_name,
                                              sizeof(session_name)) != 0)
    {
        fprintf(stderr, "既定 session 名の生成に失敗しました。\n");
        return EXIT_FAILURE;
    }

    viewer_context.process_id_filter = options.process_id_filter;
    viewer_context.has_process_id_filter = options.has_process_id_filter;

    ret = cplat_etw_session_check_access();
    if (ret == CPLAT_ERR_PERMISSION_DENIED)
    {
        print_access_error();
        return EXIT_ACCESS_DENIED;
    }
    if (ret != CPLAT_OK)
    {
        fprintf(stderr, "ETW session の権限確認に失敗しました。\n");
        return EXIT_FAILURE;
    }

    if (cplat_shutdown_request_register(etw_viewer_shutdown_request_callback, NULL) != CPLAT_OK)
    {
        fprintf(stderr, "終了要求 callback の登録に失敗しました。\n");
        return EXIT_FAILURE;
    }

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

    printf("session=%s provider=%s\n", session_name, CPLAT_TRACER_DEFAULT_PROVIDER_GUID_STR);
    if (options.has_process_id_filter)
    {
        printf("filter.pid=%" PRIu32 "\n", options.process_id_filter);
    }
    printf("Ctrl+C で終了します。\n");
    fflush(stdout);

    while (!g_stop_requested)
    {
        cplat_sleep_ms(ETW_VIEWER_WAIT_MS);
    }

    cplat_etw_session_stop(session);

    return EXIT_SUCCESS;
}

#elif defined(PLATFORM_LINUX)

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    cplat_console_init();

    fprintf(stderr, "ETW viewer is supported only on Windows.\n");

    return EXIT_FAILURE;
}

#endif /* PLATFORM_ */
