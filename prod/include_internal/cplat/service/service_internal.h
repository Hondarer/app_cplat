/**
 *******************************************************************************
 *  @file           service_internal.h
 *  @brief          cplat service モジュールの共通層・OS 層間の内部契約を定義します。
 *  @author         Tetsuo Honda
 *  @date           2026/11/28
 *  @version        1.0.0
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#ifndef CPLAT_SERVICE_INTERNAL_H
#define CPLAT_SERVICE_INTERNAL_H

#include <stdint.h>

#include <cplat/service/service.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/** 実行ファイル パスを格納する配列の大きさ [byte] (終端 NUL を含む)。 */
#define CPLAT_SERVICE_EXEC_PATH_MAX 4096U

/** 既定の異常終了から自動再起動までの遅延 [s]。 */
#define CPLAT_SERVICE_DEFAULT_RESTART_DELAY_SEC 5U

/** 既定の起動回数集計期間 [s] (Linux)。 */
#define CPLAT_SERVICE_DEFAULT_START_LIMIT_INTERVAL_SEC 60U

/** 既定の期間内最大起動回数 (Linux)。 */
#define CPLAT_SERVICE_DEFAULT_START_LIMIT_BURST 5U

/** 既定の WatchdogSec [s] (Linux)。 */
#define CPLAT_SERVICE_DEFAULT_WATCHDOG_SEC 30U

/** 既定の PRESHUTDOWN 猶予 [ms] (Windows)。 */
#define CPLAT_SERVICE_DEFAULT_PRESHUTDOWN_TIMEOUT_MS 30000U

    /*
     * ============================================================
     *  共通層が提供する内部関数
     * ============================================================
     */

    /**
     *  @brief          停止抽象の内部状態を初期化します。
     *  @return         成功時は @ref CPLAT_OK 、失敗時は負の共通結果コードを返します。
     */
    int cplat_internal_service_stop_init(void);

    /**
     *  @brief          停止抽象の内部状態を解放します。
     */
    void cplat_internal_service_stop_dispose(void);

    /**
     *  @brief          前回の確認以降に `on_run` が進行したかを判定します。
     *  @param[in,out]  seen_count  前回の確認で記録した計数。NULL 不可。
     *  @return         待機中または前回の確認以降に呼び出しがあった場合は 1、それ以外は 0 を返します。
     */
    int cplat_internal_service_on_run_made_progress(uint32_t *seen_count);

    /**
     *  @brief          現在アクティブな診断トレース出力先を返します。
     *  @return         設定済みの `cplat_tracer *`。未設定時は NULL。
     */
    cplat_tracer *cplat_internal_service_get_tracer(void);

    /**
     *  @brief          サービス名の妥当性を検査します。
     *  @param[in]      name  サービス名。
     *  @return         妥当な場合は @ref CPLAT_OK 、不正な場合は @ref CPLAT_ERR_INVALID_ARGUMENT。
     */
    int cplat_internal_service_validate_name(const char *name);

    /**
     *  @brief          起動引数文字列の妥当性 (改行文字の有無) を検査します。
     *  @param[in]      run_args  起動引数文字列 (NULL 可)。
     *  @return         妥当な場合は @ref CPLAT_OK 、改行を含む場合は @ref CPLAT_ERR_INVALID_ARGUMENT。
     */
    int cplat_internal_service_validate_run_args(const char *run_args);

    /**
     *  @brief          ライフサイクル コールバック (`on_start` / `on_run` / `on_stop`) を駆動します。
     *  @param[in]      def  サービス定義。NULL 不可、`on_run` は NULL 不可。
     *  @return         最初に失敗したコールバックの戻り値 (全成功時は 0)。
     */
    int cplat_internal_service_run_callbacks(const cplat_service_definition *def);

    /**
     *  @brief          コンソール初期化と停止要求登録を行い、ライフサイクル コールバックを駆動します。
     *  @param[in]      def            サービス定義。NULL 不可。
     *  @param[out]     exit_code_out  コールバック終了コードの格納先。NULL 可。
     *  @return         API 自体の成否 (@ref CPLAT_OK / @ref CPLAT_ERR_*)。
     */
    int cplat_internal_service_run_lifecycle(const cplat_service_definition *def, int *exit_code_out);

    /**
     *  @brief          OS イベントを `on_event` へ配送します。
     *  @param[in]      def   サービス定義 (NULL 可)。
     *  @param[in]      info  イベント付加情報 (NULL 可)。
     */
    void cplat_internal_service_dispatch_event(const cplat_service_definition *def,
                                               const cplat_service_event_info *info);

    /**
     *  @brief          設定再読込要求を `on_reload` へ配送します。
     *  @param[in]      def  サービス定義 (NULL 可)。
     */
    void cplat_internal_service_dispatch_reload(const cplat_service_definition *def);

    /*
     * ============================================================
     *  OS 層が実装する内部フック
     * ============================================================
     */

    /**
     *  @brief          OS 固有の run 事前準備を行います。
     *  @return         @ref CPLAT_OK または負の共通結果コード。
     */
    int cplat_internal_service_os_prepare_run(void);

    /**
     *  @brief          OS 固有のサービス登録を行います。
     *  @param[in]      def      サービス定義 (検証済み)。
     *  @param[in]      options  登録オプション (NULL 可)。
     *  @return         @ref CPLAT_OK または負の共通結果コード。
     */
    int cplat_internal_service_os_install(const cplat_service_definition *def,
                                          const cplat_service_install_options *options);

    /**
     *  @brief          OS 固有のサービス解除を行います。
     *  @param[in]      def  サービス定義 (検証済み)。
     *  @return         @ref CPLAT_OK または負の共通結果コード。
     */
    int cplat_internal_service_os_uninstall(const cplat_service_definition *def);

    /**
     *  @brief          OS 固有のサービス常駐実行を行います。
     *  @param[in]      def            サービス定義 (検証済み)。
     *  @param[out]     exit_code_out  コールバック終了コードの格納先 (NULL 可)。
     *  @return         @ref CPLAT_OK または負の共通結果コード。
     */
    int cplat_internal_service_os_run(const cplat_service_definition *def, int *exit_code_out);

    /**
     *  @brief          起動完了を OS へ通知します。
     */
    void cplat_internal_service_os_notify_ready(void);

    /**
     *  @brief          停止開始を OS へ通知します。
     */
    void cplat_internal_service_os_notify_stopping(void);

    /**
     *  @brief          設定再読込の開始を OS へ通知します。
     */
    void cplat_internal_service_os_notify_reloading(void);

    /**
     *  @brief          状態テキストを OS へ通知します。
     *  @param[in]      text  状態テキスト。NULL 不可。
     */
    void cplat_internal_service_os_notify_status(const char *text);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CPLAT_SERVICE_INTERNAL_H */
