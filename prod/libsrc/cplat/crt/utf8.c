/**
 *******************************************************************************
 *  @file           utf8.c
 *  @brief          UTF-8 文字列の境界処理を行う内部 API を実装します。
 *
 *  切り詰め位置の直前に不完全な UTF-8 多バイト文字が残らない長さを算出します。
 *
 *******************************************************************************
 */

#include <cplat/crt/utf8.h>

#include <cplat/base/char_code.h>

/* Doxygen コメントは、ヘッダーに記載 */

size_t cplat_internal_utf8_trimmed_length(const char *text, const size_t len)
{
    size_t pos;
    size_t cont_count;
    size_t expected_len;
    unsigned char lead;

    if (text == NULL || len == 0U)
    {
        return 0U;
    }

    pos = len - 1U;
    cont_count = 0U;

    /* 末尾の連続する後続バイト (10xxxxxx) を、最大 3 [byte] までさかのぼって先頭バイトの位置を探す。 */
    while (pos > 0U && cont_count < CPLAT_UTF8_MAX_CONT_BYTES &&
           (((unsigned char)text[pos] & CPLAT_UTF8_CONT_MASK) == CPLAT_UTF8_CONT_VALUE))
    {
        pos--;
        cont_count++;
    }

    lead = (unsigned char)text[pos];

    if ((lead & CPLAT_UTF8_LEAD2_MASK) == CPLAT_UTF8_LEAD2_VALUE)
    {
        expected_len = CPLAT_UTF8_BYTES_2;
    }
    else if ((lead & CPLAT_UTF8_LEAD3_MASK) == CPLAT_UTF8_LEAD3_VALUE)
    {
        expected_len = CPLAT_UTF8_BYTES_3;
    }
    else if ((lead & CPLAT_UTF8_LEAD4_MASK) == CPLAT_UTF8_LEAD4_VALUE)
    {
        expected_len = CPLAT_UTF8_BYTES_4;
    }
    else
    {
        /* ASCII 文字、または先頭バイトのない不正な後続バイトの並びは変更しない。 */
        return len;
    }

    if ((len - pos) < expected_len)
    {
        return pos;
    }

    return len;
}
