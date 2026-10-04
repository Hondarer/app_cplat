/**
 *******************************************************************************
 *  @file           filter.h
 *  @brief          フィルター オブジェクトの内部レイアウトを宣言します。
 *  @author         Tetsuo Honda
 *  @date           2026/09/26
 *  @version        0.1.0
 *
 *  フィルター オブジェクトの実装だけが共有する宣言です。利用側は公開ヘッダー `cplat/string_catalog/filter.h` を使用します。
 *
 *  レイアウトは次のとおりです。すべての値は固定幅の整数型で、アラインメントは 8 バイトです。
 *
 *  | 要素 | 大きさ |
 *  |---|---|
 *  | ヘッダー (@ref string_catalog_filter_image_header) | @ref CPLAT_STRING_CATALOG_FILTER_HEADER_SIZE |
 *  | 行レコード × 行数の上限 | @ref CPLAT_STRING_CATALOG_FILTER_RECORD_SIZE |
 *
 *  Table: フィルター イメージの構成要素と大きさ
 *
 *  行レコードは、見出し (@ref string_catalog_filter_record_header)、命令領域、定数領域の順に並びます。\n
 *  命令領域は行幅と同じ個数の命令 (@ref string_catalog_filter_instruction) を格納できます。\n
 *  定数領域は行幅の 8 倍のバイト数です。定数は見出し (@ref string_catalog_filter_constant_header) と値で構成します。
 *
 *  呼び出し側の領域はアラインメントを保証しないため、読み書きは本ヘッダーの関数で `memcpy` を介して行います。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *******************************************************************************
 */

#ifndef FILTER_PRIVATE_H
#define FILTER_PRIVATE_H

#include <cplat/regex/regex.h>
#include <cplat/string_catalog/filter.h>
#include <cplat/sync/atomic.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** 署名です。メモリ上では "SCFL" の順に並びます (リトル エンディアン)。 */
#define STRING_CATALOG_FILTER_SIGNATURE 0x4C464353U

/** 形式版です。 */
#define STRING_CATALOG_FILTER_FORMAT_VERSION 1U

/** バイト順序の目印です。異なるバイト順序で読むと 0x0201 になります。 */
#define STRING_CATALOG_FILTER_BYTE_ORDER_MARK 0x0102U

/** 定数の見出しのバイト数です。 */
#define STRING_CATALOG_FILTER_CONSTANT_HEADER_SIZE 8U

/** 数値の定数の値のバイト数です。 */
#define STRING_CATALOG_FILTER_CONSTANT_NUMBER_SIZE 8U

/** 定数領域のアラインメントです。 */
#define STRING_CATALOG_FILTER_CONSTANT_ALIGNMENT 8U

/** 定数の flags: 負の整数です。 */
#define STRING_CATALOG_FILTER_CONSTANT_FLAG_NEGATIVE 0x01U

/** 定数の flags: 16 進数で記述された整数です。 */
#define STRING_CATALOG_FILTER_CONSTANT_FLAG_HEXADECIMAL 0x02U

/** ソース領域の署名です。メモリ上では "SCFS" の順に並びます (リトル エンディアン)。 */
#define STRING_CATALOG_FILTER_SOURCE_SIGNATURE 0x53464353U

/** ソース領域の形式版です。 */
#define STRING_CATALOG_FILTER_SOURCE_FORMAT_VERSION 1U

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /** 命令の種類です。 */
    typedef enum string_catalog_filter_opcode
    {
        STRING_CATALOG_FILTER_OPCODE_PREDICATE = 1,     /**< 判定要素を評価して真偽を積む。 */
        STRING_CATALOG_FILTER_OPCODE_NOT = 2,           /**< 先頭の真偽を反転する。 */
        STRING_CATALOG_FILTER_OPCODE_AND = 3,           /**< 2 つの真偽を論理積で 1 つにする。 */
        STRING_CATALOG_FILTER_OPCODE_OR = 4,            /**< 2 つの真偽を論理和で 1 つにする。 */
        STRING_CATALOG_FILTER_OPCODE_JUMP_IF_FALSE = 5, /**< 先頭が偽なら、先頭を残したまま operand の命令へ進む。 */
        STRING_CATALOG_FILTER_OPCODE_JUMP_IF_TRUE = 6   /**< 先頭が真なら、先頭を残したまま operand の命令へ進む。 */
    } string_catalog_filter_opcode;

    /** 判定要素が参照するフィールドです。 */
    typedef enum string_catalog_filter_field
    {
        STRING_CATALOG_FILTER_FIELD_KEY = 1,           /**< 文字列キー。 */
        STRING_CATALOG_FILTER_FIELD_ID = 2,            /**< 項目の ID。 */
        STRING_CATALOG_FILTER_FIELD_CATEGORY = 3,      /**< 分類値。 */
        STRING_CATALOG_FILTER_FIELD_ARGUMENT_NAME = 4, /**< 名前で指定した引数 `arg.name`。 */
        STRING_CATALOG_FILTER_FIELD_ARGUMENT_INDEX = 5 /**< インデックスで指定した引数 `arg[n]`。 */
    } string_catalog_filter_field;

    /** 判定演算子です。 */
    typedef enum string_catalog_filter_operator
    {
        STRING_CATALOG_FILTER_OPERATOR_EQUAL = 1,          /**< `==` */
        STRING_CATALOG_FILTER_OPERATOR_NOT_EQUAL = 2,      /**< `!=` */
        STRING_CATALOG_FILTER_OPERATOR_LESS = 3,           /**< `<` */
        STRING_CATALOG_FILTER_OPERATOR_LESS_EQUAL = 4,     /**< `<=` */
        STRING_CATALOG_FILTER_OPERATOR_GREATER = 5,        /**< `>` */
        STRING_CATALOG_FILTER_OPERATOR_GREATER_EQUAL = 6,  /**< `>=` */
        STRING_CATALOG_FILTER_OPERATOR_IN = 7,             /**< `in [...]` */
        STRING_CATALOG_FILTER_OPERATOR_BETWEEN = 8,        /**< `between a and b` */
        STRING_CATALOG_FILTER_OPERATOR_HAS = 9,            /**< `has(...)` */
        STRING_CATALOG_FILTER_OPERATOR_STARTS_WITH = 10,   /**< `starts_with` */
        STRING_CATALOG_FILTER_OPERATOR_ENDS_WITH = 11,     /**< `ends_with` */
        STRING_CATALOG_FILTER_OPERATOR_CONTAINS = 12,      /**< `contains` */
        STRING_CATALOG_FILTER_OPERATOR_STARTS_WITH_I = 13, /**< `starts_with_i` */
        STRING_CATALOG_FILTER_OPERATOR_ENDS_WITH_I = 14,   /**< `ends_with_i` */
        STRING_CATALOG_FILTER_OPERATOR_CONTAINS_I = 15,    /**< `contains_i` */
        STRING_CATALOG_FILTER_OPERATOR_MATCHES = 16,       /**< `matches` (正規表現の部分一致) */
        STRING_CATALOG_FILTER_OPERATOR_MATCHES_I = 17      /**< `matches_i` (ASCII の大文字と小文字を区別しない) */
    } string_catalog_filter_operator;

    /** 定数の種類です。 */
    typedef enum string_catalog_filter_constant_kind
    {
        STRING_CATALOG_FILTER_CONSTANT_KIND_INTEGER = 1,      /**< 整数。値は 64 ビットの絶対値と符号の flags。 */
        STRING_CATALOG_FILTER_CONSTANT_KIND_FLOAT = 2,        /**< 倍精度浮動小数点数。 */
        STRING_CATALOG_FILTER_CONSTANT_KIND_STRING = 3,       /**< 文字列。length バイトと NUL。 */
        STRING_CATALOG_FILTER_CONSTANT_KIND_CHARACTER = 4,    /**< 文字。値は整数と同じ形式。 */
        STRING_CATALOG_FILTER_CONSTANT_KIND_NULL = 5,         /**< `null`。値を持たない。 */
        STRING_CATALOG_FILTER_CONSTANT_KIND_IDENTIFIER = 6,   /**< 文字列キーの名前。slot は行内の識別子の番号。 */
        STRING_CATALOG_FILTER_CONSTANT_KIND_ARGUMENT_NAME = 7, /**< 引数名。slot は行内の引数参照の番号。 */
        STRING_CATALOG_FILTER_CONSTANT_KIND_PATTERN = 8 /**< 正規表現のパターン。length バイトと NUL。slot は行内のパターンの番号。 */
    } string_catalog_filter_constant_kind;

    /**
     *  @brief          フィルター オブジェクトのヘッダーです。
     */
    typedef struct string_catalog_filter_image_header
    {
        uint32_t signature;       /**< @ref STRING_CATALOG_FILTER_SIGNATURE */
        uint16_t format_version;  /**< @ref STRING_CATALOG_FILTER_FORMAT_VERSION */
        uint16_t byte_order_mark; /**< @ref STRING_CATALOG_FILTER_BYTE_ORDER_MARK */
        uint32_t line_capacity;   /**< 行数の上限。 */
        uint32_t line_width;      /**< 行幅。 */
        uint32_t line_count;      /**< 格納している条件式の数。 */
        uint32_t record_size;     /**< 行レコードのバイト数。 */
        uint64_t image_size;      /**< 全体のバイト数。 */
        uint64_t content_hash;    /**< 有効な行レコードの内容から算出したハッシュ値。 */
        uint8_t reserved[24];     /**< 予約。0 を格納します。 */
    } string_catalog_filter_image_header;

    /**
     *  @brief          行レコードの見出しです。
     */
    typedef struct string_catalog_filter_record_header
    {
        uint64_t line_hash;               /**< 命令と定数の内容から算出したハッシュ値。 */
        uint16_t instruction_count;       /**< 使用している命令の数。 */
        uint16_t constant_size;           /**< 使用している定数領域のバイト数。 */
        uint8_t stack_depth;              /**< 評価に必要なスタックの深さ。 */
        uint8_t argument_reference_count; /**< 名前で参照する引数の種類の数。 */
        uint8_t identifier_count;         /**< 識別子の定数の数。 */
        uint8_t pattern_count;            /**< 正規表現のパターンの定数の数。以前の形式では予約の 0 です。 */
    } string_catalog_filter_record_header;

    /**
     *  @brief          1 つの命令です。
     *
     *  判定要素 (@ref STRING_CATALOG_FILTER_OPCODE_PREDICATE) では、@ref string_catalog_filter_instruction::operand が
     *  定数領域のオフセットです。\n
     *  @ref STRING_CATALOG_FILTER_FIELD_ARGUMENT_NAME の場合は、operand の位置に引数名の定数があり、
     *  その直後から operand_count 個の比較対象の定数が並びます。それ以外は operand の位置から並びます。\n
     *  ジャンプでは、operand が移動先の命令の番号です。
     */
    typedef struct string_catalog_filter_instruction
    {
        uint8_t opcode;         /**< @ref string_catalog_filter_opcode */
        uint8_t field;          /**< @ref string_catalog_filter_field 。判定要素以外は 0。 */
        uint8_t operator_kind;  /**< @ref string_catalog_filter_operator 。判定要素以外は 0。 */
        uint8_t argument;       /**< `arg[n]` のインデックス、または `arg.name` の引数参照の番号。 */
        uint16_t operand_count; /**< 比較対象の定数の数。 */
        uint16_t operand;       /**< 定数のオフセット、またはジャンプ先の命令の番号。 */
    } string_catalog_filter_instruction;

    /**
     *  @brief          定数の見出しです。値が直後に続きます。
     */
    typedef struct string_catalog_filter_constant_header
    {
        uint8_t kind;    /**< @ref string_catalog_filter_constant_kind */
        uint8_t flags;   /**< STRING_CATALOG_FILTER_CONSTANT_FLAG_* の組み合わせ。 */
        uint16_t slot;   /**< 識別子または引数参照の番号。それ以外は 0。 */
        uint32_t length; /**< 文字列、識別子、引数名のバイト数 (NUL を除く)。それ以外は 0。 */
    } string_catalog_filter_constant_header;

    /**
     *  @brief          定数 1 個を読み取った結果です。
     */
    typedef struct string_catalog_filter_constant
    {
        string_catalog_filter_constant_header header; /**< 見出し。 */
        uint32_t offset;                              /**< 定数領域の先頭からのオフセット。 */
        uint32_t next_offset;                         /**< 次の定数のオフセット。 */
        const char *text;                             /**< 文字列系の定数の本体。それ以外は NULL。 */
        uint64_t magnitude;                           /**< 整数と文字の絶対値。 */
        double real;                                  /**< 浮動小数点数の値。 */
    } string_catalog_filter_constant;

    /* ===== レイアウトの読み書き ===== */

    /** 行幅に対する命令領域の命令数です。 */
    uint32_t string_catalog_filter_instruction_capacity(uint32_t line_width);

    /** 行幅に対する定数領域のバイト数です。 */
    uint32_t string_catalog_filter_constant_capacity(uint32_t line_width);

    /** 行レコードの先頭アドレスを返します。境界は呼び出し側が確認済みであることを前提とします。 */
    unsigned char *string_catalog_filter_record_address(void *image, uint32_t record_size, uint32_t line_index);

    /** 行レコードの先頭アドレスを返します (読み取り専用)。 */
    const unsigned char *string_catalog_filter_record_address_const(const void *image, uint32_t record_size,
                                                                    uint32_t line_index);

    void string_catalog_filter_read_image_header(const void *image, string_catalog_filter_image_header *header_out);
    void string_catalog_filter_write_image_header(void *image, const string_catalog_filter_image_header *header);
    void string_catalog_filter_read_record_header(const unsigned char *record,
                                                  string_catalog_filter_record_header *header_out);
    void string_catalog_filter_write_record_header(unsigned char *record,
                                                   const string_catalog_filter_record_header *header);
    void string_catalog_filter_read_instruction(const unsigned char *record, uint32_t index,
                                                string_catalog_filter_instruction *instruction_out);
    void string_catalog_filter_write_instruction(unsigned char *record, uint32_t index,
                                                 const string_catalog_filter_instruction *instruction);

    /** 行レコードの定数領域の先頭アドレスを返します。 */
    const unsigned char *string_catalog_filter_record_constants(const unsigned char *record, uint32_t line_width);

    /**
     *  @brief          定数を 1 個読み取ります。
     *  @return         定数が領域内に収まり、形式が正しい場合は `CPLAT_OK`、それ以外は `CPLAT_ERR_CORRUPT_DESCRIPTOR`。
     */
    int string_catalog_filter_read_constant(const unsigned char *constants, uint32_t constant_size, uint32_t offset,
                                            string_catalog_filter_constant *constant_out);

    /**
     *  @brief          フィールド、判定演算子、定数の種類の組み合わせが成立し得るかを判定します。
     *  @return         成立し得る場合は 0 以外、成立しない場合は 0。
     *
     *  コンパイルと検証が同じ規則を使用するための関数です。
     */
    int string_catalog_filter_is_constant_allowed(uint8_t field, uint8_t operator_kind, uint8_t constant_kind);

    /* ===== ハッシュと検証 ===== */

    /** FNV-1a 64 ビットでハッシュ値を積み上げます。 */
    uint64_t string_catalog_filter_hash_bytes(uint64_t hash, const void *data, size_t size);

    /** 行レコードの使用範囲からハッシュ値を算出します。 */
    uint64_t string_catalog_filter_compute_line_hash(const unsigned char *record, uint32_t line_width);

    /** ヘッダーの行数の上限、行幅、行数と、有効な行レコードからハッシュ値を算出します。 */
    uint64_t string_catalog_filter_compute_content_hash(const void *image,
                                                        const string_catalog_filter_image_header *header);

    /**
     *  @brief          ヘッダーを読み取り、形式と大きさを確認します。行レコードの内容は確認しません。
     *  @return         `CPLAT_OK` または `CPLAT_ERR_CORRUPT_DESCRIPTOR`。
     */
    int string_catalog_filter_check_header(const void *image, size_t image_size,
                                           string_catalog_filter_image_header *header_out);

    /**
     *  @brief          行レコード 1 件の形式と、命令と定数の参照先を確認します。
     *  @return         `CPLAT_OK` または `CPLAT_ERR_CORRUPT_DESCRIPTOR`。
     */
    int string_catalog_filter_check_record(const unsigned char *record, uint32_t line_width);

    /**
     *  @brief          ヘッダーの行数と内容のハッシュ値を更新します。
     */
    void string_catalog_filter_update_content_hash(void *image, string_catalog_filter_image_header *header);

    /* ===== 命令の木と定数の表記 ===== */

    /**
     *  @brief          検証済みの行レコードの後置記法の命令列から、演算子の木を組み立てます。
     *  @param[in]      record 行レコード。@ref string_catalog_filter_check_record で検証済みであること。
     *  @param[out]     left   命令の番号をインデックスとする左の被演算子 (NOT は唯一の被演算子)。
     *                         行幅と同じ要素数が必要です。
     *  @param[out]     right  命令の番号をインデックスとする右の被演算子。行幅と同じ要素数が必要です。
     *  @return         木の根の命令の番号。
     *
     *  デコンパイルと自然文での表現が共有します。ジャンプ命令は木に現れません。
     */
    uint16_t string_catalog_filter_build_tree(const unsigned char *record, uint16_t *left, uint16_t *right);

    /**
     *  @brief          比較対象の定数を、条件式の表記で書き出します。
     *  @param[in]      constant  定数。
     *  @param[out]     dest      書き出し先。常に NUL 終端します。
     *  @param[in]      dest_size @p dest のバイト数。1 以上です。
     *  @return         `CPLAT_OK`、収まらない場合は切り詰めて `CPLAT_ERR_BUFFER_TOO_SMALL`。
     */
    int string_catalog_filter_format_constant(const string_catalog_filter_constant *constant, char *dest,
                                              size_t dest_size);

    /* ===== 自然文での表現 ===== */

    /**
     *  @brief          自然文での表現に必要な、適用済みの 1 行分の情報です。
     *
     *  フィルター スロットが適用中の面から組み立てて渡します。
     */
    typedef struct string_catalog_filter_describe_source
    {
        const cplat_string_catalog *catalog;            /**< 関連付けたカタログ。 */
        const unsigned char *record;                    /**< 検証済みの行レコード。 */
        const int64_t *identifier_values;               /**< 行の識別子の定数を解決した文字列キー。 */
        const cplat_string_catalog_entry *single_entry; /**< この行が一致し得る唯一の項目。複数の場合は NULL。 */
        const cplat_string_catalog_filter_category_names
            *category_names; /**< 分類値の名前。NULL の場合は数値で表す。 */
        uint32_t line_width; /**< 行幅。 */
        int is_japanese;     /**< 日本語の文型を使う場合は 0 以外。 */
    } string_catalog_filter_describe_source;

    /**
     *  @brief          1 行の条件式を、カタログのメタ情報を用いた自然文で書き出します。
     *  @param[in]      source    行の情報。
     *  @param[out]     dest      書き出し先。常に NUL 終端します。
     *  @param[in]      dest_size @p dest のバイト数。1 以上です。
     *  @return         `CPLAT_OK`、収まらない場合は切り詰めて `CPLAT_ERR_BUFFER_TOO_SMALL`。
     */
    int string_catalog_filter_describe_record(const string_catalog_filter_describe_source *source, char *dest,
                                              size_t dest_size);

    /* ===== コンパイル ===== */

    /**
     *  @brief          1 行の条件式を行レコードへコンパイルします。
     *  @param[in]      text         条件式の先頭。NUL 終端でなくてもかまいません。
     *  @param[in]      text_length  条件式のバイト数。
     *  @param[in]      line_width   行幅。
     *  @param[out]     record       行レコードの格納先。@ref CPLAT_STRING_CATALOG_FILTER_RECORD_SIZE バイトを 0 で埋めてから書き込みます。
     *  @param[out]     error_out    失敗の原因。
     *  @param[out]     column_out   失敗した位置。
     *  @return         成功時は `CPLAT_OK`、条件式が不正な場合は `CPLAT_ERR_MALFORMED_DEFINITION`。
     */
    int string_catalog_filter_compile_record(const char *text, size_t text_length, uint32_t line_width,
                                             unsigned char *record, cplat_string_catalog_filter_line_error *error_out,
                                             uint32_t *column_out);

    /**
     *  @brief          ソース領域のヘッダーです。
     *
     *  複数のプロセスが同じレイアウトで読み書きするため、固定幅の整数型だけで構成します。\n
     *  @ref string_catalog_filter_source_header::published_revision はアトミックに読み書きします。
     *  それ以外のメンバーとフィルター オブジェクトは、版番号を奇数にした間に書き込みます。
     */
    typedef struct string_catalog_filter_source_header
    {
        uint32_t signature;                     /**< @ref STRING_CATALOG_FILTER_SOURCE_SIGNATURE 。未公開の間は 0。 */
        uint16_t format_version;                /**< @ref STRING_CATALOG_FILTER_SOURCE_FORMAT_VERSION */
        uint16_t header_size;                   /**< @ref CPLAT_STRING_CATALOG_FILTER_SOURCE_HEADER_SIZE */
        uint32_t line_capacity;                 /**< フィルター オブジェクトの行数の上限。 */
        uint32_t line_width;                    /**< フィルター オブジェクトの行幅。 */
        uint64_t image_size;                    /**< フィルター オブジェクトのバイト数。 */
        cplat_atomic_u64 published_revision;    /**< 版番号。0 は未公開、奇数は書き込み中。 */
        int64_t published_realtime_seconds;     /**< 公開した実時刻の秒部。 */
        int64_t published_realtime_nanoseconds; /**< 公開した実時刻のナノ秒部。 */
        uint32_t publisher_process_id;          /**< 公開したプロセスの ID。 */
        uint32_t reserved;                      /**< 予約。0 を格納します。 */
        uint64_t catalog_id; /**< 公開したフィルター オブジェクトを判定に使うカタログの識別値。 */
    } string_catalog_filter_source_header;

    /**
     *  @brief          `matches` または `matches_i` のパターンをコンパイルします。
     *  @param[in]      pattern       NUL 終端のパターン。
     *  @param[in]      operator_kind @ref STRING_CATALOG_FILTER_OPERATOR_MATCHES または
     *                                @ref STRING_CATALOG_FILTER_OPERATOR_MATCHES_I 。
     *  @param[out]     regex_out     コンパイルしたハンドルの格納先。`cplat_regex_dispose` で破棄します。
     *  @return         `cplat_regex_create` と同じ結果コードを返します。
     *
     *  方言は ECMAScript で、捕捉グループを記録しません。`matches_i` は ASCII の大文字と小文字を区別しません。\n
     *  コンパイル (構文の確認) と適用の双方が使用し、同じ規則でパターンを解釈します。
     */
    int string_catalog_filter_create_pattern(const char *pattern, uint8_t operator_kind, cplat_regex **regex_out);

    /**
     *  @brief          ソース領域の先頭アドレスと大きさを確認します。
     *  @param[in]      source       ソース領域の先頭アドレス。
     *  @param[in]      source_size  @p source のバイト数。
     *  @param[in]      image_size   ヘッダーに続けて格納するフィルター オブジェクトのバイト数。
     *  @return         NULL でなく、アラインメントが合い、大きさが足りる場合は true。
     */
    bool string_catalog_filter_source_is_region_valid(const void *source, size_t source_size, size_t image_size);

    /**
     *  @brief          ソース領域のヘッダーが、本ライブラリの形式で公開されたものかを確かめます。
     *  @param[in]      header 確かめるヘッダー。書き込みと重ならないよう、複製したヘッダーを渡します。
     *  @return         署名、形式版、ヘッダー長が本ライブラリと一致する場合は `CPLAT_OK` を返します。
     *  @return         署名が一致し、形式版が異なる場合は `CPLAT_ERR_VERSION_MISMATCH` を返します。
     *  @return         署名またはヘッダー長が異なる場合は `CPLAT_ERR_CORRUPT_DESCRIPTOR` を返します。
     *
     *  ファイルをマップした領域は、異なる版のライブラリが書いた内容を残している場合があります。
     *  形式の異なる領域を、本ライブラリの配置で読まないために確かめます。
     */
    int string_catalog_filter_source_check_header(const string_catalog_filter_source_header *header);

    /**
     *  @brief          ソース領域の読み取りを始め、版番号を返します。
     *  @param[in]      source ソース領域の先頭アドレス。
     *  @return         版番号。0 は未公開、奇数は書き込み中です。
     *
     *  版番号が偶数の場合、この後に読んだ内容は @ref string_catalog_filter_source_end_read で確定します。
     */
    uint64_t string_catalog_filter_source_begin_read(const void *source);

    /**
     *  @brief          ソース領域の読み取りを終え、読んだ内容が一貫しているかを返します。
     *  @param[in]      source    ソース領域の先頭アドレス。
     *  @param[in]      revision @ref string_catalog_filter_source_begin_read が返した版番号。
     *  @return         読み取りの間に版番号が変わっていない場合は true。
     */
    bool string_catalog_filter_source_end_read(const void *source, uint64_t revision);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FILTER_PRIVATE_H */
