/**
 *******************************************************************************
 *  @file           abort.h
 *  @brief          継続が危険な状態でプロセスを異常終了する API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/10/12
 *
 *  異常終了の入口は 2 つあります。\n
 *  @ref CPLAT_ASSERT は、条件が偽の場合に異常終了します。\n
 *  @ref CPLAT_ABORT は、条件式で表せない状態 (論理的に到達しない分岐、内部の同期の失敗など) で、
 *  理由を示して無条件に異常終了します。\n
 *  どちらも失敗位置を標準エラー出力へ書き出し、登録済みのコールバックを LIFO 順で呼び出したあと
 *  `abort()` でプロセスを終了します。\n
 *  標準の `assert` と異なり、`NDEBUG` の定義有無に関わらず常に有効です。\n
 *  コールバックは、異常終了時のロギングなど、利用側の処理を外部から注入するために使用します。
 *
 *  @par            終了コード
 *  Linux では `SIGABRT` によって終了し、Bash では 134 (128 + 6) として観測されます。\n
 *  Windows では、Windows エラー報告 (WER) を呼び出さない場合、`abort()` は終了コード 3 でプロセスを終了します。\n
 *  WER を呼び出さないのは、デバッグ版ランタイムでビルドした場合や、
 *  `_set_abort_behavior()` で `_CALL_REPORTFAULT` を外した場合です。\n
 *  WER を呼び出す場合 (非デバッグ ビルドの既定) の終了コードは、OS が定める値です。\n
 *  異常終了と区別できるよう、アプリケーションは 3 を独自の終了コードに使用しないでください。\n
 *  see: https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/abort \n
 *  see: https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/set-abort-behavior
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CPLAT_RUNTIME_ABORT_H
#define CPLAT_RUNTIME_ABORT_H

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
     *  @enum           cplat_abort_kind
     *  @brief          異常終了に至った入口の種別です。
     */
    typedef enum cplat_abort_kind
    {
        CPLAT_ABORT_KIND_ASSERTION_FAILED = 1, /**< @ref CPLAT_ASSERT の条件が偽。`message` は条件式の文字列。 */
        CPLAT_ABORT_KIND_REQUESTED = 2         /**< @ref CPLAT_ABORT の呼び出し。`message` は指定した理由。 */
    } cplat_abort_kind;

    /**
     *  @brief          異常終了の理由と発生位置を表すイベント情報です。
     *
     *  文字列はすべて静的記憶域を指します。コールバックの終了後も有効ですが、変更しないでください。
     */
    typedef struct cplat_abort_event
    {
        const char *message;   /**< 偽と評価された条件式、または指定した理由の文字列。 */
        const char *file;      /**< 異常終了を記述したソース ファイル名。 */
        const char *function;  /**< 異常終了を記述した関数名。 */
        int line;              /**< 異常終了を記述した行番号。 */
        cplat_abort_kind kind; /**< 異常終了に至った入口の種別。`message` の意味を表します。 */
    } cplat_abort_event;

    /**
     *  @brief          異常終了時に呼び出すコールバック関数型です。
     *
     *  @param[in]      event   異常終了の理由と発生位置。
     *  @param[in]      context 登録時に渡した任意のコンテキスト。
     *
     *  コールバックが戻ると、次に古いコールバックを呼び出し、すべて呼び出したあと `abort()` します。\n
     *  コールバックから異常終了を取り消すことはできません。
     *
     *  @par            スレッド セーフ
     *  コールバックは、異常終了を始めたスレッドから呼び出されます。\n
     *  異常終了の処理中に別のスレッドが異常終了を始めた場合、そのスレッドは処理の完了を待ち、
     *  コールバックを重ねて呼び出しません。
     *
     *  @warning        コールバックはプロセスの内部状態が壊れている可能性を前提に、
     *                  短時間で戻る処理だけを行ってください。\n
     *                  コールバックが戻らない場合、プロセスは終了しません。\n
     *                  コールバック内で再び異常終了を始めた場合は、
     *                  残りのコールバックを呼び出さずに直ちに `abort()` します。
     */
    typedef void (*cplat_abort_fn)(const cplat_abort_event *event, void *context);

    /**
     *  @brief          異常終了時のコールバックを登録します。
     *
     *  登録済みのコールバックは、@ref CPLAT_ASSERT の条件が偽になったとき、または @ref CPLAT_ABORT を
     *  呼び出したときに、LIFO 順で 1 回だけ呼び出されます。\n
     *  登録の解除はできません。
     *
     *  @param[in]      callback 呼び出すコールバック。NULL を渡してはなりません。
     *  @param[in]      context  コールバックへ渡す任意ポインター。NULL を指定できます。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_OUT_OF_MEMORY のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_abort_register(cplat_abort_fn callback, void *context);

    /**
     *  @brief          異常終了の処理を実行します。
     *
     *  理由と発生位置を標準エラー出力へ書き出し、登録済みのコールバックを LIFO 順で呼び出したあと、
     *  `abort()` でプロセスを終了します。\n
     *  本関数は戻りません。
     *
     *  @param[in]      kind     異常終了に至った入口の種別。
     *  @param[in]      message  条件式または理由の文字列。静的記憶域を指します。NULL を指定できます。
     *  @param[in]      file     ソース ファイル名。NULL を指定できます。
     *  @param[in]      line     行番号。
     *  @param[in]      function 関数名。NULL を指定できます。
     *
     *  @attention      本関数はプロセスを終了させます。
     *
     *  @note           通常は @ref CPLAT_ASSERT または @ref CPLAT_ABORT から呼び出します。\n
     *                  引数の NULL は、異常終了の処理を妨げないよう空文字列として扱います。\n
     *                  @p kind が定義外の値の場合も、異常終了の処理を続けます。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT NO_RETURN void CPLAT_API cplat_abort_at(cplat_abort_kind kind, const char *message, const char *file,
                                                         int line, const char *function);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/**
 *  @brief          条件が偽の場合にプロセスを異常終了します。
 *
 *  @param[in]      condition 検査する条件式。真であることを期待します。
 *
 *  @p condition が偽の場合は cplat_abort_at() を呼び出し、プロセスを終了します。\n
 *  `NDEBUG` の定義有無に関わらず、常に @p condition を 1 回評価します。
 *
 *  継続が危険な状態 (内部状態の破壊の伝播、誤ったデータの書き出しなど) の検出に限って使用します。\n
 *  引数の不正、外部入力の不正、ファイルや通信やメモリ確保の失敗など、呼び出し元が回復できる失敗には使用しません。\n
 *  条件式で表せない状態には @ref CPLAT_ABORT を使用します。`CPLAT_ASSERT(0)` とは書きません。
 *
 *  @par            使用例
 *  @code{.c}
 *  // 内部状態が壊れている場合、続行すると誤ったオフセットへ書き出す
 *  CPLAT_ASSERT(writer->magic == SAMPLE_WRITER_MAGIC);
 *  @endcode
 */
#define CPLAT_ASSERT(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            cplat_abort_at(CPLAT_ABORT_KIND_ASSERTION_FAILED, #condition, __FILE__, __LINE__, __func__); \
        } \
    } while (0)

/**
 *  @brief          理由を示してプロセスを無条件に異常終了します。
 *
 *  @param[in]      message 異常終了の理由。文字列リテラルだけを指定できます。
 *
 *  cplat_abort_at() を呼び出し、プロセスを終了します。\n
 *  論理的に到達しない分岐や、失敗すると排他や内部状態が崩れる処理の失敗など、
 *  条件式で表せない継続が危険な状態に限って使用します。\n
 *  条件式で表せる場合は @ref CPLAT_ASSERT を使用します。
 *
 *  @p message を文字列リテラルに限るのは、異常終了の処理中に文字列の組み立てや確保を行わず、
 *  コールバックの終了後も有効な静的記憶域を指すためです。\n
 *  文字列リテラル以外を指定すると、コンパイル エラーになります。
 *
 *  @par            使用例
 *  @code{.c}
 *  switch (state)
 *  {
 *  case SAMPLE_STATE_OPEN:
 *      return sample_close(handle);
 *  default:
 *      // 状態は sample_open() と sample_close() だけが遷移させ、これ以外の値にならない
 *      CPLAT_ABORT("sample handle の状態が不正です");
 *  }
 *  @endcode
 */
#define CPLAT_ABORT(message) cplat_abort_at(CPLAT_ABORT_KIND_REQUESTED, "" message "", __FILE__, __LINE__, __func__)

/** @} */

#endif /* CPLAT_RUNTIME_ABORT_H */
