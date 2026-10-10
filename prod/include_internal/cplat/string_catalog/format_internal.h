/**
 *******************************************************************************
 *  @file           format_internal.h
 *  @brief          文字列カタログの引数の取り出しと、位置指定書式の展開を宣言します。
 *  @author         Tetsuo Honda
 *  @date           2026/10/01
 *  @version        1.0.0
 *
 *  cplat ライブラリ内でのみ使用する内部ヘッダーです。利用者は取り込みません。\n
 *  文字列カタログの書式展開と、条件式フィルターの判定が、同じ取り出し結果を共有するために使用します。
 *
 *  本ヘッダーで宣言する関数は NULL チェックを行いません。前提条件は各関数の Doxygen コメントに記載します。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#ifndef CPLAT_STRING_CATALOG_FORMAT_INTERNAL_H
#define CPLAT_STRING_CATALOG_FORMAT_INTERNAL_H

#include <cplat/base/result.h>
#include <cplat/string_catalog/argument.h>
#include <cplat/string_catalog/catalog.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          可変長引数から取り出した値 1 個分です。
     *
     *  @ref cplat_internal_string_catalog_argument_value::kind が、共用体のどのメンバーが有効かを表します。\n
     *  可変長引数は順次取り出す必要があるため、書式を展開する前に本構造体の配列へ格納します。\n
     *  これにより、位置指定の並べ替えと繰り返し参照を行えます。
     */
    typedef struct cplat_internal_string_catalog_argument_value
    {
        cplat_string_catalog_argument_kind kind; /**< 有効な共用体メンバーを表す引数種別です。 */
        unsigned int pad;                        /**< 明示的アラインメントです。0 を指定します。 */

        /**
     *  @brief          引数種別ごとの値です。
     */
        union cplat_internal_string_catalog_argument_storage
        {
            const char *string_value;  /**< @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_STRING の値です。 */
            char char_value;           /**< @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_CHAR の値です。 */
            int8_t int8_value;         /**< @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_INT8 の値です。 */
            uint8_t uint8_value;       /**< @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_UINT8 と HEX8 の値です。 */
            int16_t int16_value;       /**< @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_INT16 の値です。 */
            uint16_t uint16_value;     /**< @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_UINT16 と HEX16 の値です。 */
            int32_t int32_value;       /**< @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_INT32 の値です。 */
            uint32_t uint32_value;     /**< @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_UINT32 と HEX32 の値です。 */
            int64_t int64_value;       /**< @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_INT64 と SSIZE の値です。 */
            uint64_t uint64_value;     /**< @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_UINT64 と HEX64 の値です。 */
            size_t size_value;         /**< @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_SIZE の値です。 */
            const void *pointer_value; /**< @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_POINTER の値です。 */
            double double_value;       /**< @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_DOUBLE の値です。 */
            int error_code_value;      /**< @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_ERROR_CODE の値です。 */
        } value;
    } cplat_internal_string_catalog_argument_value;

    /**
     *  @brief          引数スキーマに従って、可変長引数を値の配列へ取り出します。
     *  @param[in]      entry  カタログの 1 件。NULL を渡してはなりません。
     *  @param[in]      args       取り出す引数リスト。
     *  @param[out]     values     取り出した値の格納先。
     *                             @ref CPLAT_STRING_CATALOG_ARGUMENT_MAX 個の要素が必要です。
     *  @return         成功時は @ref CPLAT_OK を返します。
     *  @return         引数種別が未知の場合は @ref CPLAT_ERR_MALFORMED_DEFINITION を返します。
     *
     *  @p args は先頭から @ref cplat_string_catalog_entry::argument_count 個だけ読み進めます。\n
     *  @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_UNUSED のインデックスは値を取り出さず、読み飛ばします。\n
     *  引数種別が未知の場合は、その時点で読み取りを打ち切ります。以降の値は取り出せません。
     *
     *  既定引数拡張と一致しない `va_arg` の指定は未定義動作となるため、
     *  引数種別ごとの取り出し型を本関数へ閉じ込めています。\n
     *  `char` と 8 bit、16 bit の整数の種別は `int` として取り出し、種別が表す幅へ変換して格納します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    int cplat_internal_string_catalog_collect_arguments(const cplat_string_catalog_entry *entry, va_list args,
                                                        cplat_internal_string_catalog_argument_value *values);

    /**
     *  @brief          位置指定書式を展開し、文字列を組み立てます。
     *  @param[out]     dest        文字列の格納先。NULL を渡してはなりません。常に NUL 終端します。
     *  @param[in]      dest_size   @p dest のバイト数。1 以上を指定してください。
     *  @param[in]      text        展開する書式。NULL を渡してはなりません。
     *  @param[in]      values      展開に使用する値の配列。NULL を渡してはなりません。
     *  @param[in]      value_count @p values の有効な要素数。
     *  @return         成功時は @ref CPLAT_OK を返します。
     *  @return         書式の構文が不正な場合、位置指定が @p value_count 以上のインデックスを指す場合、
     *                  または @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_UNUSED の値を指す場合は
     *                  @ref CPLAT_ERR_MALFORMED_DEFINITION を返します。
     *  @return         結果が @p dest に収まらない場合は、切り詰めたうえで
     *                  @ref CPLAT_ERR_BUFFER_TOO_SMALL を返します。
     *
     *  書式の構文は `{0}` から `{49}` までの位置指定と、`{{` と `}}` のエスケープのみです。\n
     *  インデックスは 10 進数で 2 桁までとし、先行ゼロは許可しません。\n
     *  書式指定は解釈しません。文字列表現は引数種別側で規定されます。
     *
     *  構文が不正な場合でも @p dest は NUL 終端します。内容は保証しません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    int cplat_internal_string_catalog_render_text(char *dest, size_t dest_size, const char *text,
                                                  const cplat_internal_string_catalog_argument_value *values,
                                                  int value_count);

    /**
     *  @brief          文字列キーの項目を検索し、現在の言語の書式を選び、可変長引数を値の配列へ取り出します。
     *  @param[in]      catalog    カタログ。
     *  @param[in]      string_key 文字列キー。
     *  @param[in]      args       取り出す引数リスト。
     *  @param[out]     entry_out  見つかった項目の格納先。NULL を渡してはなりません。
     *  @param[out]     text_out   選んだ書式の格納先。NULL を渡してはなりません。
     *  @param[out]     values     取り出した値の格納先。NULL を渡してはなりません。
     *                             @ref CPLAT_STRING_CATALOG_ARGUMENT_MAX 個の要素が必要です。
     *  @return         成功時は @ref CPLAT_OK を返します。
     *  @return         @p catalog が使用できない場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *  @return         文字列キーの項目がない場合は @ref CPLAT_ERR_NOT_FOUND を返します。
     *  @return         項目の引数定義または書式が不正な場合は @ref CPLAT_ERR_MALFORMED_DEFINITION を返します。
     *
     *  `cplat_string_catalog_vformat` の組み立て前の処理です。\n
     *  取り出した値は @ref cplat_internal_string_catalog_render_text へ渡して文字列を組み立てます。\n
     *  同じ値を判定にも使うことで、可変長引数を二度取り出さずに済みます。\n
     *  失敗した場合、@p entry_out と @p text_out は変更しません。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  言語設定を同時に変更しない場合は、同時に実行できます。\n
     *  他スレッドが言語設定を変更する場合は、呼び出し側で同期してください。\n
     *  cplat_string_catalog_set_language() で言語を設定しておらず、言語がまだ決まっていない場合は、
     *  環境変数から言語を決定します。\n
     *  このとき、他スレッドが環境変数を同時に変更する場合は、呼び出し側で同期してください。
     */
    int cplat_internal_string_catalog_prepare_format(const cplat_string_catalog *catalog, int string_key, va_list args,
                                                     const cplat_string_catalog_entry **entry_out,
                                                     const char **text_out,
                                                     cplat_internal_string_catalog_argument_value *values);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CPLAT_STRING_CATALOG_FORMAT_INTERNAL_H */
