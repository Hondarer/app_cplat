/**
 *******************************************************************************
 *  @file           filter_compile.c
 *  @brief          条件式のコンパイルと、行単位の編集を実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/09/26
 *  @version        0.1.0
 *
 *  構文解析は再帰下降で行い、後置記法の命令列を直接生成します。\n
 *  `&&` と `||` は、短絡評価のためのジャンプ命令と、結合の命令の 2 つに展開します。
 *
 *  @code{.text}
 *  a && b   →   [a] JUMP_IF_FALSE(L) [b] AND L:
 *  a || b   →   [a] JUMP_IF_TRUE(L)  [b] OR  L:
 *  @endcode
 *
 *  ジャンプした場合は左辺の真偽がスタックに残り、そのまま式の値になります。\n
 *  ジャンプしない場合は右辺を評価し、結合の命令が 2 つの真偽を 1 つにします。\n
 *  事前計算 (部分評価) ではジャンプを無視し、結合の命令で 3 値の論理演算を行います。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *******************************************************************************
 */

#include "filter.h"

#include <cplat/base/result.h>

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* 行レコードのバイト数はヘッダーへ 32 ビットで格納するため、行幅の上限で収まることを固定する */
_Static_assert(CPLAT_STRING_CATALOG_FILTER_RECORD_SIZE(CPLAT_STRING_CATALOG_FILTER_LINE_WIDTH_MAX) <= UINT32_MAX,
               "cplat: filter record size must fit in uint32_t");

/** 字句の種類です。 */
typedef enum token_kind
{
    TOKEN_KIND_END = 0,
    TOKEN_KIND_IDENTIFIER,
    TOKEN_KIND_INTEGER,
    TOKEN_KIND_FLOAT,
    TOKEN_KIND_STRING,
    TOKEN_KIND_CHARACTER,
    TOKEN_KIND_LEFT_PARENTHESIS,
    TOKEN_KIND_RIGHT_PARENTHESIS,
    TOKEN_KIND_LEFT_BRACKET,
    TOKEN_KIND_RIGHT_BRACKET,
    TOKEN_KIND_COMMA,
    TOKEN_KIND_DOT,
    TOKEN_KIND_EQUAL,
    TOKEN_KIND_NOT_EQUAL,
    TOKEN_KIND_LESS,
    TOKEN_KIND_LESS_EQUAL,
    TOKEN_KIND_GREATER,
    TOKEN_KIND_GREATER_EQUAL,
    TOKEN_KIND_AND,
    TOKEN_KIND_OR,
    TOKEN_KIND_NOT
} token_kind;

/** 字句 1 個です。 */
typedef struct token
{
    uint64_t magnitude;   /**< 整数と文字の絶対値。 */
    double real;          /**< 浮動小数点数の値。 */
    token_kind kind;      /**< 字句の種類。 */
    uint32_t column;      /**< 行内の開始位置。 */
    uint32_t length;      /**< 元の表記のバイト数。 */
    uint32_t text_length; /**< 文字列の字句を復号したバイト数。 */
    uint8_t flags;        /**< 整数と文字の STRING_CATALOG_FILTER_CONSTANT_FLAG_*。 */
    uint8_t pad[7];       /**< 明示的アラインメントです。 */
} token;

/** 1 行のコンパイルの状態です。 */
typedef struct compiler
{
    const char *text;
    size_t text_length;
    size_t position;
    unsigned char *record;
    token current;

    uint32_t line_width;
    uint32_t instruction_capacity;
    uint32_t constant_capacity;
    uint32_t instruction_count;
    uint32_t constant_size;

    int depth;
    int max_depth;
    uint32_t predicate_count;
    uint32_t nesting;
    uint32_t argument_reference_count;
    uint16_t argument_name_offsets[CPLAT_STRING_CATALOG_FILTER_ARGUMENT_REFERENCE_MAX];
    uint32_t identifier_count;

    cplat_string_catalog_filter_line_error error;
    uint32_t error_column;
    uint32_t pattern_count; /**< 正規表現のパターンの定数の数。 */

    /** 文字列の字句の復号結果。NUL の 1 バイトを含め、8 バイト単位に切り上げた大きさです。 */
    char string_buffer[CPLAT_STRING_CATALOG_FILTER_LINE_WIDTH_MAX + 8U];
} compiler;

/* ===== 字句解析 ===== */

static bool is_space(const char character)
{
    return (character == ' ') || (character == '\t') || (character == '\r') || (character == '\n') ||
           (character == '\v') || (character == '\f');
}

static bool is_identifier_start(const char character)
{
    return ((character >= 'a') && (character <= 'z')) || ((character >= 'A') && (character <= 'Z')) ||
           (character == '_');
}

static bool is_digit(const char character)
{
    return (character >= '0') && (character <= '9');
}

static bool is_identifier_part(const char character)
{
    return is_identifier_start(character) || is_digit(character);
}

static int hex_value(const char character)
{
    if (is_digit(character))
    {
        return character - '0';
    }
    if ((character >= 'a') && (character <= 'f'))
    {
        return (character - 'a') + 10;
    }
    if ((character >= 'A') && (character <= 'F'))
    {
        return (character - 'A') + 10;
    }
    return -1;
}

/** 最初の誤りだけを記録します。 */
static bool fail(compiler *state, const cplat_string_catalog_filter_line_error error, const size_t column)
{
    if (state->error == CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE)
    {
        state->error = error;
        state->error_column = (uint32_t)column;
    }
    return false;
}

static char peek_at(const compiler *state, const size_t position)
{
    if (position >= state->text_length)
    {
        return '\0';
    }
    return state->text[position];
}

/**
 *  @brief          引用符の内側の 1 文字を復号します。
 *  @return         復号したバイト。誤りの場合は -1。
 */
static int read_quoted_byte(compiler *state)
{
    const char character = peek_at(state, state->position);
    int high;
    int low;

    if (character != '\\')
    {
        state->position++;
        return (unsigned char)character;
    }

    switch (peek_at(state, state->position + 1U))
    {
    case '"':
    case '\'':
    case '\\':
        state->position += 2U;
        return (unsigned char)state->text[state->position - 1U];
    case 'n':
        state->position += 2U;
        return '\n';
    case 't':
        state->position += 2U;
        return '\t';
    case 'x':
        high = hex_value(peek_at(state, state->position + 2U));
        low = hex_value(peek_at(state, state->position + 3U));
        /* NUL は文字列を途中で終わらせるため受け付けない */
        if ((high < 0) || (low < 0) || ((high == 0) && (low == 0)))
        {
            return -1;
        }
        state->position += 4U;
        return (high * 16) + low;
    default:
        return -1;
    }
}

static bool lex_string(compiler *state, token *result)
{
    uint32_t length = 0U;

    state->position++; /* 開きの引用符 */
    for (;;)
    {
        int byte;

        if (state->position >= state->text_length)
        {
            return fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_LEXICAL, result->column);
        }
        if (state->text[state->position] == '"')
        {
            state->position++;
            break;
        }
        byte = read_quoted_byte(state);
        if (byte < 0)
        {
            return fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_LEXICAL, state->position);
        }
        state->string_buffer[length++] = (char)byte;
    }

    state->string_buffer[length] = '\0';
    result->kind = TOKEN_KIND_STRING;
    result->text_length = length;
    return true;
}

static bool lex_character(compiler *state, token *result)
{
    int byte;
    int value;

    state->position++; /* 開きの引用符 */
    if ((state->position >= state->text_length) || (state->text[state->position] == '\''))
    {
        return fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_LEXICAL, result->column);
    }

    byte = read_quoted_byte(state);
    if ((byte < 0) || (peek_at(state, state->position) != '\''))
    {
        return fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_LEXICAL, result->column);
    }
    state->position++;

    /* CHAR 型の引数と同じく、char へ変換した値を比較に使用する */
    value = (int)(char)byte;
    result->kind = TOKEN_KIND_CHARACTER;
    if (value < 0)
    {
        result->flags = STRING_CATALOG_FILTER_CONSTANT_FLAG_NEGATIVE;
        result->magnitude = (uint64_t)(-value);
    }
    else
    {
        result->magnitude = (uint64_t)value;
    }
    return true;
}

static bool lex_number(compiler *state, token *result)
{
    const size_t start = state->position;
    const bool is_negative = (peek_at(state, start) == '-');
    size_t position = start;
    uint64_t magnitude = 0U;

    if (is_negative)
    {
        position++;
    }

    if ((peek_at(state, position) == '0') &&
        ((peek_at(state, position + 1U) == 'x') || (peek_at(state, position + 1U) == 'X')))
    {
        size_t digits = 0U;

        /* 16 進数は符号を持たない */
        if (is_negative)
        {
            return fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_LEXICAL, start);
        }
        position += 2U;
        while (hex_value(peek_at(state, position)) >= 0)
        {
            if (magnitude > (UINT64_MAX >> 4))
            {
                return fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_LEXICAL, start);
            }
            magnitude = (magnitude << 4) | (uint64_t)hex_value(peek_at(state, position));
            position++;
            digits++;
        }
        if (digits == 0U)
        {
            return fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_LEXICAL, start);
        }
        result->kind = TOKEN_KIND_INTEGER;
        result->flags = STRING_CATALOG_FILTER_CONSTANT_FLAG_HEXADECIMAL;
    }
    else
    {
        bool is_float = false;

        while (is_digit(peek_at(state, position)))
        {
            position++;
        }
        if (peek_at(state, position) == '.')
        {
            is_float = true;
            position++;
            while (is_digit(peek_at(state, position)))
            {
                position++;
            }
        }
        if ((peek_at(state, position) == 'e') || (peek_at(state, position) == 'E'))
        {
            size_t exponent_digits = 0U;

            is_float = true;
            position++;
            if ((peek_at(state, position) == '+') || (peek_at(state, position) == '-'))
            {
                position++;
            }
            while (is_digit(peek_at(state, position)))
            {
                position++;
                exponent_digits++;
            }
            if (exponent_digits == 0U)
            {
                return fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_LEXICAL, start);
            }
        }

        if (is_float)
        {
            char *end = NULL;
            const size_t length = position - start;

            memcpy(state->string_buffer, state->text + start, length);
            state->string_buffer[length] = '\0';
            result->real = strtod(state->string_buffer, &end);
            if ((end != (state->string_buffer + length)) || !isfinite(result->real))
            {
                return fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_LEXICAL, start);
            }
            result->kind = TOKEN_KIND_FLOAT;
        }
        else
        {
            size_t digit_start = start;

            if (is_negative)
            {
                digit_start++;
            }
            for (size_t index = digit_start; index < position; index++)
            {
                const uint64_t digit = (uint64_t)(state->text[index] - '0');

                if (magnitude > ((UINT64_MAX - digit) / 10U))
                {
                    return fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_LEXICAL, start);
                }
                magnitude = (magnitude * 10U) + digit;
            }
            /* 負の値は int64_t の範囲に収める。-0 は 0 として扱う */
            if (is_negative && (magnitude > ((uint64_t)INT64_MAX + 1U)))
            {
                return fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_LEXICAL, start);
            }
            result->kind = TOKEN_KIND_INTEGER;
            if (is_negative && (magnitude != 0U))
            {
                result->flags = STRING_CATALOG_FILTER_CONSTANT_FLAG_NEGATIVE;
            }
        }
    }

    /* 数値の直後に識別子の文字が続く表記 (12abc など) は誤り */
    if (is_identifier_part(peek_at(state, position)) || (peek_at(state, position) == '.'))
    {
        return fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_LEXICAL, start);
    }

    result->magnitude = magnitude;
    state->position = position;
    return true;
}

/** 次の字句を読み、state->current へ格納します。 */
static bool advance(compiler *state)
{
    token *result = &state->current;
    char character;
    char next;

    while ((state->position < state->text_length) && is_space(state->text[state->position]))
    {
        state->position++;
    }

    memset(result, 0, sizeof(*result));
    result->column = (uint32_t)state->position;

    if (state->position >= state->text_length)
    {
        result->kind = TOKEN_KIND_END;
        return true;
    }

    character = state->text[state->position];
    next = peek_at(state, state->position + 1U);

    if (is_identifier_start(character))
    {
        while (is_identifier_part(peek_at(state, state->position)))
        {
            state->position++;
        }
        result->kind = TOKEN_KIND_IDENTIFIER;
        result->length = (uint32_t)(state->position - result->column);
        return true;
    }

    if (is_digit(character) || ((character == '-') && is_digit(next)))
    {
        if (!lex_number(state, result))
        {
            return false;
        }
        result->length = (uint32_t)(state->position - result->column);
        return true;
    }

    if (character == '"')
    {
        return lex_string(state, result);
    }
    if (character == '\'')
    {
        return lex_character(state, result);
    }

    state->position++;
    switch (character)
    {
    case '(':
        result->kind = TOKEN_KIND_LEFT_PARENTHESIS;
        return true;
    case ')':
        result->kind = TOKEN_KIND_RIGHT_PARENTHESIS;
        return true;
    case '[':
        result->kind = TOKEN_KIND_LEFT_BRACKET;
        return true;
    case ']':
        result->kind = TOKEN_KIND_RIGHT_BRACKET;
        return true;
    case ',':
        result->kind = TOKEN_KIND_COMMA;
        return true;
    case '.':
        result->kind = TOKEN_KIND_DOT;
        return true;
    default:
        break;
    }

    /* 2 文字の演算子と、その先頭 1 文字だけの演算子 */
    if ((character == '=') && (next == '='))
    {
        result->kind = TOKEN_KIND_EQUAL;
    }
    else if ((character == '!') && (next == '='))
    {
        result->kind = TOKEN_KIND_NOT_EQUAL;
    }
    else if ((character == '<') && (next == '='))
    {
        result->kind = TOKEN_KIND_LESS_EQUAL;
    }
    else if ((character == '>') && (next == '='))
    {
        result->kind = TOKEN_KIND_GREATER_EQUAL;
    }
    else if ((character == '&') && (next == '&'))
    {
        result->kind = TOKEN_KIND_AND;
    }
    else if ((character == '|') && (next == '|'))
    {
        result->kind = TOKEN_KIND_OR;
    }
    else if (character == '!')
    {
        result->kind = TOKEN_KIND_NOT;
        return true;
    }
    else if (character == '<')
    {
        result->kind = TOKEN_KIND_LESS;
        return true;
    }
    else if (character == '>')
    {
        result->kind = TOKEN_KIND_GREATER;
        return true;
    }
    else
    {
        return fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_LEXICAL, result->column);
    }

    state->position++;
    return true;
}

/** 現在の字句が、指定した綴りの識別子かを判定します。 */
static bool is_word(const compiler *state, const char *word)
{
    const size_t length = strlen(word);

    return (state->current.kind == TOKEN_KIND_IDENTIFIER) && (state->current.length == length) &&
           (memcmp(state->text + state->current.column, word, length) == 0);
}

static bool expect(compiler *state, const token_kind kind)
{
    if (state->current.kind != kind)
    {
        return fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_SYNTAX, state->current.column);
    }
    return advance(state);
}

/* ===== 命令と定数の生成 ===== */

static bool emit_instruction(compiler *state, const string_catalog_filter_instruction *instruction, uint32_t *index_out)
{
    if (state->instruction_count >= state->instruction_capacity)
    {
        return fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_LIMIT_EXCEEDED, state->current.column);
    }

    string_catalog_filter_write_instruction(state->record, state->instruction_count, instruction);
    if (index_out != NULL)
    {
        *index_out = state->instruction_count;
    }
    state->instruction_count++;

    switch (instruction->opcode)
    {
    case STRING_CATALOG_FILTER_OPCODE_PREDICATE:
        state->depth++;
        break;
    case STRING_CATALOG_FILTER_OPCODE_AND:
    case STRING_CATALOG_FILTER_OPCODE_OR:
        state->depth--;
        break;
    default:
        break;
    }
    if (state->depth > state->max_depth)
    {
        state->max_depth = state->depth;
    }
    return true;
}

static bool emit_logical(compiler *state, const string_catalog_filter_opcode opcode)
{
    string_catalog_filter_instruction instruction;

    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (uint8_t)opcode;
    return emit_instruction(state, &instruction, NULL);
}

/**
 *  @brief          定数を定数領域の末尾へ追加します。
 *  @param[in,out]  state        コンパイル状態。
 *  @param[in]      header       定数の見出し。
 *  @param[in]      payload      値。NULL の場合は値を持ちません。
 *  @param[in]      payload_size 値のバイト数。文字列系は NUL を含めない長さを渡し、NUL と切り上げは本関数が行います。
 *  @param[out]     offset_out   追加した定数のオフセット。
 */
static bool emit_constant(compiler *state, const string_catalog_filter_constant_header *header, const void *payload,
                          const uint32_t payload_size, uint32_t *offset_out)
{
    unsigned char *constants = state->record + CPLAT_STRING_CATALOG_FILTER_RECORD_HEADER_SIZE +
                               ((size_t)state->instruction_capacity * sizeof(string_catalog_filter_instruction));
    uint32_t stored_size = 0U;

    if (payload != NULL)
    {
        stored_size = payload_size;
        if ((header->kind == (uint8_t)STRING_CATALOG_FILTER_CONSTANT_KIND_STRING) ||
            (header->kind == (uint8_t)STRING_CATALOG_FILTER_CONSTANT_KIND_IDENTIFIER) ||
            (header->kind == (uint8_t)STRING_CATALOG_FILTER_CONSTANT_KIND_ARGUMENT_NAME))
        {
            stored_size =
                ((payload_size + STRING_CATALOG_FILTER_CONSTANT_ALIGNMENT) / STRING_CATALOG_FILTER_CONSTANT_ALIGNMENT) *
                STRING_CATALOG_FILTER_CONSTANT_ALIGNMENT;
        }
    }

    if ((state->constant_capacity - state->constant_size) < (STRING_CATALOG_FILTER_CONSTANT_HEADER_SIZE + stored_size))
    {
        return fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_LIMIT_EXCEEDED, state->current.column);
    }

    /* 領域は 0 で埋めてあるため、NUL と切り上げの余りは書き込まない */
    memcpy(constants + state->constant_size, header, sizeof(*header));
    if (payload != NULL)
    {
        memcpy(constants + state->constant_size + STRING_CATALOG_FILTER_CONSTANT_HEADER_SIZE, payload, payload_size);
    }

    *offset_out = state->constant_size;
    state->constant_size += STRING_CATALOG_FILTER_CONSTANT_HEADER_SIZE + stored_size;
    return true;
}

/**
 *  @brief          現在の字句を比較対象の定数として追加します。
 *  @param[in,out]  state    コンパイル状態。
 *  @param[out]     kind_out 追加した定数の種類。
 */
static bool parse_literal(compiler *state, uint8_t *kind_out)
{
    string_catalog_filter_constant_header header;
    const token *current = &state->current;
    uint32_t offset;
    bool is_emitted;

    memset(&header, 0, sizeof(header));

    switch (current->kind)
    {
    case TOKEN_KIND_INTEGER:
    case TOKEN_KIND_CHARACTER:
        if (current->kind == TOKEN_KIND_INTEGER)
        {
            header.kind = (uint8_t)STRING_CATALOG_FILTER_CONSTANT_KIND_INTEGER;
        }
        else
        {
            header.kind = (uint8_t)STRING_CATALOG_FILTER_CONSTANT_KIND_CHARACTER;
        }
        header.flags = current->flags;
        is_emitted =
            emit_constant(state, &header, &current->magnitude, STRING_CATALOG_FILTER_CONSTANT_NUMBER_SIZE, &offset);
        break;

    case TOKEN_KIND_FLOAT:
        header.kind = (uint8_t)STRING_CATALOG_FILTER_CONSTANT_KIND_FLOAT;
        is_emitted = emit_constant(state, &header, &current->real, STRING_CATALOG_FILTER_CONSTANT_NUMBER_SIZE, &offset);
        break;

    case TOKEN_KIND_STRING:
        header.kind = (uint8_t)STRING_CATALOG_FILTER_CONSTANT_KIND_STRING;
        header.length = current->text_length;
        is_emitted = emit_constant(state, &header, state->string_buffer, current->text_length, &offset);
        break;

    case TOKEN_KIND_IDENTIFIER:
        if (is_word(state, "null"))
        {
            header.kind = (uint8_t)STRING_CATALOG_FILTER_CONSTANT_KIND_NULL;
            is_emitted = emit_constant(state, &header, NULL, 0U, &offset);
            break;
        }
        /* 文字列キーの名前。整数への解決は適用の時点で行う */
        if (state->identifier_count >= CPLAT_STRING_CATALOG_FILTER_IDENTIFIER_REFERENCE_MAX)
        {
            return fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_LIMIT_EXCEEDED, current->column);
        }
        header.kind = (uint8_t)STRING_CATALOG_FILTER_CONSTANT_KIND_IDENTIFIER;
        header.slot = (uint16_t)state->identifier_count;
        header.length = current->length;
        is_emitted = emit_constant(state, &header, state->text + current->column, current->length, &offset);
        if (is_emitted)
        {
            state->identifier_count++;
        }
        break;

    case TOKEN_KIND_END:
    case TOKEN_KIND_LEFT_PARENTHESIS:
    case TOKEN_KIND_RIGHT_PARENTHESIS:
    case TOKEN_KIND_LEFT_BRACKET:
    case TOKEN_KIND_RIGHT_BRACKET:
    case TOKEN_KIND_COMMA:
    case TOKEN_KIND_DOT:
    case TOKEN_KIND_EQUAL:
    case TOKEN_KIND_NOT_EQUAL:
    case TOKEN_KIND_LESS:
    case TOKEN_KIND_LESS_EQUAL:
    case TOKEN_KIND_GREATER:
    case TOKEN_KIND_GREATER_EQUAL:
    case TOKEN_KIND_AND:
    case TOKEN_KIND_OR:
    case TOKEN_KIND_NOT:
    default:
        return fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_SYNTAX, current->column);
    }

    if (!is_emitted)
    {
        return false;
    }

    *kind_out = header.kind;
    return advance(state);
}

/** `arg.name` または `arg[n]` を読み、判定要素のフィールドを設定します。 */
static bool parse_argument_reference(compiler *state, string_catalog_filter_instruction *instruction)
{
    if (!is_word(state, "arg"))
    {
        return fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_SYNTAX, state->current.column);
    }
    if (!advance(state))
    {
        return false;
    }

    if (state->current.kind == TOKEN_KIND_DOT)
    {
        string_catalog_filter_constant_header header;
        const unsigned char *constants;
        const char *name;
        uint32_t name_length;
        uint32_t slot;
        uint32_t offset;

        if (!advance(state))
        {
            return false;
        }
        if (state->current.kind != TOKEN_KIND_IDENTIFIER)
        {
            return fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_SYNTAX, state->current.column);
        }

        name = state->text + state->current.column;
        name_length = state->current.length;
        constants = string_catalog_filter_record_constants(state->record, state->line_width);

        /* 同じ引数名には同じ参照番号を割り当てる。適用時の名前解決を種類の数だけに抑えるためです。 */
        for (slot = 0U; slot < state->argument_reference_count; slot++)
        {
            string_catalog_filter_constant_header existing;

            memcpy(&existing, constants + state->argument_name_offsets[slot], sizeof(existing));
            if ((existing.length == name_length) &&
                (memcmp(constants + state->argument_name_offsets[slot] + STRING_CATALOG_FILTER_CONSTANT_HEADER_SIZE,
                        name, name_length) == 0))
            {
                break;
            }
        }
        if (slot == state->argument_reference_count)
        {
            if (state->argument_reference_count >= CPLAT_STRING_CATALOG_FILTER_ARGUMENT_REFERENCE_MAX)
            {
                return fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_LIMIT_EXCEEDED, state->current.column);
            }
        }

        memset(&header, 0, sizeof(header));
        header.kind = (uint8_t)STRING_CATALOG_FILTER_CONSTANT_KIND_ARGUMENT_NAME;
        header.slot = (uint16_t)slot;
        header.length = name_length;
        if (!emit_constant(state, &header, name, name_length, &offset))
        {
            return false;
        }
        if (slot == state->argument_reference_count)
        {
            state->argument_name_offsets[slot] = (uint16_t)offset;
            state->argument_reference_count++;
        }

        instruction->field = (uint8_t)STRING_CATALOG_FILTER_FIELD_ARGUMENT_NAME;
        instruction->argument = (uint8_t)slot;
        instruction->operand = (uint16_t)offset;
        return advance(state);
    }

    if (state->current.kind == TOKEN_KIND_LEFT_BRACKET)
    {
        if (!advance(state))
        {
            return false;
        }
        if ((state->current.kind != TOKEN_KIND_INTEGER) || (state->current.flags != 0U))
        {
            return fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_SYNTAX, state->current.column);
        }
        if (state->current.magnitude >= CPLAT_STRING_CATALOG_ARGUMENT_MAX)
        {
            return fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_LIMIT_EXCEEDED, state->current.column);
        }
        instruction->field = (uint8_t)STRING_CATALOG_FILTER_FIELD_ARGUMENT_INDEX;
        instruction->argument = (uint8_t)state->current.magnitude;
        if (!advance(state))
        {
            return false;
        }
        return expect(state, TOKEN_KIND_RIGHT_BRACKET);
    }

    return fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_SYNTAX, state->current.column);
}

/** 判定演算子を読みます。 */
static bool parse_operator(compiler *state, string_catalog_filter_instruction *instruction)
{
    static const struct operator_symbol
    {
        token_kind kind;
        string_catalog_filter_operator operator_kind;
    } symbols[] = {
        {TOKEN_KIND_EQUAL, STRING_CATALOG_FILTER_OPERATOR_EQUAL},
        {TOKEN_KIND_NOT_EQUAL, STRING_CATALOG_FILTER_OPERATOR_NOT_EQUAL},
        {TOKEN_KIND_LESS, STRING_CATALOG_FILTER_OPERATOR_LESS},
        {TOKEN_KIND_LESS_EQUAL, STRING_CATALOG_FILTER_OPERATOR_LESS_EQUAL},
        {TOKEN_KIND_GREATER, STRING_CATALOG_FILTER_OPERATOR_GREATER},
        {TOKEN_KIND_GREATER_EQUAL, STRING_CATALOG_FILTER_OPERATOR_GREATER_EQUAL},
    };
    static const struct operator_word
    {
        const char *word;
        string_catalog_filter_operator operator_kind;
        unsigned int pad;
    } words[] = {
        {"in", STRING_CATALOG_FILTER_OPERATOR_IN, 0U},
        {"between", STRING_CATALOG_FILTER_OPERATOR_BETWEEN, 0U},
        {"starts_with", STRING_CATALOG_FILTER_OPERATOR_STARTS_WITH, 0U},
        {"ends_with", STRING_CATALOG_FILTER_OPERATOR_ENDS_WITH, 0U},
        {"contains", STRING_CATALOG_FILTER_OPERATOR_CONTAINS, 0U},
        {"starts_with_i", STRING_CATALOG_FILTER_OPERATOR_STARTS_WITH_I, 0U},
        {"ends_with_i", STRING_CATALOG_FILTER_OPERATOR_ENDS_WITH_I, 0U},
        {"contains_i", STRING_CATALOG_FILTER_OPERATOR_CONTAINS_I, 0U},
        {"matches", STRING_CATALOG_FILTER_OPERATOR_MATCHES, 0U},
        {"matches_i", STRING_CATALOG_FILTER_OPERATOR_MATCHES_I, 0U},
    };

    for (size_t index = 0; index < (sizeof(symbols) / sizeof(symbols[0])); index++)
    {
        if (state->current.kind == symbols[index].kind)
        {
            instruction->operator_kind = (uint8_t)symbols[index].operator_kind;
            return advance(state);
        }
    }

    for (size_t index = 0; index < (sizeof(words) / sizeof(words[0])); index++)
    {
        if (is_word(state, words[index].word))
        {
            instruction->operator_kind = (uint8_t)words[index].operator_kind;
            return advance(state);
        }
    }

    return fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_SYNTAX, state->current.column);
}

/** 比較対象を 1 個読み、フィールドと判定演算子との組み合わせを確認します。 */
static bool parse_checked_literal(compiler *state, const string_catalog_filter_instruction *instruction,
                                  uint8_t *kind_out)
{
    const uint32_t column = state->current.column;

    if (!parse_literal(state, kind_out))
    {
        return false;
    }
    if (string_catalog_filter_is_constant_allowed(instruction->field, instruction->operator_kind, *kind_out) == 0)
    {
        return fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_TYPE_MISMATCH, column);
    }
    return true;
}

/**
 *  @brief          正規表現のパターンを 1 個読み、構文を確かめて、行内のパターンの番号を割り当てます。
 *
 *  パターンは文字列の字句で書きます。定数はパターンの種類として格納し、番号を見出しの slot へ記録します。\n
 *  構文は、ここで一度コンパイルして確かめます。コンパイルしたハンドルは保持せず、適用の時点でコンパイルし直します。
 *  フィルター オブジェクトはポインターを含まない領域であるためです。
 */
static bool parse_pattern(compiler *state, const string_catalog_filter_instruction *instruction)
{
    string_catalog_filter_constant_header header;
    unsigned char *constants = state->record + CPLAT_STRING_CATALOG_FILTER_RECORD_HEADER_SIZE +
                               ((size_t)state->instruction_capacity * sizeof(string_catalog_filter_instruction));
    const uint32_t column = state->current.column;
    const uint32_t offset = state->constant_size;
    cplat_regex *regex = NULL;
    uint8_t kind = 0U;

    if ((state->current.kind != TOKEN_KIND_STRING) ||
        (string_catalog_filter_is_constant_allowed(instruction->field, instruction->operator_kind,
                                                   (uint8_t)STRING_CATALOG_FILTER_CONSTANT_KIND_PATTERN) == 0))
    {
        return fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_TYPE_MISMATCH, column);
    }
    if (state->pattern_count >= CPLAT_STRING_CATALOG_FILTER_PATTERN_REFERENCE_MAX)
    {
        return fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_LIMIT_EXCEEDED, column);
    }

    /* 字句の復号結果は次の字句で上書きされるため、格納した定数からパターンを読む */
    if (!parse_literal(state, &kind))
    {
        return false;
    }
    memcpy(&header, constants + offset, sizeof(header));
    header.kind = (uint8_t)STRING_CATALOG_FILTER_CONSTANT_KIND_PATTERN;
    header.slot = (uint16_t)state->pattern_count;
    memcpy(constants + offset, &header, sizeof(header));

    if (string_catalog_filter_create_pattern(
            (const char *)(constants + offset + STRING_CATALOG_FILTER_CONSTANT_HEADER_SIZE), instruction->operator_kind,
            &regex) != CPLAT_OK)
    {
        return fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_INVALID_PATTERN, column);
    }
    cplat_regex_dispose(regex);

    state->pattern_count++;
    return true;
}

static bool is_same_class(const uint8_t first_kind, const uint8_t kind)
{
    const bool is_first_string = (first_kind == (uint8_t)STRING_CATALOG_FILTER_CONSTANT_KIND_STRING);
    const bool is_string = (kind == (uint8_t)STRING_CATALOG_FILTER_CONSTANT_KIND_STRING);

    return is_first_string == is_string;
}

static bool parse_predicate(compiler *state)
{
    string_catalog_filter_instruction instruction;
    uint32_t literal_offset;
    uint8_t first_kind = 0U;
    uint8_t kind = 0U;

    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = (uint8_t)STRING_CATALOG_FILTER_OPCODE_PREDICATE;

    state->predicate_count++;
    if (state->predicate_count > CPLAT_STRING_CATALOG_FILTER_PREDICATE_MAX)
    {
        return fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_LIMIT_EXCEEDED, state->current.column);
    }

    if (is_word(state, "has"))
    {
        if (!advance(state) || !expect(state, TOKEN_KIND_LEFT_PARENTHESIS) ||
            !parse_argument_reference(state, &instruction) || !expect(state, TOKEN_KIND_RIGHT_PARENTHESIS))
        {
            return false;
        }
        instruction.operator_kind = (uint8_t)STRING_CATALOG_FILTER_OPERATOR_HAS;
        return emit_instruction(state, &instruction, NULL);
    }

    if (is_word(state, "key"))
    {
        instruction.field = (uint8_t)STRING_CATALOG_FILTER_FIELD_KEY;
        if (!advance(state))
        {
            return false;
        }
    }
    else if (is_word(state, "id"))
    {
        instruction.field = (uint8_t)STRING_CATALOG_FILTER_FIELD_ID;
        if (!advance(state))
        {
            return false;
        }
    }
    else if (is_word(state, "category"))
    {
        instruction.field = (uint8_t)STRING_CATALOG_FILTER_FIELD_CATEGORY;
        if (!advance(state))
        {
            return false;
        }
    }
    else if (!parse_argument_reference(state, &instruction))
    {
        return false;
    }

    if (!parse_operator(state, &instruction))
    {
        return false;
    }

    /* 引数名の定数を除き、比較対象の定数はこの位置から並ぶ */
    literal_offset = state->constant_size;

    switch (instruction.operator_kind)
    {
    case STRING_CATALOG_FILTER_OPERATOR_IN:
        if (!expect(state, TOKEN_KIND_LEFT_BRACKET))
        {
            return false;
        }
        for (;;)
        {
            const uint32_t column = state->current.column;

            if (!parse_checked_literal(state, &instruction, &kind))
            {
                return false;
            }
            if (instruction.operand_count == 0U)
            {
                first_kind = kind;
            }
            else if (!is_same_class(first_kind, kind))
            {
                return fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_TYPE_MISMATCH, column);
            }
            instruction.operand_count++;

            if (state->current.kind != TOKEN_KIND_COMMA)
            {
                break;
            }
            if (!advance(state))
            {
                return false;
            }
        }
        if (!expect(state, TOKEN_KIND_RIGHT_BRACKET))
        {
            return false;
        }
        break;

    case STRING_CATALOG_FILTER_OPERATOR_MATCHES:
    case STRING_CATALOG_FILTER_OPERATOR_MATCHES_I:
        if (!parse_pattern(state, &instruction))
        {
            return false;
        }
        instruction.operand_count = 1U;
        break;

    case STRING_CATALOG_FILTER_OPERATOR_BETWEEN:
        if (!parse_checked_literal(state, &instruction, &kind))
        {
            return false;
        }
        if (!is_word(state, "and"))
        {
            return fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_SYNTAX, state->current.column);
        }
        if (!advance(state) || !parse_checked_literal(state, &instruction, &kind))
        {
            return false;
        }
        instruction.operand_count = 2U;
        break;

    default:
        if (!parse_checked_literal(state, &instruction, &kind))
        {
            return false;
        }
        instruction.operand_count = 1U;
        break;
    }

    if (instruction.field != (uint8_t)STRING_CATALOG_FILTER_FIELD_ARGUMENT_NAME)
    {
        instruction.operand = (uint16_t)literal_offset;
    }

    return emit_instruction(state, &instruction, NULL);
}

static bool parse_or(compiler *state);

static bool enter_nesting(compiler *state)
{
    state->nesting++;
    if (state->nesting > CPLAT_STRING_CATALOG_FILTER_NESTING_MAX)
    {
        return fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_LIMIT_EXCEEDED, state->current.column);
    }
    return true;
}

static bool parse_unary(compiler *state)
{
    if (state->current.kind == TOKEN_KIND_NOT)
    {
        if (!enter_nesting(state) || !advance(state) || !parse_unary(state) ||
            !emit_logical(state, STRING_CATALOG_FILTER_OPCODE_NOT))
        {
            return false;
        }
        state->nesting--;
        return true;
    }

    if (state->current.kind == TOKEN_KIND_LEFT_PARENTHESIS)
    {
        if (!enter_nesting(state) || !advance(state) || !parse_or(state) ||
            !expect(state, TOKEN_KIND_RIGHT_PARENTHESIS))
        {
            return false;
        }
        state->nesting--;
        return true;
    }

    return parse_predicate(state);
}

/**
 *  @brief          `&&` または `||` で結ばれた並びを読みます。
 *  @param[in,out]  state          コンパイル状態。
 *  @param[in]      operator_token 結合の字句。
 *  @param[in]      jump_opcode    短絡評価のジャンプ命令。
 *  @param[in]      join_opcode    結合の命令。
 *  @param[in]      parse_operand  被演算子を読む関数。
 */
static bool parse_binary(compiler *state, const token_kind operator_token,
                         const string_catalog_filter_opcode jump_opcode, const string_catalog_filter_opcode join_opcode,
                         bool (*parse_operand)(compiler *))
{
    if (!parse_operand(state))
    {
        return false;
    }

    while (state->current.kind == operator_token)
    {
        string_catalog_filter_instruction jump;
        uint32_t jump_index;

        memset(&jump, 0, sizeof(jump));
        jump.opcode = (uint8_t)jump_opcode;
        if (!emit_instruction(state, &jump, &jump_index) || !advance(state) || !parse_operand(state) ||
            !emit_logical(state, join_opcode))
        {
            return false;
        }

        /* ジャンプ先は結合の命令の直後 */
        jump.operand = (uint16_t)state->instruction_count;
        string_catalog_filter_write_instruction(state->record, jump_index, &jump);
    }

    return true;
}

static bool parse_and(compiler *state)
{
    return parse_binary(state, TOKEN_KIND_AND, STRING_CATALOG_FILTER_OPCODE_JUMP_IF_FALSE,
                        STRING_CATALOG_FILTER_OPCODE_AND, parse_unary);
}

static bool parse_or(compiler *state)
{
    return parse_binary(state, TOKEN_KIND_OR, STRING_CATALOG_FILTER_OPCODE_JUMP_IF_TRUE,
                        STRING_CATALOG_FILTER_OPCODE_OR, parse_and);
}

/* Doxygen コメントは、ヘッダーに記載 */

int string_catalog_filter_compile_record(const char *text, const size_t text_length, const uint32_t line_width,
                                         unsigned char *record, cplat_string_catalog_filter_line_error *error_out,
                                         uint32_t *column_out)
{
    string_catalog_filter_record_header header;
    compiler *state;
    compiler state_storage;

    state = &state_storage;
    memset(state, 0, sizeof(*state));
    memset(record, 0, CPLAT_STRING_CATALOG_FILTER_RECORD_SIZE(line_width));

    state->text = text;
    state->text_length = text_length;
    state->record = record;
    state->line_width = line_width;
    state->instruction_capacity = string_catalog_filter_instruction_capacity(line_width);
    state->constant_capacity = string_catalog_filter_constant_capacity(line_width);

    if (text_length > line_width)
    {
        (void)fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_LIMIT_EXCEEDED, line_width);
    }
    else if (advance(state) && parse_or(state) && (state->current.kind != TOKEN_KIND_END))
    {
        (void)fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_SYNTAX, state->current.column);
    }

    if ((state->error == CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE) && (state->instruction_count == 0U))
    {
        (void)fail(state, CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_SYNTAX, 0U);
    }

    if (state->error != CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE)
    {
        memset(record, 0, CPLAT_STRING_CATALOG_FILTER_RECORD_SIZE(line_width));
        *error_out = state->error;
        *column_out = state->error_column;
        return CPLAT_ERR_MALFORMED_DEFINITION;
    }

    memset(&header, 0, sizeof(header));
    header.instruction_count = (uint16_t)state->instruction_count;
    header.constant_size = (uint16_t)state->constant_size;
    header.stack_depth = (uint8_t)state->max_depth;
    header.argument_reference_count = (uint8_t)state->argument_reference_count;
    header.identifier_count = (uint8_t)state->identifier_count;
    header.pattern_count = (uint8_t)state->pattern_count;
    string_catalog_filter_write_record_header(record, &header);

    header.line_hash = string_catalog_filter_compute_line_hash(record, line_width);
    string_catalog_filter_write_record_header(record, &header);

    *error_out = CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE;
    *column_out = 0U;
    return CPLAT_OK;
}

/* ===== 公開関数 ===== */

/**
 *  @brief          固定長の行から、前後の空白を除いた範囲を求めます。
 *  @return         判定に使用する行なら true、空行とコメント行なら false。
 */
static bool trim_line(const char *row, const size_t width, size_t *start_out, size_t *length_out)
{
    const char *terminator = (const char *)memchr(row, '\0', width);
    size_t end = width;
    size_t start = 0U;

    if (terminator != NULL)
    {
        end = (size_t)(terminator - row);
    }
    while ((start < end) && is_space(row[start]))
    {
        start++;
    }
    while ((end > start) && is_space(row[end - 1U]))
    {
        end--;
    }

    *start_out = start;
    *length_out = end - start;
    return (end > start) && (row[start] != '#');
}

static void record_diagnostic(cplat_string_catalog_filter_diagnostic *diagnostics, const size_t diagnostic_capacity,
                              const size_t invalid_count, const size_t line_index, const size_t column,
                              const cplat_string_catalog_filter_line_error error)
{
    if ((diagnostics != NULL) && (invalid_count < diagnostic_capacity))
    {
        diagnostics[invalid_count].line_index = (uint32_t)line_index;
        diagnostics[invalid_count].column = (uint32_t)column;
        diagnostics[invalid_count].error = error;
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_string_catalog_filter_compile(const char *lines, const size_t line_count, const size_t line_width,
                                        const size_t line_capacity, void *image_out, const size_t image_size,
                                        cplat_string_catalog_filter_diagnostic *diagnostics,
                                        const size_t diagnostic_capacity, size_t *invalid_count_out)
{
    string_catalog_filter_image_header header;
    size_t invalid_count = 0U;

    if ((image_out == NULL) || ((lines == NULL) && (line_count > 0U)) ||
        (line_width < CPLAT_STRING_CATALOG_FILTER_LINE_WIDTH_MIN) ||
        (line_width > CPLAT_STRING_CATALOG_FILTER_LINE_WIDTH_MAX) || (line_capacity == 0U) ||
        (line_capacity > CPLAT_STRING_CATALOG_FILTER_LINE_MAX))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (image_size < CPLAT_STRING_CATALOG_FILTER_IMAGE_SIZE(line_capacity, line_width))
    {
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    memset(image_out, 0, CPLAT_STRING_CATALOG_FILTER_IMAGE_SIZE(line_capacity, line_width));
    memset(&header, 0, sizeof(header));
    header.signature = STRING_CATALOG_FILTER_SIGNATURE;
    header.format_version = STRING_CATALOG_FILTER_FORMAT_VERSION;
    header.byte_order_mark = STRING_CATALOG_FILTER_BYTE_ORDER_MARK;
    header.line_capacity = (uint32_t)line_capacity;
    header.line_width = (uint32_t)line_width;
    header.record_size = (uint32_t)CPLAT_STRING_CATALOG_FILTER_RECORD_SIZE(line_width);
    header.image_size = CPLAT_STRING_CATALOG_FILTER_IMAGE_SIZE(line_capacity, line_width);

    for (size_t index = 0; index < line_count; index++)
    {
        const char *row = lines + (index * line_width);
        cplat_string_catalog_filter_line_error error;
        uint32_t column;
        size_t start;
        size_t length;

        if (!trim_line(row, line_width, &start, &length))
        {
            continue;
        }

        if (header.line_count >= header.line_capacity)
        {
            record_diagnostic(diagnostics, diagnostic_capacity, invalid_count, index, start,
                              CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_LINE_CAPACITY);
            invalid_count++;
            continue;
        }

        if (string_catalog_filter_compile_record(
                row + start, length, header.line_width,
                string_catalog_filter_record_address(image_out, header.record_size, header.line_count), &error,
                &column) != CPLAT_OK)
        {
            record_diagnostic(diagnostics, diagnostic_capacity, invalid_count, index, start + column, error);
            invalid_count++;
            continue;
        }

        header.line_count++;
    }

    string_catalog_filter_update_content_hash(image_out, &header);

    if (invalid_count_out != NULL)
    {
        *invalid_count_out = invalid_count;
    }
    return CPLAT_OK;
}

/**
 *  @brief          1 行の条件式を作業領域へコンパイルします。
 *
 *  失敗した場合にフィルター オブジェクトを変更しないよう、作業領域を使用します。
 */
static int compile_text(const char *text, const uint32_t line_width, unsigned char *record,
                        cplat_string_catalog_filter_diagnostic *diagnostic_out)
{
    const size_t text_length = strlen(text);
    cplat_string_catalog_filter_line_error error = CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_SYNTAX;
    uint32_t column = 0U;
    size_t start;
    size_t length;
    int ret = CPLAT_ERR_MALFORMED_DEFINITION;

    if (text_length > line_width)
    {
        error = CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_LIMIT_EXCEEDED;
        column = line_width;
    }
    else if (trim_line(text, text_length, &start, &length))
    {
        ret = string_catalog_filter_compile_record(text + start, length, line_width, record, &error, &column);
        column += (uint32_t)start;
    }

    if (diagnostic_out != NULL)
    {
        diagnostic_out->line_index = 0U;
        diagnostic_out->column = column;
        diagnostic_out->error = error;
        if (ret == CPLAT_OK)
        {
            diagnostic_out->column = 0U;
            diagnostic_out->error = CPLAT_STRING_CATALOG_FILTER_LINE_ERROR_NONE;
        }
    }
    return ret;
}

/** 行単位の編集に共通する事前確認です。 */
static int prepare_edit(void *image, const size_t image_size, const char *text,
                        string_catalog_filter_image_header *header)
{
    int ret;

    if ((image == NULL) || (text == NULL))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    ret = cplat_string_catalog_filter_validate(image, image_size);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    string_catalog_filter_read_image_header(image, header);
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_string_catalog_filter_compile_line(void *image, const size_t image_size, const size_t line_index,
                                             const char *text, cplat_string_catalog_filter_diagnostic *diagnostic_out)
{
    unsigned char record[CPLAT_STRING_CATALOG_FILTER_RECORD_SIZE(CPLAT_STRING_CATALOG_FILTER_LINE_WIDTH_MAX)];
    string_catalog_filter_image_header header;
    int ret = prepare_edit(image, image_size, text, &header);

    if (ret != CPLAT_OK)
    {
        return ret;
    }
    if (line_index >= header.line_count)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    ret = compile_text(text, header.line_width, record, diagnostic_out);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    if (diagnostic_out != NULL)
    {
        diagnostic_out->line_index = (uint32_t)line_index;
    }

    memcpy(string_catalog_filter_record_address(image, header.record_size, (uint32_t)line_index), record,
           header.record_size);
    string_catalog_filter_update_content_hash(image, &header);
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_string_catalog_filter_insert_line(void *image, const size_t image_size, const size_t line_index,
                                            const char *text, cplat_string_catalog_filter_diagnostic *diagnostic_out)
{
    unsigned char record[CPLAT_STRING_CATALOG_FILTER_RECORD_SIZE(CPLAT_STRING_CATALOG_FILTER_LINE_WIDTH_MAX)];
    string_catalog_filter_image_header header;
    unsigned char *target;
    int ret = prepare_edit(image, image_size, text, &header);

    if (ret != CPLAT_OK)
    {
        return ret;
    }
    if (line_index > header.line_count)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (header.line_count >= header.line_capacity)
    {
        return CPLAT_ERR_STORAGE_FULL;
    }

    ret = compile_text(text, header.line_width, record, diagnostic_out);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    if (diagnostic_out != NULL)
    {
        diagnostic_out->line_index = (uint32_t)line_index;
    }

    /* 挿入位置から後ろの行を 1 つ後ろへずらす */
    target = string_catalog_filter_record_address(image, header.record_size, (uint32_t)line_index);
    memmove(target + header.record_size, target, (size_t)header.record_size * ((size_t)header.line_count - line_index));
    memcpy(target, record, header.record_size);

    header.line_count++;
    string_catalog_filter_update_content_hash(image, &header);
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int string_catalog_filter_create_pattern(const char *pattern, const uint8_t operator_kind, cplat_regex **regex_out)
{
    unsigned int flags = CPLAT_REGEX_DEFAULT | CPLAT_REGEX_NOSUB;

    if (operator_kind == (uint8_t)STRING_CATALOG_FILTER_OPERATOR_MATCHES_I)
    {
        flags |= CPLAT_REGEX_ICASE;
    }
    return cplat_regex_create(pattern, flags, regex_out, NULL);
}
