/**
 *******************************************************************************
 *  @file           filter_slot.c
 *  @brief          フィルター スロット (適用、事前計算、判定、差し替え) を実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/09/26
 *  @version        0.1.0
 *
 *  スロットは、判定が参照する内容を 2 面 (plane) で持ちます。\n
 *  適用は使用していない面へ複製、名前解決、事前計算を行い、排他モードのロックの下で参照する面を切り替えます。\n
 *  判定は共有モードのロックの下で、参照中の面だけを読みます。
 *
 *  面を書き換えるのは適用だけであり、適用は専用のロックで 1 つずつ処理します。\n
 *  そのため、適用は参照中の面をロックなしで読み、未変更の行の結果を再利用できます。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *******************************************************************************
 */

#include "filter.h"

#include <cplat/base/result.h>
#include <cplat/string_catalog/format_internal.h>
#include <cplat/sync/sync.h>

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/** 3 値の論理値です。事前計算では、引数の値に依存する判定要素を不定とします。 */
typedef enum truth_value
{
    TRUTH_VALUE_FALSE = 0,
    TRUTH_VALUE_TRUE = 1,
    TRUTH_VALUE_UNKNOWN = 2
} truth_value;

/** 行の集合を表す 1 語のビット数です。 */
#define LINE_WORD_BITS 64U

/** 数値の比較結果のうち、NaN を含むため順序が定まらないことを表す値です。 */
#define ORDER_UNORDERED 2

/** 判定が参照する内容の 1 面分です。 */
typedef struct filter_plane
{
    unsigned char *image;                           /**< フィルター オブジェクトの複製。 */
    uint8_t *entry_states;                          /**< [項目] の cplat_string_catalog_filter_state。 */
    uint64_t *entry_dependent_lines;                /**< [項目][語] の、引数値に依存する行の集合 (ビット i が行 i)。 */
    uint8_t *line_states;                           /**< [行][項目] の truth_value。 */
    int8_t *argument_maps;                          /**< [行][項目][引数参照] の引数インデックス。未定義は -1。 */
    int64_t *identifier_values;                     /**< [行][識別子] の文字列キー。 */
    cplat_string_catalog_filter_line_error *line_errors; /**< [行] の無効にした原因。 */
    cplat_regex **patterns; /**< [行][パターン] のコンパイルした正規表現。面の構築のたびに作り直します。 */
    uint32_t line_count;                            /**< 格納している条件式の数。 */
    uint32_t pad;                                   /**< 明示的アラインメントです。 */
} filter_plane;

struct cplat_string_catalog_filter_slot
{
    const cplat_string_catalog *catalog;
    const cplat_string_catalog_filter_key_name *key_names;
    size_t key_name_count;
    size_t entry_count;
    size_t line_word_count; /**< 行の集合を表す 64 ビットの語の数。行数の上限から求めます。 */
    uint64_t catalog_id;    /**< カタログの識別値。ソース領域の公開内容がこのカタログ向けかを確かめます。 */
    size_t image_size;
    uint32_t line_capacity;
    uint32_t line_width;
    uint32_t record_size;
    int active_plane; /**< 判定が参照する面。plane_lock の下で読み書きします。 */
    const cplat_string_catalog_filter_category_names
        *category_names; /**< 自然文での表現に使う分類値の名前。未設定は NULL。 */
    cplat_local_rwlock *plane_lock;
    cplat_local_lock *apply_lock;
    filter_plane planes[2];
    const void *source;      /**< 結び付けたソース領域。未設定は NULL。 */
    size_t source_size;      /**< @ref cplat_string_catalog_filter_slot::source のバイト数。 */
    cplat_string_catalog_filter_source_lock source_lock; /**< 書き込み側の排他。未設定は lock が NULL。 */
    cplat_atomic_u64 taken_revision;  /**< 取り込みを試みた版番号。判定のたびにロックなしで読みます。 */
    size_t source_last_invalid_count; /**< 直近の取り込みで無効にした行の数。apply_lock の下で読み書きします。 */
    int source_last_result;           /**< 直近の取り込みの結果コード。apply_lock の下で読み書きします。 */
    unsigned int pad;                 /**< 明示的アラインメントです。 */
};

/** 判定に使用する引数の値です。比較の区分ごとに正規化して保持します。 */
typedef struct argument_value
{
    cplat_string_catalog_argument_kind kind;
    unsigned int pad;
    union argument_storage
    {
        const char *string_value;
        int64_t signed_value;
        uint64_t unsigned_value;
        double real_value;
    } value;
} argument_value;

/** 比較に使用する数値です。整数は符号と絶対値で表し、数学的な大小で比較します。 */
typedef struct number
{
    uint64_t magnitude;
    double real;
    bool is_real;
    bool is_negative;
    uint8_t pad[6]; /**< 明示的アラインメントです。 */
} number;

/** 引数の比較上の区分です。 */
typedef enum argument_class
{
    ARGUMENT_CLASS_NONE = 0,
    ARGUMENT_CLASS_SIGNED = 1,
    ARGUMENT_CLASS_UNSIGNED = 2,
    ARGUMENT_CLASS_REAL = 3,
    ARGUMENT_CLASS_STRING = 4,
    ARGUMENT_CLASS_POINTER = 5
} argument_class;

/** 1 行を 1 項目について評価するための情報です。 */
typedef struct evaluation_context
{
    const cplat_string_catalog_entry *entry;
    const unsigned char *record;
    const unsigned char *constants;
    uint32_t constant_size;
    uint32_t pad;
    const int8_t *argument_map;       /**< [引数参照] */
    const int64_t *identifier_values; /**< [識別子] */
    cplat_regex *const *patterns;     /**< [パターン] */
    const argument_value *values;     /**< 引数の値。事前計算では NULL。 */
} evaluation_context;

/* ===== 引数と数値 ===== */

static argument_class class_of_argument(const cplat_string_catalog_argument_kind kind)
{
    switch (kind)
    {
    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_CHAR:
    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_INT8:
    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_INT16:
    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_INT32:
    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_INT64:
    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_SSIZE:
    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_ERROR_CODE:
        return ARGUMENT_CLASS_SIGNED;
    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_UINT8:
    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_UINT16:
    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_UINT32:
    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_UINT64:
    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_HEX8:
    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_HEX16:
    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_HEX32:
    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_HEX64:
    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_SIZE:
        return ARGUMENT_CLASS_UNSIGNED;
    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_DOUBLE:
        return ARGUMENT_CLASS_REAL;
    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_STRING:
        return ARGUMENT_CLASS_STRING;
    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_POINTER:
        return ARGUMENT_CLASS_POINTER;
    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_UNUSED:
    default:
        return ARGUMENT_CLASS_NONE;
    }
}

/**
 *  @brief          文字列カタログが取り出した値を、比較の区分ごとに正規化します。
 *  @param[in]      entry  カタログの 1 件。
 *  @param[in]      shared 文字列カタログが取り出した値の配列。
 *  @param[out]     values 正規化した値の格納先。
 *  @return         すべての値を正規化できた場合は true、未知の種別があった場合は false。
 *
 *  可変長引数の取り出しは cplat_internal_string_catalog_prepare_format() が行い、判定と書式展開で同じ値を使用します。
 */
static bool normalize_arguments(const cplat_string_catalog_entry *entry,
                                const cplat_internal_string_catalog_argument_value *shared, argument_value *values)
{
    for (int index = 0; index < entry->argument_count; index++)
    {
        const cplat_string_catalog_argument_kind kind = entry->arguments[index].kind;
        const union cplat_internal_string_catalog_argument_storage *source = &shared[index].value;
        argument_value *value = &values[index];

        value->kind = kind;
        value->pad = 0U;
        value->value.unsigned_value = 0U;

        switch (kind)
        {
        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_UNUSED:
            break;
        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_STRING:
            value->value.string_value = source->string_value;
            break;
        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_CHAR:
            value->value.signed_value = source->char_value;
            break;
        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_INT8:
            value->value.signed_value = source->int8_value;
            break;
        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_UINT8:
        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_HEX8:
            value->value.unsigned_value = source->uint8_value;
            break;
        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_INT16:
            value->value.signed_value = source->int16_value;
            break;
        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_UINT16:
        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_HEX16:
            value->value.unsigned_value = source->uint16_value;
            break;
        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_INT32:
            value->value.signed_value = source->int32_value;
            break;
        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_UINT32:
        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_HEX32:
            value->value.unsigned_value = source->uint32_value;
            break;
        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_INT64:
        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_SSIZE:
            value->value.signed_value = source->int64_value;
            break;
        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_UINT64:
        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_HEX64:
            value->value.unsigned_value = source->uint64_value;
            break;
        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_SIZE:
            value->value.unsigned_value = source->size_value;
            break;
        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_POINTER:
            value->value.unsigned_value = (uint64_t)(uintptr_t)source->pointer_value;
            break;
        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_DOUBLE:
            value->value.real_value = source->double_value;
            break;
        case CPLAT_STRING_CATALOG_ARGUMENT_KIND_ERROR_CODE:
            value->value.signed_value = source->error_code_value;
            break;
        default:
            return false;
        }
    }
    return true;
}

static number number_from_signed(const int64_t value)
{
    number result;

    memset(&result, 0, sizeof(result));
    if (value < 0)
    {
        result.is_negative = true;
        /* INT64_MIN でも回り込まないよう、1 を足してから符号を反転する */
        result.magnitude = (uint64_t)(-(value + 1)) + 1U;
    }
    else
    {
        result.magnitude = (uint64_t)value;
    }
    return result;
}

static number number_from_unsigned(const uint64_t value)
{
    number result;

    memset(&result, 0, sizeof(result));
    result.magnitude = value;
    return result;
}

static number number_from_real(const double value)
{
    number result;

    memset(&result, 0, sizeof(result));
    result.is_real = true;
    result.real = value;
    return result;
}

static double real_of_number(const number *value)
{
    if (value->is_real)
    {
        return value->real;
    }
    if (value->is_negative)
    {
        return -(double)value->magnitude;
    }
    return (double)value->magnitude;
}

/**
 *  @brief          2 つの数値を数学的な大小で比較します。
 *  @return         -1、0、1、または NaN を含む場合は ORDER_UNORDERED。
 */
static int compare_numbers(const number *left, const number *right)
{
    if (left->is_real || right->is_real)
    {
        const double left_real = real_of_number(left);
        const double right_real = real_of_number(right);

        if (isnan(left_real) || isnan(right_real))
        {
            return ORDER_UNORDERED;
        }
        if (left_real < right_real)
        {
            return -1;
        }
        if (left_real > right_real)
        {
            return 1;
        }
        return 0;
    }

    if (left->is_negative != right->is_negative)
    {
        if (left->is_negative)
        {
            return -1;
        }
        return 1;
    }
    if (left->magnitude == right->magnitude)
    {
        return 0;
    }
    /* 負どうしでは、絶対値の大きい方が小さい */
    if ((left->magnitude < right->magnitude) != left->is_negative)
    {
        return -1;
    }
    return 1;
}

static bool is_order_accepted(const uint8_t operator_kind, const int order)
{
    switch (operator_kind)
    {
    case STRING_CATALOG_FILTER_OPERATOR_EQUAL:
    case STRING_CATALOG_FILTER_OPERATOR_IN:
        return order == 0;
    case STRING_CATALOG_FILTER_OPERATOR_NOT_EQUAL:
        return order != 0;
    case STRING_CATALOG_FILTER_OPERATOR_LESS:
        return order == -1;
    case STRING_CATALOG_FILTER_OPERATOR_LESS_EQUAL:
        return (order == -1) || (order == 0);
    case STRING_CATALOG_FILTER_OPERATOR_GREATER:
        return order == 1;
    case STRING_CATALOG_FILTER_OPERATOR_GREATER_EQUAL:
        return (order == 1) || (order == 0);
    default:
        return false;
    }
}

/** 数値の定数を比較用の数値へ変換します。 */
static number number_from_constant(const evaluation_context *context, const string_catalog_filter_constant *constant)
{
    switch (constant->header.kind)
    {
    case STRING_CATALOG_FILTER_CONSTANT_KIND_FLOAT:
        return number_from_real(constant->real);
    case STRING_CATALOG_FILTER_CONSTANT_KIND_IDENTIFIER:
        return number_from_signed(context->identifier_values[constant->header.slot]);
    default:
    {
        number result = number_from_unsigned(constant->magnitude);
        result.is_negative = ((constant->header.flags & STRING_CATALOG_FILTER_CONSTANT_FLAG_NEGATIVE) != 0U);
        return result;
    }
    }
}

/* ===== 文字列の比較 ===== */

static unsigned char to_lower_ascii(const unsigned char character)
{
    if ((character >= (unsigned char)'A') && (character <= (unsigned char)'Z'))
    {
        return (unsigned char)(character + ('a' - 'A'));
    }
    return character;
}

static bool equals_bytes(const char *left, const char *right, const size_t length, const bool ignores_case)
{
    if (!ignores_case)
    {
        return memcmp(left, right, length) == 0;
    }
    for (size_t index = 0; index < length; index++)
    {
        if (to_lower_ascii((unsigned char)left[index]) != to_lower_ascii((unsigned char)right[index]))
        {
            return false;
        }
    }
    return true;
}

static bool match_string(const uint8_t operator_kind, const char *subject, const char *pattern,
                         const size_t pattern_length)
{
    const size_t subject_length = strlen(subject);
    const bool ignores_case = (operator_kind >= (uint8_t)STRING_CATALOG_FILTER_OPERATOR_STARTS_WITH_I);

    switch (operator_kind)
    {
    case STRING_CATALOG_FILTER_OPERATOR_EQUAL:
    case STRING_CATALOG_FILTER_OPERATOR_IN:
        return (subject_length == pattern_length) && equals_bytes(subject, pattern, pattern_length, false);
    case STRING_CATALOG_FILTER_OPERATOR_NOT_EQUAL:
        return (subject_length != pattern_length) || !equals_bytes(subject, pattern, pattern_length, false);
    case STRING_CATALOG_FILTER_OPERATOR_STARTS_WITH:
    case STRING_CATALOG_FILTER_OPERATOR_STARTS_WITH_I:
        return (subject_length >= pattern_length) && equals_bytes(subject, pattern, pattern_length, ignores_case);
    case STRING_CATALOG_FILTER_OPERATOR_ENDS_WITH:
    case STRING_CATALOG_FILTER_OPERATOR_ENDS_WITH_I:
        return (subject_length >= pattern_length) &&
               equals_bytes(subject + (subject_length - pattern_length), pattern, pattern_length, ignores_case);
    case STRING_CATALOG_FILTER_OPERATOR_CONTAINS:
    case STRING_CATALOG_FILTER_OPERATOR_CONTAINS_I:
        if (pattern_length > subject_length)
        {
            return false;
        }
        for (size_t start = 0; start <= (subject_length - pattern_length); start++)
        {
            if (equals_bytes(subject + start, pattern, pattern_length, ignores_case))
            {
                return true;
            }
        }
        return false;
    default:
        return false;
    }
}

/**
 *  @brief          文字列の一部が正規表現に一致するかを照合します。
 *
 *  @ref CPLAT_STRING_CATALOG_FILTER_PATTERN_SUBJECT_MAX を超える文字列は照合せず、不一致とします。
 *  バックトラッキングの照合が、長い文字列で時間やスタックを消費しないようにするためです。\n
 *  照合に失敗した場合 (不正な UTF-8、メモリ不足など) も不一致とします。判定の失敗でトレースの出力を止めないためです。
 */
static bool match_pattern(const cplat_regex *regex, const char *subject)
{
    const size_t subject_length = strlen(subject);
    int is_found = 0;

    if ((regex == NULL) || (subject_length > CPLAT_STRING_CATALOG_FILTER_PATTERN_SUBJECT_MAX))
    {
        return false;
    }
    if (cplat_regex_search(regex, subject, subject_length, 0U, CPLAT_REGEX_MATCH_DEFAULT, NULL, 0U, &is_found, NULL) !=
        CPLAT_OK)
    {
        return false;
    }
    return is_found != 0;
}

/* ===== 判定要素と命令列の評価 ===== */

/** 文字列を対象とする判定要素を評価します。対象は NULL の場合があります。 */
static truth_value evaluate_string(const evaluation_context *context,
                                   const string_catalog_filter_instruction *instruction, uint32_t offset,
                                   const char *subject)
{
    string_catalog_filter_constant constant;

    for (uint16_t index = 0; index < instruction->operand_count; index++)
    {
        (void)string_catalog_filter_read_constant(context->constants, context->constant_size, offset, &constant);
        offset = constant.next_offset;

        if (constant.header.kind == (uint8_t)STRING_CATALOG_FILTER_CONSTANT_KIND_NULL)
        {
            /* null との比較は == と != だけが成立し得る */
            if ((subject == NULL) == (instruction->operator_kind == (uint8_t)STRING_CATALOG_FILTER_OPERATOR_EQUAL))
            {
                return TRUTH_VALUE_TRUE;
            }
            return TRUTH_VALUE_FALSE;
        }

        if (subject == NULL)
        {
            /* NULL は、どの文字列とも等しくない。文字列の判定演算はすべて偽とする */
            if (instruction->operator_kind == (uint8_t)STRING_CATALOG_FILTER_OPERATOR_NOT_EQUAL)
            {
                return TRUTH_VALUE_TRUE;
            }
            return TRUTH_VALUE_FALSE;
        }

        if (constant.header.kind == (uint8_t)STRING_CATALOG_FILTER_CONSTANT_KIND_PATTERN)
        {
            if (match_pattern(context->patterns[constant.header.slot], subject))
            {
                return TRUTH_VALUE_TRUE;
            }
        }
        else if (match_string(instruction->operator_kind, subject, constant.text, constant.header.length))
        {
            return TRUTH_VALUE_TRUE;
        }
    }
    return TRUTH_VALUE_FALSE;
}

/** 数値を対象とする判定要素を評価します。 */
static truth_value evaluate_number(const evaluation_context *context,
                                   const string_catalog_filter_instruction *instruction, uint32_t offset,
                                   const number *subject)
{
    string_catalog_filter_constant constant;

    if (instruction->operator_kind == (uint8_t)STRING_CATALOG_FILTER_OPERATOR_BETWEEN)
    {
        number lower;
        number upper;
        int lower_order;
        int upper_order;

        (void)string_catalog_filter_read_constant(context->constants, context->constant_size, offset, &constant);
        lower = number_from_constant(context, &constant);
        (void)string_catalog_filter_read_constant(context->constants, context->constant_size, constant.next_offset,
                                                  &constant);
        upper = number_from_constant(context, &constant);

        lower_order = compare_numbers(subject, &lower);
        upper_order = compare_numbers(subject, &upper);
        if (((lower_order == 0) || (lower_order == 1)) && ((upper_order == 0) || (upper_order == -1)))
        {
            return TRUTH_VALUE_TRUE;
        }
        return TRUTH_VALUE_FALSE;
    }

    for (uint16_t index = 0; index < instruction->operand_count; index++)
    {
        number literal;

        (void)string_catalog_filter_read_constant(context->constants, context->constant_size, offset, &constant);
        offset = constant.next_offset;
        literal = number_from_constant(context, &constant);

        if (is_order_accepted(instruction->operator_kind, compare_numbers(subject, &literal)))
        {
            return TRUTH_VALUE_TRUE;
        }
    }
    return TRUTH_VALUE_FALSE;
}

/**
 *  @brief          引数の型区分と、比較対象の定数の種類が組み合わせとして成立するかを判定します。
 *
 *  成立しない組み合わせは、その項目では偽とします (設計資料「引数のデータ型と比較規則」)。
 */
static bool is_argument_comparable(const argument_class kind_class, const uint8_t constant_kind)
{
    switch (constant_kind)
    {
    case STRING_CATALOG_FILTER_CONSTANT_KIND_STRING:
    case STRING_CATALOG_FILTER_CONSTANT_KIND_PATTERN:
        return kind_class == ARGUMENT_CLASS_STRING;
    case STRING_CATALOG_FILTER_CONSTANT_KIND_NULL:
        return (kind_class == ARGUMENT_CLASS_STRING) || (kind_class == ARGUMENT_CLASS_POINTER);
    default:
        return kind_class != ARGUMENT_CLASS_STRING;
    }
}

static truth_value evaluate_predicate(const evaluation_context *context,
                                      const string_catalog_filter_instruction *instruction)
{
    const cplat_string_catalog_entry *entry = context->entry;
    string_catalog_filter_constant constant;
    uint32_t offset = instruction->operand;
    int argument_index = -1;
    argument_class kind_class;

    switch (instruction->field)
    {
    case STRING_CATALOG_FILTER_FIELD_KEY:
    {
        const number subject = number_from_signed(entry->key);
        return evaluate_number(context, instruction, offset, &subject);
    }
    case STRING_CATALOG_FILTER_FIELD_CATEGORY:
    {
        const number subject = number_from_signed(entry->category);
        return evaluate_number(context, instruction, offset, &subject);
    }
    case STRING_CATALOG_FILTER_FIELD_ID:
        return evaluate_string(context, instruction, offset, entry->id);

    case STRING_CATALOG_FILTER_FIELD_ARGUMENT_NAME:
        argument_index = context->argument_map[instruction->argument];
        /* 比較対象は引数名の定数の直後から並ぶ */
        (void)string_catalog_filter_read_constant(context->constants, context->constant_size, offset, &constant);
        offset = constant.next_offset;
        break;

    case STRING_CATALOG_FILTER_FIELD_ARGUMENT_INDEX:
        if (((int)instruction->argument < entry->argument_count) &&
            (entry->arguments[instruction->argument].kind != CPLAT_STRING_CATALOG_ARGUMENT_KIND_UNUSED))
        {
            argument_index = instruction->argument;
        }
        break;

    default:
        return TRUTH_VALUE_FALSE;
    }

    if (instruction->operator_kind == (uint8_t)STRING_CATALOG_FILTER_OPERATOR_HAS)
    {
        if (argument_index >= 0)
        {
            return TRUTH_VALUE_TRUE;
        }
        return TRUTH_VALUE_FALSE;
    }

    /* 項目に対応する引数がない場合、その引数を参照する比較は偽 */
    if (argument_index < 0)
    {
        return TRUTH_VALUE_FALSE;
    }

    kind_class = class_of_argument(entry->arguments[argument_index].kind);
    (void)string_catalog_filter_read_constant(context->constants, context->constant_size, offset, &constant);
    if (!is_argument_comparable(kind_class, constant.header.kind))
    {
        return TRUTH_VALUE_FALSE;
    }

    /* 事前計算では、引数の値に依存する判定要素を不定とする */
    if (context->values == NULL)
    {
        return TRUTH_VALUE_UNKNOWN;
    }

    switch (kind_class)
    {
    case ARGUMENT_CLASS_STRING:
        return evaluate_string(context, instruction, offset, context->values[argument_index].value.string_value);

    case ARGUMENT_CLASS_POINTER:
        if (constant.header.kind == (uint8_t)STRING_CATALOG_FILTER_CONSTANT_KIND_NULL)
        {
            const bool is_null = (context->values[argument_index].value.unsigned_value == 0U);

            if (is_null == (instruction->operator_kind == (uint8_t)STRING_CATALOG_FILTER_OPERATOR_EQUAL))
            {
                return TRUTH_VALUE_TRUE;
            }
            return TRUTH_VALUE_FALSE;
        }
        {
            const number subject = number_from_unsigned(context->values[argument_index].value.unsigned_value);
            return evaluate_number(context, instruction, offset, &subject);
        }

    case ARGUMENT_CLASS_SIGNED:
    {
        const number subject = number_from_signed(context->values[argument_index].value.signed_value);
        return evaluate_number(context, instruction, offset, &subject);
    }
    case ARGUMENT_CLASS_UNSIGNED:
    {
        const number subject = number_from_unsigned(context->values[argument_index].value.unsigned_value);
        return evaluate_number(context, instruction, offset, &subject);
    }
    case ARGUMENT_CLASS_REAL:
    {
        const number subject = number_from_real(context->values[argument_index].value.real_value);
        return evaluate_number(context, instruction, offset, &subject);
    }
    case ARGUMENT_CLASS_NONE:
    default:
        return TRUTH_VALUE_FALSE;
    }
}

static truth_value truth_not(const truth_value value)
{
    if (value == TRUTH_VALUE_TRUE)
    {
        return TRUTH_VALUE_FALSE;
    }
    if (value == TRUTH_VALUE_FALSE)
    {
        return TRUTH_VALUE_TRUE;
    }
    return TRUTH_VALUE_UNKNOWN;
}

static truth_value truth_and(const truth_value left, const truth_value right)
{
    if ((left == TRUTH_VALUE_FALSE) || (right == TRUTH_VALUE_FALSE))
    {
        return TRUTH_VALUE_FALSE;
    }
    if ((left == TRUTH_VALUE_TRUE) && (right == TRUTH_VALUE_TRUE))
    {
        return TRUTH_VALUE_TRUE;
    }
    return TRUTH_VALUE_UNKNOWN;
}

static truth_value truth_or(const truth_value left, const truth_value right)
{
    if ((left == TRUTH_VALUE_TRUE) || (right == TRUTH_VALUE_TRUE))
    {
        return TRUTH_VALUE_TRUE;
    }
    if ((left == TRUTH_VALUE_FALSE) && (right == TRUTH_VALUE_FALSE))
    {
        return TRUTH_VALUE_FALSE;
    }
    return TRUTH_VALUE_UNKNOWN;
}

/**
 *  @brief          1 行の命令列を評価します。
 *  @param[in]      context      評価する行と文字列カタログ項目。
 *  @param[in]      honors_jumps 短絡評価のジャンプに従う場合は true。
 *                               事前計算では false とし、結合の命令で 3 値の論理演算を行います。
 *
 *  命令列は適用の時点で検証済みです。スタックの深さの確認は、検証の漏れに備えた防御です。
 */
static truth_value evaluate_line(const evaluation_context *context, const bool honors_jumps)
{
    string_catalog_filter_record_header header;
    string_catalog_filter_instruction instruction;
    uint8_t stack[CPLAT_STRING_CATALOG_FILTER_LINE_WIDTH_MAX];
    uint32_t depth = 0U;
    uint32_t index = 0U;

    string_catalog_filter_read_record_header(context->record, &header);

    while (index < header.instruction_count)
    {
        string_catalog_filter_read_instruction(context->record, index, &instruction);
        index++;

        switch (instruction.opcode)
        {
        case STRING_CATALOG_FILTER_OPCODE_PREDICATE:
            stack[depth++] = (uint8_t)evaluate_predicate(context, &instruction);
            break;
        case STRING_CATALOG_FILTER_OPCODE_NOT:
            stack[depth - 1U] = (uint8_t)truth_not((truth_value)stack[depth - 1U]);
            break;
        case STRING_CATALOG_FILTER_OPCODE_AND:
            stack[depth - 2U] = (uint8_t)truth_and((truth_value)stack[depth - 2U], (truth_value)stack[depth - 1U]);
            depth--;
            break;
        case STRING_CATALOG_FILTER_OPCODE_OR:
            stack[depth - 2U] = (uint8_t)truth_or((truth_value)stack[depth - 2U], (truth_value)stack[depth - 1U]);
            depth--;
            break;
        case STRING_CATALOG_FILTER_OPCODE_JUMP_IF_FALSE:
            if (honors_jumps && (stack[depth - 1U] == (uint8_t)TRUTH_VALUE_FALSE))
            {
                index = instruction.operand;
            }
            break;
        case STRING_CATALOG_FILTER_OPCODE_JUMP_IF_TRUE:
            if (honors_jumps && (stack[depth - 1U] == (uint8_t)TRUTH_VALUE_TRUE))
            {
                index = instruction.operand;
            }
            break;
        default:
            return TRUTH_VALUE_FALSE;
        }
    }

    if (depth != 1U)
    {
        return TRUTH_VALUE_FALSE;
    }
    return (truth_value)stack[0];
}

/* ===== 面の管理 ===== */

static uint8_t *line_states_of(const cplat_string_catalog_filter_slot *slot, filter_plane *plane,
                               const uint32_t line_index)
{
    return plane->line_states + ((size_t)line_index * slot->entry_count);
}

static int8_t *argument_maps_of(const cplat_string_catalog_filter_slot *slot, filter_plane *plane,
                                const uint32_t line_index)
{
    return plane->argument_maps +
           ((size_t)line_index * slot->entry_count * CPLAT_STRING_CATALOG_FILTER_ARGUMENT_REFERENCE_MAX);
}

static int64_t *identifier_values_of(filter_plane *plane, const uint32_t line_index)
{
    return plane->identifier_values + ((size_t)line_index * CPLAT_STRING_CATALOG_FILTER_IDENTIFIER_REFERENCE_MAX);
}

static uint64_t *dependent_lines_of(const cplat_string_catalog_filter_slot *slot, const filter_plane *plane,
                                    const size_t entry_index)
{
    return plane->entry_dependent_lines + (entry_index * slot->line_word_count);
}

static bool is_line_enabled(const filter_plane *plane, const uint32_t line_index)
{
    return plane->line_errors[line_index] == CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE;
}

static cplat_regex **patterns_of(filter_plane *plane, const uint32_t line_index)
{
    return plane->patterns + ((size_t)line_index * CPLAT_STRING_CATALOG_FILTER_PATTERN_REFERENCE_MAX);
}

/** 面が保持する正規表現をすべて破棄します。 */
static void dispose_patterns(const cplat_string_catalog_filter_slot *slot, filter_plane *plane)
{
    const size_t count = (size_t)slot->line_capacity * CPLAT_STRING_CATALOG_FILTER_PATTERN_REFERENCE_MAX;

    if (plane->patterns == NULL)
    {
        return;
    }
    for (size_t index = 0; index < count; index++)
    {
        if (plane->patterns[index] != NULL)
        {
            cplat_regex_dispose(plane->patterns[index]);
            plane->patterns[index] = NULL;
        }
    }
}

static void free_plane(const cplat_string_catalog_filter_slot *slot, filter_plane *plane)
{
    dispose_patterns(slot, plane);
    free(plane->patterns);
    free(plane->image);
    free(plane->entry_states);
    free(plane->entry_dependent_lines);
    free(plane->line_states);
    free(plane->argument_maps);
    free(plane->identifier_values);
    free(plane->line_errors);
    memset(plane, 0, sizeof(*plane));
}

static bool allocate_plane(const cplat_string_catalog_filter_slot *slot, filter_plane *plane)
{
    const size_t line_entries = (size_t)slot->line_capacity * slot->entry_count;

    memset(plane, 0, sizeof(*plane));
    plane->image = (unsigned char *)malloc(slot->image_size);
    /* 項目数が 0 のカタログでも確保に失敗と区別できるよう、要素数は 1 以上とする */
    plane->entry_states = (uint8_t *)calloc(slot->entry_count + 1U, sizeof(*plane->entry_states));
    plane->entry_dependent_lines =
        (uint64_t *)calloc((slot->entry_count + 1U) * slot->line_word_count, sizeof(*plane->entry_dependent_lines));
    plane->line_states = (uint8_t *)calloc(line_entries + 1U, sizeof(*plane->line_states));
    plane->argument_maps = (int8_t *)calloc((line_entries * CPLAT_STRING_CATALOG_FILTER_ARGUMENT_REFERENCE_MAX) + 1U,
                                            sizeof(*plane->argument_maps));
    plane->identifier_values =
        (int64_t *)calloc(((size_t)slot->line_capacity * CPLAT_STRING_CATALOG_FILTER_IDENTIFIER_REFERENCE_MAX),
                          sizeof(*plane->identifier_values));
    plane->line_errors =
        (cplat_string_catalog_filter_line_error *)calloc(slot->line_capacity, sizeof(*plane->line_errors));
    plane->patterns = (cplat_regex **)calloc((size_t)slot->line_capacity * CPLAT_STRING_CATALOG_FILTER_PATTERN_REFERENCE_MAX,
                                             sizeof(*plane->patterns));

    if ((plane->image == NULL) || (plane->entry_states == NULL) || (plane->entry_dependent_lines == NULL) ||
        (plane->line_states == NULL) || (plane->argument_maps == NULL) || (plane->identifier_values == NULL) ||
        (plane->line_errors == NULL) || (plane->patterns == NULL))
    {
        free_plane(slot, plane);
        return false;
    }
    return true;
}

/**
 *  @brief          名前解決テーブルがカタログと整合することを確認します。
 *  @param[in]      catalog        カタログ。
 *  @param[in]      key_names      名前解決テーブル。@p key_name_count が 0 の場合は NULL を指定できます。
 *  @param[in]      key_name_count @p key_names の要素数。
 *  @return         すべての名前が NULL でなく、名前が重複せず、文字列キーがカタログに存在する場合は true を返します。
 *
 *  同じ名前が複数あると、どちらの値へ解決するかが決まらないため拒否します。\n
 *  異なる名前が同じ文字列キーを指すことは許可します。
 */
static bool is_key_name_table_valid(const cplat_string_catalog *catalog,
                                    const cplat_string_catalog_filter_key_name *key_names, size_t key_name_count)
{
    for (size_t index = 0; index < key_name_count; index++)
    {
        if ((key_names[index].name == NULL) || (cplat_string_catalog_get_entry(catalog, key_names[index].key) == NULL))
        {
            return false;
        }
        for (size_t other = 0; other < index; other++)
        {
            if (strcmp(key_names[index].name, key_names[other].name) == 0)
            {
                return false;
            }
        }
    }
    return true;
}

/**
 *  @brief          分類値の名前の設定が正しいことを確認します。
 *  @param[in]      category_names 分類値の名前。NULL を指定できます。
 *  @return         NULL、またはすべての項目が設定されている場合は true を返します。
 */
static bool is_category_names_valid(const cplat_string_catalog_filter_category_names *category_names)
{
    if (category_names == NULL)
    {
        return true;
    }
    if ((category_names->names == NULL) || (category_names->count == 0U) ||
        (category_names->subject_japanese == NULL) || (category_names->subject_neutral == NULL))
    {
        return false;
    }
    for (size_t index = 0; index < category_names->count; index++)
    {
        if (category_names->names[index] == NULL)
        {
            return false;
        }
    }
    return true;
}

static const cplat_string_catalog_filter_key_name *find_key_name(const cplat_string_catalog_filter_slot *slot,
                                                                 const char *name)
{
    for (size_t index = 0; index < slot->key_name_count; index++)
    {
        if (strcmp(slot->key_names[index].name, name) == 0)
        {
            return &slot->key_names[index];
        }
    }
    return NULL;
}

/**
 *  @brief          分類値と比較する定数 1 個を、分類値の名前の範囲で確かめ、識別子なら名前で解決します。
 *  @return         受け入れられる場合は CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE、それ以外は無効にする原因。
 */
static cplat_string_catalog_filter_line_error
resolve_category_constant(const cplat_string_catalog_filter_category_names *names,
                          const string_catalog_filter_constant *constant, int64_t *identifiers,
                          bool *is_category_identifier)
{
    switch (constant->header.kind)
    {
    case STRING_CATALOG_FILTER_CONSTANT_KIND_IDENTIFIER:
        for (size_t index = 0; index < names->count; index++)
        {
            if (strcmp(names->names[index], constant->text) == 0)
            {
                identifiers[constant->header.slot] = (int64_t)index;
                is_category_identifier[constant->header.slot] = true;
                return CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE;
            }
        }
        return CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_UNRESOLVED_CATEGORY_NAME;

    case STRING_CATALOG_FILTER_CONSTANT_KIND_INTEGER:
    case STRING_CATALOG_FILTER_CONSTANT_KIND_CHARACTER:
        if (((constant->header.flags & STRING_CATALOG_FILTER_CONSTANT_FLAG_NEGATIVE) != 0U) ||
            (constant->magnitude >= (uint64_t)names->count))
        {
            return CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_CATEGORY_OUT_OF_RANGE;
        }
        return CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE;

    case STRING_CATALOG_FILTER_CONSTANT_KIND_FLOAT:
        /* 分類値は整数のため、整数でない値は範囲外として扱う */
        if ((constant->real < 0.0) || (constant->real >= (double)names->count) ||
            (constant->real != floor(constant->real)))
        {
            return CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_CATEGORY_OUT_OF_RANGE;
        }
        return CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE;

    default:
        /* 文字列と null は、コンパイルの時点で分類値との比較から除かれている */
        return CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE;
    }
}

/**
 *  @brief          分類値の比較に現れる定数を確かめます。分類値の名前が設定されている場合に限ります。
 *
 *  分類値の意味と範囲を決めるのは利用側です。名前が設定されていなければ、分類値の定数を確かめません。
 */
static cplat_string_catalog_filter_line_error
resolve_category_predicates(const cplat_string_catalog_filter_slot *slot, const unsigned char *record,
                            const unsigned char *constants, const string_catalog_filter_record_header *header,
                            int64_t *identifiers, bool *is_category_identifier)
{
    string_catalog_filter_instruction instruction;
    string_catalog_filter_constant constant;

    if (slot->category_names == NULL)
    {
        return CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE;
    }

    for (uint32_t index = 0; index < header->instruction_count; index++)
    {
        uint32_t offset;

        string_catalog_filter_read_instruction(record, index, &instruction);
        if ((instruction.opcode != (uint8_t)STRING_CATALOG_FILTER_OPCODE_PREDICATE) ||
            (instruction.field != (uint8_t)STRING_CATALOG_FILTER_FIELD_CATEGORY))
        {
            continue;
        }

        offset = instruction.operand;
        for (uint16_t operand = 0; operand < instruction.operand_count; operand++)
        {
            cplat_string_catalog_filter_line_error error;

            (void)string_catalog_filter_read_constant(constants, header->constant_size, offset, &constant);
            offset = constant.next_offset;

            error = resolve_category_constant(slot->category_names, &constant, identifiers, is_category_identifier);
            if (error != CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE)
            {
                return error;
            }
        }
    }
    return CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE;
}

/**
 *  @brief          1 行の名前を解決し、項目ごとの判定結果を事前計算します。
 *  @return         解決できた場合は CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE、それ以外は無効にする原因。
 */
/**
 *  @brief          行の正規表現のパターンをコンパイルし、面へ保持します。
 *  @return         すべてコンパイルできた場合は @ref CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE 。
 *                  できないパターンがある場合は @ref CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_INVALID_PATTERN 。
 *
 *  フィルター オブジェクトはポインターを含まないため、コンパイルした正規表現は面ごとに保持します。\n
 *  別の版のライブラリでコンパイルした条件式は、構文の確認を通っていても、この版で解釈できない場合があります。
 */
static cplat_string_catalog_filter_line_error compile_line_patterns(const cplat_string_catalog_filter_slot *slot,
                                                                    filter_plane *plane, const uint32_t line_index)
{
    const unsigned char *record =
        string_catalog_filter_record_address_const(plane->image, slot->record_size, line_index);
    const unsigned char *constants = string_catalog_filter_record_constants(record, slot->line_width);
    cplat_regex **patterns = patterns_of(plane, line_index);
    string_catalog_filter_record_header header;
    string_catalog_filter_instruction instruction;
    string_catalog_filter_constant constant;

    string_catalog_filter_read_record_header(record, &header);
    for (uint32_t index = 0; index < header.instruction_count; index++)
    {
        uint32_t offset;

        string_catalog_filter_read_instruction(record, index, &instruction);
        if ((instruction.opcode != (uint8_t)STRING_CATALOG_FILTER_OPCODE_PREDICATE) ||
            ((instruction.operator_kind != (uint8_t)STRING_CATALOG_FILTER_OPERATOR_MATCHES) &&
             (instruction.operator_kind != (uint8_t)STRING_CATALOG_FILTER_OPERATOR_MATCHES_I)))
        {
            continue;
        }

        /* 名前で指定した引数では、引数名の定数の直後にパターンが並ぶ */
        offset = instruction.operand;
        (void)string_catalog_filter_read_constant(constants, header.constant_size, offset, &constant);
        if (instruction.field == (uint8_t)STRING_CATALOG_FILTER_FIELD_ARGUMENT_NAME)
        {
            (void)string_catalog_filter_read_constant(constants, header.constant_size, constant.next_offset, &constant);
        }
        if (string_catalog_filter_create_pattern(constant.text, instruction.operator_kind,
                                                 &patterns[constant.header.slot]) != CPLAT_OK)
        {
            return CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_INVALID_PATTERN;
        }
    }
    return CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE;
}

static cplat_string_catalog_filter_line_error resolve_line(const cplat_string_catalog_filter_slot *slot,
                                                           filter_plane *plane, const uint32_t line_index)
{
    const unsigned char *record =
        string_catalog_filter_record_address_const(plane->image, slot->record_size, line_index);
    const unsigned char *constants = string_catalog_filter_record_constants(record, slot->line_width);
    string_catalog_filter_record_header header;
    string_catalog_filter_constant constant;
    uint8_t *states = line_states_of(slot, plane, line_index);
    int8_t *maps = argument_maps_of(slot, plane, line_index);
    int64_t *identifiers = identifier_values_of(plane, line_index);
    bool is_category_identifier[CPLAT_STRING_CATALOG_FILTER_IDENTIFIER_REFERENCE_MAX] = {false};
    bool is_satisfiable = false;
    cplat_string_catalog_filter_line_error error;
    uint32_t offset = 0U;

    string_catalog_filter_read_record_header(record, &header);
    memset(states, 0, slot->entry_count * sizeof(*states));
    memset(maps, -1, slot->entry_count * CPLAT_STRING_CATALOG_FILTER_ARGUMENT_REFERENCE_MAX * sizeof(*maps));
    memset(identifiers, 0, CPLAT_STRING_CATALOG_FILTER_IDENTIFIER_REFERENCE_MAX * sizeof(*identifiers));

    /* 分類値の比較を先に確かめる。分類値と比較する識別子は分類値の名前で解決し、文字列キーの名前では解決しない */
    error = resolve_category_predicates(slot, record, constants, &header, identifiers, is_category_identifier);
    if (error != CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE)
    {
        return error;
    }

    /* 定数をたどり、文字列キーの名前と引数名を解決する */
    while (offset < header.constant_size)
    {
        (void)string_catalog_filter_read_constant(constants, header.constant_size, offset, &constant);
        offset = constant.next_offset;

        if ((constant.header.kind == (uint8_t)STRING_CATALOG_FILTER_CONSTANT_KIND_IDENTIFIER) &&
            is_category_identifier[constant.header.slot])
        {
            /* 分類値の名前で解決済み */
        }
        else if (constant.header.kind == (uint8_t)STRING_CATALOG_FILTER_CONSTANT_KIND_IDENTIFIER)
        {
            const cplat_string_catalog_filter_key_name *key_name = find_key_name(slot, constant.text);

            if (key_name == NULL)
            {
                return CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_UNRESOLVED_KEY_NAME;
            }
            identifiers[constant.header.slot] = key_name->key;
        }
        else if (constant.header.kind == (uint8_t)STRING_CATALOG_FILTER_CONSTANT_KIND_ARGUMENT_NAME)
        {
            bool is_found = false;

            for (size_t entry_index = 0; entry_index < slot->entry_count; entry_index++)
            {
                const cplat_string_catalog_entry *entry = &slot->catalog->entries[entry_index];

                for (int argument = 0; argument < entry->argument_count; argument++)
                {
                    if ((entry->arguments[argument].kind != CPLAT_STRING_CATALOG_ARGUMENT_KIND_UNUSED) &&
                        (entry->arguments[argument].name != NULL) &&
                        (strcmp(entry->arguments[argument].name, constant.text) == 0))
                    {
                        maps[(entry_index * CPLAT_STRING_CATALOG_FILTER_ARGUMENT_REFERENCE_MAX) +
                             constant.header.slot] = (int8_t)argument;
                        is_found = true;
                        break;
                    }
                }
            }
            if (!is_found)
            {
                return CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_UNRESOLVED_ARGUMENT_NAME;
            }
        }
        else
        {
            /* 比較対象の定数は、名前の解決を必要としない */
        }
    }

    for (size_t entry_index = 0; entry_index < slot->entry_count; entry_index++)
    {
        evaluation_context context;

        memset(&context, 0, sizeof(context));
        context.entry = &slot->catalog->entries[entry_index];
        context.record = record;
        context.constants = constants;
        context.constant_size = header.constant_size;
        context.argument_map = maps + (entry_index * CPLAT_STRING_CATALOG_FILTER_ARGUMENT_REFERENCE_MAX);
        context.identifier_values = identifiers;
        context.patterns = patterns_of(plane, line_index);
        context.values = NULL;
        states[entry_index] = (uint8_t)evaluate_line(&context, false);
        if (states[entry_index] != (uint8_t)TRUTH_VALUE_FALSE)
        {
            is_satisfiable = true;
        }
    }

    /* どの項目でも常に不一致となる行は、型の合わない比較や矛盾する条件の書き誤りと考えられるため通知する。
     * 決して一致しない行のため、無効にしても判定の結果は変わらない */
    if (!is_satisfiable)
    {
        return CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NEVER_SATISFIABLE;
    }
    return CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE;
}

/** 参照中の面から、内容が同一の行を探します。 */
static bool find_reusable_line(const cplat_string_catalog_filter_slot *slot, const filter_plane *current,
                               const unsigned char *record, uint32_t *line_index_out)
{
    string_catalog_filter_record_header header;

    string_catalog_filter_read_record_header(record, &header);

    for (uint32_t index = 0; index < current->line_count; index++)
    {
        const unsigned char *candidate =
            string_catalog_filter_record_address_const(current->image, slot->record_size, index);
        string_catalog_filter_record_header candidate_header;

        string_catalog_filter_read_record_header(candidate, &candidate_header);
        /* ハッシュ値の衝突に備え、一致した場合は内容を比較する */
        if ((candidate_header.line_hash == header.line_hash) && (memcmp(candidate, record, slot->record_size) == 0))
        {
            *line_index_out = index;
            return true;
        }
    }
    return false;
}

/** 複製済みのフィルター オブジェクトから、面の判定用の内容を構築します。 */
static void build_plane(const cplat_string_catalog_filter_slot *slot, filter_plane *target, filter_plane *current,
                        cplat_string_catalog_filter_diagnostic *diagnostics, const size_t diagnostic_capacity,
                        size_t *invalid_count)
{
    string_catalog_filter_image_header header;

    string_catalog_filter_read_image_header(target->image, &header);
    target->line_count = header.line_count;

    /* 未使用の面が前回の構築で保持した正規表現を破棄する。判定はこの面を参照していない */
    dispose_patterns(slot, target);

    for (uint32_t line_index = 0; line_index < header.line_count; line_index++)
    {
        const unsigned char *record =
            string_catalog_filter_record_address_const(target->image, slot->record_size, line_index);
        uint32_t reusable_index;

        /* 正規表現は面ごとに保持するため、内容が同一の行を再利用する場合もコンパイルする */
        target->line_errors[line_index] = compile_line_patterns(slot, target, line_index);
        if (target->line_errors[line_index] != CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE)
        {
            /* 無効な行として扱う。事前計算は行わない */
        }
        else if (find_reusable_line(slot, current, record, &reusable_index))
        {
            memcpy(line_states_of(slot, target, line_index), line_states_of(slot, current, reusable_index),
                   slot->entry_count * sizeof(*target->line_states));
            memcpy(argument_maps_of(slot, target, line_index), argument_maps_of(slot, current, reusable_index),
                   slot->entry_count * CPLAT_STRING_CATALOG_FILTER_ARGUMENT_REFERENCE_MAX *
                       sizeof(*target->argument_maps));
            memcpy(identifier_values_of(target, line_index), identifier_values_of(current, reusable_index),
                   CPLAT_STRING_CATALOG_FILTER_IDENTIFIER_REFERENCE_MAX * sizeof(*target->identifier_values));
            target->line_errors[line_index] = current->line_errors[reusable_index];
        }
        else
        {
            target->line_errors[line_index] = resolve_line(slot, target, line_index);
        }

        if (target->line_errors[line_index] != CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE)
        {
            if ((diagnostics != NULL) && (*invalid_count < diagnostic_capacity))
            {
                diagnostics[*invalid_count].line_index = line_index;
                diagnostics[*invalid_count].column = 0U;
                diagnostics[*invalid_count].error = target->line_errors[line_index];
            }
            (*invalid_count)++;
        }
    }

    /* 行ごとの結果を、論理和の規則で項目ごとに集約する */
    for (size_t entry_index = 0; entry_index < slot->entry_count; entry_index++)
    {
        uint64_t *dependent_lines = dependent_lines_of(slot, target, entry_index);
        bool has_dependent = false;
        bool is_always = false;

        memset(dependent_lines, 0, slot->line_word_count * sizeof(*dependent_lines));
        for (uint32_t line_index = 0; line_index < target->line_count; line_index++)
        {
            uint8_t state;

            if (!is_line_enabled(target, line_index))
            {
                continue;
            }
            state = line_states_of(slot, target, line_index)[entry_index];
            if (state == (uint8_t)TRUTH_VALUE_TRUE)
            {
                is_always = true;
                break;
            }
            if (state == (uint8_t)TRUTH_VALUE_UNKNOWN)
            {
                dependent_lines[line_index / LINE_WORD_BITS] |= (uint64_t)1U << (line_index % LINE_WORD_BITS);
                has_dependent = true;
            }
        }

        if (is_always)
        {
            target->entry_states[entry_index] = (uint8_t)CPLAT_STRING_CATALOG_FILTER_STATE_ALWAYS_MATCH;
            memset(dependent_lines, 0, slot->line_word_count * sizeof(*dependent_lines));
        }
        else if (has_dependent)
        {
            target->entry_states[entry_index] = (uint8_t)CPLAT_STRING_CATALOG_FILTER_STATE_ARGUMENT_DEPENDENT;
        }
        else
        {
            target->entry_states[entry_index] = (uint8_t)CPLAT_STRING_CATALOG_FILTER_STATE_NEVER_MATCH;
        }
    }
}

/** 文字列キーに対応する項目のインデックスを求めます。 */
static bool find_entry_index(const cplat_string_catalog_filter_slot *slot, const int string_key,
                             size_t *entry_index_out)
{
    const cplat_string_catalog_entry *entry = cplat_string_catalog_get_entry(slot->catalog, string_key);

    if (entry == NULL)
    {
        return false;
    }
    *entry_index_out = (size_t)(entry - slot->catalog->entries);
    return true;
}

/* ===== 公開関数 ===== */

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_string_catalog_filter_slot_create(const cplat_string_catalog *catalog,
                                            const cplat_string_catalog_filter_key_name *key_names,
                                            const size_t key_name_count,
                                            const cplat_string_catalog_filter_category_names *category_names,
                                            const size_t line_capacity, const size_t line_width,
                                            cplat_string_catalog_filter_slot **slot_out)
{
    cplat_string_catalog_filter_slot *slot;
    int ret;

    if (slot_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    *slot_out = NULL;

    if ((catalog == NULL) || (catalog->entry_count < 0) || ((catalog->entry_count > 0) && (catalog->entries == NULL)) ||
        ((key_names == NULL) && (key_name_count > 0U)) || (line_capacity == 0U) ||
        (line_capacity > CPLAT_STRING_CATALOG_FILTER_LINE_MAX) ||
        (line_width < CPLAT_STRING_CATALOG_FILTER_LINE_WIDTH_MIN) ||
        (line_width > CPLAT_STRING_CATALOG_FILTER_LINE_WIDTH_MAX))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    /* 判定と説明文はカタログの引数定義を直接参照するため、作成時に定義全体を検査する。
     * 項目数が 0 のカタログは、どの文字列キーにも一致しない何もしないカタログとして検査せずに受け付ける */
    if (catalog->entry_count > 0)
    {
        ret = cplat_string_catalog_verify(catalog, NULL, NULL);
        if (ret != CPLAT_OK)
        {
            return ret;
        }
    }
    if (!is_key_name_table_valid(catalog, key_names, key_name_count) || !is_category_names_valid(category_names))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    slot = (cplat_string_catalog_filter_slot *)calloc(1U, sizeof(*slot));
    if (slot == NULL)
    {
        return CPLAT_ERR_OUT_OF_MEMORY;
    }

    slot->catalog = catalog;
    slot->key_names = key_names;
    slot->key_name_count = key_name_count;
    slot->category_names = category_names;
    slot->entry_count = (size_t)catalog->entry_count;
    slot->line_capacity = (uint32_t)line_capacity;
    slot->line_width = (uint32_t)line_width;
    slot->record_size = (uint32_t)CPLAT_STRING_CATALOG_FILTER_RECORD_SIZE(line_width);
    slot->image_size = CPLAT_STRING_CATALOG_FILTER_IMAGE_SIZE(line_capacity, line_width);
    slot->line_word_count = (line_capacity + LINE_WORD_BITS - 1U) / LINE_WORD_BITS;
    (void)cplat_string_catalog_filter_get_catalog_id(catalog, &slot->catalog_id);

    if (!allocate_plane(slot, &slot->planes[0]) || !allocate_plane(slot, &slot->planes[1]))
    {
        cplat_string_catalog_filter_slot_dispose(&slot);
        return CPLAT_ERR_OUT_OF_MEMORY;
    }

    /* ロックの作成に失敗した場合は、作成関数の結果コードをそのまま返す */
    ret = cplat_local_rwlock_create(&slot->plane_lock);
    if (ret == CPLAT_OK)
    {
        ret = cplat_local_lock_create(&slot->apply_lock);
    }
    if (ret != CPLAT_OK)
    {
        cplat_string_catalog_filter_slot_dispose(&slot);
        return ret;
    }

    /* 行を持たないフィルター オブジェクトを適用した状態から始める。全項目が「常に不一致」となる */
    (void)cplat_string_catalog_filter_compile(NULL, 0U, line_width, line_capacity, slot->planes[0].image,
                                              slot->image_size, NULL, 0U, NULL);
    slot->active_plane = 0;

    *slot_out = slot;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_string_catalog_filter_slot_dispose(cplat_string_catalog_filter_slot **slot)
{
    if ((slot == NULL) || (*slot == NULL))
    {
        return;
    }

    if ((*slot)->plane_lock != NULL)
    {
        cplat_local_rwlock_dispose((*slot)->plane_lock);
    }
    if ((*slot)->apply_lock != NULL)
    {
        cplat_local_lock_dispose((*slot)->apply_lock);
    }
    free_plane(*slot, &(*slot)->planes[0]);
    free_plane(*slot, &(*slot)->planes[1]);
    free(*slot);
    *slot = NULL;
}

/* Doxygen コメントは、ヘッダーに記載 */

const cplat_string_catalog *cplat_string_catalog_filter_slot_get_catalog(const cplat_string_catalog_filter_slot *slot)
{
    if (slot == NULL)
    {
        return NULL;
    }
    return slot->catalog;
}

/**
 *  @brief          未使用の面へ複製したフィルター オブジェクトを検証し、行数の上限と行幅がスロットと一致することを確かめます。
 *  @return         `CPLAT_OK`、形式版が異なる場合は `CPLAT_ERR_VERSION_MISMATCH`、それ以外は `CPLAT_ERR_CORRUPT_DESCRIPTOR`。
 */
static int validate_copied_image(const cplat_string_catalog_filter_slot *slot, const filter_plane *target)
{
    string_catalog_filter_image_header header;
    int ret;

    ret = cplat_string_catalog_filter_validate(target->image, slot->image_size);
    if (ret == CPLAT_OK)
    {
        string_catalog_filter_read_image_header(target->image, &header);
        if ((header.line_capacity != slot->line_capacity) || (header.line_width != slot->line_width))
        {
            ret = CPLAT_ERR_CORRUPT_DESCRIPTOR;
        }
    }
    return ret;
}

/**
 *  @brief          apply_lock を保持した状態で、フィルター オブジェクトを未使用の面へ構築して切り替えます。
 *  @param[in,out]  slot                フィルター スロット。
 *  @param[in]      image               適用するフィルター オブジェクト。
 *  @param[in]      image_size          @p image のバイト数。
 *  @param[out]     diagnostics         無効にした行の診断の格納先。不要な場合は NULL。
 *  @param[in]      diagnostic_capacity @p diagnostics の要素数。
 *  @param[out]     invalid_count_out   無効にした行数の格納先。不要な場合は NULL。
 *  @param[in]      source    読み取り元のソース領域。@p image がソース領域内にない場合は NULL。
 *  @param[in]      revision @p source の読み取りを始めたときの版番号。
 *  @param[in]      writer_lock 取得済みの書き込み側の排他。複製を終えた時点で解放します。保持していない場合は NULL。
 *  @param[out]     torn_out  複製の間にソース領域が書き換えられた場合は true の格納先。
 *
 *  戻り値は @ref cplat_string_catalog_filter_slot_apply と同じです。\n
 *  @p torn_out へ true を格納した場合は、面を切り替えずに `CPLAT_ERR_CORRUPT_DESCRIPTOR` を返します。
 */
static int apply_image_locked(cplat_string_catalog_filter_slot *slot, const void *image, const size_t image_size,
                              cplat_string_catalog_filter_diagnostic *diagnostics, const size_t diagnostic_capacity,
                              size_t *invalid_count_out, const void *source, const uint64_t revision,
                              const cplat_string_catalog_filter_source_lock *writer_lock, bool *torn_out)
{
    filter_plane *target;
    filter_plane *current;
    size_t invalid_count = 0U;
    int target_plane;
    int ret;

    *torn_out = false;

    /* 面を書き換えるのは適用だけであり、適用は apply_lock で直列化している。
     * そのため active_plane と参照中の面は、ここではロックなしで読める。 */
    target_plane = 1 - slot->active_plane;
    target = &slot->planes[target_plane];
    current = &slot->planes[slot->active_plane];

    /* 引き渡された領域は、複製してから検証する。
     * 共有メモリの領域が検証後に書き換えられても、検証済みの内容だけを使用するためです。 */
    ret = CPLAT_ERR_CORRUPT_DESCRIPTOR;
    if (image_size >= slot->image_size)
    {
        memcpy(target->image, image, slot->image_size);

        /* ソース領域からの複製では、複製の間に公開が重なっていないことを確かめる。
         * 重なった場合は未使用の面へ複製しただけなので、捨てても参照中の条件に影響しない */
        *torn_out = (source != NULL) && !string_catalog_filter_source_end_read(source, revision);
    }

    /* 書き込み側の排他は複製と確認の間だけ保持する。検証と適用は排他の外で行い、書き込み側を待たせない */
    if (writer_lock != NULL)
    {
        writer_lock->unlock(writer_lock->context);
    }
    if (*torn_out)
    {
        return CPLAT_ERR_CORRUPT_DESCRIPTOR;
    }

    if (image_size >= slot->image_size)
    {
        ret = validate_copied_image(slot, target);
    }

    if (ret == CPLAT_OK)
    {
        build_plane(slot, target, current, diagnostics, diagnostic_capacity, &invalid_count);

        /* 判定の途中で面が変わらないよう、切り替えは排他モードで行う */
        ret = cplat_local_rwlock_lock_exclusive(slot->plane_lock, CPLAT_SYNC_WAIT_FOREVER);
        if (ret == CPLAT_OK)
        {
            slot->active_plane = target_plane;
            (void)cplat_local_rwlock_unlock_exclusive(slot->plane_lock);
        }
    }

    if ((ret == CPLAT_OK) && (invalid_count_out != NULL))
    {
        *invalid_count_out = invalid_count;
    }
    return ret;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_string_catalog_filter_slot_apply(cplat_string_catalog_filter_slot *slot, const void *image,
                                           const size_t image_size, cplat_string_catalog_filter_diagnostic *diagnostics,
                                           const size_t diagnostic_capacity, size_t *invalid_count_out)
{
    bool torn;
    int ret;

    if ((slot == NULL) || (image == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    ret = cplat_local_lock_lock(slot->apply_lock, CPLAT_SYNC_WAIT_FOREVER);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    ret = apply_image_locked(slot, image, image_size, diagnostics, diagnostic_capacity, invalid_count_out, NULL, 0U,
                             NULL, &torn);
    (void)cplat_local_lock_unlock(slot->apply_lock);
    return ret;
}

/* ===== 型が合わない比較要素の警告 ===== */

/** 警告を 1 件数え、容量までを格納します。 */
static void add_warning(cplat_string_catalog_filter_warning *warnings, const size_t capacity, size_t *count,
                        const cplat_string_catalog_filter_warning *warning)
{
    if ((warnings != NULL) && (*count < capacity))
    {
        warnings[*count] = *warning;
    }
    (*count)++;
}

/**
 *  @brief          構築した面の 1 行について、型の不一致と型区分の混在の警告を集めます。
 *
 *  名前の解決が済んだ行だけを対象とします。項目ごとの引数の位置は、適用時に解決した対応表を使います。\n
 *  比較の可否は、判定と同じ @ref is_argument_comparable で決めます。
 */
static void collect_line_warnings(const cplat_string_catalog_filter_slot *slot, filter_plane *plane,
                                  const uint32_t line_index, cplat_string_catalog_filter_warning *warnings,
                                  const size_t capacity, size_t *count)
{
    const unsigned char *record =
        string_catalog_filter_record_address_const(plane->image, slot->record_size, line_index);
    const unsigned char *constants = string_catalog_filter_record_constants(record, slot->line_width);
    const int8_t *maps = argument_maps_of(slot, plane, line_index);
    const char *noted[CPLAT_STRING_CATALOG_FILTER_ARGUMENT_REFERENCE_MAX];
    size_t noted_count = 0U;
    uint32_t predicate_index = 0U;
    string_catalog_filter_record_header header;
    string_catalog_filter_instruction instruction;
    string_catalog_filter_constant constant;
    cplat_string_catalog_filter_warning warning;

    string_catalog_filter_read_record_header(record, &header);
    for (uint32_t index = 0; index < header.instruction_count; index++)
    {
        const char *name = NULL;
        uint32_t offset;

        string_catalog_filter_read_instruction(record, index, &instruction);
        if (instruction.opcode != (uint8_t)STRING_CATALOG_FILTER_OPCODE_PREDICATE)
        {
            continue;
        }
        predicate_index++;
        if (((instruction.field != (uint8_t)STRING_CATALOG_FILTER_FIELD_ARGUMENT_NAME) &&
             (instruction.field != (uint8_t)STRING_CATALOG_FILTER_FIELD_ARGUMENT_INDEX)) ||
            (instruction.operator_kind == (uint8_t)STRING_CATALOG_FILTER_OPERATOR_HAS))
        {
            continue;
        }

        memset(&warning, 0, sizeof(warning));
        warning.line_index = line_index;
        warning.predicate_index = predicate_index - 1U;

        /* 名前で指定した引数では、比較対象は引数名の定数の直後から並ぶ */
        offset = instruction.operand;
        if (instruction.field == (uint8_t)STRING_CATALOG_FILTER_FIELD_ARGUMENT_NAME)
        {
            (void)string_catalog_filter_read_constant(constants, header.constant_size, offset, &constant);
            name = constant.text;
            offset = constant.next_offset;
        }
        (void)string_catalog_filter_read_constant(constants, header.constant_size, offset, &constant);

        /* 型区分の混在は、引数名ごとに最初の比較要素で 1 回だけ警告する */
        if (name != NULL)
        {
            string_catalog_filter_mixed_argument mixed;
            bool is_noted = false;

            for (size_t other = 0; other < noted_count; other++)
            {
                if (strcmp(noted[other], name) == 0)
                {
                    is_noted = true;
                    break;
                }
            }
            if (!is_noted && (noted_count < CPLAT_STRING_CATALOG_FILTER_ARGUMENT_REFERENCE_MAX) &&
                string_catalog_filter_find_mixed_argument(slot->catalog, name, &mixed))
            {
                noted[noted_count] = name;
                noted_count++;
                warning.kind = CPLAT_STRING_CATALOG_FILTER_WARNING_MIXED_ARGUMENT_TYPES;
                warning.string_key = mixed.string_entry->key;
                warning.argument_index = mixed.string_argument;
                warning.other_string_key = mixed.other_entry->key;
                warning.other_argument_index = mixed.other_argument;
                add_warning(warnings, capacity, count, &warning);
            }
        }

        for (size_t entry_index = 0; entry_index < slot->entry_count; entry_index++)
        {
            const cplat_string_catalog_entry *entry = &slot->catalog->entries[entry_index];
            int argument_index = -1;

            if (instruction.field == (uint8_t)STRING_CATALOG_FILTER_FIELD_ARGUMENT_NAME)
            {
                argument_index =
                    maps[(entry_index * CPLAT_STRING_CATALOG_FILTER_ARGUMENT_REFERENCE_MAX) + instruction.argument];
            }
            else if (((int)instruction.argument < entry->argument_count) &&
                     (entry->arguments[instruction.argument].kind != CPLAT_STRING_CATALOG_ARGUMENT_KIND_UNUSED))
            {
                argument_index = instruction.argument;
            }
            else
            {
                /* 項目に引数が無い。型の不一致の対象外 */
            }

            if ((argument_index < 0) ||
                is_argument_comparable(class_of_argument(entry->arguments[argument_index].kind), constant.header.kind))
            {
                continue;
            }
            warning.kind = CPLAT_STRING_CATALOG_FILTER_WARNING_TYPE_MISMATCH;
            warning.string_key = entry->key;
            warning.argument_index = argument_index;
            warning.other_string_key = entry->key;
            warning.other_argument_index = -1;
            add_warning(warnings, capacity, count, &warning);
        }
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_string_catalog_filter_slot_check(cplat_string_catalog_filter_slot *slot, const void *image,
                                           const size_t image_size, cplat_string_catalog_filter_diagnostic *diagnostics,
                                           const size_t diagnostic_capacity, size_t *invalid_count_out,
                                           cplat_string_catalog_filter_warning *warnings, const size_t warning_capacity,
                                           size_t *warning_count_out)
{
    filter_plane *target;
    size_t invalid_count = 0U;
    size_t warning_count = 0U;
    int ret;

    if ((slot == NULL) || (image == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    ret = cplat_local_lock_lock(slot->apply_lock, CPLAT_SYNC_WAIT_FOREVER);
    if (ret != CPLAT_OK)
    {
        return ret;
    }

    /* 未使用の面へ構築し、切り替えない。未使用の面は適用のたびに作り直すため、残った内容は判定に影響しない */
    target = &slot->planes[1 - slot->active_plane];
    ret = CPLAT_ERR_CORRUPT_DESCRIPTOR;
    if (image_size >= slot->image_size)
    {
        memcpy(target->image, image, slot->image_size);
        ret = validate_copied_image(slot, target);
    }

    if (ret == CPLAT_OK)
    {
        build_plane(slot, target, &slot->planes[slot->active_plane], diagnostics, diagnostic_capacity,
                    &invalid_count);
        for (uint32_t line_index = 0; line_index < target->line_count; line_index++)
        {
            /* 成立し得ない行は、型の不一致が原因であり得るため対象に含める */
            if ((target->line_errors[line_index] == CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE) ||
                (target->line_errors[line_index] == CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NEVER_SATISFIABLE))
            {
                collect_line_warnings(slot, target, line_index, warnings, warning_capacity, &warning_count);
            }
        }
    }
    (void)cplat_local_lock_unlock(slot->apply_lock);

    if (ret == CPLAT_OK)
    {
        if (invalid_count_out != NULL)
        {
            *invalid_count_out = invalid_count;
        }
        if (warning_count_out != NULL)
        {
            *warning_count_out = warning_count;
        }
    }
    return ret;
}

/**
 *  @brief          ソース領域の版番号が変わっていれば、フィルター オブジェクトを取り込みます。
 *
 *  版番号の読み取りは 1 回のアトミックな読み取りで、変化がなければロックを取らずに戻ります (緩いチェック)。\n
 *  ほかのスレッドが取り込み中または適用中の場合は待たずに戻り、適用済みの条件で判定を続けます。
 *
 *  書き込み側の排他を結び付けている場合は、その排他を取ってから版番号を読み直し (最終チェック)、
 *  取り込み済みでなければ複製します。排他は複製を終えた時点で解放します。\n
 *  結び付けていない場合は、ロックを取らずに複製し、複製の後に版番号を読み直します。
 *  複製の間に公開が重なった場合は記録せず、次の判定で改めて取り込みます。
 *
 *  ファイルをマップした領域は、異なる版のライブラリや異なる行数の上限と行幅で書かれた内容を残している場合があります。
 *  ヘッダーの形式と大きさがスロットと一致しない公開内容は、フィルター オブジェクトを読まずに記録します。
 *  形式版の不一致は `CPLAT_ERR_VERSION_MISMATCH`、別のカタログ向けの公開内容は `CPLAT_ERR_IDENTITY_MISMATCH`、
 *  そのほかの不一致は `CPLAT_ERR_CORRUPT_DESCRIPTOR` です。
 */
static void refresh_from_source(cplat_string_catalog_filter_slot *slot)
{
    const cplat_string_catalog_filter_source_lock *writer_lock = NULL;
    string_catalog_filter_source_header header;
    size_t invalid_count = 0U;
    uint64_t revision;
    bool torn;
    int ret;

    if (slot->source == NULL)
    {
        return;
    }
    revision = string_catalog_filter_source_begin_read(slot->source);
    if ((revision == 0U) || ((revision & 1U) != 0U) ||
        (revision == cplat_atomic_load_u64(&slot->taken_revision, CPLAT_MEMORY_ORDER_RELAXED)))
    {
        return;
    }
    if (cplat_local_lock_try_lock(slot->apply_lock) != CPLAT_OK)
    {
        return;
    }

    if (slot->source_lock.lock != NULL)
    {
        writer_lock = &slot->source_lock;
        ret = writer_lock->lock(writer_lock->context);
        if (ret != CPLAT_OK)
        {
            /* 取り込みを試みていないため取り込み済みとはせず、次の判定で改めて試みる */
            slot->source_last_result = ret;
            (void)cplat_local_lock_unlock(slot->apply_lock);
            return;
        }

        /* 排他の下で版番号を読み直す。待つ間に公開が進んでいれば、新しい版番号を取り込む */
        revision = string_catalog_filter_source_begin_read(slot->source);
    }

    /* ロックを待つ間に、ほかのスレッドが同じ版番号を取り込んでいる場合がある。
     * 排他の下で奇数が見えるのは、書き込みの途中で書き込み側が停止した場合だけで、次の公開まで取り込まない */
    if ((revision == 0U) || ((revision & 1U) != 0U) ||
        (revision == cplat_atomic_load_u64(&slot->taken_revision, CPLAT_MEMORY_ORDER_RELAXED)))
    {
        if (writer_lock != NULL)
        {
            writer_lock->unlock(writer_lock->context);
        }
        (void)cplat_local_lock_unlock(slot->apply_lock);
        return;
    }

    /* ヘッダーも版番号の読み直しで一貫性を確かめる。一貫しない場合は次の判定で改めて取り込む */
    memcpy(&header, slot->source, sizeof(header));
    ret = string_catalog_filter_source_check_header(&header);
    if ((ret == CPLAT_OK) &&
        ((header.line_capacity != slot->line_capacity) || (header.line_width != slot->line_width) ||
         (header.image_size != (uint64_t)slot->image_size)))
    {
        ret = CPLAT_ERR_CORRUPT_DESCRIPTOR;
    }
    else if ((ret == CPLAT_OK) && (header.catalog_id != slot->catalog_id))
    {
        /* 別のカタログ (別の版の定義を含む) 向けの公開内容は、名前が解決できても意味が異なり得るため取り込まない */
        ret = CPLAT_ERR_IDENTITY_MISMATCH;
    }
    if (ret != CPLAT_OK)
    {
        if (writer_lock != NULL)
        {
            writer_lock->unlock(writer_lock->context);
        }
        if (string_catalog_filter_source_end_read(slot->source, revision))
        {
            slot->source_last_result = ret;
            slot->source_last_invalid_count = 0U;
            cplat_atomic_store_u64(&slot->taken_revision, revision, CPLAT_MEMORY_ORDER_RELAXED);
        }
        (void)cplat_local_lock_unlock(slot->apply_lock);
        return;
    }

    ret = apply_image_locked(slot, (const unsigned char *)slot->source + CPLAT_STRING_CATALOG_FILTER_SOURCE_HEADER_SIZE,
                             slot->source_size - CPLAT_STRING_CATALOG_FILTER_SOURCE_HEADER_SIZE, NULL, 0U,
                             &invalid_count, slot->source, revision, writer_lock, &torn);
    if (!torn)
    {
        /* 適用に失敗した公開内容も記録し、同じ内容の取り込みを繰り返さない */
        slot->source_last_result = ret;
        if (ret == CPLAT_OK)
        {
            slot->source_last_invalid_count = invalid_count;
        }
        else
        {
            slot->source_last_invalid_count = 0U;
        }
        cplat_atomic_store_u64(&slot->taken_revision, revision, CPLAT_MEMORY_ORDER_RELAXED);
    }
    (void)cplat_local_lock_unlock(slot->apply_lock);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_string_catalog_filter_slot_attach_source(cplat_string_catalog_filter_slot *slot, const void *source,
                                                   const size_t source_size,
                                                   const cplat_string_catalog_filter_source_lock *lock)
{
    if (slot == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if ((source != NULL) && !string_catalog_filter_source_is_region_valid(source, source_size, slot->image_size))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if ((lock != NULL) && ((lock->lock == NULL) || (lock->unlock == NULL)))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    slot->source = source;
    if (source != NULL)
    {
        slot->source_size = source_size;
    }
    else
    {
        slot->source_size = 0U;
    }
    memset(&slot->source_lock, 0, sizeof(slot->source_lock));
    if ((source != NULL) && (lock != NULL))
    {
        slot->source_lock = *lock;
    }
    slot->source_last_result = CPLAT_OK;
    slot->source_last_invalid_count = 0U;
    cplat_atomic_store_u64(&slot->taken_revision, 0U, CPLAT_MEMORY_ORDER_RELAXED);
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_string_catalog_filter_slot_get_source_status(cplat_string_catalog_filter_slot *slot,
                                                       cplat_string_catalog_filter_source_status *status_out)
{
    int ret;

    if ((slot == NULL) || (status_out == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    /* 結果コードと無効にした行の数は、同じ取り込みの値をそろえて返す */
    ret = cplat_local_lock_lock(slot->apply_lock, CPLAT_SYNC_WAIT_FOREVER);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    memset(status_out, 0, sizeof(*status_out));
    status_out->taken_revision = cplat_atomic_load_u64(&slot->taken_revision, CPLAT_MEMORY_ORDER_RELAXED);
    status_out->last_result = slot->source_last_result;
    status_out->last_invalid_count = slot->source_last_invalid_count;
    (void)cplat_local_lock_unlock(slot->apply_lock);
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_string_catalog_filter_slot_snapshot(cplat_string_catalog_filter_slot *slot, void *image_out,
                                              const size_t image_size)
{
    const filter_plane *plane;
    int ret;

    if ((slot == NULL) || (image_out == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (image_size < slot->image_size)
    {
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    ret = cplat_local_rwlock_lock_shared(slot->plane_lock, CPLAT_SYNC_WAIT_FOREVER);
    if (ret != CPLAT_OK)
    {
        return ret;
    }

    plane = &slot->planes[slot->active_plane];
    memcpy(image_out, plane->image, slot->image_size);

    (void)cplat_local_rwlock_unlock_shared(slot->plane_lock);
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_string_catalog_filter_slot_get_line_error(cplat_string_catalog_filter_slot *slot, const size_t line_index,
                                                    cplat_string_catalog_filter_line_error *error_out)
{
    const filter_plane *plane;
    int ret;

    if ((slot == NULL) || (error_out == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    ret = cplat_local_rwlock_lock_shared(slot->plane_lock, CPLAT_SYNC_WAIT_FOREVER);
    if (ret != CPLAT_OK)
    {
        return ret;
    }

    plane = &slot->planes[slot->active_plane];
    if (line_index >= plane->line_count)
    {
        ret = CPLAT_ERR_INVALID_ARGUMENT;
    }
    else
    {
        *error_out = plane->line_errors[line_index];
    }

    (void)cplat_local_rwlock_unlock_shared(slot->plane_lock);
    return ret;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_string_catalog_filter_slot_test(cplat_string_catalog_filter_slot *slot, const int string_key,
                                          cplat_string_catalog_filter_state *state_out)
{
    size_t entry_index;
    int ret;

    if ((slot == NULL) || (state_out == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (!find_entry_index(slot, string_key, &entry_index))
    {
        return CPLAT_ERR_NOT_FOUND;
    }

    ret = cplat_local_rwlock_lock_shared(slot->plane_lock, CPLAT_SYNC_WAIT_FOREVER);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    *state_out = (cplat_string_catalog_filter_state)slot->planes[slot->active_plane].entry_states[entry_index];
    (void)cplat_local_rwlock_unlock_shared(slot->plane_lock);
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_string_catalog_filter_slot_describe_line(cplat_string_catalog_filter_slot *slot, const size_t line_index,
                                                   char *dest, const size_t dest_size)
{
    string_catalog_filter_describe_source source;
    filter_plane *plane;
    const uint8_t *states;
    size_t candidate_count = 0U;
    int ret;

    if ((slot == NULL) || (dest == NULL) || (dest_size == 0U))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    dest[0] = '\0';

    ret = cplat_local_rwlock_lock_shared(slot->plane_lock, CPLAT_SYNC_WAIT_FOREVER);
    if (ret != CPLAT_OK)
    {
        return ret;
    }

    plane = &slot->planes[slot->active_plane];
    if (line_index >= plane->line_count)
    {
        ret = CPLAT_ERR_INVALID_ARGUMENT;
    }
    else if (!is_line_enabled(plane, (uint32_t)line_index))
    {
        /* 名前を解決できずに無効とした行は、メタ情報と結び付けられない */
        ret = CPLAT_ERR_MALFORMED_DEFINITION;
    }
    else
    {
        memset(&source, 0, sizeof(source));
        source.catalog = slot->catalog;
        source.record =
            string_catalog_filter_record_address_const(plane->image, slot->record_size, (uint32_t)line_index);
        source.identifier_values = identifier_values_of(plane, (uint32_t)line_index);
        source.category_names = slot->category_names;
        source.line_width = slot->line_width;
        source.is_japanese = (cplat_string_catalog_get_language() == CPLAT_STRING_CATALOG_LANGUAGE_JAPANESE);

        /* 事前計算で「常に不一致」とならなかった項目が 1 つだけなら、その行は 1 つの項目に限定されている */
        states = line_states_of(slot, plane, (uint32_t)line_index);
        for (size_t entry_index = 0; entry_index < slot->entry_count; entry_index++)
        {
            if (states[entry_index] != (uint8_t)TRUTH_VALUE_FALSE)
            {
                source.single_entry = &slot->catalog->entries[entry_index];
                candidate_count++;
            }
        }
        if (candidate_count != 1U)
        {
            source.single_entry = NULL;
        }

        ret = string_catalog_filter_describe_record(&source, dest, dest_size);
    }

    (void)cplat_local_rwlock_unlock_shared(slot->plane_lock);
    return ret;
}

/** 参照中の面で、1 項目の一致を判定します。共有モードのロックの下で呼び出します。 */
/** 引数の値に依存する 1 行を、引数の値で評価します。 */
static bool is_line_matched(const cplat_string_catalog_filter_slot *slot, filter_plane *plane,
                            const size_t entry_index, const uint32_t line_index, const argument_value *values)
{
    const unsigned char *record = string_catalog_filter_record_address_const(plane->image, slot->record_size, line_index);
    string_catalog_filter_record_header header;
    evaluation_context context;

    string_catalog_filter_read_record_header(record, &header);
    memset(&context, 0, sizeof(context));
    context.entry = &slot->catalog->entries[entry_index];
    context.record = record;
    context.constants = string_catalog_filter_record_constants(record, slot->line_width);
    context.constant_size = header.constant_size;
    context.argument_map =
        argument_maps_of(slot, plane, line_index) + (entry_index * CPLAT_STRING_CATALOG_FILTER_ARGUMENT_REFERENCE_MAX);
    context.identifier_values = identifier_values_of(plane, line_index);
    context.patterns = patterns_of(plane, line_index);
    context.values = values;
    return evaluate_line(&context, true) == TRUTH_VALUE_TRUE;
}

static bool is_matched(const cplat_string_catalog_filter_slot *slot, filter_plane *plane, const size_t entry_index,
                       const cplat_internal_string_catalog_argument_value *shared)
{
    const cplat_string_catalog_entry *entry = &slot->catalog->entries[entry_index];
    const uint64_t *dependent_lines = dependent_lines_of(slot, plane, entry_index);
    argument_value values[CPLAT_STRING_CATALOG_ARGUMENT_MAX];

    switch (plane->entry_states[entry_index])
    {
    case CPLAT_STRING_CATALOG_FILTER_STATE_ALWAYS_MATCH:
        return true;
    case CPLAT_STRING_CATALOG_FILTER_STATE_ARGUMENT_DEPENDENT:
        break;
    default:
        return false;
    }

    if (!normalize_arguments(entry, shared, values))
    {
        return false;
    }

    /* 引数の値に依存する行だけを、語ごとに立っているビットをたどって評価する */
    for (size_t word_index = 0; word_index < slot->line_word_count; word_index++)
    {
        uint64_t lines = dependent_lines[word_index];

        for (uint32_t bit = 0; lines != 0U; bit++, lines >>= 1)
        {
            if (((lines & 1U) != 0U) &&
                is_line_matched(slot, plane, entry_index, (uint32_t)(word_index * LINE_WORD_BITS) + bit, values))
            {
                return true;
            }
        }
    }
    return false;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_string_catalog_filter_slot_vformat(cplat_string_catalog_filter_slot *slot, char *dest, const size_t dest_size,
                                             int *matched_out, const int string_key, va_list args)
{
    const cplat_string_catalog_entry *entry = NULL;
    const char *text = NULL;
    cplat_internal_string_catalog_argument_value values[CPLAT_STRING_CATALOG_ARGUMENT_MAX] = {0};
    int ret;

    if ((slot == NULL) || (matched_out == NULL) || (dest == NULL) || (dest_size == 0U))
    {
        if (matched_out != NULL)
        {
            *matched_out = 0;
        }
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    *matched_out = 0;
    dest[0] = '\0';

    /* 判定の前に、ソース領域の公開内容が変わっていれば取り込む。通常は版番号の比較 1 回で戻る */
    refresh_from_source(slot);

    /* 可変長引数は 1 回だけ取り出し、判定と書式展開で同じ値を使用する */
    ret = cplat_internal_string_catalog_prepare_format(slot->catalog, string_key, args, &entry, &text, values);
    if (ret != CPLAT_OK)
    {
        return ret;
    }

    /* 判定できない場合は不一致と区別するため、文字列を組み立てずに結果コードを返す */
    ret = cplat_local_rwlock_lock_shared(slot->plane_lock, CPLAT_SYNC_WAIT_FOREVER);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    *matched_out =
        is_matched(slot, &slot->planes[slot->active_plane], (size_t)(entry - slot->catalog->entries), values);
    (void)cplat_local_rwlock_unlock_shared(slot->plane_lock);

    /* 書式展開はロックの外で行う。判定の結果によらず文字列を組み立てる */
    return cplat_internal_string_catalog_render_text(dest, dest_size, text, values, entry->argument_count);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_string_catalog_filter_slot_format(cplat_string_catalog_filter_slot *slot, char *dest, const size_t dest_size,
                                            int *matched_out, const int string_key, ...)
{
    va_list args;
    int ret;

    va_start(args, string_key);
    ret = cplat_string_catalog_filter_slot_vformat(slot, dest, dest_size, matched_out, string_key, args);
    va_end(args);
    return ret;
}
