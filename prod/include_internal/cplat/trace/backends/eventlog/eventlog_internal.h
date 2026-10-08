/**
 *******************************************************************************
 *  @file           eventlog_internal.h
 *  @brief          EventLog シンクを管理する内部インターフェイスを宣言します。
 *  @author         Tetsuo Honda
 *  @date           2026/06/14
 *  @version        1.0.0
 *
 *  trace_eventlog.c が公開する内部関数を宣言します。
 *  このヘッダーはモジュール内部でのみ使用します。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef TRACE_EVENTLOG_SINK_INTERNAL_H
#define TRACE_EVENTLOG_SINK_INTERNAL_H

#include <cplat/base/platform.h>

#if defined(PLATFORM_WINDOWS)
    #include <cplat/trace/eventlog.h>
    #include <cplat/runtime/shutdown.h>

    #ifdef __cplusplus
extern "C"
{
    #endif /* __cplusplus */

    /**
     *  @brief          shutdown フェーズで EventLog シンク ハンドルを解放します。
     *  @param[in]      handle 解放する EventLog シンク ハンドルです。
     *  @param[in]      event shutdown イベント情報です。
     *
     *  `process_terminating` 相当のイベントでは free() のみ実行します。\n
     *  通常終了では DeregisterEventSource() を呼び出します。
     *  呼び出し時点で cplat_eventlog_sink_write() を実行中のスレッドが存在する場合は
     *  未定義動作になります。
     *  通常は cplat_internal_trace_registry_dispose_all_on_shutdown() 経由で呼び出されるため、
     *  呼び出し側がスレッドの静止を保証します。
     */
    void cplat_internal_eventlog_sink_dispose_on_shutdown(cplat_eventlog_sink *handle,
                                                          const cplat_shutdown_event *event);

    #ifdef __cplusplus
}
    #endif /* __cplusplus */

#endif /* PLATFORM_WINDOWS */

#endif /* TRACE_EVENTLOG_SINK_INTERNAL_H */
