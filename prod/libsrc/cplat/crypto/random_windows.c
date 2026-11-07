/**
 *******************************************************************************
 *  @file           random_windows.c
 *  @brief          CNG を用いて暗号論的乱数を取得する Windows 向け機能を実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/07/30
 *  @version        1.0.0
 *
 *  CNG の `BCryptGenRandom` を `BCRYPT_USE_SYSTEM_PREFERRED_RNG` 指定で使用し、
 *  アルゴリズム プロバイダーを開かずに暗号論的乱数源からバイト列を取得します。\n
 *  本ファイルは Windows ビルドでのみコンパイルされます。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/base/platform.h>

#if defined(PLATFORM_WINDOWS)

    #include <cplat/base/windows_sdk.h>

    #include <bcrypt.h>
    #pragma comment(lib, "Bcrypt.lib")

    #include <cplat/base/result.h>
    #include <cplat/crypto/random.h>

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_random_bytes(void *buf, const size_t size)
{
    NTSTATUS status;

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
     * BCRYPT_USE_SYSTEM_PREFERRED_RNG を指定すると、アルゴリズム プロバイダーの
     * ハンドルを開かずに、システム既定の乱数源を使用できる。
     * see: https://learn.microsoft.com/windows/win32/api/bcrypt/nf-bcrypt-bcryptgenrandom
     */
    status = BCryptGenRandom(NULL, (PUCHAR)buf, (ULONG)size, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (!BCRYPT_SUCCESS(status))
    {
        return CPLAT_ERR_UNKNOWN;
    }

    return CPLAT_OK;
}

#endif /* PLATFORM_WINDOWS */
