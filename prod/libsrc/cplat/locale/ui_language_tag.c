/**
 *******************************************************************************
 *  @file           ui_language_tag.c
 *  @brief          ロケールの指定を言語タグへ正規化する内部 API を実装します。
 *
 *  環境変数の値と OS が返すロケール名は、区切り文字、文字コードの付加、
 *  大文字と小文字の使い方が一定ではありません。\n
 *  本ファイルは、これらを言語、表記体系、地域の区別へ分解し、決まった表記へそろえます。
 *
 *******************************************************************************
 */

#include <cplat/locale/ui_language_internal.h>

#include <cplat/base/result.h>
#include <cplat/crt/string.h>
#include <cplat/locale/ui_language.h>

#include <ctype.h>
#include <string.h>

/** 言語の区別として受け付ける最小の文字数です。 */
#define LANGUAGE_LENGTH_MIN 2U

/** 言語の区別として受け付ける最大の文字数です。 */
#define LANGUAGE_LENGTH_MAX 3U

/** 表記体系の区別の文字数です。 */
#define SCRIPT_LENGTH 4U

/** 英字で表す地域の区別の文字数です。 */
#define REGION_ALPHA_LENGTH 2U

/** 数字で表す地域の区別の文字数です。 */
#define REGION_DIGIT_LENGTH 3U

/**
 *  @brief          指定された範囲がすべて英字かどうかを判定します。
 *  @param[in]      text    判定する文字列。NULL を渡してはなりません。
 *  @param[in]      length  判定する文字数。
 *  @return         すべて英字の場合は 1、それ以外は 0 を返します。
 */
static int is_alpha_text(const char *const text, const size_t length)
{
    size_t index;

    for (index = 0U; index < length; index++)
    {
        if (isalpha((unsigned char)text[index]) == 0)
        {
            return 0;
        }
    }

    return 1;
}

/**
 *  @brief          指定された範囲がすべて数字かどうかを判定します。
 *  @param[in]      text    判定する文字列。NULL を渡してはなりません。
 *  @param[in]      length  判定する文字数。
 *  @return         すべて数字の場合は 1、それ以外は 0 を返します。
 */
static int is_digit_text(const char *const text, const size_t length)
{
    size_t index;

    for (index = 0U; index < length; index++)
    {
        if (isdigit((unsigned char)text[index]) == 0)
        {
            return 0;
        }
    }

    return 1;
}

/**
 *  @brief          区切り文字または終端までの文字数を返します。
 *  @param[in]      text  走査する文字列。NULL を渡してはなりません。
 *  @return         区切り文字 (`_` または `-`) または終端までの文字数を返します。
 */
static size_t subtag_length(const char *const text)
{
    size_t length = 0U;

    while ((text[length] != '\0') && (text[length] != '_') && (text[length] != '-'))
    {
        length++;
    }

    return length;
}

/**
 *  @brief          区別を言語タグへ追加します。
 *  @param[out]     tag                 言語タグの組み立て先。NULL を渡してはなりません。
 *  @param[in,out]  tag_length_inout    組み立て済みの文字数。追加した文字数を加えて返します。
 *  @param[in]      text                追加する区別。NULL を渡してはなりません。
 *  @param[in]      length              @p text の文字数。
 *  @param[in]      upper_length        先頭から大文字にする文字数。残りは小文字にします。
 *
 *  すでに区別が 1 つ以上ある場合は、区切りのハイフンを先に追加します。\n
 *  呼び出し側は、追加後の文字数が @p tag の容量に収まることを保証してください。
 */
static void append_subtag(char *const tag, size_t *const tag_length_inout, const char *const text, const size_t length,
                          const size_t upper_length)
{
    size_t tag_length = *tag_length_inout;
    size_t index;

    if (tag_length > 0U)
    {
        tag[tag_length] = '-';
        tag_length++;
    }

    for (index = 0U; index < length; index++)
    {
        const unsigned char source = (unsigned char)text[index];

        if (index < upper_length)
        {
            tag[tag_length] = (char)toupper(source);
        }
        else
        {
            tag[tag_length] = (char)tolower(source);
        }
        tag_length++;
    }

    *tag_length_inout = tag_length;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_ui_language_normalize(const char *const value, char *const tag_out, const size_t tag_size)
{
    /* 言語、表記体系、地域と区切りを合わせても言語タグの上限に収まるため、組み立て先は上限の配列とする */
    char tag[CPLAT_UI_LANGUAGE_TAG_MAX];
    char work[CPLAT_UI_LANGUAGE_TAG_MAX];
    size_t tag_length = 0U;
    size_t work_length;
    size_t offset;
    int has_script = 0;
    int has_region = 0;

    if ((value == NULL) || (tag_out == NULL) || (tag_size == 0U))
    {
        if ((tag_out != NULL) && (tag_size > 0U))
        {
            tag_out[0] = '\0';
        }
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    tag_out[0] = '\0';

    /* 文字コード (`.` 以降) と修飾子 (`@` 以降) は言語を表さないため取り除く */
    work_length = strcspn(value, ".@");
    if ((work_length == 0U) || (work_length >= sizeof(work)))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    memcpy(work, value, work_length);
    work[work_length] = '\0';

    /* C と POSIX は、言語が不明であることではなく、言語に依存しない出力の指定である */
    if ((cplat_strcasecmp(work, "C") == 0) || (cplat_strcasecmp(work, "POSIX") == 0))
    {
        return CPLAT_OK;
    }

    offset = subtag_length(work);
    if ((offset < LANGUAGE_LENGTH_MIN) || (offset > LANGUAGE_LENGTH_MAX) || (is_alpha_text(work, offset) == 0))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    append_subtag(tag, &tag_length, work, offset, 0U);

    while (work[offset] != '\0')
    {
        const char *subtag;
        size_t length;

        /* 区切り文字を読み飛ばす */
        offset++;
        subtag = &work[offset];
        length = subtag_length(subtag);

        if (length == 0U)
        {
            return CPLAT_ERR_INVALID_ARGUMENT;
        }

        if ((has_script == 0) && (has_region == 0) && (length == SCRIPT_LENGTH) && (is_alpha_text(subtag, length) != 0))
        {
            append_subtag(tag, &tag_length, subtag, length, 1U);
            has_script = 1;
        }
        else if ((has_region == 0) && ((((length == REGION_ALPHA_LENGTH) && (is_alpha_text(subtag, length) != 0))) ||
                                       ((length == REGION_DIGIT_LENGTH) && (is_digit_text(subtag, length) != 0))))
        {
            append_subtag(tag, &tag_length, subtag, length, length);
            has_region = 1;
        }
        else
        {
            /* 表記体系と地域より後ろの区別は、表示する文言の選択に使用しないため取り込まない */
            break;
        }

        offset += length;
    }

    tag[tag_length] = '\0';

    return cplat_strcpy(tag_out, tag_size, tag);
}
