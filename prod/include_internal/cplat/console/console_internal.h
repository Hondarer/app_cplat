/**
 *******************************************************************************
 *  @file           console_internal.h
 *  @brief          コンソール ヘルパーの内部インターフェイスを宣言します。
 *  @author         Tetsuo Honda
 *  @date           2026/04/04
 *  @version        1.0.0
 *
 *  console.c が公開する内部関数を宣言します。
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

#ifndef CONSOLE_UTIL_INTERNAL_H
#define CONSOLE_UTIL_INTERNAL_H

#include <cplat/runtime/shutdown.h>

/**
 *  @brief          昇格プロセスへ親コンソール引き継ぎを指示する内部フラグです。
 *
 *  cplat_elevated_process_run_if_needed() が昇格プロセスのコマンド ラインへ
 *  `{FLAG}={親プロセス ID}:{親コンソール window ハンドル}` の形式で付与し、
 *  cplat_console_attach_parent() がこれを検出して親コンソールへ再接続します。\n
 *  window ハンドル部は省略可能で、子側は再接続後に GetConsoleWindow() がこの値に
 *  一致するまで待つことで、一時コンソールではなく親コンソールへ確実に繋がったことを
 *  確認します。旧形式 (`{FLAG}={親プロセス ID}`) も後方互換で受理します。
 */
#define CPLAT_CONSOLE_HANDOVER_FLAG "--cplat-attach-console"

/**
 *  @brief          コンソール再接続診断ログの有効化環境変数名です。
 *
 *  値が空文字または "0" 以外のとき、`%TEMP%` 配下へ診断ログを追記します。
 */
#define CPLAT_CONSOLE_ATTACH_DIAG_ENV "CPLAT_CONSOLE_ATTACH_DIAG"

/**
 *  @brief          コンソール再接続診断ログのファイル名です。
 */
#define CPLAT_CONSOLE_ATTACH_DIAG_FILE "cplat_console_attach.log"

/**
 *  @brief          昇格子プロセスへ診断ログ有効化を引き継ぐ内部フラグです。
 *
 *  親プロセスで診断ログを有効化している場合、昇格子プロセスのコマンド ラインへ
 *  このフラグを追加します。cplat_console_attach_parent() が argv から除去して
 *  環境変数へ反映します。
 */
#define CPLAT_CONSOLE_ATTACH_DIAG_FLAG "--cplat-attach-console-diag"

/**
 *  @brief          昇格時の AttachConsole リトライ回数の上限です。
 *
 *  UAC 昇格直後は子プロセスの一時コンソール (conhost) 割り当てが非同期に進むため、
 *  cplat_console_attach_parent() の AttachConsole が一時的に失敗することがあります。
 *  割り当てが落ち着くまで本回数を上限にリトライします。
 */
#define CPLAT_CONSOLE_ATTACH_MAX_ATTEMPTS 100

/**
 *  @brief          昇格時の AttachConsole リトライ間隔 [ms] です。
 *
 *  CPLAT_CONSOLE_ATTACH_MAX_ATTEMPTS と合わせて最悪待ち時間を決めます。
 *  通常は 1 回目か 2 回目で接続できるため、待ち時間は無視できます。
 */
#define CPLAT_CONSOLE_ATTACH_RETRY_INTERVAL_MS 10

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          shutdown フェーズでコンソール ヘルパーを解放します。
     *  @param[in]      event   shutdown イベント情報です。
     *  @param[in]      context 登録時に渡した任意のコンテキストです。本関数では使用しません。
     *
     *  通常終了ではストリームを元に戻してスレッド終了を待機します。\n
     *  シグナルや強制終了に近いイベントでは待機を避け、安全側で短絡します。
     */
    void cplat_internal_console_dispose_on_shutdown(const cplat_shutdown_event *event, void *context);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CONSOLE_UTIL_INTERNAL_H */
