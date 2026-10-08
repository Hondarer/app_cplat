/**
 *******************************************************************************
 *  @file           trace_file_internal.h
 *  @brief          ファイル プロバイダーを管理する内部インターフェイスを宣言します。
 *  @author         Tetsuo Honda
 *  @date           2026/04/03
 *  @version        1.0.0
 *
 *  trace_file.c が公開する内部関数を宣言します。
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

#ifndef TRACE_FILE_PROVIDER_INTERNAL_H
#define TRACE_FILE_PROVIDER_INTERNAL_H

#include <cplat/trace/trace_file.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          shutdown フェーズでファイル プロバイダー ハンドルを解放します。
     *  @param[in]      handle 解放するファイル プロバイダー ハンドルです。
     *
     *  内部ミューテックスを取得せずにハンドルを解放します。
     *  呼び出し時点で cplat_trace_file_sink_write() を実行中のスレッドが存在する場合は
     *  未定義動作になります。
     *  通常は cplat_internal_trace_registry_dispose_all_on_shutdown() 経由で呼び出されるため、
     *  呼び出し側がスレッドの静止を保証します。
     */
    void cplat_internal_trace_file_sink_dispose_on_shutdown(cplat_trace_file_sink *handle);

    /**
     *  @brief          整形済みタイムスタンプを使用してファイルへトレースを書き込みます。
     *  @param[in]      handle          ファイル sink ハンドルです。NULL は無視します。
     *  @param[in]      level           トレース レベルです。
     *  @param[in]      timestamp       トレース時刻です。現在の実装では使用しません。
     *  @param[in]      timestamp_text  ISO 8601 ローカル時刻文字列です。NULL を渡してはなりません。
     *  @param[in]      message         null 終端 UTF-8 文字列です。NULL は無視します。
     *  @return         成功時は @ref CPLAT_OK を、失敗時は @ref CPLAT_ERR_UNKNOWN を返します。
     *
     *  tracer がすでに整形したタイムスタンプを再利用する内部経路です。
     */
    int cplat_internal_trace_file_sink_write_text(cplat_trace_file_sink *handle, int level,
                                                  const cplat_timespec *timestamp, const char *timestamp_text,
                                                  const char *message);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TRACE_FILE_PROVIDER_INTERNAL_H */
