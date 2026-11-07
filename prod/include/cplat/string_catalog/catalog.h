/**
 *******************************************************************************
 *  @file           catalog.h
 *  @brief          利用者が注入するカタログ 1 件分の表現を定義します。
 *  @author         Tetsuo Honda
 *  @date           2026/09/10
 *  @version        1.0.0
 *
 *  カタログはライブラリ側では保持せず、利用側で定義します。\n
 *  配列とインデックス表を @ref cplat_string_catalog へまとめ、組み立て API の呼び出しごとに渡します。\n
 *  利用側で用意するのは文字列キーの列挙と本構造体の配列の 2 点のみです。\n
 *  項目の `key` は処理から項目を参照する識別子で、定義間で一意な値とします。\n
 *  項目の `id` は処理では意味を持たない補足の文字列です。ライブラリは解釈も検査もしません。\n
 *  言語、引数種別、レベル、書式の構文はライブラリが定めます。
 *
 *  文字列キーの型を列挙にせず `int` としているのは、列挙を利用者側で定義できるようにするためです。\n
 *  利用者は任意の名前の列挙を定義し、その定数をそのまま渡せます。
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

#ifndef CPLAT_STRING_CATALOG_CATALOG_H
#define CPLAT_STRING_CATALOG_CATALOG_H

#include <cplat/string_catalog/argument.h>
#include <cplat/string_catalog/language.h>

/**
 *  @ingroup        CPLAT_STRING_CATALOG
 *  @{
 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          文字列の 1 つの引数に対する定義です。
     *
     *  引数の種別と、利用側が参照する名前および説明を保持します。\n
     *  配列の先頭から、対応する @ref cplat_string_catalog_entry::argument_count 個までが有効です。
     *
     *  @ref cplat_string_catalog_argument::pad は明示的アラインメントです。\n
     *  配列の初期化子では 0 を指定してください。
     */
    typedef struct cplat_string_catalog_argument
    {
        cplat_string_catalog_argument_kind kind; /**< 引数の種別です。 */
        unsigned int pad;                        /**< 明示的アラインメントです。0 を指定します。 */
        const char *name;                        /**< 引数の名前です。NULL にできません。 */
        const char *description;                 /**< 引数の説明です。NULL にできません。 */
    } cplat_string_catalog_argument;

    /**
     *  @brief          1 つの文字列定義が持つカタログの 1 件分です。
     *
     *  文字列キー、補足の ID、引数スキーマ、分類値、メタデータ、言語別の書式と備考を 1 つの表で保持します。\n
     *  @ref cplat_string_catalog_entry::arguments が指す配列の先頭から、
     *  @ref cplat_string_catalog_entry::argument_count 個までが有効です。引数がない場合は NULL を指定します。
     *
     *  @ref cplat_string_catalog_entry::id と @ref cplat_string_catalog_entry::category は、ライブラリが解釈しない補足情報です。\n
     *  ID は省略でき、NULL を指定できます。定義間の重複も検査しません。\n
     *  値の意味と有効な範囲は利用者が決めます。ライブラリは保持して返すだけです。
     *
     *  @ref cplat_string_catalog_entry::brief は、文字列の短い説明です。\n
     *  @ref cplat_string_catalog_entry::details は、文字列の詳細説明です。省略でき、NULL を指定できます。\n
     *  @ref cplat_string_catalog_entry::remarks は、文字列の補足説明です。省略でき、NULL を指定できます。
     *
     *  @ref cplat_string_catalog_entry::texts と @ref cplat_string_catalog_entry::notes は、
     *  言語をインデックスとして参照します。\n
     *  ニュートラル言語以外の要素が NULL の場合は、ニュートラル言語の要素を使用します。\n
     *  ニュートラル言語の要素は NULL にできません。
     *
     *  この 2 つの配列は、@ref cplat_string_catalog_language をキーとした指示付き初期化子で記載できます。\n
     *  記載しなかった言語の要素は暗黙にヌル ポインターとなるため、リソースを持たない言語を省略できます。
     *
     *  @ref cplat_string_catalog_entry::pad は明示的アラインメントです。\n
     *  配列の初期化子では 0 を指定してください。
     */
    typedef struct cplat_string_catalog_entry
    {
        int key;            /**< 文字列キーです。処理から項目を参照する識別子で、定義間で一意な値を指定します。 */
        int category;       /**< 利用者が意味を決める分類値です。0 は分類なしを表します。 */
        int argument_count; /**< 引数の個数です。0 以上、上限以下です。 */
        unsigned int pad;   /**< 明示的アラインメントです。0 を指定します。 */
        const cplat_string_catalog_argument *arguments; /**< 引数の定義配列です。引数がない場合は NULL です。 */
        const char *id;      /**< 処理では意味を持たない補足の ID です。省略時は NULL です。 */
        const char *brief;   /**< 文字列の短い説明です。NULL にできません。 */
        const char *details; /**< 文字列の詳細説明です。省略時は NULL です。 */
        const char *remarks; /**< 文字列の補足説明です。NULL を指定できます。 */
        const char *texts[CPLAT_STRING_CATALOG_LANGUAGE_COUNT]; /**< 言語別の書式です。NULL は自動選択です。 */
        const char *notes[CPLAT_STRING_CATALOG_LANGUAGE_COUNT]; /**< 言語別の備考です。NULL は自動選択です。 */
    } cplat_string_catalog_entry;

    /**
     *  @brief          1 つのカタログを識別します。
     *
     *  カタログの配列と、文字列キーから配列のインデックスを参照する表を 1 つにまとめた値です。\n
     *  ライブラリはこの値を保持せず、API の呼び出しごとに受け取ります。\n
     *  1 つのプロセスで複数のカタログを扱えます。
     *
     *  すべてのメンバーを初期化子で与えられるため、静的記憶域期間を持つ `const` として定義できます。\n
     *  配列はコピーせず、ポインターだけを保持します。
     *  指す領域は、このカタログを使用する間ずっと有効である必要があります。
     *
     *  @ref cplat_string_catalog::key_index は、文字列キーからカタログ項目を検索する探索コストを低減するための変換表です。\n
     *  文字列キーをインデックスとして @ref cplat_string_catalog::entries のインデックスを格納し、
     *  登録のないインデックスには負の値を格納します。\n
     *  不要な場合は NULL と 0 を指定します。この場合は線形探索になります。
     *
     *  メンバーは配列を先に、要素数をあとにまとめています。\n
     *  この順序であれば暗黙のパディングが生じません。
     *
     *  内容の妥当性は @ref cplat_string_catalog_verify で確認します。
     */
    typedef struct cplat_string_catalog
    {
        const cplat_string_catalog_entry *entries; /**< カタログの配列です。NULL にできません。 */
        const int *
            key_index; /**< 文字列キーをインデックスとして entries のインデックスを参照する変換表です。不要な場合は NULL です。 */
        int entry_count;     /**< entries の要素数です。0 以上を指定します。 */
        int key_index_count; /**< key_index の要素数です。key_index が NULL の場合は 0 を指定します。 */
    } cplat_string_catalog;

#ifdef __cplusplus
}
#endif /* __cplusplus */

/**
 *  @brief          呼び出し側が用意するバッファーの推奨サイズです。
 *
 *  ライブラリ側の制限ではありません。呼び出し側が任意の容量を指定できます。
 */
#define CPLAT_STRING_CATALOG_TEXT_MAX 512

/** @} */

#endif /* CPLAT_STRING_CATALOG_CATALOG_H */
