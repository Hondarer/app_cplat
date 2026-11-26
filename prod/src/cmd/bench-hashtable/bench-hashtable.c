/**
 *******************************************************************************
 *  @file           bench-hashtable.c
 *  @brief          ハッシュ テーブルの可変長ストレージ操作の所要時間を測定します。
 *  @author         Tetsuo Honda
 *  @date           2026/08/22
 *  @version        1.0.0
 *
 *  可変長キーと可変長値を持つテーブルに対し、充填、更新、断片化した状態での
 *  再確保、圧縮、レコード数の変更の所要時間を capacity 別に測定します。\n
 *  可変長ストレージの確保は空き領域の個数に比例するため、capacity を増やしても
 *  1 件あたりの所要時間がほぼ一定であることを確認できます。\n
 *  測定方法は `benchmark-method.md` を参照してください。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/argparser/argparser.h>
#include <cplat/base/result.h>
#include <cplat/console/console.h>
#include <cplat/crt/stdio.h>
#include <cplat/hashtable/hashtable.h>

#include "bench_timer.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** @brief キー 1 件あたりのストレージ確保量 [byte] です。 */
#define BENCH_KEY_BYTES 16U

/** @brief 値 1 件あたりのストレージ確保量 [byte] です。 */
#define BENCH_VALUE_BYTES 24U

/** @brief 1 ケースあたりの試行回数です。 */
#define BENCH_TRIALS 5U

/** @brief キーと値を組み立てる作業領域の大きさ [byte] (終端 NUL を含む) です。 */
#define BENCH_TEXT_SIZE 32U

static_assert((BENCH_TRIALS >= 1U) && (BENCH_TRIALS <= (unsigned int)BENCH_TIMER_MAX_TRIALS),
              "試行回数は 1 以上上限以下であること");
static_assert(BENCH_TEXT_SIZE >= (6U + 20U + 1U), "\"value-\" と size_t 最大 20 桁と終端 NUL が収まること");

/** CHURN で 1 件おきに削除・埋め直す間隔です。 */
#define BENCH_CHURN_STRIDE 2U

/** RESIZE で capacity を拡張する倍率です。 */
#define BENCH_RESIZE_FACTOR 2U

/**
 *  @brief  削除済みレコードの寿命です。加齢の処理を呼び出さないため、3 以上なら測定結果に影響しません。
 *          制約: 2 以上かつ CPLAT_HASHTABLE_LIFETIME_INFINITE 未満であること。
 *          変更時は削除済みレコードの保持期間が変わる。
 */
#define BENCH_LIFETIME 5U

/**
 *  @brief  測定を始める capacity です。--max-capacity には、この値以上を指定します。
 *          制約: 1 以上かつ 2 のべき乗であること。変更時は最小測定点とヘルプの最小値表記が変わる。
 */
#define BENCH_MIN_CAPACITY 256U

/**
 *  @brief  capacity を増やす倍率です。
 *          制約: 2 以上であること。変更時は測定点の間隔が変わる。
 */
#define BENCH_CAPACITY_GROWTH_FACTOR 4U

/**
 *  @brief  測定する capacity の上限の既定値です。
 *          既定では 256、1024、4096、16384 の 4 点を測定します。
 *          制約: BENCH_MIN_CAPACITY 以上かつ INT_MAX 以下 (options.max_capacity が int のため)。
 *          変更時は省略時の最大測定点とヘルプの既定値表記が変わる。
 */
#define BENCH_DEFAULT_MAX_CAPACITY 16384U

/** @brief --max-capacity のヘルプ説明文を組み立てる作業配列の大きさ [byte] です。 */
#define BENCH_HELP_DESC_BYTES 128U

static_assert(BENCH_LIFETIME >= 2U, "lifetime must be at least 2");
static_assert(BENCH_LIFETIME < (unsigned int)CPLAT_HASHTABLE_LIFETIME_INFINITE,
              "lifetime must be less than CPLAT_HASHTABLE_LIFETIME_INFINITE");
static_assert(BENCH_MIN_CAPACITY >= 1U, "minimum capacity must be at least 1");
static_assert((BENCH_MIN_CAPACITY & (BENCH_MIN_CAPACITY - 1U)) == 0U, "minimum capacity must be power of 2");
static_assert(BENCH_CAPACITY_GROWTH_FACTOR >= 2U, "capacity growth factor must be at least 2");
static_assert(BENCH_DEFAULT_MAX_CAPACITY >= BENCH_MIN_CAPACITY, "default max capacity must be >= min");
static_assert(BENCH_DEFAULT_MAX_CAPACITY <= (size_t)INT_MAX, "default max capacity must fit in int");

/**
 *  @brief  測定対象の操作です。
 */
typedef enum
{
    BENCH_SCENARIO_FILL = 0, /**< 空のテーブルへ capacity 件を追加します。 */
    BENCH_SCENARIO_UPDATE,   /**< 満杯のテーブルの全件を同じ長さの値へ更新します。 */
    BENCH_SCENARIO_CHURN,    /**< 半数を回収して空きブロックを作り、同数を追加し直します。 */
    BENCH_SCENARIO_COMPACT,  /**< 半数を回収した状態のストレージを圧縮します。 */
    BENCH_SCENARIO_RESIZE,   /**< capacity を 2 倍へ広げます。 */
    BENCH_SCENARIO_COUNT     /**< 列挙子の個数です。 */
} bench_scenario;

/**
 *  @brief  1 つの測定条件です。
 */
typedef struct bench_hashtable_case
{
    size_t capacity;         /**< スロット数です。 */
    bench_scenario scenario; /**< 測定対象の操作です。 */
    int _pad_struct_end;     /**< パディング抑止用の予約領域です。 */
} bench_hashtable_case;

/**
 *  @brief  測定中に共有する状態です。
 */
typedef struct bench_state
{
    cplat_hashtable *ht;     /**< 測定対象のテーブルです。 */
    size_t capacity;         /**< スロット数です。 */
    bench_scenario scenario; /**< 測定対象の操作です。 */
    int failed;              /**< 準備または測定に失敗した場合は 1 です。 */
} bench_state;

/**
 *  @brief          測定対象の操作の表示名を返します。
 *  @param[in]      scenario  測定対象の操作。
 *  @return         表示名を指すポインターです。
 */
static const char *bench_scenario_name(bench_scenario scenario)
{
    switch (scenario)
    {
    case BENCH_SCENARIO_FILL:
        return "fill";
    case BENCH_SCENARIO_UPDATE:
        return "update";
    case BENCH_SCENARIO_CHURN:
        return "churn";
    case BENCH_SCENARIO_COMPACT:
        return "compact";
    case BENCH_SCENARIO_RESIZE:
        return "resize";
    case BENCH_SCENARIO_COUNT:
    default:
        return "unknown";
    }
}

/**
 *  @brief          測定対象の操作が 1 反復で扱う件数を返します。
 *  @param[in]      scenario  測定対象の操作。
 *  @param[in]      capacity  スロット数。
 *  @return         1 反復で扱う件数です。
 *
 *  1 件あたりの所要時間を求めるための除数です。@ref BENCH_SCENARIO_COMPACT と
 *  @ref BENCH_SCENARIO_RESIZE はテーブル全体を 1 回操作するため capacity を返します。
 */
static size_t bench_scenario_units(bench_scenario scenario, size_t capacity)
{
    if (scenario == BENCH_SCENARIO_CHURN)
    {
        return capacity / BENCH_CHURN_STRIDE;
    }

    return capacity;
}

/**
 *  @brief          測定用のキーを組み立てます。
 *  @param[in]      index  レコード番号。
 *  @param[out]     dest   組み立て先。NULL を渡してはなりません。
 */
static void bench_make_key(size_t index, char *dest)
{
    /*
     * size_t の最大 20 桁でも 1 + 20 + 1 = 22 [byte] であり、BENCH_TEXT_SIZE (32) に収まって切り詰めが起きないため、
     * 戻り値は明示的に破棄する。
     */
    (void)snprintf(dest, BENCH_TEXT_SIZE, "k%08zu", index);
}

/**
 *  @brief          測定用の値を組み立てます。
 *  @param[in]      index  レコード番号。
 *  @param[out]     dest   組み立て先。NULL を渡してはなりません。
 *
 *  長さを一定にし、更新で確保量が変わらないようにします。
 */
static void bench_make_value(size_t index, char *dest)
{
    /*
     * size_t の最大 20 桁でも 6 + 20 + 1 = 27 [byte] であり、BENCH_TEXT_SIZE (32) に収まって切り詰めが起きないため、
     * 戻り値は明示的に破棄する。
     */
    (void)snprintf(dest, BENCH_TEXT_SIZE, "value-%010zu", index);
}

/**
 *  @brief          可変長キーと可変長値を持つ設定を組み立てます。
 *  @param[in]      capacity    スロット数。
 *  @param[out]     config_out  組み立て先。NULL を渡してはなりません。
 */
static void bench_make_config(size_t capacity, cplat_hashtable_config *config_out)
{
    memset(config_out, 0, sizeof(*config_out));
    config_out->capacity = capacity;
    config_out->key_type = CPLAT_HASHTABLE_FIELD_VARIABLE_STRING;
    config_out->value_type = CPLAT_HASHTABLE_FIELD_VARIABLE_STRING;
    config_out->key_storage_size = capacity * BENCH_KEY_BYTES;
    config_out->value_storage_size = capacity * BENCH_VALUE_BYTES;
    config_out->lifetime = (unsigned char)BENCH_LIFETIME;
}

/**
 *  @brief          指定した件数を追加します。
 *  @param[in,out]  ht     対象。NULL を渡してはなりません。
 *  @param[in]      from   追加を始めるレコード番号。
 *  @param[in]      count  追加する件数。
 *  @param[in]      step   レコード番号の増分。1 以上を指定します。
 *  @return         成功時は 0、失敗時は -1 を返します。
 */
static int bench_add_range(cplat_hashtable *ht, size_t from, size_t count, size_t step)
{
    size_t i;

    for (i = 0; i < count; i++)
    {
        char key[BENCH_TEXT_SIZE];
        char value[BENCH_TEXT_SIZE];

        bench_make_key(from + (i * step), key);
        bench_make_value(from + (i * step), value);

        if (cplat_hashtable_add(ht, key, value, CPLAT_HASHTABLE_ADD_DELETED_OVERWRITE) != CPLAT_OK)
        {
            return -1;
        }
    }

    return 0;
}

/**
 *  @brief          1 件おきに削除し、回収して空きブロックを作ります。
 *  @param[in,out]  ht        対象。NULL を渡してはなりません。
 *  @param[in]      capacity  スロット数。
 *  @return         成功時は 0、失敗時は -1 を返します。
 */
static int bench_fragment_storage(cplat_hashtable *ht, size_t capacity)
{
    size_t i;

    for (i = 0; i < capacity; i += BENCH_CHURN_STRIDE)
    {
        char key[BENCH_TEXT_SIZE];

        bench_make_key(i, key);

        if (cplat_hashtable_delete(ht, key) != CPLAT_OK)
        {
            return -1;
        }
    }

    if (cplat_hashtable_purge_deleted(ht) == CPLAT_OK)
    {
        return 0;
    }

    return -1;
}

/**
 *  @brief          測定の直前に、対象テーブルを所定の状態へ用意します。
 *  @param[in,out]  arg  @ref bench_state を指すポインター。NULL を渡してはなりません。
 *  @return         成功時は 0、失敗時は -1 を返します。
 */
static int bench_prepare(void *arg)
{
    bench_state *state = (bench_state *)arg;
    cplat_hashtable_config config;

    cplat_hashtable_dispose(state->ht);
    state->ht = NULL;
    bench_make_config(state->capacity, &config);

    if (cplat_hashtable_create(&config, NULL, 0, NULL, 0, &state->ht) != CPLAT_OK)
    {
        state->failed = 1;
        return -1;
    }

    switch (state->scenario)
    {
    case BENCH_SCENARIO_FILL:
        /* 空のテーブルへの追加を測定するため、事前のデータ追加は行わない。 */
        break;
    case BENCH_SCENARIO_UPDATE:
    case BENCH_SCENARIO_RESIZE:
        if (bench_add_range(state->ht, 0, state->capacity, 1) != 0)
        {
            state->failed = 1;
            return -1;
        }
        break;
    case BENCH_SCENARIO_CHURN:
    case BENCH_SCENARIO_COMPACT:
        if (bench_add_range(state->ht, 0, state->capacity, 1) != 0)
        {
            state->failed = 1;
            return -1;
        }

        if (bench_fragment_storage(state->ht, state->capacity) != 0)
        {
            state->failed = 1;
            return -1;
        }

        break;
    case BENCH_SCENARIO_COUNT:
    default:
        state->failed = 1;
        return -1;
    }

    return 0;
}

/**
 *  @brief          測定対象の操作を 1 反復ぶん実行します。
 *  @param[in,out]  arg  @ref bench_state を指すポインター。NULL を渡してはなりません。
 *  @return         成功時は 0、失敗時は -1 を返します。
 */
static int bench_iterate(void *arg)
{
    bench_state *state = (bench_state *)arg;
    cplat_hashtable_config config;
    size_t i;

    switch (state->scenario)
    {
    case BENCH_SCENARIO_FILL:
        return bench_add_range(state->ht, 0, state->capacity, 1);
    case BENCH_SCENARIO_UPDATE:
        for (i = 0; i < state->capacity; i++)
        {
            char key[BENCH_TEXT_SIZE];
            char value[BENCH_TEXT_SIZE];

            bench_make_key(i, key);
            bench_make_value(i + state->capacity, value);

            if (cplat_hashtable_update(state->ht, key, value) != CPLAT_OK)
            {
                return -1;
            }
        }

        return 0;
    case BENCH_SCENARIO_CHURN:
        /* 回収済みのレコード番号を、同じ長さのキーと値で埋め直す。 */
        return bench_add_range(state->ht, 0, state->capacity / BENCH_CHURN_STRIDE, BENCH_CHURN_STRIDE);
    case BENCH_SCENARIO_COMPACT:
        if (cplat_hashtable_compact(state->ht) == CPLAT_OK)
        {
            return 0;
        }

        return -1;
    case BENCH_SCENARIO_RESIZE:
        bench_make_config(state->capacity * BENCH_RESIZE_FACTOR, &config);
        if (cplat_hashtable_resize(state->ht, &config) == CPLAT_OK)
        {
            return 0;
        }

        return -1;
    case BENCH_SCENARIO_COUNT:
    default:
        return -1;
    }
}

/**
 *  @brief          1 ケースを測定し、結果を 1 行で出力します。
 *  @param[in,out]  csv   CSV の出力先。NULL の場合は CSV を出力しません。
 *  @param[in]      item  測定条件。NULL を渡してはなりません。
 *  @return         成功時は 0、失敗時は -1 を返します。
 */
static int bench_run_case(FILE *csv, const bench_hashtable_case *item)
{
    bench_state state = {0};
    bench_timing timing;
    size_t units = bench_scenario_units(item->scenario, item->capacity);
    double per_unit_ns;
    int ret;

    state.capacity = item->capacity;
    state.scenario = item->scenario;

    ret = bench_timer_measure_cold(bench_iterate, &state, bench_prepare, &state, BENCH_TRIALS, &timing);
    cplat_hashtable_dispose(state.ht);

    if ((ret != 0) || (state.failed != 0))
    {
        /* 測定の失敗は直後の -1 で伝え、診断の出力先もほかにないため、戻り値は明示的に破棄する。 */
        (void)fprintf(stderr, "measurement failed: capacity=%zu scenario=%s\n", item->capacity,
                      bench_scenario_name(item->scenario));
        return -1;
    }

    if (units != 0)
    {
        per_unit_ns = ((double)timing.median_ns / (double)units);
    }
    else
    {
        per_unit_ns = 0.0;
    }

    /*
     * 表の出力の失敗は標準出力のストリームに残り、main が終了の直前にまとめて検査するため、
     * 戻り値は明示的に破棄する。
     */
    (void)printf("%10zu  %-8s  %14.3f  %12.1f\n", item->capacity, bench_scenario_name(item->scenario),
                 (double)timing.median_ns / 1000000.0, per_unit_ns); /* [ns] から [ms] への換算 */

    if (csv != NULL)
    {
        /*
         * CSV の出力の失敗はストリームに残り、main が CSV を閉じる直前にまとめて検査するため、
         * 戻り値は明示的に破棄する。
         */
        (void)fprintf(csv, "%zu,%s,%llu,%llu,%llu,%.1f\n", item->capacity, bench_scenario_name(item->scenario),
                      (unsigned long long)timing.median_ns, (unsigned long long)timing.min_ns,
                      (unsigned long long)timing.max_ns, per_unit_ns);
    }

    return 0;
}

/**
 *  @brief  コマンド ライン オプションです。
 */
typedef struct bench_hashtable_options
{
    const char *csv_path; /**< CSV の出力先パス。指定がなければ NULL です。 */
    int max_capacity;     /**< 測定する capacity の上限。既定値は `BENCH_DEFAULT_MAX_CAPACITY` です。 */
    int need_help;        /**< ヘルプ表示が指定された場合は 1 です。 */
} bench_hashtable_options;

/**
 *  @brief          コマンド ライン オプションを登録します。
 *  @param[in,out]  options  解析結果の格納先として各メンバーのアドレスを登録するオプションです。
 *  @return         成功時は 0、失敗時は -1 を返します。
 */
static int register_options(bench_hashtable_options *options)
{
    char max_cap_desc[BENCH_HELP_DESC_BYTES];

    /*
     * 書式結果の最大長は固定文言 64 [byte] + uint 最大 10 桁 × 2 + NUL 1 = 85 [byte] であり、
     * BENCH_HELP_DESC_BYTES (128) に収まって切り詰めが起きないため、戻り値は明示的に破棄する。
     */
    (void)snprintf(max_cap_desc, sizeof(max_cap_desc), "largest number of slots to measure (default %u, minimum %u)",
                   BENCH_DEFAULT_MAX_CAPACITY, BENCH_MIN_CAPACITY);

    /*
     * 登録の失敗はパーサーに記録され、直後の cplat_argparser_get_register_error_count() でまとめて検査するため、
     * この関数の登録の関数の戻り値は明示的に破棄する。
     */
    (void)cplat_argparser_register_flag("-h", "--help", "show this help", &options->need_help);
    (void)cplat_argparser_register_option_int(NULL, "--max-capacity", "N", max_cap_desc, 0, &options->max_capacity);
    (void)cplat_argparser_register_option_string(NULL, "--csv", "PATH", "also write results as CSV", 0,
                                                 &options->csv_path);

    if (cplat_argparser_get_register_error_count() > 0)
    {
        /* 登録の失敗は直後の -1 で伝え、診断の出力先もほかにないため、戻り値は明示的に破棄する。 */
        (void)cplat_argparser_print_register_error_messages(stderr);
        return -1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    bench_hashtable_options options = {0};
    FILE *csv = NULL;
    size_t max_capacity;
    size_t capacity;
    int failures = 0;
    int ret;

    options.max_capacity = (int)BENCH_DEFAULT_MAX_CAPACITY;

    cplat_console_init();

    if (cplat_argparser_init(argc, argv, "Measure hash table variable-length storage operations.") != CPLAT_OK)
    {
        /* 診断の出力先がほかにないため、戻り値は明示的に破棄する。 */
        (void)fprintf(stderr, "引数の解析の準備に失敗しました。\n");
        return EXIT_FAILURE;
    }

    if (register_options(&options) != 0)
    {
        return EXIT_FAILURE;
    }

    ret = cplat_argparser_parse();

    if (options.need_help != 0)
    {
        if (cplat_argparser_print_usage(stdout) != CPLAT_OK || fflush(stdout) != 0 || ferror(stdout) != 0)
        {
            (void)fprintf(stderr, "failed to write stdout.\n"); /* 診断の出力先がほかにないため、戻り値は明示的に破棄する */
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }

    if (ret != CPLAT_OK)
    {
        /* 終了コードはすでに失敗に決まっているため、この 2 つの表示の関数の戻り値は明示的に破棄する。 */
        (void)cplat_argparser_print_error_messages(stderr);
        (void)cplat_argparser_print_usage(stderr);
        return EXIT_FAILURE;
    }

    if (options.max_capacity < (int)BENCH_MIN_CAPACITY)
    {
        /* 診断の出力先がほかにないため、戻り値は明示的に破棄する。 */
        (void)fprintf(stderr, "max capacity must be at least %u.\n", BENCH_MIN_CAPACITY);
        return EXIT_FAILURE;
    }

    max_capacity = (size_t)options.max_capacity;

    if (options.csv_path != NULL)
    {
        csv = cplat_fopen(options.csv_path, "w", NULL);
        if (csv == NULL)
        {
            /* 診断の出力先がほかにないため、戻り値は明示的に破棄する。 */
            (void)fprintf(stderr, "cannot open %s for writing.\n", options.csv_path);
            return EXIT_FAILURE;
        }

        /* CSV の出力の失敗はストリームに残り、CSV を閉じる直前にまとめて検査するため、戻り値は明示的に破棄する。 */
        (void)fprintf(csv, "capacity,scenario,median_ns,min_ns,max_ns,ns_per_unit\n");
    }

    /*
     * 表の見出しの出力の失敗は標準出力のストリームに残り、終了の直前にまとめて検査するため、
     * この 2 つの printf の戻り値は明示的に破棄する。
     */
    (void)printf("%10s  %-8s  %14s  %12s\n", "capacity", "scenario", "median (ms)", "ns/unit");
    (void)printf("--------------------------------------------------------\n");

    /* capacity を 4 倍ずつ増やしながら、各シナリオの測定を実行する。 */
    for (capacity = BENCH_MIN_CAPACITY; capacity <= max_capacity; capacity *= BENCH_CAPACITY_GROWTH_FACTOR)
    {
        int scenario;

        for (scenario = 0; scenario < (int)BENCH_SCENARIO_COUNT; scenario++)
        {
            bench_hashtable_case item = {0};

            item.capacity = capacity;
            item.scenario = (bench_scenario)scenario;

            if (bench_run_case(csv, &item) != 0)
            {
                failures++;
            }
        }
    }

    if (csv != NULL)
    {
        /* CSV への書き込みの失敗はストリームに残るため、閉じる前にまとめて検査する。 */
        if (ferror(csv) != 0)
        {
            (void)fprintf(stderr, "failed to write %s.\n", options.csv_path); /* 診断の出力先がほかにないため、戻り値は明示的に破棄する */
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
        (void)fprintf(stderr, "failed to write stdout.\n"); /* 診断の出力先がほかにないため、戻り値は明示的に破棄する */
        failures++;
    }

    if (failures == 0)
    {
        return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
}
