/**
 *******************************************************************************
 *  @file           syslog_internal.h
 *  @brief          syslog プロバイダーを管理する内部インターフェイスを宣言します。
 *  @author         Tetsuo Honda
 *  @date           2026/04/03
 *  @version        1.0.0
 *
 *  trace_syslog.c が公開する内部関数を宣言します。
 *  このヘッダーはモジュール内部でのみ使用します。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/*
 * このヘッダーは多数のソース ファイルから参照されるため、
 * @hideincludedbygraph によって "Included by" グラフを無効にする。
 */

#ifndef TRACE_SYSLOG_PROVIDER_INTERNAL_H
#define TRACE_SYSLOG_PROVIDER_INTERNAL_H

#include <cplat/base/platform.h>

#if defined(PLATFORM_LINUX)
    #include <cplat/trace/syslog.h>

    #ifdef __cplusplus
extern "C"
{
    #endif /* __cplusplus */

    /**
     *  @brief          shutdown フェーズで syslog プロバイダー ハンドルを解放します。
     *  @param[in]      handle 解放する syslog プロバイダー ハンドルです。
     *
     *  reconnect_lock を取得せずにソケットを閉じてハンドルを解放します。
     *  呼び出し時点で cplat_syslog_sink_write() を実行中のスレッドが存在する場合は
     *  未定義動作になります。
     *  通常は cplat_internal_trace_registry_dispose_all_on_shutdown() 経由で呼び出されるため、
     *  呼び出し側がスレッドの静止を保証します。
     */
    void cplat_internal_syslog_sink_dispose_on_shutdown(cplat_syslog_sink *handle);

    #ifdef __cplusplus
}
    #endif /* __cplusplus */

#endif /* PLATFORM_LINUX */

#endif /* TRACE_SYSLOG_PROVIDER_INTERNAL_H */
