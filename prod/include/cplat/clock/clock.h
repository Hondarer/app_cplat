/**
 *******************************************************************************
 *  @file           clock.h
 *  @brief          プラットフォームを抽象化してクロックを取得する API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/04/19
 *  @version        1.0.0
 *
 *  OS ごとに異なるクロック API を共通インターフェースで抽象化します。\n
 *  単調増加クロック、実時刻クロック、UTC 分解、絶対 deadline 生成を提供します。
 *
 *  @section        clock_comparison クロックの比較
 *
 *  | 項目                  | 単調増加クロック               | 実時刻クロック                        |
 *  | --------------------- | ------------------------------ | ------------------------------------- |
 *  | 対応関数              | cplat_clock_get_monotonic_ms()\n cplat_clock_get_monotonic() | cplat_clock_get_realtime() |
 *  | 基準点                | 起動時 (不定)                  | Unix epoch (1970-01-01T00:00:00Z)     |
 *  | NTP 補正の影響        | 受けない                       | 受ける (時刻が前後する可能性あり)     |
 *  | 値の単調増加          | 保証される                     | 保証されない                          |
 *  | 精度 (Linux)          | ナノ秒                         | ナノ秒                                |
 *  | 精度 (Windows)        | ミリ秒 (GetTickCount64)        | 100 ナノ秒 (FILETIME)                 |
 *  | 主な用途              | 経過時間測定・タイムアウト判定 | 実時刻の刻印 (ログ・セッション ID 等) |
 *
 *  Table: 単調増加クロックと実時刻クロックの比較
 *
 *  @section        clock_usage 使い分けの指針
 *
 *  - **経過時間を測る・タイムアウトを判定する** → cplat_clock_get_monotonic_ms() または cplat_clock_get_monotonic() を使用する。\n
 *    実時刻クロックは NTP 補正でジャンプするため、差分計算が正しく行えない場合がある。
 *  - **実時刻を記録・外部と共有する** → cplat_clock_get_realtime() を使用する。\n
 *    セッション開始時刻、ログのタイムスタンプなど、カレンダー時刻として意味を持つ場合に限定します。
 *  - **現在の UTC を分解済みで扱う** → cplat_clock_get_realtime_utc() を使用する。\n
 *    ログのタイムスタンプ書式化など、年月日時分秒に分解して扱う用途に使用します。
 *  - **実時刻 deadline を作る** → cplat_clock_get_realtime_deadline_ms() を使用する。\n
 *    sync 実装や OS 変換層で absolute deadline を要求する API へ渡す値を生成します。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CLOCK_H
#define CLOCK_H

#include <cplat/base/platform.h>
#include <cplat/base/result.h>
#include <cplat/clock/timespec.h>
#include <cplat/cplat_export.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

/**
 *  @ingroup        CPLAT_CLOCK
 *  @{
 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          単調増加クロックの現在値をミリ秒単位で返します。
     *  @return         単調増加クロックのミリ秒値。
     *
     *  OS の単調増加クロック (CLOCK_MONOTONIC 相当) を読み取り、ミリ秒に変換して返します。\n
     *  タイムアウト判定・差分計算など ms 精度で十分な用途に使用します。\n
     *  より高い精度が必要な場合は cplat_clock_get_monotonic() を使用してください。
     *
     *  返す値の特性:
     *
     *  | 項目                  | Linux                            | Windows                          |
     *  | --------------------- | -------------------------------- | -------------------------------- |
     *  | 基準点                | 起動時 (不定)                    | 起動時 (不定)                    |
     *  | 内部 API              | `clock_gettime(CLOCK_MONOTONIC)` | `GetTickCount64()`               |
     *  | 精度                  | ナノ秒 (ms に切り捨て)           | ～15 ms (ハードウェア依存)       |
     *  | オーバーフロー        | 実質なし (uint64_t)              | 実質なし (uint64_t, ～5.8 億年)  |
     *
     *  Table: cplat_clock_get_monotonic_ms の返す値の特性
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     *
     *  @note           返す値の絶対値に意味はありません。必ず 2 点間の差分として使用してください。
     *
     *  @remark         Windows では GetTickCount64() の分解能がハードウェアに依存し、
     *                  通常 15 ms 程度です。1 ms 未満の精度が必要な場合は
     *                  cplat_clock_get_monotonic() を使用してください。
     *
     *  使用例:
        @code{.c}
        uint64_t deadline = cplat_clock_get_monotonic_ms() + 500; // 500 ms タイムアウト
        while (cplat_clock_get_monotonic_ms() < deadline)
        {
            // 処理
        }
        @endcode
     */
    CPLAT_EXPORT uint64_t CPLAT_API cplat_clock_get_monotonic_ms(void);

    /**
     *  @brief          単調増加クロックの現在値を cplat_timespec で返します。
     *  @param[out]     ts  単調増加クロックの現在値 (秒部・ナノ秒部) の格納先。
     *
     *  OS の単調増加クロック (CLOCK_MONOTONIC 相当) をナノ秒精度で返します。\n
     *  受信タイムスタンプ・高精度な経過時間測定など μs 以下の精度が必要な用途に使用します。
     *
     *  返す値の特性:
     *
     *  | 項目                  | Linux                            | Windows                           |
     *  | --------------------- | -------------------------------- | --------------------------------- |
     *  | 基準点                | 起動時 (不定)                    | 起動時 (不定)                     |
     *  | 内部 API              | `clock_gettime(CLOCK_MONOTONIC)` | `GetTickCount64()`                |
     *  | tv_sec の精度         | ナノ秒                           | ミリ秒 (tv_nsec は ms 単位で格納) |
     *  | tv_nsec の範囲        | 0 ～ 999,999,999                 | 0 ～ 999,000,000 (1 ms 刻み)      |
     *
     *  Table: cplat_clock_get_monotonic の返す値の特性
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     *
     *  @note           返す値の絶対値に意味はありません。必ず 2 点間の差分として使用してください。
     *
     *  @note           Windows では内部で GetTickCount64() を使用するため、
     *                  tv_nsec の有効桁は ms 単位 (1,000,000 ns 刻み) になります。
     *                  ms 以上の精度が必要な場合は QueryPerformanceCounter() の利用を検討してください。
     *
     *  @remark         ms 精度で十分な場合は cplat_clock_get_monotonic_ms() の使用を推奨します。
     *
     *  使用例 (経過ミリ秒を算出):
        @code{.c}
        cplat_timespec start;
        cplat_timespec end;
        int64_t elapsed_ms;

        cplat_clock_get_monotonic(&start);
        // ... 計測対象の処理 ...
        cplat_clock_get_monotonic(&end);

        elapsed_ms = cplat_timespec_diff_ms(&end, &start);
        @endcode
     */
    CPLAT_EXPORT void CPLAT_API cplat_clock_get_monotonic(cplat_timespec *ts);

    /**
     *  @brief          現在時刻を cplat_timespec で返します。
     *  @param[out]     ts  Unix epoch (1970-01-01T00:00:00Z) 基準の現在時刻の格納先。
     *
     *  OS の実時刻クロック (CLOCK_REALTIME 相当) を読み取り、
     *  Unix epoch (1970-01-01T00:00:00Z) からの経過時間として返します。\n
     *  セッション識別子・ログのタイムスタンプなどカレンダー時刻として意味を持つ値を記録する場合に使用します。
     *
     *  返す値の特性:
     *
     *  | 項目                  | Linux                           | Windows                               |
     *  | --------------------- | ------------------------------- | ------------------------------------- |
     *  | 内部 API              | `clock_gettime(CLOCK_REALTIME)` | `GetSystemTimeAsFileTime()`           |
     *  | 基準点                | Unix epoch (1970-01-01)         | Unix epoch に変換 (内部は 1601-01-01) |
     *  | 精度                  | ナノ秒                          | 100 ナノ秒 (tv_nsec は 100 ns 刻み)   |
     *  | NTP 補正の影響        | 受ける                          | 受ける                                |
     *
     *  Table: cplat_clock_get_realtime の返す値の特性
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     *
     *  @warning        **経過時間の測定やタイムアウト判定には使用しないでください。**\n
     *                  NTP 補正や管理者による手動設定で時刻が前後する場合があり、
     *                  差分が負値になるなど正しい結果が得られないことがあります。\n
     *                  経過時間の測定には cplat_clock_get_monotonic_ms() または cplat_clock_get_monotonic() を使用してください。
     *
     *  @note           Windows では FILETIME (100 ns 単位、1601-01-01 起算) を内部で使用し、
     *                  Unix epoch へ変換しています。変換オフセットは 11,644,473,600 秒
     *                  (1601-01-01 から 1970-01-01 までの差) です。
     *
     *  @note           Windows では tv_nsec の有効桁は 100 ns 単位 (100 刻み) になります。
     *
     *  使用例 (セッション開始時刻の記録):
        @code{.c}
        cplat_timespec session_ts;

        cplat_clock_get_realtime(&session_ts);
        // session_ts を構造体に保存して識別子として使用する
        @endcode
     */
    CPLAT_EXPORT void CPLAT_API cplat_clock_get_realtime(cplat_timespec *ts);

    /**
     *  @brief          cplat_clock_format_realtime_iso8601_local() が返す時刻文字列の長さです。
     *
     *  書式は `YYYY-MM-DDTHH:MM:SS.sss+09:00` です。\n
     *  値は null 終端を含まない文字数です。
     */
#define CPLAT_CLOCK_ISO8601_LOCAL_MSEC_LEN 29

    /**
     *  @brief          cplat_clock_format_realtime_iso8601_utc() が返す時刻文字列の長さです。
     *
     *  書式は `YYYY-MM-DDTHH:MM:SS.sssZ` です。\n
     *  値は null 終端を含まない文字数です。
     */
#define CPLAT_CLOCK_ISO8601_UTC_MSEC_LEN 24

    /**
     *  @brief          UTC 基準の実時刻をローカル時刻の ISO8601 文字列へ整形します。
     *  @param[out]     buf       出力先バッファー。
     *  @param[in]      buf_size  出力先バッファー サイズ。
     *  @param[in]      timestamp Unix epoch 基準の実時刻 (`tv_nsec` は 0 以上 999,999,999 以下)。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  cplat_clock_get_realtime() が返す UTC 基準の時刻値を、
     *  ローカル時刻の `YYYY-MM-DDTHH:MM:SS.sss+09:00` 形式へ変換します。\n
     *  バッファーが十分な場合の必要サイズは
     *  CPLAT_CLOCK_ISO8601_LOCAL_MSEC_LEN + 1 です。
     *
     *  @note           失敗時は `0000-00-00T00:00:00.000+00:00` を格納しようと試みます。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_clock_format_realtime_iso8601_local(char *buf, size_t buf_size,
                                                                         const cplat_timespec *timestamp);

    /**
     *  @brief          UTC 基準の実時刻を UTC の ISO8601 文字列へ整形します。
     *  @param[out]     buf       出力先バッファー。
     *  @param[in]      buf_size  出力先バッファー サイズ。
     *  @param[in]      timestamp Unix epoch 基準の実時刻 (`tv_nsec` は 0 以上 999,999,999 以下)。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  cplat_clock_get_realtime() が返す UTC 基準の時刻値を、
     *  UTC の `YYYY-MM-DDTHH:MM:SS.sssZ` 形式へ変換します。\n
     *  バッファーが十分な場合の必要サイズは
     *  CPLAT_CLOCK_ISO8601_UTC_MSEC_LEN + 1 です。
     *
     *  @note           失敗時は `0000-00-00T00:00:00.000Z` を格納しようと試みます。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_clock_format_realtime_iso8601_utc(char *buf, size_t buf_size,
                                                                       const cplat_timespec *timestamp);

    /**
     *  @brief          現在時刻を UTC の分解済み時刻とナノ秒で返します。
     *  @param[out]     utc_tm  UTC の年月日時分秒を受け取る `struct tm`。
     *  @param[out]     tv_nsec ナノ秒部 (0 以上 999,999,999 以下)。
     *
     *  現在の実時刻を取得し、UTC の分解済み時刻へ変換して返します。\n
     *  ログのタイムスタンプ生成など、カレンダー時刻を文字列化する用途を想定します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     *
     *  @note           `utc_tm` は UTC であり、ローカル タイムではありません。
     *
     *  使用例:
        @code{.c}
        struct tm utc_tm;
        int32_t   nsec;

        cplat_clock_get_realtime_utc(&utc_tm, &nsec);
        // utc_tm と nsec を使って "YYYY-MM-DD HH:MM:SS.mmm" を組み立てる
        @endcode
     */
    CPLAT_EXPORT void CPLAT_API cplat_clock_get_realtime_utc(struct tm *utc_tm, int32_t *tv_nsec);

    /**
     *  @brief          現在時刻から指定ミリ秒後の absolute deadline を返します。
     *  @param[in]      timeout_ms  現在時刻へ加算するタイムアウト値 (ミリ秒)。
     *  @param[out]     abs_timeout absolute deadline を受け取る `struct timespec`。
     *
     *  現在の実時刻を取得し、指定ミリ秒を加算した absolute deadline を返します。\n
     *  sync 実装や OS 変換層のように absolute deadline を要求する API の
     *  入力生成に使用します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     *
     *  @warning        返す値は実時刻ベースです。\n
     *                  NTP 補正や管理者による時刻変更の影響を受けるため、
     *                  差分計算には使用しないでください。
     *
     *  使用例:
        @code{.c}
     *  struct timespec deadline;
     *
     *  cplat_clock_get_realtime_deadline_ms(100, &deadline);
     *  // absolute deadline を要求する同期 API へ渡す
        @endcode
     */
    CPLAT_EXPORT void CPLAT_API cplat_clock_get_realtime_deadline_ms(uint64_t timeout_ms, struct timespec *abs_timeout);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CLOCK_H */
