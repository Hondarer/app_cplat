/**
 *******************************************************************************
 *  @file           string.c
 *  @brief          string 系の CRT 関数を抽象化する API を実装します。
 *
 *  境界検証付き文字列操作と書式入力のプラットフォーム差異を吸収します。
 *
 *******************************************************************************
 */

#include <cplat/crt/string.h>

#include <stdio.h>
#include <string.h>

/**
 *  @brief          ASCII の大文字を小文字へ畳みます。
 *  @param[in]      c  対象のバイト。
 *  @return         `A` から `Z` なら対応する小文字、それ以外は元の値です。
 */
static int ascii_tolower(const unsigned char c)
{
    if ((c >= (unsigned char)'A') && (c <= (unsigned char)'Z'))
    {
        return (int)c + ((int)'a' - (int)'A');
    }

    return (int)c;
}

/**
 *  @brief          非 0 の差分を -1 / 1 へ正規化します。
 *  @param[in]      diff  減算による差分。0 以外であること。
 *  @return         負なら -1、正なら 1 です。
 */
static int cmp_sign(const int diff)
{
    if (diff < 0)
    {
        return -1;
    }

    return 1;
}

/**
 *  @brief          1 [byte] を ASCII 大小無視で比較し、比較を終えるかどうかを判定します。
 *  @param[in]      lhs_char  1 つ目の文字列のバイト。
 *  @param[in]      rhs_char  2 つ目の文字列のバイト。
 *  @param[out]     result    比較を終える場合の結果。@p lhs_char 側が小さいとき -1、一致するとき 0、大きいとき 1 です。
 *  @return         差分があるか終端に達したため比較を終える場合は 1、次のバイトへ進む場合は 0 です。
 */
static int ascii_casecmp_byte(const char lhs_char, const char rhs_char, int *result)
{
    const unsigned char left = (unsigned char)lhs_char;
    const unsigned char right = (unsigned char)rhs_char;
    const int diff = ascii_tolower(left) - ascii_tolower(right);

    if (diff != 0)
    {
        *result = cmp_sign(diff);
        return 1;
    }

    if (left == '\0')
    {
        *result = 0;
        return 1;
    }

    return 0;
}

/**
 *  @brief          非 NULL の 2 文字列を、終端まで ASCII 大小無視で比較します。
 *  @param[in]      lhs  比較する 1 つ目の文字列。
 *  @param[in]      rhs  比較する 2 つ目の文字列。
 *  @return         @p lhs が小さいとき -1、一致するとき 0、@p lhs が大きいとき 1 です。
 */
static int ascii_casecmp(const char *lhs, const char *rhs)
{
    size_t i;
    int result = 0;

    i = 0;
    while (ascii_casecmp_byte(lhs[i], rhs[i], &result) == 0)
    {
        i++;
    }

    return result;
}

/**
 *  @brief          非 NULL の 2 文字列を、最大 count バイトまで ASCII 大小無視で比較します。
 *  @param[in]      lhs    比較する 1 つ目の文字列。
 *  @param[in]      rhs    比較する 2 つ目の文字列。
 *  @param[in]      count  比較する最大バイト数。
 *  @return         @p lhs が小さいとき -1、一致するとき 0、@p lhs が大きいとき 1 です。
 */
static int ascii_casecmp_bounded(const char *lhs, const char *rhs, const size_t count)
{
    size_t i;
    int result = 0;

    for (i = 0; i < count; i++)
    {
        if (ascii_casecmp_byte(lhs[i], rhs[i], &result) != 0)
        {
            return result;
        }
    }

    return 0;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_strcpy(char *dest, const size_t dest_size, const char *src)
{
    size_t len;

    if (dest == NULL || dest_size == 0 || src == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    len = strlen(src);
    if (len + 1 > dest_size)
    {
        dest[0] = '\0';
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    memcpy(dest, src, len + 1);

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_strncpy(char *dest, const size_t dest_size, const char *src, const size_t count)
{
    size_t len;

    if (dest == NULL || dest_size == 0 || src == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    len = strlen(src);
    if (len > count)
    {
        len = count;
    }

    if (len >= dest_size)
    {
        len = dest_size - 1;
    }

    memcpy(dest, src, len);
    dest[len] = '\0';

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_strcat(char *dest, const size_t dest_size, const char *src)
{
    size_t dest_len;
    size_t src_len;

    if (dest == NULL || dest_size == 0 || src == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    dest_len = 0;
    while (dest_len < dest_size && dest[dest_len] != '\0')
    {
        dest_len++;
    }

    if (dest_len >= dest_size)
    {
        dest[0] = '\0';
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    src_len = strlen(src);
    if (dest_len + src_len + 1 > dest_size)
    {
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    memcpy(dest + dest_len, src, src_len + 1);

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_strncat(char *dest, const size_t dest_size, const char *src, const size_t count)
{
    size_t dest_len;
    size_t src_len;

    if (dest == NULL || dest_size == 0 || src == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    dest_len = 0;
    while (dest_len < dest_size && dest[dest_len] != '\0')
    {
        dest_len++;
    }

    if (dest_len >= dest_size)
    {
        dest[0] = '\0';
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    /* src が count 文字未満で終端している場合に備え、終端までの長さで頭打ちにする。 */
    src_len = 0;
    while (src_len < count && src[src_len] != '\0')
    {
        src_len++;
    }

    if (dest_len + src_len + 1 > dest_size)
    {
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    memcpy(dest + dest_len, src, src_len);
    dest[dest_len + src_len] = '\0';

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

char *cplat_strtok_r(char *str, const char *delim, char **saveptr)
{
    if (delim == NULL || saveptr == NULL)
    {
        return NULL;
    }

#if defined(PLATFORM_LINUX)
    return strtok_r(str, delim, saveptr);
#elif defined(PLATFORM_WINDOWS)
    /*
     * Windows では再入可能版の名前が strtok_s() である。引数と意味は strtok_r() と同じである。
     * see: https://learn.microsoft.com/cpp/c-runtime-library/reference/strtok-s-strtok-s-l-wcstok-s-wcstok-s-l-mbstok-s-mbstok-s-l
     */
    return strtok_s(str, delim, saveptr);
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_wcscpy(wchar_t *dest, const size_t dest_size, const wchar_t *src)
{
    size_t len;

    if (dest == NULL || dest_size == 0 || src == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    len = wcslen(src);
    if (len + 1 > dest_size)
    {
        dest[0] = L'\0';
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    wmemcpy(dest, src, len + 1);

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_vsscanf(const char *buffer, const char *format, va_list args)
{
    return vsscanf(buffer, format, args);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_sscanf(const char *buffer, const char *format, ...)
{
    va_list args;
    int result;

    va_start(args, format);
    result = cplat_vsscanf(buffer, format, args);
    va_end(args);

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

char *cplat_strdup(const char *src)
{
    if (src == NULL)
    {
        return NULL;
    }

#if defined(PLATFORM_LINUX)
    return strdup(src);
#elif defined(PLATFORM_WINDOWS)
    /*
     * MSVC では strdup() が C4996 で非推奨のため _strdup() を使用する。
     * see: https://learn.microsoft.com/cpp/c-runtime-library/reference/strdup-wcsdup-mbsdup
     */
    return _strdup(src);
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_strcasecmp(const char *lhs, const char *rhs)
{
    if (lhs == NULL && rhs == NULL)
    {
        return 0;
    }

    if (lhs == NULL)
    {
        return -1;
    }

    if (rhs == NULL)
    {
        return 1;
    }

    return ascii_casecmp(lhs, rhs);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_strncasecmp(const char *lhs, const char *rhs, const size_t count)
{
    if (count == 0u)
    {
        return 0;
    }

    if (lhs == NULL && rhs == NULL)
    {
        return 0;
    }

    if (lhs == NULL)
    {
        return -1;
    }

    if (rhs == NULL)
    {
        return 1;
    }

    return ascii_casecmp_bounded(lhs, rhs, count);
}
