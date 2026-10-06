/**
 *******************************************************************************
 *  @file           filter_describe.c
 *  @brief          条件式を、カタログのメタ情報を用いた自然文で表現します。
 *  @author         Tetsuo Honda
 *  @date           2026/09/26
 *  @version        0.1.0
 *
 *  設計資料「デコンパイルと自然文での表現」の実装です。\n
 *  文字列キーの比較は項目の `brief` と `id`、引数の比較は引数の名前と説明で表します。\n
 *  分類値は、ライブラリが意味を解釈しないため、値そのもので表します。
 *  利用側が分類値の名前を設定した場合に限り、条件を満たす分類値の名前を列挙して表します。
 *
 *  文型はニュートラル言語 (英語) と日本語を持ちます。\n
 *  カタログのメタ情報は言語別に持たないため、どちらの文型でも `brief` と説明はカタログの記述のまま埋め込みます。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *******************************************************************************
 */

#include "filter.h"

#include <cplat/base/result.h>

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/** 演算子の優先順位です。値が大きいほど強く結合します。 */
#define PRECEDENCE_OR        1
#define PRECEDENCE_AND       2
#define PRECEDENCE_NOT       3
#define PRECEDENCE_PREDICATE 4

/** 判定演算子の数 (インデックス 0 は未使用) です。 */
#define OPERATOR_TABLE_SIZE ((size_t)STRING_CATALOG_FILTER_OPERATOR_MATCHES_I + 1U)

/** 定数 1 個の表記の最大バイト数です。文字列の定数は行幅を超えません。 */
#define CONSTANT_TEXT_MAX (CPLAT_STRING_CATALOG_FILTER_LINE_WIDTH_MAX * 4U + 16U)

/** 判定演算子 1 個の文型です。主語、before、値、after の順に並べます。 */
typedef struct operator_phrase
{
    const char *before; /**< 主語と値の間に置く語。 */
    const char *after;  /**< 値の後ろに置く語。 */
} operator_phrase;

/** 1 言語分の文型です。 */
typedef struct language_phrases
{
    const char *key_subject;           /**< 文字列キーを主語にする場合の名前。 */
    const char *id_subject;            /**< ID を主語にする場合の名前。 */
    const char *category_subject;      /**< 分類値を主語にする場合の名前。 */
    const char *argument_prefix;       /**< 引数の名前の前に置く語。 */
    const char *key_name_prefix;       /**< カタログにない文字列キーを表す語。 */
    const char *entry_open;            /**< 項目の brief を囲む開きの記号。 */
    const char *entry_close;           /**< 項目の brief を囲む閉じの記号。 */
    const char *entry_id_open;         /**< 項目の ID を囲む開きの記号。 */
    const char *key_not_equal_prefix;  /**< 「この項目以外」の前置。 */
    const char *key_not_equal_suffix;  /**< 「この項目以外」の後置。 */
    const char *key_in_prefix;         /**< 「いずれかの項目」の前置。 */
    const char *key_in_suffix;         /**< 「いずれかの項目」の後置。 */
    const char *has_prefix;            /**< 「引数を持つ」の前置。 */
    const char *has_suffix;            /**< 「引数を持つ」の後置。 */
    const char *between_before;        /**< between の主語と下限の間。 */
    const char *between_middle;        /**< between の下限と上限の間。 */
    const char *between_after;         /**< between の上限の後ろ。 */
    const char *case_insensitive;      /**< 大文字と小文字を区別しない演算子の注記。 */
    const char *and_text;              /**< 論理積。 */
    const char *or_text;               /**< 論理和。 */
    const char *not_prefix;            /**< 否定の前置。 */
    const char *not_suffix;            /**< 否定の後置。 */
    const char *list_separator;        /**< 列挙の区切り。 */
    const char *null_text;             /**< null の表記。 */
    const char *category_none;         /**< 分類値の名前: 条件を満たす名前がない場合の後置。 */
    const char *category_any;          /**< 分類値の名前: すべての名前が条件を満たす場合の後置。 */
    const char *category_is_after;     /**< 分類値の名前: 名前が 1 つの場合の後置。前置は演算子 == の文型。 */
    const char *category_other_before; /**< 分類値の名前: 補集合を表す場合の前置。 */
    const char *category_other_after;  /**< 分類値の名前: 補集合を表す場合の後置。 */
    const char *mixed_types_prefix;    /**< 型区分が混在する引数の注記: 引数名の前置。 */
    const char *mixed_types_suffix;    /**< 型区分が混在する引数の注記: 引数名の後置。 */
    int inserts_space_after_ascii;     /**< 主語が ASCII で終わる場合に before の前へ空白を置くなら 0 以外。 */
    unsigned int pad;                  /**< 明示的アラインメントです。 */
    operator_phrase operators[OPERATOR_TABLE_SIZE]; /**< 判定演算子ごとの文型。 */
} language_phrases;

/** 日本語の文型です。 */
static const language_phrases s_japanese = {
    "文字列キー",
    "ID",
    "分類値",
    "引数 ",
    "文字列キー ",
    "「",
    "」",
    "(",
    "",
    " 以外",
    "",
    " のいずれか",
    "",
    " を持つ",
    "が ",
    " 以上 ",
    " 以下",
    " (大文字と小文字を区別しない)",
    " かつ ",
    "、または ",
    "(",
    ") ではない",
    "、",
    "NULL",
    "がいずれにも該当しない",
    "を問わない",
    " である",
    "が ",
    " 以外",
    " (注意: 引数 ",
    " はカタログ内に文字列型と数値型が混在しているため、意図した判定結果にならない可能性があります)",
    1,
    0U,
    {
        {"", ""},
        {"が ", " である"},
        {"が ", " ではない"},
        {"が ", " より小さい"},
        {"が ", " 以下"},
        {"が ", " より大きい"},
        {"が ", " 以上"},
        {"が ", " のいずれか"},
        {"", ""},
        {"", ""},
        {"が ", " で始まる"},
        {"が ", " で終わる"},
        {"が ", " を含む"},
        {"が ", " で始まる"},
        {"が ", " で終わる"},
        {"が ", " を含む"},
        {"が 正規表現 ", " に一致する部分を含む"},
        {"が 正規表現 ", " に一致する部分を含む"},
    },
};

/** ニュートラル言語 (英語) の文型です。 */
static const language_phrases s_neutral = {
    "the string key",
    "the ID",
    "the category",
    "argument ",
    "string key ",
    "\"",
    "\"",
    " (",
    "other than ",
    "",
    "one of ",
    "",
    "has ",
    "",
    " is between ",
    " and ",
    "",
    " (case-insensitive)",
    " and ",
    " or ",
    "not (",
    ")",
    ", ",
    "null",
    " matches none",
    " is any",
    "",
    " is other than ",
    "",
    " (Note: argument ",
    " has both string and numeric types in the catalog; the result may not be as intended.)",
    0,
    0U,
    {
        {"", ""},
        {" is ", ""},
        {" is not ", ""},
        {" is less than ", ""},
        {" is less than or equal to ", ""},
        {" is greater than ", ""},
        {" is greater than or equal to ", ""},
        {" is one of ", ""},
        {"", ""},
        {"", ""},
        {" starts with ", ""},
        {" ends with ", ""},
        {" contains ", ""},
        {" starts with ", ""},
        {" ends with ", ""},
        {" contains ", ""},
        {" contains a match for the regular expression ", ""},
        {" contains a match for the regular expression ", ""},
    },
};

/** 自然文の書き出し先です。 */
typedef struct describe_writer
{
    char *dest;
    size_t size;
    size_t length;
    bool is_truncated;
    uint8_t pad[7]; /**< 明示的アラインメントです。 */
} describe_writer;

/** 1 行の自然文を組み立てる間の状態です。 */
typedef struct describe_context
{
    const string_catalog_filter_describe_source *source;
    const language_phrases *phrases;
    const unsigned char *constants;
    uint32_t constant_size;
    uint32_t pad; /**< 明示的アラインメントです。 */
    uint16_t left[CPLAT_STRING_CATALOG_FILTER_LINE_WIDTH_MAX];
    uint16_t right[CPLAT_STRING_CATALOG_FILTER_LINE_WIDTH_MAX];
    describe_writer writer;
} describe_context;

/* ===== 書き出し ===== */

static void append(describe_writer *writer, const char *text, const size_t length)
{
    size_t copy_length = length;
    size_t available;

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

static void append_text(describe_writer *writer, const char *text)
{
    append(writer, text, strlen(text));
}

/**
 *  @brief          カタログの説明文を、文末の句点を除いて書き出します。
 *
 *  `brief` と引数の説明は 1 文として句点で終わるため、文の途中へ埋め込むときは句点を除きます。
 */
static void append_sentence(describe_writer *writer, const char *text)
{
    static const char japanese_period[] = "\xE3\x80\x82"; /* 「。」の UTF-8 表現 */
    size_t length = strlen(text);

    if ((length >= (sizeof(japanese_period) - 1U)) &&
        (memcmp(text + (length - (sizeof(japanese_period) - 1U)), japanese_period, sizeof(japanese_period) - 1U) == 0))
    {
        length -= sizeof(japanese_period) - 1U;
    }
    else if ((length > 0U) && (text[length - 1U] == '.'))
    {
        length--;
    }
    else
    {
        /* 句点で終わらない説明は、そのまま書き出す */
    }

    append(writer, text, length);
}

/** 主語の直後に before を置きます。日本語では、主語が ASCII で終わる場合に空白を挟みます。 */
static void append_before(describe_context *context, const char *before)
{
    describe_writer *writer = &context->writer;

    if ((context->phrases->inserts_space_after_ascii != 0) && (writer->length > 0U) &&
        ((unsigned char)writer->dest[writer->length - 1U] < 0x80U) && (writer->dest[writer->length - 1U] != ' '))
    {
        append_text(writer, " ");
    }
    append_text(writer, before);
}

/* ===== 定数と主語 ===== */

static void append_constant(describe_context *context, const string_catalog_filter_constant *constant)
{
    char text[CONSTANT_TEXT_MAX];

    if (constant->header.kind == (uint8_t)STRING_CATALOG_FILTER_CONSTANT_KIND_NULL)
    {
        append_text(&context->writer, context->phrases->null_text);
        return;
    }
    (void)string_catalog_filter_format_constant(constant, text, sizeof(text));
    append_text(&context->writer, text);
}

/**
 *  @brief          整数として扱える定数から文字列キーを求めます。
 *  @return         求められた場合は true。
 */
static bool key_of_constant(const describe_context *context, const string_catalog_filter_constant *constant,
                            int *key_out)
{
    switch (constant->header.kind)
    {
    case STRING_CATALOG_FILTER_CONSTANT_KIND_IDENTIFIER:
        *key_out = (int)context->source->identifier_values[constant->header.slot];
        return true;
    case STRING_CATALOG_FILTER_CONSTANT_KIND_INTEGER:
        if (constant->magnitude > (uint64_t)INT32_MAX)
        {
            return false;
        }
        *key_out = (int)constant->magnitude;
        if ((constant->header.flags & STRING_CATALOG_FILTER_CONSTANT_FLAG_NEGATIVE) != 0U)
        {
            *key_out = -*key_out;
        }
        return true;
    default:
        return false;
    }
}

/** 文字列キーを、項目の brief と ID で書き出します。カタログにない場合はキーの値を書き出します。 */
static void append_entry(describe_context *context, const string_catalog_filter_constant *constant)
{
    const language_phrases *phrases = context->phrases;
    const cplat_string_catalog_entry *entry = NULL;
    int key = 0;

    if (key_of_constant(context, constant, &key))
    {
        entry = cplat_string_catalog_get_entry(context->source->catalog, key);
    }

    /* カタログにない文字列キーは、条件式に書いた表記のまま示す */
    if (entry == NULL)
    {
        append_text(&context->writer, phrases->key_name_prefix);
        append_constant(context, constant);
        return;
    }

    append_text(&context->writer, phrases->entry_open);
    append_sentence(&context->writer, entry->brief);
    append_text(&context->writer, phrases->entry_close);
    if (entry->id != NULL)
    {
        append_text(&context->writer, phrases->entry_id_open);
        append_text(&context->writer, entry->id);
        append_text(&context->writer, ")");
    }
}

/**
 *  @brief          引数の定義を探します。
 *  @param[in]      entry  対象の文字列カタログ項目。
 *  @param[in]      name   引数名。NULL の場合は @p index で探します。
 *  @param[in]      index  引数のインデックス。@p name が NULL の場合に使用します。
 */
static const cplat_string_catalog_argument *find_argument(const cplat_string_catalog_entry *entry, const char *name,
                                                          const int index)
{
    if (name == NULL)
    {
        if ((index < entry->argument_count) &&
            (entry->arguments[index].kind != CPLAT_STRING_CATALOG_ARGUMENT_KIND_UNUSED))
        {
            return &entry->arguments[index];
        }
        return NULL;
    }

    for (int argument = 0; argument < entry->argument_count; argument++)
    {
        if ((entry->arguments[argument].kind != CPLAT_STRING_CATALOG_ARGUMENT_KIND_UNUSED) &&
            (entry->arguments[argument].name != NULL) && (strcmp(entry->arguments[argument].name, name) == 0))
        {
            return &entry->arguments[argument];
        }
    }
    return NULL;
}

/**
 *  @brief          引数を主語として書き出します。
 *
 *  行が 1 つの項目に限定されている場合は、その項目の引数の名前と説明を使います (設計資料の規則)。\n
 *  複数の項目が対象の場合でも、引数を持つすべての項目で名前と説明が一致すれば、それを使います (設計資料からの拡張)。\n
 *  一致しない場合は、名前だけ、またはインデックスだけを書き出します。
 */
static void append_argument(describe_context *context, const string_catalog_filter_instruction *instruction)
{
    const string_catalog_filter_describe_source *source = context->source;
    const cplat_string_catalog_argument *common = NULL;
    const char *name = NULL;
    string_catalog_filter_constant constant;
    bool is_name_common = true;
    bool is_description_common = true;
    char text[32];

    if (instruction->field == (uint8_t)STRING_CATALOG_FILTER_FIELD_ARGUMENT_NAME)
    {
        (void)string_catalog_filter_read_constant(context->constants, context->constant_size, instruction->operand,
                                                  &constant);
        name = constant.text;
    }

    if (source->single_entry != NULL)
    {
        common = find_argument(source->single_entry, name, instruction->argument);
    }
    else
    {
        for (int entry_index = 0; entry_index < source->catalog->entry_count; entry_index++)
        {
            const cplat_string_catalog_argument *argument =
                find_argument(&source->catalog->entries[entry_index], name, instruction->argument);

            if (argument == NULL)
            {
                continue;
            }
            if (common == NULL)
            {
                common = argument;
                continue;
            }
            if (strcmp(common->name, argument->name) != 0)
            {
                is_name_common = false;
            }
            if (strcmp(common->description, argument->description) != 0)
            {
                is_description_common = false;
            }
        }
    }

    append_text(&context->writer, context->phrases->argument_prefix);
    if (name != NULL)
    {
        append_text(&context->writer, name);
    }
    else if ((common != NULL) && is_name_common)
    {
        (void)snprintf(text, sizeof(text), "{%u} ", (unsigned int)instruction->argument);
        append_text(&context->writer, text);
        append_text(&context->writer, common->name);
    }
    else
    {
        (void)snprintf(text, sizeof(text), "{%u}", (unsigned int)instruction->argument);
        append_text(&context->writer, text);
    }

    if ((common != NULL) && is_description_common)
    {
        append_text(&context->writer, " (");
        append_sentence(&context->writer, common->description);
        append_text(&context->writer, ")");
    }
}

/** 判定要素の主語を書き出し、比較対象の定数の先頭オフセットを返します。 */
static uint32_t append_subject(describe_context *context, const string_catalog_filter_instruction *instruction)
{
    const language_phrases *phrases = context->phrases;
    string_catalog_filter_constant constant;

    switch (instruction->field)
    {
    case STRING_CATALOG_FILTER_FIELD_KEY:
        append_text(&context->writer, phrases->key_subject);
        return instruction->operand;
    case STRING_CATALOG_FILTER_FIELD_ID:
        append_text(&context->writer, phrases->id_subject);
        return instruction->operand;
    case STRING_CATALOG_FILTER_FIELD_CATEGORY:
        append_text(&context->writer, phrases->category_subject);
        return instruction->operand;
    case STRING_CATALOG_FILTER_FIELD_ARGUMENT_NAME:
        append_argument(context, instruction);
        /* 比較対象は引数名の定数の直後から並ぶ */
        (void)string_catalog_filter_read_constant(context->constants, context->constant_size, instruction->operand,
                                                  &constant);
        return constant.next_offset;
    default:
        append_argument(context, instruction);
        return instruction->operand;
    }
}

/* ===== 判定要素と演算子の木 ===== */

/** 比較対象の定数を、区切りを挟んで並べます。 */
static void append_constants(describe_context *context, const string_catalog_filter_instruction *instruction,
                             uint32_t offset, const bool is_entry)
{
    string_catalog_filter_constant constant;

    for (uint16_t index = 0; index < instruction->operand_count; index++)
    {
        if (index > 0U)
        {
            append_text(&context->writer, context->phrases->list_separator);
        }
        (void)string_catalog_filter_read_constant(context->constants, context->constant_size, offset, &constant);
        offset = constant.next_offset;
        if (is_entry)
        {
            append_entry(context, &constant);
        }
        else
        {
            append_constant(context, &constant);
        }
    }
}

/**
 *  @brief          文字列キーの一致、不一致、列挙を、項目の brief と ID で表します。
 *  @return         項目の表現で書き出した場合は true。大小の比較などは false を返し、一般の文型に任せます。
 */
static bool append_key_predicate(describe_context *context, const string_catalog_filter_instruction *instruction)
{
    const language_phrases *phrases = context->phrases;

    switch (instruction->operator_kind)
    {
    case STRING_CATALOG_FILTER_OPERATOR_EQUAL:
        append_constants(context, instruction, instruction->operand, true);
        return true;
    case STRING_CATALOG_FILTER_OPERATOR_NOT_EQUAL:
        append_text(&context->writer, phrases->key_not_equal_prefix);
        append_constants(context, instruction, instruction->operand, true);
        append_text(&context->writer, phrases->key_not_equal_suffix);
        return true;
    case STRING_CATALOG_FILTER_OPERATOR_IN:
        append_text(&context->writer, phrases->key_in_prefix);
        append_constants(context, instruction, instruction->operand, true);
        append_text(&context->writer, phrases->key_in_suffix);
        return true;
    default:
        return false;
    }
}

/** 比較対象の定数を、分類値との比較のための数値へ変換します。 */
static double real_of_constant(const describe_context *context, const string_catalog_filter_constant *constant)
{
    switch (constant->header.kind)
    {
    case STRING_CATALOG_FILTER_CONSTANT_KIND_FLOAT:
        return constant->real;
    case STRING_CATALOG_FILTER_CONSTANT_KIND_IDENTIFIER:
        return (double)context->source->identifier_values[constant->header.slot];
    default:
        if ((constant->header.flags & STRING_CATALOG_FILTER_CONSTANT_FLAG_NEGATIVE) != 0U)
        {
            return -(double)constant->magnitude;
        }
        return (double)constant->magnitude;
    }
}

/** 分類値が判定要素を満たすかを求めます。分類値の比較に使える演算子は数値の比較、in、between です。 */
static bool is_category_matched(const describe_context *context, const string_catalog_filter_instruction *instruction,
                                const double value)
{
    string_catalog_filter_constant constant;
    uint32_t offset = instruction->operand;

    if (instruction->operator_kind == (uint8_t)STRING_CATALOG_FILTER_OPERATOR_BETWEEN)
    {
        double lower;

        (void)string_catalog_filter_read_constant(context->constants, context->constant_size, offset, &constant);
        lower = real_of_constant(context, &constant);
        (void)string_catalog_filter_read_constant(context->constants, context->constant_size, constant.next_offset,
                                                  &constant);
        return (lower <= value) && (value <= real_of_constant(context, &constant));
    }

    for (uint16_t index = 0; index < instruction->operand_count; index++)
    {
        double literal;

        (void)string_catalog_filter_read_constant(context->constants, context->constant_size, offset, &constant);
        offset = constant.next_offset;
        literal = real_of_constant(context, &constant);

        switch (instruction->operator_kind)
        {
        case STRING_CATALOG_FILTER_OPERATOR_EQUAL:
        case STRING_CATALOG_FILTER_OPERATOR_IN:
            if (value == literal)
            {
                return true;
            }
            break;
        case STRING_CATALOG_FILTER_OPERATOR_NOT_EQUAL:
            return value != literal;
        case STRING_CATALOG_FILTER_OPERATOR_LESS:
            return value < literal;
        case STRING_CATALOG_FILTER_OPERATOR_LESS_EQUAL:
            return value <= literal;
        case STRING_CATALOG_FILTER_OPERATOR_GREATER:
            return value > literal;
        case STRING_CATALOG_FILTER_OPERATOR_GREATER_EQUAL:
            return value >= literal;
        default:
            return false;
        }
    }
    return false;
}

/**
 *  @brief          分類値の比較を、条件を満たす分類値の名前の列挙で表します。
 *
 *  大小の比較を名前の大小に言い換えると、分類値の並びの意味 (例: 重大度の向き) を読み手が補う必要があるためです。\n
 *  例: 名前が CRITICAL から NONE の順のとき、category <= 2 は「レベルが CRITICAL、ERROR、WARNING のいずれか」。\n
 *  満たさない名前のほうが少ない場合は補集合で表します。例: category != 4 は「レベルが VERBOSE 以外」。
 */
static void append_category_predicate(describe_context *context, const string_catalog_filter_instruction *instruction)
{
    const cplat_string_catalog_filter_category_names *names = context->source->category_names;
    const language_phrases *phrases = context->phrases;
    size_t matched_count = 0U;
    size_t listed_count;
    size_t written = 0U;
    bool is_complement;

    for (size_t value = 0; value < names->count; value++)
    {
        if (is_category_matched(context, instruction, (double)value))
        {
            matched_count++;
        }
    }

    if (context->source->is_japanese != 0)
    {
        append_text(&context->writer, names->subject_japanese);
    }
    else
    {
        append_text(&context->writer, names->subject_neutral);
    }

    if (matched_count == 0U)
    {
        append_before(context, phrases->category_none);
        return;
    }
    if (matched_count == names->count)
    {
        append_before(context, phrases->category_any);
        return;
    }

    /* 満たさない名前のほうが少なければ、補集合を「以外」で表す。列挙を短くするため */
    is_complement = ((names->count - matched_count) < matched_count);
    listed_count = matched_count;
    if (is_complement)
    {
        listed_count = names->count - matched_count;
    }

    if (is_complement)
    {
        append_before(context, phrases->category_other_before);
    }
    else if (listed_count == 1U)
    {
        append_before(context, phrases->operators[STRING_CATALOG_FILTER_OPERATOR_EQUAL].before);
    }
    else
    {
        append_before(context, phrases->operators[STRING_CATALOG_FILTER_OPERATOR_IN].before);
    }

    for (size_t value = 0; value < names->count; value++)
    {
        /* 補集合を表す場合は、条件を満たさない名前を並べる */
        if (is_category_matched(context, instruction, (double)value) == is_complement)
        {
            continue;
        }
        if (written > 0U)
        {
            append_text(&context->writer, phrases->list_separator);
        }
        append_text(&context->writer, names->names[value]);
        written++;
    }

    if (is_complement)
    {
        append_text(&context->writer, phrases->category_other_after);
    }
    else if (listed_count == 1U)
    {
        append_text(&context->writer, phrases->category_is_after);
    }
    else
    {
        append_text(&context->writer, phrases->operators[STRING_CATALOG_FILTER_OPERATOR_IN].after);
    }
}

static void append_predicate(describe_context *context, const string_catalog_filter_instruction *instruction)
{
    const language_phrases *phrases = context->phrases;
    string_catalog_filter_constant constant;
    uint32_t offset;

    if ((instruction->field == (uint8_t)STRING_CATALOG_FILTER_FIELD_KEY) && append_key_predicate(context, instruction))
    {
        return;
    }

    /* 分類値の名前が設定されていれば、分類値を名前で表す。設定がなければ数値のまま表す */
    if ((instruction->field == (uint8_t)STRING_CATALOG_FILTER_FIELD_CATEGORY) &&
        (context->source->category_names != NULL))
    {
        append_category_predicate(context, instruction);
        return;
    }

    if (instruction->operator_kind == (uint8_t)STRING_CATALOG_FILTER_OPERATOR_HAS)
    {
        append_text(&context->writer, phrases->has_prefix);
        (void)append_subject(context, instruction);
        append_text(&context->writer, phrases->has_suffix);
        return;
    }

    offset = append_subject(context, instruction);

    if (instruction->operator_kind == (uint8_t)STRING_CATALOG_FILTER_OPERATOR_BETWEEN)
    {
        append_before(context, phrases->between_before);
        (void)string_catalog_filter_read_constant(context->constants, context->constant_size, offset, &constant);
        append_constant(context, &constant);
        append_text(&context->writer, phrases->between_middle);
        (void)string_catalog_filter_read_constant(context->constants, context->constant_size, constant.next_offset,
                                                  &constant);
        append_constant(context, &constant);
        append_text(&context->writer, phrases->between_after);
        return;
    }

    if (instruction->operator_kind >= OPERATOR_TABLE_SIZE)
    {
        return;
    }

    append_before(context, phrases->operators[instruction->operator_kind].before);
    append_constants(context, instruction, offset, false);
    append_text(&context->writer, phrases->operators[instruction->operator_kind].after);

    if ((instruction->operator_kind == (uint8_t)STRING_CATALOG_FILTER_OPERATOR_STARTS_WITH_I) ||
        (instruction->operator_kind == (uint8_t)STRING_CATALOG_FILTER_OPERATOR_ENDS_WITH_I) ||
        (instruction->operator_kind == (uint8_t)STRING_CATALOG_FILTER_OPERATOR_CONTAINS_I) ||
        (instruction->operator_kind == (uint8_t)STRING_CATALOG_FILTER_OPERATOR_MATCHES_I))
    {
        append_text(&context->writer, phrases->case_insensitive);
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
 *  @brief          演算子の木を自然文で書き出します。
 *
 *  論理積と論理和は結合法則が成り立つため、同じ演算子の連なりには括弧を付けません。\n
 *  否定は対象を括弧で囲む文型のため、対象の優先順位によらず括弧を重ねません。
 */
static void append_node(describe_context *context, const uint16_t node, const int parent_precedence)
{
    const language_phrases *phrases = context->phrases;
    string_catalog_filter_instruction instruction;
    int precedence;
    bool needs_parenthesis;

    string_catalog_filter_read_instruction(context->source->record, node, &instruction);
    precedence = precedence_of(instruction.opcode);
    needs_parenthesis = (precedence < parent_precedence) && (parent_precedence != PRECEDENCE_NOT);

    if (needs_parenthesis)
    {
        append_text(&context->writer, "(");
    }

    switch (instruction.opcode)
    {
    case STRING_CATALOG_FILTER_OPCODE_PREDICATE:
        append_predicate(context, &instruction);
        break;

    case STRING_CATALOG_FILTER_OPCODE_NOT:
        append_text(&context->writer, phrases->not_prefix);
        append_node(context, context->left[node], precedence);
        append_text(&context->writer, phrases->not_suffix);
        break;

    case STRING_CATALOG_FILTER_OPCODE_AND:
    case STRING_CATALOG_FILTER_OPCODE_OR:
        append_node(context, context->left[node], precedence);
        if (instruction.opcode == (uint8_t)STRING_CATALOG_FILTER_OPCODE_AND)
        {
            append_text(&context->writer, phrases->and_text);
        }
        else
        {
            append_text(&context->writer, phrases->or_text);
        }
        append_node(context, context->right[node], precedence);
        break;

    default:
        break;
    }

    if (needs_parenthesis)
    {
        append_text(&context->writer, ")");
    }
}

/* ===== 型区分の混在 ===== */

/* Doxygen コメントは、ヘッダーに記載 */

bool string_catalog_filter_find_mixed_argument(const cplat_string_catalog *catalog, const char *name,
                                               string_catalog_filter_mixed_argument *mixed_out)
{
    bool has_string = false;
    bool has_other = false;

    memset(mixed_out, 0, sizeof(*mixed_out));
    for (int entry_index = 0; entry_index < catalog->entry_count; entry_index++)
    {
        const cplat_string_catalog_entry *entry = &catalog->entries[entry_index];

        /* 適用時の名前の解決と同じく、項目ごとに最初に名前が一致した引数を対象とする */
        for (int argument = 0; argument < entry->argument_count; argument++)
        {
            const cplat_string_catalog_argument *candidate = &entry->arguments[argument];

            if ((candidate->kind == CPLAT_STRING_CATALOG_ARGUMENT_KIND_UNUSED) || (candidate->name == NULL) ||
                (strcmp(candidate->name, name) != 0))
            {
                continue;
            }
            if ((candidate->kind == CPLAT_STRING_CATALOG_ARGUMENT_KIND_STRING) && !has_string)
            {
                mixed_out->string_entry = entry;
                mixed_out->string_argument = argument;
                has_string = true;
            }
            else if ((candidate->kind != CPLAT_STRING_CATALOG_ARGUMENT_KIND_STRING) && !has_other)
            {
                mixed_out->other_entry = entry;
                mixed_out->other_argument = argument;
                has_other = true;
            }
            else
            {
                /* 区分ごとに最初の項目だけを代表として残す */
            }
            break;
        }
        if (has_string && has_other)
        {
            return true;
        }
    }
    return false;
}

/**
 *  @brief          型区分が混在する引数名を、説明文の末尾に注記します。
 *
 *  比較に使う引数名を条件式に現れる順にたどり、同じ名前は 1 回だけ注記します。\n
 *  `has` は引数の有無だけを調べるため対象にしません。
 */
static void append_mixed_notes(describe_context *context)
{
    const char *noted[CPLAT_STRING_CATALOG_FILTER_ARGUMENT_REFERENCE_MAX];
    size_t noted_count = 0U;
    string_catalog_filter_record_header header;
    string_catalog_filter_instruction instruction;
    string_catalog_filter_constant constant;
    string_catalog_filter_mixed_argument mixed;

    string_catalog_filter_read_record_header(context->source->record, &header);
    for (uint32_t index = 0; index < header.instruction_count; index++)
    {
        bool is_noted = false;

        string_catalog_filter_read_instruction(context->source->record, index, &instruction);
        if ((instruction.opcode != (uint8_t)STRING_CATALOG_FILTER_OPCODE_PREDICATE) ||
            (instruction.field != (uint8_t)STRING_CATALOG_FILTER_FIELD_ARGUMENT_NAME) ||
            (instruction.operator_kind == (uint8_t)STRING_CATALOG_FILTER_OPERATOR_HAS))
        {
            continue;
        }

        (void)string_catalog_filter_read_constant(context->constants, context->constant_size, instruction.operand,
                                                  &constant);
        for (size_t other = 0; other < noted_count; other++)
        {
            if (strcmp(noted[other], constant.text) == 0)
            {
                is_noted = true;
                break;
            }
        }
        if (is_noted || (noted_count >= CPLAT_STRING_CATALOG_FILTER_ARGUMENT_REFERENCE_MAX) ||
            !string_catalog_filter_find_mixed_argument(context->source->catalog, constant.text, &mixed))
        {
            continue;
        }

        noted[noted_count] = constant.text;
        noted_count++;
        append_text(&context->writer, context->phrases->mixed_types_prefix);
        append_text(&context->writer, constant.text);
        append_text(&context->writer, context->phrases->mixed_types_suffix);
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

int string_catalog_filter_describe_record(const string_catalog_filter_describe_source *source, char *dest,
                                          const size_t dest_size)
{
    describe_context *context;
    describe_context context_storage;
    string_catalog_filter_record_header header;
    uint16_t root;

    context = &context_storage;
    memset(context, 0, sizeof(*context));
    context->source = source;
    if (source->is_japanese != 0)
    {
        context->phrases = &s_japanese;
    }
    else
    {
        context->phrases = &s_neutral;
    }

    string_catalog_filter_read_record_header(source->record, &header);
    context->constants = string_catalog_filter_record_constants(source->record, source->line_width);
    context->constant_size = header.constant_size;
    context->writer.dest = dest;
    context->writer.size = dest_size;
    dest[0] = '\0';

    root = string_catalog_filter_build_tree(source->record, context->left, context->right);
    append_node(context, root, 0);
    append_mixed_notes(context);

    if (context->writer.is_truncated)
    {
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }
    return CPLAT_OK;
}
