/**
 *******************************************************************************
 *  @file           process.h
 *  @brief          プロセス情報を取得する API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/06/07
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CPLAT_PROCESS_H
#define CPLAT_PROCESS_H

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <cplat/base/platform.h>
#include <cplat/base/result.h>
#include <cplat/cplat_export.h>

/**
 *  @ingroup        CPLAT_RUNTIME
 *  @{
 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          現在のプロセスの実行ファイル本体の絶対パスを取得します。
     *  @param[out]     path_out      絶対パス (UTF-8) の格納先。NULL を渡してはなりません。
     *  @param[in]      path_size     @p path_out のサイズ (バイト)。0 を渡してはなりません。
     *  @retval         CPLAT_OK                    実行ファイルのパスを取得しました。
     *  @retval         CPLAT_ERR_INVALID_ARGUMENT  @p path_out が NULL、または @p path_size が 0 です。
     *  @retval         CPLAT_ERR_BUFFER_TOO_SMALL  @p path_out の容量が不足しています。
     *  @retval         CPLAT_ERR_UNSUPPORTED       現在のプラットフォームをサポートしていません。
     *  @return         上記以外の失敗時は、OS エラーを変換した共通結果コードを返します。
     *
     *  cplat_module_get_path() が関数アドレスの所属モジュールを返す (Windows では
     *  DLL を指しうる) のに対し、本関数は常にプロセス本体の実行ファイルのパスを返します。\n
     *  返されるパスは UTF-8 文字列で、パス セパレーターは '/' に統一されます。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_process_get_executable_path(char *path_out, size_t path_size);

    /**
     *  @brief          現在のプロセスの PID (プロセス ID) を取得します。
     *  @return         現在のプロセスの PID。
     *
     *  ログ出力やロック ファイル名の一意化など、診断目的で PID の数値を必要とする場合に使用します。\n
     *  プロセスの生成・待機・終了操作には、本関数の戻り値ではなく cplat_process_start() が
     *  返すハンドル (@ref cplat_process) を使用してください。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT uint32_t CPLAT_API cplat_process_get_pid(void);

    /**
     *  @brief          現在のスレッドの TID (スレッド ID) を取得します。
     *  @return         現在のスレッドの TID。
     *
     *  ログ出力など、診断目的でスレッドを識別する数値を必要とする場合に使用します。\n
     *  スレッドの生成・待機・切り離し操作には、本関数の戻り値ではなく cplat_thread_create() が
     *  返すハンドル (@ref cplat_thread) を使用してください。\n
     *  返る値は OS がスレッドへ割り当てた識別子であり、同時に存在するスレッドの間で一意です。\n
     *  終了したスレッドの識別子は、OS が後続のスレッドへ再び割り当てる場合があります。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT uint32_t CPLAT_API cplat_process_get_tid(void);

#define CPLAT_PROCESS_WAIT_FOREVER INT_MAX /**< タイムアウトなしで待機する (INT_MAX)。 */
#define CPLAT_PROCESS_NO_WAIT      0       /**< 即時リターン (タイムアウト 0 ms)。 */

    /** @brief 子プロセスの標準入出力ハンドルの扱い。 */
    typedef enum cplat_process_stdio_mode
    {
        CPLAT_PROCESS_STDIO_INHERIT = 0,      /**< 親プロセスの標準ハンドルを継承します。 */
        CPLAT_PROCESS_STDIO_NULL_DEVICE = 1,  /**< null device へ接続します。 */
        CPLAT_PROCESS_STDIO_NATIVE_HANDLE = 2 /**< native_handle で指定した OS ハンドルを使います。 */
    } cplat_process_stdio_mode;

    /**
     *  @brief          子プロセスの標準入出力指定です。
     *
     *  Linux では @p native_handle をファイル記述子、Windows では HANDLE として扱います。\n
     *  呼び出し側が渡した native handle の所有権は移動しません。
     */
    typedef struct cplat_process_stdio
    {
        cplat_process_stdio_mode mode; /**< 標準入出力の扱い。 */
        unsigned int pad;              /**< x64 で native_handle のアラインメントを明示するパディング。 */
        intptr_t native_handle;        /**< OS ネイティブ ハンドル。 */
    } cplat_process_stdio;

    /** @brief 子プロセス起動オプション。 */
    typedef struct cplat_process_options
    {
        char *const *argv;               /**< コマンドと引数の配列 (NULL 終端)。NULL を渡してはなりません。 */
        char *const *env_overrides;      /**< 追加・上書きする KEY=VALUE 配列 (NULL 終端)。NULL 可。 */
        const char *working_directory;   /**< 作業ディレクトリ。NULL の場合は親の作業ディレクトリを継承します。 */
        cplat_process_stdio stdin_spec;  /**< stdin 指定。 */
        cplat_process_stdio stdout_spec; /**< stdout 指定。 */
        cplat_process_stdio stderr_spec; /**< stderr 指定。 */
    } cplat_process_options;

    typedef struct cplat_process cplat_process; /**< 子プロセス ハンドル。 */

    /**
     *  @brief          子プロセスを起動します。
     *  @param[in]      options  起動オプション。NULL を渡してはなりません。
     *  @param[out]     process  起動したプロセス ハンドルの格納先。NULL を渡してはなりません。
     *  @return         結果コードを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_process_start(const cplat_process_options *options, cplat_process **process);

    /**
     *  @brief          子プロセスの終了を待機します。
     *  @param[in]      process     対象のプロセス ハンドル。NULL を渡してはなりません。
     *  @param[in]      timeout_ms  タイムアウト (ms)。@ref CPLAT_PROCESS_WAIT_FOREVER または
     *                              @ref CPLAT_PROCESS_NO_WAIT も指定可能です。
     *  @return         結果コードを返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p process に対する操作は同時に実行できます。\n
     *  同一 @p process に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_process_wait(cplat_process *process, int timeout_ms);

    /**
     *  @brief          子プロセスの終了コードを取得します。
     *  @param[in]      process    対象のプロセス ハンドル。NULL を渡してはなりません。
     *  @param[out]     exit_code  終了コードの格納先。NULL を渡してはなりません。
     *  @return         結果コードを返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p process に対する操作は同時に実行できます。\n
     *  同一 @p process に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_process_get_exit_code(cplat_process *process, int *exit_code);

    /**
     *  @brief          子プロセスを強制終了します。
     *  @param[in]      process  対象のプロセス ハンドル。NULL を渡してはなりません。
     *  @return         結果コードを返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p process に対する操作は同時に実行できます。\n
     *  同一 @p process に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_process_terminate(cplat_process *process);

    /**
     *  @brief          子プロセス ハンドルを破棄します。
     *  @param[in]      process  破棄するプロセス ハンドル。NULL 可。
     *
     *  実行中のプロセスは終了しません。\n
     *  実行中プロセスを終了する場合は、先に cplat_process_terminate() を呼び出してください。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p process に対する操作は同時に実行できます。\n
     *  同一 @p process に対する破棄操作および他の操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT void CPLAT_API cplat_process_dispose(cplat_process *process);

    /**
     *  @brief          子プロセスを起動し、終了まで同期的に待機します。
     *  @param[in]      options     起動オプション。NULL を渡してはなりません。
     *  @param[in]      timeout_ms  タイムアウト (ms)。
     *  @param[out]     exit_code   終了コードの格納先。NULL を渡してはなりません。
     *  @return         結果コードを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_process_run_sync(const cplat_process_options *options, int timeout_ms,
                                                      int *exit_code);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_PROCESS_H */
