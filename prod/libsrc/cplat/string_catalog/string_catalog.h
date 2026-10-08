/**
 *******************************************************************************
 *  @file           string_catalog.h
 *  @brief          位置指定書式の確認を宣言します。
 *  @author         Tetsuo Honda
 *  @date           2026/09/10
 *  @version        1.0.0
 *
 *  本ヘッダーは `prod/libsrc/cplat/string_catalog/` のモジュール私有ヘッダーです。\n
 *  同一ディレクトリ内の実装ファイルからのみ `#include "string_catalog.h"` で取り込みます。\n
 *  公開契約は公開ヘッダー `<cplat/string_catalog/string_catalog.h>` を正とします。
 *
 *  本ヘッダーで宣言する関数は、呼び出し元が同一ディレクトリ内に限定されるため、NULL チェックは行いません。\n
 *  前提条件は各関数の Doxygen コメントに記載します。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef STRING_CATALOG_PRIVATE_H
#define STRING_CATALOG_PRIVATE_H

#include <cplat/base/result.h>
#include <cplat/string_catalog/argument.h>
#include <cplat/string_catalog/catalog_internal.h>
#include <cplat/string_catalog/format_internal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          位置指定書式の構文と、位置指定の範囲を確認します。
     *  @param[in]      text        確認する書式。NULL を渡してはなりません。
     *  @param[in]      arguments   引数の定義配列。NULL を渡した場合は、引数種別を確認しません。
     *  @param[in]      value_count 位置指定が指してよい引数の個数。
     *  @return         書式が正しい場合は @ref CPLAT_OK を返します。
     *  @return         構文が不正な場合、位置指定が @p value_count 以上のインデックスを指す場合、
     *                  または @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_UNUSED のインデックスを指す場合は
     *                  @ref CPLAT_ERR_MALFORMED_DEFINITION を返します。
     *
     *  値を持たずに書式だけを確認するため、カタログ全体の点検に使用します。\n
     *  値を伴う展開では、引数を割り当てないインデックスの参照を
     *  @ref cplat_internal_string_catalog_render_text が値の種別から検出します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    int string_catalog_validate_text(const char *text, const cplat_string_catalog_argument *arguments, int value_count);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* STRING_CATALOG_PRIVATE_H */
