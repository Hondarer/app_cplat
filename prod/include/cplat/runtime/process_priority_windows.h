/**
 *******************************************************************************
 *  @file           process_priority_windows.h
 *  @brief          Windows のプロセスとスレッドの優先度を設定・取得する API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/10/26
 *
 *  Windows 専用の API です。呼び出し元は `#if defined(PLATFORM_WINDOWS)` の中でのみ使用してください。\n
 *  優先度クラス、バックグラウンド処理モード、メモリ優先度、電力スロットリング (EcoQoS)、
 *  優先度ブースト、スレッド優先度を、軸ごとの関数で設定・取得します。
 *
 *  @par            対象の指定
 *  プロセス単位の関数は @p process_id 、スレッド単位の関数は @p thread_id で対象を指定します。\n
 *  0 は自プロセスまたは呼び出しスレッドを表します。それ以外の ID は、関数内で対象を開いてから操作し、
 *  関数を抜ける前に閉じます。\n
 *  プロセス ID とスレッド ID は終了後に再利用されるため、指定した ID が意図と別の対象を指す場合があります。
 *
 *  @par            結果コード
 *  @parblock
 *  失敗時は @p detail_out とスレッド ローカルの直前値 (cplat_error_get_last()) に Windows エラーを記録し、
 *  成功時と省略時は両方をクリアします。
 *
 *  | 状況 | 結果コード |
 *  | --- | --- |
 *  | 対象を開けない、または特権が不足する (`ERROR_ACCESS_DENIED`、`ERROR_PRIVILEGE_NOT_HELD`) | @ref CPLAT_ERR_PERMISSION_DENIED |
 *  | 対象が存在しない (対象を開くときの `ERROR_INVALID_PARAMETER`) | @ref CPLAT_ERR_NOT_FOUND |
 *  | OS が機能に対応しない (`ERROR_NOT_SUPPORTED` など) | @ref CPLAT_ERR_UNSUPPORTED |
 *  | 引数の誤り | @ref CPLAT_ERR_INVALID_ARGUMENT |
 *
 *  Table: 主な失敗と結果コード
 *
 *  取得関数の出力引数は、失敗時に変更しません。
 *  @endparblock
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CPLAT_PROCESS_PRIORITY_WINDOWS_H
#define CPLAT_PROCESS_PRIORITY_WINDOWS_H

#include <stdint.h>
#include <cplat/base/platform.h>
#include <cplat/base/error.h>
#include <cplat/base/result.h>
#include <cplat/cplat_export.h>

/**
 *  @ingroup        CPLAT_RUNTIME
 *  @{
 */

#if defined(PLATFORM_WINDOWS)

    #ifdef __cplusplus
extern "C"
{
    #endif /* __cplusplus */

    /**
     *  @brief          プロセスの優先度クラスです。
     */
    typedef enum cplat_process_priority_windows_class
    {
        CPLAT_PROCESS_PRIORITY_WINDOWS_CLASS_IDLE = 0,         /**< `IDLE_PRIORITY_CLASS`。 */
        CPLAT_PROCESS_PRIORITY_WINDOWS_CLASS_BELOW_NORMAL = 1, /**< `BELOW_NORMAL_PRIORITY_CLASS`。 */
        CPLAT_PROCESS_PRIORITY_WINDOWS_CLASS_NORMAL = 2,       /**< `NORMAL_PRIORITY_CLASS`。 */
        CPLAT_PROCESS_PRIORITY_WINDOWS_CLASS_ABOVE_NORMAL = 3, /**< `ABOVE_NORMAL_PRIORITY_CLASS`。 */
        CPLAT_PROCESS_PRIORITY_WINDOWS_CLASS_HIGH = 4,         /**< `HIGH_PRIORITY_CLASS`。 */
        CPLAT_PROCESS_PRIORITY_WINDOWS_CLASS_REAL_TIME = 5     /**< `REALTIME_PRIORITY_CLASS`。 */
    } cplat_process_priority_windows_class;

    /**
     *  @brief          プロセスのメモリ優先度です。
     */
    typedef enum cplat_process_priority_windows_memory_priority
    {
        CPLAT_PROCESS_PRIORITY_WINDOWS_MEMORY_PRIORITY_VERY_LOW = 1,     /**< `MEMORY_PRIORITY_VERY_LOW`。 */
        CPLAT_PROCESS_PRIORITY_WINDOWS_MEMORY_PRIORITY_LOW = 2,          /**< `MEMORY_PRIORITY_LOW`。 */
        CPLAT_PROCESS_PRIORITY_WINDOWS_MEMORY_PRIORITY_MEDIUM = 3,       /**< `MEMORY_PRIORITY_MEDIUM`。 */
        CPLAT_PROCESS_PRIORITY_WINDOWS_MEMORY_PRIORITY_BELOW_NORMAL = 4, /**< `MEMORY_PRIORITY_BELOW_NORMAL`。 */
        CPLAT_PROCESS_PRIORITY_WINDOWS_MEMORY_PRIORITY_NORMAL = 5        /**< `MEMORY_PRIORITY_NORMAL`。 */
    } cplat_process_priority_windows_memory_priority;

    /**
     *  @brief          プロセスの電力スロットリング (実行速度の抑制) の状態です。
     */
    typedef enum cplat_process_priority_windows_power_throttling
    {
        CPLAT_PROCESS_PRIORITY_WINDOWS_POWER_THROTTLING_SYSTEM_MANAGED = 0, /**< OS に任せます。 */
        CPLAT_PROCESS_PRIORITY_WINDOWS_POWER_THROTTLING_ENABLED = 1,        /**< 常に抑制します (EcoQoS)。 */
        CPLAT_PROCESS_PRIORITY_WINDOWS_POWER_THROTTLING_DISABLED = 2        /**< 常に抑制しません。 */
    } cplat_process_priority_windows_power_throttling;

    /**
     *  @brief          優先度クラス内のスレッドの相対優先度です。
     */
    typedef enum cplat_process_priority_windows_thread_priority
    {
        CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_IDLE = 0,          /**< `THREAD_PRIORITY_IDLE`。 */
        CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_LOWEST = 1,        /**< `THREAD_PRIORITY_LOWEST`。 */
        CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_BELOW_NORMAL = 2,  /**< `THREAD_PRIORITY_BELOW_NORMAL`。 */
        CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_NORMAL = 3,        /**< `THREAD_PRIORITY_NORMAL`。 */
        CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_ABOVE_NORMAL = 4,  /**< `THREAD_PRIORITY_ABOVE_NORMAL`。 */
        CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_HIGHEST = 5,       /**< `THREAD_PRIORITY_HIGHEST`。 */
        CPLAT_PROCESS_PRIORITY_WINDOWS_THREAD_PRIORITY_TIME_CRITICAL = 6  /**< `THREAD_PRIORITY_TIME_CRITICAL`。 */
    } cplat_process_priority_windows_thread_priority;

    /**
     *  @brief          プロセスの優先度クラスを設定します。
     *  @param[in]      process_id      対象のプロセス ID。0 は自プロセスです。
     *  @param[in]      priority_class  優先度クラス。
     *  @param[out]     detail_out      エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                                  エラー詳細を設定せず、返却しません。
     *                                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_PERMISSION_DENIED 、
     *                  @ref CPLAT_ERR_NOT_FOUND 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  REAL_TIME には特権 `SeIncreaseBasePriorityPrivilege` が必要です。\n
     *  特権がない場合、OS は失敗を返さずに HIGH を設定するため、本関数は設定後のクラスを確認します。
     *  REAL_TIME になっていなければ元のクラスへ戻し、@p detail_out に `ERROR_PRIVILEGE_NOT_HELD` を記録して
     *  @ref CPLAT_ERR_PERMISSION_DENIED を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     *  ただし、同じプロセスの優先度クラスを他のスレッドが同時に変更する場合、REAL_TIME の確認と復元の
     *  結果は保証されません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_process_priority_windows_set_class(
        uint32_t process_id, cplat_process_priority_windows_class priority_class, cplat_error *detail_out);

    /**
     *  @brief          プロセスの優先度クラスを取得します。
     *  @param[in]      process_id          対象のプロセス ID。0 は自プロセスです。
     *  @param[out]     priority_class_out  優先度クラスの格納先。NULL を渡してはなりません。
     *  @param[out]     detail_out          エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                                      エラー詳細を設定せず、返却しません。
     *                                      NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_PERMISSION_DENIED 、
     *                  @ref CPLAT_ERR_NOT_FOUND 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  OS が返した値が列挙型にない場合は @ref CPLAT_ERR_UNKNOWN を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_process_priority_windows_get_class(
        uint32_t process_id, cplat_process_priority_windows_class *priority_class_out, cplat_error *detail_out);

    /**
     *  @brief          自プロセスのバックグラウンド処理モードを開始します。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                              エラー詳細を設定せず、返却しません。
     *                              NULL 以外を指定した場合、成功時と省略時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_SKIPPED 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  CPU に加えて、I/O とメモリの優先度も下げます。自プロセスだけに適用できます。\n
     *  すでにモード中の場合は何もせず @ref CPLAT_SKIPPED を返します。\n
     *  現在モード中かを取得する公開 API はありません。本関数が @ref CPLAT_OK を返した場合だけ、
     *  処理後に cplat_process_priority_windows_end_background_mode() を呼び出してください。
     *
     *  @attention      戻り値を `CPLAT_OK` との一致だけで判定すると、@ref CPLAT_SKIPPED を失敗として扱います。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_process_priority_windows_begin_background_mode(cplat_error *detail_out);

    /**
     *  @brief          自プロセスのバックグラウンド処理モードを終了します。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                              エラー詳細を設定せず、返却しません。
     *                              NULL 以外を指定した場合、成功時と省略時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_SKIPPED 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  モード中でない場合は何もせず @ref CPLAT_SKIPPED を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_process_priority_windows_end_background_mode(cplat_error *detail_out);

    /**
     *  @brief          プロセスのメモリ優先度を設定します。
     *  @param[in]      process_id       対象のプロセス ID。0 は自プロセスです。
     *  @param[in]      memory_priority  メモリ優先度。
     *  @param[out]     detail_out       エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                                   エラー詳細を設定せず、返却しません。
     *                                   NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_PERMISSION_DENIED 、
     *                  @ref CPLAT_ERR_NOT_FOUND 、@ref CPLAT_ERR_UNSUPPORTED 、@ref CPLAT_ERR_UNKNOWN の
     *                  いずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_process_priority_windows_set_memory_priority(
        uint32_t process_id, cplat_process_priority_windows_memory_priority memory_priority, cplat_error *detail_out);

    /**
     *  @brief          プロセスのメモリ優先度を取得します。
     *  @param[in]      process_id           対象のプロセス ID。0 は自プロセスです。
     *  @param[out]     memory_priority_out  メモリ優先度の格納先。NULL を渡してはなりません。
     *  @param[out]     detail_out           エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                                       エラー詳細を設定せず、返却しません。
     *                                       NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_PERMISSION_DENIED 、
     *                  @ref CPLAT_ERR_NOT_FOUND 、@ref CPLAT_ERR_UNSUPPORTED 、@ref CPLAT_ERR_UNKNOWN の
     *                  いずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_process_priority_windows_get_memory_priority(
        uint32_t process_id, cplat_process_priority_windows_memory_priority *memory_priority_out,
        cplat_error *detail_out);

    /**
     *  @brief          プロセスの電力スロットリング (実行速度の抑制) を設定します。
     *  @param[in]      process_id        対象のプロセス ID。0 は自プロセスです。
     *  @param[in]      power_throttling  電力スロットリングの状態。
     *  @param[out]     detail_out        エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                                    エラー詳細を設定せず、返却しません。
     *                                    NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_PERMISSION_DENIED 、
     *                  @ref CPLAT_ERR_NOT_FOUND 、@ref CPLAT_ERR_UNSUPPORTED 、@ref CPLAT_ERR_UNKNOWN の
     *                  いずれかを返します。
     *
     *  `PROCESS_POWER_THROTTLING_EXECUTION_SPEED` だけを扱います。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_process_priority_windows_set_power_throttling(
        uint32_t process_id, cplat_process_priority_windows_power_throttling power_throttling,
        cplat_error *detail_out);

    /**
     *  @brief          プロセスの電力スロットリング (実行速度の抑制) を取得します。
     *  @param[in]      process_id            対象のプロセス ID。0 は自プロセスです。
     *  @param[out]     power_throttling_out  電力スロットリングの状態の格納先。NULL を渡してはなりません。
     *  @param[out]     detail_out            エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                                        エラー詳細を設定せず、返却しません。
     *                                        NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_PERMISSION_DENIED 、
     *                  @ref CPLAT_ERR_NOT_FOUND 、@ref CPLAT_ERR_UNSUPPORTED 、@ref CPLAT_ERR_UNKNOWN の
     *                  いずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_process_priority_windows_get_power_throttling(
        uint32_t process_id, cplat_process_priority_windows_power_throttling *power_throttling_out,
        cplat_error *detail_out);

    /**
     *  @brief          プロセスの優先度ブーストを有効または無効にします。
     *  @param[in]      process_id  対象のプロセス ID。0 は自プロセスです。
     *  @param[in]      enabled     1 で有効、0 で無効にします。0 または 1 を指定します。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                              エラー詳細を設定せず、返却しません。
     *                              NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_PERMISSION_DENIED 、
     *                  @ref CPLAT_ERR_NOT_FOUND 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  優先度ブーストは、待機から復帰したスレッドの優先度を OS が一時的に引き上げる仕組みです。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_process_priority_windows_set_priority_boost(uint32_t process_id, int enabled,
                                                                                 cplat_error *detail_out);

    /**
     *  @brief          プロセスの優先度ブーストが有効かを取得します。
     *  @param[in]      process_id   対象のプロセス ID。0 は自プロセスです。
     *  @param[out]     enabled_out  有効なら 1、無効なら 0 の格納先。NULL を渡してはなりません。
     *  @param[out]     detail_out   エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                               エラー詳細を設定せず、返却しません。
     *                               NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_PERMISSION_DENIED 、
     *                  @ref CPLAT_ERR_NOT_FOUND 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_process_priority_windows_get_priority_boost(uint32_t process_id,
                                                                                 int *enabled_out,
                                                                                 cplat_error *detail_out);

    /**
     *  @brief          スレッドの相対優先度を設定します。
     *  @param[in]      thread_id        対象のスレッド ID。0 は呼び出しスレッドです。
     *  @param[in]      thread_priority  相対優先度。
     *  @param[out]     detail_out       エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                                   エラー詳細を設定せず、返却しません。
     *                                   NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_PERMISSION_DENIED 、
     *                  @ref CPLAT_ERR_NOT_FOUND 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_process_priority_windows_set_thread_priority(
        uint32_t thread_id, cplat_process_priority_windows_thread_priority thread_priority, cplat_error *detail_out);

    /**
     *  @brief          スレッドの相対優先度を取得します。
     *  @param[in]      thread_id            対象のスレッド ID。0 は呼び出しスレッドです。
     *  @param[out]     thread_priority_out  相対優先度の格納先。NULL を渡してはなりません。
     *  @param[out]     detail_out           エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                                       エラー詳細を設定せず、返却しません。
     *                                       NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_PERMISSION_DENIED 、
     *                  @ref CPLAT_ERR_NOT_FOUND 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  REAL_TIME クラス内でだけ使える値 (-7 〜 -3、3 〜 6) の場合は @ref CPLAT_ERR_UNKNOWN を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_process_priority_windows_get_thread_priority(
        uint32_t thread_id, cplat_process_priority_windows_thread_priority *thread_priority_out,
        cplat_error *detail_out);

    #ifdef __cplusplus
}
    #endif /* __cplusplus */

#endif /* PLATFORM_WINDOWS */

/** @} */

#endif /* CPLAT_PROCESS_PRIORITY_WINDOWS_H */
