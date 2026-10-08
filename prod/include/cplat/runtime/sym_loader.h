/**
 *******************************************************************************
 *  @file           sym_loader.h
 *  @brief          関数を動的に解決する symbol loader の公開 API を提供します。
 *  @author         c-modenization-kit sample team
 *  @date           2026/03/17
 *  @version        1.0.0
 *
 *  sym_loader は、JSONC 設定ファイルから関数シンボルとライブラリ名を読み込み、
 *  実行時に動的リンクで関数を解決するキャッシュ機構です。
 *
 *  使用方法:
 *  1. cplat_sym_loader_entry を CPLAT_SYM_LOADER_ENTRY_INIT マクロで静的初期化します。
 *  2. cplat_sym_loader_init() で JSONC 設定ファイルを読み込みます (DllMain / constructor から呼び出します)。
 *  3. cplat_sym_loader_resolve_as() で関数ポインターを取得して呼び出します。
 *  4. cplat_sym_loader_dispose() でリソースを解放します (DllMain / destructor から呼び出します)。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CPLAT_SYM_LOADER_H
#define CPLAT_SYM_LOADER_H

#include <cplat/base/platform.h>
#include <cplat/base/result.h>
#include <cplat/sync/sync.h>
#include <cplat/cplat_export.h>

#if defined(PLATFORM_LINUX)
    #ifndef _GNU_SOURCE
        #define _GNU_SOURCE
    #endif /* _GNU_SOURCE */
#endif     /* PLATFORM_LINUX */

#include <stddef.h>

#if defined(PLATFORM_LINUX)
    #include <dlfcn.h>
#elif defined(PLATFORM_WINDOWS)
    #include <cplat/base/windows_sdk.h>
#endif /* PLATFORM_ */

/**
 *  @ingroup        CPLAT_RUNTIME
 *  @{
 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#ifdef DOXYGEN
    /**
     *  @brief          Linux/Windows 共通のモジュール ハンドル型です。
     *
     *  sym_loader が内部で保持する動的ロード済みモジュールの不透明ハンドルです。\n
     *  Linux では `dlopen()` が返す `void *`、Windows では `LoadLibrary()` 系が返す
     *  `HMODULE` を表します。
     */
    #define CPLAT_MODULE_HANDLE void *
#elif defined(PLATFORM_LINUX)
    #define CPLAT_MODULE_HANDLE void *
#elif defined(PLATFORM_WINDOWS)
    #define CPLAT_MODULE_HANDLE HMODULE
#endif

#define CPLAT_SYM_LOADER_NAME_MAX 256 /**< lib_name / func_name 配列の最大長 (終端 '\0' を含む)。 */

    /**
     *  @brief          関数ポインター キャッシュ エントリです。
     *
     *  ライブラリ名・関数名・ハンドル・関数ポインターおよび排他制御用ロックを管理します。\n
     *  静的変数として定義する場合は CPLAT_SYM_LOADER_ENTRY_INIT マクロで初期化してください。
     */
    typedef struct cplat_sym_loader_entry
    {
        const char *func_key;                      /**< この関数インスタンスの識別キー。 */
        char lib_name[CPLAT_SYM_LOADER_NAME_MAX];  /**< 拡張子なしライブラリ名。[0]=='\0' = 未設定。 */
        char func_name[CPLAT_SYM_LOADER_NAME_MAX]; /**< 関数シンボル名。[0]=='\0' = 未設定。 */
        CPLAT_MODULE_HANDLE handle;                /**< キャッシュ済みハンドル (NULL = 未ロード)。 */
        /* 次の 3 つは、ロックを取らない高速経路から読むため、アトミック型で保持する。 */
        cplat_atomic_ptr func_ptr;   /**< キャッシュ済み関数ポインター (NULL = 未取得)。 */
        cplat_atomic_i32 resolved;   /**< 解決済フラグ (0 = 未解決)。 */
        cplat_atomic_i32 lock_state; /**< ロック初期化状態 (0=未初期化,1=初期化中,2=初期化済み)。 */
        cplat_local_lock *lock;      /**< ロード処理を保護するミューテックス。 */
    } cplat_sym_loader_entry;

/**
 *  @brief          cplat_sym_loader_entry 静的変数の初期化マクロです。
 *
 *  @param[in]      key     この関数インスタンスの識別キー (文字列リテラル)。
 *  @param[in]      type    格納する関数ポインターの型 (例: sample_func_t)。
 */
#define CPLAT_SYM_LOADER_ENTRY_INIT(key, type) \
    {(key), {0}, {0}, NULL, CPLAT_ATOMIC_INIT(NULL), CPLAT_ATOMIC_INIT(0), CPLAT_ATOMIC_INIT(0), NULL}

    /**
     *  @brief          拡張関数ポインターを返します (内部用)。
     *
     *  @param[in]      fobj cplat_sym_loader_entry へのポインター。
     *  @return         成功時 void * (関数ポインター)、失敗時 NULL。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void *CPLAT_API cplat_sym_loader_resolve(cplat_sym_loader_entry *fobj);

/**
 *  @brief          拡張関数ポインターを返します。
 *
 *  @param[in]      fobj cplat_sym_loader_entry へのポインター。
 *  @param[in]      type CPLAT_SYM_LOADER_ENTRY_INIT で指定したものと同じ関数ポインター型。
 *  @return         成功時指定された型の関数ポインター、失敗時 NULL。
 *
 *  @par            スレッド セーフ
 *  本マクロはスレッド セーフです。
 */
#define cplat_sym_loader_resolve_as(fobj, type) ((type)cplat_sym_loader_resolve(fobj))

    /**
     *  @brief          cplat_sym_loader_entry が明示的既定値かどうかを返します。
     *
     *  @param[in]      fobj cplat_sym_loader_entry へのポインター。
     *  @return         明示的既定値の場合は 1、それ以外は 0。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_sym_loader_is_default(cplat_sym_loader_entry *fobj);

    /**
     *  @brief          cplat_sym_loader_entry ポインター配列を初期化します。
     *
     *  @param[in]      fobj_array  cplat_sym_loader_entry ポインター配列。
     *  @param[in]      fobj_length 配列の要素数。
     *  @param[in]      configpath  JSONC 定義ファイルのパス。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  DLL ロード直後のシングル スレッド フェーズで呼び出してください。複数スレッドから同時に @p fobj_array の同一エントリへ書き込むと競合が発生します。
     */
    CPLAT_EXPORT void CPLAT_API cplat_sym_loader_init(cplat_sym_loader_entry *const *fobj_array,
                                                      const size_t fobj_length, const char *configpath);

    /**
     *  @brief          cplat_sym_loader_entry ポインター配列を解放します。
     *
     *  @param[in]      fobj_array  cplat_sym_loader_entry ポインター配列。
     *  @param[in]      fobj_length 配列の要素数。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  DllMain / destructor コンテキストのシングル スレッド フェーズで呼び出してください。他スレッドが resolve を実行中の場合、解放と競合します。
     */
    CPLAT_EXPORT void CPLAT_API cplat_sym_loader_dispose(cplat_sym_loader_entry *const *fobj_array,
                                                         const size_t fobj_length);

    /**
     *  @brief          cplat_sym_loader_entry ポインター配列の内容を標準出力に表示します。
     *
     *  @param[in]      fobj_array      cplat_sym_loader_entry ポインター配列。要素数が 0 の場合は NULL 可。
     *  @param[in]      fobj_length     配列の要素数。
     *  @retval         CPLAT_OK                    すべてのエントリが正常に解決されています。
     *  @retval         CPLAT_ERR_UNKNOWN                   1 つ以上のエントリを解決できません。
     *  @retval         CPLAT_ERR_INVALID_ARGUMENT  配列または配列要素が NULL です。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_sym_loader_info(cplat_sym_loader_entry *const *fobj_array,
                                                     const size_t fobj_length);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_SYM_LOADER_H */
