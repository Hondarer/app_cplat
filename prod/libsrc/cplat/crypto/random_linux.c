/**
 *******************************************************************************
 *  @file           random_linux.c
 *  @brief          OpenSSL を用いて暗号論的乱数を取得する Linux 向け機能を実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/07/30
 *  @version        1.0.0
 *
 *  OpenSSL の `RAND_bytes` を使用して、暗号論的乱数源からバイト列を取得します。\n
 *  本ファイルは Linux ビルドでのみコンパイルされます。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/base/platform.h>

#if defined(PLATFORM_LINUX)

    #include <openssl/rand.h>

    #include <cplat/base/result.h>
    #include <cplat/crypto/random.h>

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_random_bytes(void *buf, const size_t size)
{
    if (size == 0U)
    {
        return CPLAT_OK;
    }

    if (buf == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    /* クロスプラットフォームの最大値を超える要求は、OS API へ渡す前に引数不正として扱う。 */
    if (size > CPLAT_CRYPTO_RANDOM_MAX_BYTES)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    /*
     * RAND_bytes() は成功時に 1 を返す。0 と -1 は、いずれも乱数を得られなかったことを表す。
     * see: https://docs.openssl.org/master/man3/RAND_bytes/
     */
    if (RAND_bytes((unsigned char *)buf, (int)size) != 1)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    return CPLAT_OK;
}

#elif defined(PLATFORM_WINDOWS) && defined(COMPILER_MSVC)
    #pragma warning(disable : 4206)
#endif
