/**
 *******************************************************************************
 *  @file           service.h
 *  @brief          クロスプラットフォーム サービス基盤の公開 API を宣言します。
 *  @author         Tetsuo Honda
 *  @date           2026/11/28
 *  @version        1.0.0
 *
 *  Windows サービス (SCM) と Linux systemd の両方に対応した
 *  サービス (デーモン) 基盤 API です。\n
 *  \n
 *  利用者は @ref cplat_service_definition 構造体にサービス名・表示名・説明・
 *  ライフサイクル コールバック (on_start / on_run / on_stop)・任意のイベントおよび
 *  再読込コールバック・診断トレース出力先を設定し、個別のサービス操作 API
 *  (cplat_service_install / cplat_service_uninstall / cplat_service_run /
 *  cplat_service_run_console) を呼び出します。\n
 *  \n
 *  本 API はプロセス グローバル API です。1 プロセス内での複数インスタンスの
 *  同時実行はサポートしません。\n
 *  コマンド ライン引数の解析、サブコマンドの振り分け、および UAC 等による
 *  権限昇格再起動は利用側の CLI 層の責務です。\n
 *  \n
 *  サービスの停止要求は、コンソール割り込み (SIGINT/SIGTERM、SetConsoleCtrlHandler)、
 *  Windows SCM の SERVICE_CONTROL_STOP 等、および明示的な呼び出しを
 *  cplat_service_request_stop() に集約します。
 *  on_run コールバックは cplat_service_wait_for_stop() を呼び出すことで停止を検知できます。
 *
 *  @par            Windows の SCM へ通知する所要時間の見積もり
 *  Windows のサービスでは、起動中と停止中の状態を SCM へ通知するときに、
 *  次の通知までの所要時間の見積もり (`SERVICE_STATUS` の `dwWaitHint`) を固定値で渡します。\n
 *  `dwWaitHint` は見積もりであり、`on_start`、停止要求後の `on_run`、`on_stop` が
 *  これより長くかかる場合の進捗通知 (`dwCheckPoint` の動的更新) には対応していません。\n
 *  see: https://learn.microsoft.com/en-us/windows/win32/api/winsvc/ns-winsvc-service_status
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#ifndef CPLAT_SERVICE_SERVICE_H
#define CPLAT_SERVICE_SERVICE_H

#include <cplat/base/platform.h>
#include <cplat/base/result.h>
#include <cplat/base/wait.h>
#include <cplat/cplat_export.h>
#include <cplat/trace/tracer.h>

/**
 *  @ingroup        CPLAT_SERVICE
 *  @{
 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /*
     * ============================================================
     *  ライフサイクル コールバック型定義
     * ============================================================
     */

    /**
     *  @brief          サービス初期化コールバックの型。
     *
     *  サービス起動時に 1 回だけ呼ばれます。\n
     *  0 を返すと `on_run` が呼ばれます。0 以外を返すと起動を中断します
     *  (`on_run` と `on_stop` は呼ばれません)。\n
     *  失敗値はプロセスの終了コードまたは Windows SCM の `dwServiceSpecificExitCode`
     *  として OS へ伝わるため、1 から 125 の範囲 (3 は `abort()` と重複するため除く) を推奨します。\n
     *  Linux では、systemd の起動タイムアウト (`TimeoutStartSec=`、既定 90 [s]) より
     *  短い時間で戻る必要があります。
     *
     *  @param[in]      user_data   @ref cplat_service_definition に登録した任意ポインター。
     *  @return         成功時は 0、失敗時は 0 以外を返します。
     */
    typedef int (*cplat_service_on_start_fn)(void *user_data);

    /**
     *  @brief          サービス メイン ループ コールバックの型。
     *
     *  `on_start` が成功した後に 1 回呼ばれます。\n
     *  停止要求が届くまで戻らないように実装してください。\n
     *  停止要求の検知には cplat_service_wait_for_stop() を使用してください。\n
     *  0 以外を返しても、後始末のため `on_stop` が呼ばれます。\n
     *  失敗値は 1 から 125 の範囲 (3 を除く) を推奨します。
     *
     *  @par            Linux の watchdog との関係
     *  systemd が watchdog を要求している場合 (unit の `WatchdogSec=`) は、本関数の進行を
     *  watchdog 通知へ結び付けます。\n
     *  サービス層は、本関数が cplat_service_wait_for_stop() で待機している間、および
     *  cplat_service_wait_for_stop() の呼び出しを続けている間だけ `WATCHDOG=1` を送信します。\n
     *  cplat_service_wait_for_stop() から戻った後は、watchdog 期限の半分より短い時間で
     *  次の cplat_service_wait_for_stop() を呼び出してください
     *  (既定の `WatchdogSec=30` の場合は 15 [s] 未満)。
     *  本関数の開始から最初の呼び出しまでも同様です。\n
     *  cplat_service_wait_for_stop() は本関数を実行しているスレッドから呼び出してください。\n
     *  期限内に呼ばない場合は、systemd がサービスを失敗として `SIGABRT` で終了させ、
     *  `Restart=on-failure` により再起動します。\n
     *  Windows の SCM には同等の仕組みがないため、この条件は Linux だけに適用します。
     *
     *  @param[in]      user_data   @ref cplat_service_definition に登録した任意ポインター。
     *  @return         成功時は 0、失敗時は 0 以外を返します。
     */
    typedef int (*cplat_service_on_run_fn)(void *user_data);

    /**
     *  @brief          サービス停止処理コールバックの型。
     *
     *  `on_run` が戻った後に必ず 1 回呼ばれます (`on_run` が失敗を返した場合も
     *  後始末のために呼ばれます)。`on_start` が失敗した場合は呼ばれません。\n
     *  `on_run` と `on_stop` の双方が失敗した場合は、`on_run` の戻り値を採用します。\n
     *  失敗値は 1 から 125 の範囲 (3 を除く) を推奨します。\n
     *  Linux では、停止要求から `on_run` の復帰と `on_stop` の完了までの合計を
     *  60 [s] 未満 (既定の停止タイムアウト 90 [s] からイベント監視スレッドの終了待ち上限
     *  30 [s] を除いた時間) にしてください。
     *
     *  @param[in]      user_data   @ref cplat_service_definition に登録した任意ポインター。
     *  @return         成功時は 0、失敗時は 0 以外を返します。
     */
    typedef int (*cplat_service_on_stop_fn)(void *user_data);

    /*
     * ============================================================
     *  OS イベント定義
     * ============================================================
     */

    /**
     *  @brief          OS イベントの種別。
     *
     *  Windows SCM のコントロール通知と Linux systemd-logind のシグナルを
     *  共通のイベント種別に対応付けます。
     */
    typedef enum cplat_service_event_type
    {
        CPLAT_SERVICE_EVENT_POWER_SUSPEND =
            1, /**< サスペンド開始。Windows: PBT_APMSUSPEND / Linux: PrepareForSleep(true)。 */
        CPLAT_SERVICE_EVENT_POWER_RESUME = 2, /**< サスペンド復帰。Windows: PBT_APMRESUMEAUTOMATIC,
                                                     PBT_APMRESUMESUSPEND / Linux: PrepareForSleep(false)。 */
        CPLAT_SERVICE_EVENT_SESSION_LOGON =
            3, /**< セッションのログオン。Windows: WTS_SESSION_LOGON / Linux: SessionNew。 */
        CPLAT_SERVICE_EVENT_SESSION_LOGOFF =
            4, /**< セッションのログオフ。Windows: WTS_SESSION_LOGOFF / Linux: SessionRemoved。 */
        CPLAT_SERVICE_EVENT_PRESHUTDOWN = 5 /**< シャットダウン前の猶予通知。Windows: SERVICE_CONTROL_PRESHUTDOWN /
                                                     Linux: PrepareForShutdown(true)。 */
    } cplat_service_event_type;

    /**
     *  @brief          OS イベントの付加情報。
     */
    typedef struct cplat_service_event_info
    {
        cplat_service_event_type type; /**< イベント種別。 */
        unsigned int pad;              /**< 64 ビット環境でのアラインメント明示用パディング。未使用。 */
        const char *session_id;        /**< セッション ID。@ref CPLAT_SERVICE_EVENT_SESSION_LOGON /
                                            @ref CPLAT_SERVICE_EVENT_SESSION_LOGOFF のみ有効。他は NULL。\n
                                            コールバック復帰後に無効となるため、保持する場合は複製してください。 */
    } cplat_service_event_info;

    /**
     *  @brief          OS イベント コールバックの型。
     *
     *  電源・セッション・シャットダウン前のイベント発生時に呼ばれます。\n
     *  `on_run` とは別スレッド (Linux: イベント監視スレッド、Windows: SCM コントロール
     *  ハンドラー スレッド) から呼ばれるため、共有データへのアクセスには同期が必要です。\n
     *  OS 側の応答期限 (Linux のサスペンド猶予は logind の `InhibitDelayMaxSec`、既定 5 [s])
     *  があるため、短時間で戻るように実装してください。\n
     *  配送は `on_start` 成功後 (動作中段階) から `on_run` 復帰前までに開始し、
     *  すでに開始した呼び出しは `on_stop` と並行実行中の場合があります。
     *
     *  @param[in]      info        イベントの付加情報。NULL は渡されません。
     *  @param[in]      user_data   @ref cplat_service_definition に登録した任意ポインター。
     */
    typedef void (*cplat_service_on_event_fn)(const cplat_service_event_info *info, void *user_data);

    /**
     *  @brief          設定再読込コールバックの型。
     *
     *  Windows: `SERVICE_CONTROL_PARAMCHANGE`、Linux: `SIGHUP` (`systemctl reload`)
     *  を受けると呼ばれます。\n
     *  Linux では呼び出しの前後で `RELOADING=1` / `READY=1` の通知を自動で行います。\n
     *  `on_run` とは別スレッドから呼ばれるため、共有データへのアクセスには同期が必要です。
     *  短時間で戻るように実装してください。\n
     *  配送は `on_start` 成功後 (動作中段階) から `on_run` 復帰前までに開始し、
     *  すでに開始した呼び出しは `on_stop` と並行実行中の場合があります。
     *
     *  @param[in]      user_data   @ref cplat_service_definition に登録した任意ポインター。
     */
    typedef void (*cplat_service_on_reload_fn)(void *user_data);

    /*
     * ============================================================
     *  サービス定義・登録オプション構造体
     * ============================================================
     */

    /**
     *  @brief          サービスの定義。
     */
    typedef struct cplat_service_definition
    {
        const char *name;                     /**< 登録名 (必須・空文字不可。パス区切り・制御文字不可)。 */
        const char *display_name;             /**< 表示名 (Windows SCM 表示名。install 時は必須)。 */
        const char *description;              /**< 説明文 (NULL 可、NULL 時は空文字扱い)。 */
        cplat_service_on_start_fn on_start;   /**< 初期化コールバック (NULL 可)。 */
        cplat_service_on_run_fn on_run;       /**< メイン ループ コールバック (run / run_console 時は必須)。 */
        cplat_service_on_stop_fn on_stop;     /**< 停止処理コールバック (NULL 可)。 */
        void *user_data;                      /**< 各コールバックへ渡す任意ポインター (NULL 可)。 */
        cplat_service_on_event_fn on_event;   /**< OS イベント コールバック (NULL 可)。 */
        cplat_service_on_reload_fn on_reload; /**< 設定再読込コールバック (NULL 可)。 */
        cplat_tracer *tracer;                 /**< 診断トレース出力先 (NULL 可、所有権は利用側)。 */
    } cplat_service_definition;

    /**
     *  @brief          サービス登録オプション。
     *
     *  cplat_service_install() へ NULL またはゼロ初期化 (`{0}`) を渡した場合、
     *  すべての項目に安全な既定値を適用します。
     */
    typedef struct cplat_service_install_options
    {
        const char *run_args;                  /**< 起動引数 (NULL 時は "run"、"" 時は引数なし)。改行不可。 */
        unsigned int restart_delay_sec;        /**< 自動再起動遅延 [s] (0 時は既定 5 [s])。 */
        unsigned int start_limit_interval_sec; /**< Linux: 起動回数集計期間 [s] (0 時は既定 60 [s])。 */
        unsigned int start_limit_burst;        /**< Linux: 期間内最大起動回数 (0 時は既定 5 回)。 */
        unsigned int watchdog_sec;             /**< Linux: WatchdogSec [s] (0 時は既定 30 [s])。 */
        unsigned int preshutdown_timeout_ms;   /**< Windows: PRESHUTDOWN 猶予 [ms] (0 時は既定 30000 [ms])。 */
        int disable_auto_restart;              /**< 0: 自動再起動あり (既定) / 非 0: 自動再起動なし。 */
        int disable_watchdog;                  /**< Linux: 0: watchdog あり (既定) / 非 0: WatchdogSec= なし。 */
        int disable_oom_protection;            /**< Linux: 0: OOM 保護あり (既定) / 非 0: OOM 保護なし。 */
    } cplat_service_install_options;

    /*
     * ============================================================
     *  サービス操作 API
     * ============================================================
     */

    /**
     *  @brief          サービス常駐実行 (run) の事前準備を行います。
     *  @return         成功時は @ref CPLAT_OK 、Linux で `pthread_sigmask` が失敗した場合は
     *                  対応する負の共通結果コードを返します。
     *
     *  Linux では、スレッド生成およびトレーサー開始より前の単一スレッド段階で本関数を呼び出し、
     *  `SIGHUP` をブロックします。以後に生成するスレッドはこのシグナル マスクを引き継ぎます。\n
     *  Windows では何もせず @ref CPLAT_OK を返します。
     *
     *  @attention      Linux では、ブロックした `SIGHUP` マスクが `fork` と `execve` を越えて
     *                  子プロセスへ引き継がれます。サービス内から起動した子プロセスで `SIGHUP`
     *                  を使用する場合は、子プロセス側でマスクを解除してください。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。起動直後の単一スレッド段階で呼び出してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_service_prepare_run(void);

    /**
     *  @brief          OS のサービス マネージャーへサービスを登録します。
     *  @param[in]      def      サービス定義。NULL 不可。`def->name` は必須 (空文字・`/`・`\`・制御文字不可)。
     *                           Windows では `def->display_name` も必須です。
     *  @param[in]      options  登録オプション。NULL 可 (NULL 時はすべて既定値を使用)。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_PERMISSION_DENIED 、@ref CPLAT_ERR_BUFFER_TOO_SMALL 、
     *                  @ref CPLAT_ERR_DUPLICATE_DEFINITION (Windows 既存サービス時)、
     *                  @ref CPLAT_ERR_BUSY (Windows 削除待ち時)、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  自動権限昇格は行いません。権限不足時は @ref CPLAT_ERR_PERMISSION_DENIED を返します。\n
     *  Linux で `disable_auto_restart == 0` の場合、実効の `start_limit_burst`・`restart_delay_sec`・
     *  `start_limit_interval_sec` に対して `((burst - 1) * restart_delay_sec) >= interval_sec`
     *  が成り立つとき、または乗算がオーバーフローするときは @ref CPLAT_ERR_INVALID_ARGUMENT を返します。\n
     *  Windows で `restart_delay_sec * 1000` が `DWORD` 上限を超える場合も
     *  @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_service_install(const cplat_service_definition *def,
                                                     const cplat_service_install_options *options);

    /**
     *  @brief          OS のサービス マネージャーからサービスを停止・解除します。
     *  @param[in]      def  サービス定義。NULL 不可。`def->name` は必須です。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_PERMISSION_DENIED 、@ref CPLAT_ERR_NOT_FOUND (Windows 未登録時)、
     *                  @ref CPLAT_ERR_TIMEOUT (Windows 停止待機時間切れ時)、
     *                  @ref CPLAT_ERR_BUSY (Windows 削除待ち時)、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  自動権限昇格は行いません。権限不足時は @ref CPLAT_ERR_PERMISSION_DENIED を返します。\n
     *  Windows は最大 30 [s] の停止確認後に削除を要求します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_service_uninstall(const cplat_service_definition *def);

    /**
     *  @brief          OS サービス マネージャー管理下でサービスを常駐実行します。
     *  @param[in]      def            サービス定義。NULL 不可。`def->name` と `def->on_run` は必須です。
     *  @param[out]     exit_code_out  利用側コールバックが返した終了コードの格納先。NULL 可。
     *  @return         @ref CPLAT_OK (コールバック終了コードは @p exit_code_out へ格納)、
     *                  @ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_OUT_OF_MEMORY 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  入口で停止抽象を初期化し、出口で確実に解放します。\n
     *  Linux では `Type=notify` として常駐し、イベント監視スレッドを起動します。
     *  イベント監視スレッドの必須初期化に失敗した場合は `on_start` を呼ばず @ref CPLAT_ERR_UNKNOWN を返します。\n
     *  Windows では `StartServiceCtrlDispatcherU` で SCM ディスパッチャーへ接続します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_service_run(const cplat_service_definition *def, int *exit_code_out);

    /**
     *  @brief          フォアグラウンドのコンソール モードでサービス ライフサイクルを実行します。
     *  @param[in]      def            サービス定義。NULL 不可。`def->name` と `def->on_run` は必須です。
     *  @param[out]     exit_code_out  利用側コールバックが返した終了コードの格納先。NULL 可。
     *  @return         @ref CPLAT_OK (コールバック終了コードは @p exit_code_out へ格納)、
     *                  @ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_OUT_OF_MEMORY 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  入口で停止抽象を初期化し、cplat_console_init() と cplat_shutdown_request_register() により
     *  `SIGINT` / `SIGTERM` / `SetConsoleCtrlHandler` を停止要求へ接続します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_service_run_console(const cplat_service_definition *def, int *exit_code_out);

    /*
     * ============================================================
     *  停止抽象・状態通知 API
     * ============================================================
     */

    /**
     *  @brief          停止要求が届くか指定時間が経過するまで待機します。
     *  @param[in]      timeout_ms  タイムアウト [ms]。@ref CPLAT_NO_WAIT (0) および
     *                              @ref CPLAT_WAIT_FOREVER (`INT_MAX`) に対応します。負値は不正です。
     *  @return         停止要求が到達した場合は @ref CPLAT_OK 、時間経過した場合は @ref CPLAT_ERR_TIMEOUT 、
     *                  @p timeout_ms < 0 または未初期化状態での呼び出しは @ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  条件変数待機に失敗した場合は対応する負の共通結果コードを返します。
     *
     *  入口と出口で `on_run` の進行カウンターを更新します (Linux の watchdog 判定に使用)。
     *
     *  @par            使用例
     *  @code{.c}
     *  static int on_run(void *user_data)
     *  {
     *      (void)user_data;
     *      while (cplat_service_wait_for_stop(1000) == CPLAT_ERR_TIMEOUT)
     *      {
     *          // 周期処理
     *      }
     *      return 0;
     *  }
     *  @endcode
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_service_wait_for_stop(int timeout_ms);

    /**
     *  @brief          停止要求が届いているかを即時判定します。
     *  @return         停止要求済みの場合は 1、未要求または未初期化時は 0 を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_service_stop_requested(void);

    /**
     *  @brief          停止を要求し、待機中のスレッドへ通知します。
     *
     *  複数回呼び出しても安全 (べき等) です。未初期化状態では何もしません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void CPLAT_API cplat_service_request_stop(void);

    /**
     *  @brief          サービスの状態テキストを通知します。
     *  @param[in]      text  状態テキスト。NULL を指定した場合は何もしません。
     *
     *  診断トレース (`VERBOSE`) へ出力し、Linux では `sd_notify("STATUS=...")` を送信します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。
     */
    CPLAT_EXPORT void CPLAT_API cplat_service_set_status_text(const char *text);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_SERVICE_SERVICE_H */
