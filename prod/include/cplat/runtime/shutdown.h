/**
 *******************************************************************************
 *  @file           shutdown.h
 *  @brief          プロセスを終了する共通 API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/05/06
 *
 *  通常終了時の cleanup callback と、補足可能な終了要求の request callback を提供します。\n
 *  いずれも登録したコールバックは LIFO 順で 1 回だけ実行されます。\n
 *  `atexit()` 経路では C 標準の制約により `exit(code)` の引数を直接取得できません。\n
 *  終了コードを確実に渡したい場合は `cplat_exit()` を使用してください。\n
 *  `TerminateProcess`、`_exit`、クラッシュ、強制 kill などの補足不能な終了は保証対象外です。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CPLAT_RUNTIME_SHUTDOWN_H
#define CPLAT_RUNTIME_SHUTDOWN_H

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
     *  @enum           cplat_shutdown_reason
     *  @brief          終了理由の種別です。
     */
    typedef enum cplat_shutdown_reason
    {
        CPLAT_SHUTDOWN_REASON_NORMAL_EXIT = 0,            /**< 通常終了。 */
        CPLAT_SHUTDOWN_REASON_PROCESS_TERMINATING = 1,    /**< 終了処理中で待機を避けるべき終了。 */
        CPLAT_SHUTDOWN_REASON_SIGNAL_OR_CONSOLE_EVENT = 2 /**< シグナルまたはコンソール イベント。 */
    } cplat_shutdown_reason;

    /**
     *  @enum           cplat_shutdown_code_kind
     *  @brief          終了イベントに付随する数値コードの意味です。
     */
    typedef enum cplat_shutdown_code_kind
    {
        CPLAT_SHUTDOWN_CODE_KIND_NONE = 0,             /**< 追加コードなし。 */
        CPLAT_SHUTDOWN_CODE_KIND_EXIT_CODE = 1,        /**< `exit(code)` の終了コード。 */
        CPLAT_SHUTDOWN_CODE_KIND_SIGNAL_NUMBER = 2,    /**< `SIGINT` などのシグナル番号。 */
        CPLAT_SHUTDOWN_CODE_KIND_CONSOLE_CTRL_TYPE = 3 /**< Windows `CTRL_*_EVENT`。 */
    } cplat_shutdown_code_kind;

    /**
     *  @brief          終了イベント情報です。
     */
    typedef struct cplat_shutdown_event
    {
        cplat_shutdown_reason reason;       /**< 終了理由。 */
        cplat_shutdown_code_kind code_kind; /**< `code` の意味。 */
        int code;                           /**< 終了コード、シグナル番号、CTRL 種別。 */
    } cplat_shutdown_event;

    /**
     *  @brief          終了コールバック関数型です。
     *
     *  通常終了時のコールバックで終了コードが必要な場合は、`cplat_exit()` を使用してください。\n
     *  `exit()` の直接呼び出しまたは `main()` の戻り値による終了では、終了コードを取得できません。
     *
     *  @param[in]      event   終了イベント情報。
     *  @param[in]      context 登録時に渡した任意のコンテキスト。
     *
     *  @par            スレッド セーフ
     *  コールバックはシャットダウン ハンドラーから 1 スレッドで呼び出されます。\n
     *  コールバック内で再帰的にシャットダウン処理を呼び出さないでください。
     */
    typedef void (*cplat_shutdown_fn)(const cplat_shutdown_event *event, void *context);

/**
 *  @brief          `cplat_exit()` が範囲外の終了コードを差し替える際に使う予約値です。
 *
 *  アプリケーションが `cplat_exit()` へ指定できる終了コードは `0` から
 *  `CPLAT_EXIT_CODE_RESERVED_OUT_OF_RANGE - 1` (`124`) までです。\n
 *  この値自体はアプリケーション固有の終了コードとして使用しないでください。
 */
#define CPLAT_EXIT_CODE_RESERVED_OUT_OF_RANGE 125

    /**
     *  @brief          終了コールバックを登録します。
     *
     *  登録済みコールバックは、通常終了または補足可能な終了イベント時に LIFO 順で
     *  1 回だけ実行されます。\n
     *  shutdown 開始後の登録は失敗します。
     *
     *  @param[in]      callback 実行するコールバック。
     *  @param[in]      context  コールバックへ渡す任意ポインター。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_OUT_OF_MEMORY 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_shutdown_register(cplat_shutdown_fn callback, void *context);

    /**
     *  @brief          終了要求 callback を登録します。
     *
     *  `SIGINT` / `SIGTERM` / `CTRL_C_EVENT` など、補足可能な終了要求で
     *  LIFO 順に 1 回だけ実行されます。\n
     *  request callback の実行後も final shutdown callback は未実行のまま保持され、
     *  通常終了時に別途実行されます。\n
     *  shutdown 開始後または終了要求通知後の登録は失敗します。
     *
     *  @param[in]      callback 実行するコールバック。
     *  @param[in]      context  コールバックへ渡す任意ポインター。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_OUT_OF_MEMORY 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_shutdown_request_register(cplat_shutdown_fn callback, void *context);

    /**
     *  @brief          終了コードを記録して `exit(code)` を実行します。
     *
     *  本関数は、`cplat_shutdown_register()` で登録した終了コールバックへ終了コードを渡します。\n
     *  イベントの `code_kind` は `CPLAT_SHUTDOWN_CODE_KIND_EXIT_CODE` です。\n
     *  イベントの `code` は @p code です。範囲外指定により差し替えが発生した場合は、差し替え後の値になります。
     *
     *  @note
     *  @parblock
     *  Linux と Windows の両方で動作し、シェルから利用される CLI では、独自に定義する終了コードに
     *  0-124 の範囲を推奨します。
     *
     *  **推奨する終了コード**
     *
     *  - 0: 正常終了。
     *  - 1-124: アプリケーション固有の失敗。
     *  - 125: 範囲外の値を指定した場合に自動的に差し替わる予約値 (@ref CPLAT_EXIT_CODE_RESERVED_OUT_OF_RANGE)。
     *
     *  @p code に `0`-`124` の範囲外 (負数、`125` 以上) を指定した場合、本関数は
     *  @ref CPLAT_EXIT_CODE_RESERVED_OUT_OF_RANGE (`125`) へ差し替えたうえで `exit()` します。\n
     *  これは、POSIX の `wait()`/`waitpid()` が終了コードの下位 8 bit しか呼び出し元へ伝えないため、
     *  例えば `256` を指定すると呼び出し元からは `0` (正常終了) として観測されてしまう問題への対策です。\n
     *  それ以外の範囲検証は行わないため、呼び出し側がクロスプラットフォームの終了コード規約として
     *  0-124 を使用してください。
     *
     *  **0-124 を推奨する理由**
     *
     *  POSIX の `wait()` と `waitpid()` から利用できる通常終了コードは、`exit()` などへ渡した値の
     *  下位 8 bit です。\n
     *  そのため、256 以上の値は呼び出し側が指定した値とは異なる終了コードとして取得され、
     *  256 は 0 として取得されます。
     *
     *  Bash が扱う終了ステータスの範囲は 0-255 ですが、126 はコマンドを実行できない場合、
     *  127 はコマンドが見つからない場合に使用されます。\n
     *  Bash は致命的なシグナル N でコマンドが終了した場合に 128+N を使用するため、128 以上も
     *  アプリケーション固有の終了コードとの区別が難しくなります。
     *
     *  **Windows との共通利用**
     *
     *  Windows の `ExitProcess()` は `UINT` 型の終了コードを受け取り、`GetExitCodeProcess()` は
     *  プロセスの終了ステータスを `DWORD` 型で取得します。\n
     *  Windows API 間では 32 bit の値を利用できますが、POSIX とシェルを含む共通の CLI 契約では、
     *  その範囲を前提にできません。\n
     *  負数や 256 以上の値は、OS や呼び出し側によって切り捨てまたは符号の解釈が異なります。
     *  `GetExitCodeProcess()` が実行中を表す `STILL_ACTIVE` として使用する 259 も、
     *  アプリケーション固有の終了コードには使用しません。
     *
     *  **詳細なエラー情報**
     *
     *  終了コードには、成功か失敗か、および失敗の大分類だけを割り当てます。\n
     *  詳細なエラー情報は、用途に応じて次の出力先へ記録します。
     *
     *  - 標準エラー出力: 利用者が読むエラー メッセージ。
     *  - ログ: OS エラー コードや内部状態などの診断情報。
     *  - IPC または結果ファイル: 呼び出し側が機械処理する構造化された結果。
     *
     *  `errno`、`GetLastError()`、`HRESULT`、`NTSTATUS` などの値を終了コードとして直接使用すると、
     *  POSIX で値が失われたり、呼び出し側の符号解釈が変わったりします。\n
     *  これらの値は終了コードへ変換せず、標準エラー出力、ログ、IPC などへ記録してください。
     *
     *  **参考資料**
     *
     *  - [POSIX.1-2024 `_Exit`](https://pubs.opengroup.org/onlinepubs/9799919799/functions/_exit.html)
     *  - [GNU Bash Reference Manual: Exit Status](https://www.gnu.org/software/bash/manual/html_node/Exit-Status.html)
     *  - [Microsoft Learn: `ExitProcess` function](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-exitprocess)
     *  - [Microsoft Learn: `GetExitCodeProcess` function](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getexitcodeprocess)
     *  @endparblock
     *
     *  @param[in]      code 終了コード。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  プロセス内で 1 スレッドのみが呼び出してください。
     */
    CPLAT_EXPORT void CPLAT_API cplat_exit(int code);

    /**
     *  @brief          テスト用に任意の終了イベントを同期実行します。
     *
     *  実アプリケーションでは使用しません。登録済みコールバックを 1 回だけ実行します。
     *
     *  @param[in]      event       実行に使用する終了イベント。
     *  @param[out]     invoked_out コールバックを実行した場合は 1、すでに実行済みで何もしなかった場合は 0 の格納先。
     *                              NULL 可。戻り値が @ref CPLAT_OK の場合のみ有効です。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_shutdown_invoke_for_test(const cplat_shutdown_event *event, int *invoked_out);

    /**
     *  @brief          テスト用に終了要求 callback を同期実行します。
     *
     *  実アプリケーションでは使用しません。登録済み request callback を 1 回だけ実行します。
     *
     *  @param[in]      event       実行に使用する終了イベント。
     *  @param[out]     invoked_out コールバックを実行した場合は 1、すでに実行済みで何もしなかった場合は 0 の格納先。
     *                              NULL 可。戻り値が @ref CPLAT_OK の場合のみ有効です。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_shutdown_request_invoke_for_test(const cplat_shutdown_event *event,
                                                                      int *invoked_out);

    /**
     *  @brief          テスト用に shutdown ランタイムの内部状態を初期化します。
     *
     *  すでに登録済みの callback は破棄されます。\n
     *  既存モジュール側の `call_once` 状態までは巻き戻しません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void CPLAT_API cplat_shutdown_reset_for_test(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_RUNTIME_SHUTDOWN_H */
