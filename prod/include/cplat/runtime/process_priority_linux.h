/**
 *******************************************************************************
 *  @file           process_priority_linux.h
 *  @brief          Linux のスケジューリングと優先度を設定・取得する API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/10/26
 *
 *  Linux 専用の API です。呼び出し元は `#if defined(PLATFORM_LINUX)` の中でのみ使用してください。\n
 *  スケジューリング ポリシーと静的優先度、デッド ライン スケジューリング、nice 値、I/O 優先度、
 *  CPU 使用率クランプを、軸ごとの関数で設定・取得します。
 *
 *  @par            対象の指定
 *  各関数の @p thread_id はスレッド ID (TID) です。0 は呼び出しスレッドを表します。\n
 *  Linux の優先度属性はすべてスレッド単位であり、PID を渡すとそのプロセスのメイン スレッドだけが
 *  変わります。0 と自プロセスの PID は意味が異なります。\n
 *  新しいスレッドは作成元スレッドの値を引き継ぐため、スレッドを作成する前に呼び出すと、
 *  以降に作成するスレッド全体に及びます。\n
 *  他プロセスの全スレッドを変更する場合は、`/proc/<pid>/task` を列挙して各スレッド ID を渡してください。\n
 *  PID とスレッド ID は終了後に再利用されるため、指定した ID が意図と別のスレッドを指す場合があります。\n
 *  `INT32_MAX` を超える ID は @ref CPLAT_ERR_INVALID_ARGUMENT になります。
 *
 *  @par            結果コード
 *  @parblock
 *  失敗時は @p detail_out とスレッド ローカルの直前値 (cplat_error_get_last()) に errno を記録し、
 *  成功時は両方をクリアします。
 *
 *  | 状況 | 結果コード |
 *  | --- | --- |
 *  | 権限不足 (`EPERM`、`EACCES`) | @ref CPLAT_ERR_PERMISSION_DENIED |
 *  | 対象が存在しない (`ESRCH`) | @ref CPLAT_ERR_NOT_FOUND |
 *  | カーネルが機能に対応しない | @ref CPLAT_ERR_UNSUPPORTED |
 *  | 引数の誤り (cplat の検査、`EINVAL`) | @ref CPLAT_ERR_INVALID_ARGUMENT |
 *  | デッド ラインの帯域不足 (`EBUSY`) | @ref CPLAT_ERR_BUSY |
 *
 *  Table: 主な失敗と結果コード
 *
 *  権限不足の原因 (ケーパビリティ、`RLIMIT_RTPRIO` / `RLIMIT_NICE`、cgroup のリアルタイム帯域) は、
 *  OS が同じ errno を返すため区別できません。\n
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

#ifndef CPLAT_PROCESS_PRIORITY_LINUX_H
#define CPLAT_PROCESS_PRIORITY_LINUX_H

#include <stdint.h>
#include <cplat/base/platform.h>
#include <cplat/base/error.h>
#include <cplat/base/result.h>
#include <cplat/cplat_export.h>

/**
 *  @ingroup        CPLAT_RUNTIME
 *  @{
 */

#if defined(PLATFORM_LINUX)

    #ifdef __cplusplus
extern "C"
{
    #endif /* __cplusplus */

    /**
     *  @brief          スケジューリング ポリシーです。
     */
    typedef enum cplat_process_priority_linux_policy
    {
        CPLAT_PROCESS_PRIORITY_LINUX_POLICY_OTHER = 0,       /**< 通常の時分割 (`SCHED_OTHER`)。 */
        CPLAT_PROCESS_PRIORITY_LINUX_POLICY_BATCH = 1,       /**< CPU 負荷の高いバッチ処理 (`SCHED_BATCH`)。 */
        CPLAT_PROCESS_PRIORITY_LINUX_POLICY_IDLE = 2,        /**< 非常に低い優先度 (`SCHED_IDLE`)。 */
        CPLAT_PROCESS_PRIORITY_LINUX_POLICY_FIFO = 3,        /**< リアルタイムの先入れ先出し (`SCHED_FIFO`)。 */
        CPLAT_PROCESS_PRIORITY_LINUX_POLICY_ROUND_ROBIN = 4, /**< リアルタイムのラウンド ロビン (`SCHED_RR`)。 */
        CPLAT_PROCESS_PRIORITY_LINUX_POLICY_DEADLINE = 5     /**< デッド ライン (`SCHED_DEADLINE`)。取得専用。 */
    } cplat_process_priority_linux_policy;

    /**
     *  @brief          デッド ライン スケジューリングのパラメーターです。
     *
     *  時間の関係 (実行時間 ≦ 期限 ≦ 周期) と最小値はカーネルが検証します。
     */
    typedef struct cplat_process_priority_linux_deadline
    {
        uint64_t runtime_nanoseconds;  /**< 周期あたりの実行時間 (ナノ秒)。 */
        uint64_t deadline_nanoseconds; /**< 周期の開始から期限までの時間 (ナノ秒)。 */
        uint64_t period_nanoseconds;   /**< 周期 (ナノ秒)。0 は deadline_nanoseconds と同じ値を表します。 */
    } cplat_process_priority_linux_deadline;

    /**
     *  @brief          I/O 優先度のクラスです。
     */
    typedef enum cplat_process_priority_linux_io_class
    {
        CPLAT_PROCESS_PRIORITY_LINUX_IO_CLASS_NONE = 0,        /**< 未設定。nice 値から導出 (`IOPRIO_CLASS_NONE`)。 */
        CPLAT_PROCESS_PRIORITY_LINUX_IO_CLASS_REAL_TIME = 1,   /**< リアルタイム (`IOPRIO_CLASS_RT`)。 */
        CPLAT_PROCESS_PRIORITY_LINUX_IO_CLASS_BEST_EFFORT = 2, /**< ベスト エフォート (`IOPRIO_CLASS_BE`)。 */
        CPLAT_PROCESS_PRIORITY_LINUX_IO_CLASS_IDLE = 3         /**< アイドル時のみ (`IOPRIO_CLASS_IDLE`)。 */
    } cplat_process_priority_linux_io_class;

    /**
     *  @brief          スケジューリング ポリシーと静的優先度を設定します。
     *  @param[in]      thread_id        対象のスレッド ID。0 は呼び出しスレッドです。
     *  @param[in]      policy           ポリシー。@ref CPLAT_PROCESS_PRIORITY_LINUX_POLICY_DEADLINE は指定できません。
     *  @param[in]      static_priority  静的優先度。FIFO と ROUND_ROBIN では 1 〜 99、それ以外では 0 を指定します。
     *  @param[in]      reset_on_fork    1 の場合、子プロセスはポリシーと優先度を既定値へ戻して開始します
     *                                   (`SCHED_RESET_ON_FORK`)。0 または 1 を指定します。
     *  @param[out]     detail_out       エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                                   エラー詳細を設定せず、返却しません。
     *                                   NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_PERMISSION_DENIED 、
     *                  @ref CPLAT_ERR_NOT_FOUND 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  リアルタイム ポリシー (FIFO、ROUND_ROBIN) への変更と、静的優先度を上げる操作には、
     *  `CAP_SYS_NICE` または `RLIMIT_RTPRIO` が必要です。\n
     *  デッド ライン スケジューリングへの変更は cplat_process_priority_linux_set_deadline() を使用してください。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_process_priority_linux_set_policy(uint32_t thread_id,
                                                                       cplat_process_priority_linux_policy policy,
                                                                       int static_priority, int reset_on_fork,
                                                                       cplat_error *detail_out);

    /**
     *  @brief          スケジューリング ポリシーと静的優先度を取得します。
     *  @param[in]      thread_id            対象のスレッド ID。0 は呼び出しスレッドです。
     *  @param[out]     policy_out           ポリシーの格納先。NULL を渡してはなりません。
     *  @param[out]     static_priority_out  静的優先度の格納先。NULL を渡してはなりません。
     *  @param[out]     reset_on_fork_out    `SCHED_RESET_ON_FORK` の有無 (1 または 0) の格納先。
     *                                       NULL を渡してはなりません。
     *  @param[out]     detail_out           エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                                       エラー詳細を設定せず、返却しません。
     *                                       NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_PERMISSION_DENIED 、
     *                  @ref CPLAT_ERR_NOT_FOUND 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  デッド ライン スケジューリング中は @ref CPLAT_PROCESS_PRIORITY_LINUX_POLICY_DEADLINE と静的優先度 0 を返します。\n
     *  列挙型にないポリシーの場合は @ref CPLAT_ERR_UNKNOWN を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_process_priority_linux_get_policy(uint32_t thread_id,
                                                                       cplat_process_priority_linux_policy *policy_out,
                                                                       int *static_priority_out, int *reset_on_fork_out,
                                                                       cplat_error *detail_out);

    /**
     *  @brief          デッド ライン スケジューリングを設定します。
     *  @param[in]      thread_id      対象のスレッド ID。0 は呼び出しスレッドです。
     *  @param[in]      deadline       デッド ラインのパラメーター。NULL を渡してはなりません。
     *  @param[in]      reset_on_fork  1 の場合、子プロセスはポリシーを既定値へ戻して開始します。
     *                                 0 または 1 を指定します。
     *  @param[out]     detail_out     エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                                 エラー詳細を設定せず、返却しません。
     *                                 NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_PERMISSION_DENIED 、
     *                  @ref CPLAT_ERR_NOT_FOUND 、@ref CPLAT_ERR_BUSY 、@ref CPLAT_ERR_UNSUPPORTED 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  `CAP_SYS_NICE` が必要です。\n
     *  パラメーターの関係に違反する場合は @ref CPLAT_ERR_INVALID_ARGUMENT 、
     *  システムの帯域を超える場合は @ref CPLAT_ERR_BUSY を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_process_priority_linux_set_deadline(
        uint32_t thread_id, const cplat_process_priority_linux_deadline *deadline, int reset_on_fork,
        cplat_error *detail_out);

    /**
     *  @brief          デッド ライン スケジューリングのパラメーターを取得します。
     *  @param[in]      thread_id     対象のスレッド ID。0 は呼び出しスレッドです。
     *  @param[out]     deadline_out  パラメーターの格納先。NULL を渡してはなりません。
     *  @param[out]     detail_out    エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                                エラー詳細を設定せず、返却しません。
     *                                NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_PERMISSION_DENIED 、
     *                  @ref CPLAT_ERR_NOT_FOUND 、@ref CPLAT_ERR_UNSUPPORTED 、@ref CPLAT_ERR_UNKNOWN の
     *                  いずれかを返します。
     *
     *  対象がデッド ライン スケジューリング中でない場合は、3 つの時間に 0 を格納して @ref CPLAT_OK を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_process_priority_linux_get_deadline(
        uint32_t thread_id, cplat_process_priority_linux_deadline *deadline_out, cplat_error *detail_out);

    /**
     *  @brief          nice 値を設定します。
     *  @param[in]      thread_id   対象のスレッド ID。0 は呼び出しスレッドです。
     *  @param[in]      nice_value  nice 値。-20 (最高) 〜 19 (最低) を指定します。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                              エラー詳細を設定せず、返却しません。
     *                              NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_PERMISSION_DENIED 、
     *                  @ref CPLAT_ERR_NOT_FOUND 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  値を下げる (優先度を上げる) 操作には、`CAP_SYS_NICE` または `RLIMIT_NICE` が必要です。\n
     *  nice 値は OTHER と BATCH のポリシーにだけ作用します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_process_priority_linux_set_nice(uint32_t thread_id, int nice_value,
                                                                     cplat_error *detail_out);

    /**
     *  @brief          nice 値を取得します。
     *  @param[in]      thread_id       対象のスレッド ID。0 は呼び出しスレッドです。
     *  @param[out]     nice_value_out  nice 値の格納先。NULL を渡してはなりません。
     *  @param[out]     detail_out      エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                                  エラー詳細を設定せず、返却しません。
     *                                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_PERMISSION_DENIED 、
     *                  @ref CPLAT_ERR_NOT_FOUND 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_process_priority_linux_get_nice(uint32_t thread_id, int *nice_value_out,
                                                                     cplat_error *detail_out);

    /**
     *  @brief          I/O 優先度を設定します。
     *  @param[in]      thread_id   対象のスレッド ID。0 は呼び出しスレッドです。
     *  @param[in]      io_class    I/O 優先度のクラス。
     *  @param[in]      level       クラス内のレベル。REAL_TIME と BEST_EFFORT では 0 (最高) 〜 7 (最低)、
     *                              NONE と IDLE では 0 を指定します。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                              エラー詳細を設定せず、返却しません。
     *                              NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_PERMISSION_DENIED 、
     *                  @ref CPLAT_ERR_NOT_FOUND 、@ref CPLAT_ERR_UNSUPPORTED 、@ref CPLAT_ERR_UNKNOWN の
     *                  いずれかを返します。
     *
     *  REAL_TIME クラスには `CAP_SYS_ADMIN` が必要です。\n
     *  I/O 優先度は、I/O スケジューラーが対応する場合 (BFQ など) にだけ効果があります。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_process_priority_linux_set_io_priority(
        uint32_t thread_id, cplat_process_priority_linux_io_class io_class, int level, cplat_error *detail_out);

    /**
     *  @brief          I/O 優先度を取得します。
     *  @param[in]      thread_id     対象のスレッド ID。0 は呼び出しスレッドです。
     *  @param[out]     io_class_out  I/O 優先度のクラスの格納先。NULL を渡してはなりません。
     *  @param[out]     level_out     クラス内のレベルの格納先。NULL を渡してはなりません。
     *  @param[out]     detail_out    エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                                エラー詳細を設定せず、返却しません。
     *                                NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_PERMISSION_DENIED 、
     *                  @ref CPLAT_ERR_NOT_FOUND 、@ref CPLAT_ERR_UNSUPPORTED 、@ref CPLAT_ERR_UNKNOWN の
     *                  いずれかを返します。
     *
     *  OS が返すクラスとレベルをそのまま格納します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_process_priority_linux_get_io_priority(
        uint32_t thread_id, cplat_process_priority_linux_io_class *io_class_out, int *level_out,
        cplat_error *detail_out);

    /**
     *  @brief          CPU 使用率クランプを設定します。
     *  @param[in]      thread_id   対象のスレッド ID。0 は呼び出しスレッドです。
     *  @param[in]      minimum     使用率の下限。0 〜 1024 を指定します。
     *  @param[in]      maximum     使用率の上限。@p minimum 〜 1024 を指定します。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                              エラー詳細を設定せず、返却しません。
     *                              NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_PERMISSION_DENIED 、
     *                  @ref CPLAT_ERR_NOT_FOUND 、@ref CPLAT_ERR_UNSUPPORTED 、@ref CPLAT_ERR_UNKNOWN の
     *                  いずれかを返します。
     *
     *  ポリシーと優先度を変えずに、クランプだけを変更します。\n
     *  上限を引き上げる操作には `CAP_SYS_NICE` が必要です。\n
     *  Linux 5.3 より前のカーネル、および `CONFIG_UCLAMP_TASK` が無効なカーネルでは
     *  @ref CPLAT_ERR_UNSUPPORTED を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_process_priority_linux_set_utilization_clamp(uint32_t thread_id,
                                                                                  uint32_t minimum,
                                                                                  uint32_t maximum,
                                                                                  cplat_error *detail_out);

    /**
     *  @brief          CPU 使用率クランプを取得します。
     *  @param[in]      thread_id    対象のスレッド ID。0 は呼び出しスレッドです。
     *  @param[out]     minimum_out  使用率の下限の格納先。NULL を渡してはなりません。
     *  @param[out]     maximum_out  使用率の上限の格納先。NULL を渡してはなりません。
     *  @param[out]     detail_out   エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                               エラー詳細を設定せず、返却しません。
     *                               NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_PERMISSION_DENIED 、
     *                  @ref CPLAT_ERR_NOT_FOUND 、@ref CPLAT_ERR_UNSUPPORTED 、@ref CPLAT_ERR_UNKNOWN の
     *                  いずれかを返します。
     *
     *  Linux 5.3 より前のカーネル、および `CONFIG_UCLAMP_TASK` が無効なカーネルでは
     *  @ref CPLAT_ERR_UNSUPPORTED を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_process_priority_linux_get_utilization_clamp(uint32_t thread_id,
                                                                                  uint32_t *minimum_out,
                                                                                  uint32_t *maximum_out,
                                                                                  cplat_error *detail_out);

    #ifdef __cplusplus
}
    #endif /* __cplusplus */

#endif /* PLATFORM_LINUX */

/** @} */

#endif /* CPLAT_PROCESS_PRIORITY_LINUX_H */
