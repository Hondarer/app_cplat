/**
 *******************************************************************************
 *  @file           bench_report.c
 *  @brief          測定環境の収集と、CSV および人間可読テーブルの出力を実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/07/29
 *  @version        1.0.0
 *
 *  CSV は Linux と Windows の測定結果を突き合わせる際の正本として使用します。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <stdio.h>
#include <string.h>

#include <cplat/base/platform.h>
#include <cplat/crt/stdlib.h>
#include <cplat/crt/stdio.h>

#if defined(PLATFORM_LINUX)
    #include <sys/vfs.h>
#elif defined(PLATFORM_WINDOWS)
    #include <cplat/base/windows_sdk.h>
    #include <cplat/win32/win32.h>
#endif /* PLATFORM_ */

#include "bench_case.h"

/**
 *  @brief          文字列に含まれるカンマを空白へ置き換えます。
 *  @param[in,out]  text  対象の文字列。NULL を渡してはなりません。
 *
 *  Windows の `PROCESSOR_IDENTIFIER` は "Intel64 Family 6 Model 85 Stepping 7, GenuineIntel" のように
 *  カンマを含みます。CSV では引用符で囲んでいても、awk などの単純な分割で列位置に不整合が生じるため、
 *  環境情報を格納する時点でカンマを取り除きます。\n
 *  環境情報を @ref bench_environment へ格納する経路はすべて本関数を経由してください。
 */
static void sanitize_commas(char *text)
{
    size_t index;

    for (index = 0U; text[index] != '\0'; index++)
    {
        if (text[index] == ',')
        {
            text[index] = ' ';
        }
    }
}

/**
 *  @brief          文字列を上限付きで複製します。
 *  @param[out]     dest  格納先。NULL を渡してはなりません。
 *  @param[in]      size  @p dest のサイズ (バイト)。1 以上を指定します。
 *  @param[in]      text  複製する文字列。NULL の場合は "unknown" を格納します。
 *
 *  格納する文字列に含まれるカンマは `sanitize_commas()` により空白へ置き換えます。
 */
static void copy_text(char *dest, size_t size, const char *text)
{
    const char *source = text;
    size_t length;

    if (source == NULL)
    {
        source = "unknown";
    }
    length = strlen(source);
    if (length >= size)
    {
        length = size - 1U;
    }
    memcpy(dest, source, length);
    dest[length] = '\0';

    sanitize_commas(dest);
}

/**
 *  @brief          文字列の末尾から改行と空白を取り除きます。
 *  @param[in,out]  text  対象の文字列。NULL を渡してはなりません。
 */
static void trim_trailing(char *text)
{
    size_t length = strlen(text);

    while (length > 0U)
    {
        char last = text[length - 1U];

        if (last != '\n' && last != '\r' && last != ' ' && last != '\t')
        {
            break;
        }
        text[length - 1U] = '\0';
        length--;
    }
}

/**
 *  @brief          CPU の型名を収集します。
 *  @param[out]     env  格納先。NULL を渡してはなりません。
 */
static void collect_cpu_model(bench_environment *env)
{
#if defined(PLATFORM_LINUX)
    FILE *stream = cplat_fopen("/proc/cpuinfo", "r", NULL);
    char line[256];

    copy_text(env->cpu_model, sizeof(env->cpu_model), NULL);
    if (stream == NULL)
    {
        return;
    }
    while (fgets(line, (int)sizeof(line), stream) != NULL)
    {
        if (strncmp(line, "model name", 10) == 0)
        {
            const char *separator = strchr(line, ':');

            if (separator != NULL)
            {
                copy_text(env->cpu_model, sizeof(env->cpu_model), separator + 1);
                trim_trailing(env->cpu_model);
            }
            break;
        }
    }
    (void)fclose(stream);
#elif defined(PLATFORM_WINDOWS)
    int exists = 0;

    if (cplat_getenv("PROCESSOR_IDENTIFIER", env->cpu_model, sizeof(env->cpu_model), &exists, NULL) != 0 || exists == 0)
    {
        copy_text(env->cpu_model, sizeof(env->cpu_model), NULL);
    }
    else
    {
        /* cplat_getenv は copy_text を経由せず直接書き込むため、ここでカンマを取り除きます。 */
        sanitize_commas(env->cpu_model);
    }
#else
    copy_text(env->cpu_model, sizeof(env->cpu_model), NULL);
#endif /* PLATFORM_ */
}

/**
 *  @brief          対象ディレクトリのファイル システム種別を収集します。
 *  @param[in]      dir  対象ディレクトリのパス。NULL を渡してはなりません。
 *  @param[out]     env  格納先。NULL を渡してはなりません。
 */
static void collect_fs_type(const char *dir, bench_environment *env)
{
#if defined(PLATFORM_LINUX)
    struct statfs info;
    char text[BENCH_ENV_TEXT_SIZE];

    if (statfs(dir, &info) != 0)
    {
        copy_text(env->fs_type, sizeof(env->fs_type), NULL);
        return;
    }
    /* statfs はファイル システム名を返さないため、magic 値をそのまま記録します。 */
    /* 主要な値の対応は Linux の statfs(2) を参照します。                         */
    /* see: https://man7.org/linux/man-pages/man2/statfs.2.html                 */
    (void)cplat_snprintf(text, sizeof(text), "magic=0x%lx", (unsigned long)info.f_type);
    copy_text(env->fs_type, sizeof(env->fs_type), text);
#elif defined(PLATFORM_WINDOWS)
    char fs_name[MAX_PATH + 1];
    char root[MAX_PATH + 1];

    if (GetVolumePathNameU(dir, root, (DWORD)sizeof(root)) == 0)
    {
        copy_text(env->fs_type, sizeof(env->fs_type), NULL);
        return;
    }
    if (GetVolumeInformationU(root, NULL, 0, NULL, NULL, NULL, fs_name, (DWORD)sizeof(fs_name)) == 0)
    {
        copy_text(env->fs_type, sizeof(env->fs_type), NULL);
        return;
    }
    copy_text(env->fs_type, sizeof(env->fs_type), fs_name);
#else
    (void)dir;
    copy_text(env->fs_type, sizeof(env->fs_type), NULL);
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

void bench_report_collect_environment(const char *dir, bench_environment *env)
{
    if (dir == NULL || env == NULL)
    {
        return;
    }

#if defined(PLATFORM_LINUX)
    copy_text(env->os_name, sizeof(env->os_name), "linux");
#elif defined(PLATFORM_WINDOWS)
    copy_text(env->os_name, sizeof(env->os_name), "windows");
#else
    copy_text(env->os_name, sizeof(env->os_name), NULL);
#endif /* PLATFORM_ */

    collect_cpu_model(env);
    collect_fs_type(dir, env);
    copy_text(env->cache_state, sizeof(env->cache_state), "warm");
}

/* Doxygen コメントは、ヘッダーに記載 */

void bench_report_begin_csv(FILE *csv, const bench_environment *env)
{
    if (csv == NULL || env == NULL)
    {
        return;
    }
    (void)cplat_fprintf(csv, "# os=%s\n", env->os_name);
    (void)cplat_fprintf(csv, "# cpu_model=%s\n", env->cpu_model);
    (void)cplat_fprintf(csv, "# fs_type=%s\n", env->fs_type);
    (void)cplat_fprintf(csv, "# record_bytes=%d\n", BENCH_RECORD_SIZE);
    (void)cplat_fprintf(csv, "os,cpu_model,fs_type,cache_state,api,pattern,file_size_bytes,record_bytes,"
                             "records_touched,iterations,trial_median_ns,trial_min_ns,trial_max_ns,"
                             "ns_per_record,mib_per_sec\n");
}

/* Doxygen コメントは、ヘッダーに記載 */

void bench_report_begin_table(const bench_environment *env)
{
    if (env == NULL)
    {
        return;
    }
    printf("os=%s  cpu=%s  fs=%s\n", env->os_name, env->cpu_model, env->fs_type);
    printf("%-16s %-14s %12s %10s %14s %12s %12s\n", "api", "pattern", "file_size", "records", "median_ns", "ns/record",
           "MiB/s");
}

/* Doxygen コメントは、ヘッダーに記載 */

void bench_report_row(FILE *csv, const bench_environment *env, const bench_case *item, const bench_context *ctx,
                      const bench_timing *timing)
{
    const char *api_name;
    const char *pattern_name;
    double ns_per_record = 0.0;
    double mib_per_sec = 0.0;
    double bytes;

    if (env == NULL || item == NULL || ctx == NULL || timing == NULL)
    {
        return;
    }

    api_name = bench_api_name(item->api, item->durable);
    pattern_name = bench_pattern_name(item->pattern);

    if (ctx->touch_count > 0U)
    {
        ns_per_record = (double)timing->median_ns / (double)ctx->touch_count;
    }
    bytes = (double)ctx->touch_count * (double)BENCH_RECORD_SIZE;
    if (timing->median_ns > 0U)
    {
        mib_per_sec = (bytes * 1000000000.0) / ((double)timing->median_ns * 1024.0 * 1024.0);
    }

    printf("%-16s %-14s %12llu %10llu %14llu %12.1f %12.1f\n", api_name, pattern_name,
           (unsigned long long)ctx->file_size, (unsigned long long)ctx->touch_count,
           (unsigned long long)timing->median_ns, ns_per_record, mib_per_sec);

    if (csv == NULL)
    {
        return;
    }
    (void)cplat_fprintf(csv, "%s,\"%s\",%s,%s,%s,%s,%llu,%d,%llu,%llu,%llu,%llu,%llu,%.3f,%.3f\n", env->os_name,
                        env->cpu_model, env->fs_type, env->cache_state, api_name, pattern_name,
                        (unsigned long long)ctx->file_size, BENCH_RECORD_SIZE, (unsigned long long)ctx->touch_count,
                        (unsigned long long)timing->iterations, (unsigned long long)timing->median_ns,
                        (unsigned long long)timing->min_ns, (unsigned long long)timing->max_ns, ns_per_record,
                        mib_per_sec);
}
