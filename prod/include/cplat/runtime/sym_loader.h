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
 *  3. cplat_sym_loader_count() と cplat_sym_loader_get_as() で関数ポインターを取得して呼び出します。
 *  4. cplat_sym_loader_dispose() でリソースを解放します (DllMain / destructor から呼び出します)。
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

/**
 *  @brief          終端を含む名前バッファー長です。
 *
 *  設定された名前の上限そのものではありません。\n
 *  設定名は終端を除く 255 バイトまで解決でき、この定数は終端を含む 256 です。
 */
#define CPLAT_SYM_LOADER_NAME_MAX 256

/** 名前が空である診断理由です。 */
#define CPLAT_SYM_LOADER_REJECT_EMPTY_NAME (-1)

/** 名前が長すぎる診断理由です。 */
#define CPLAT_SYM_LOADER_REJECT_NAME_TOO_LONG (-2)

/** ライブラリを開けない診断理由です。 */
#define CPLAT_SYM_LOADER_REJECT_OPEN_FAILED (-3)

/** シンボルが見つからない診断理由です。 */
#define CPLAT_SYM_LOADER_REJECT_SYMBOL_NOT_FOUND (-4)

/** 複数の移譲先に明示的既定値が含まれるため、キー全体を未適用にした診断理由です。 */
#define CPLAT_SYM_LOADER_REJECT_MIXED_DEFAULT (-5)

/** lib と func の片方だけが default であるため、その先を開かなかった診断理由です。 */
#define CPLAT_SYM_LOADER_REJECT_PARTIAL_DEFAULT (-6)

/** 同じキーの後の値で置き換えたため、前の先を捨てた診断理由です。 */
#define CPLAT_SYM_LOADER_REJECT_DUPLICATE_KEY (-7)

    /** 移譲先の列と診断の列を保持する非公開の管理構造体です。 */
    typedef struct cplat_sym_loader_entry_impl cplat_sym_loader_entry_impl;

    /**
     *  @brief          関数ポインター キャッシュ エントリです。
     *
     *  1 つのキーに対する移譲先の列を、非公開の管理構造体で保持します。\n
     *  静的変数として定義する場合は CPLAT_SYM_LOADER_ENTRY_INIT マクロで初期化してください。\n
     *  resolved は 0 または 1 です。1 は、利用側の列と診断の列を公開したあとに release で書きます。\n
     *  failed は 0 または 1 です。内部失敗のあいだは resolved を 0 のままにします。\n
     *  lock_state は 0 が未初期化、1 が初期化中、2 が初期化済みです。
     */
    typedef struct cplat_sym_loader_entry
    {
        const char *func_key;                 /**< この関数インスタンスの識別キー。 */
        cplat_sym_loader_entry_impl *impl;    /**< 移譲先の列。未設定のときは NULL。 */
        cplat_atomic_i32 resolved;            /**< 解決済みフラグ (0 = 未解決、1 = 公開済み)。 */
        cplat_atomic_i32 failed;              /**< 内部失敗フラグ (0 = 失敗ではない、1 = 失敗中)。 */
        cplat_atomic_i32 lock_state;          /**< ロック初期化状態 (0=未初期化,1=初期化中,2=初期化済み)。 */
        cplat_local_lock *lock;               /**< ロード処理を保護するミューテックス。 */
    } cplat_sym_loader_entry;

/**
 *  @brief          cplat_sym_loader_entry 静的変数の初期化マクロです。
 *
 *  @param[in]      key     この関数インスタンスの識別キー (文字列リテラル)。
 */
#define CPLAT_SYM_LOADER_ENTRY_INIT(key) \
    {(key), NULL, CPLAT_ATOMIC_INIT(0), CPLAT_ATOMIC_INIT(0), CPLAT_ATOMIC_INIT(0), NULL}

    /**
     *  @brief          cplat_sym_loader_entry ポインター配列へ JSONC 設定を読み込みます。
     *
     *  値がオブジェクトなら移譲先は 1 件、配列なら要素ごとに 1 件です。\n
     *  設定ファイルが無い、読めない、解析できない場合は、エントリを失敗にしません。
     *
     *  @param[in]      fobj_array   cplat_sym_loader_entry ポインター配列。
     *  @param[in]      fobj_length  配列の要素数。
     *  @param[in]      configpath   JSONC 定義ファイルのパス。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  DLL ロード直後のシングルスレッド フェーズで呼び出してください。複数スレッドから同時に @p fobj_array の同一エントリへ書き込むと競合が発生します。
     */
    CPLAT_EXPORT void CPLAT_API cplat_sym_loader_init(cplat_sym_loader_entry *const *fobj_array,
                                                      const size_t fobj_length, const char *configpath);

    /**
     *  @brief          cplat_sym_loader_entry ポインター配列の移譲先を解放します。
     *
     *  開いたモジュールは、開いた回数だけ閉じます。ロックは破棄しません。
     *
     *  @param[in]      fobj_array   cplat_sym_loader_entry ポインター配列。
     *  @param[in]      fobj_length  配列の要素数。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  DllMain / destructor コンテキストのシングルスレッド フェーズで呼び出してください。他スレッドが count または get を実行中の場合、解放と競合します。
     */
    CPLAT_EXPORT void CPLAT_API cplat_sym_loader_dispose(cplat_sym_loader_entry *const *fobj_array,
                                                         const size_t fobj_length);

    /**
     *  @brief          cplat_sym_loader_entry が明示的既定値かどうかを返します。
     *
     *  移譲先が 1 件で、lib と func がどちらも default のときだけ 1 です。\n
     *  未解決なら、返す前に解決します。内部失敗が有効なあいだは 0 です。
     *
     *  @param[in]      fobj  cplat_sym_loader_entry へのポインター。
     *  @return         明示的既定値の場合は 1、それ以外は 0。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_sym_loader_is_default(cplat_sym_loader_entry *fobj);

    /**
     *  @brief          cplat_sym_loader_entry の内部失敗が有効かを返します。
     *
     *  すでに failed が 1 のときは 1 を返し、失敗を解消しません。\n
     *  未解決で失敗でもないときは、返す前に解決を試みます。
     *
     *  @param[in]      fobj  cplat_sym_loader_entry へのポインター。
     *  @return         内部失敗が有効なあいだは 1、それ以外は 0。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_sym_loader_is_failed(cplat_sym_loader_entry *fobj);

    /**
     *  @brief          cplat_sym_loader_entry の利用側の列を標準出力へ表示します。
     *
     *  表示するのは、キー、明示的既定値、利用側の件数、残った先のライブラリ名、関数名、関数ポインターです。\n
     *  診断へ外した先と、内部失敗の値は表示しません。外したことだけでは失敗にしません。\n
     *  未解決なら、表示する前に解決を試みます。内部失敗が残っていても、引数が正しければ CPLAT_OK です。
     *
     *  @param[in]      fobj_array   cplat_sym_loader_entry ポインター配列。要素数が 0 の場合は NULL 可。
     *  @param[in]      fobj_length  配列の要素数。
     *  @retval         CPLAT_OK                   引数が正しい。
     *  @retval         CPLAT_ERR_INVALID_ARGUMENT 配列または配列要素が NULL です。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_sym_loader_info(cplat_sym_loader_entry *const *fobj_array,
                                                     const size_t fobj_length);

    /**
     *  @brief          利用側へ渡す移譲先の件数を返します。
     *
     *  0 は移譲先なしです。未解決なら、返す前に解決します。\n
     *  内部失敗が有効なあいだは 0 です。
     *
     *  @param[in]      fobj  cplat_sym_loader_entry へのポインター。
     *  @return         利用側の列の件数。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT size_t CPLAT_API cplat_sym_loader_count(cplat_sym_loader_entry *fobj);

    /**
     *  @brief          利用側の列から、指定した添字の関数ポインターを返します。
     *
     *  先頭は添字 0 です。範囲外、または内部失敗のあいだは NULL です。\n
     *  返したポインターは dispose するまで有効です。呼び出し側は解放しません。
     *
     *  @param[in]      fobj   cplat_sym_loader_entry へのポインター。
     *  @param[in]      index  利用側の列の添字。
     *  @return         関数ポインター。範囲外のときは NULL。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void *CPLAT_API cplat_sym_loader_get(cplat_sym_loader_entry *fobj, size_t index);

/**
 *  @brief          指定した添字の関数ポインターを、指定した型で返します。
 *
 *  @param          fobj   cplat_sym_loader_entry へのポインター。
 *  @param          index  利用側の列の添字。先頭は 0。
 *  @param          type   関数ポインター型。
 */
#define cplat_sym_loader_get_as(fobj, index, type) ((type)cplat_sym_loader_get((fobj), (index)))

    /**
     *  @brief          診断の列の件数を返します。
     *
     *  未解決なら、返す前に解決します。内部失敗が有効なあいだは 0 です。
     *
     *  @param[in]      fobj  cplat_sym_loader_entry へのポインター。
     *  @return         診断の列の件数。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT size_t CPLAT_API cplat_sym_loader_reject_count(cplat_sym_loader_entry *fobj);

    /**
     *  @brief          診断の列から、ライブラリ名、関数名、理由を取得します。
     *
     *  成功時は 3 つの出力引数へ書き、CPLAT_OK を返します。\n
     *  範囲外の添字、または出力引数のいずれかが NULL のときは CPLAT_ERR_INVALID_ARGUMENT を返し、出力引数へは書きません。\n
     *  返した文字列は dispose するまで有効です。呼び出し側は解放しません。理由の値は、本関数の戻り値ではありません。
     *
     *  @param[in]      fobj       cplat_sym_loader_entry へのポインター。
     *  @param[in]      index      診断の列の添字。
     *  @param[out]     lib_name   ライブラリ名の受け取り先。NULL を渡してはなりません。
     *  @param[out]     func_name  関数名の受け取り先。NULL を渡してはなりません。
     *  @param[out]     reason     診断理由の受け取り先。NULL を渡してはなりません。
     *  @retval         CPLAT_OK                   名前と理由を書き込んだ。
     *  @retval         CPLAT_ERR_INVALID_ARGUMENT 添字が範囲外、または出力引数が NULL。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_sym_loader_reject_get(cplat_sym_loader_entry *fobj, size_t index,
                                                           const char **lib_name, const char **func_name, int *reason);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_SYM_LOADER_H */
