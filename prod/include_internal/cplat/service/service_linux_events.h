/**
 *******************************************************************************
 *  @file           service_linux_events.h
 *  @brief          Linux の OS イベントを監視するスレッドの内部インターフェイスを宣言します。
 *  @author         Tetsuo Honda
 *  @date           2026/11/28
 *  @version        1.0.0
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#ifndef CPLAT_SERVICE_LINUX_EVENTS_INTERNAL_H
#define CPLAT_SERVICE_LINUX_EVENTS_INTERNAL_H

#include <cplat/base/platform.h>

#if defined(PLATFORM_LINUX)

    #include <cplat/service/service.h>

    #ifdef __cplusplus
extern "C"
{
    #endif /* __cplusplus */

    /**
     *  @brief          イベント監視スレッドを起動します。
     *  @param[in]      def  サービス定義。NULL の場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *  @return         成功時は @ref CPLAT_OK 、必須初期化失敗時は @ref CPLAT_ERR_UNKNOWN を返します。
     */
    int cplat_internal_service_linux_events_start(const cplat_service_definition *def);

    /**
     *  @brief          イベント監視スレッドを停止して資源を解放します。
     *  @return         成功時は @ref CPLAT_OK 、実行中にイベント ループが異常終了していた場合は
     *                  @ref CPLAT_ERR_UNKNOWN を返します。
     */
    int cplat_internal_service_linux_events_stop(void);

    /**
     *  @brief          実行中のイベント ループが異常終了したかを返します。
     *  @return         異常終了した場合は 1、それ以外は 0 を返します。
     */
    int cplat_internal_service_linux_events_loop_failed(void);

    /**
     *  @brief          systemd へ通知を送信します。
     *  @param[in]      message  送信する通知文字列。
     *  @return         `sd_notify(3)` の戻り値を返します。
     */
    int cplat_internal_service_linux_sd_notify_send(const char *message);

    /**
     *  @brief          動作段階と systemd 通知を保護するロックを取得します。
     */
    void cplat_internal_service_linux_events_notify_lock(void);

    /**
     *  @brief          動作段階と systemd 通知を保護するロックを解放します。
     */
    void cplat_internal_service_linux_events_notify_unlock(void);

    /**
     *  @brief          READY=1 の通知に合わせて動作中段階へ進めます (通知ロック保持下で呼び出し)。
     */
    void cplat_internal_service_linux_events_enter_running(void);

    /**
     *  @brief          STOPPING=1 の通知に合わせて停止中段階へ進めます (通知ロック保持下で呼び出し)。
     */
    void cplat_internal_service_linux_events_enter_stopping(void);

    /**
     *  @brief          通知ロック保持下で動作中段階かを返します。
     *  @return         動作中の場合は 1、それ以外は 0。
     */
    int cplat_internal_service_linux_events_is_running(void);

    /**
     *  @brief          通知ロック保持下で停止中段階かを返します。
     *  @return         停止中の場合は 1、それ以外は 0。
     */
    int cplat_internal_service_linux_events_is_stopping(void);

    #ifdef __cplusplus
}
    #endif /* __cplusplus */

#endif /* PLATFORM_LINUX */

#endif /* CPLAT_SERVICE_LINUX_EVENTS_INTERNAL_H */
