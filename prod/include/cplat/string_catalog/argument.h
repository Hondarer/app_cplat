/**
 *******************************************************************************
 *  @file           argument.h
 *  @brief          文字列引数の種別を定義します。
 *  @author         Tetsuo Honda
 *  @date           2026/09/10
 *  @version        1.0.0
 *
 *  引数種別は、可変長引数から取り出す C の型と、文字列化の書式を 1 つにまとめた列挙です。\n
 *  型と書式を別々の列挙に分けると、意味を持たない組み合わせを表現できてしまうため、
 *  1 つの列挙として扱います。
 *
 *  引数種別は文字列キー側の定義が保持します。言語別リソースは語順だけを決め、
 *  文字列表現には関与しません。\n
 *  同じ文字列キーの `{0}` は、どの言語でも同じ表現になります。
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

#ifndef CPLAT_STRING_CATALOG_ARGUMENT_H
#define CPLAT_STRING_CATALOG_ARGUMENT_H

/**
 *  @ingroup        CPLAT_STRING_CATALOG
 *  @{
 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          文字列引数として渡す値の種別を表します。
     *
     *  種別ごとに、可変長引数へ渡す型と文字列表現が次のとおり決まります。
     *
     *  | 種別                                          | 渡す型          | 文字列表現の例         |
     *  | --------------------------------------------- | --------------- | ---------------------- |
     *  | @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_UNUSED     | なし            | なし                   |
     *  | @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_STRING     | `const char *`  | `config.json`          |
     *  | @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_CHAR       | `char`          | `'A'` / `138 (0x8a)`   |
     *  | @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_INT8       | `int8_t`        | `-12`                  |
     *  | @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_UINT8      | `uint8_t`       | `200`                  |
     *  | @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_INT16      | `int16_t`       | `-1200`                |
     *  | @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_UINT16     | `uint16_t`      | `48000`                |
     *  | @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_INT32      | `int32_t`       | `-12`                  |
     *  | @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_UINT32     | `uint32_t`      | `12`                   |
     *  | @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_INT64      | `int64_t`       | `-4294967296`          |
     *  | @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_UINT64     | `uint64_t`      | `4294967296`           |
     *  | @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_HEX8       | `uint8_t`       | `0x8a`                 |
     *  | @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_HEX16      | `uint16_t`      | `0xbeef`               |
     *  | @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_HEX32      | `uint32_t`      | `0x1234abcd`           |
     *  | @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_HEX64      | `uint64_t`      | `0x00000000deadbeef`   |
     *  | @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_SIZE       | `size_t`        | `4096`                 |
     *  | @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_SSIZE      | `int64_t`       | `-1`                   |
     *  | @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_POINTER    | `const void *`  | `0x00007fffa1234567`   |
     *  | @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_DOUBLE     | `double`        | `12.5`                 |
     *  | @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_ERROR_CODE | `int`           | `2 (0x00000002)`       |
     *
     *  Table: 文字列カタログ引数の種別と渡す型
     *
     *  16 進表現は、桁数を種別で固定し、英小文字で出力します。\n
     *  @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_CHAR は、ASCII の印字可能な範囲 (`0x20` から `0x7e`) を
     *  単引用符で囲んだ 1 文字として出力し、それ以外は値を 10 進数と 16 進数で併記します。
     *  印字可能かどうかの判定はロケールへ依存させず、プラットフォーム間で表現を揃えます。\n
     *  @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_POINTER は、プラットフォーム間で表現を揃えるため、
     *  `%p` ではなくポインター幅の 16 進表現へ変換します。
     *
     *  @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_SSIZE は、符号付きのバイト数や要素数を受け取ります。\n
     *  POSIX の `ssize_t` は Windows に無く、幅も処理系で異なるため、公開契約の型は `int64_t` とします。
     *  呼び出し側で `int64_t` へ変換してから渡してください。
     *
     *  既定引数拡張のため、`char` と 8 bit、16 bit の整数は `int` へ、`float` は `double` へ昇格します。\n
     *  8 bit と 16 bit の種別は、`va_arg` で `int` として取り出したあと、種別が表す幅へ変換します。
     *  昇格後の型と一致しない `va_arg` の指定は行いません。\n
     *  単精度浮動小数点数の種別を設けていないのは、`double` へ昇格した値を倍精度として扱えば十分なためです。
     *
     *  @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_ERROR_CODE は、Linux の `errno` と
     *  Windows のエラー コードのどちらも `int` として受け取り、10 進数と 16 進数を併記します。
     *
     *  @ref CPLAT_STRING_CATALOG_ARGUMENT_KIND_UNUSED は、引数を割り当てないインデックスを表します。\n
     *  可変長引数から値を取り出さず、書式からも参照できません。
     *  引数配列の途中に、値を渡さないインデックスを設ける場合に使用します。\n
     *  値を 0 としているのは、要素を明示しない初期化子と、ゼロで初期化した領域が、
     *  そのまま未使用を表すようにするためです。
     */
    typedef enum cplat_string_catalog_argument_kind
    {
        CPLAT_STRING_CATALOG_ARGUMENT_KIND_UNUSED = 0, /**< 引数を割り当てないインデックス。値を取り出しません。 */
        CPLAT_STRING_CATALOG_ARGUMENT_KIND_STRING = 1, /**< NUL 終端の文字列。NULL は `(null)` と表現します。 */
        CPLAT_STRING_CATALOG_ARGUMENT_KIND_CHAR = 2,  /**< 1 文字。印字できない場合は 10 進数と 16 進数を併記します。 */
        CPLAT_STRING_CATALOG_ARGUMENT_KIND_INT8 = 3,  /**< 符号付き 8 bit 整数。10 進数で表現します。 */
        CPLAT_STRING_CATALOG_ARGUMENT_KIND_UINT8 = 4, /**< 符号なし 8 bit 整数。10 進数で表現します。 */
        CPLAT_STRING_CATALOG_ARGUMENT_KIND_INT16 = 5, /**< 符号付き 16 bit 整数。10 進数で表現します。 */
        CPLAT_STRING_CATALOG_ARGUMENT_KIND_UINT16 = 6,   /**< 符号なし 16 bit 整数。10 進数で表現します。 */
        CPLAT_STRING_CATALOG_ARGUMENT_KIND_INT32 = 7,    /**< 符号付き 32 bit 整数。10 進数で表現します。 */
        CPLAT_STRING_CATALOG_ARGUMENT_KIND_UINT32 = 8,   /**< 符号なし 32 bit 整数。10 進数で表現します。 */
        CPLAT_STRING_CATALOG_ARGUMENT_KIND_INT64 = 9,    /**< 符号付き 64 bit 整数。10 進数で表現します。 */
        CPLAT_STRING_CATALOG_ARGUMENT_KIND_UINT64 = 10,  /**< 符号なし 64 bit 整数。10 進数で表現します。 */
        CPLAT_STRING_CATALOG_ARGUMENT_KIND_HEX8 = 11,    /**< 符号なし 8 bit 整数。2 桁の 16 進数で表現します。 */
        CPLAT_STRING_CATALOG_ARGUMENT_KIND_HEX16 = 12,   /**< 符号なし 16 bit 整数。4 桁の 16 進数で表現します。 */
        CPLAT_STRING_CATALOG_ARGUMENT_KIND_HEX32 = 13,   /**< 符号なし 32 bit 整数。8 桁の 16 進数で表現します。 */
        CPLAT_STRING_CATALOG_ARGUMENT_KIND_HEX64 = 14,   /**< 符号なし 64 bit 整数。16 桁の 16 進数で表現します。 */
        CPLAT_STRING_CATALOG_ARGUMENT_KIND_SIZE = 15,    /**< オブジェクトのバイト数や要素数。10 進数で表現します。 */
        CPLAT_STRING_CATALOG_ARGUMENT_KIND_SSIZE = 16,   /**< 符号付きのバイト数や要素数。10 進数で表現します。 */
        CPLAT_STRING_CATALOG_ARGUMENT_KIND_POINTER = 17, /**< オブジェクトのアドレス。16 進数で表現します。 */
        CPLAT_STRING_CATALOG_ARGUMENT_KIND_DOUBLE = 18,  /**< 倍精度浮動小数点数。有効桁を保った簡潔な表現にします。 */
        CPLAT_STRING_CATALOG_ARGUMENT_KIND_ERROR_CODE = 19 /**< OS のエラー コード。10 進数と 16 進数を併記します。 */
    } cplat_string_catalog_argument_kind;

#ifdef __cplusplus
}
#endif /* __cplusplus */

/**
 *  @brief          1 つの文字列が取り得る引数の最大個数です。
 *
 *  書式中の位置指定は `{0}` から `{49}` までとなります。インデックスは 0 起点です。\n
 *  書式解析は 2 桁までのインデックスを受け付けるため、この値の上限は 100 です。\n
 *  100 を超える値を設定する場合は、書式解析の桁数の上限も合わせて拡張してください。
 *
 *  この値は文字列組み立て 1 回あたりのスタック消費量を決定します。\n
 *  @ref cplat_string_catalog_entry は実際の引数個数分の定義配列を参照し、文字列組み立て処理では値の配列をスタック上に確保します。\n
 *  値の配列は実際の引数の個数ではなく、この上限値に比例します。
 */
#define CPLAT_STRING_CATALOG_ARGUMENT_MAX 50

/** @} */

#endif /* CPLAT_STRING_CATALOG_ARGUMENT_H */
