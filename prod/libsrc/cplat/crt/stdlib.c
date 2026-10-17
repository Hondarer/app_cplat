/**
 *******************************************************************************
 *  @file           stdlib.c
 *  @brief          stdlib 系の CRT 関数を抽象化する API を実装します。
 *
 *  環境変数の有無とバッファー不足を共通の戻り値へ変換します。\n
 *  文字列から数値への変換は、変換位置と `errno` の検査を関数側へ内包し、共通結果コードへ変換します。\n
 *  メモリ確保は、長さ 0 と要素数の乗算オーバーフローの検査を関数側へ内包します。
 *
 *******************************************************************************
 */

#include <cplat/crt/stdlib.h>
#include <cplat/base/error_internal.h>
#include <cplat/base/platform.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* 基本整数型 API の委譲先と値域が一致することを保証する。 */
_Static_assert(CHAR_BIT == 8 && CHAR_MIN == INT8_MIN && CHAR_MAX == INT8_MAX, "cplat: char must be signed 8-bit");
_Static_assert(SCHAR_MIN == INT8_MIN && SCHAR_MAX == INT8_MAX && UCHAR_MAX == UINT8_MAX,
               "cplat: signed/unsigned char must be 8-bit");
_Static_assert(SHRT_MIN == INT16_MIN && SHRT_MAX == INT16_MAX && USHRT_MAX == UINT16_MAX,
               "cplat: short must be 16-bit");
_Static_assert(INT_MIN == INT32_MIN && INT_MAX == INT32_MAX && UINT_MAX == UINT32_MAX, "cplat: int must be 32-bit");
_Static_assert(LLONG_MIN == INT64_MIN && LLONG_MAX == INT64_MAX && ULLONG_MAX == UINT64_MAX,
               "cplat: long long must be 64-bit");

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_getenv(const char *name, char *buf, const size_t buf_size, int *exists_out, cplat_error *detail_out)
{
    if (exists_out != NULL)
    {
        *exists_out = 0;
    }
    if (name == NULL)
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

#if defined(PLATFORM_LINUX)
    {
        const char *val = getenv(name);
        if (val == NULL)
        {
            if (buf != NULL && buf_size > 0)
            {
                buf[0] = '\0';
            }
            return cplat_internal_error_report_success(detail_out);
        }
        if (exists_out != NULL)
        {
            *exists_out = 1;
        }
        if (buf != NULL)
        {
            size_t len = strlen(val);
            if (len + 1 > buf_size)
            {
                return cplat_internal_error_report_errno(detail_out, ERANGE);
            }
            memcpy(buf, val, len + 1);
        }
        return cplat_internal_error_report_success(detail_out);
    }
#elif defined(PLATFORM_WINDOWS)
    {
        char *val = NULL;
        size_t val_len = 0;
        const errno_t err = _dupenv_s(&val, &val_len, name);

        if (err != 0)
        {
            free(val);
            return cplat_internal_error_report_errno(detail_out, (int)err);
        }
        if (val == NULL)
        {
            if (buf != NULL && buf_size > 0)
            {
                buf[0] = '\0';
            }
            return cplat_internal_error_report_success(detail_out);
        }
        if (exists_out != NULL)
        {
            *exists_out = 1;
        }
        if (buf != NULL)
        {
            if (val_len > buf_size)
            {
                free(val);
                return cplat_internal_error_report_errno(detail_out, ERANGE);
            }
            memcpy(buf, val, val_len);
        }
        free(val);
        return cplat_internal_error_report_success(detail_out);
    }
#endif /* PLATFORM_ */
}

/* 環境変数名として妥当かを判定する。空文字列と '=' を含む名前は POSIX / MSVC の
 * いずれでも不正であるため、プラットフォーム差を出さないよう先に弾く。 */
static int env_name_is_valid(const char *name)
{
    if (name == NULL || name[0] == '\0')
    {
        return 0;
    }
    if (strchr(name, '=') != NULL)
    {
        return 0;
    }
    return 1;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_setenv(const char *name, const char *value, const int overwrite, cplat_error *detail_out)
{
    if (!env_name_is_valid(name) || value == NULL)
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

#if defined(PLATFORM_LINUX)
    {
        if (setenv(name, value, overwrite) != 0)
        {
            const int errno_value = errno;

            return cplat_internal_error_report_errno(detail_out, errno_value);
        }
        return cplat_internal_error_report_success(detail_out);
    }
#elif defined(PLATFORM_WINDOWS)
    {
        /* _putenv_s に overwrite 相当の引数はないため、既存の有無を自前で確認する */
        if (overwrite == 0)
        {
            int exists = 0;

            if (cplat_getenv(name, NULL, 0u, &exists, NULL) == CPLAT_OK && exists != 0)
            {
                return cplat_internal_error_report_success(detail_out);
            }
        }

        if (_putenv_s(name, value) != 0)
        {
            int errno_value = errno;

            if (errno_value == 0)
            {
                errno_value = EIO;
            }

            return cplat_internal_error_report_errno(detail_out, errno_value);
        }
        return cplat_internal_error_report_success(detail_out);
    }
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_unsetenv(const char *name, cplat_error *detail_out)
{
    if (!env_name_is_valid(name))
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

#if defined(PLATFORM_LINUX)
    {
        if (unsetenv(name) != 0)
        {
            const int errno_value = errno;

            return cplat_internal_error_report_errno(detail_out, errno_value);
        }
        return cplat_internal_error_report_success(detail_out);
    }
#elif defined(PLATFORM_WINDOWS)
    {
        /* Windows は値に空文字列を指定した _putenv_s を削除として扱う。
         * see: https://learn.microsoft.com/cpp/c-runtime-library/reference/putenv-s-wputenv-s */
        if (_putenv_s(name, "") != 0)
        {
            int errno_value = errno;

            if (errno_value == 0)
            {
                errno_value = EIO;
            }

            return cplat_internal_error_report_errno(detail_out, errno_value);
        }
        return cplat_internal_error_report_success(detail_out);
    }
#endif /* PLATFORM_ */
}

/**
 *  @brief          strtoll 系へ渡す基数として妥当かを判定します。
 *  @param[in]      base 判定する基数。
 *  @return         妥当な場合は 1、そうでない場合は 0 を返します。
 *
 *  0 (接頭辞による自動判別) と 2 から 36 を妥当とします。
 *  これ以外の値に対する strtoll 系の動作は未定義であるため、変換前に弾きます。
 */
static int parse_base_is_valid(const int base)
{
    if (base == 0)
    {
        return 1;
    }
    if ((base >= 2) && (base <= 36))
    {
        return 1;
    }

    return 0;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_parse_int64(int64_t *value_out, const char *text, const int base)
{
    char *end;
    long long parsed;

    if (value_out == NULL || text == NULL || parse_base_is_valid(base) == 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    end = NULL;
    errno = 0;
    parsed = strtoll(text, &end, base);

    /* end == text は 1 文字も変換できなかったことを表す。空文字列はこの条件だけで検出される。 */
    if (end == text || *end != '\0')
    {
        return CPLAT_ERR_INVALID_INTEGER;
    }
    if (errno == ERANGE)
    {
        return CPLAT_ERR_OUT_OF_RANGE;
    }

    *value_out = (int64_t)parsed;

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_parse_uint64(uint64_t *value_out, const char *text, const int base)
{
    const char *cursor;
    char *end;
    unsigned long long parsed;

    if (value_out == NULL || text == NULL || parse_base_is_valid(base) == 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    /* strtoull は '-' 付きの入力を符号なしの折り返し値として受け付けるため、変換前に拒否する。
     * see: https://en.cppreference.com/w/c/string/byte/strtoul */
    cursor = text;
    while (isspace((unsigned char)*cursor) != 0)
    {
        cursor++;
    }
    if (*cursor == '-')
    {
        return CPLAT_ERR_OUT_OF_RANGE;
    }

    end = NULL;
    errno = 0;
    parsed = strtoull(text, &end, base);

    if (end == text || *end != '\0')
    {
        return CPLAT_ERR_INVALID_INTEGER;
    }
    if (errno == ERANGE)
    {
        return CPLAT_ERR_OUT_OF_RANGE;
    }

    *value_out = (uint64_t)parsed;

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_parse_int8(int8_t *value_out, const char *text, const int base)
{
    int64_t value;
    int ret;

    if (value_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    ret = cplat_parse_int64(&value, text, base);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    if ((value < (int64_t)INT8_MIN) || (value > (int64_t)INT8_MAX))
    {
        return CPLAT_ERR_OUT_OF_RANGE;
    }

    *value_out = (int8_t)value;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_parse_int16(int16_t *value_out, const char *text, const int base)
{
    int64_t value;
    int ret;

    if (value_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    ret = cplat_parse_int64(&value, text, base);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    if ((value < (int64_t)INT16_MIN) || (value > (int64_t)INT16_MAX))
    {
        return CPLAT_ERR_OUT_OF_RANGE;
    }

    *value_out = (int16_t)value;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_parse_int32(int32_t *value_out, const char *text, const int base)
{
    int64_t value;
    int ret;

    if (value_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    ret = cplat_parse_int64(&value, text, base);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    if ((value < (int64_t)INT32_MIN) || (value > (int64_t)INT32_MAX))
    {
        return CPLAT_ERR_OUT_OF_RANGE;
    }

    *value_out = (int32_t)value;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_parse_uint8(uint8_t *value_out, const char *text, const int base)
{
    uint64_t value;
    int ret;

    if (value_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    ret = cplat_parse_uint64(&value, text, base);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    if (value > (uint64_t)UINT8_MAX)
    {
        return CPLAT_ERR_OUT_OF_RANGE;
    }

    *value_out = (uint8_t)value;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_parse_uint16(uint16_t *value_out, const char *text, const int base)
{
    uint64_t value;
    int ret;

    if (value_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    ret = cplat_parse_uint64(&value, text, base);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    if (value > (uint64_t)UINT16_MAX)
    {
        return CPLAT_ERR_OUT_OF_RANGE;
    }

    *value_out = (uint16_t)value;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_parse_uint32(uint32_t *value_out, const char *text, const int base)
{
    uint64_t value;
    int ret;

    if (value_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    ret = cplat_parse_uint64(&value, text, base);
    if (ret != CPLAT_OK)
    {
        return ret;
    }
    if (value > (uint64_t)UINT32_MAX)
    {
        return CPLAT_ERR_OUT_OF_RANGE;
    }

    *value_out = (uint32_t)value;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_parse_char(char *value_out, const char *text, const int base)
{
    int8_t value;
    int ret;

    if (value_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    ret = cplat_parse_int8(&value, text, base);
    if (ret != CPLAT_OK)
    {
        return ret;
    }

    /* 委譲先と目的型の値域はファイル先頭の静的検査で一致を保証する。 */
    *value_out = (char)value;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_parse_schar(signed char *value_out, const char *text, const int base)
{
    int8_t value;
    int ret;

    if (value_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    ret = cplat_parse_int8(&value, text, base);
    if (ret != CPLAT_OK)
    {
        return ret;
    }

    /* 委譲先と目的型の値域はファイル先頭の静的検査で一致を保証する。 */
    *value_out = (signed char)value;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_parse_uchar(unsigned char *value_out, const char *text, const int base)
{
    uint8_t value;
    int ret;

    if (value_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    ret = cplat_parse_uint8(&value, text, base);
    if (ret != CPLAT_OK)
    {
        return ret;
    }

    /* 委譲先と目的型の値域はファイル先頭の静的検査で一致を保証する。 */
    *value_out = (unsigned char)value;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_parse_short(short *value_out, const char *text, const int base)
{
    int16_t value;
    int ret;

    if (value_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    ret = cplat_parse_int16(&value, text, base);
    if (ret != CPLAT_OK)
    {
        return ret;
    }

    /* 委譲先と目的型の値域はファイル先頭の静的検査で一致を保証する。 */
    *value_out = (short)value;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_parse_ushort(unsigned short *value_out, const char *text, const int base)
{
    uint16_t value;
    int ret;

    if (value_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    ret = cplat_parse_uint16(&value, text, base);
    if (ret != CPLAT_OK)
    {
        return ret;
    }

    /* 委譲先と目的型の値域はファイル先頭の静的検査で一致を保証する。 */
    *value_out = (unsigned short)value;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_parse_int(int *value_out, const char *text, const int base)
{
    int32_t value;
    int ret;

    if (value_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    ret = cplat_parse_int32(&value, text, base);
    if (ret != CPLAT_OK)
    {
        return ret;
    }

    /* 委譲先と目的型の値域はファイル先頭の静的検査で一致を保証する。 */
    *value_out = (int)value;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_parse_uint(unsigned int *value_out, const char *text, const int base)
{
    uint32_t value;
    int ret;

    if (value_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    ret = cplat_parse_uint32(&value, text, base);
    if (ret != CPLAT_OK)
    {
        return ret;
    }

    /* 委譲先と目的型の値域はファイル先頭の静的検査で一致を保証する。 */
    *value_out = (unsigned int)value;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_parse_longlong(long long *value_out, const char *text, const int base)
{
    int64_t value;
    int ret;

    if (value_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    ret = cplat_parse_int64(&value, text, base);
    if (ret != CPLAT_OK)
    {
        return ret;
    }

    /* 委譲先と目的型の値域はファイル先頭の静的検査で一致を保証する。 */
    *value_out = (long long)value;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_parse_ulonglong(unsigned long long *value_out, const char *text, const int base)
{
    uint64_t value;
    int ret;

    if (value_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    ret = cplat_parse_uint64(&value, text, base);
    if (ret != CPLAT_OK)
    {
        return ret;
    }

    /* 委譲先と目的型の値域はファイル先頭の静的検査で一致を保証する。 */
    *value_out = (unsigned long long)value;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_parse_double(double *value_out, const char *text)
{
    char *end;
    double parsed;

    if (value_out == NULL || text == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    end = NULL;
    errno = 0;
    parsed = strtod(text, &end);

    if (end == text || *end != '\0')
    {
        return CPLAT_ERR_INVALID_INTEGER;
    }

    /* strtod はオーバーフローとアンダーフローの双方で ERANGE を設定する。
     * see: https://en.cppreference.com/w/c/string/byte/strtof */
    if (errno == ERANGE)
    {
        return CPLAT_ERR_OUT_OF_RANGE;
    }

    *value_out = parsed;

    return CPLAT_OK;
}

/* NOTE: 以降の確保・解放関数は、上位規範「動的メモリの確保と解放」および
 *       cplat 規範「メモリ確保の代替」が定める代替の実体である。
 *       本ファイル内の malloc / calloc / realloc / free の直接呼び出しは、
 *       ラッパーが元関数へ委譲するための意図的な例外であり、置換対象外とする。 */

/**
 *  @brief          要素数と要素サイズの乗算が size_t を回り込むかを検査します。
 *  @param[in]      count  要素数。
 *  @param[in]      size   要素 1 個あたりのバイト数。
 *  @return         確保してはならない場合は 1、確保してよい場合は 0 を返します。
 *
 *  いずれかが 0 の場合も、長さ 0 の確保を行わないため 1 を返します。
 */
static int alloc_size_rejects(const size_t count, const size_t size)
{
    if ((count == 0U) || (size == 0U))
    {
        return 1;
    }

    if (count > (SIZE_MAX / size))
    {
        return 1;
    }

    return 0;
}

/* Doxygen コメントは、ヘッダーに記載 */

void *cplat_malloc(const size_t size)
{
    if (size == 0U)
    {
        return NULL;
    }

    return malloc(size); /* ラッパーの実体。置換対象外 */
}

/* Doxygen コメントは、ヘッダーに記載 */

void *cplat_malloc_zerofill(const size_t size)
{
    void *ptr;

    if (size == 0U)
    {
        return NULL;
    }

    ptr = calloc(1U, size); /* ラッパーの実体。置換対象外 */

    return ptr;
}

/* Doxygen コメントは、ヘッダーに記載 */

void *cplat_calloc(const size_t count, const size_t size)
{
    if (alloc_size_rejects(count, size) != 0)
    {
        return NULL;
    }

    return calloc(count, size); /* ラッパーの実体。置換対象外 */
}

/* Doxygen コメントは、ヘッダーに記載 */

void *cplat_realloc(void *ptr, const size_t count, const size_t size)
{
    if (alloc_size_rejects(count, size) != 0)
    {
        return NULL;
    }

    return realloc(ptr, count * size); /* ラッパーの実体。置換対象外 */
}

/* Doxygen コメントは、ヘッダーに記載 */

void *cplat_realloc_zerofill(void *ptr, const size_t old_count, const size_t count, const size_t size)
{
    void *new_ptr;

    if (alloc_size_rejects(count, size) != 0)
    {
        return NULL;
    }

    new_ptr = realloc(ptr, count * size); /* ラッパーの実体。置換対象外 */
    if (new_ptr == NULL)
    {
        return NULL;
    }

    if (old_count < count)
    {
        /* 拡張した範囲のみをゼロ初期化する。
         * old_count は count 未満であり、count * size は上で回り込みを検査済みのため、
         * old_count * size と (count - old_count) * size はいずれも回り込まない。 */
        memset((char *)new_ptr + (old_count * size), 0, (count - old_count) * size);
    }

    return new_ptr;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_free(void *ptr)
{
    free(ptr); /* ラッパーの実体。置換対象外。free(NULL) は無害 */
}
