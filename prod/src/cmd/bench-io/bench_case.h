/**
 *******************************************************************************
 *  @file           bench_case.h
 *  @brief          ベンチマークのレコード型、ケース記述子、API 実装の共通インターフェイスを提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/07/29
 *  @version        1.0.0
 *
 *  固定レコード長バイナリ ファイルを対象に、`stdio` ラッパー API と
 *  メモリ マップド ファイル API の性能を同一条件で比較するための型を定義します。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#ifndef BENCH_CASE_PRIVATE_H
#define BENCH_CASE_PRIVATE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "bench_timer.h"

/** @brief 1 レコードのサイズ (バイト)。 */
#define BENCH_RECORD_SIZE 64

/** @brief ブロック単位アクセスで 1 回に扱うレコード数 (64 KB 相当)。 */
#define BENCH_BLOCK_RECORDS 1024

/**
 *  @brief  固定レコード長バイナリ ファイルの 1 レコードです。
 *
 *  パディングが入らないよう、@ref BENCH_RECORD_SIZE バイトちょうどに揃えています。
 */
typedef struct bench_record
{
    uint64_t id;     /**< レコード識別子。 */
    int64_t counter; /**< 更新回数。 */
    int32_t value_a; /**< 集計対象の値 1。 */
    int32_t value_b; /**< 集計対象の値 2。 */
    char name[40];   /**< 固定長の名前。 */
} bench_record;

/** @brief 測定対象の API 形態です。 */
typedef enum
{
    BENCH_API_STDIO_REC = 0, /**< `cplat_fopen` + レコード単位の読み書き。 */
    BENCH_API_STDIO_BLK,     /**< `cplat_fopen` + ブロック単位の読み書き。 */
    BENCH_API_MMAP_ONCE,     /**< アタッチを測定ループの外で 1 回だけ行う。 */
    BENCH_API_MMAP_EACH,     /**< 反復ごとにアタッチとデタッチを行う。 */
    BENCH_API_COUNT          /**< 列挙子の個数。 */
} bench_api;

/** @brief 測定対象のアクセス パターンです。 */
typedef enum
{
    BENCH_PATTERN_SEQ_READ = 0, /**< 全レコードを先頭から読む。 */
    BENCH_PATTERN_SEQ_WRITE,    /**< 全レコードを先頭から書く。 */
    BENCH_PATTERN_RAND_READ,    /**< 乱数順にレコードを読む。 */
    BENCH_PATTERN_RAND_UPDATE,  /**< 乱数順にレコードを read-modify-write する。 */
    BENCH_PATTERN_POINT_LOOKUP, /**< 開いて 1 レコードだけ読み、閉じる。 */
    BENCH_PATTERN_OPEN_CLOSE,   /**< 開いて閉じるだけ (オープン コストの分離)。 */
    BENCH_PATTERN_COUNT         /**< 列挙子の個数。 */
} bench_pattern;

/**
 *  @brief  1 つの測定条件を表す記述子です。
 */
typedef struct bench_case
{
    size_t file_size;      /**< 対象ファイルのサイズ (バイト)。 */
    bench_api api;         /**< API 形態。 */
    bench_pattern pattern; /**< アクセス パターン。 */
    int durable;           /**< 書き込み後にディスクへの反映を要求する場合は 1。 */
    int _pad_struct_end;   /**< パディング抑止用の予約領域。 */
} bench_case;

/**
 *  @brief  測定中に API 実装が共有する状態です。
 */
typedef struct bench_context
{
    const char *path;    /**< 対象ファイルのパス。 */
    const size_t *order; /**< ランダム アクセスのレコード番号列。 */
    void *state;         /**< API 実装が保持する状態 (アタッチ済みハンドルなど)。 */
    size_t file_size;    /**< 対象ファイルのサイズ (バイト)。 */
    size_t record_count; /**< 対象ファイルに含まれるレコード数。 */
    size_t touch_count;  /**< 1 反復でアクセスするレコード数。 */
    uint64_t checksum;   /**< 最適化による処理除去を防ぐための集計値。 */
} bench_context;

/**
 *  @brief          レコードの初期値を組み立てます。
 *  @param[in]      index   レコード番号。
 *  @param[out]     record  組み立てた内容の格納先。NULL を渡してはなりません。
 */
void bench_fill_record(size_t index, bench_record *record);

/**
 *  @brief          `stdio` ラッパー API 系のケースで、測定ループ開始前の準備を行います。
 *  @param[in,out]  ctx   共有状態。NULL を渡してはなりません。
 *  @param[in]      item  測定条件。NULL を渡してはなりません。
 *  @return         成功時は 0、失敗時は -1 を返します。
 */
int bench_stdio_setup(bench_context *ctx, const bench_case *item);

/**
 *  @brief          `stdio` ラッパー API 系のケースを 1 反復ぶん実行します。
 *  @param[in,out]  ctx   共有状態。NULL を渡してはなりません。
 *  @param[in]      item  測定条件。NULL を渡してはなりません。
 *  @return         成功時は 0、失敗時は -1 を返します。
 */
int bench_stdio_iterate(bench_context *ctx, const bench_case *item);

/**
 *  @brief          `stdio` ラッパー API 系のケースで確保した資源を解放します。
 *  @param[in,out]  ctx   共有状態。NULL を渡してはなりません。
 *  @param[in]      item  測定条件。NULL を渡してはなりません。
 */
void bench_stdio_teardown(bench_context *ctx, const bench_case *item);

/**
 *  @brief          メモリ マップド ファイル API 系のケースで、測定ループ開始前の準備を行います。
 *  @param[in,out]  ctx   共有状態。NULL を渡してはなりません。
 *  @param[in]      item  測定条件。NULL を渡してはなりません。
 *  @return         成功時は 0、失敗時は -1 を返します。
 */
int bench_mmap_setup(bench_context *ctx, const bench_case *item);

/**
 *  @brief          メモリ マップド ファイル API 系のケースを 1 反復ぶん実行します。
 *  @param[in,out]  ctx   共有状態。NULL を渡してはなりません。
 *  @param[in]      item  測定条件。NULL を渡してはなりません。
 *  @return         成功時は 0、失敗時は -1 を返します。
 */
int bench_mmap_iterate(bench_context *ctx, const bench_case *item);

/**
 *  @brief          メモリ マップド ファイル API 系のケースで確保した資源を解放します。
 *  @param[in,out]  ctx   共有状態。NULL を渡してはなりません。
 *  @param[in]      item  測定条件。NULL を渡してはなりません。
 */
void bench_mmap_teardown(bench_context *ctx, const bench_case *item);

/**
 *  @brief          API 形態の表示名を返します。
 *  @param[in]      api      API 形態。
 *  @param[in]      durable  ディスクへの反映を伴う場合は 1。
 *  @return         表示名を指すポインター。
 */
const char *bench_api_name(bench_api api, int durable);

/**
 *  @brief          アクセス パターンの表示名を返します。
 *  @param[in]      pattern  アクセス パターン。
 *  @return         表示名を指すポインター。
 */
const char *bench_pattern_name(bench_pattern pattern);

/** @brief 環境情報の文字列長の上限です。 */
#define BENCH_ENV_TEXT_SIZE 128

/**
 *  @brief  測定を実施した環境の情報です。
 */
typedef struct bench_environment
{
    char os_name[BENCH_ENV_TEXT_SIZE];     /**< OS 種別。 */
    char cpu_model[BENCH_ENV_TEXT_SIZE];   /**< CPU の型名。 */
    char fs_type[BENCH_ENV_TEXT_SIZE];     /**< 対象ディレクトリのファイル システム種別。 */
    char cache_state[BENCH_ENV_TEXT_SIZE]; /**< ページ キャッシュの状態 ("warm" または "cold")。 */
} bench_environment;

/**
 *  @brief          測定環境の情報を収集します。
 *  @param[in]      dir  対象ディレクトリのパス。NULL を渡してはなりません。
 *  @param[out]     env  収集した情報の格納先。NULL を渡してはなりません。
 */
void bench_report_collect_environment(const char *dir, bench_environment *env);

/**
 *  @brief          CSV のヘッダーと環境情報のコメント行を出力します。
 *  @param[in,out]  csv  出力先。NULL の場合は何もしません。
 *  @param[in]      env  測定環境の情報。NULL を渡してはなりません。
 */
void bench_report_begin_csv(FILE *csv, const bench_environment *env);

/**
 *  @brief          人間可読テーブルの見出しを出力します。
 *  @param[in]      env  測定環境の情報。NULL を渡してはなりません。
 */
void bench_report_begin_table(const bench_environment *env);

/**
 *  @brief          1 ケースの測定結果を出力します。
 *  @param[in,out]  csv     CSV の出力先。NULL の場合は CSV を出力しません。
 *  @param[in]      env     測定環境の情報。NULL を渡してはなりません。
 *  @param[in]      item    測定条件。NULL を渡してはなりません。
 *  @param[in]      ctx     共有状態。NULL を渡してはなりません。
 *  @param[in]      timing  測定結果。NULL を渡してはなりません。
 */
void bench_report_row(FILE *csv, const bench_environment *env, const bench_case *item, const bench_context *ctx,
                      const bench_timing *timing);

#endif /* BENCH_CASE_PRIVATE_H */
