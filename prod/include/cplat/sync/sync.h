/**
 *******************************************************************************
 *  @file           sync.h
 *  @brief          スレッドと同期プリミティブを抽象化する API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/04/20
 *  @version        2.0.0
 *
 *  OS ごとの差異を隠蔽し、プロセス内同期とプロセス横断可能な
 *  アプリケーション排他を共通の結果コードで提供します。
 *
 *  @details
 *  待機を伴う API (本ヘッダー、@ref process.h 、@ref socket.h ) は、次の設計原則に従います。
 *  - 待ち方の分け方: 無期限に待つ関数と期限付きで待つ関数 (@c *_timed_* ) を分けます。
 *    期限付きの関数は @ref CPLAT_WAIT_FOREVER 、@ref CPLAT_NO_WAIT 、または 0 以上の有限の待ち時間 [ms] を受け付け、
 *    負値を渡した場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
 *  - 戻り値の決め方: ローカル同期の無期限待機および状態通知・解放のうち、
 *    正常系で失敗を返す契約を持たない操作は @c void を返します。
 *    期限付き待機、およびプロセスやソケットなど外部資源を扱う待機は @c int の結果コードを返します。
 *  - @c void の関数の異常時: @c NULL を渡した場合は何もせずに戻ります。
 *    OS がエラーを返した場合、または保持していない読み書きロックの解放など
 *    呼び出し側の不具合で排他状態を保てない場合は、続行せずプロセスを異常終了させます。
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

#ifndef CPLAT_SYNC_H
#define CPLAT_SYNC_H

#include <stddef.h>
#include <stdint.h>

#include <cplat/base/result.h>
#include <cplat/base/wait.h>
#include <cplat/cplat_export.h>
#include <cplat/sync/atomic.h>

/**
 *  @ingroup        CPLAT_SYNC
 *  @{
 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /** @brief プロセス横断同期のバックエンド種別。 */
    typedef enum cplat_interprocess_sync_backend
    {
        CPLAT_INTERPROCESS_SYNC_BACKEND_LOCK_FILE = 1 /**< ロック ファイルを使用するバックエンド。 */
    } cplat_interprocess_sync_backend;

    typedef struct cplat_local_lock cplat_local_lock;                   /**< プロセス内ミューテックス。 */
    typedef struct cplat_condvar cplat_condvar;                         /**< プロセス内条件変数。 */
    typedef struct cplat_local_rwlock cplat_local_rwlock;               /**< プロセス内読み書きロック。 */
    typedef struct cplat_thread cplat_thread;                           /**< スレッド ハンドル。 */
    typedef struct cplat_interprocess_lock cplat_interprocess_lock;     /**< プロセス横断ミューテックス。 */
    typedef struct cplat_interprocess_rwlock cplat_interprocess_rwlock; /**< プロセス横断読み書きロック。 */

    /** スレッド関数ポインター型。 */
    typedef void (*cplat_thread_fn)(void *);

    /** call_once で 1 回だけ呼び出される関数ポインター型。 */
    typedef void (*cplat_once_fn)(void);

    /** call_once 状態。静的領域では 0 初期化して用いる。 */
    typedef struct cplat_once_flag
    {
        cplat_atomic_i32 state; /**< 0=未実行、1=実行中、2=実行済み。cplat_call_once 以外から操作しないこと。 */
    } cplat_once_flag;

    /**
     *  @brief          プロセス内ミューテックスを生成します。
     *  @param[out]     mtx  生成したミューテックスの格納先。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_OUT_OF_MEMORY 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_local_lock_create(cplat_local_lock **mtx);

    /**
     *  @brief          ミューテックスを取得できるまで無期限に待機します。
     *  @param[in]      mtx  対象のミューテックス。NULL を渡した場合は何もしません。
     *
     *  @attention      OS がロック取得に失敗した場合は、排他状態を保てないため
     *                  プロセスを異常終了させます。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void CPLAT_API cplat_local_lock_lock(cplat_local_lock *mtx);

    /**
     *  @brief          ミューテックスを指定期限内でロックします。
     *  @param[in]      mtx        対象のミューテックス。NULL を渡してはなりません。
     *  @param[in]      timeout_ms タイムアウト [ms]。@ref CPLAT_WAIT_FOREVER または
     *                             @ref CPLAT_NO_WAIT も指定可能です。
     *                             負値を渡した場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_TIMEOUT 、
     *                  @ref CPLAT_ERR_BUSY (@p timeout_ms が @ref CPLAT_NO_WAIT の場合) 、
     *                  @ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_local_lock_timed_lock(cplat_local_lock *mtx, int timeout_ms);

    /**
     *  @brief          ミューテックスを非ブロッキングでロック試行します。
     *  @param[in]      mtx  対象のミューテックス。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK または @ref CPLAT_ERR_BUSY を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_local_lock_try_lock(cplat_local_lock *mtx);

    /**
     *  @brief          ミューテックスをロック解除します。
     *  @param[in]      mtx  対象のミューテックス。NULL を渡した場合は何もしません。
     *
     *  @attention      OS がロック解除に失敗した場合は、排他状態を保てないため
     *                  プロセスを異常終了させます。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void CPLAT_API cplat_local_lock_unlock(cplat_local_lock *mtx);

    /**
     *  @brief          ミューテックスを破棄します。
     *  @param[in]      mtx  破棄するミューテックス。NULL を渡してはなりません。
     *
     *  @attention      Linux では、OS が破棄に失敗した場合 (ロックしたままの破棄など) は、使用中の領域を解放しないよう、
     *                  プロセスを異常終了させます。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p mtx に対する操作は同時に実行できます。\n
     *  同一 @p mtx に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT void CPLAT_API cplat_local_lock_dispose(cplat_local_lock *mtx);

    /**
     *  @brief          条件変数を生成します。
     *  @param[out]     cv  生成した条件変数の格納先。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_OUT_OF_MEMORY 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_condvar_create(cplat_condvar **cv);

    /**
     *  @brief          条件変数を無期限に待機します (@p mtx を atomically ロック解除後に待機し、通知後に再ロック)。
     *  @param[in]      cv   対象の条件変数。NULL を渡した場合は何もしません。
     *  @param[in]      mtx  待機中にロック解除するミューテックス。NULL を渡した場合は何もしません。
     *
     *  @attention      OS が待機に失敗した場合は、同期状態を保てないため
     *                  プロセスを異常終了させます。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void CPLAT_API cplat_condvar_wait(cplat_condvar *cv, cplat_local_lock *mtx);

    /**
     *  @brief          条件変数を指定期限内で待機します (@p mtx を atomically ロック解除後に待機し、通知後に再ロック)。
     *  @param[in]      cv         対象の条件変数。NULL を渡してはなりません。
     *  @param[in]      mtx        待機中にロック解除するミューテックス。NULL を渡してはなりません。
     *  @param[in]      timeout_ms タイムアウト [ms]。@ref CPLAT_WAIT_FOREVER または
     *                             @ref CPLAT_NO_WAIT も指定可能です。
     *                             負値を渡した場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_TIMEOUT 、
     *                  @ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_condvar_timed_wait(cplat_condvar *cv, cplat_local_lock *mtx, int timeout_ms);

    /**
     *  @brief          待機中のスレッドを 1 つ起床させます。
     *  @param[in]      cv  対象の条件変数。NULL を渡した場合は何もしません。
     *
     *  @attention      OS が通知に失敗した場合は、同期状態を保てないため
     *                  プロセスを異常終了させます。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void CPLAT_API cplat_condvar_signal(cplat_condvar *cv);

    /**
     *  @brief          待機中のすべてのスレッドを起床させます。
     *  @param[in]      cv  対象の条件変数。NULL を渡した場合は何もしません。
     *
     *  @attention      OS が通知に失敗した場合は、同期状態を保てないため
     *                  プロセスを異常終了させます。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void CPLAT_API cplat_condvar_broadcast(cplat_condvar *cv);

    /**
     *  @brief          条件変数を破棄します。
     *  @param[in]      cv  破棄する条件変数。NULL を渡してはなりません。
     *
     *  @attention      Linux では、OS が破棄に失敗した場合 (ロックしたままの破棄など) は、使用中の領域を解放しないよう、
     *                  プロセスを異常終了させます。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p cv に対する操作は同時に実行できます。\n
     *  同一 @p cv に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT void CPLAT_API cplat_condvar_dispose(cplat_condvar *cv);

    /**
     *  @brief          プロセス内読み書きロックを生成します。
     *  @param[out]     rwlock  生成した読み書きロックの格納先。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_OUT_OF_MEMORY 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_local_rwlock_create(cplat_local_rwlock **rwlock);

    /**
     *  @brief          共有 (読み取り) ロックを取得できるまで無期限に待機します。
     *  @param[in]      rwlock  対象の読み書きロック。NULL を渡した場合は何もしません。
     *
     *  @attention      読み書きロックの状態を守る内部の同期に失敗した場合は、
     *                  排他を保てないため、プロセスを異常終了させます。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void CPLAT_API cplat_local_rwlock_lock_shared(cplat_local_rwlock *rwlock);

    /**
     *  @brief          共有 (読み取り) ロックを指定期限内で取得します。
     *  @param[in]      rwlock     対象の読み書きロック。NULL を渡してはなりません。
     *  @param[in]      timeout_ms タイムアウト [ms]。@ref CPLAT_WAIT_FOREVER または
     *                             @ref CPLAT_NO_WAIT も指定可能です。
     *                             負値を渡した場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_TIMEOUT 、
     *                  @ref CPLAT_ERR_BUSY (@p timeout_ms が @ref CPLAT_NO_WAIT の場合) 、
     *                  @ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @attention      読み書きロックの状態を守る内部の同期に失敗した場合は、
     *                  排他を保てないため、プロセスを異常終了させます。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_local_rwlock_timed_lock_shared(cplat_local_rwlock *rwlock, int timeout_ms);

    /**
     *  @brief          共有 (読み取り) ロックを非ブロッキングで取得試行します。
     *  @param[in]      rwlock  対象の読み書きロック。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK または @ref CPLAT_ERR_BUSY を返します。
     *
     *  @attention      読み書きロックの状態を守る内部の同期に失敗した場合は、
     *                  排他を保てないため、プロセスを異常終了させます。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_local_rwlock_try_lock_shared(cplat_local_rwlock *rwlock);

    /**
     *  @brief          排他 (書き込み) ロックを取得できるまで無期限に待機します。
     *  @param[in]      rwlock  対象の読み書きロック。NULL を渡した場合は何もしません。
     *
     *  @attention      読み書きロックの状態を守る内部の同期に失敗した場合は、
     *                  排他を保てないため、プロセスを異常終了させます。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void CPLAT_API cplat_local_rwlock_lock_exclusive(cplat_local_rwlock *rwlock);

    /**
     *  @brief          排他 (書き込み) ロックを指定期限内で取得します。
     *  @param[in]      rwlock     対象の読み書きロック。NULL を渡してはなりません。
     *  @param[in]      timeout_ms タイムアウト [ms]。@ref CPLAT_WAIT_FOREVER または
     *                             @ref CPLAT_NO_WAIT も指定可能です。
     *                             負値を渡した場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_TIMEOUT 、
     *                  @ref CPLAT_ERR_BUSY (@p timeout_ms が @ref CPLAT_NO_WAIT の場合) 、
     *                  @ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @attention      読み書きロックの状態を守る内部の同期に失敗した場合は、
     *                  排他を保てないため、プロセスを異常終了させます。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_local_rwlock_timed_lock_exclusive(cplat_local_rwlock *rwlock, int timeout_ms);

    /**
     *  @brief          排他 (書き込み) ロックを非ブロッキングで取得試行します。
     *  @param[in]      rwlock  対象の読み書きロック。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK または @ref CPLAT_ERR_BUSY を返します。
     *
     *  @attention      読み書きロックの状態を守る内部の同期に失敗した場合は、
     *                  排他を保てないため、プロセスを異常終了させます。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_local_rwlock_try_lock_exclusive(cplat_local_rwlock *rwlock);

    /**
     *  @brief          共有 (読み取り) ロックを解放します。
     *  @param[in]      rwlock  対象の読み書きロック。NULL を渡した場合は何もしません。
     *
     *  @attention      共有ロックを保持していない状態で呼び出した場合、または内部の同期に失敗した場合は、
     *                  排他を保てないため、プロセスを異常終了させます。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void CPLAT_API cplat_local_rwlock_unlock_shared(cplat_local_rwlock *rwlock);

    /**
     *  @brief          排他 (書き込み) ロックを解放します。
     *  @param[in]      rwlock  対象の読み書きロック。NULL を渡した場合は何もしません。
     *
     *  @attention      排他ロックを保持していない状態で呼び出した場合、または内部の同期に失敗した場合は、
     *                  排他を保てないため、プロセスを異常終了させます。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void CPLAT_API cplat_local_rwlock_unlock_exclusive(cplat_local_rwlock *rwlock);

    /**
     *  @brief          読み書きロックを破棄します。
     *  @param[in]      rwlock  破棄する読み書きロック。NULL を渡してはなりません。
     *
     *  @attention      Linux では、OS が破棄に失敗した場合 (ロックしたままの破棄など) は、使用中の領域を解放しないよう、
     *                  プロセスを異常終了させます。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p rwlock に対する操作は同時に実行できます。\n
     *  同一 @p rwlock に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT void CPLAT_API cplat_local_rwlock_dispose(cplat_local_rwlock *rwlock);

    /**
     *  @brief          スレッドを生成して起動します。
     *  @param[out]     thread  生成したスレッド ハンドルの格納先。NULL を渡してはなりません。
     *  @param[in]      func    スレッド関数。NULL を渡してはなりません。
     *  @param[in]      arg     @p func に渡す引数。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_OUT_OF_MEMORY 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_thread_create(cplat_thread **thread, cplat_thread_fn func, void *arg);

    /**
     *  @brief          スレッドの終了を無期限に待機し、ハンドルを解放します。
     *  @param[in]      thread  対象のスレッド ハンドル。NULL を渡した場合は何もしません。
     *
     *  @attention      OS が待機に失敗した場合は、スレッドの終了状態を保てないため
     *                  プロセスを異常終了させます。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p thread に対する操作は同時に実行できます。\n
     *  同一 @p thread に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT void CPLAT_API cplat_thread_join(cplat_thread *thread);

    /**
     *  @brief          スレッドの終了を指定期限内で待機します。
     *  @param[in]      thread     対象のスレッド ハンドル。NULL を渡してはなりません。
     *  @param[in]      timeout_ms タイムアウト [ms]。@ref CPLAT_WAIT_FOREVER または
     *                             @ref CPLAT_NO_WAIT も指定可能です。
     *                             負値を渡した場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *  @return         終了した場合はハンドルを解放し、@ref CPLAT_OK を返します。
     *                  タイムアウトした場合はハンドルを有効なまま維持し、@ref CPLAT_ERR_TIMEOUT を返します。
     *                  その他、@ref CPLAT_ERR_INVALID_ARGUMENT または
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p thread に対する操作は同時に実行できます。\n
     *  同一 @p thread に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_thread_timed_join(cplat_thread *thread, int timeout_ms);

    /**
     *  @brief          スレッドを切り離します。切り離し後はリソースを自動解放します。
     *  @param[in]      thread  切り離すスレッド ハンドル。NULL を渡してはなりません。
     *
     *  切り離しに失敗しても、失敗は通知しません。呼び出し側に、失敗を受けて取れる手段がないためです。
     *  失敗は、すでに切り離しまたは join をしたハンドルに限られます。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p thread に対する操作は同時に実行できます。\n
     *  同一 @p thread に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT void CPLAT_API cplat_thread_detach(cplat_thread *thread);

    /**
     *  @brief          現在のスレッドの実行権を手放し、実行可能な他のスレッドへ CPU を譲ります。
     *
     *  @details        Linux では sched_yield()、Windows では SwitchToThread() を呼び出します。\n
     *                  実行可能な他のスレッドがない場合は、待機せずに即時リターンします。\n
     *                  他のスレッドへ切り替わること、および切り替わった後に戻るまでの時間は保証しません。
     *
     *  @note           Windows では、切り替え先を現在のプロセッサで実行可能なスレッドに限ります。\n
     *                  Linux では、通常のスケジューリング ポリシー (SCHED_OTHER) における譲渡の強さを
     *                  カーネルのスケジューラーが決めます。\n
     *                  他のスレッドの処理完了を待つ場合は、本関数の繰り返しではなく、条件変数または
     *                  cplat_sleep_ms() による待機を使用してください。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void CPLAT_API cplat_thread_yield(void);

    /**
     *  @brief          識別子でプロセス横断ミューテックスを開きます (存在しない場合は生成)。
     *  @param[in]      identity  ロックを識別する名前文字列。ロック ファイルのパスとして使用します。
     *                            NULL および空文字列を渡してはなりません。
     *  @param[out]     lock      生成したロック ハンドルの格納先。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_OUT_OF_MEMORY 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。\n
     *                  @p identity が NULL または空文字列の場合、および @p lock が NULL の場合は
     *                  @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *
     *  `identity` はファイル パスになります。ファイルが存在しない場合は、その位置に作成します。
     *  同じロックに参加するプロセスが開いている間は、このファイルを削除または名前変更しないでください。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_interprocess_lock_open(const char *identity, cplat_interprocess_lock **lock);

    /**
     *  @brief          エクスポートされたディスクリプタからプロセス横断ミューテックスをインポートします。
     *  @param[in]      descriptor       インポートするディスクリプタ データ。NULL を渡してはなりません。
     *  @param[in]      descriptor_size  @p descriptor のサイズ [byte]。
     *  @param[out]     lock             インポートしたロック ハンドルの格納先。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_CORRUPT_DESCRIPTOR 、
     *                  @ref CPLAT_ERR_VERSION_MISMATCH 、@ref CPLAT_ERR_OUT_OF_MEMORY 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_interprocess_lock_import_descriptor(const void *descriptor, size_t descriptor_size,
                                                                         cplat_interprocess_lock **lock);

    /**
     *  @brief          プロセス横断ミューテックスをディスクリプタにエクスポートします (プロセス間受け渡し用)。
     *  @param[in]      lock             エクスポートするロック。NULL を渡してはなりません。
     *  @param[out]     descriptor       エクスポート先バッファー。NULL の場合はサイズのみ取得します。
     *  @param[in,out]  descriptor_size  入力: @p descriptor のサイズ。出力: 必要サイズ。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_BUFFER_TOO_SMALL 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_interprocess_lock_export_descriptor(const cplat_interprocess_lock *lock,
                                                                         void *descriptor, size_t *descriptor_size);

    /**
     *  @brief          プロセス横断ミューテックスを取得できるまで無期限に待機します。
     *  @param[in]      lock  対象のロック。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_interprocess_lock_lock(cplat_interprocess_lock *lock);

    /**
     *  @brief          プロセス横断ミューテックスを指定期限内でロックします。
     *  @param[in]      lock       対象のロック。NULL を渡してはなりません。
     *  @param[in]      timeout_ms タイムアウト [ms]。@ref CPLAT_WAIT_FOREVER または
     *                             @ref CPLAT_NO_WAIT も指定可能です。
     *                             負値を渡した場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_TIMEOUT 、
     *                  @ref CPLAT_ERR_BUSY (@p timeout_ms が @ref CPLAT_NO_WAIT の場合) 、
     *                  @ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_interprocess_lock_timed_lock(cplat_interprocess_lock *lock, int timeout_ms);

    /**
     *  @brief          プロセス横断ミューテックスを非ブロッキングでロック試行します。
     *  @param[in]      lock  対象のロック。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK または @ref CPLAT_ERR_BUSY を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_interprocess_lock_try_lock(cplat_interprocess_lock *lock);

    /**
     *  @brief          プロセス横断ミューテックスをロック解除します。
     *  @param[in]      lock  対象のロック。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK または @ref CPLAT_ERR_UNKNOWN を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_interprocess_lock_unlock(cplat_interprocess_lock *lock);

    /**
     *  @brief          プロセス横断ミューテックスを破棄します。
     *  @param[in]      lock  破棄するロック。NULL を渡してはなりません。
     *
     *  破棄の途中の失敗は通知しません。呼び出し側に、失敗を受けて取れる手段がないためです。
     *  ロックを保持したまま呼んだ場合は、破棄の中で解除します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p lock に対する操作は同時に実行できます。\n
     *  同一 @p lock に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT void CPLAT_API cplat_interprocess_lock_dispose(cplat_interprocess_lock *lock);

    /**
     *  @brief          識別子でプロセス横断読み書きロックを開きます (存在しない場合は生成)。
     *  @param[in]      identity  ロックを識別する名前文字列。ロック ファイルのパスとして使用します。
     *                            NULL および空文字列を渡してはなりません。
     *  @param[out]     lock      生成したロック ハンドルの格納先。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_OUT_OF_MEMORY 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。\n
     *                  @p identity が NULL または空文字列の場合、および @p lock が NULL の場合は
     *                  @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *
     *  `identity` はファイル パスになります。ファイルが存在しない場合は、その位置に作成します。
     *  同じロックに参加するプロセスが開いている間は、このファイルを削除または名前変更しないでください。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_interprocess_rwlock_open(const char *identity, cplat_interprocess_rwlock **lock);

    /**
     *  @brief          エクスポートされたディスクリプタからプロセス横断読み書きロックをインポートします。
     *  @param[in]      descriptor       インポートするディスクリプタ データ。NULL を渡してはなりません。
     *  @param[in]      descriptor_size  @p descriptor のサイズ [byte]。
     *  @param[out]     lock             インポートしたロック ハンドルの格納先。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_CORRUPT_DESCRIPTOR 、
     *                  @ref CPLAT_ERR_VERSION_MISMATCH 、@ref CPLAT_ERR_OUT_OF_MEMORY 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_interprocess_rwlock_import_descriptor(const void *descriptor,
                                                                           size_t descriptor_size,
                                                                           cplat_interprocess_rwlock **lock);

    /**
     *  @brief          プロセス横断読み書きロックをディスクリプタにエクスポートします (プロセス間受け渡し用)。
     *  @param[in]      lock             エクスポートするロック。NULL を渡してはなりません。
     *  @param[out]     descriptor       エクスポート先バッファー。NULL の場合はサイズのみ取得します。
     *  @param[in,out]  descriptor_size  入力: @p descriptor のサイズ。出力: 必要サイズ。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_BUFFER_TOO_SMALL 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_interprocess_rwlock_export_descriptor(const cplat_interprocess_rwlock *lock,
                                                                           void *descriptor, size_t *descriptor_size);

    /**
     *  @brief          プロセス横断共有 (読み取り) ロックを取得できるまで無期限に待機します。
     *  @param[in]      lock  対象のロック。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_interprocess_rwlock_lock_shared(cplat_interprocess_rwlock *lock);

    /**
     *  @brief          プロセス横断共有 (読み取り) ロックを指定期限内で取得します。
     *  @param[in]      lock       対象のロック。NULL を渡してはなりません。
     *  @param[in]      timeout_ms タイムアウト [ms]。@ref CPLAT_WAIT_FOREVER または
     *                             @ref CPLAT_NO_WAIT も指定可能です。
     *                             負値を渡した場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_TIMEOUT 、
     *                  @ref CPLAT_ERR_BUSY (@p timeout_ms が @ref CPLAT_NO_WAIT の場合) 、
     *                  @ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_interprocess_rwlock_timed_lock_shared(cplat_interprocess_rwlock *lock,
                                                                          int timeout_ms);

    /**
     *  @brief          プロセス横断共有 (読み取り) ロックを非ブロッキングで取得試行します。
     *  @param[in]      lock  対象のロック。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK または @ref CPLAT_ERR_BUSY を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_interprocess_rwlock_try_lock_shared(cplat_interprocess_rwlock *lock);

    /**
     *  @brief          プロセス横断排他 (書き込み) ロックを取得できるまで無期限に待機します。
     *  @param[in]      lock  対象のロック。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_interprocess_rwlock_lock_exclusive(cplat_interprocess_rwlock *lock);

    /**
     *  @brief          プロセス横断排他 (書き込み) ロックを指定期限内で取得します。
     *  @param[in]      lock       対象のロック。NULL を渡してはなりません。
     *  @param[in]      timeout_ms タイムアウト [ms]。@ref CPLAT_WAIT_FOREVER または
     *                             @ref CPLAT_NO_WAIT も指定可能です。
     *                             負値を渡した場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_TIMEOUT 、
     *                  @ref CPLAT_ERR_BUSY (@p timeout_ms が @ref CPLAT_NO_WAIT の場合) 、
     *                  @ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_interprocess_rwlock_timed_lock_exclusive(cplat_interprocess_rwlock *lock,
                                                                             int timeout_ms);

    /**
     *  @brief          プロセス横断排他 (書き込み) ロックを非ブロッキングで取得試行します。
     *  @param[in]      lock  対象のロック。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK または @ref CPLAT_ERR_BUSY を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_interprocess_rwlock_try_lock_exclusive(cplat_interprocess_rwlock *lock);

    /**
     *  @brief          プロセス横断読み書きロックを解放します。
     *  @param[in]      lock  対象のロック。NULL を渡してはなりません。
     *  @return         @ref CPLAT_OK または @ref CPLAT_ERR_UNKNOWN を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_interprocess_rwlock_unlock(cplat_interprocess_rwlock *lock);

    /**
     *  @brief          プロセス横断読み書きロックを破棄します。
     *  @param[in]      lock  破棄するロック。NULL を渡してはなりません。
     *
     *  破棄の途中の失敗は通知しません。呼び出し側に、失敗を受けて取れる手段がないためです。
     *  ロックを保持したまま呼んだ場合は、破棄の中で解除します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p lock に対する操作は同時に実行できます。\n
     *  同一 @p lock に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT void CPLAT_API cplat_interprocess_rwlock_dispose(cplat_interprocess_rwlock *lock);

    /**
     *  @brief          @p func をプロセス内で 1 回だけ呼び出します。
     *  @param[in]      flag  呼び出し状態を管理するフラグ。静的領域で 0 初期化して使用してください。
     *                        NULL を渡してはなりません。
     *  @param[in]      func  1 回だけ実行する関数。NULL を渡してはなりません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void CPLAT_API cplat_call_once(cplat_once_flag *flag, cplat_once_fn func);

    /**
     *  @brief      指定時間だけ現在のスレッドをスリープします。
     *  @param[in]  ms  スリープする時間 [ms]。0 以下の値を渡した場合は即時リターンします (no-op)。
     *
     *  @note           Linux では、待機中にシグナルを受信した場合も、
     *                  残り時間を再計算して待機を継続するため、規定時間が経過するまで待機します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void CPLAT_API cplat_sleep_ms(int ms);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_SYNC_H */
