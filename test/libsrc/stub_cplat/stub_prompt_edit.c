/**
 *  @file           stub_prompt_edit.c
 *  @brief          プロンプト編集バッファーの内部 API を、呼び出し元の単体テスト向けに提供します。
 *
 *  prompt_edit.c のカバレッジは promptEditTest が担う。
 *  本スタブは UTF-8 文字境界、容量拡張、オプション解決、初期値の検証の契約だけを満たす。
 */

#include <cplat/prompt/prompt_edit.h>

#include <cplat/base/result.h>
#include <cplat/crt/stdlib.h>
#include <cplat/prompt/prompt.h>

#include <stdlib.h>

static int utf8_is_continuation(unsigned char c)
{
    return (c & 0xC0U) == 0x80U;
}

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

    /* 本番の prompt_edit.c と同じく cplat_realloc を使い、呼び出し元のテストが確保失敗を注入できるようにする */
    new_buf = (char *)cplat_realloc(*buf, new_cap, 1U);
    if (new_buf == NULL)
    {
        return -1;
    }
    *buf = new_buf;
    *cap = new_cap;
    return 0;
}

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
