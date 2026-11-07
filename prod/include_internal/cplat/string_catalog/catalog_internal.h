/**
 *******************************************************************************
 *  @file           catalog_internal.h
 *  @brief          注入されたカタログへの参照 API を宣言します。
 *  @author         Tetsuo Honda
 *  @date           2026/09/10
 *  @version        1.0.0
 *
 *  本ヘッダーは string_catalog モジュール内の共有ヘッダーです。\n
 *  カタログの検索は `prod/libsrc/cplat/string_catalog/string_catalog_catalog.c` が担い、
 *  書式を展開する実装はこの抽象化境界を通してのみカタログを参照します。\n
 *  境界を設けることで、検索方式を変更しても展開処理側は影響を受けません。\n
 *  ライブラリはカタログを保持しません。呼び出し元が @ref cplat_string_catalog を渡します。\n
 *  see: app/cplat/docs/functional-spec/string_catalog.md
 *
 *  カタログ 1 件分の型は公開ヘッダー `<cplat/string_catalog/catalog.h>` を正とします。\n
 *  利用者が配列を組み立てるため、内部型にはできません。
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

#ifndef CPLAT_STRING_CATALOG_CATALOG_INTERNAL_H
#define CPLAT_STRING_CATALOG_CATALOG_INTERNAL_H

#include <cplat/string_catalog/catalog.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          カタログの内容が利用可能な構造であることを確認します。
     *  @param[in]      catalog 確認するカタログ。NULL を渡せます。
     *  @return         利用可能な場合は true を返します。
     *
     *  NULL、配列が NULL、要素数が負のいずれかであれば false を返します。\n
     *  カタログは利用側で静的初期化されるため、参照前に本関数で構造の妥当性を確認します。\n
     *  書式や引数スキーマの妥当性は確認しません。@ref cplat_string_catalog_verify が担います。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    bool cplat_internal_string_catalog_is_usable(const cplat_string_catalog *catalog);

    /**
     *  @brief          カタログの件数を返します。
     *  @param[in]      catalog 参照するカタログ。NULL を渡せます。
     *  @return         文字列の件数です。0 以上を返します。
     *
     *  参照できないカタログでは 0 を返します。\n
     *  @ref cplat_internal_string_catalog_entry_at で走査するときの上限として使用します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    int cplat_internal_string_catalog_entry_count(const cplat_string_catalog *catalog);

    /**
     *  @brief          インデックスを指定してカタログの 1 件を取得します。
     *  @param[in]      catalog 参照するカタログ。NULL を渡せます。
     *  @param[in]      index   0 以上、件数未満のインデックス。
     *  @return         カタログの 1 件です。@p index が範囲外の場合は NULL を返します。
     *
     *  返すポインターは利用者が用意した領域を指します。ライブラリでは解放しません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    const cplat_string_catalog_entry *cplat_internal_string_catalog_entry_at(const cplat_string_catalog *catalog,
                                                                             int index);

    /**
     *  @brief          文字列キーに対応するカタログの 1 件を取得します。
     *  @param[in]      catalog    検索するカタログ。NULL を渡せます。
     *  @param[in]      string_key 検索する文字列キー。
     *  @return         カタログの 1 件です。見つからない場合は NULL を返します。
     *
     *  返すポインターは利用者が用意した領域を指します。ライブラリでは解放しません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    const cplat_string_catalog_entry *cplat_internal_string_catalog_find_entry(const cplat_string_catalog *catalog,
                                                                               int string_key);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CPLAT_STRING_CATALOG_CATALOG_INTERNAL_H */
