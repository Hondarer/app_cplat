/**
 *******************************************************************************
 *  @file           bench-io.c
 *  @brief          固定レコード長バイナリ ファイルに対する stdio と mmap の性能を比較するコマンドを実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/07/29
 *  @version        1.0.0
 *
 *  ファイル サイズ、アクセス パターン、API 形態を組み合わせた測定条件を列挙し、
 *  条件ごとに 1 レコードあたりの所要時間とスループットを求めます。\n
 *  測定方法の詳細は `app/cplat/prod/src/cmd/bench-io/benchmark-method.md` を参照してください。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cplat/argparser/argparser.h>
#include <cplat/base/platform.h>
#include <cplat/console/console.h>
#include <cplat/crt/stdio.h>
#include <cplat/crt/stdlib.h>

#include "bench_case.h"
#include "bench_timer.h"

/** @brief ランダム アクセスで 1 反復にアクセスするレコード数の上限です。 */
#define BENCH_RANDOM_TOUCH_MAX 65536U

/** @brief 乱数列の生成に使う固定シードです。 */
#define BENCH_RANDOM_SEED 88172645463325252ULL

/** @brief 測定対象ファイルのパスを格納する配列の大きさ [byte] (終端 NUL を含む) です。 */
#define BENCH_PATH_SIZE 512U

/** --min-ms の既定値 [ms] (1 以上) です。 */
#define BENCH_DEFAULT_MIN_MS 500

/** --trials の既定値です。 */
#define BENCH_DEFAULT_TRIALS 5

/** --sizes の 1 要素を受ける配列の大きさ [byte] (終端を含む) です。 */
#define BENCH_SIZE_TOKEN_SIZE 32U

/** --huge で追加するファイルの大きさ [GiB] です。 */
#define BENCH_HUGE_FILE_GIB 1U

/** --huge で追加するファイルの大きさ [byte] です。 */
#define BENCH_HUGE_FILE_SIZE ((size_t)BENCH_HUGE_FILE_GIB * 1024U * 1024U * 1024U)

static_assert((BENCH_DEFAULT_TRIALS >= 1) && ((unsigned int)BENCH_DEFAULT_TRIALS <= BENCH_TIMER_MAX_TRIALS), "範囲内");

/**
 *  @brief  測定ループへ渡す引数です。
 */
typedef struct bench_invocation
{
    bench_context *ctx;    /**< 共有状態。 */
    const bench_case *job; /**< 測定条件。 */
} bench_invocation;

static int is_mmap_api(bench_api api);
static int invoke_iteration(void *arg);
static int drop_page_cache(void *arg);
static int parse_size(const char *text, size_t *size);
static int list_contains(const char *list, const char *name);
static int create_data_file(const char *path, size_t size);
static uint64_t xorshift64_next(uint64_t *state);
static size_t *create_random_order(size_t record_count, size_t touch_count);
static int is_supported_case(bench_api api, bench_pattern pattern, int durable);
static size_t resolve_touch_count(bench_pattern pattern, size_t record_count);
static int execute_case(const bench_case *job, bench_context *ctx, const bench_environment *env, FILE *csv,
                        uint64_t min_ms, size_t trials, int cold);
static int run_size(const char *dir, size_t size, const char *api_list, const char *pat_list,
                    const bench_environment *env, FILE *csv, uint64_t min_ms, size_t trials, int cold, int keep);

/**
 *  @brief          アプリケーションのエントリ ポイント。
 *  @param[in]      argc コマンド ライン引数の個数。
 *  @param[in]      argv コマンド ライン引数の文字列の配列。
 *  @return         成功時は EXIT_SUCCESS、失敗時は EXIT_FAILURE を返します。
 */
int main(int argc, char *argv[])
{
    const char *dir = ".";
    const char *csv_path = NULL;
    const char *sizes = "4K,64K,1M,16M,256M";
    const char *api_list = NULL;
    const char *pattern_list = NULL;
    int need_help = 0;
    int huge = 0;
    int cold = 0;
    int keep = 0;
    int min_ms = BENCH_DEFAULT_MIN_MS;
    int trials = BENCH_DEFAULT_TRIALS;
    char huge_help[64];
    int parse_result;
    FILE *csv = NULL;
    bench_environment env;
    const char *cursor;
    int failures = 0;

    cplat_console_init();

    if (cplat_argparser_init(argc, argv,
                             "固定レコード長バイナリ ファイルに対する stdio と mmap の性能を比較します。") != CPLAT_OK)
    {
        /* 診断の出力先がほかにないため、戻り値は明示的に破棄する。 */
        (void)fprintf(stderr, "引数の解析の準備に失敗しました。\n");
        return EXIT_FAILURE;
    }
    /*
     * 登録の失敗はパーサーに記録され、直後の cplat_argparser_get_register_error_count() でまとめて検査するため、
     * 登録の関数の戻り値は明示的に破棄する。
     */
    (void)cplat_argparser_register_flag("-h", "--help", "ヘルプを表示します。", &need_help);
    (void)cplat_argparser_register_option_string(NULL, "--dir", "PATH", "測定用ファイルを置くディレクトリ。", 0U,
                                                 &dir);
    (void)cplat_argparser_register_option_string(NULL, "--csv", "PATH", "CSV の出力先。", 0U, &csv_path);
    (void)cplat_argparser_register_option_string(NULL, "--sizes", "LIST",
                                                 "測定するファイル サイズ (例: 4K,1M,256M)。", 0U, &sizes);
    (void)cplat_argparser_register_option_string(NULL, "--apis", "LIST", "測定する API 形態を絞り込みます。", 0U,
                                                 &api_list);
    (void)cplat_argparser_register_option_string(NULL, "--patterns", "LIST",
                                                 "測定するアクセス パターンを絞り込みます。", 0U, &pattern_list);
    (void)cplat_argparser_register_option_int(NULL, "--min-ms", "MS", "1 試行の測定区間の下限 (ミリ秒)。", 0U,
                                              &min_ms);
    (void)cplat_argparser_register_option_int(NULL, "--trials", "N", "1 条件あたりの試行回数。", 0U, &trials);
    if (cplat_snprintf(huge_help, sizeof(huge_help), "%u GB のケースを追加します。", BENCH_HUGE_FILE_GIB) != CPLAT_OK)
    {
        return EXIT_FAILURE;
    }
    (void)cplat_argparser_register_flag(NULL, "--huge", huge_help, &huge);
    (void)cplat_argparser_register_flag(NULL, "--cold", "ページ キャッシュを破棄して測定します (Linux、要 root)。",
                                        &cold);
    (void)cplat_argparser_register_flag(NULL, "--keep", "測定用ファイルを削除せずに残します。", &keep);

    if (cplat_argparser_get_register_error_count() > 0U)
    {
        /* 終了コードはすでに失敗に決まっているため、戻り値は明示的に破棄する。 */
        (void)cplat_argparser_print_register_error_messages(stderr);
        return EXIT_FAILURE;
    }

    parse_result = cplat_argparser_parse();

    if (need_help != 0)
    {
        if (cplat_argparser_print_usage(stdout) != CPLAT_OK || fflush(stdout) != 0 || ferror(stdout) != 0)
        {
            (void)cplat_fprintf(stderr, "error: failed to write stdout\n"); /* 診断の出力先がほかにないため、戻り値は明示的に破棄する */
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

    if (min_ms <= 0 || trials <= 0 || (unsigned int)trials > BENCH_TIMER_MAX_TRIALS)
    {
        /* 診断の出力先がほかにないため、戻り値は明示的に破棄する。 */
        (void)cplat_fprintf(stderr, "error: --min-ms は 1 以上、--trials は 1 以上 %u 以下を指定してください。\n",
                            BENCH_TIMER_MAX_TRIALS);
        return EXIT_FAILURE;
    }

    /* ページ キャッシュを破棄できることを、測定を始める前に確認する。 */
    if (cold != 0 && drop_page_cache(NULL) != 0)
    {
        /* 診断の出力先がほかにないため、戻り値は明示的に破棄する。 */
        (void)cplat_fprintf(stderr, "error: ページ キャッシュを破棄できません。"
                                    "--cold は Linux で root 権限が必要です。\n");
        return EXIT_FAILURE;
    }

    bench_report_collect_environment(dir, &env);

    if (cold != 0)
    {
        memcpy(env.cache_state, "cold", sizeof("cold")); /* 終端 NUL を含む */
    }

    if (csv_path != NULL)
    {
        csv = cplat_fopen(csv_path, "w", NULL);
        if (csv == NULL)
        {
            /* 診断の出力先がほかにないため、戻り値は明示的に破棄する。 */
            (void)cplat_fprintf(stderr, "error: failed to open %s\n", csv_path);
            return EXIT_FAILURE;
        }

        bench_report_begin_csv(csv, &env);
    }

    bench_report_begin_table(&env);

    /* カンマ区切りのサイズごとに測定する。 */
    cursor = sizes;
    while (*cursor != '\0')
    {
        char token[BENCH_SIZE_TOKEN_SIZE];
        const char *separator = strchr(cursor, ',');
        size_t length;
        size_t size = 0U;

        if (separator == NULL)
        {
            length = strlen(cursor);
        }
        else
        {
            length = (size_t)(separator - cursor);
        }

        if (length == 0U || length >= sizeof(token))
        {
            /* 診断の出力先がほかにないため、戻り値は明示的に破棄する。 */
            (void)cplat_fprintf(stderr, "error: invalid size token\n");
            failures++;
            break;
        }

        memcpy(token, cursor, length);
        token[length] = '\0';

        if (parse_size(token, &size) != 0)
        {
            /* 診断の出力先がほかにないため、戻り値は明示的に破棄する。 */
            (void)cplat_fprintf(stderr, "error: invalid size '%s'\n", token);
            failures++;
            break;
        }

        if (run_size(dir, size, api_list, pattern_list, &env, csv, (uint64_t)min_ms, (size_t)trials, cold, keep) != 0)
        {
            failures++;
        }

        if (separator == NULL)
        {
            break;
        }

        cursor = separator + 1;
    }

    /*
     * それまでの処理に失敗が 1 件でもある場合は、--huge の測定を省略する。
     * 指定したオプションが通知なく無視されないよう、省略したことを標準エラー出力へ通知する。
     */
    if (huge != 0)
    {
        if (failures == 0)
        {
            size_t size = BENCH_HUGE_FILE_SIZE;

            if (run_size(dir, size, api_list, pattern_list, &env, csv, (uint64_t)min_ms, (size_t)trials, cold, keep) !=
                0)
            {
                failures++;
            }
        }
        else
        {
            /* 診断の出力先がほかにないため、戻り値は明示的に破棄する。 */
            (void)cplat_fprintf(stderr, "error: --huge skipped due to earlier failures\n");
        }
    }

    if (csv != NULL)
    {
        /* bench_report_*() は CSV への書き込みの失敗を通知しないため、閉じる前にまとめて検査する。 */
        if (ferror(csv) != 0)
        {
            (void)cplat_fprintf(stderr, "error: failed to write %s\n", csv_path); /* 診断の出力先がほかにないため、戻り値は明示的に破棄する */
            failures++;
        }

        if (fclose(csv) != 0)
        {
            failures++;
        }
    }

    /* 標準出力の表の書き込みの失敗はストリームに残るため、終了の直前にまとめて検査する。 */
    if (fflush(stdout) != 0 || ferror(stdout) != 0)
    {
        (void)cplat_fprintf(stderr, "error: failed to write stdout\n"); /* 診断の出力先がほかにないため、戻り値は明示的に破棄する */
        failures++;
    }

    if (failures > 0)
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/* Doxygen コメントは、ヘッダーに記載 */

void bench_fill_record(size_t index, bench_record *record)
{
    record->id = (uint64_t)index;
    record->counter = 0;
    record->value_a = (int32_t)(index & 0xFFFFU);
    record->value_b = (int32_t)((index >> 3) & 0xFFFFU);
    memset(record->name, 0, sizeof(record->name));
    record->name[0] = 'r';
}

/* Doxygen コメントは、ヘッダーに記載 */

const char *bench_api_name(bench_api api, int durable)
{
    const char *name;

    switch (api)
    {
    case BENCH_API_STDIO_REC:
        name = "stdio-rec";
        if (durable != 0)
        {
            name = "stdio-rec+sync";
        }

        break;
    case BENCH_API_STDIO_BLK:
        name = "stdio-blk";
        if (durable != 0)
        {
            name = "stdio-blk+sync";
        }

        break;
    case BENCH_API_MMAP_ONCE:
        name = "mmap-once";
        if (durable != 0)
        {
            name = "mmap-once+sync";
        }

        break;
    case BENCH_API_MMAP_EACH:
        name = "mmap-each";
        if (durable != 0)
        {
            name = "mmap-each+sync";
        }

        break;
    case BENCH_API_COUNT:
    default:
        name = "unknown";
        break;
    }

    return name;
}

/* Doxygen コメントは、ヘッダーに記載 */

const char *bench_pattern_name(bench_pattern pattern)
{
    const char *name;

    switch (pattern)
    {
    case BENCH_PATTERN_SEQ_READ:
        name = "seq-read";
        break;
    case BENCH_PATTERN_SEQ_WRITE:
        name = "seq-write";
        break;
    case BENCH_PATTERN_RAND_READ:
        name = "rand-read";
        break;
    case BENCH_PATTERN_RAND_UPDATE:
        name = "rand-update";
        break;
    case BENCH_PATTERN_POINT_LOOKUP:
        name = "point-lookup";
        break;
    case BENCH_PATTERN_OPEN_CLOSE:
        name = "open-close";
        break;
    case BENCH_PATTERN_COUNT:
    default:
        name = "unknown";
        break;
    }

    return name;
}

/**
 *  @brief          API 形態が mmap 系であるかを判定します。
 *  @param[in]      api  API 形態。
 *  @return         mmap 系の場合は 1、それ以外は 0 を返します。
 */
static int is_mmap_api(bench_api api)
{
    if (api == BENCH_API_MMAP_ONCE || api == BENCH_API_MMAP_EACH)
    {
        return 1;
    }

    return 0;
}

/**
 *  @brief          測定対象を 1 反復ぶん実行します。
 *  @param[in,out]  arg  @ref bench_invocation へのポインター。NULL を渡してはなりません。
 *  @return         成功時は 0、失敗時は -1 を返します。
 */
static int invoke_iteration(void *arg)
{
    bench_invocation *invocation = (bench_invocation *)arg;

    if (is_mmap_api(invocation->job->api) != 0)
    {
        return bench_mmap_iterate(invocation->ctx, invocation->job);
    }

    return bench_stdio_iterate(invocation->ctx, invocation->job);
}

/**
 *  @brief          ページ キャッシュを破棄します。
 *  @param[in,out]  arg  未使用。
 *  @return         成功時は 0、失敗時は -1 を返します。
 *
 *  Linux では `/proc/sys/vm/drop_caches` へ 3 を書き込みます。root 権限が必要です。\n
 *  Windows には同等の手段がないため、常に失敗を返します。
 */
static int drop_page_cache(void *arg)
{
    (void)arg;
#if defined(PLATFORM_LINUX)
    FILE *stream;

    /*
     * drop_caches はダーティなページを破棄しない。
     * 破棄できるページを増やすため、書き込みの前に sync() する。
     * see: https://docs.kernel.org/admin-guide/sysctl/vm.html#drop-caches
     */
    sync();
    stream = cplat_fopen("/proc/sys/vm/drop_caches", "w", NULL);
    if (stream == NULL)
    {
        return -1;
    }

    static const char drop_caches_val[] = "3\n";
    const size_t val_len = sizeof(drop_caches_val) - 1U;

    if (fwrite(drop_caches_val, 1U, val_len, stream) != val_len)
    {
        (void)fclose(stream); /* 先行する書き込みの失敗を返す経路のため、戻り値は明示的に破棄する */
        return -1;
    }

    if (fclose(stream) != 0)
    {
        return -1;
    }

    return 0;
#else
    return -1;
#endif /* PLATFORM_LINUX */
}

/**
 *  @brief          サイズ表記を解析します。
 *  @param[in]      text  "4K"、"16M"、"1G" などのサイズ表記。NULL を渡してはなりません。
 *  @param[out]     size  解析結果の格納先。NULL を渡してはなりません。
 *  @return         成功時は 0、解析できない場合は -1 を返します。
 */
static int parse_size(const char *text, size_t *size)
{
    unsigned long long value = 0ULL;
    size_t index = 0U;
    unsigned long long multiplier = 1ULL;

    while (text[index] >= '0' && text[index] <= '9')
    {
        value = (value * 10ULL) + (unsigned long long)(text[index] - '0');
        index++;
    }

    if (index == 0U)
    {
        return -1;
    }

    if (text[index] == 'K' || text[index] == 'k')
    {
        multiplier = 1024ULL; /* [KiB] への換算係数 */
        index++;
    }
    else if (text[index] == 'M' || text[index] == 'm')
    {
        multiplier = 1024ULL * 1024ULL; /* [MiB] への換算係数 */
        index++;
    }
    else if (text[index] == 'G' || text[index] == 'g')
    {
        multiplier = 1024ULL * 1024ULL * 1024ULL; /* [GiB] への換算係数 */
        index++;
    }
    else
    {
        multiplier = 1ULL;
    }

    if (text[index] != '\0')
    {
        return -1;
    }

    value = value * multiplier;
    if (value < (unsigned long long)BENCH_RECORD_SIZE)
    {
        return -1;
    }

    *size = (size_t)value;

    return 0;
}

/**
 *  @brief          カンマ区切りのリストに指定の名前が含まれるかを判定します。
 *  @param[in]      list  カンマ区切りのリスト。NULL の場合は常に含まれるとみなします。
 *  @param[in]      name  検索する名前。NULL を渡してはなりません。
 *  @return         含まれる場合は 1、含まれない場合は 0 を返します。
 */
static int list_contains(const char *list, const char *name)
{
    const char *cursor = list;
    size_t name_length = strlen(name);

    if (list == NULL)
    {
        return 1;
    }

    while (*cursor != '\0')
    {
        const char *separator = strchr(cursor, ',');
        size_t length;

        if (separator == NULL)
        {
            length = strlen(cursor);
        }
        else
        {
            length = (size_t)(separator - cursor);
        }

        if (length == name_length && strncmp(cursor, name, length) == 0)
        {
            return 1;
        }

        if (separator == NULL)
        {
            break;
        }

        cursor = separator + 1;
    }

    return 0;
}

/**
 *  @brief          測定対象ファイルを生成し、全レコードを初期化します。
 *  @param[in]      path  生成するファイルのパス。NULL を渡してはなりません。
 *  @param[in]      size  ファイル サイズ [byte]。
 *  @return         成功時は 0、失敗時は -1 を返します。
 */
static int create_data_file(const char *path, size_t size)
{
    FILE *stream = cplat_fopen(path, "wb", NULL);
    bench_record *block;
    size_t written = 0U;
    size_t total = size / BENCH_RECORD_SIZE;

    if (stream == NULL)
    {
        return -1;
    }

    block = (bench_record *)cplat_calloc(BENCH_BLOCK_RECORDS, sizeof(bench_record));
    if (block == NULL)
    {
        (void)fclose(stream); /* 先行するメモリ確保の失敗を返す経路のため、戻り値は明示的に破棄する */
        return -1;
    }

    while (written < total)
    {
        size_t chunk = total - written;
        size_t index;

        if (chunk > BENCH_BLOCK_RECORDS)
        {
            chunk = BENCH_BLOCK_RECORDS;
        }

        for (index = 0U; index < chunk; index++)
        {
            bench_fill_record(written + index, &block[index]);
        }

        if (fwrite(block, sizeof(*block), chunk, stream) != chunk)
        {
            cplat_free(block);
            (void)fclose(stream); /* 先行する書き込みの失敗を返す経路のため、戻り値は明示的に破棄する */
            return -1;
        }

        written += chunk;
    }

    cplat_free(block);

    if (fclose(stream) != 0)
    {
        return -1;
    }

    return 0;
}

/**
 *  @brief          xorshift64 の状態を 1 ステップ進めます。
 *  @param[in,out]  state  乱数の状態。0 以外であること。
 *  @return         進めた後の状態を返します。
 *
 *  シフト量 13 / 7 / 17 は Marsaglia の原典 (Xorshift RNGs、印刷ページ 4 の xor64) に従います。\n
 *  see: https://www.jstatsoft.org/article/view/v008i14
 */
static uint64_t xorshift64_next(uint64_t *state)
{
    uint64_t value = *state;

    /* 符号なしの左シフトで上位ビットを捨てるのは xorshift64 の計算の一部であり、意図どおりである。 */
    /* coverity[overflow_const : SUPPRESS] */
    value ^= value << 13;
    value ^= value >> 7;

    /* 符号なしの左シフトで上位ビットを捨てるのは xorshift64 の計算の一部であり、意図どおりである。 */
    /* coverity[overflow_const : SUPPRESS] */
    value ^= value << 17;
    *state = value;

    return value;
}

/**
 *  @brief          ランダム アクセス順のレコード番号列を生成します。
 *  @param[in]      record_count  対象ファイルのレコード数。1 以上を指定します。
 *  @param[in]      touch_count   生成する要素数。1 以上を指定します。
 *  @return         生成した配列。呼び出し側が `free` します。失敗時は NULL を返します。
 *
 *  結果を Linux と Windows で一致させるため、固定シードの xorshift64 を使用します。
 */
static size_t *create_random_order(size_t record_count, size_t touch_count)
{
    size_t *order = (size_t *)cplat_calloc(touch_count, sizeof(size_t));
    uint64_t state = BENCH_RANDOM_SEED;
    size_t index;

    if (order == NULL)
    {
        return NULL;
    }

    for (index = 0U; index < touch_count; index++)
    {
        order[index] = (size_t)(xorshift64_next(&state) % (uint64_t)record_count);
    }

    return order;
}

/**
 *  @brief          測定条件が実行対象として妥当かを判定します。
 *  @param[in]      api      API 形態。
 *  @param[in]      pattern  アクセス パターン。
 *  @param[in]      durable  ディスクへの反映を伴う場合は 1。
 *  @return         実行対象の場合は 1、対象外の場合は 0 を返します。
 */
static int is_supported_case(bench_api api, bench_pattern pattern, int durable)
{
    /* 書き込みを伴わないパターンでは、ディスクへの反映条件は意味を持たない。 */
    if (durable != 0 && pattern != BENCH_PATTERN_SEQ_WRITE && pattern != BENCH_PATTERN_RAND_UPDATE)
    {
        return 0;
    }

    /* アタッチを測定ループの外で行う形態では、オープン コストの分離が成立しない。 */
    if (api == BENCH_API_MMAP_ONCE && pattern == BENCH_PATTERN_OPEN_CLOSE)
    {
        return 0;
    }

    /* ブロック単位アクセスは逐次パターンでのみ意味を持つ。 */
    if (api == BENCH_API_STDIO_BLK && pattern != BENCH_PATTERN_SEQ_READ && pattern != BENCH_PATTERN_SEQ_WRITE)
    {
        return 0;
    }

    return 1;
}

/**
 *  @brief          アクセス パターンごとに 1 反復でアクセスするレコード数を求めます。
 *  @param[in]      pattern       アクセス パターン。
 *  @param[in]      record_count  対象ファイルのレコード数。
 *  @return         1 反復でアクセスするレコード数。
 */
static size_t resolve_touch_count(bench_pattern pattern, size_t record_count)
{
    size_t touch;

    switch (pattern)
    {
    case BENCH_PATTERN_SEQ_READ:
    case BENCH_PATTERN_SEQ_WRITE:
        touch = record_count;
        break;
    case BENCH_PATTERN_RAND_READ:
    case BENCH_PATTERN_RAND_UPDATE:
        touch = record_count;
        if (touch > (size_t)BENCH_RANDOM_TOUCH_MAX)
        {
            touch = (size_t)BENCH_RANDOM_TOUCH_MAX;
        }

        break;
    case BENCH_PATTERN_POINT_LOOKUP:
    case BENCH_PATTERN_OPEN_CLOSE:
        touch = 1U;
        break;
    case BENCH_PATTERN_COUNT:
    default:
        touch = 0U;
        break;
    }

    return touch;
}

/**
 *  @brief          1 つの測定条件を実行し、結果を出力します。
 *  @param[in]      job          測定条件。NULL を渡してはなりません。
 *  @param[in,out]  ctx          共有状態。NULL を渡してはなりません。
 *  @param[in]      env          測定環境の情報。NULL を渡してはなりません。
 *  @param[in,out]  csv          CSV の出力先。NULL の場合は CSV を出力しません。
 *  @param[in]      min_ms       1 試行の測定区間の下限 [ms]。
 *  @param[in]      trials       試行回数。
 *  @param[in]      cold         ページ キャッシュを破棄して測定する場合は 1。
 *  @return         成功時は 0、失敗時は -1 を返します。
 */
static int execute_case(const bench_case *job, bench_context *ctx, const bench_environment *env, FILE *csv,
                        uint64_t min_ms, size_t trials, int cold)
{
    bench_invocation invocation;
    bench_timing timing;
    int setup_result;
    int measure_result;

    invocation.ctx = ctx;
    invocation.job = job;
    ctx->checksum = 0U;

    if (is_mmap_api(job->api) != 0)
    {
        setup_result = bench_mmap_setup(ctx, job);
    }
    else
    {
        setup_result = bench_stdio_setup(ctx, job);
    }

    if (setup_result != 0)
    {
        /* 診断の出力先がほかにないため、戻り値は明示的に破棄する。 */
        (void)cplat_fprintf(stderr, "error: setup failed for %s/%s\n", bench_api_name(job->api, job->durable),
                            bench_pattern_name(job->pattern));
        return -1;
    }

    if (cold != 0)
    {
        measure_result =
            bench_timer_measure_cold(invoke_iteration, &invocation, drop_page_cache, NULL, trials, &timing);
    }
    else
    {
        measure_result = bench_timer_measure(invoke_iteration, &invocation, min_ms, trials, 0U, &timing);
    }

    /* 測定の成否にかかわらず、準備で確保した資源を解放する。 */
    if (is_mmap_api(job->api) != 0)
    {
        if (bench_mmap_teardown(ctx, job) != 0)
        {
            measure_result = -1;
        }
    }
    else
    {
        bench_stdio_teardown(ctx, job);
    }

    if (measure_result != 0)
    {
        /* 診断の出力先がほかにないため、戻り値は明示的に破棄する。 */
        (void)cplat_fprintf(stderr, "error: measurement failed for %s/%s\n", bench_api_name(job->api, job->durable),
                            bench_pattern_name(job->pattern));
        return -1;
    }

    bench_report_row(csv, env, job, ctx, &timing);

    return 0;
}

/**
 *  @brief          1 つのファイル サイズについて、全 API 形態と全パターンを測定します。
 *  @param[in]      dir       作業ディレクトリのパス。NULL を渡してはなりません。
 *  @param[in]      size      対象ファイルのサイズ [byte]。
 *  @param[in]      api_list  対象とする API 形態のリスト。NULL の場合は全形態。
 *  @param[in]      pat_list  対象とするアクセス パターンのリスト。NULL の場合は全パターン。
 *  @param[in]      env       測定環境の情報。NULL を渡してはなりません。
 *  @param[in,out]  csv       CSV の出力先。NULL の場合は CSV を出力しません。
 *  @param[in]      min_ms    1 試行の測定区間の下限 [ms]。
 *  @param[in]      trials    試行回数。
 *  @param[in]      cold      ページ キャッシュを破棄して測定する場合は 1。
 *  @param[in]      keep      測定後にファイルを残す場合は 1。
 *  @return         成功時は 0、失敗時は -1 を返します。
 */
static int run_size(const char *dir, size_t size, const char *api_list, const char *pat_list,
                    const bench_environment *env, FILE *csv, uint64_t min_ms, size_t trials, int cold, int keep)
{
    char path[BENCH_PATH_SIZE];
    bench_context ctx = {0};
    size_t *order;
    size_t record_count = size / BENCH_RECORD_SIZE;
    int api_index;
    int failures = 0;

    if (cplat_snprintf(path, sizeof(path), "%s/bench_%llu.bin", dir, (unsigned long long)size) != CPLAT_OK)
    {
        /* 診断の出力先がほかにないため、戻り値は明示的に破棄する。 */
        (void)cplat_fprintf(stderr, "error: measurement path is too long\n");
        return -1;
    }

    if (create_data_file(path, size) != 0)
    {
        /* 診断の出力先がほかにないため、戻り値は明示的に破棄する。 */
        (void)cplat_fprintf(stderr, "error: failed to create %s\n", path);
        return -1;
    }

    order = create_random_order(record_count, (size_t)BENCH_RANDOM_TOUCH_MAX);
    if (order == NULL)
    {
        /*
         * すでに -1 を返す失敗の経路の後始末であり、削除の失敗で正常な測定の扱いにはならないため、
         * 戻り値は明示的に破棄する。
         */
        (void)cplat_remove(path, NULL);
        return -1;
    }

    ctx.path = path;
    ctx.order = order;
    ctx.file_size = size;
    ctx.record_count = record_count;

    for (api_index = 0; api_index < (int)BENCH_API_COUNT; api_index++)
    {
        int pattern_index;

        if (list_contains(api_list, bench_api_name((bench_api)api_index, 0)) == 0)
        {
            continue;
        }

        for (pattern_index = 0; pattern_index < (int)BENCH_PATTERN_COUNT; pattern_index++)
        {
            int durable;

            if (list_contains(pat_list, bench_pattern_name((bench_pattern)pattern_index)) == 0)
            {
                continue;
            }

            for (durable = 0; durable <= 1; durable++)
            {
                bench_case job;

                if (is_supported_case((bench_api)api_index, (bench_pattern)pattern_index, durable) == 0)
                {
                    continue;
                }

                job.file_size = size;
                job.api = (bench_api)api_index;
                job.pattern = (bench_pattern)pattern_index;
                job.durable = durable;
                job._pad_struct_end = 0;

                ctx.touch_count = resolve_touch_count(job.pattern, record_count);

                if (execute_case(&job, &ctx, env, csv, min_ms, trials, cold) != 0)
                {
                    failures++;
                }
            }
        }
    }

    cplat_free(order);

    if (keep == 0)
    {
        if (cplat_remove(path, NULL) != CPLAT_OK)
        {
            /* 診断の出力先がほかにないため、戻り値は明示的に破棄する。 */
            (void)cplat_fprintf(stderr, "error: failed to remove %s\n", path);
            failures++;
        }
    }

    if (failures > 0)
    {
        return -1;
    }

    return 0;
}
