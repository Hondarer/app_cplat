/**
 *  @file           prompt_edit.c
 *  @brief          プロンプトの UTF-8 編集バッファーを管理する内部 API を実装します。
 */

#include <cplat/prompt/prompt_edit.h>

#include <cplat/base/result.h>

#include <cplat/prompt/prompt.h>
#include <cplat/crt/stdlib.h>

#include <stdlib.h>
#include <string.h>

static int utf8_is_continuation(unsigned char c)
{
    return (c & 0xC0U) == 0x80U;
}

/* Doxygen コメントは、ヘッダーに記載 */

size_t cplat_internal_prompt_edit_utf8_prev_boundary(const char *buf, size_t pos)
{
    if (pos == 0U)
    {
        return 0U;
    }
    pos--;
    while (pos > 0U && utf8_is_continuation((unsigned char)buf[pos]))
    {
        pos--;
    }
    return pos;
}

/* Doxygen コメントは、ヘッダーに記載 */

size_t cplat_internal_prompt_edit_utf8_next_boundary(const char *buf, size_t len, size_t pos)
{
    if (pos >= len)
    {
        return len;
    }
    pos++;
    while (pos < len && utf8_is_continuation((unsigned char)buf[pos]))
    {
        pos++;
    }
    return pos;
}

/* Doxygen コメントは、ヘッダーに記載 */

size_t cplat_internal_prompt_edit_utf8_sanitize_boundary(const char *buf, size_t len, size_t pos)
{
    if (pos > len)
    {
        pos = len;
    }
    while (pos > 0U && pos < len && utf8_is_continuation((unsigned char)buf[pos]))
    {
        pos--;
    }
    return pos;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_prompt_edit_ensure_capacity(char **buf, size_t *cap, size_t max_bytes, size_t required)
{
    size_t new_cap;
    char *new_buf;

    if (buf == NULL || cap == NULL)
    {
        return -1;
    }
    if (required <= *cap)
    {
        return 0;
    }
    if (required > max_bytes)
    {
        return -1;
    }

    new_cap = *cap;
    while (new_cap < required)
    {
        size_t next_cap = new_cap * 2U;
        if (next_cap <= new_cap || next_cap > max_bytes)
        {
            next_cap = max_bytes;
        }
        new_cap = next_cap;
    }

    new_buf = (char *)cplat_realloc(*buf, new_cap, 1U);
    if (new_buf == NULL)
    {
        return -1;
    }
    *buf = new_buf;
    *cap = new_cap;
    return 0;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_prompt_edit_resolve_options(size_t requested_history_max, size_t requested_initial_capacity,
                                                size_t requested_max_bytes, size_t initial_capacity_default,
                                                size_t *history_max, size_t *initial_capacity, size_t *max_bytes)
{
    size_t resolved_history_max = requested_history_max;
    size_t resolved_max_bytes = requested_max_bytes;
    size_t resolved_initial_capacity = requested_initial_capacity;

    if (resolved_history_max == 0U)
    {
        resolved_history_max = CPLAT_PROMPT_HISTORY_DEFAULT;
    }
    if (resolved_max_bytes == 0U)
    {
        resolved_max_bytes = CPLAT_PROMPT_INPUT_BYTES_DEFAULT;
    }
    if (resolved_max_bytes < 2U)
    {
        resolved_max_bytes = 2U;
    }
    if (resolved_initial_capacity == 0U)
    {
        resolved_initial_capacity = initial_capacity_default;
    }
    if (resolved_initial_capacity < 2U)
    {
        resolved_initial_capacity = 2U;
    }
    if (resolved_initial_capacity > resolved_max_bytes)
    {
        resolved_initial_capacity = resolved_max_bytes;
    }

    if (history_max != NULL)
    {
        *history_max = resolved_history_max;
    }
    if (initial_capacity != NULL)
    {
        *initial_capacity = resolved_initial_capacity;
    }
    if (max_bytes != NULL)
    {
        *max_bytes = resolved_max_bytes;
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_prompt_edit_validate_initial_text(const char *initial_text, size_t max_bytes, size_t *length_out)
{
    size_t length = 0U;

    if (length_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    *length_out = 0U;

    if (initial_text == NULL)
    {
        return CPLAT_OK;
    }

    while (initial_text[length] != '\0')
    {
        const unsigned char byte = (unsigned char)initial_text[length];

        if ((byte < 0x20U) || (byte == 0x7FU))
        {
            return CPLAT_ERR_INVALID_ARGUMENT;
        }
        length++;
    }

    if ((max_bytes == 0U) || (length > (max_bytes - 1U)))
    {
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    *length_out = length;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

size_t cplat_internal_prompt_edit_char_display_width(const char *buf, size_t len, size_t pos)
{
    unsigned char first_byte;
    unsigned int code_point;

    if (pos >= len)
        return 0U;

    first_byte = (unsigned char)buf[pos];

    if ((first_byte & 0x80U) == 0U)
    {
        return 1U;
    }

    if ((first_byte & 0xE0U) == 0xC0U && pos + 1U < len)
    {
        code_point = ((unsigned int)(first_byte & 0x1FU) << 6U) | (unsigned int)((unsigned char)buf[pos + 1U] & 0x3FU);
        if (code_point >= 0x0300U && code_point <= 0x036FU)
            return 0U;
        return 1U;
    }

    if ((first_byte & 0xF0U) == 0xE0U && pos + 2U < len)
    {
        code_point = ((unsigned int)(first_byte & 0x0FU) << 12U) |
                     ((unsigned int)((unsigned char)buf[pos + 1U] & 0x3FU) << 6U) |
                     (unsigned int)((unsigned char)buf[pos + 2U] & 0x3FU);

        if (code_point >= 0x4E00U && code_point <= 0x9FFFU)
            return 2U;
        if (code_point >= 0x3040U && code_point <= 0x309FU)
            return 2U;
        if (code_point >= 0x30A0U && code_point <= 0x30FFU)
            return 2U;
        if (code_point >= 0x3400U && code_point <= 0x4DBFU)
            return 2U;
        return 1U;
    }

    if ((first_byte & 0xF8U) == 0xF0U && pos + 3U < len)
    {
        code_point = ((unsigned int)(first_byte & 0x07U) << 18U) |
                     ((unsigned int)((unsigned char)buf[pos + 1U] & 0x3FU) << 12U) |
                     ((unsigned int)((unsigned char)buf[pos + 2U] & 0x3FU) << 6U) |
                     (unsigned int)((unsigned char)buf[pos + 3U] & 0x3FU);

        /* Unicode 17.0 の East_Asian_Width=W に含まれる補助平面の絵文字を 2 桁として扱います。
         * see: https://www.unicode.org/Public/17.0.0/ucd/EastAsianWidth.txt */
        static const unsigned int wide_symbols[][2] = {
            {0x1F004U, 0x1F004U}, {0x1F0CFU, 0x1F0CFU}, {0x1F18EU, 0x1F18EU}, {0x1F191U, 0x1F19AU},
            {0x1F200U, 0x1F202U}, {0x1F210U, 0x1F23BU}, {0x1F240U, 0x1F248U}, {0x1F250U, 0x1F251U},
            {0x1F260U, 0x1F265U}, {0x1F300U, 0x1F320U}, {0x1F32DU, 0x1F335U}, {0x1F337U, 0x1F37CU},
            {0x1F37EU, 0x1F393U}, {0x1F3A0U, 0x1F3CAU}, {0x1F3CFU, 0x1F3D3U}, {0x1F3E0U, 0x1F3F0U},
            {0x1F3F4U, 0x1F3F4U}, {0x1F3F8U, 0x1F43EU}, {0x1F440U, 0x1F440U}, {0x1F442U, 0x1F4FCU},
            {0x1F4FFU, 0x1F53DU}, {0x1F54BU, 0x1F54EU}, {0x1F550U, 0x1F567U}, {0x1F57AU, 0x1F57AU},
            {0x1F595U, 0x1F596U}, {0x1F5A4U, 0x1F5A4U}, {0x1F5FBU, 0x1F64FU}, {0x1F680U, 0x1F6C5U},
            {0x1F6CCU, 0x1F6CCU}, {0x1F6D0U, 0x1F6D2U}, {0x1F6D5U, 0x1F6D8U}, {0x1F6DCU, 0x1F6DFU},
            {0x1F6EBU, 0x1F6ECU}, {0x1F6F4U, 0x1F6FCU}, {0x1F7E0U, 0x1F7EBU}, {0x1F7F0U, 0x1F7F0U},
            {0x1F90CU, 0x1F93AU}, {0x1F93CU, 0x1F945U}, {0x1F947U, 0x1F9FFU}, {0x1FA70U, 0x1FA7CU},
            {0x1FA80U, 0x1FA8AU}, {0x1FA8EU, 0x1FAC6U}, {0x1FAC8U, 0x1FAC8U}, {0x1FACDU, 0x1FADCU},
            {0x1FADFU, 0x1FAEAU}, {0x1FAEFU, 0x1FAF8U},
        };
        size_t i;
        for (i = 0U; i < sizeof(wide_symbols) / sizeof(wide_symbols[0]); i++)
        {
            if (code_point >= wide_symbols[i][0] && code_point <= wide_symbols[i][1])
            {
                return 2U;
            }
        }

        if (code_point >= 0x20000U && code_point <= 0x2FFFFU)
            return 2U;
        return 1U;
    }

    return 1U;
}

/* Doxygen コメントは、ヘッダーに記載 */

size_t cplat_internal_prompt_edit_sgr_sequence_len(const char *buf, size_t len, size_t pos)
{
    size_t i;

    if (pos + 2U >= len || (unsigned char)buf[pos] != 0x1BU || buf[pos + 1U] != '[')
    {
        return 0U;
    }

    i = pos + 2U;
    while (i < len)
    {
        unsigned char ch;

        ch = (unsigned char)buf[i];
        if (ch == 'm')
        {
            return i - pos + 1U;
        }
        if ((ch >= 0x30U && ch <= 0x3FU) || (ch >= 0x20U && ch <= 0x2FU))
        {
            i++;
            continue;
        }
        return 0U;
    }
    return 0U;
}

/* Doxygen コメントは、ヘッダーに記載 */

size_t cplat_internal_prompt_edit_visible_bytes_from(const char *buf, size_t len, size_t start, size_t max_cols)
{
    size_t pos;
    size_t cols;
    size_t char_width;
    size_t sgr_len;

    pos = start;
    cols = 0U;
    while (pos < len)
    {
        sgr_len = cplat_internal_prompt_edit_sgr_sequence_len(buf, len, pos);
        if (sgr_len > 0U)
        {
            pos += sgr_len;
            continue;
        }
        if (cols >= max_cols)
        {
            break;
        }
        char_width = cplat_internal_prompt_edit_char_display_width(buf, len, pos);
        if (cols + char_width > max_cols)
            break;
        cols += char_width;
        pos = cplat_internal_prompt_edit_utf8_next_boundary(buf, len, pos);
    }
    return pos - start;
}

/* Doxygen コメントは、ヘッダーに記載 */

size_t cplat_internal_prompt_edit_display_width_between(const char *buf, size_t len, size_t start, size_t end)
{
    size_t pos;
    size_t width;
    size_t sgr_len;

    if (end > len)
        end = len;

    pos = start;
    width = 0U;
    while (pos < end)
    {
        sgr_len = cplat_internal_prompt_edit_sgr_sequence_len(buf, end, pos);
        if (sgr_len > 0U)
        {
            pos += sgr_len;
            continue;
        }
        width += cplat_internal_prompt_edit_char_display_width(buf, len, pos);
        pos = cplat_internal_prompt_edit_utf8_next_boundary(buf, len, pos);
    }
    return width;
}

/* Doxygen コメントは、ヘッダーに記載 */

size_t cplat_internal_prompt_edit_display_width(const char *text)
{
    size_t len = 0U;
    if (text != NULL)
    {
        len = strlen(text);
    }
    return cplat_internal_prompt_edit_display_width_between(text, len, 0U, len);
}

/* Doxygen コメントは、ヘッダーに記載 */

size_t cplat_internal_prompt_edit_adjust_view_start(const char *buf, size_t len, size_t cursor, size_t view_start,
                                                    size_t input_cols)
{
    view_start = cplat_internal_prompt_edit_utf8_sanitize_boundary(buf, len, view_start);
    if (cursor < view_start)
    {
        view_start = cursor;
    }
    while (cplat_internal_prompt_edit_display_width_between(buf, len, view_start, cursor) > input_cols)
    {
        view_start = cplat_internal_prompt_edit_utf8_next_boundary(buf, len, view_start);
    }
    return view_start;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_prompt_edit_utf8_complete(const char *buf, size_t len)
{
    size_t pos;
    size_t required;
    unsigned char first;
    if (len == 0U)
    {
        return 1;
    }
    pos = cplat_internal_prompt_edit_utf8_prev_boundary(buf, len);
    first = (unsigned char)buf[pos];
    if (first < 0x80U)
    {
        required = 1U;
    }
    else if (first >= 0xC2U && first <= 0xDFU)
    {
        required = 2U;
    }
    else if (first >= 0xE0U && first <= 0xEFU)
    {
        required = 3U;
    }
    else if (first >= 0xF0U && first <= 0xF4U)
    {
        required = 4U;
    }
    else
    {
        return 0;
    }
    return len - pos == required;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_prompt_edit_completions_reset(cplat_prompt_completions *completions, const char *line,
                                                  size_t cursor, size_t max_bytes)
{
    completions->line = line;
    completions->cursor = cursor;
    completions->max_bytes = max_bytes;
    completions->count = 0U;
    completions->start = 0U;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_prompt_edit_completions_dispose(cplat_prompt_completions *completions)
{
    cplat_free(completions->candidate);
    completions->candidate = NULL;
    completions->capacity = 0U;
    completions->count = 0U;
}

/* Doxygen コメントは、ヘッダーに記載 */

const char *cplat_internal_prompt_edit_completion_suffix(const cplat_prompt_completions *completions)
{
    const char *suffix;
    if (completions->count != 1U)
    {
        return NULL;
    }
    suffix = completions->candidate + completions->cursor - completions->start;
    if (*suffix == '\0')
    {
        return NULL;
    }
    return suffix;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_prompt_completions_add(cplat_prompt_completions *completions, size_t start, const char *candidate)
{
    size_t length;
    size_t prefix;
    int result;
    if (completions == NULL || candidate == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (start > completions->cursor ||
        cplat_internal_prompt_edit_utf8_sanitize_boundary(completions->line, completions->cursor, start) != start)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    result = cplat_internal_prompt_edit_validate_initial_text(candidate, completions->max_bytes, &length);
    if (result != CPLAT_OK)
    {
        return result;
    }
    prefix = completions->cursor - start;
    if (length < prefix || memcmp(candidate, completions->line + start, prefix) != 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (start >= completions->max_bytes || length >= completions->max_bytes - start)
    {
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }
    if (completions->count == 0U)
    {
        if (cplat_internal_prompt_edit_ensure_capacity(&completions->candidate, &completions->capacity,
                                                       completions->max_bytes, length + 1U) != 0)
        {
            return CPLAT_ERR_OUT_OF_MEMORY;
        }
        memcpy(completions->candidate, candidate, length + 1U);
        completions->start = start;
        completions->count = 1U;
    }
    else if (completions->start != start || strcmp(completions->candidate, candidate) != 0)
    {
        /* 2 件以上かどうかだけを保持し、候補数のオーバーフローを避ける。 */
        completions->count = 2U;
    }
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_prompt_edit_complete(cplat_prompt_completions *completions, cplat_prompt_completion_fn fn,
                                         void *user_data, const char *line, size_t len, size_t cursor, size_t max_bytes)
{
    cplat_internal_prompt_edit_completions_reset(completions, line, cursor, max_bytes);
    if (fn != NULL && len > 0U && cursor == len && cplat_internal_prompt_edit_utf8_complete(line, len))
    {
        fn(line, cursor, completions, user_data);
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_prompt_edit_completion_accept(cplat_prompt_completions *completions, char **buf, size_t *cap,
                                                 size_t max_bytes, size_t *len, size_t *cursor)
{
    const char *suffix = cplat_internal_prompt_edit_completion_suffix(completions);
    size_t length;
    if (suffix == NULL)
    {
        return 0;
    }
    length = completions->start + strlen(completions->candidate);
    if (cplat_internal_prompt_edit_ensure_capacity(buf, cap, max_bytes, length + 1U) != 0)
    {
        return 0;
    }
    memcpy(*buf + completions->start, completions->candidate, length - completions->start + 1U);
    *len = length;
    *cursor = length;
    completions->count = 0U;
    return 1;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_prompt_edit_utf16_feed(cplat_internal_prompt_utf16_input *input, unsigned int unit)
{
    unsigned int code_point = unit;
    if (input->high_surrogate != 0U)
    {
        unsigned int high = input->high_surrogate;
        input->high_surrogate = 0U;
        if (unit >= 0xDC00U && unit <= 0xDFFFU)
        {
            code_point = 0x10000U + ((high - 0xD800U) << 10U) + (unit - 0xDC00U);
        }
        else
        {
            input->pending_unit = unit;
            input->has_pending_unit = 1U;
            code_point = 0xFFFDU;
        }
    }
    else if (unit >= 0xD800U && unit <= 0xDBFFU)
    {
        input->high_surrogate = unit;
        return;
    }
    else if (unit >= 0xDC00U && unit <= 0xDFFFU)
    {
        code_point = 0xFFFDU;
    }

    input->offset = 0U;
    if (code_point <= 0x7FU)
    {
        input->length = 1U;
        input->bytes[0] = (unsigned char)code_point;
    }
    else if (code_point <= 0x7FFU)
    {
        input->length = 2U;
        input->bytes[0] = (unsigned char)(0xC0U | (code_point >> 6U));
        input->bytes[1] = (unsigned char)(0x80U | (code_point & 0x3FU));
    }
    else if (code_point <= 0xFFFFU)
    {
        input->length = 3U;
        input->bytes[0] = (unsigned char)(0xE0U | (code_point >> 12U));
        input->bytes[1] = (unsigned char)(0x80U | ((code_point >> 6U) & 0x3FU));
        input->bytes[2] = (unsigned char)(0x80U | (code_point & 0x3FU));
    }
    else
    {
        input->length = 4U;
        input->bytes[0] = (unsigned char)(0xF0U | (code_point >> 18U));
        input->bytes[1] = (unsigned char)(0x80U | ((code_point >> 12U) & 0x3FU));
        input->bytes[2] = (unsigned char)(0x80U | ((code_point >> 6U) & 0x3FU));
        input->bytes[3] = (unsigned char)(0x80U | (code_point & 0x3FU));
    }
}
