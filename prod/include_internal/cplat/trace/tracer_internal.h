/**
 *******************************************************************************
 *  @file           tracer_internal.h
 *  @brief          トレース プロバイダーを管理する内部インターフェイスを宣言します。
 *  @author         Tetsuo Honda
 *  @date           2026/04/03
 *  @version        1.0.0
 *
 *  tracer.c が公開する内部関数を宣言します。
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

#ifndef CPLAT_TRACER_INTERNAL_H
#define CPLAT_TRACER_INTERNAL_H

#include <stddef.h>
#include <cplat/runtime/shutdown.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          shutdown フェーズで全トレース ハンドルを解放します。
     *  @param[in]      event shutdown イベント情報です。
     *
     *  本関数は終了フェーズ向けの安全側解放を行います。\n
     *  内部でレジストリ ロックを取得しません。
     *  呼び出し前に、すべてのスレッドが trace API
     *  (cplat_tracer_create / cplat_tracer_dispose / cplat_tracer_write 等) の
     *  呼び出しを完了している必要があります。
     *  並行してトレース API が呼び出された場合は未定義動作になります。
     */
    void cplat_internal_trace_registry_dispose_all_on_shutdown(const cplat_shutdown_event *event);

    /**
     *  @brief          現在アクティブなトレース ハンドルの数を返します。
     *  @return         アクティブなハンドルの数を返します。
     */
    size_t cplat_internal_trace_registry_count(void);

    /**
     *  @brief          トレース ハンドル レジストリの現在の容量を返します。
     *  @return         確保済みのスロット数を返します。
     */
    size_t cplat_internal_trace_registry_capacity(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CPLAT_TRACER_INTERNAL_H */
