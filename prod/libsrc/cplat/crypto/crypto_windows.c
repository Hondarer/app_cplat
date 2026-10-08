/**
 *******************************************************************************
 *  @file           crypto_windows.c
 *  @brief          CNG の AES-256-GCM で暗号化および復号する Windows 向け機能を実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/03/12
 *  @version        1.0.0
 *
 *  Windows CNG (Cryptography Next Generation) の BCrypt API を使用して
 *  AES-256-GCM 暗号化・復号を実装します。\n
 *  Linux 実装 (OpenSSL) と同一の wire フォーマット ([暗号文][GCM タグ 16B]) を使用するため、
 *  クロスプラットフォーム通信に対応します。\n
 *  本ファイルは Windows ビルドでのみコンパイルされます (_WIN32 定義時)。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/base/platform.h>

#if defined(PLATFORM_WINDOWS)

    #include <cplat/base/windows_sdk.h>
    #include <bcrypt.h>
    #pragma comment(lib, "bcrypt.lib")
    #include <string.h>

    #include <cplat/base/result.h>
    #include <cplat/crypto/crypto.h>

/* BCrypt を使用した AES-256-GCM 暗号化の共通実装。
   is_encrypt: TRUE = 暗号化、FALSE = 復号 (タグ検証含む)。
   src/src_len: 暗号化時は平文、復号時は暗号文 (タグを除く)。
   tag: 暗号化時は出力バッファー、復号時は検証用入力バッファー。 */
static int bcrypt_aes_gcm(const BOOL is_encrypt, uint8_t *dst, size_t *dst_len, const uint8_t *src,
                          const size_t src_len, const uint8_t *key, const uint8_t *nonce, const uint8_t *aad,
                          const size_t aad_len, uint8_t *tag, const ULONG tag_len)
{
    BCRYPT_ALG_HANDLE h_alg = NULL;
    BCRYPT_KEY_HANDLE h_key = NULL;
    NTSTATUS status;
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth_info;
    ULONG len = 0;

    status = BCryptOpenAlgorithmProvider(&h_alg, BCRYPT_AES_ALGORITHM, NULL, 0);
    if (!BCRYPT_SUCCESS(status))
    {
        return CPLAT_ERR_UNKNOWN;
    }

    status =
        BCryptSetProperty(h_alg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
    if (!BCRYPT_SUCCESS(status))
    {
        BCryptCloseAlgorithmProvider(h_alg, 0);
        return CPLAT_ERR_UNKNOWN;
    }

    status = BCryptGenerateSymmetricKey(h_alg, &h_key, NULL, 0, (PUCHAR)key, (ULONG)CPLAT_CRYPTO_KEY_SIZE, 0);
    if (!BCRYPT_SUCCESS(status))
    {
        BCryptCloseAlgorithmProvider(h_alg, 0);
        return CPLAT_ERR_UNKNOWN;
    }

    BCRYPT_INIT_AUTH_MODE_INFO(auth_info);
    auth_info.pbNonce = (PUCHAR)nonce;
    auth_info.cbNonce = (ULONG)CPLAT_CRYPTO_NONCE_SIZE;
    auth_info.pbAuthData = (PUCHAR)aad;
    if (aad != NULL)
    {
        auth_info.cbAuthData = (ULONG)aad_len;
    }
    else
    {
        auth_info.cbAuthData = 0U;
    }
    auth_info.pbTag = (PUCHAR)tag;
    auth_info.cbTag = tag_len;

    if (is_encrypt)
    {
        status = BCryptEncrypt(h_key, (PUCHAR)src, (ULONG)src_len, &auth_info, NULL, 0, (PUCHAR)dst, (ULONG)*dst_len,
                               &len, 0);
    }
    else
    {
        status = BCryptDecrypt(h_key, (PUCHAR)src, (ULONG)src_len, &auth_info, NULL, 0, (PUCHAR)dst, (ULONG)*dst_len,
                               &len, 0);
    }

    BCryptDestroyKey(h_key);
    BCryptCloseAlgorithmProvider(h_alg, 0);

    if (!BCRYPT_SUCCESS(status))
    {
        return CPLAT_ERR_UNKNOWN;
    }

    *dst_len = (size_t)len;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_crypto_encrypt(uint8_t *dst, size_t *dst_len, const uint8_t *src, const size_t src_len, const uint8_t *key,
                         const uint8_t *nonce, const uint8_t *aad, const size_t aad_len)
{
    uint8_t tag[CPLAT_CRYPTO_TAG_SIZE];
    size_t enc_len;

    if (dst == NULL || dst_len == NULL || (src == NULL && src_len > 0) || key == NULL || nonce == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (*dst_len < src_len + CPLAT_CRYPTO_TAG_SIZE)
    {
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    enc_len = src_len;

    {
        /* BCrypt は src=NULL を受け付けないため、空バッファーを用意する */
        static const uint8_t empty_src[1] = {0};
        const uint8_t *actual_src;
        int bcrypt_result;

        if (src != NULL)
        {
            actual_src = src;
        }
        else
        {
            actual_src = empty_src;
        }

        bcrypt_result = bcrypt_aes_gcm(TRUE, dst, &enc_len, actual_src, src_len, key, nonce, aad, aad_len, tag,
                                       (ULONG)CPLAT_CRYPTO_TAG_SIZE);
        if (bcrypt_result != CPLAT_OK)
        {
            return bcrypt_result;
        }
    }

    /* タグ (16B) を暗号文の直後に付加する */
    memcpy(dst + enc_len, tag, CPLAT_CRYPTO_TAG_SIZE);
    *dst_len = enc_len + CPLAT_CRYPTO_TAG_SIZE;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_crypto_decrypt(uint8_t *dst, size_t *dst_len, const uint8_t *src, const size_t src_len, const uint8_t *key,
                         const uint8_t *nonce, const uint8_t *aad, const size_t aad_len)
{
    size_t plain_len;

    if (dst == NULL || dst_len == NULL || src == NULL || src_len < CPLAT_CRYPTO_TAG_SIZE || key == NULL ||
        nonce == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    plain_len = src_len - CPLAT_CRYPTO_TAG_SIZE;

    if (plain_len > 0 && *dst_len < plain_len)
    {
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    /* タグは暗号文の末尾 CPLAT_CRYPTO_TAG_SIZE バイト。BCrypt がタグ検証を行う。
       STATUS_AUTH_TAG_MISMATCH 時は bcrypt_aes_gcm が CPLAT_ERR_UNKNOWN を返す。 */
    return bcrypt_aes_gcm(FALSE, dst, dst_len, src, plain_len, key, nonce, aad, aad_len, (uint8_t *)(src + plain_len),
                          (ULONG)CPLAT_CRYPTO_TAG_SIZE);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_passphrase_to_key(uint8_t *key, const uint8_t *passphrase, const size_t passphrase_len)
{
    BCRYPT_ALG_HANDLE h_alg = NULL;
    BCRYPT_HASH_HANDLE h_hash = NULL;
    NTSTATUS status;
    static const uint8_t empty[1] = {0};
    const uint8_t *data;
    ULONG data_len;

    /* 引数検証は Linux 実装 (crypto_linux.c) と同じく関数先頭で行う */
    if (key == NULL || (passphrase == NULL && passphrase_len > 0))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (passphrase != NULL)
    {
        data = passphrase;
        data_len = (ULONG)passphrase_len;
    }
    else
    {
        data = empty;
        data_len = 0UL;
    }

    status = BCryptOpenAlgorithmProvider(&h_alg, BCRYPT_SHA256_ALGORITHM, NULL, 0);
    if (!BCRYPT_SUCCESS(status))
    {
        return CPLAT_ERR_UNKNOWN;
    }

    status = BCryptCreateHash(h_alg, &h_hash, NULL, 0, NULL, 0, 0);
    if (!BCRYPT_SUCCESS(status))
    {
        BCryptCloseAlgorithmProvider(h_alg, 0);
        return CPLAT_ERR_UNKNOWN;
    }

    status = BCryptHashData(h_hash, (PUCHAR)data, data_len, 0);
    if (!BCRYPT_SUCCESS(status))
    {
        BCryptDestroyHash(h_hash);
        BCryptCloseAlgorithmProvider(h_alg, 0);
        return CPLAT_ERR_UNKNOWN;
    }

    status = BCryptFinishHash(h_hash, (PUCHAR)key, (ULONG)CPLAT_CRYPTO_KEY_SIZE, 0);

    BCryptDestroyHash(h_hash);
    BCryptCloseAlgorithmProvider(h_alg, 0);

    if (BCRYPT_SUCCESS(status))
    {
        return CPLAT_OK;
    }
    else
    {
        return CPLAT_ERR_UNKNOWN;
    }
}

#endif /* PLATFORM_WINDOWS */
