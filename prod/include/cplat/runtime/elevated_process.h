/**
 *******************************************************************************
 *  @file           elevated_process.h
 *  @brief          管理者権限を確認し、昇格プロセスを起動する API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/06/20
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CPLAT_ELEVATED_PROCESS_H
#define CPLAT_ELEVATED_PROCESS_H

#include <stddef.h>
#include <cplat/base/platform.h>
#include <cplat/base/result.h>
#include <cplat/cplat_export.h>

/**
 *  @ingroup        CPLAT_RUNTIME
 *  @{
 */

#define CPLAT_ELEVATED_PROCESS_RESULT_MESSAGE_SIZE \
    4096 /**< 昇格プロセスの結果メッセージを受け渡すバッファのバイト数 (終端 NUL を含む)。 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          昇格プロセスの出力元ストリームです。
     */
    typedef enum cplat_elevated_process_stream
    {
        CPLAT_ELEVATED_PROCESS_STREAM_STDOUT = 1, /**< 昇格プロセスの標準出力。 */
        CPLAT_ELEVATED_PROCESS_STREAM_STDERR = 2  /**< 昇格プロセスの標準エラー出力。 */
    } cplat_elevated_process_stream;

    /**
     *  @brief          昇格プロセスの出力を受け取るコールバック関数型です。
     *
     *  @param[in]      stream   出力元ストリーム。
     *  @param[in]      data     昇格プロセスが書き込んだバイト列。NUL 終端しません。
     *  @param[in]      size     @p data のバイト数。1 以上です。
     *  @param[in]      context  登録時に渡した任意のコンテキスト。
     *
     *  @p data は昇格プロセスが書き込んだバイト列を変換せずに渡します。\n
     *  1 回の呼び出しで渡す範囲は、昇格プロセスの書き込み単位と一致しません。
     *  UTF-8 の複数バイト文字や行の途中で分割される場合があります。
     *
     *  @par            スレッド セーフ
     *  コールバックは cplat_elevated_process_run_piped() を呼び出したスレッドから呼び出されます。
     */
    typedef void (*cplat_elevated_process_output_fn)(cplat_elevated_process_stream stream, const char *data,
                                                     size_t size, void *context);

    /**
     *  @brief          現在のプロセスが管理者/root 権限で動作しているかを確認します。
     *  @param[out]     elevated  昇格済みなら 1、そうでなければ 0 の格納先。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_UNSUPPORTED 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  Windows ではプロセス トークンの昇格状態 (TokenElevation) を確認します。\n
     *  Linux では実効ユーザー ID が root (0) かどうかを確認します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_elevated_process_is_elevated(int *elevated);

    /**
     *  @brief          管理者/root 権限が必要な処理のため、必要に応じて昇格実行します。
     *  @param[in]      arguments  昇格実行時に現在の実行ファイルへ渡す引数文字列。NULL 可。
     *  @param[out]     exit_code  昇格プロセスの終了コード、または 0 の格納先。
     *  @param[out]     handled    昇格プロセスで処理した場合は 0 以外、現プロセスで継続する場合は 0 の格納先。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_OUT_OF_MEMORY 、@ref CPLAT_ERR_UNSUPPORTED 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  Windows では、未昇格の場合に UAC を要求して現在の実行ファイルを @p arguments 付きで
     *  再起動し、子プロセスの終了まで待機します。すでに昇格済みの場合は何もしません。\n
     *  未昇格かつセッション 0 (非対話セッション。Windows サービスなど、UAC ダイアログを
     *  表示できる対話デスクトップを持たないセッション) の場合は、昇格を試みず失敗を返します。\n
     *  Linux では、実効ユーザー ID が root であることを確認します。root でない場合は失敗します。\n
     *  本関数は権限保証のための API であり、Linux で `sudo` などの外部昇格コマンドは実行しません。
     *
     *  @note           Windows で呼び出し元にコンソールがある場合、昇格プロセスへ親コンソールを
     *                  引き継ぐため、昇格プロセスは別ウインドウを表示せず親コンソールへ出力します。\n
     *                  この場合 @p arguments の末尾に内部フラグを付与して再起動するため、昇格
     *                  プロセスは起動直後に cplat_console_attach_parent() を呼び出す必要が
     *                  あります。呼び出さない場合、昇格プロセスの出力は表示されません。\n
     *                  呼び出し元にコンソールが無い場合は、昇格プロセスを通常表示で起動します。\n
     *                  Windows で UAC を表示するため、通常はメイン スレッドまたはユーザー操作に応答するスレッドから呼び出してください。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_elevated_process_run_if_needed(const char *arguments, int *exit_code,
                                                                    int *handled);

    /**
     *  @brief          管理者/root 権限が必要な処理のため、必要に応じて昇格実行し、
     *                  昇格プロセスが報告した結果メッセージを取得します。
     *  @param[in]      arguments            昇格実行時に現在の実行ファイルへ渡す引数文字列。NULL 可。
     *  @param[out]     exit_code            昇格プロセスの終了コード、または 0 の格納先。
     *  @param[out]     handled              昇格プロセスで処理した場合は 0 以外、現プロセスで継続する場合は 0 の格納先。
     *  @param[out]     result_message       昇格プロセスが報告したメッセージ (UTF-8) の格納先。NULL 可。
     *  @param[in]      result_message_size  @p result_message のバイト数。
     *                                       通常は @ref CPLAT_ELEVATED_PROCESS_RESULT_MESSAGE_SIZE を指定します。
     *                                       報告されたメッセージが @p result_message_size - 1 バイトを
     *                                       超える場合、超えた部分は切り捨てます。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_OUT_OF_MEMORY 、@ref CPLAT_ERR_UNSUPPORTED 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  cplat_elevated_process_run_if_needed() はコンソールの再接続を昇格プロセス側に
     *  要求しますが、UAC 昇格直後の親コンソール再割り当ては実機調査でも原因を特定できない
     *  間欠的な書き込み不能 (`ERROR_INVALID_HANDLE`) を起こすことが分かっています。\n
     *  本関数は昇格プロセスのコンソールを一切引き継がず、結果メッセージを一時ファイル経由で
     *  受け渡します。昇格プロセス側は cplat_elevated_process_extract_result_target() を起動直後に
     *  呼び出し、処理結果を cplat_elevated_process_report_result() で報告してください。\n
     *  cplat_elevated_process_run_if_needed() と同様、未昇格かつセッション 0 (非対話
     *  セッション) の場合は、昇格を試みず失敗を返します。\n
     *  Linux では cplat_elevated_process_run_if_needed() と同じ判定を行い、
     *  @p result_message は変更しません (別プロセスを起動しないため報告の余地がない)。
     *
     *  @note           Windows で UAC を表示するため、通常はメイン スレッドまたはユーザー操作に応答するスレッドから呼び出してください。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_elevated_process_run_with_result(const char *arguments, int *exit_code,
                                                                      int *handled, char *result_message,
                                                                      size_t result_message_size);

    /**
     *  @brief          argv から結果報告先フラグを取り出します。
     *  @param[in,out]  argc          引数の数へのポインター。NULL 可。
     *  @param[in,out]  argv          引数配列。NULL 可。
     *  @param[out]     detected_out  報告先フラグを検出した場合は 1、そうでない場合は 0 の格納先。
     *                                NULL 可。戻り値が @ref CPLAT_OK の場合のみ有効です。
     *  @return         常に @ref CPLAT_OK を返します。
     *
     *  cplat_elevated_process_run_with_result() が付与したフラグを検出し、後続の
     *  cplat_elevated_process_report_result() のために報告先を保持します。検出した
     *  フラグは @p argv から取り除き、@p argc を 1 減らします。\n
     *  プログラム開始直後、引数解析より前に 1 度だけ呼び出してください。\n
     *  Linux では何もせず @p detected_out に 0 を設定します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  プロセス起動直後のシングル スレッド フェーズで呼び出してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_elevated_process_extract_result_target(int *argc, char **argv, int *detected_out);

    /**
     *  @brief          昇格プロセスから、呼び出し元プロセスへ結果メッセージを報告します。
     *  @param[in]      message  報告するメッセージ (UTF-8)。NULL を渡してはなりません。
     *                           終端 NUL を含めて @ref CPLAT_ELEVATED_PROCESS_RESULT_MESSAGE_SIZE
     *                           バイト以内にしてください。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_UNKNOWN (報告先が無い場合を含む) のいずれかを返します。
     *
     *  cplat_elevated_process_extract_result_target() で報告先を検出している場合のみ、
     *  そのファイルへ @p message を書き込みます。報告先を検出していない場合
     *  (cplat_elevated_process_run_with_result() 経由で起動されていない場合) は
     *  何も行わず @ref CPLAT_ERR_UNKNOWN を返すため、呼び出し元はその場合に自分自身の標準出力へ
     *  直接表示してください。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  内部状態は cplat_elevated_process_extract_result_target() が設定したものを参照します。
     */
    CPLAT_EXPORT int CPLAT_API cplat_elevated_process_report_result(const char *message);

    /**
     *  @brief          管理者/root 権限が必要な処理のため、必要に応じて昇格実行し、
     *                  昇格プロセスの標準出力と標準エラー出力を呼び出し元で受け取ります。
     *  @param[in]      arguments  昇格実行時に現在の実行ファイルへ渡す引数文字列。NULL 可。
     *  @param[in]      output_fn  昇格プロセスの出力を受け取るコールバック。NULL 可。
     *                             NULL の場合は、呼び出し元の標準出力と標準エラー出力へそのまま書き込みます。
     *  @param[in]      context    @p output_fn へ渡す任意のコンテキスト。NULL 可。
     *  @param[out]     exit_code  昇格プロセスの終了コード、または 0 の格納先。
     *  @param[out]     handled    昇格プロセスで処理した場合は 0 以外、現プロセスで継続する場合は 0 の格納先。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_OUT_OF_MEMORY 、@ref CPLAT_ERR_UNSUPPORTED 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  Windows では、未昇格の場合に標準出力用と標準エラー出力用の無名パイプを作成し、
     *  UAC を要求して現在の実行ファイルを @p arguments 付きで再起動します。
     *  子プロセスの終了まで待機し、その間にパイプから読み取った出力を @p output_fn へ渡します。\n
     *  すでに昇格済みの場合は何もしません。\n
     *  昇格プロセスは親のコンソールへ接続し直さないため、cplat_elevated_process_run_if_needed() で
     *  発生する間欠的な書き込み不能 (`ERROR_INVALID_HANDLE`) の影響を受けません。
     *  また、呼び出し元の標準出力がリダイレクトされている場合も、昇格プロセスの出力はそのリダイレクト先へ届きます。\n
     *  昇格プロセス側は起動直後に cplat_elevated_process_attach_output_pipes() を呼び出してください。
     *  呼び出さない場合、昇格プロセスの出力は呼び出し元へ届きません。\n
     *  未昇格かつセッション 0 (非対話セッション) の場合は、昇格を試みず失敗を返します。\n
     *  Linux では cplat_elevated_process_run_if_needed() と同じ判定を行い、@p output_fn は呼び出しません。
     *
     *  @note           標準出力と標準エラー出力は別のパイプで受け取るため、両者の間の出力順序は保証しません。\n
     *                  昇格プロセスへ標準入力は渡しません。未昇格のプロセスから昇格プロセスを操作できないようにするためです。\n
     *                  Windows で UAC を表示するため、通常はメイン スレッドまたはユーザー操作に応答するスレッドから呼び出してください。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。\n
     *  @p output_fn は本関数を呼び出したスレッドから呼び出されます。
     */
    CPLAT_EXPORT int CPLAT_API cplat_elevated_process_run_piped(const char *arguments,
                                                                cplat_elevated_process_output_fn output_fn,
                                                                void *context, int *exit_code, int *handled);

    /**
     *  @brief          昇格プロセスの標準出力と標準エラー出力を、呼び出し元プロセスのパイプへ接続します。
     *  @param[in,out]  argc          引数の数へのポインター。NULL 可。
     *  @param[in,out]  argv          引数配列。NULL 可。
     *  @param[out]     attached_out  パイプへ接続した場合は 1、そうでない場合は 0 の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  cplat_elevated_process_run_piped() が付与した内部フラグを検出し、@p argv から取り除いて
     *  @p argc を 1 減らします。フラグがない場合は何もせず @ref CPLAT_OK を返します。\n
     *  フラグを検出した場合は、呼び出し元プロセスが作成したパイプを本プロセスへ複製し、
     *  Win32 の標準ハンドルと CRT の stdout / stderr をそのパイプへ付け替えます。
     *  stdout はバッファーなしに設定し、改行コードを変換しないバイナリ モードで開きます。
     *  標準入力は `NUL` デバイスへ付け替え、読み取りは直ちにファイル終端となります。\n
     *  フラグの値が不正な場合、またはパイプの複製と付け替えに失敗した場合は
     *  @ref CPLAT_ERR_UNKNOWN を返します。この場合も、フラグは @p argv から取り除きます。\n
     *  プログラム開始直後、引数解析および cplat_console_init() より前に 1 度だけ呼び出してください。\n
     *  Linux では何もせず @p attached_out に 0 を設定します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  プロセス起動直後のシングル スレッド フェーズで呼び出してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_elevated_process_attach_output_pipes(int *argc, char **argv, int *attached_out);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_ELEVATED_PROCESS_H */
