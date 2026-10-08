/**
 *******************************************************************************
 *  @file           memory_lock.h
 *  @brief          自プロセスのメモリ ロック API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/07/05
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CPLAT_MEMORY_LOCK_H
#define CPLAT_MEMORY_LOCK_H

#include <stddef.h>
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

#define CPLAT_MEMORY_LOCK_CURRENT 0x01 /**< 現在マップ済みのメモリをロック対象にします。 */
#define CPLAT_MEMORY_LOCK_FUTURE  0x02 /**< 今後マップされるメモリもロック対象にします。 */
#define CPLAT_MEMORY_LOCK_ONFAULT 0x04 /**< ページ フォルト時にロックします。 */

    /**
     *  @brief          自プロセス メモリ ロックの解除情報です。
     *
     *  cplat_memory_lock_self() が成功した場合に生成され、呼び出し側が
     *  cplat_memory_lock_scope_release() へ渡して破棄します。\n
     *  本構造体のメンバーは公開されません。
     */
    typedef struct cplat_memory_lock_scope cplat_memory_lock_scope;

    /**
     *  @brief          自プロセス メモリ ロックのオプションです。
     */
    typedef struct cplat_memory_lock_self_options
    {
        int flags;                   /**< ロック対象を示す flag。0 または未知の bit を指定してはなりません。 */
        unsigned int pad;            /**< x64 で stack_prefault_bytes のアラインメントを明示するパディング。 */
        size_t stack_prefault_bytes; /**< ロック前に呼び出しスレッドで追加消費するスタック サイズ。0 可。 */
    } cplat_memory_lock_self_options;

    /**
     *  @brief          指定したメモリ範囲をロックします。
     *  @param[in]      address  ロック対象範囲の先頭アドレス。NULL を渡してはなりません。
     *  @param[in]      size     ロック対象範囲のサイズ (バイト)。0 を渡してはなりません。
     *  @return         結果コードを返します。
     *
     *  実際にロックされる範囲は OS のページ単位に丸められます。\n
     *  Windows では committed page だけをロックできます。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_memory_lock_range(const void *address, size_t size);

    /**
     *  @brief          指定したメモリ範囲のロックを解除します。
     *  @param[in]      address  解除対象範囲の先頭アドレス。NULL を渡してはなりません。
     *  @param[in]      size     解除対象範囲のサイズ (バイト)。0 を渡してはなりません。
     *  @return         結果コードを返します。
     *
     *  実際に解除される範囲は OS のページ単位に丸められます。\n
     *  Windows では部分的に未ロックのページを含むと失敗することがあります。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_memory_unlock_range(const void *address, size_t size);

    /**
     *  @brief          機密データを保持していたメモリ範囲を確実にゼロ クリアします。
     *  @param[out]     buf   クリア対象の先頭アドレス。@p size が 0 の場合に限り NULL も指定できます。
     *  @param[in]      size  クリア対象のサイズ (バイト)。
     *
     *  鍵、パスフレーズ、復号済み平文など、解放前に消去すべきデータに使用します。\n
     *  Linux では `explicit_bzero`、Windows では `SecureZeroMemory` を使用します。
     *
     *  @important      素の `memset` を使用しないでください。書き込み後に領域を読まない場合、
     *                  コンパイラは不要な書き込みとみなして除去できます (デッド ストア除去)。
     *                  本関数はその除去が起きないことを保証します。
     *
     *  @remark         本関数はメモリの内容を消すだけで、ページのロック状態は変更しません。
     *                  スワップへの書き出しも防ぐ場合は cplat_memory_lock_range() と併用してください。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void CPLAT_API cplat_secure_zero(void *buf, size_t size);

    /**
     *  @brief          自プロセスのメモリをまとめてロックします。
     *  @param[in]      options  ロック オプション。NULL を渡してはなりません。
     *  @param[out]     scope    解除情報の格納先。NULL を渡してはなりません。
     *  @return         結果コードを返します。
     *
     *  @p options の stack_prefault_bytes が 0 より大きい場合、ロック前に呼び出しスレッドのスタックへ
     *  指定サイズ分だけアクセスします。\n
     *  この処理は呼び出しスレッドだけを対象にします。\n
     *  Linux では mlockall() を使用します。\n
     *  Windows では @ref CPLAT_MEMORY_LOCK_CURRENT だけをサポートし、VirtualQuery() で現在の
     *  committed region を列挙して VirtualLock() を適用します。\n
     *  Windows で @ref CPLAT_MEMORY_LOCK_FUTURE または @ref CPLAT_MEMORY_LOCK_ONFAULT を指定した場合は
     *  @ref CPLAT_ERR_UNSUPPORTED を返します。\n
     *  返された @p scope は cplat_memory_lock_scope_release() で破棄してください。
     *
     *  @par            呼び出しタイミング
     *  ロック対象のスタックを先に committed page にしたい場合は、@p options の stack_prefault_bytes を指定して
     *  本関数を呼び出します。\n
     *  関数が成功して @p scope が返った後、ページ フォルトを避けたい処理を実行します。
     *
    @startuml cplat_memory_lock_self のロック取得
        caption cplat_memory_lock_self のロック取得
        participant "呼び出し側" as Caller
        participant "cplat_memory_lock_self" as API
        participant "内部 lock" as Lock
        participant "OS" as OS

        Caller -> API : options, &scope
        API -> API : 引数と flag を検証
        opt stack_prefault_bytes > 0
            API -> API : 呼び出しスレッドのスタックへアクセス
        end
        API -> Lock : 取得
        alt Linux
            API -> OS : mlockall(flags)
            API -> API : self scope 数を加算
        else Windows
            API -> OS : VirtualQuery で committed region を列挙
            API -> OS : 未登録範囲へ VirtualLock
            API -> API : 登録済み範囲の ref-count を加算
        end
        API -> Lock : 解放
        API --> Caller : CPLAT_OK, scope
    @enduml
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_memory_lock_self(const cplat_memory_lock_self_options *options,
                                                      cplat_memory_lock_scope **scope);

    /**
     *  @brief          自プロセス全体ロックの解除情報を破棄し、ロックを解除します。
     *  @param[in]      scope  cplat_memory_lock_self() で取得した解除情報。NULL 可。
     *  @return         結果コードを返します。
     *
     *  NULL を渡した場合は何もせず @ref CPLAT_OK を返します。
     *
     *  @par            解放タイミング
     *  @p scope は、対応する cplat_memory_lock_self() が成功した後、ロック状態を必要とする処理が終わった時点で
     *  1 回だけ解放します。\n
     *  Linux では最後の self scope が解放された時点で `munlockall()` を呼び出します。\n
     *  Windows では @p scope が保持する各範囲の ref-count を下げ、最後の参照がなくなった範囲だけ
     *  `VirtualUnlock()` を呼び出します。
     *
    @startuml cplat_memory_lock_scope_release のロック解放
        caption cplat_memory_lock_scope_release のロック解放
        participant "呼び出し側" as Caller
        participant "cplat_memory_lock_scope_release" as API
        participant "内部 lock" as Lock
        participant "OS" as OS

        Caller -> API : scope
        alt scope == NULL
            API --> Caller : CPLAT_OK
        else scope != NULL
            API -> Lock : 取得
            alt Linux
                API -> API : self scope 数を減算
                opt self scope 数 == 0
                    API -> OS : munlockall()
                end
            else Windows
                loop scope の各範囲
                    API -> API : registry の ref-count を減算
                    opt ref-count == 0
                        API -> OS : VirtualUnlock()
                    end
                end
            end
            API -> Lock : 解放
            API -> API : scope を破棄
            API --> Caller : 結果コード
        end
    @enduml
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p scope に対する操作は同時に実行できます。\n
     *  同一 @p scope に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_memory_lock_scope_release(cplat_memory_lock_scope *scope);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_MEMORY_LOCK_H */
