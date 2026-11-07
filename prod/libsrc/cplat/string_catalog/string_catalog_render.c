/**
 *******************************************************************************
 *  @file           string_catalog_render.c
 *  @brief          位置指定書式の展開と構文確認を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/09/10
 *  @version        1.0.0
 *
 *  本モジュールが解釈する構文は、`{0}` から `{49}` までの位置指定と、`{{` と `}}` のエスケープのみです。\n
 *  値の文字列表現は引数種別側で規定されるため、書式内には書式指定子を記述できません。\n
 *  展開と構文確認は同一の走査ロジックで行い、出力先バッファーを指定しない呼び出しを構文確認として扱います。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include "string_catalog.h"

#include <cplat/base/result.h>
#include <cplat/crt/stdio.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/** 1 個の値を文字列化する一時領域のバイト数です。最長はエラー コードの併記表現で 24 [byte] です。 */
#define VALUE_TEXT_MAX 64

/** 10 進数へ変換した符号なし 64 bit 整数の最大桁数です。 */
#define DECIMAL_DIGITS_MAX 20

/** 文字列引数が NULL のときに出力する表現です。 */
#define NULL_STRING_TEXT "(null)"

/** 位置指定のインデックスに指定可能な最大桁数です。@ref CPLAT_STRING_CATALOG_ARGUMENT_MAX の桁数と一致させます。 */
#define INDEX_DIGITS_MAX 2

/** 1 文字として出力する ASCII の下限です。 */
#define CHAR_PRINTABLE_MIN 0x20U

/** 1 文字として出力する ASCII の上限です。 */
#define CHAR_PRINTABLE_MAX 0x7EU

/**
 *  @brief          符号なし整数を 10 進数へ変換します。
 *  @param[out]     out   書き込み先。@ref DECIMAL_DIGITS_MAX バイト以上の空きが必要です。
 *  @param[in]      value 変換する値。
 *  @return         書き込んだバイト数を返します。NUL 終端しません。
 *
 *  `snprintf` を使用しないのは、1 引数あたりの変換コストが数倍になるためです。\n
 *  書き込み先の容量は呼び出し側が確保するため、本関数では確認しません。
 */
static size_t decimal_from_uint64(char *const out, uint64_t value)
{
    char digits[DECIMAL_DIGITS_MAX];
    size_t count = 0U;
    size_t index;

    /* 下位の桁から得られるため、いったん逆順に作ってから並べ替える。 */
    do
    {
        digits[count] = (char)('0' + (int)(value % 10U));
        value /= 10U;
        count++;
    } while (value != 0U);

    for (index = 0U; index < count; index++)
    {
        out[index] = digits[count - 1U - index];
    }

    return count;
}

/**
 *  @brief          符号付き整数を 10 進数へ変換します。
 *  @param[out]     out   書き込み先。@ref DECIMAL_DIGITS_MAX に 1 を加えたバイト数以上の空きが必要です。
 *  @param[in]      value 変換する値。
 *  @return         書き込んだバイト数を返します。NUL 終端しません。
 */
static size_t decimal_from_int64(char *const out, const int64_t value)
{
    if (value < 0)
    {
        out[0] = '-';

        /* -value は最小値でオーバーフローするため、符号なしで絶対値を作る。 */
        return 1U + decimal_from_uint64(&out[1], (uint64_t)0U - (uint64_t)value);
    }

    return decimal_from_uint64(out, (uint64_t)value);
}

/**
 *  @brief          符号なし整数を、桁数を固定した 16 進数へ変換します。
 *  @param[out]     out    書き込み先。@p digits に 2 を加えたバイト数以上の空きが必要です。
 *  @param[in]      value  変換する値。
 *  @param[in]      digits 出力する桁数。16 以下を指定してください。
 *  @return         書き込んだバイト数を返します。`0x` の 2 [byte] を含みます。NUL 終端しません。
 *
 *  英小文字で出力します。@p digits に収まらない上位の桁は出力しません。
 */
static size_t hex_from_uint64(char *const out, const uint64_t value, const size_t digits)
{
    static const char table[] = "0123456789abcdef";
    size_t index;

    out[0] = '0';
    out[1] = 'x';

    for (index = 0U; index < digits; index++)
    {
        const size_t shift = (digits - 1U - index) * 4U;

        out[2U + index] = table[(size_t)((value >> shift) & 0xFU)];
    }

    return 2U + digits;
}

/**
 *  @brief          書き込み先と、書き込みの経過を保持します。
 *
 *  @ref render_buffer::dest が NULL のときは、書き込みを行わず構文だけを確認します。
 */
typedef struct render_buffer
{
    char *dest;           /**< 書き込み先です。NULL のときは構文確認だけを行います。 */
    size_t dest_size;     /**< dest のバイト数です。dest が NULL のときは 0 です。 */
    size_t length;        /**< dest へ書き込んだバイト数です。NUL 終端を含みません。 */
    bool is_truncated;    /**< 容量不足で切り詰めた場合に true になります。 */
    unsigned char pad[7]; /**< 明示的アラインメントです。0 を指定します。 */
} render_buffer;

/**
 *  @brief          指定した長さの文字列を書き込み先へ追加します。
 *  @param[in,out]  buffer 書き込み先と経過。NULL を渡してはなりません。
 *  @param[in]      text   追加する文字列。NULL を渡してはなりません。
 *  @param[in]      length @p text から追加するバイト数。
 *
 *  容量が不足する場合は入るところまで書き込み、切り詰めを記録します。\n
 *  書き込みを行った場合は常に NUL 終端します。
 */
static void render_buffer_append(render_buffer *buffer, const char *text, const size_t length)
{
    size_t capacity;
    size_t copy_length;

    if (buffer->dest == NULL)
    {
        return;
    }

    /* NUL 終端の 1 [byte] を除いた、本文を書き込める最大バイト数。 */
    capacity = buffer->dest_size - 1U;
    copy_length = length;

    if (copy_length > (capacity - buffer->length))
    {
        copy_length = capacity - buffer->length;
        buffer->is_truncated = true;
    }

    memcpy(&buffer->dest[buffer->length], text, copy_length);
    buffer->length += copy_length;
    buffer->dest[buffer->length] = '\0';
}

/**
 *  @brief          NUL 終端の文字列を書き込み先へ追加します。
 *  @param[in,out]  buffer 書き込み先と経過。NULL を渡してはなりません。
 *  @param[in]      text   追加する文字列。NULL を渡してはなりません。
 */
static void render_buffer_append_string(render_buffer *buffer, const char *text)
{
    render_buffer_append(buffer, text, strlen(text));
}

/**
 *  @brief          引数 1 個の文字列表現を書き込み先へ追加します。
 *  @param[in,out]  buffer 書き込み先と経過。NULL を渡してはなりません。
 *  @param[in]      value  追加する値。NULL を渡してはなりません。
 *  @return         成功時は @ref CPLAT_OK を返します。
 *  @return         引数種別が未知の場合、または引数を割り当てないインデックスを指す場合は
 *                  @ref CPLAT_ERR_MALFORMED_DEFINITION を返します。
 */
static int render_buffer_append_argument(render_buffer *buffer,
                                         const cplat_internal_string_catalog_argument_value *value)
{
    /* 各種別の出力は VALUE_TEXT_MAX に収まるため、切り詰めは発生しない。 */
    char value_text[VALUE_TEXT_MAX];
    size_t text_length;

    switch (value->kind)
    {
    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_UNUSED:
        /* 値を持たないインデックスを書式が参照しているため、書式の誤りとして扱う。 */
        return CPLAT_ERR_MALFORMED_DEFINITION;

    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_STRING:
        if (value->value.string_value == NULL)
        {
            render_buffer_append_string(buffer, NULL_STRING_TEXT);
        }
        else
        {
            render_buffer_append_string(buffer, value->value.string_value);
        }

        return CPLAT_OK;

    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_DOUBLE:
        /* 有効桁数を保つ丸め処理は独自実装せず、標準ライブラリに委任する。 */
        (void)cplat_snprintf(value_text, sizeof(value_text), "%g", value->value.double_value);
        render_buffer_append_string(buffer, value_text);
        return CPLAT_OK;

    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_CHAR:
    {
        /* isprint() はロケールに依存するため、ASCII の印字可能範囲を直接判定する。 */
        const unsigned int code = (unsigned int)(unsigned char)value->value.char_value;

        if ((code >= CHAR_PRINTABLE_MIN) && (code <= CHAR_PRINTABLE_MAX))
        {
            value_text[0] = '\'';
            value_text[1] = (char)code;
            value_text[2] = '\'';
            text_length = 3U;
        }
        else
        {
            text_length = decimal_from_uint64(value_text, (uint64_t)code);
            value_text[text_length] = ' ';
            value_text[text_length + 1U] = '(';
            text_length += 2U;
            text_length += hex_from_uint64(&value_text[text_length], (uint64_t)code, 2U);
            value_text[text_length] = ')';
            text_length += 1U;
        }
    }

    break;

    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_INT8:
        text_length = decimal_from_int64(value_text, (int64_t)value->value.int8_value);
        break;

    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_UINT8:
        text_length = decimal_from_uint64(value_text, (uint64_t)value->value.uint8_value);
        break;

    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_INT16:
        text_length = decimal_from_int64(value_text, (int64_t)value->value.int16_value);
        break;

    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_UINT16:
        text_length = decimal_from_uint64(value_text, (uint64_t)value->value.uint16_value);
        break;

    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_INT32:
        text_length = decimal_from_int64(value_text, (int64_t)value->value.int32_value);
        break;

    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_UINT32:
        text_length = decimal_from_uint64(value_text, (uint64_t)value->value.uint32_value);
        break;

    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_INT64:
        text_length = decimal_from_int64(value_text, value->value.int64_value);
        break;

    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_UINT64:
        text_length = decimal_from_uint64(value_text, value->value.uint64_value);
        break;

    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_HEX8:
        text_length = hex_from_uint64(value_text, (uint64_t)value->value.uint8_value, 2U);
        break;

    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_HEX16:
        text_length = hex_from_uint64(value_text, (uint64_t)value->value.uint16_value, 4U);
        break;

    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_HEX32:
        text_length = hex_from_uint64(value_text, (uint64_t)value->value.uint32_value, 8U);
        break;

    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_HEX64:
        text_length = hex_from_uint64(value_text, value->value.uint64_value, 16U);
        break;

    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_SIZE:
        text_length = decimal_from_uint64(value_text, (uint64_t)value->value.size_value);
        break;

    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_SSIZE:
        text_length = decimal_from_int64(value_text, value->value.int64_value);
        break;

    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_POINTER:
        /* %p の表現はプラットフォームで異なるため、ポインター幅の 16 進表現へ揃える。 */
        text_length = hex_from_uint64(value_text, (uint64_t)(uintptr_t)value->value.pointer_value, 16U);
        break;

    case CPLAT_STRING_CATALOG_ARGUMENT_KIND_ERROR_CODE:
        text_length = decimal_from_int64(value_text, (int64_t)value->value.error_code_value);
        value_text[text_length] = ' ';
        value_text[text_length + 1U] = '(';
        text_length += 2U;
        text_length += hex_from_uint64(&value_text[text_length], (uint64_t)(uint32_t)value->value.error_code_value, 8U);
        value_text[text_length] = ')';
        text_length += 1U;
        break;

    default:
        return CPLAT_ERR_MALFORMED_DEFINITION;
    }

    render_buffer_append(buffer, value_text, text_length);

    return CPLAT_OK;
}

/**
 *  @brief          書式を走査し、位置指定を展開します。
 *  @param[in,out]  buffer      書き込み先と経過。NULL を渡してはなりません。
 *  @param[in]      text        走査する書式。NULL を渡してはなりません。
 *  @param[in]      values      展開に使用する値の配列。NULL のときは構文確認だけを行います。
 *  @param[in]      arguments   引数の定義配列。NULL 以外のときは、引数を割り当てないインデックスを
 *                              参照していないことも確認します。
 *  @param[in]      value_count 位置指定が指してよい引数の個数。
 *  @return         成功時は @ref CPLAT_OK を返します。
 *  @return         構文が不正な場合、位置指定が @p value_count 以上のインデックスを指す場合、
 *                  または引数を割り当てないインデックスを指す場合は
 *                  @ref CPLAT_ERR_MALFORMED_DEFINITION を返します。
 */
static int render_scan_text(render_buffer *buffer, const char *text,
                            const cplat_internal_string_catalog_argument_value *values,
                            const cplat_string_catalog_argument *arguments, const int value_count)
{
    size_t position = 0U;

    while (text[position] != '\0')
    {
        size_t plain_length;
        int digit_count;
        int index;
        int ret;

        /* 位置指定とエスケープ以外は、連続する範囲をまとめて追加する。 */
        plain_length = strcspn(&text[position], "{}");
        if (plain_length > 0U)
        {
            render_buffer_append(buffer, &text[position], plain_length);
            position += plain_length;
            continue;
        }

        if (text[position] == '}')
        {
            if (text[position + 1U] != '}')
            {
                return CPLAT_ERR_MALFORMED_DEFINITION;
            }

            render_buffer_append(buffer, "}", 1U);
            position += 2U;
            continue;
        }

        if (text[position + 1U] == '{')
        {
            render_buffer_append(buffer, "{", 1U);
            position += 2U;
            continue;
        }

        /*
         * 位置指定のインデックスは 10 進数で、桁数は INDEX_DIGITS_MAX までとする。
         * 同一インデックスの表記揺れを防ぐため、先行ゼロは許可しない。
         */
        digit_count = 0;
        index = 0;

        while ((digit_count < INDEX_DIGITS_MAX) && (text[position + 1U + (size_t)digit_count] >= '0') &&
               (text[position + 1U + (size_t)digit_count] <= '9'))
        {
            index = (index * 10) + (text[position + 1U + (size_t)digit_count] - '0');
            digit_count++;
        }

        if ((digit_count == 0) || (text[position + 1U + (size_t)digit_count] != '}'))
        {
            return CPLAT_ERR_MALFORMED_DEFINITION;
        }

        if ((digit_count > 1) && (text[position + 1U] == '0'))
        {
            return CPLAT_ERR_MALFORMED_DEFINITION;
        }

        if (index >= value_count)
        {
            return CPLAT_ERR_MALFORMED_DEFINITION;
        }

        /* 値を伴う展開では、引数を割り当てないインデックスを値の種別から検出する。 */
        if ((arguments != NULL) && (arguments[index].kind == CPLAT_STRING_CATALOG_ARGUMENT_KIND_UNUSED))
        {
            return CPLAT_ERR_MALFORMED_DEFINITION;
        }

        if (values != NULL)
        {
            ret = render_buffer_append_argument(buffer, &values[index]);
            if (ret != CPLAT_OK)
            {
                return ret;
            }
        }

        position += 2U + (size_t)digit_count;
    }

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_string_catalog_render_text(char *dest, const size_t dest_size, const char *text,
                                              const cplat_internal_string_catalog_argument_value *values,
                                              const int value_count)
{
    render_buffer buffer = {0};
    int ret;

    buffer.dest = dest;
    buffer.dest_size = dest_size;
    dest[0] = '\0';

    ret = render_scan_text(&buffer, text, values, NULL, value_count);
    if (ret != CPLAT_OK)
    {
        return ret;
    }

    if (buffer.is_truncated)
    {
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int string_catalog_validate_text(const char *text, const cplat_string_catalog_argument *arguments,
                                 const int value_count)
{
    render_buffer buffer = {0};

    return render_scan_text(&buffer, text, NULL, arguments, value_count);
}
