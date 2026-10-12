/**
 *******************************************************************************
 *  @file           assertion.h
 *  @brief          継続が危険な状態を検出してプロセスを異常終了する API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/10/12
 *
 *  @ref CPLAT_ASSERT は、条件が偽の場合に失敗位置を標準エラー出力へ書き出し、
 *  登録済みのコールバックを LIFO 順で呼び出したあと `abort()` でプロセスを終了します。\n
 *  標準の `assert` と異なり、`NDEBUG` の定義有無に関わらず常に評価します。\n
 *  コールバックは、異常終了時のロギングなど、利用側の処理を外部から注入するために使用します。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CPLAT_RUNTIME_ASSERTION_H
#define CPLAT_RUNTIME_ASSERTION_H

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
     *  @brief          異常終了の発生位置を表すイベント情報です。
     *
     *  文字列はすべて静的記憶域を指します。コールバックの終了後も有効ですが、変更しないでください。
     */
    typedef struct cplat_assert_event
    {
        const char *expression; /**< 偽と評価された条件式の文字列。 */
        const char *file;       /**< 条件式を記述したソース ファイル名。 */
        int line;               /**< 条件式を記述した行番号。 */
        const char *function;   /**< 条件式を記述した関数名。 */
    } cplat_assert_event;

    /**
     *  @brief          異常終了時に呼び出すコールバック関数型です。
     *
     *  @param[in]      event   異常終了の発生位置。
     *  @param[in]      context 登録時に渡した任意のコンテキスト。
     *
     *  コールバックが戻ると、次に古いコールバックを呼び出し、すべて呼び出したあと `abort()` します。\n
     *  コールバックから異常終了を取り消すことはできません。
     *
     *  @par            スレッド セーフ
     *  コールバックは、条件が偽になったスレッドから呼び出されます。\n
     *  異常終了の処理中に別のスレッドで条件が偽になった場合、そのスレッドは処理の完了を待ち、
     *  コールバックを重ねて呼び出しません。
     *
     *  @warning        コールバックはプロセスの内部状態が壊れている可能性を前提に、
     *                  短時間で戻る処理だけを行ってください。\n
     *                  コールバックが戻らない場合、プロセスは終了しません。\n
     *                  コールバック内で @ref CPLAT_ASSERT の条件が偽になった場合は、
     *                  残りのコールバックを呼び出さずに直ちに `abort()` します。
     */
    typedef void (*cplat_assert_fn)(const cplat_assert_event *event, void *context);

    /**
     *  @brief          異常終了時のコールバックを登録します。
     *
     *  登録済みのコールバックは、@ref CPLAT_ASSERT の条件が偽になったときに LIFO 順で 1 回だけ呼び出されます。\n
     *  登録の解除はできません。
     *
     *  @param[in]      callback 呼び出すコールバック。NULL を渡してはなりません。
     *  @param[in]      context  コールバックへ渡す任意ポインター。NULL を指定できます。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_OUT_OF_MEMORY のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_assert_register(cplat_assert_fn callback, void *context);

    /**
     *  @brief          異常終了の処理を実行します。
     *
     *  失敗位置を標準エラー出力へ書き出し、登録済みのコールバックを LIFO 順で呼び出したあと、
     *  `abort()` でプロセスを終了します。\n
     *  本関数は戻りません。
     *
     *  @param[in]      expression 偽と評価された条件式の文字列。NULL を指定できます。
     *  @param[in]      file       ソース ファイル名。NULL を指定できます。
     *  @param[in]      line       行番号。
     *  @param[in]      function   関数名。NULL を指定できます。
     *
     *  @attention      本関数はプロセスを終了させます。
     *
     *  @note           通常は @ref CPLAT_ASSERT から呼び出します。\n
     *                  引数の NULL は、異常終了の処理を妨げないよう空文字列として扱います。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT NO_RETURN void CPLAT_API cplat_assert_fail_at(const char *expression, const char *file, int line,
                                                               const char *function);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/**
 *  @brief          条件が偽の場合にプロセスを異常終了します。
 *
 *  @param[in]      condition 検査する条件式。真であることを期待します。
 *
 *  @p condition が偽の場合は cplat_assert_fail_at() を呼び出し、プロセスを終了します。\n
 *  `NDEBUG` の定義有無に関わらず、常に @p condition を 1 回評価します。
 *
 *  継続が危険な状態 (内部状態の破壊の伝播、誤ったデータの書き出しなど) の検出に限って使用します。\n
 *  引数の不正、外部入力の不正、ファイルや通信やメモリ確保の失敗など、呼び出し元が回復できる失敗には使用しません。
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
            cplat_assert_fail_at(#condition, __FILE__, __LINE__, __func__); \
        } \
    } while (0)

/** @} */

#endif /* CPLAT_RUNTIME_ASSERTION_H */
