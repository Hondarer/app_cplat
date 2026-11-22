/**
 *******************************************************************************
 *  @file           sync_internal.h
 *  @brief          同期モジュールの OS 別の実装が共有する定義 (内部共有) を宣言します。
 *
 *  sync_linux.c と sync_windows.c が、同じ値を使うために取り込みます。
 *******************************************************************************
 */
#ifndef CPLAT_SYNC_INTERNAL_H
#define CPLAT_SYNC_INTERNAL_H

/** cplat_once_flag::state が取る状態です。ゼロ初期化した状態が未実行です。 */
typedef enum cplat_internal_once_state
{
    CPLAT_ONCE_STATE_UNINITIALIZED = 0, /**< 未実行。 */
    CPLAT_ONCE_STATE_RUNNING = 1,       /**< 1 つのスレッドが初期化関数を実行中。 */
    CPLAT_ONCE_STATE_DONE = 2           /**< 初期化関数の実行を完了した。 */
} cplat_internal_once_state;

#endif /* CPLAT_SYNC_INTERNAL_H */
