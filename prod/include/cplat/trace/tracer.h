/**
 *******************************************************************************
 *  @file           tracer.h
 *  @brief          クロスプラットフォームのトレーシング API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/03/05
 *  @version        1.0.0
 *
 *  Windows (ETW) と Linux (syslog) の差異を抽象化し、
 *  共通のトレース レベルとインターフェースを提供します。\n
 *  内部で `cplat/trace/etw.h` (Windows) または `cplat/trace/syslog.h` (Linux) を
 *  使用します。
 *
 *  @par            アーキテクチャー
    @code
   Application
         |
         v  tracer.h (共通 API)
         |
   +-----+-----+------+--------+
   |           |      |        |
   ETW        syslog File    stderr
   (Windows)  (Linux) (両OS)  (両OS)
    @endcode
 *
 *  @par            使用例 (共通)
 *  すべての出力先の既定値は CPLAT_TRACE_LEVEL_NONE (無効) です。\n
 *  出力する出力先の詳細度を、cplat_tracer_start の前に設定します。
    @code{.c}
   #include <cplat/trace/tracer.h>

   cplat_tracer *tracer = cplat_tracer_create(CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED);
   cplat_tracer_set_name(tracer, "myapp", 0);
   cplat_tracer_set_stderr_level(tracer, CPLAT_TRACE_LEVEL_INFO);
   cplat_tracer_start(tracer);
   cplat_tracer_write(tracer, CPLAT_TRACE_LEVEL_INFO, NULL, "application started");
   cplat_tracer_stop(tracer);
   cplat_tracer_dispose(&tracer);
    @endcode
 *
 *  @par            使用例 (設定変更)
    @code{.c}
   cplat_tracer *tracer = cplat_tracer_create(CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED);
   cplat_tracer_set_name(tracer, "myapp", 0);
   cplat_tracer_set_os_level(tracer, CPLAT_TRACE_LEVEL_VERBOSE);
   cplat_tracer_start(tracer);
   cplat_tracer_write(tracer, CPLAT_TRACE_LEVEL_INFO, NULL, "running as myapp");
   cplat_tracer_stop(tracer);
   cplat_tracer_set_name(tracer, "myapp", 1); // "myapp_1" として再開
   cplat_tracer_start(tracer);
   cplat_tracer_write(tracer, CPLAT_TRACE_LEVEL_INFO, NULL, "running as myapp_1");
   cplat_tracer_stop(tracer);
   cplat_tracer_dispose(&tracer);
    @endcode
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CPLAT_TRACER_H
#define CPLAT_TRACER_H

/* size_t (cplat_tracer_write_hex / cplat_tracer_write_hexf で使用) */
#include <stddef.h>
/* int64_t (cplat_tracer_set_name で使用) */
#include <inttypes.h>
#include <cplat/base/platform.h>
#include <cplat/base/result.h>
#include <cplat/clock/clock.h>
#include <cplat/crt/path.h>
#include <cplat/cplat_export.h>

/* 内部で使用するプラットフォーム固有ヘッダー */
#if defined(PLATFORM_LINUX)
    #include <cplat/trace/syslog.h>
#elif defined(PLATFORM_WINDOWS)
    #include <cplat/trace/etw.h>
#endif /* PLATFORM_ */

/**
 *  @ingroup        CPLAT_TRACE
 *  @{
 */

/* ===== 既定のプロバイダー定義 (Windows) ===== */

#if defined(PLATFORM_WINDOWS)

    /**
     *  @brief          cplat が使用する既定の OS トレース識別子 (Windows) です。
     *
     *  cplat_tracer_create が使用する ETW プロバイダー名と、
     *  EventLog の共通イベント ソース名 (@ref eventlog.h) を兼ねます。\n
     *  ETW と EventLog で同一の識別子を共用します。
     */
    #define CPLAT_TRACER_DEFAULT_PROVIDER_NAME "cplat.tracer"

    /**
     *  @brief          既定の ETW プロバイダーの GUID (TraceLogging タプル形式) です。
     *
     *  TRACELOGGING_DEFINE_PROVIDER で使用する形式です。
     */
    #define CPLAT_TRACER_DEFAULT_PROVIDER_GUID \
        (0xc3a7b5d1, 0x4e2f, 0x4a89, 0x96, 0xc8, 0xd7, 0xe9, 0xf1, 0xa2, 0xb3, 0xc4)

    /**
     *  @brief          既定の ETW プロバイダーの GUID (文字列形式) です。
     *
     *  cplat_etw_session_start に渡す場合など、文字列形式の GUID が
     *  必要な場面で使用します。
     */
    #define CPLAT_TRACER_DEFAULT_PROVIDER_GUID_STR "c3a7b5d1-4e2f-4a89-96c8-d7e9f1a2b3c4"

#endif /* PLATFORM_WINDOWS */

/* ===== メッセージ長制限 ===== */

/**
 *  @brief          cplat_tracer_write が受け付けるメッセージの最大バイト数 (null 終端含む) です。
 *
 *  ETW (約 65,000 バイト) と syslog (RFC 3164: 1,024 バイト) の
 *  推奨上限のうち小さい方を採用し、クロスプラットフォームでの
 *  安全な転送を保証します。\n
 *  本文の最大長は `CPLAT_TRACER_MESSAGE_MAX_BYTES` @c - @c 1 (= 1,023) バイトです。
 */
#define CPLAT_TRACER_MESSAGE_MAX_BYTES 1024

/**
 *  @brief          cplat_tracer_write_hex がラベルなしで HEX 出力できるバイナリ データの最大バイト数です。
 *
 *  1 バイトあたり 3 文字 (HH + スペース) を消費し、最終バイトは 2 文字です。\n
 *  ラベル (@p message) を指定した場合はラベル長 + セパレータ (": ") 分だけ
 *  出力可能なバイナリ データ量が減少します。\n
 *  データがこの上限を超える場合は切り詰めが行われ、
 *  末尾に `"..."` が付与されます。
 */
#define CPLAT_TRACER_HEX_MAX_DATA_BYTES 341

/* ===== 共通トレース レベル ===== */

/**
 *  @enum           cplat_trace_level
 *  @brief          アプリケーション共通トレース レベルです。
 *
 *  OS 非依存のトレース レベルを定義します。重大度は上から下へ低下します。\n
 *  内部で ETW Level (1-5) および syslog severity へマッピングされます。\n
 *  CPLAT_TRACE_LEVEL_DEBUG は ETW / syslog では CPLAT_TRACE_LEVEL_VERBOSE と同じ詳細度で扱われます。
 *
 *  | cplat_trace_level          | ETW Level         | syslog severity |
 *  | ----------------------------- | ----------------- | --------------- |
 *  | CPLAT_TRACE_LEVEL_CRITICAL | Critical (1)      | LOG_CRIT (2)    |
 *  | CPLAT_TRACE_LEVEL_ERROR    | Error (2)         | LOG_ERR (3)     |
 *  | CPLAT_TRACE_LEVEL_WARNING  | Warning (3)       | LOG_WARNING (4) |
 *  | CPLAT_TRACE_LEVEL_INFO     | Informational (4) | LOG_INFO (6)    |
 *  | CPLAT_TRACE_LEVEL_VERBOSE  | Verbose (5)       | LOG_DEBUG (7)   |
 *  | CPLAT_TRACE_LEVEL_DEBUG    | Verbose (5)       | LOG_DEBUG (7)   |
 *
 *  Table: 通常トレース レベルの ETW・syslog との対応
 *
 *  CPLAT_TRACE_LEVEL_INFO 以上は、常時記録の対象となる想定でレベルを設計しています。\n
 *  運用中のシステムで何が起きたかを、設定を変更せずに後から追えるようにするためです。
 *
 *  CPLAT_TRACE_LEVEL_INFO には、繰り返し発生する処理の記録や、1 件ごとの詳細を含めないでください。\n
 *  常時記録の対象が増え続けると保存期間が短くなり、必要な記録が先に失われます。
 *
 *  量の多い記録や、内部の状態を細かく残す記録は CPLAT_TRACE_LEVEL_VERBOSE または
 *  CPLAT_TRACE_LEVEL_DEBUG にしてください。\n
 *  これらは既定では記録せず、調査の必要が生じたときに有効にします。
 *
 *  通常のレベルと対になる強制出力のレベルを、負の値の帯として持ちます。\n
 *  強制出力のレベルで書き込みを要求すると、出力先のスレッショルド レベルによらず出力します。
 *  重大度では選び切れない記録を、絞り込みの結果として残すための入口です。\n
 *  スレッショルド レベルが CPLAT_TRACE_LEVEL_NONE の出力先へは出力しません。
 *  出力しない設定を明示している出力先までは越えません。
 *
 *  強制出力のレベルは、出力行の表記では対応する通常のレベルと同じに見えます。\n
 *  強制であるかどうかは絞り込みの結果であり、記録の重大度ではないためです。
 *
 *  ETW と syslog へは、対応する通常のレベルと同じ重大度で渡します。\n
 *  ただし CPLAT_TRACE_LEVEL_FORCE_VERBOSE と CPLAT_TRACE_LEVEL_FORCE_DEBUG は、
 *  Informational (4) と LOG_INFO (6) へ引き上げます。
 *  常時記録の対象となる帯へ入れ、cplat の外側の設定で捨てられないようにするためです。
 *
 *  | cplat_trace_level                | ETW Level         | syslog severity |
 *  | -------------------------------- | ----------------- | --------------- |
 *  | CPLAT_TRACE_LEVEL_FORCE_CRITICAL | Critical (1)      | LOG_CRIT (2)    |
 *  | CPLAT_TRACE_LEVEL_FORCE_ERROR    | Error (2)         | LOG_ERR (3)     |
 *  | CPLAT_TRACE_LEVEL_FORCE_WARNING  | Warning (3)       | LOG_WARNING (4) |
 *  | CPLAT_TRACE_LEVEL_FORCE_INFO     | Informational (4) | LOG_INFO (6)    |
 *  | CPLAT_TRACE_LEVEL_FORCE_VERBOSE  | Informational (4) | LOG_INFO (6)    |
 *  | CPLAT_TRACE_LEVEL_FORCE_DEBUG    | Informational (4) | LOG_INFO (6)    |
 *
 *  Table: 強制出力トレース レベルの ETW・syslog との対応
 *
 *  強制出力のレベルは、スレッショルド レベルとして指定できません。\n
 *  指定するとスレッショルド レベルを設定する関数が引数の誤りを返します。
 */
typedef enum cplat_trace_level
{
    CPLAT_TRACE_LEVEL_FORCE_CRITICAL = -7, /**< 致命的エラー。強制出力します。 */
    CPLAT_TRACE_LEVEL_FORCE_ERROR = -6,    /**< エラー。強制出力します。 */
    CPLAT_TRACE_LEVEL_FORCE_WARNING = -5,  /**< 警告。強制出力します。 */
    CPLAT_TRACE_LEVEL_FORCE_INFO = -4,     /**< 情報。強制出力します。 */
    CPLAT_TRACE_LEVEL_FORCE_VERBOSE = -3,  /**< 詳細な診断情報。強制出力します。 */
    CPLAT_TRACE_LEVEL_FORCE_DEBUG = -2,    /**< 最も詳細な診断情報。強制出力します。 */
    CPLAT_TRACE_LEVEL_FORCE_NONE = -1,     /**< 出力しない。強制出力の帯における対の値です。 */
    CPLAT_TRACE_LEVEL_CRITICAL = 0,        /**< 致命的エラー。 */
    CPLAT_TRACE_LEVEL_ERROR = 1,           /**< エラー。 */
    CPLAT_TRACE_LEVEL_WARNING = 2,         /**< 警告。 */
    CPLAT_TRACE_LEVEL_INFO = 3,            /**< 情報。 */
    CPLAT_TRACE_LEVEL_VERBOSE = 4,         /**< 詳細な診断情報。 */
    CPLAT_TRACE_LEVEL_DEBUG = 5,           /**< 最も詳細な診断情報。 */
    CPLAT_TRACE_LEVEL_NONE = 6             /**< 出力しない。 */
} cplat_trace_level;

/**
 *  @brief          通常のトレース レベルと強制出力のトレース レベルの差です。
 *
 *  強制出力のレベルは、通常のレベルからこの値を引いた負の値です。\n
 *  しきい値の判定は「出力要求のレベルがスレッショルド レベル以下であること」のため、
 *  どの通常のスレッショルド レベルよりも小さい値になり、判定を通過します。
 */
#define CPLAT_TRACE_LEVEL_FORCE_OFFSET 7

/**
 *  @brief          通常のトレース レベルを、強制出力のトレース レベルへ変換します。
 *  @param[in]      level 変換する通常のトレース レベル。
 *  @return         対応する強制出力のトレース レベルです。
 *
 *  すでに強制出力のレベルを渡した場合の結果は規定しません。
 */
#define CPLAT_TRACE_LEVEL_TO_FORCE(level) ((cplat_trace_level)((int)(level) - CPLAT_TRACE_LEVEL_FORCE_OFFSET))

/**
 *  @brief          強制出力のトレース レベルを、通常のトレース レベルへ変換します。
 *  @param[in]      level 変換する強制出力のトレース レベル。
 *  @return         対応する通常のトレース レベルです。
 *
 *  通常のレベルを渡した場合の結果は規定しません。
 */
#define CPLAT_TRACE_LEVEL_FROM_FORCE(level) ((cplat_trace_level)((int)(level) + CPLAT_TRACE_LEVEL_FORCE_OFFSET))

/**
 *  @brief          トレース レベルが強制出力のレベルかどうかを判定します。
 *  @param[in]      level 判定するトレース レベル。
 *  @return         強制出力のレベルであれば 0 以外、そうでなければ 0 を返します。
 */
#define CPLAT_TRACE_LEVEL_IS_FORCE(level) ((int)(level) < 0)

/**
 *  @enum           cplat_tracer_state
 *  @brief          tracer handle のライフサイクル状態です。
 */
typedef enum cplat_tracer_state
{
    CPLAT_TRACER_STATE_STOPPED = 0, /**< 作成済みで停止中。 */
    CPLAT_TRACER_STATE_STARTED = 1, /**< 作成済みで実行中。 */
    CPLAT_TRACER_STATE_DISPOSED = 2 /**< 利用不可または解放済み。 */
} cplat_tracer_state;

/**
 *  @enum           cplat_tracer_concurrency_mode
 *  @brief          同一 tracer handle への並行呼び出しを管理する主体を指定します。
 *
 *  cplat_tracer_create に渡したモードはハンドルの生存期間中に変更できません。
 *  このモードが制御するのは同一ハンドルの状態と設定へのアクセスです。
 *  異なるハンドル間で共有されるレジストリや OS バックエンドは、どちらのモードでも tracer が同期します。
 *
 *  CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED では、tracer はハンドル専用の同期オブジェクトを生成しません。
 *  同一ハンドルに対する start、stop、設定、出力、状態取得、フック操作、dispose が互いに重ならないよう、
 *  呼び出し側が単一スレッドで使用するか、外部の同期機構で直列化してください。
 *
 *  CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED では、tracer がハンドル専用の同期オブジェクトを生成し、
 *  通常の API 呼び出しを調停します。同期オブジェクトの所有権は tracer にあり、利用者は取得または破棄できません。
 *  ただし、cplat_tracer_dispose と同一ハンドルの他の API を並行して呼び出すことはできません。
 */
typedef enum cplat_tracer_concurrency_mode
{
    /**
     *  同一ハンドルへの呼び出しを利用者が直列化します。
     *  tracer はハンドル専用の同期オブジェクトを生成しません。
     */
    CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED = 0,

    /**
     *  同一ハンドルへの通常の呼び出しを tracer が調停します。
     *  tracer はハンドル専用の同期オブジェクトを生成し、dispose またはプロセス shutdown で破棄します。
     */
    CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED = 1
} cplat_tracer_concurrency_mode;

/* ===== 既定のトレース レベル ===== */

/*
 *  すべての出力先の既定のトレース レベルを CPLAT_TRACE_LEVEL_NONE (無効) とします。
 *  利用者が出力先ごとに詳細度を設定するまで、トレースは副作用を持ちません。
 *  ログ ファイルの生成のような副作用を、利用者の意図しないうちに起こさないための方針です。
 */

/**
 *  @brief          cplat_tracer_create() が設定する OS トレース (EventLog / syslog) の既定のレベルです。
 *
 *  OS トレースは Windows ではイベント ログ (EventLog)、Linux では syslog を指します。\n
 *  運用者が参照する OS ネイティブの運用ログであり、ユーザーが
 *  cplat_tracer_set_os_level() で変更するまで有効な初期値です。\n
 *  既定値は CPLAT_TRACE_LEVEL_NONE (無効) です。
 */
#define CPLAT_TRACER_DEFAULT_OS_LEVEL CPLAT_TRACE_LEVEL_NONE

/**
 *  @brief          cplat_tracer_create() が設定する ETW トレースの既定のレベルです。
 *
 *  ETW (Event Tracing for Windows) は開発者向けの低オーバーヘッド診断チャネルであり、
 *  OS トレース (EventLog) とは独立した軸として制御します。\n
 *  ユーザーが cplat_tracer_set_etw_level() で変更するまで有効な初期値です。\n
 *  既定値は CPLAT_TRACE_LEVEL_NONE (無効) です。\n
 *  本定義は Windows でのみ意味を持ちます。Linux では ETW は存在せず、
 *  cplat_tracer_set_etw_level() / cplat_tracer_get_etw_level() は何もしません。
 */
#define CPLAT_TRACER_DEFAULT_ETW_LEVEL CPLAT_TRACE_LEVEL_NONE

/**
 *  @brief          cplat_tracer_create() が設定するファイル トレースの既定のレベルです。
 *
 *  ユーザーが cplat_tracer_set_file_level() で変更するまで有効な初期値です。\n
 *  既定値は CPLAT_TRACE_LEVEL_NONE (無効) であり、
 *  cplat_tracer_set_file_level() を呼び出さない場合、cplat_tracer_start() は
 *  ファイルを作成しません。
 */
#define CPLAT_TRACER_DEFAULT_FILE_LEVEL CPLAT_TRACE_LEVEL_NONE

/**
 *  @brief          cplat_tracer_create() が設定する stderr トレースの既定のレベルです。
 *
 *  ユーザーが cplat_tracer_set_stderr_level() で変更するまで有効な初期値です。\n
 *  既定値は CPLAT_TRACE_LEVEL_NONE (無効) です。
 */
#define CPLAT_TRACER_DEFAULT_STDERR_LEVEL CPLAT_TRACE_LEVEL_NONE

/* ===== 不透明ハンドル型 ===== */

/** トレース プロバイダー ハンドル (不透明型)。 */
typedef struct cplat_tracer cplat_tracer;

/* ===== フック (コールバック) ===== */

/**
 *  @brief  トレース フック エントリ (不透明型) です。
 *
 *  cplat_tracer_set_hook が返す不透明ハンドル。\n
 *  cplat_tracer_remove_hook および cplat_tracer_call_next_hook に渡して使用します。
 */
typedef struct cplat_tracer_hook_entry cplat_tracer_hook_entry;

/**
 *  @brief  トレース フックのコールバック関数型です。
 *
 *  @param[in]  prev      チェーン継続に使う前エントリ。cplat_tracer_call_next_hook に渡します。
 *  @param[in]  handle    trace を行った tracer ハンドル。
 *  @param[in]  level     trace レベル (CPLAT_TRACE_LEVEL_NONE を含む全レベル)。
 *  @param[in]  timestamp 解決済みタイムスタンプ (常に有効)。
 *  @param[in]  message   解決済みメッセージ文字列。
 *  @param[in]  context   cplat_tracer_set_hook で渡したコンテキスト。
 *
 *  @par        チェーン例
    @code{.c}
    void my_hook(cplat_tracer_hook_entry *prev,
                 cplat_tracer *handle,
                 cplat_trace_level level,
                 const cplat_timespec *timestamp,
                 const char *message, void *context)
    {
        // 独自処理
        printf("hook: %s\n", message);
        // 前のフックへ継続 (省略すると以降のチェーンは呼び出されない)
        cplat_tracer_call_next_hook(prev, handle, level, timestamp, message);
    }
    @endcode
 *
 *  @par            スレッド セーフ
 *  コールバックは複数スレッドから同時に呼び出される可能性があります。\n
 *  コールバックの実装者は再入性を確保してください。
 */
typedef void (*cplat_tracer_hook_fn)(cplat_tracer_hook_entry *prev, cplat_tracer *handle, cplat_trace_level level,
                                     const cplat_timespec *timestamp, const char *message, void *context);

/* ===== API 関数 ===== */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          トレース プロバイダーを初期化します。
     *
     *  自プロセスの実行ファイル名を既定の識別名として初期化します
     *  (例: Linux `/usr/bin/myapp` → `"myapp"`,
     *  Windows `C:\bin\myapp.exe` → `"myapp.exe"`)。\n
     *  プロセス名の取得に失敗した場合は `"unknown"` を使用します。\n
     *  Linux 環境では syslog を LOG_USER facility で初期化します。\n
     *  Windows 環境ではライブラリ内蔵の ETW 既定プロバイダー
     *  (`CPLAT_TRACER_DEFAULT_PROVIDER_NAME`) を使用します。\n
     *  識別名を変更するには cplat_tracer_set_name を呼び出してください。
     *
     *  既定の出力先はファイル トレースのみです
     *  (OS トレースと stderr トレースの既定のレベルは CPLAT_TRACE_LEVEL_NONE)。\n
     *  ファイル トレースの出力先は既定で実行ファイルのディレクトリ配下の
     *  `log/{ファイル名}.log` であり、占有モード、最大
     *  CPLAT_TRACE_FILE_SINK_DEFAULT_MAX_BYTES バイト、
     *  CPLAT_TRACE_FILE_SINK_DEFAULT_GENERATIONS 世代で運用されます。
     *  ファイル名の既定値はプロセス名 (実行ファイルのベース名。Windows は末尾の `.exe` を除く) です。\n
     *  パスとパラメーターは cplat_tracer_set_file_level で、
     *  ファイル名とファイル識別は cplat_tracer_set_file_name で変更できます。
     *
     *  識別名 (インスタンス名とインスタンス識別) とトレース ファイル名 (ファイル名とファイル識別) は
     *  独立して管理されます。cplat_tracer_set_name はトレース ファイル名に影響しません。
     *
     *  @param[in]      concurrency_mode  同一ハンドルへの並行呼び出しを管理する主体。
     *                                    CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED または
     *                                    CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED を指定します。
     *  @return         成功時は stopped 状態のハンドルを返します。メモリ確保または同期オブジェクトの
     *                  初期化に失敗した場合、または concurrency_mode が未定義値の場合は NULL を返します。
     *
     *  @post           戻り値のハンドルは stopped 状態です。
     *                  出力関数を使用するには cplat_tracer_start を呼び出してください。\n
     *                  識別子・ファイル名・フックの設定関数 (cplat_tracer_set_name,
     *                  cplat_tracer_set_file_name, cplat_tracer_set_hook, cplat_tracer_remove_hook) は
     *                  stopped 状態でのみスレッド安全に使用できます。\n
     *                  レベル設定関数 (cplat_tracer_set_os_level, cplat_tracer_set_etw_level,
     *                  cplat_tracer_set_file_level, cplat_tracer_set_stderr_level) は
     *                  stopped / started のどちらでも使用できます。
     *
     *  @par            使用例
        @code{.c}
       cplat_tracer *tracer = cplat_tracer_create(CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED);
       cplat_tracer_set_name(tracer, "myapp", 0);
       cplat_tracer_start(tracer);
       cplat_tracer_write(tracer, CPLAT_TRACE_LEVEL_INFO, NULL, "application started");
       cplat_tracer_stop(tracer);
       cplat_tracer_dispose(&tracer);
        @endcode
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  Linux 環境では、内部で生成する syslog プロバイダーが環境変数を参照します (cplat_syslog_sink_create() を参照)。\n
     *  他スレッドが環境変数を同時に変更しない場合は、同時に実行できます。\n
     *  他スレッドが同時に環境変数を変更する場合は、呼び出し側で同期してください。
     *
     *  @attention      CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED を指定した場合、同一ハンドルへの
     *                  API 呼び出しを利用者が直列化する必要があります。
     *  @attention      どちらのモードでも、cplat_tracer_dispose と同一ハンドルの他の API を
     *                  並行して呼び出してはなりません。
     */
    CPLAT_EXPORT cplat_tracer *CPLAT_API cplat_tracer_create(cplat_tracer_concurrency_mode concurrency_mode);

    /**
     *  @brief          トレース プロバイダーを開始します。
     *
     *  ハンドルを実行中 (started) 状態に遷移させます。\n
     *  started 状態では出力関数 (cplat_tracer_write 等) が有効になります。\n
     *  レベル設定関数 (cplat_tracer_set_os_level, cplat_tracer_set_etw_level,
     *  cplat_tracer_set_file_level, cplat_tracer_set_stderr_level) は started 状態でも使用でき、
     *  停止せずにしきい値レベルを変更できます。\n
     *  識別子・ファイル名・フックの設定関数 (cplat_tracer_set_name, cplat_tracer_set_file_name,
     *  cplat_tracer_set_hook, cplat_tracer_remove_hook) は started 状態では使用できません (@ref CPLAT_ERR_UNKNOWN / NULL を返します)。\n
     *  すでに started 状態の場合は何もせず @ref CPLAT_OK を返します (べき等)。
     *
     *  ファイル トレースのレベルが CPLAT_TRACE_LEVEL_NONE 以外の場合、
     *  本関数の呼び出し時点の設定 (出力ファイル パス、ファイル名、ファイル識別) で
     *  トレース ファイルを開きます。\n
     *  出力ファイル パスの妥当性 (オープン可否) は本関数の戻り値で報告されます。\n
     *  トレース ファイルを開けなかった場合も started 状態へは遷移し、
     *  ファイル以外のトレース出力 (OS / stderr / フック) は継続したうえで @ref CPLAT_ERR_UNKNOWN を返します。
     *  この場合、cplat_tracer_stop 後に再度本関数を呼び出すとオープンを再試行します。
     *
     *  同一プロセス内の複数の tracer が同一パスのトレース ファイルを開いた場合は、
     *  プロセス内で同一ファイルへの書き込みが調停されるため、占有モードでも併用できます
     *  (詳細は cplat_trace_file_sink_create を参照)。
     *
     *  @param[in]      handle   cplat_tracer_create の戻り値。NULL を渡してはなりません。
     *  @return         成功時は @ref CPLAT_OK を返します。
     *  @return         @p handle が NULL の場合、または開始処理に失敗した場合は @ref CPLAT_ERR_UNKNOWN を返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p handle に対する操作は同時に実行できます。\n
     *  同一 @p handle に対する操作は、CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED で生成したハンドルでは同時に実行できます。CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED では、呼び出し側で直列化してください。\n
     *  内部で排他制御を行います。
     *
     *  @warning        別プロセスとの間では占有モードの排他が働くため、同一実行ファイルを複数プロセス
     *                  起動すると既定パスのオープンが 2 プロセス目以降で失敗する場合があります
     *                  (Windows)。cplat_tracer_set_file_name のファイル識別、または
     *                  cplat_tracer_set_file_level の明示パスでプロセスごとにファイルを分けてください。
     *
     *  @see            cplat_tracer_stop
     */
    CPLAT_EXPORT int CPLAT_API cplat_tracer_start(cplat_tracer *handle);

    /**
     *  @brief          トレース プロバイダーを停止します。
     *
     *  ハンドルを停止中 (stopped) 状態に遷移させます。\n
     *  stopped 状態では出力関数 (cplat_tracer_write 等) は @ref CPLAT_ERR_UNKNOWN を返し、
     *  識別子・ファイル名・フックの設定関数 (cplat_tracer_set_name, cplat_tracer_set_file_name,
     *  cplat_tracer_set_hook, cplat_tracer_remove_hook) がスレッド セーフに使用できるようになります。\n
     *  レベル設定関数 (cplat_tracer_set_os_level 等) は stopped / started のどちらでも使用できます。\n
     *  ファイル トレースが有効な場合、開いていたトレース ファイルを閉じます。
     *  ファイル トレースの設定は保持され、次回の cplat_tracer_start で改めてファイルを開きます。\n
     *  すでに stopped 状態の場合は何もせず @ref CPLAT_OK を返します (べき等)。
     *
     *  @param[in]      handle   cplat_tracer_create の戻り値。NULL を渡してはなりません。
     *  @return         成功時は @ref CPLAT_OK を返します。
     *  @return         @p handle が NULL の場合は @ref CPLAT_ERR_UNKNOWN を返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p handle に対する操作は同時に実行できます。\n
     *  同一 @p handle に対する操作は、CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED で生成したハンドルでは同時に実行できます。CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED では、呼び出し側で直列化してください。\n
     *  内部で排他制御を行います。
     *
     *  @see            cplat_tracer_start
     */
    CPLAT_EXPORT int CPLAT_API cplat_tracer_stop(cplat_tracer *handle);

    /**
     *  @brief          tracer handle の現在状態を取得します。
     *
     *  create 直後および stop 後は stopped、start 後は started を返します。\n
     *  handle が NULL、解放済み、または shutdown 中で利用できない場合は disposed を返します。\n
     *  dispose 実行後のポインター再利用は未定義動作のため、disposed 判定には NULL または
     *  呼び出し側が保持するセッション状態を使用してください。
     *
     *  @param[in]      handle   cplat_tracer_create の戻り値。NULL 可。
     *  @return         現在の状態 (cplat_tracer_state)。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p handle に対する操作は同時に実行できます。\n
     *  同一 @p handle に対する操作は、CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED で生成したハンドルでは同時に実行できます。CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED では、呼び出し側で直列化してください。\n
     *  返される状態は取得時点のスナップショットです。呼び出し直後に状態が変化する場合があります。
     */
    CPLAT_EXPORT cplat_tracer_state CPLAT_API cplat_tracer_get_state(cplat_tracer *handle);

    /**
     *  @brief          トレース メッセージを書き込む低レベル関数です。
     *
     *  @param[in]      handle     cplat_tracer_create の戻り値。
     *  @param[in]      level      トレース レベル (cplat_trace_level)。
     *  @param[in]      timestamp  使用する実時刻。NULL の場合は API 内部で現在時刻を取得。
     *                             不正な明示タイムスタンプが渡された場合も現在時刻で出力を継続し、
     *                             戻り値は @ref CPLAT_ERR_UNKNOWN を返します。
     *  @param[in]      message    null 終端 UTF-8 文字列。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p handle に対する操作は同時に実行できます。\n
     *  同一 @p handle に対する操作は、CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED で生成したハンドルでは同時に実行できます。CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED では、呼び出し側で直列化してください。\n
     *  内部で共有ロックを取得して設定を参照し、複数スレッドから同時に呼び出せます。
     */
    CPLAT_EXPORT int CPLAT_API cplat_tracer_write_at(cplat_tracer *handle, cplat_trace_level level,
                                                     const cplat_timespec *timestamp, const char *message);

    /**
     *  @brief          printf 形式でトレース メッセージを書き込む低レベル関数です。
     *
     *  @param[in]      handle     cplat_tracer_create の戻り値。
     *  @param[in]      level      トレース レベル (cplat_trace_level)。
     *  @param[in]      timestamp  使用する実時刻。NULL の場合は API 内部で現在時刻を取得。
     *                             不正な明示タイムスタンプが渡された場合も現在時刻で出力を継続し、
     *                             戻り値は @ref CPLAT_ERR_UNKNOWN を返します。
     *  @param[in]      format     printf 形式のフォーマット文字列。
     *  @param[in]      ...        フォーマット文字列に対応する可変長引数。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p handle に対する操作は同時に実行できます。\n
     *  同一 @p handle に対する操作は、CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED で生成したハンドルでは同時に実行できます。CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED では、呼び出し側で直列化してください。\n
     *  内部で共有ロックを取得して設定を参照し、複数スレッドから同時に呼び出せます。
     */
    CPLAT_EXPORT int CPLAT_API cplat_tracer_writef_at(cplat_tracer *handle, cplat_trace_level level,
                                                      const cplat_timespec *timestamp, const char *format, ...);

    /**
     *  @brief          書式付きメッセージをトレースに書き込む低レベル関数 (`cplat_tracer_writef_at` の `va_list` 版) です。
     *
     *  @param[in]      handle     cplat_tracer_create の戻り値。
     *  @param[in]      level      トレース レベル (cplat_trace_level)。
     *  @param[in]      timestamp  使用する実時刻。NULL の場合は API 内部で現在時刻を取得。
     *  @param[in]      format     printf 形式のフォーマット文字列。
     *  @param[in]      args       フォーマット文字列に対応する引数リスト。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p handle に対する操作は同時に実行できます。\n
     *  同一 @p handle に対する操作は、CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED で生成したハンドルでは同時に実行できます。CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED では、呼び出し側で直列化してください。\n
     *  内部で共有ロックを取得して設定を参照し、複数スレッドから同時に呼び出せます。
     */
    CPLAT_EXPORT int CPLAT_API cplat_tracer_vwritef_at(cplat_tracer *handle, cplat_trace_level level,
                                                       const cplat_timespec *timestamp, const char *format,
                                                       va_list args);

    /**
     *  @brief          バイナリ データを HEX テキスト形式でトレースに書き込む低レベル関数です。
     *
     *  @param[in]      handle     cplat_tracer_create の戻り値。
     *  @param[in]      level      トレース レベル (cplat_trace_level)。
     *  @param[in]      timestamp  使用する実時刻。NULL の場合は API 内部で現在時刻を取得。
     *                             不正な明示タイムスタンプが渡された場合も現在時刻で出力を継続し、
     *                             戻り値は @ref CPLAT_ERR_UNKNOWN を返します。
     *  @param[in]      data       バイナリ データへのポインター。
     *  @param[in]      size       バイナリ データのバイト数。
     *  @param[in]      message    HEX データの手前に付与するラベル文字列。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p handle に対する操作は同時に実行できます。\n
     *  同一 @p handle に対する操作は、CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED で生成したハンドルでは同時に実行できます。CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED では、呼び出し側で直列化してください。\n
     *  内部で共有ロックを取得して設定を参照し、複数スレッドから同時に呼び出せます。
     */
    CPLAT_EXPORT int CPLAT_API cplat_tracer_write_hex_at(cplat_tracer *handle, cplat_trace_level level,
                                                         const cplat_timespec *timestamp, const void *data, size_t size,
                                                         const char *message);

    /**
     *  @brief          バイナリ データを HEX テキスト形式でトレースに書き込む低レベル関数 (printf 形式ラベル) です。
     *
     *  @param[in]      handle     cplat_tracer_create の戻り値。
     *  @param[in]      level      トレース レベル (cplat_trace_level)。
     *  @param[in]      timestamp  使用する実時刻。NULL の場合は API 内部で現在時刻を取得。
     *                             不正な明示タイムスタンプが渡された場合も現在時刻で出力を継続し、
     *                             戻り値は @ref CPLAT_ERR_UNKNOWN を返します。
     *  @param[in]      data       バイナリ データへのポインター。
     *  @param[in]      size       バイナリ データのバイト数。
     *  @param[in]      format     printf 形式のフォーマット文字列 (ラベル)。NULL 可。
     *  @param[in]      ...        フォーマット文字列に対応する可変長引数。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p handle に対する操作は同時に実行できます。\n
     *  同一 @p handle に対する操作は、CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED で生成したハンドルでは同時に実行できます。CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED では、呼び出し側で直列化してください。\n
     *  内部で共有ロックを取得して設定を参照し、複数スレッドから同時に呼び出せます。
     */
    CPLAT_EXPORT int CPLAT_API cplat_tracer_write_hexf_at(cplat_tracer *handle, cplat_trace_level level,
                                                          const cplat_timespec *timestamp, const void *data,
                                                          size_t size, const char *format, ...);

    /**
     *  @brief          バイナリ データを HEX テキスト形式で書き込む低レベル関数 (`cplat_tracer_write_hexf_at` の `va_list` 版) です。
     *
     *  @param[in]      handle     cplat_tracer_create の戻り値。
     *  @param[in]      level      トレース レベル (cplat_trace_level)。
     *  @param[in]      timestamp  使用する実時刻。NULL の場合は API 内部で現在時刻を取得。
     *  @param[in]      data       バイナリ データへのポインター。
     *  @param[in]      size       バイナリ データのバイト数。
     *  @param[in]      format     printf 形式のフォーマット文字列 (ラベル)。NULL 可。
     *  @param[in]      args       フォーマット文字列に対応する引数リスト。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p handle に対する操作は同時に実行できます。\n
     *  同一 @p handle に対する操作は、CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED で生成したハンドルでは同時に実行できます。CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED では、呼び出し側で直列化してください。\n
     *  内部で共有ロックを取得して設定を参照し、複数スレッドから同時に呼び出せます。
     */
    CPLAT_EXPORT int CPLAT_API cplat_tracer_vwrite_hexf_at(cplat_tracer *handle, cplat_trace_level level,
                                                           const cplat_timespec *timestamp, const void *data,
                                                           size_t size, const char *format, va_list args);

    /**
     *  @brief          トレース プロバイダーのインスタンス名とインスタンス識別を設定します。
     *
     *  OS トレース (syslog ident / EventLog のインスタンス名) と ETW (サービス名) で
     *  使用する識別名を設定します。
     *  識別名は `{name}` (identifier が 0 の場合) または `{name}_{identifier}` です。\n
     *  EventLog はソースが cplat 共通のため、本識別名を本文先頭に付与して
     *  インスタンスを判別可能にします。\n
     *  本関数はトレース ファイル名には影響しません。トレース ファイル名とファイル識別は
     *  cplat_tracer_set_file_name で独立して設定します。
     *
     *  @param[in]      handle      cplat_tracer_create の戻り値。
     *  @param[in]      name        インスタンス名。NULL で自プロセス名を使用。
     *  @param[in]      identifier  インスタンス識別番号 (0 以上)。0 でサフィックスなし。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_OUT_OF_MEMORY 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p handle に対する操作は同時に実行できます。\n
     *  同一 @p handle に対する操作は、CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED で生成したハンドルでは同時に実行できます。CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED では、呼び出し側で直列化してください。\n
     *  stopped 状態でのみ有効です。started 状態では @ref CPLAT_ERR_UNKNOWN を返します。
     *
     *  @see            cplat_tracer_get_name
     *  @see            cplat_tracer_get_identifier
     *  @see            cplat_tracer_set_file_name
     */
    CPLAT_EXPORT int CPLAT_API cplat_tracer_set_name(cplat_tracer *handle, const char *name, int64_t identifier);

    /**
     *  @brief          解決済みのインスタンス名 (識別番号サフィックス込み) を取得します。
     *
     *  OS トレース (syslog ident / EventLog のインスタンス名) と ETW (サービス名) で
     *  実際に使用される識別名を返します。\n
     *  cplat_tracer_set_name 未呼び出しの場合は自プロセス名です。
     *
     *  @param[in]      handle      cplat_tracer_create の戻り値。
     *  @param[out]     name_out    識別名を格納するバッファー。NULL の場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *  @param[in]      name_size   バッファーのバイト数。0 の場合は @ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                            格納に不足する場合は @ref CPLAT_ERR_BUFFER_TOO_SMALL を返します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_BUFFER_TOO_SMALL 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p handle に対する操作は同時に実行できます。\n
     *  同一 @p handle に対する操作は、CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED で生成したハンドルでは同時に実行できます。CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED では、呼び出し側で直列化してください。\n
     *  stopped / started のどちらの状態でも使用できます。
     *
     *  @see            cplat_tracer_set_name
     */
    CPLAT_EXPORT int CPLAT_API cplat_tracer_get_name(cplat_tracer *handle, char *name_out, size_t name_size);

    /**
     *  @brief          インスタンス識別番号を取得します。
     *
     *  @param[in]      handle    cplat_tracer_create の戻り値。
     *  @return         現在のインスタンス識別番号 (0 以上)。handle が NULL または利用不可の場合 -1。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p handle に対する操作は同時に実行できます。\n
     *  同一 @p handle に対する操作は、CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED で生成したハンドルでは同時に実行できます。CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED では、呼び出し側で直列化してください。\n
     *  stopped / started のどちらの状態でも使用できます。
     *
     *  @see            cplat_tracer_set_name
     */
    CPLAT_EXPORT int64_t CPLAT_API cplat_tracer_get_identifier(cplat_tracer *handle);

    /**
     *  @brief          トレース ファイル名とファイル識別を設定します。
     *
     *  ファイル トレースの既定パス (実行ファイルのディレクトリ配下の
     *  `log/{ファイル名}.log`) に使用するファイル名を設定します。
     *  ファイル名は `{name}` (identifier が 0 の場合) または `{name}_{identifier}` です。\n
     *  name に NULL を指定した場合はプロセス名 (実行ファイルのベース名。Windows は末尾の
     *  `.exe` を除く) を使用します。明示設定した名前には `.exe` の除去を適用しません。\n
     *  本関数は OS トレースの識別名 (cplat_tracer_set_name) には影響しません。\n
     *  cplat_tracer_set_file_level で出力ファイル パスを明示設定している場合、
     *  本設定は既定パスの解決に使用されないため効果を持ちません。
     *
     *  @param[in]      handle      cplat_tracer_create の戻り値。
     *  @param[in]      name        ファイル名。NULL でプロセス名を使用。
     *  @param[in]      identifier  ファイル識別番号 (0 以上)。0 でサフィックスなし。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_OUT_OF_MEMORY 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p handle に対する操作は同時に実行できます。\n
     *  同一 @p handle に対する操作は、CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED で生成したハンドルでは同時に実行できます。CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED では、呼び出し側で直列化してください。\n
     *  stopped 状態でのみ有効です。started 状態では @ref CPLAT_ERR_UNKNOWN を返します。
     *
     *  @see            cplat_tracer_get_file_name
     *  @see            cplat_tracer_get_file_identifier
     *  @see            cplat_tracer_set_file_level
     */
    CPLAT_EXPORT int CPLAT_API cplat_tracer_set_file_name(cplat_tracer *handle, const char *name, int64_t identifier);

    /**
     *  @brief          解決済みのトレース ファイル名 (ファイル識別サフィックス込み) を取得します。
     *
     *  ファイル トレースの既定パスで実際に使用されるファイル名
     *  (拡張子 `.log` を除く) を返します。\n
     *  cplat_tracer_set_file_name 未呼び出しの場合はプロセス名
     *  (実行ファイルのベース名。Windows は末尾の `.exe` を除く) です。
     *
     *  @param[in]      handle          cplat_tracer_create の戻り値。
     *  @param[out]     file_name_out   ファイル名を格納するバッファー。NULL の場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *  @param[in]      file_name_size  バッファーのバイト数。0 の場合は @ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                                 格納に不足する場合は @ref CPLAT_ERR_BUFFER_TOO_SMALL を返します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_BUFFER_TOO_SMALL 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p handle に対する操作は同時に実行できます。\n
     *  同一 @p handle に対する操作は、CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED で生成したハンドルでは同時に実行できます。CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED では、呼び出し側で直列化してください。\n
     *  stopped / started のどちらの状態でも使用できます。
     *
     *  @see            cplat_tracer_set_file_name
     */
    CPLAT_EXPORT int CPLAT_API cplat_tracer_get_file_name(cplat_tracer *handle, char *file_name_out,
                                                          size_t file_name_size);

    /**
     *  @brief          ファイル識別番号を取得します。
     *
     *  @param[in]      handle    cplat_tracer_create の戻り値。
     *  @return         現在のファイル識別番号 (0 以上)。handle が NULL または利用不可の場合 -1。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p handle に対する操作は同時に実行できます。\n
     *  同一 @p handle に対する操作は、CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED で生成したハンドルでは同時に実行できます。CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED では、呼び出し側で直列化してください。\n
     *  stopped / started のどちらの状態でも使用できます。
     *
     *  @see            cplat_tracer_set_file_name
     */
    CPLAT_EXPORT int64_t CPLAT_API cplat_tracer_get_file_identifier(cplat_tracer *handle);

    /**
     *  @brief          OS トレース (EventLog / syslog) の現在のスレッショルド レベルを取得します。
     *
     *  OS トレースは Windows ではイベント ログ (EventLog)、Linux では syslog を指します。
     *
     *  @param[in]      handle   cplat_tracer_create の戻り値。
     *  @return         現在のスレッショルド レベル。handle が NULL 時は CPLAT_TRACE_LEVEL_NONE。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p handle に対する操作は同時に実行できます。\n
     *  同一 @p handle に対する操作は、CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED で生成したハンドルでは同時に実行できます。CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED では、呼び出し側で直列化してください。\n
     *  返される値は取得時点のスナップショットです。
     */
    CPLAT_EXPORT cplat_trace_level CPLAT_API cplat_tracer_get_os_level(cplat_tracer *handle);

    /**
     *  @brief          OS トレース (EventLog / syslog) のスレッショルド レベルを設定します。
     *
     *  OS トレースは Windows ではイベント ログ (EventLog)、Linux では syslog を指します。
     *
     *  @param[in]      handle   cplat_tracer_create の戻り値。
     *  @param[in]      level    新しいスレッショルド レベル (cplat_trace_level)。
     *                           強制出力のレベルは指定できません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_UNKNOWN の
     *                  いずれかを返します。
     *  @retval         CPLAT_ERR_INVALID_ARGUMENT  level が通常のトレース レベルの範囲外です。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p handle に対する操作は同時に実行できます。\n
     *  同一 @p handle に対する操作は、CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED で生成したハンドルでは同時に実行できます。CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED では、呼び出し側で直列化してください。\n
     *  stopped / started のどちらの状態でも有効です。変更は排他制御下で原子的に反映され、
     *  旧閾値と新閾値の両方で出力対象となるトレースを取りこぼしません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_tracer_set_os_level(cplat_tracer *handle, cplat_trace_level level);

    /**
     *  @brief          ETW トレースの現在のスレッショルド レベルを取得します。
     *
     *  ETW は Windows 専用の独立した診断チャネルです。\n
     *  Linux では ETW が存在しないため、常に CPLAT_TRACE_LEVEL_NONE を返します。
     *
     *  @param[in]      handle   cplat_tracer_create の戻り値。
     *  @return         現在のスレッショルド レベル。handle が NULL 時または Linux では
     *                  CPLAT_TRACE_LEVEL_NONE。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p handle に対する操作は同時に実行できます。\n
     *  同一 @p handle に対する操作は、CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED で生成したハンドルでは同時に実行できます。CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED では、呼び出し側で直列化してください。\n
     *  返される値は取得時点のスナップショットです。
     */
    CPLAT_EXPORT cplat_trace_level CPLAT_API cplat_tracer_get_etw_level(cplat_tracer *handle);

    /**
     *  @brief          ETW トレースのスレッショルド レベルを設定します。
     *
     *  ETW は Windows 専用の独立した診断チャネルであり、OS トレース (EventLog) とは
     *  別の軸として制御します。既定値は CPLAT_TRACER_DEFAULT_ETW_LEVEL です。\n
     *  Linux では ETW が存在しないため、本関数は何もせず @ref CPLAT_OK を返します。
     *
     *  @param[in]      handle   cplat_tracer_create の戻り値。
     *  @param[in]      level    新しいスレッショルド レベル (cplat_trace_level)。
     *                           強制出力のレベルは指定できません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_UNKNOWN の
     *                  いずれかを返します。
     *  @retval         CPLAT_ERR_INVALID_ARGUMENT  level が通常のトレース レベルの範囲外です。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p handle に対する操作は同時に実行できます。\n
     *  同一 @p handle に対する操作は、CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED で生成したハンドルでは同時に実行できます。CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED では、呼び出し側で直列化してください。\n
     *  stopped / started のどちらの状態でも有効です。変更は排他制御下で原子的に反映され、
     *  旧閾値と新閾値の両方で出力対象となるトレースを取りこぼしません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_tracer_set_etw_level(cplat_tracer *handle, cplat_trace_level level);

    /**
     *  @brief          ファイル トレースの現在のスレッショルド レベルを取得します。
     *
     *  @param[in]      handle   cplat_tracer_create の戻り値。
     *  @return         現在のスレッショルド レベル。handle が NULL 時は CPLAT_TRACE_LEVEL_NONE。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p handle に対する操作は同時に実行できます。\n
     *  同一 @p handle に対する操作は、CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED で生成したハンドルでは同時に実行できます。CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED では、呼び出し側で直列化してください。\n
     *  返される値は取得時点のスナップショットです。
     */
    CPLAT_EXPORT cplat_trace_level CPLAT_API cplat_tracer_get_file_level(cplat_tracer *handle);

    /**
     *  @brief          ファイル トレースの出力先と設定を変更します。
     *
     *  本関数は設定の記録のみを行い、トレース ファイルは cplat_tracer_start 時に開かれます。\n
     *  そのため、出力ファイル パスの妥当性 (オープン可否) は本関数ではなく
     *  cplat_tracer_start の戻り値で報告されます。
     *
     *  path に NULL を指定した場合は既定パスを使用します。
     *  既定パスは実行ファイルのディレクトリ配下の `log/{ファイル名}.log` であり、
     *  ファイル名は start 時点の設定 (cplat_tracer_set_file_name のファイル名とファイル識別)
     *  で解決されます。ファイル名の既定値はプロセス名です
     *  (例: `myapp` または Windows の `myapp.exe` → `log/myapp.log`)。\n
     *  実行ファイル パスの取得に失敗した場合は、カレント ディレクトリからの相対パス
     *  `log/{ファイル名}.log` へ出力します。\n
     *  ファイル トレースを無効化するには level に CPLAT_TRACE_LEVEL_NONE を指定します。
     *
     *  flags は cplat_trace_file_sink_create にそのまま渡されます。\n
     *  flags に 0 を指定した場合、ファイル トレースは単一プロセス専用になります。
     *  このモードは OS の排他的オープンを使用しないため、呼び出し側が他プロセスから
     *  同一パスへ書き込まないことを保証してください。\n
     *  @ref CPLAT_TRACE_FILE_SINK_SHARED を指定すると、
     *  複数プロセスから同一パスへ書き込むための調停を有効にします。
     *  @ref CPLAT_TRACE_FILE_SINK_OS_BUFFERED を指定すると、OS の書き込みキャッシュを使用し、
     *  各書き込みの永続媒体への即時反映を要求しません。
     *  詳細は cplat_trace_file_sink_create を参照してください。
     *
     *  @param[in]      handle       cplat_tracer_create の戻り値。
     *  @param[in]      path         出力ファイル パス。NULL で既定パスを使用。
     *  @param[in]      level        ファイル トレースのスレッショルド レベル。
     *                               CPLAT_TRACE_LEVEL_NONE でファイル トレースを無効化。
     *                               強制出力のレベルは指定できません。
     *  @param[in]      max_bytes    1 ファイルあたりの最大バイト数。0 で既定値を使用。
     *  @param[in]      generations  保持する旧世代数。0 以下で既定値を使用。
     *  @param[in]      flags        ファイル sink の動作フラグの OR 結合、または 0。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_OUT_OF_MEMORY 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *  @retval         CPLAT_ERR_INVALID_ARGUMENT  level が通常のトレース レベルの範囲外です。
     *
     *  @par            started 中の即時反映
     *  started 状態でも呼び出せます。この場合は設定を記録するだけでなく、変更を即座に反映します。\n
     *  level に CPLAT_TRACE_LEVEL_NONE を指定するとファイル出力を停止します。\n
     *  出力ファイル パスや max_bytes / generations / flags を変更した場合 (または無効状態から
     *  有効化した場合) は、新しい設定でトレース ファイルを開き直します。新しいファイルのオープンに
     *  失敗した場合は、開いていたファイルと従来の設定を保持したまま @ref CPLAT_ERR_UNKNOWN を返します。\n
     *  パスとパラメーターが現状と一致し、しきい値レベルのみを変更する場合はファイルを開き直しません。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p handle に対する操作は同時に実行できます。\n
     *  同一 @p handle に対する操作は、CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED で生成したハンドルでは同時に実行できます。CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED では、呼び出し側で直列化してください。\n
     *  stopped / started のどちらの状態でも有効です。変更は排他制御下で原子的に反映され、
     *  旧閾値と新閾値の両方で出力対象となるトレースを取りこぼしません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_tracer_set_file_level(cplat_tracer *handle, const char *path,
                                                           cplat_trace_level level, size_t max_bytes, int generations,
                                                           int flags);

    /**
     *  @brief          stderr トレースの現在のスレッショルド レベルを取得します。
     *
     *  @param[in]      handle   cplat_tracer_create の戻り値。
     *  @return         現在のスレッショルド レベル。handle が NULL 時は CPLAT_TRACE_LEVEL_NONE。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p handle に対する操作は同時に実行できます。\n
     *  同一 @p handle に対する操作は、CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED で生成したハンドルでは同時に実行できます。CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED では、呼び出し側で直列化してください。\n
     *  返される値は取得時点のスナップショットです。
     */
    CPLAT_EXPORT cplat_trace_level CPLAT_API cplat_tracer_get_stderr_level(cplat_tracer *handle);

    /**
     *  @brief          stderr トレースのスレッショルド レベルを設定します。
     *
     *  @param[in]      handle   cplat_tracer_create の戻り値。
     *  @param[in]      level    新しいスレッショルド レベル (cplat_trace_level)。
     *                           強制出力のレベルは指定できません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_UNKNOWN の
     *                  いずれかを返します。
     *  @retval         CPLAT_ERR_INVALID_ARGUMENT  level が通常のトレース レベルの範囲外です。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p handle に対する操作は同時に実行できます。\n
     *  同一 @p handle に対する操作は、CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED で生成したハンドルでは同時に実行できます。CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED では、呼び出し側で直列化してください。\n
     *  stopped / started のどちらの状態でも有効です。変更は排他制御下で原子的に反映され、
     *  旧閾値と新閾値の両方で出力対象となるトレースを取りこぼしません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_tracer_set_stderr_level(cplat_tracer *handle, cplat_trace_level level);

    /**
     *  @brief          トレース プロバイダーを終了し、リソースを解放します。
     *
     *  @param[in,out]  handle   cplat_tracer_create の戻り値を保持するポインター。NULL または *handle が NULL の場合は何もしません。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p handle に対する操作は同時に実行できます。\n
     *  同一 @p handle に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT void CPLAT_API cplat_tracer_dispose(cplat_tracer **handle);

    /**
     *  @brief          トレース フックを登録します。
     *
     *  フックはプロセス内コールバックとして動作し、
     *  cplat_tracer_write_at 系関数を経由したすべての trace 呼び出しを受信できます。\n
     *  フィルター条件はなく、CPLAT_TRACE_LEVEL_NONE で要求された呼び出しも含め
     *  すべての trace イベントが通知されます。\n
     *  タイムスタンプは解決済みの状態でコールバックに渡されます。\n
     *  複数のフックを登録した場合はチェーンとして順次呼び出されます。
     *  コールバック内で cplat_tracer_call_next_hook を呼び出すことでチェーンを継続できます。\n
     *  本関数は stopped 状態でのみ有効です。
     *
     *  @param[in]      handle   cplat_tracer_create の戻り値。
     *  @param[in]      fn       コールバック関数。NULL は無効。
     *  @param[in]      context  コールバックに渡す任意のコンテキスト。NULL 可。
     *  @return         成功時は cplat_tracer_remove_hook() に渡すフック エントリを返します。
     *                  @p handle または @p fn が NULL、started 状態、またはメモリ不足の場合は
     *                  NULL を返します。
     *
     *  @par            使用例
        @code{.c}
        cplat_tracer_hook_entry *entry =
            cplat_tracer_set_hook(tracer, my_hook, NULL);
        cplat_tracer_start(tracer);
        // ... trace 処理 ...
        cplat_tracer_stop(tracer);
        cplat_tracer_remove_hook(tracer, entry);
        @endcode
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  stopped 状態のときは同時に実行できます。\n
     *  started 状態のときは呼び出せません。
     */
    CPLAT_EXPORT cplat_tracer_hook_entry *CPLAT_API cplat_tracer_set_hook(cplat_tracer *handle, cplat_tracer_hook_fn fn,
                                                                          void *context);

    /**
     *  @brief          登録済みトレース フックを解除します。
     *
     *  cplat_tracer_set_hook で登録したフックを解除し、エントリのメモリを解放します。\n
     *  本関数は stopped 状態でのみ有効です。
     *
     *  @param[in]      handle      cplat_tracer_create の戻り値。
     *  @param[in]      hook_entry  cplat_tracer_set_hook の戻り値。NULL は無視。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  stopped 状態のときは同時に実行できます。\n
     *  started 状態のときは呼び出せません。
     */
    CPLAT_EXPORT void CPLAT_API cplat_tracer_remove_hook(cplat_tracer *handle, cplat_tracer_hook_entry *hook_entry);

    /**
     *  @brief          フック チェーンを継続します。
     *
     *  コールバック内から呼び出し、前のフックへ処理を継続させます。\n
     *  @p prev が NULL の場合は何もしません (チェーン末端)。
     *
     *  @param[in]      prev       コールバックに渡された `prev` 引数。
     *  @param[in]      handle     trace を行った tracer ハンドル。
     *  @param[in]      level      trace レベル。
     *  @param[in]      timestamp  解決済みタイムスタンプ。
     *  @param[in]      message    解決済みメッセージ文字列。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p handle に対する操作は同時に実行できます。\n
     *  同一 @p handle に対する操作は、CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED で生成したハンドルでは同時に実行できます。CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED では、呼び出し側で直列化してください。\n
     *  フック コールバック内から複数スレッドで同時に呼び出せます。
     */
    CPLAT_EXPORT void CPLAT_API cplat_tracer_call_next_hook(cplat_tracer_hook_entry *prev, cplat_tracer *handle,
                                                            cplat_trace_level level, const cplat_timespec *timestamp,
                                                            const char *message);

    /**
     *  @brief  HEX 出力用ラベル セパレータを返すヘルパー関数です。
     *  @internal
     */
    CPLAT_EXPORT const char *CPLAT_API cplat_tracer_hex_sep(const char *message);

    /**
     *  @brief  HEX 出力用ラベル文字列を返すヘルパー関数 (NULL ガード) です。
     *  @internal
     */
    CPLAT_EXPORT const char *CPLAT_API cplat_tracer_hex_msg(const char *message);

    /**
     *  @brief          ソース位置付きメッセージを組み立てて tracer へ書き込みます。
     *  @param[in]      handle     cplat_tracer_create の戻り値。
     *  @param[in]      level      トレース レベル (cplat_trace_level)。
     *  @param[in]      timestamp  使用する実時刻。NULL の場合は API 内部で現在時刻を取得。
     *  @param[in]      file       出力に付与するソース ファイル名。
     *  @param[in]      line       出力に付与するソース行番号。
     *  @param[in]      message    null 終端 UTF-8 文字列。NULL の場合はソース位置のみを出力。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *  @internal
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p handle に対する操作は同時に実行できます。\n
     *  同一 @p handle に対する操作は、CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED で生成したハンドルでは同時に実行できます。CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED では、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_tracer_write_with_source(cplat_tracer *handle, cplat_trace_level level,
                                                              const cplat_timespec *timestamp, const char *file,
                                                              int line, const char *message);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/**
 *  @brief          ソース ファイル名と行番号を自動付与する cplat_tracer_write マクロです。
 */
#define cplat_tracer_write(handle, level, timestamp, message) \
    cplat_tracer_write_with_source((handle), (level), (timestamp), cplat_path_basename(__FILE__), __LINE__, (message))

/**
 *  @brief          ソース ファイル名と行番号を自動付与する cplat_tracer_writef マクロです。
 */
#define cplat_tracer_writef(handle, level, timestamp, fmt, ...) \
    cplat_tracer_writef_at((handle), (level), (timestamp), "[%s:%d] " fmt, cplat_path_basename(__FILE__), \
                           __LINE__, ##__VA_ARGS__)

/**
 *  @brief          ソース ファイル名と行番号を自動付与する cplat_tracer_write_hex マクロです。
 */
#define cplat_tracer_write_hex(handle, level, timestamp, data, size, message) \
    cplat_tracer_write_hexf_at((handle), (level), (timestamp), (data), (size), "[%s:%d]%s%s", \
                               cplat_path_basename(__FILE__), __LINE__, cplat_tracer_hex_sep(message), \
                               cplat_tracer_hex_msg(message))

/**
 *  @brief          ソース ファイル名と行番号を自動付与する cplat_tracer_write_hexf マクロです。
 */
#define cplat_tracer_write_hexf(handle, level, timestamp, data, size, fmt, ...) \
    cplat_tracer_write_hexf_at((handle), (level), (timestamp), (data), (size), "[%s:%d] " fmt, \
                               cplat_path_basename(__FILE__), __LINE__, ##__VA_ARGS__)

/** @} */

#endif /* CPLAT_TRACER_H */
