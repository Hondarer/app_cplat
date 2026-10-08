/**
 *******************************************************************************
 *  @file           filter_image.c
 *  @brief          フィルター オブジェクトの読み書き、検証、デコンパイルを実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/09/26
 *  @version        0.1.0
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *******************************************************************************
 */

#include "filter.h"

#include <cplat/base/result.h>

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(string_catalog_filter_image_header) == CPLAT_STRING_CATALOG_FILTER_HEADER_SIZE,
               "cplat: header size");
_Static_assert(sizeof(string_catalog_filter_record_header) == CPLAT_STRING_CATALOG_FILTER_RECORD_HEADER_SIZE,
               "cplat: record header size");
_Static_assert(sizeof(string_catalog_filter_instruction) == 8U, "cplat: instruction size");
_Static_assert(sizeof(string_catalog_filter_constant_header) == STRING_CATALOG_FILTER_CONSTANT_HEADER_SIZE,
               "cplat: constant header size");

/** FNV-1a 64 ビットの初期値です。 */
#define FNV_OFFSET_BASIS 14695981039346656037ULL

/** FNV-1a 64 ビットの乗数です。 */
#define FNV_PRIME 1099511628211ULL

/** 演算子の優先順位です。値が大きいほど強く結合します。 */
#define PRECEDENCE_OR        1
#define PRECEDENCE_AND       2
#define PRECEDENCE_NOT       3
#define PRECEDENCE_PREDICATE 4

/** デコンパイルの出力先です。 */
typedef struct text_writer
{
    char *dest;
    size_t size;
    size_t length;
    bool is_truncated;
    uint8_t pad[7]; /**< 明示的アラインメントです。 */
} text_writer;

/** デコンパイル中の 1 行の状態です。 */
typedef struct decompile_context
{
    const unsigned char *record;
    const unsigned char *constants;
    uint32_t constant_size;
    uint32_t pad; /**< 明示的アラインメントです。 */
    uint16_t left[CPLAT_STRING_CATALOG_FILTER_LINE_WIDTH_MAX];
    uint16_t right[CPLAT_STRING_CATALOG_FILTER_LINE_WIDTH_MAX];
    text_writer writer;
} decompile_context;

/* ===== レイアウトの読み書き ===== */

uint32_t string_catalog_filter_instruction_capacity(const uint32_t line_width)
{
    return line_width;
}

uint32_t string_catalog_filter_constant_capacity(const uint32_t line_width)
{
    return 8U * line_width;
}

unsigned char *string_catalog_filter_record_address(void *image, const uint32_t record_size, const uint32_t line_index)
{
    return (unsigned char *)image + CPLAT_STRING_CATALOG_FILTER_HEADER_SIZE + ((size_t)record_size * line_index);
}

const unsigned char *string_catalog_filter_record_address_const(const void *image, const uint32_t record_size,
                                                                const uint32_t line_index)
{
    return (const unsigned char *)image + CPLAT_STRING_CATALOG_FILTER_HEADER_SIZE + ((size_t)record_size * line_index);
}

void string_catalog_filter_read_image_header(const void *image, string_catalog_filter_image_header *header_out)
{
    memcpy(header_out, image, sizeof(*header_out));
}

void string_catalog_filter_write_image_header(void *image, const string_catalog_filter_image_header *header)
{
    memcpy(image, header, sizeof(*header));
}

void string_catalog_filter_read_record_header(const unsigned char *record,
                                              string_catalog_filter_record_header *header_out)
{
    memcpy(header_out, record, sizeof(*header_out));
}

void string_catalog_filter_write_record_header(unsigned char *record, const string_catalog_filter_record_header *header)
{
    memcpy(record, header, sizeof(*header));
}

void string_catalog_filter_read_instruction(const unsigned char *record, const uint32_t index,
                                            string_catalog_filter_instruction *instruction_out)
{
    memcpy(instruction_out,
           record + CPLAT_STRING_CATALOG_FILTER_RECORD_HEADER_SIZE + ((size_t)index * sizeof(*instruction_out)),
           sizeof(*instruction_out));
}

void string_catalog_filter_write_instruction(unsigned char *record, const uint32_t index,
                                             const string_catalog_filter_instruction *instruction)
{
    memcpy(record + CPLAT_STRING_CATALOG_FILTER_RECORD_HEADER_SIZE + ((size_t)index * sizeof(*instruction)),
           instruction, sizeof(*instruction));
}

const unsigned char *string_catalog_filter_record_constants(const unsigned char *record, const uint32_t line_width)
{
    return record + CPLAT_STRING_CATALOG_FILTER_RECORD_HEADER_SIZE +
           ((size_t)string_catalog_filter_instruction_capacity(line_width) * sizeof(string_catalog_filter_instruction));
}

int string_catalog_filter_read_constant(const unsigned char *constants, const uint32_t constant_size,
                                        const uint32_t offset, string_catalog_filter_constant *constant_out)
{
    string_catalog_filter_constant_header header;
    uint32_t payload_size;
    uint32_t payload_offset;

    if (((offset % STRING_CATALOG_FILTER_CONSTANT_ALIGNMENT) != 0U) || (offset > constant_size) ||
        ((constant_size - offset) < STRING_CATALOG_FILTER_CONSTANT_HEADER_SIZE))
    {
        return CPLAT_ERR_CORRUPT_DESCRIPTOR;
    }

    memcpy(&header, constants + offset, sizeof(header));
    payload_offset = offset + STRING_CATALOG_FILTER_CONSTANT_HEADER_SIZE;

    memset(constant_out, 0, sizeof(*constant_out));
    constant_out->header = header;
    constant_out->offset = offset;

    switch (header.kind)
    {
    case STRING_CATALOG_FILTER_CONSTANT_KIND_INTEGER:
    case STRING_CATALOG_FILTER_CONSTANT_KIND_CHARACTER:
    case STRING_CATALOG_FILTER_CONSTANT_KIND_FLOAT:
        if ((header.length != 0U) || (header.slot != 0U))
        {
            return CPLAT_ERR_CORRUPT_DESCRIPTOR;
        }
        payload_size = STRING_CATALOG_FILTER_CONSTANT_NUMBER_SIZE;
        break;

    case STRING_CATALOG_FILTER_CONSTANT_KIND_NULL:
        if ((header.length != 0U) || (header.slot != 0U) || (header.flags != 0U))
        {
            return CPLAT_ERR_CORRUPT_DESCRIPTOR;
        }
        payload_size = 0U;
        break;

    case STRING_CATALOG_FILTER_CONSTANT_KIND_STRING:
    case STRING_CATALOG_FILTER_CONSTANT_KIND_IDENTIFIER:
    case STRING_CATALOG_FILTER_CONSTANT_KIND_ARGUMENT_NAME:
    case STRING_CATALOG_FILTER_CONSTANT_KIND_PATTERN:
        if ((header.flags != 0U) || (header.length >= constant_size))
        {
            return CPLAT_ERR_CORRUPT_DESCRIPTOR;
        }
        if ((header.kind == (uint8_t)STRING_CATALOG_FILTER_CONSTANT_KIND_STRING) && (header.slot != 0U))
        {
            return CPLAT_ERR_CORRUPT_DESCRIPTOR;
        }
        /* 空の文字列と空のパターンは許可する。空のパターンはすべての文字列に一致する */
        if ((header.kind != (uint8_t)STRING_CATALOG_FILTER_CONSTANT_KIND_STRING) &&
            (header.kind != (uint8_t)STRING_CATALOG_FILTER_CONSTANT_KIND_PATTERN) && (header.length == 0U))
        {
            return CPLAT_ERR_CORRUPT_DESCRIPTOR;
        }
        /* NUL を含めて 8 バイト単位へ切り上げる */
        payload_size =
            ((header.length + STRING_CATALOG_FILTER_CONSTANT_ALIGNMENT) / STRING_CATALOG_FILTER_CONSTANT_ALIGNMENT) *
            STRING_CATALOG_FILTER_CONSTANT_ALIGNMENT;
        break;

    default:
        return CPLAT_ERR_CORRUPT_DESCRIPTOR;
    }

    if ((constant_size - payload_offset) < payload_size)
    {
        return CPLAT_ERR_CORRUPT_DESCRIPTOR;
    }

    switch (header.kind)
    {
    case STRING_CATALOG_FILTER_CONSTANT_KIND_INTEGER:
    case STRING_CATALOG_FILTER_CONSTANT_KIND_CHARACTER:
        memcpy(&constant_out->magnitude, constants + payload_offset, sizeof(constant_out->magnitude));
        if ((header.flags &
             ~(STRING_CATALOG_FILTER_CONSTANT_FLAG_NEGATIVE | STRING_CATALOG_FILTER_CONSTANT_FLAG_HEXADECIMAL)) != 0U)
        {
            return CPLAT_ERR_CORRUPT_DESCRIPTOR;
        }
        if ((header.flags & STRING_CATALOG_FILTER_CONSTANT_FLAG_NEGATIVE) != 0U)
        {
            /* 負の値は、0 ではなく、int64_t で表せる絶対値だけを正規の形とする */
            if ((constant_out->magnitude == 0U) || (constant_out->magnitude > ((uint64_t)INT64_MAX + 1U)) ||
                ((header.flags & STRING_CATALOG_FILTER_CONSTANT_FLAG_HEXADECIMAL) != 0U))
            {
                return CPLAT_ERR_CORRUPT_DESCRIPTOR;
            }
        }
        if ((header.kind == (uint8_t)STRING_CATALOG_FILTER_CONSTANT_KIND_CHARACTER) &&
            (((header.flags & STRING_CATALOG_FILTER_CONSTANT_FLAG_HEXADECIMAL) != 0U) ||
             (constant_out->magnitude > 255U)))
        {
            return CPLAT_ERR_CORRUPT_DESCRIPTOR;
        }
        break;

    case STRING_CATALOG_FILTER_CONSTANT_KIND_FLOAT:
        if (header.flags != 0U)
        {
            return CPLAT_ERR_CORRUPT_DESCRIPTOR;
        }
        memcpy(&constant_out->real, constants + payload_offset, sizeof(constant_out->real));
        break;

    case STRING_CATALOG_FILTER_CONSTANT_KIND_STRING:
    case STRING_CATALOG_FILTER_CONSTANT_KIND_IDENTIFIER:
    case STRING_CATALOG_FILTER_CONSTANT_KIND_ARGUMENT_NAME:
    case STRING_CATALOG_FILTER_CONSTANT_KIND_PATTERN:
        constant_out->text = (const char *)(constants + payload_offset);
        /* 終端の NUL があり、途中に NUL を含まないこと */
        if ((constant_out->text[header.length] != '\0') || (memchr(constant_out->text, '\0', header.length) != NULL))
        {
            return CPLAT_ERR_CORRUPT_DESCRIPTOR;
        }
        break;

    default:
        break;
    }

    constant_out->next_offset = payload_offset + payload_size;
    return CPLAT_OK;
}

/* ===== ハッシュ ===== */

uint64_t string_catalog_filter_hash_bytes(uint64_t hash, const void *data, const size_t size)
{
    const unsigned char *bytes = (const unsigned char *)data;

    for (size_t index = 0; index < size; index++)
    {
        hash ^= bytes[index];
        hash *= FNV_PRIME;
    }

    return hash;
}

uint64_t string_catalog_filter_compute_line_hash(const unsigned char *record, const uint32_t line_width)
{
    string_catalog_filter_record_header header;
    uint64_t hash = FNV_OFFSET_BASIS;

    string_catalog_filter_read_record_header(record, &header);

    /* ハッシュ値自身を除く見出しと、命令と定数の使用範囲を対象とする */
    hash = string_catalog_filter_hash_bytes(hash, record + sizeof(header.line_hash),
                                            CPLAT_STRING_CATALOG_FILTER_RECORD_HEADER_SIZE - sizeof(header.line_hash));
    hash =
        string_catalog_filter_hash_bytes(hash, record + CPLAT_STRING_CATALOG_FILTER_RECORD_HEADER_SIZE,
                                         (size_t)header.instruction_count * sizeof(string_catalog_filter_instruction));
    hash = string_catalog_filter_hash_bytes(hash, string_catalog_filter_record_constants(record, line_width),
                                            header.constant_size);
    return hash;
}

uint64_t string_catalog_filter_compute_content_hash(const void *image, const string_catalog_filter_image_header *header)
{
    uint64_t hash = FNV_OFFSET_BASIS;

    hash = string_catalog_filter_hash_bytes(hash, &header->line_capacity, sizeof(header->line_capacity));
    hash = string_catalog_filter_hash_bytes(hash, &header->line_width, sizeof(header->line_width));
    hash = string_catalog_filter_hash_bytes(hash, &header->line_count, sizeof(header->line_count));
    hash = string_catalog_filter_hash_bytes(hash,
                                            string_catalog_filter_record_address_const(image, header->record_size, 0U),
                                            (size_t)header->record_size * header->line_count);
    return hash;
}

void string_catalog_filter_update_content_hash(void *image, string_catalog_filter_image_header *header)
{
    header->content_hash = string_catalog_filter_compute_content_hash(image, header);
    string_catalog_filter_write_image_header(image, header);
}

/* ===== 検証 ===== */

int string_catalog_filter_check_header(const void *image, const size_t image_size,
                                       string_catalog_filter_image_header *header_out)
{
    string_catalog_filter_image_header header;

    if (image_size < CPLAT_STRING_CATALOG_FILTER_HEADER_SIZE)
    {
        return CPLAT_ERR_CORRUPT_DESCRIPTOR;
    }

    string_catalog_filter_read_image_header(image, &header);

    if ((header.signature != STRING_CATALOG_FILTER_SIGNATURE) ||
        (header.byte_order_mark != STRING_CATALOG_FILTER_BYTE_ORDER_MARK))
    {
        return CPLAT_ERR_CORRUPT_DESCRIPTOR;
    }

    if (header.format_version != STRING_CATALOG_FILTER_FORMAT_VERSION)
    {
        return CPLAT_ERR_VERSION_MISMATCH;
    }

    if ((header.line_width < CPLAT_STRING_CATALOG_FILTER_LINE_WIDTH_MIN) ||
        (header.line_width > CPLAT_STRING_CATALOG_FILTER_LINE_WIDTH_MAX) || (header.line_capacity == 0U) ||
        (header.line_capacity > CPLAT_STRING_CATALOG_FILTER_LINE_MAX) || (header.line_count > header.line_capacity))
    {
        return CPLAT_ERR_CORRUPT_DESCRIPTOR;
    }

    if ((header.record_size != CPLAT_STRING_CATALOG_FILTER_RECORD_SIZE(header.line_width)) ||
        (header.image_size != CPLAT_STRING_CATALOG_FILTER_IMAGE_SIZE(header.line_capacity, header.line_width)) ||
        (header.image_size > image_size))
    {
        return CPLAT_ERR_CORRUPT_DESCRIPTOR;
    }

    for (size_t index = 0; index < sizeof(header.reserved); index++)
    {
        if (header.reserved[index] != 0U)
        {
            return CPLAT_ERR_CORRUPT_DESCRIPTOR;
        }
    }

    *header_out = header;
    return CPLAT_OK;
}

/** 定数の区分です。判定演算子との組み合わせの確認に使用します。 */
typedef enum constant_class
{
    CONSTANT_CLASS_INVALID = 0,
    CONSTANT_CLASS_NUMERIC = 1,
    CONSTANT_CLASS_STRING = 2,
    CONSTANT_CLASS_NULL = 3,
    CONSTANT_CLASS_PATTERN = 4
} constant_class;

static constant_class class_of_constant(const uint8_t kind)
{
    switch (kind)
    {
    case STRING_CATALOG_FILTER_CONSTANT_KIND_INTEGER:
    case STRING_CATALOG_FILTER_CONSTANT_KIND_FLOAT:
    case STRING_CATALOG_FILTER_CONSTANT_KIND_CHARACTER:
    case STRING_CATALOG_FILTER_CONSTANT_KIND_IDENTIFIER:
        return CONSTANT_CLASS_NUMERIC;
    case STRING_CATALOG_FILTER_CONSTANT_KIND_STRING:
        return CONSTANT_CLASS_STRING;
    case STRING_CATALOG_FILTER_CONSTANT_KIND_NULL:
        return CONSTANT_CLASS_NULL;
    case STRING_CATALOG_FILTER_CONSTANT_KIND_PATTERN:
        return CONSTANT_CLASS_PATTERN;
    default:
        return CONSTANT_CLASS_INVALID;
    }
}

static bool is_argument_field(const uint8_t field)
{
    return (field == (uint8_t)STRING_CATALOG_FILTER_FIELD_ARGUMENT_NAME) ||
           (field == (uint8_t)STRING_CATALOG_FILTER_FIELD_ARGUMENT_INDEX);
}

static bool is_pattern_operator(const uint8_t operator_kind)
{
    return (operator_kind == (uint8_t)STRING_CATALOG_FILTER_OPERATOR_MATCHES) ||
           (operator_kind == (uint8_t)STRING_CATALOG_FILTER_OPERATOR_MATCHES_I);
}

static bool is_string_operator(const uint8_t operator_kind)
{
    return (operator_kind >= (uint8_t)STRING_CATALOG_FILTER_OPERATOR_STARTS_WITH) &&
           (operator_kind <= (uint8_t)STRING_CATALOG_FILTER_OPERATOR_CONTAINS_I);
}

/**
 *  @brief          フィールド、判定演算子、定数の区分の組み合わせが成立し得るかを判定します。
 *
 *  コンパイルと検証の双方が使用します。コンパイルで拒否した組み合わせを、検証でも拒否するためです。
 */
static bool is_constant_allowed(const uint8_t field, const uint8_t operator_kind, const uint8_t constant_kind)
{
    const constant_class kind_class = class_of_constant(constant_kind);
    const bool is_equality = (operator_kind == (uint8_t)STRING_CATALOG_FILTER_OPERATOR_EQUAL) ||
                             (operator_kind == (uint8_t)STRING_CATALOG_FILTER_OPERATOR_NOT_EQUAL);

    switch (kind_class)
    {
    case CONSTANT_CLASS_NULL:
        return is_equality && ((field == (uint8_t)STRING_CATALOG_FILTER_FIELD_ID) || is_argument_field(field));

    case CONSTANT_CLASS_STRING:
        if ((field != (uint8_t)STRING_CATALOG_FILTER_FIELD_ID) && !is_argument_field(field))
        {
            return false;
        }
        return is_equality || (operator_kind == (uint8_t)STRING_CATALOG_FILTER_OPERATOR_IN) ||
               is_string_operator(operator_kind);

    case CONSTANT_CLASS_PATTERN:
        /* 正規表現のパターンは matches と matches_i だけが受け取り、それらはパターンだけを受け取る */
        return is_pattern_operator(operator_kind) &&
               ((field == (uint8_t)STRING_CATALOG_FILTER_FIELD_ID) || is_argument_field(field));

    case CONSTANT_CLASS_NUMERIC:
        if (field == (uint8_t)STRING_CATALOG_FILTER_FIELD_ID)
        {
            return false;
        }
        return (operator_kind >= (uint8_t)STRING_CATALOG_FILTER_OPERATOR_EQUAL) &&
               (operator_kind <= (uint8_t)STRING_CATALOG_FILTER_OPERATOR_BETWEEN);

    case CONSTANT_CLASS_INVALID:
    default:
        return false;
    }
}

/* コンパイラーから同じ規則を使用するための入口 */
int string_catalog_filter_is_constant_allowed(const uint8_t field, const uint8_t operator_kind,
                                              const uint8_t constant_kind)
{
    return is_constant_allowed(field, operator_kind, constant_kind);
}

/** 判定演算子が要求する定数の数を確認します。 */
static bool is_operand_count_valid(const uint8_t operator_kind, const uint16_t operand_count)
{
    switch (operator_kind)
    {
    case STRING_CATALOG_FILTER_OPERATOR_HAS:
        return operand_count == 0U;
    case STRING_CATALOG_FILTER_OPERATOR_BETWEEN:
        return operand_count == 2U;
    case STRING_CATALOG_FILTER_OPERATOR_IN:
        return operand_count >= 1U;
    default:
        return operand_count == 1U;
    }
}

/** 判定要素 1 件の参照先と型の組み合わせを確認します。 */
static int check_predicate(const string_catalog_filter_instruction *instruction,
                           const string_catalog_filter_record_header *header, const unsigned char *constants,
                           const uint8_t *constant_starts)
{
    string_catalog_filter_constant constant;
    uint32_t offset = instruction->operand;
    uint8_t first_class = (uint8_t)CONSTANT_CLASS_INVALID;

    if ((instruction->field < (uint8_t)STRING_CATALOG_FILTER_FIELD_KEY) ||
        (instruction->field > (uint8_t)STRING_CATALOG_FILTER_FIELD_ARGUMENT_INDEX) ||
        (instruction->operator_kind < (uint8_t)STRING_CATALOG_FILTER_OPERATOR_EQUAL) ||
        (instruction->operator_kind > (uint8_t)STRING_CATALOG_FILTER_OPERATOR_MATCHES_I) ||
        !is_operand_count_valid(instruction->operator_kind, instruction->operand_count))
    {
        return CPLAT_ERR_CORRUPT_DESCRIPTOR;
    }

    if ((instruction->operator_kind == (uint8_t)STRING_CATALOG_FILTER_OPERATOR_HAS) &&
        !is_argument_field(instruction->field))
    {
        return CPLAT_ERR_CORRUPT_DESCRIPTOR;
    }

    switch (instruction->field)
    {
    case STRING_CATALOG_FILTER_FIELD_ARGUMENT_INDEX:
        if (instruction->argument >= CPLAT_STRING_CATALOG_ARGUMENT_MAX)
        {
            return CPLAT_ERR_CORRUPT_DESCRIPTOR;
        }
        break;

    case STRING_CATALOG_FILTER_FIELD_ARGUMENT_NAME:
        /* 引数名の定数が先頭にあり、比較対象の定数はその直後から並ぶ */
        if ((instruction->argument >= header->argument_reference_count) || (offset >= header->constant_size) ||
            (constant_starts[offset / STRING_CATALOG_FILTER_CONSTANT_ALIGNMENT] == 0U) ||
            (string_catalog_filter_read_constant(constants, header->constant_size, offset, &constant) != CPLAT_OK) ||
            (constant.header.kind != (uint8_t)STRING_CATALOG_FILTER_CONSTANT_KIND_ARGUMENT_NAME) ||
            (constant.header.slot != instruction->argument))
        {
            return CPLAT_ERR_CORRUPT_DESCRIPTOR;
        }
        offset = constant.next_offset;
        break;

    default:
        if (instruction->argument != 0U)
        {
            return CPLAT_ERR_CORRUPT_DESCRIPTOR;
        }
        break;
    }

    /* 比較対象を持たない判定要素は、operand を使用しない。正規の形として 0 を求める */
    if ((instruction->operand_count == 0U) &&
        (instruction->field != (uint8_t)STRING_CATALOG_FILTER_FIELD_ARGUMENT_NAME) && (instruction->operand != 0U))
    {
        return CPLAT_ERR_CORRUPT_DESCRIPTOR;
    }

    for (uint16_t index = 0; index < instruction->operand_count; index++)
    {
        if ((offset >= header->constant_size) ||
            (constant_starts[offset / STRING_CATALOG_FILTER_CONSTANT_ALIGNMENT] == 0U) ||
            (string_catalog_filter_read_constant(constants, header->constant_size, offset, &constant) != CPLAT_OK))
        {
            return CPLAT_ERR_CORRUPT_DESCRIPTOR;
        }

        if (!is_constant_allowed(instruction->field, instruction->operator_kind, constant.header.kind))
        {
            return CPLAT_ERR_CORRUPT_DESCRIPTOR;
        }

        /* in の要素は区分をそろえる */
        if (index == 0U)
        {
            first_class = (uint8_t)class_of_constant(constant.header.kind);
        }
        else if ((uint8_t)class_of_constant(constant.header.kind) != first_class)
        {
            return CPLAT_ERR_CORRUPT_DESCRIPTOR;
        }

        if ((constant.header.kind == (uint8_t)STRING_CATALOG_FILTER_CONSTANT_KIND_IDENTIFIER) &&
            (constant.header.slot >= header->identifier_count))
        {
            return CPLAT_ERR_CORRUPT_DESCRIPTOR;
        }
        if ((constant.header.kind == (uint8_t)STRING_CATALOG_FILTER_CONSTANT_KIND_PATTERN) &&
            (constant.header.slot >= header->pattern_count))
        {
            return CPLAT_ERR_CORRUPT_DESCRIPTOR;
        }

        offset = constant.next_offset;
    }

    return CPLAT_OK;
}

int string_catalog_filter_check_record(const unsigned char *record, const uint32_t line_width)
{
    string_catalog_filter_record_header header;
    string_catalog_filter_instruction instruction;
    string_catalog_filter_constant constant;
    const unsigned char *constants = string_catalog_filter_record_constants(record, line_width);
    uint8_t constant_starts[CPLAT_STRING_CATALOG_FILTER_LINE_WIDTH_MAX] = {0};
    int16_t depth_before[CPLAT_STRING_CATALOG_FILTER_LINE_WIDTH_MAX + 1U];
    uint32_t offset = 0U;
    int depth = 0;
    int max_depth = 0;

    string_catalog_filter_read_record_header(record, &header);

    if ((header.instruction_count == 0U) ||
        (header.instruction_count > string_catalog_filter_instruction_capacity(line_width)) ||
        (header.constant_size > string_catalog_filter_constant_capacity(line_width)) ||
        ((header.constant_size % STRING_CATALOG_FILTER_CONSTANT_ALIGNMENT) != 0U) ||
        (header.pattern_count > CPLAT_STRING_CATALOG_FILTER_PATTERN_REFERENCE_MAX) ||
        (header.argument_reference_count > CPLAT_STRING_CATALOG_FILTER_ARGUMENT_REFERENCE_MAX) ||
        (header.identifier_count > CPLAT_STRING_CATALOG_FILTER_IDENTIFIER_REFERENCE_MAX) || (header.stack_depth == 0U))
    {
        return CPLAT_ERR_CORRUPT_DESCRIPTOR;
    }

    /* 定数を先頭から順にたどり、各定数の開始位置を記録する */
    while (offset < header.constant_size)
    {
        if (string_catalog_filter_read_constant(constants, header.constant_size, offset, &constant) != CPLAT_OK)
        {
            return CPLAT_ERR_CORRUPT_DESCRIPTOR;
        }
        if ((constant.header.kind == (uint8_t)STRING_CATALOG_FILTER_CONSTANT_KIND_ARGUMENT_NAME) &&
            (constant.header.slot >= header.argument_reference_count))
        {
            return CPLAT_ERR_CORRUPT_DESCRIPTOR;
        }
        if ((constant.header.kind == (uint8_t)STRING_CATALOG_FILTER_CONSTANT_KIND_IDENTIFIER) &&
            (constant.header.slot >= header.identifier_count))
        {
            return CPLAT_ERR_CORRUPT_DESCRIPTOR;
        }
        if ((constant.header.kind == (uint8_t)STRING_CATALOG_FILTER_CONSTANT_KIND_PATTERN) &&
            (constant.header.slot >= header.pattern_count))
        {
            return CPLAT_ERR_CORRUPT_DESCRIPTOR;
        }
        constant_starts[offset / STRING_CATALOG_FILTER_CONSTANT_ALIGNMENT] = 1U;
        offset = constant.next_offset;
    }

    /* 命令列を先頭から評価したときのスタックの深さを追跡する */
    for (uint32_t index = 0; index < header.instruction_count; index++)
    {
        string_catalog_filter_read_instruction(record, index, &instruction);
        depth_before[index] = (int16_t)depth;

        switch (instruction.opcode)
        {
        case STRING_CATALOG_FILTER_OPCODE_PREDICATE:
            if (check_predicate(&instruction, &header, constants, constant_starts) != CPLAT_OK)
            {
                return CPLAT_ERR_CORRUPT_DESCRIPTOR;
            }
            depth++;
            break;

        case STRING_CATALOG_FILTER_OPCODE_NOT:
        case STRING_CATALOG_FILTER_OPCODE_AND:
        case STRING_CATALOG_FILTER_OPCODE_OR:
        case STRING_CATALOG_FILTER_OPCODE_JUMP_IF_FALSE:
        case STRING_CATALOG_FILTER_OPCODE_JUMP_IF_TRUE:
            if ((instruction.field != 0U) || (instruction.operator_kind != 0U) || (instruction.argument != 0U) ||
                (instruction.operand_count != 0U))
            {
                return CPLAT_ERR_CORRUPT_DESCRIPTOR;
            }
            if ((instruction.opcode == (uint8_t)STRING_CATALOG_FILTER_OPCODE_AND) ||
                (instruction.opcode == (uint8_t)STRING_CATALOG_FILTER_OPCODE_OR))
            {
                if (depth < 2)
                {
                    return CPLAT_ERR_CORRUPT_DESCRIPTOR;
                }
                depth--;
            }
            else if (depth < 1)
            {
                return CPLAT_ERR_CORRUPT_DESCRIPTOR;
            }

            if ((instruction.opcode == (uint8_t)STRING_CATALOG_FILTER_OPCODE_JUMP_IF_FALSE) ||
                (instruction.opcode == (uint8_t)STRING_CATALOG_FILTER_OPCODE_JUMP_IF_TRUE))
            {
                /* ジャンプは前方のみ。移動先は命令列の末尾を含む */
                if ((instruction.operand <= index) || (instruction.operand > header.instruction_count))
                {
                    return CPLAT_ERR_CORRUPT_DESCRIPTOR;
                }
            }
            else if (instruction.operand != 0U)
            {
                return CPLAT_ERR_CORRUPT_DESCRIPTOR;
            }
            break;

        default:
            return CPLAT_ERR_CORRUPT_DESCRIPTOR;
        }

        if (depth > max_depth)
        {
            max_depth = depth;
        }
    }
    depth_before[header.instruction_count] = (int16_t)depth;

    if ((depth != 1) || (max_depth != (int)header.stack_depth))
    {
        return CPLAT_ERR_CORRUPT_DESCRIPTOR;
    }

    /* ジャンプした先で、ジャンプしなかった場合と同じ深さになること */
    for (uint32_t index = 0; index < header.instruction_count; index++)
    {
        string_catalog_filter_read_instruction(record, index, &instruction);
        if (((instruction.opcode == (uint8_t)STRING_CATALOG_FILTER_OPCODE_JUMP_IF_FALSE) ||
             (instruction.opcode == (uint8_t)STRING_CATALOG_FILTER_OPCODE_JUMP_IF_TRUE)) &&
            (depth_before[instruction.operand] != depth_before[index]))
        {
            return CPLAT_ERR_CORRUPT_DESCRIPTOR;
        }
    }

    if (header.line_hash != string_catalog_filter_compute_line_hash(record, line_width))
    {
        return CPLAT_ERR_CORRUPT_DESCRIPTOR;
    }

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_string_catalog_filter_validate(const void *image, const size_t image_size)
{
    string_catalog_filter_image_header header;
    int ret;

    if (image == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    ret = string_catalog_filter_check_header(image, image_size, &header);
    if (ret != CPLAT_OK)
    {
        return ret;
    }

    for (uint32_t index = 0; index < header.line_count; index++)
    {
        if (string_catalog_filter_check_record(
                string_catalog_filter_record_address_const(image, header.record_size, index), header.line_width) !=
            CPLAT_OK)
        {
            return CPLAT_ERR_CORRUPT_DESCRIPTOR;
        }
    }

    if (header.content_hash != string_catalog_filter_compute_content_hash(image, &header))
    {
        return CPLAT_ERR_CORRUPT_DESCRIPTOR;
    }

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_string_catalog_filter_get_info(const void *image, const size_t image_size,
                                         cplat_string_catalog_filter_info *info_out)
{
    string_catalog_filter_image_header header;
    int ret;

    if ((image == NULL) || (info_out == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    ret = cplat_string_catalog_filter_validate(image, image_size);
    if (ret != CPLAT_OK)
    {
        return ret;
    }

    string_catalog_filter_read_image_header(image, &header);
    info_out->image_size = header.image_size;
    info_out->content_hash = header.content_hash;
    info_out->line_capacity = header.line_capacity;
    info_out->line_width = header.line_width;
    info_out->line_count = header.line_count;
    info_out->format_version = header.format_version;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_string_catalog_filter_remove_line(void *image, const size_t image_size, const size_t line_index)
{
    string_catalog_filter_image_header header;
    unsigned char *target;
    int ret;

    if (image == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    ret = cplat_string_catalog_filter_validate(image, image_size);
    if (ret != CPLAT_OK)
    {
        return ret;
    }

    string_catalog_filter_read_image_header(image, &header);
    if (line_index >= header.line_count)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    /* 後続の行を 1 つ前へ詰め、末尾を 0 で埋める */
    target = string_catalog_filter_record_address(image, header.record_size, (uint32_t)line_index);
    memmove(target, target + header.record_size,
            (size_t)header.record_size * ((size_t)header.line_count - line_index - 1U));
    memset(string_catalog_filter_record_address(image, header.record_size, header.line_count - 1U), 0,
           header.record_size);

    header.line_count--;
    string_catalog_filter_update_content_hash(image, &header);
    return CPLAT_OK;
}

/* ===== デコンパイル ===== */

static void writer_append(text_writer *writer, const char *text, const size_t length)
{
    size_t available;
    size_t copy_length = length;

    if (writer->length >= (writer->size - 1U))
    {
        if (length > 0U)
        {
            writer->is_truncated = true;
        }
        return;
    }

    available = writer->size - 1U - writer->length;
    if (copy_length > available)
    {
        copy_length = available;
        writer->is_truncated = true;
    }

    memcpy(writer->dest + writer->length, text, copy_length);
    writer->length += copy_length;
    writer->dest[writer->length] = '\0';
}

static void writer_append_text(text_writer *writer, const char *text)
{
    writer_append(writer, text, strlen(text));
}

/** 1 バイトを、引用符の内側の表記で書き出します。 */
static void writer_append_escaped_byte(text_writer *writer, const unsigned char byte, const char quote)
{
    char buffer[8];

    if ((byte == (unsigned char)quote) || (byte == (unsigned char)'\\'))
    {
        buffer[0] = '\\';
        buffer[1] = (char)byte;
        writer_append(writer, buffer, 2U);
    }
    else if (byte == (unsigned char)'\n')
    {
        writer_append(writer, "\\n", 2U);
    }
    else if (byte == (unsigned char)'\t')
    {
        writer_append(writer, "\\t", 2U);
    }
    else if ((byte < 0x20U) || (byte == 0x7FU))
    {
        (void)snprintf(buffer, sizeof(buffer), "\\x%02X", (unsigned int)byte);
        writer_append(writer, buffer, 4U);
    }
    else
    {
        buffer[0] = (char)byte;
        writer_append(writer, buffer, 1U);
    }
}

static void write_constant(text_writer *writer, const string_catalog_filter_constant *constant)
{
    char buffer[64];

    switch (constant->header.kind)
    {
    case STRING_CATALOG_FILTER_CONSTANT_KIND_INTEGER:
        if ((constant->header.flags & STRING_CATALOG_FILTER_CONSTANT_FLAG_HEXADECIMAL) != 0U)
        {
            (void)snprintf(buffer, sizeof(buffer), "0x%" PRIX64, constant->magnitude);
        }
        else if ((constant->header.flags & STRING_CATALOG_FILTER_CONSTANT_FLAG_NEGATIVE) != 0U)
        {
            (void)snprintf(buffer, sizeof(buffer), "-%" PRIu64, constant->magnitude);
        }
        else
        {
            (void)snprintf(buffer, sizeof(buffer), "%" PRIu64, constant->magnitude);
        }
        writer_append_text(writer, buffer);
        break;

    case STRING_CATALOG_FILTER_CONSTANT_KIND_FLOAT:
        /* 元の値へ戻せる最短の桁数で書き出す。17 桁あれば倍精度の値は必ず元へ戻る */
        for (int precision = 15; precision <= 17; precision++)
        {
            (void)snprintf(buffer, sizeof(buffer), "%.*g", precision, constant->real);
            if (strtod(buffer, NULL) == constant->real)
            {
                break;
            }
        }
        /* 整数の表記にならないよう、小数点か指数部を必ず含める */
        if (strpbrk(buffer, ".eE") == NULL)
        {
            const size_t length = strlen(buffer);

            buffer[length] = '.';
            buffer[length + 1U] = '0';
            buffer[length + 2U] = '\0';
        }
        writer_append_text(writer, buffer);
        break;

    case STRING_CATALOG_FILTER_CONSTANT_KIND_CHARACTER:
    {
        /* 格納値は (int)(char) の値。char へ戻したバイトを表記する */
        int value = (int)constant->magnitude;
        if ((constant->header.flags & STRING_CATALOG_FILTER_CONSTANT_FLAG_NEGATIVE) != 0U)
        {
            value = -value;
        }
        writer_append(writer, "'", 1U);
        writer_append_escaped_byte(writer, (unsigned char)(char)value, '\'');
        writer_append(writer, "'", 1U);
        break;
    }

    case STRING_CATALOG_FILTER_CONSTANT_KIND_STRING:
    case STRING_CATALOG_FILTER_CONSTANT_KIND_PATTERN:
        writer_append(writer, "\"", 1U);
        for (uint32_t index = 0; index < constant->header.length; index++)
        {
            writer_append_escaped_byte(writer, (unsigned char)constant->text[index], '"');
        }
        writer_append(writer, "\"", 1U);
        break;

    case STRING_CATALOG_FILTER_CONSTANT_KIND_NULL:
        writer_append_text(writer, "null");
        break;

    case STRING_CATALOG_FILTER_CONSTANT_KIND_IDENTIFIER:
    case STRING_CATALOG_FILTER_CONSTANT_KIND_ARGUMENT_NAME:
        writer_append(writer, constant->text, constant->header.length);
        break;

    default:
        break;
    }
}

static const char *operator_text(const uint8_t operator_kind)
{
    static const char *const texts[] = {
        "",
        "==",
        "!=",
        "<",
        "<=",
        ">",
        ">=",
        "in",
        "between",
        "has",
        "starts_with",
        "ends_with",
        "contains",
        "starts_with_i",
        "ends_with_i",
        "contains_i",
        "matches",
        "matches_i",
    };

    if (operator_kind >= (sizeof(texts) / sizeof(texts[0])))
    {
        return "";
    }
    return texts[operator_kind];
}

static void write_predicate(decompile_context *context, const string_catalog_filter_instruction *instruction)
{
    string_catalog_filter_constant constant;
    char buffer[32];
    uint32_t offset = instruction->operand;

    if (instruction->operator_kind == (uint8_t)STRING_CATALOG_FILTER_OPERATOR_HAS)
    {
        writer_append_text(&context->writer, "has(");
    }

    switch (instruction->field)
    {
    case STRING_CATALOG_FILTER_FIELD_KEY:
        writer_append_text(&context->writer, "key");
        break;
    case STRING_CATALOG_FILTER_FIELD_ID:
        writer_append_text(&context->writer, "id");
        break;
    case STRING_CATALOG_FILTER_FIELD_CATEGORY:
        writer_append_text(&context->writer, "category");
        break;
    case STRING_CATALOG_FILTER_FIELD_ARGUMENT_NAME:
        (void)string_catalog_filter_read_constant(context->constants, context->constant_size, offset, &constant);
        writer_append_text(&context->writer, "arg.");
        write_constant(&context->writer, &constant);
        offset = constant.next_offset;
        break;
    case STRING_CATALOG_FILTER_FIELD_ARGUMENT_INDEX:
        (void)snprintf(buffer, sizeof(buffer), "arg[%u]", (unsigned int)instruction->argument);
        writer_append_text(&context->writer, buffer);
        break;
    default:
        break;
    }

    if (instruction->operator_kind == (uint8_t)STRING_CATALOG_FILTER_OPERATOR_HAS)
    {
        writer_append_text(&context->writer, ")");
        return;
    }

    writer_append_text(&context->writer, " ");
    writer_append_text(&context->writer, operator_text(instruction->operator_kind));
    writer_append_text(&context->writer, " ");

    if (instruction->operator_kind == (uint8_t)STRING_CATALOG_FILTER_OPERATOR_IN)
    {
        writer_append_text(&context->writer, "[");
    }

    for (uint16_t index = 0; index < instruction->operand_count; index++)
    {
        if (index > 0U)
        {
            if (instruction->operator_kind == (uint8_t)STRING_CATALOG_FILTER_OPERATOR_BETWEEN)
            {
                writer_append_text(&context->writer, " and ");
            }
            else
            {
                writer_append_text(&context->writer, ", ");
            }
        }
        (void)string_catalog_filter_read_constant(context->constants, context->constant_size, offset, &constant);
        write_constant(&context->writer, &constant);
        offset = constant.next_offset;
    }

    if (instruction->operator_kind == (uint8_t)STRING_CATALOG_FILTER_OPERATOR_IN)
    {
        writer_append_text(&context->writer, "]");
    }
}

static int precedence_of(const uint8_t opcode)
{
    switch (opcode)
    {
    case STRING_CATALOG_FILTER_OPCODE_OR:
        return PRECEDENCE_OR;
    case STRING_CATALOG_FILTER_OPCODE_AND:
        return PRECEDENCE_AND;
    case STRING_CATALOG_FILTER_OPCODE_NOT:
        return PRECEDENCE_NOT;
    default:
        return PRECEDENCE_PREDICATE;
    }
}

/**
 *  @brief          命令の木を中置記法で書き出します。
 *
 *  2 項演算子の右側に同じ優先順位の演算子がある場合は括弧を付けます。
 *  構文解析は左結合で木を作るため、括弧を省くと再コンパイルで木の形が変わるためです。
 *
 *  `!` の対象が判定要素の場合も括弧を付けます。`!key == 1` は構文上 `!(key == 1)` ですが、
 *  C の読み方では `(!key) == 1` と誤読されるためです。括弧は命令を生成しないため、再コンパイルの結果は変わりません。
 */
static void write_node(decompile_context *context, const uint16_t node, const int parent_precedence,
                       const bool is_right_operand)
{
    string_catalog_filter_instruction instruction;
    int precedence;
    bool needs_parenthesis;

    string_catalog_filter_read_instruction(context->record, node, &instruction);
    precedence = precedence_of(instruction.opcode);
    needs_parenthesis = (precedence < parent_precedence) ||
                        (is_right_operand && (precedence == parent_precedence) && (precedence <= PRECEDENCE_AND)) ||
                        ((parent_precedence == PRECEDENCE_NOT) && (precedence == PRECEDENCE_PREDICATE));

    if (needs_parenthesis)
    {
        writer_append_text(&context->writer, "(");
    }

    switch (instruction.opcode)
    {
    case STRING_CATALOG_FILTER_OPCODE_PREDICATE:
        write_predicate(context, &instruction);
        break;

    case STRING_CATALOG_FILTER_OPCODE_NOT:
        writer_append_text(&context->writer, "!");
        write_node(context, context->left[node], precedence, false);
        break;

    case STRING_CATALOG_FILTER_OPCODE_AND:
    case STRING_CATALOG_FILTER_OPCODE_OR:
        write_node(context, context->left[node], precedence, false);
        if (instruction.opcode == (uint8_t)STRING_CATALOG_FILTER_OPCODE_AND)
        {
            writer_append_text(&context->writer, " && ");
        }
        else
        {
            writer_append_text(&context->writer, " || ");
        }
        write_node(context, context->right[node], precedence, true);
        break;

    default:
        break;
    }

    if (needs_parenthesis)
    {
        writer_append_text(&context->writer, ")");
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

uint16_t string_catalog_filter_build_tree(const unsigned char *record, uint16_t *left, uint16_t *right)
{
    string_catalog_filter_record_header record_header;
    string_catalog_filter_instruction instruction;
    uint16_t stack[CPLAT_STRING_CATALOG_FILTER_LINE_WIDTH_MAX];
    size_t depth = 0U;

    string_catalog_filter_read_record_header(record, &record_header);

    /* ジャンプは短絡評価のためだけの命令であり、木には現れない */
    for (uint16_t index = 0; index < record_header.instruction_count; index++)
    {
        string_catalog_filter_read_instruction(record, index, &instruction);
        switch (instruction.opcode)
        {
        case STRING_CATALOG_FILTER_OPCODE_PREDICATE:
            stack[depth++] = index;
            break;
        case STRING_CATALOG_FILTER_OPCODE_NOT:
            left[index] = stack[depth - 1U];
            stack[depth - 1U] = index;
            break;
        case STRING_CATALOG_FILTER_OPCODE_AND:
        case STRING_CATALOG_FILTER_OPCODE_OR:
            right[index] = stack[depth - 1U];
            left[index] = stack[depth - 2U];
            depth--;
            stack[depth - 1U] = index;
            break;
        default:
            break;
        }
    }

    /* 検証済みのため、深さは 1 に戻っている */
    return stack[0];
}

/* Doxygen コメントは、ヘッダーに記載 */

int string_catalog_filter_format_constant(const string_catalog_filter_constant *constant, char *dest,
                                          const size_t dest_size)
{
    text_writer writer;

    memset(&writer, 0, sizeof(writer));
    writer.dest = dest;
    writer.size = dest_size;
    dest[0] = '\0';
    write_constant(&writer, constant);
    if (writer.is_truncated)
    {
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_string_catalog_filter_decompile_line(const void *image, const size_t image_size, const size_t line_index,
                                               char *dest, const size_t dest_size)
{
    string_catalog_filter_image_header header;
    string_catalog_filter_record_header record_header;
    decompile_context context;
    int ret;

    if ((image == NULL) || (dest == NULL) || (dest_size == 0U))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    dest[0] = '\0';

    ret = string_catalog_filter_check_header(image, image_size, &header);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    if (line_index >= header.line_count)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    memset(&context, 0, sizeof(context));
    context.record = string_catalog_filter_record_address_const(image, header.record_size, (uint32_t)line_index);
    if (string_catalog_filter_check_record(context.record, header.line_width) != CPLAT_OK)
    {
        return CPLAT_ERR_CORRUPT_DESCRIPTOR;
    }

    string_catalog_filter_read_record_header(context.record, &record_header);
    context.constants = string_catalog_filter_record_constants(context.record, header.line_width);
    context.constant_size = record_header.constant_size;
    context.writer.dest = dest;
    context.writer.size = dest_size;

    write_node(&context, string_catalog_filter_build_tree(context.record, context.left, context.right), 0, false);

    if (context.writer.is_truncated)
    {
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }
    return CPLAT_OK;
}
