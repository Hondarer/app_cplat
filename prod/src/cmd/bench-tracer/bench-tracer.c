/**
 *******************************************************************************
 *  @file           bench-tracer.c
 *  @brief          tracer の破棄経路と複数プロセス ファイル出力を測定します。
 *  @author         Tetsuo Honda
 *  @date           2026/08/30
 *  @version        1.0.0
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/base/result.h>
#include <cplat/console/console.h>
#include <cplat/clock/clock.h>
#include <cplat/runtime/process.h>
#include <cplat/trace/trace_file.h>
#include <cplat/trace/tracer.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** @brief 既定のプロセス数です。 */
#define DEFAULT_PROCESSES 16
/** @brief 各プロセスが出力する既定の行数です。 */
#define DEFAULT_MESSAGES 1000
/** @brief 数値引数用の文字列領域です。 */
#define NUMBER_TEXT_SIZE 32

/**
 *  @brief          単調増加時計をナノ秒で取得します。
 *  @return         ナノ秒単位の時刻です。
 */
static uint64_t monotonic_ns(void)
{
    cplat_timespec value;

    cplat_clock_get_monotonic(&value);
    return ((uint64_t)value.tv_sec * 1000000000U) + (uint64_t)value.tv_nsec;
}

/**
 *  @brief          worker として共有ファイルへ出力します。
 *  @param[in]      path       出力先。
 *  @param[in]      messages   出力行数。
 *  @param[in]      buffered   OS バッファーを使う場合は 1。
 *  @return         成功時は 0、失敗時は 1 を返します。
 */
static int run_worker(const char *path, int messages, int buffered)
{
    int flags = CPLAT_TRACE_FILE_SINK_SHARED;
    cplat_trace_file_sink *sink;
    int index;

    if (buffered != 0)
    {
        flags |= CPLAT_TRACE_FILE_SINK_OS_BUFFERED;
    }
    sink = cplat_trace_file_sink_create(path, 4096U, 3, flags);
    if (sink == NULL)
    {
        return 1;
    }
    for (index = 0; index < messages; index++)
    {
        if (cplat_trace_file_sink_write(sink, CPLAT_TRACE_LEVEL_INFO, NULL, "benchmark message") != CPLAT_OK)
        {
            cplat_trace_file_sink_dispose(sink);
            return 1;
        }
    }
    cplat_trace_file_sink_dispose(sink);
    return 0;
}

/**
 *  @brief          出力先がない tracer の破棄経路を測定します。
 *  @param[in]      messages  呼び出し回数。
 *  @return         成功時は 0、失敗時は 1 を返します。
 */
static int measure_filtered(int messages)
{
    cplat_tracer *tracer = cplat_tracer_create(CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED);
    uint64_t begin;
    uint64_t elapsed;
    int index;

    if (tracer == NULL || cplat_tracer_set_stderr_level(tracer, CPLAT_TRACE_LEVEL_NONE) != CPLAT_OK ||
        cplat_tracer_set_file_level(tracer, NULL, CPLAT_TRACE_LEVEL_NONE, 0, 0, 0) != CPLAT_OK ||
        cplat_tracer_set_os_level(tracer, CPLAT_TRACE_LEVEL_NONE) != CPLAT_OK ||
        cplat_tracer_set_etw_level(tracer, CPLAT_TRACE_LEVEL_NONE) != CPLAT_OK ||
        cplat_tracer_start(tracer) != CPLAT_OK)
    {
        cplat_tracer_dispose(&tracer);
        return 1;
    }
    begin = monotonic_ns();
    for (index = 0; index < messages; index++)
    {
        (void)cplat_tracer_writef(tracer, CPLAT_TRACE_LEVEL_INFO, NULL, "filtered=%d", index);
    }
    elapsed = monotonic_ns() - begin;
    printf("filtered,processes=1,messages=%d,total_ns=%llu,ns_per_message=%llu\n", messages,
           (unsigned long long)elapsed, (unsigned long long)(elapsed / (uint64_t)messages));
    cplat_tracer_dispose(&tracer);
    return 0;
}

/**
 *  @brief          worker 群を起動し、終了までの時間を測定します。
 *  @param[in]      executable  この実行ファイルのパス。
 *  @param[in]      path        共有ログのパス。
 *  @param[in]      processes   worker 数。
 *  @param[in]      messages    worker ごとの出力行数。
 *  @param[in]      buffered    OS バッファーを使う場合は 1。
 *  @return         成功時は 0、失敗時は 1 を返します。
 */
static int measure_files(char *executable, char *path, int processes, int messages, int buffered)
{
    cplat_process **children;
    char messages_text[NUMBER_TEXT_SIZE];
    char buffered_text[NUMBER_TEXT_SIZE];
    uint64_t begin;
    uint64_t elapsed;
    int index;
    int failed = 0;

    children = (cplat_process **)calloc((size_t)processes, sizeof(*children));
    if (children == NULL)
    {
        return 1;
    }
    (void)snprintf(messages_text, sizeof(messages_text), "%d", messages);
    (void)snprintf(buffered_text, sizeof(buffered_text), "%d", buffered);
    begin = monotonic_ns();
    for (index = 0; index < processes; index++)
    {
        char *args[] = {executable, "--worker", path, messages_text, buffered_text, NULL};
        cplat_process_options options;

        memset(&options, 0, sizeof(options));
        options.argv = args;
        options.stdin_spec.mode = CPLAT_PROCESS_STDIO_NULL_DEVICE;
        options.stdout_spec.mode = CPLAT_PROCESS_STDIO_NULL_DEVICE;
        options.stderr_spec.mode = CPLAT_PROCESS_STDIO_INHERIT;
        if (cplat_process_start(&options, &children[index]) != CPLAT_OK)
        {
            failed = 1;
            break;
        }
    }
    for (index = 0; index < processes; index++)
    {
        int exit_code = 1;

        if (children[index] != NULL &&
            (cplat_process_wait(children[index], CPLAT_PROCESS_WAIT_FOREVER) != CPLAT_OK ||
             cplat_process_get_exit_code(children[index], &exit_code) != CPLAT_OK || exit_code != 0))
        {
            failed = 1;
        }
        cplat_process_dispose(children[index]);
    }
    elapsed = monotonic_ns() - begin;
    const char *mode;
    if (buffered != 0)
    {
        mode = "buffered";
    }
    else
    {
        mode = "durable";
    }
    printf("file_%s,processes=%d,messages=%d,total_ns=%llu,ns_per_message=%llu\n", mode, processes,
           processes * messages, (unsigned long long)elapsed,
           (unsigned long long)(elapsed / ((uint64_t)processes * (uint64_t)messages)));
    free(children);
    return failed;
}

/**
 *  @brief          コマンド エントリ ポイントです。
 *  @param[in]      argc  引数の個数。
 *  @param[in]      argv  引数配列。
 *  @return         成功時は 0、失敗時は 1 を返します。
 */
int main(int argc, char **argv)
{
    char *path = "bench-tracer.log";
    int processes = DEFAULT_PROCESSES;
    int messages = DEFAULT_MESSAGES;

    cplat_console_init();

    if (argc == 5 && strcmp(argv[1], "--worker") == 0)
    {
        return run_worker(argv[2], atoi(argv[3]), atoi(argv[4]));
    }
    if (argc > 1)
    {
        path = argv[1];
    }
    if (argc > 2)
    {
        processes = atoi(argv[2]);
    }
    if (argc > 3)
    {
        messages = atoi(argv[3]);
    }
    if (processes <= 0 || messages <= 0)
    {
        fprintf(stderr, "usage: bench-tracer [path [processes [messages]]]\n");
        return 1;
    }
    if (measure_filtered(messages * processes) != 0 || measure_files(argv[0], path, processes, messages, 0) != 0 ||
        measure_files(argv[0], path, processes, messages, 1) != 0)
    {
        return 1;
    }
    return 0;
}
