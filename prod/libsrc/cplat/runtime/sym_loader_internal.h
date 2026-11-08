/**
 *******************************************************************************
 *  @file           sym_loader_internal.h
 *  @brief          sym_loader の翻訳単位間で共有する非公開の型と関数を宣言します。
 *  @author         c-modenization-kit sample team
 *  @date           2026/11/08
 *  @version        1.0.0
 *
 *  本ヘッダーは `prod/libsrc/cplat/runtime/` のモジュール私有ヘッダーです。\n
 *  同ディレクトリの実装ファイルから `#include "sym_loader_internal.h"` で取り込みます。\n
 *  公開契約は `<cplat/runtime/sym_loader.h>` を正とします。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#ifndef CPLAT_SYM_LOADER_INTERNAL_H
#define CPLAT_SYM_LOADER_INTERNAL_H

#include <cplat/runtime/sym_loader.h>
#include <stddef.h>

/**
 *  @brief          設定から保持するライブラリ名と関数名の組です。
 */
typedef struct cplat_sym_loader_name_pair
{
    char *lib_name;  /**< 設定されたライブラリ名。呼び出し側が cplat_free で解放します。 */
    char *func_name; /**< 設定された関数名。呼び出し側が cplat_free で解放します。 */
} cplat_sym_loader_name_pair;

/**
 *  @brief          利用側へ渡す、開けた移譲先です。
 */
typedef struct cplat_sym_loader_usable
{
    char *lib_name;             /**< 設定されたライブラリ名。 */
    char *func_name;            /**< 設定された関数名。 */
    CPLAT_MODULE_HANDLE handle; /**< 開いたモジュール。NULL のときは未オープン。 */
    void *func_ptr;             /**< 解決した関数ポインター。 */
} cplat_sym_loader_usable;

/**
 *  @brief          診断の列の 1 件です。
 */
typedef struct cplat_sym_loader_reject_item
{
    char *lib_name;  /**< 設定されたライブラリ名。 */
    char *func_name; /**< 設定された関数名。 */
    int reason;      /**< CPLAT_SYM_LOADER_REJECT_* の診断理由。 */
} cplat_sym_loader_reject_item;

/**
 *  @brief          1 つのキーに対する移譲先の列と診断の列です。
 */
struct cplat_sym_loader_entry_impl
{
    cplat_sym_loader_name_pair *pending;     /**< 未解決の設定。解決が成功すると解放します。 */
    size_t pending_count;                    /**< pending の件数。 */
    cplat_sym_loader_reject_item *duplicates; /**< 同じ適用の中で -7 として保留した先。 */
    size_t duplicate_count;                  /**< duplicates の件数。 */
    cplat_sym_loader_usable *usable;         /**< 公開済みの利用側の列。 */
    size_t usable_count;                     /**< usable の件数。 */
    cplat_sym_loader_reject_item *rejects;   /**< 公開済みの診断の列。 */
    size_t reject_count;                     /**< rejects の件数。 */
    int is_default;                          /**< 明示的既定値のとき 1。 */
    char *config_path;                       /**< 再読込に使う設定ファイルのパス。 */
    int needs_reload;                        /**< 1 のとき、次の取得で設定ファイルを読み直します。 */
    int adopted_in_apply;                    /**< この適用で、すでに値を採用したとき 1。 */
    int failed_this_apply;                   /**< この適用で確保に失敗したとき 1。以降のキーは採用しません。 */
};

/**
 *  @brief          エントリを解決し、公開済みなら 0 を返します。
 *
 *  成功時は resolved が 1 です。失敗時は failed が 1、resolved が 0 です。
 *
 *  @param[in,out]  fobj  対象のエントリ。NULL を渡してはなりません。
 *  @return         成功時は 0、内部失敗のときは -1。
 */
int cplat_sym_loader_ensure(cplat_sym_loader_entry *fobj);

/**
 *  @brief          JSONC 設定ファイルを、指定したエントリへ適用します。
 *
 *  ファイルを適用できたときは 0 です。エントリごとの確保失敗は、そのエントリへ記録済みです。\n
 *  ファイルが無い、読めない、解析できないときは 1 で、エントリは変えません。
 *
 *  @param[in,out]  fobj_array   対象のエントリ配列。
 *  @param[in]      fobj_length  配列の要素数。
 *  @param[in]      configpath   JSONC 定義ファイルのパス。
 *  @return         適用できたとき 0、ファイルを使わなかったとき 1。
 */
int cplat_sym_loader_apply_config(cplat_sym_loader_entry *const *fobj_array, size_t fobj_length,
                                  const char *configpath);

#endif /* CPLAT_SYM_LOADER_INTERNAL_H */
