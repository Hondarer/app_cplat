/**
 *******************************************************************************
 *  @file           crypto_linux.c
 *  @brief          OpenSSL の AES-256-GCM で暗号化および復号する Linux 向け機能を実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/03/12
 *  @version        1.0.0
 *
 *  OpenSSL の EVP インターフェースを使用して AES-256-GCM 暗号化・復号を実装します。\n
 *  Windows 実装 (BCrypt) と同一の wire フォーマット ([暗号文][GCM タグ 16B]) を使用するため、
 *  クロスプラットフォーム通信に対応します。\n
 *  本ファイルは Linux ビルドでのみコンパイルされます (_WIN32 未定義時)。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/base/platform.h>

#if defined(PLATFORM_LINUX)

    #include <string.h>

    #include <openssl/evp.h>

    #include <cplat/base/result.h>
    #include <cplat/crypto/crypto.h>

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_crypto_encrypt(uint8_t *dst, size_t *dst_len, const uint8_t *src, const size_t src_len, const uint8_t *key,
                         const uint8_t *nonce, const uint8_t *aad, const size_t aad_len)
{
    EVP_CIPHER_CTX *ctx;
    int outl;
    int final_len;

    if (dst == NULL || dst_len == NULL || (src == NULL && src_len > 0) || key == NULL || nonce == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (*dst_len < src_len + CPLAT_CRYPTO_TAG_SIZE)
    {
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL)
    {
        return CPLAT_ERR_OUT_OF_MEMORY;
    }

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return CPLAT_ERR_UNKNOWN;
    }

    /* ノンス長を 12 バイトに設定 (既定値と同一だが明示する) */
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, (int)CPLAT_CRYPTO_NONCE_SIZE, NULL) != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return CPLAT_ERR_UNKNOWN;
    }

    if (EVP_EncryptInit_ex(ctx, NULL, NULL, key, nonce) != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return CPLAT_ERR_UNKNOWN;
    }

    /* AAD を入力 (ヘッダー改ざん検知用) */
    if (aad != NULL && aad_len > 0)
    {
        if (EVP_EncryptUpdate(ctx, NULL, &outl, aad, (int)aad_len) != 1)
        {
            EVP_CIPHER_CTX_free(ctx);
            return CPLAT_ERR_UNKNOWN;
        }
    }

    /* 平文を暗号化 (in-place 対応: dst == src 可)。平文 0B の場合はスキップ。 */
    outl = 0;
    if (src_len > 0)
    {
        if (EVP_EncryptUpdate(ctx, dst, &outl, src, (int)src_len) != 1)
        {
            EVP_CIPHER_CTX_free(ctx);
            return CPLAT_ERR_UNKNOWN;
        }
    }

    /* GCM では EncryptFinal は追加データを出力しない (final_len == 0) */
    if (EVP_EncryptFinal_ex(ctx, dst + outl, &final_len) != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return CPLAT_ERR_UNKNOWN;
    }

    /* GCM 認証タグ (16 バイト) を暗号文の直後に書き込む */
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, (int)CPLAT_CRYPTO_TAG_SIZE, dst + src_len) != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return CPLAT_ERR_UNKNOWN;
    }

    *dst_len = src_len + CPLAT_CRYPTO_TAG_SIZE;

    EVP_CIPHER_CTX_free(ctx);
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_crypto_decrypt(uint8_t *dst, size_t *dst_len, const uint8_t *src, const size_t src_len, const uint8_t *key,
                         const uint8_t *nonce, const uint8_t *aad, const size_t aad_len)
{
    EVP_CIPHER_CTX *ctx;
    size_t plain_len;
    int outl;
    int final_len;

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

    ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL)
    {
        return CPLAT_ERR_OUT_OF_MEMORY;
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return CPLAT_ERR_UNKNOWN;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, (int)CPLAT_CRYPTO_NONCE_SIZE, NULL) != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return CPLAT_ERR_UNKNOWN;
    }

    if (EVP_DecryptInit_ex(ctx, NULL, NULL, key, nonce) != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return CPLAT_ERR_UNKNOWN;
    }

    if (aad != NULL && aad_len > 0)
    {
        if (EVP_DecryptUpdate(ctx, NULL, &outl, aad, (int)aad_len) != 1)
        {
            EVP_CIPHER_CTX_free(ctx);
            return CPLAT_ERR_UNKNOWN;
        }
    }

    /* 平文 0B の場合はスキップ */
    outl = 0;
    if (plain_len > 0)
    {
        if (EVP_DecryptUpdate(ctx, dst, &outl, src, (int)plain_len) != 1)
        {
            EVP_CIPHER_CTX_free(ctx);
            return CPLAT_ERR_UNKNOWN;
        }
    }

    /* 認証タグを設定 (暗号文の末尾 CPLAT_CRYPTO_TAG_SIZE バイト)。
       EVP_CIPHER_CTX_ctrl は void * を要求するため const を外すために
       ローカル バッファーにコピーしてから渡す (-Wcast-qual 回避)。 */
    {
        uint8_t tag_buf[CPLAT_CRYPTO_TAG_SIZE];
        memcpy(tag_buf, src + plain_len, CPLAT_CRYPTO_TAG_SIZE);
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, (int)CPLAT_CRYPTO_TAG_SIZE, tag_buf) != 1)
        {
            EVP_CIPHER_CTX_free(ctx);
            return CPLAT_ERR_UNKNOWN;
        }
    }

    /* DecryptFinal でタグ検証を行う。失敗時は CPLAT_ERR_UNKNOWN を返す (認証タグ不一致)。 */
    if (EVP_DecryptFinal_ex(ctx, dst + outl, &final_len) != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return CPLAT_ERR_UNKNOWN;
    }

    *dst_len = plain_len;

    EVP_CIPHER_CTX_free(ctx);
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_passphrase_to_key(uint8_t *key, const uint8_t *passphrase, const size_t passphrase_len)
{
    EVP_MD_CTX *ctx;
    unsigned int len = CPLAT_CRYPTO_KEY_SIZE;

    if (key == NULL || (passphrase == NULL && passphrase_len > 0))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    ctx = EVP_MD_CTX_new();
    if (ctx == NULL)
    {
        return CPLAT_ERR_OUT_OF_MEMORY;
    }

    {
        const uint8_t *pass_data;
        if (passphrase != NULL)
        {
            pass_data = passphrase;
        }
        else
        {
            pass_data = (const uint8_t *)"";
        }
        if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1 || EVP_DigestUpdate(ctx, pass_data, passphrase_len) != 1 ||
            EVP_DigestFinal_ex(ctx, key, &len) != 1)
        {
            EVP_MD_CTX_free(ctx);
            return CPLAT_ERR_UNKNOWN;
        }
    }

    EVP_MD_CTX_free(ctx);
    return CPLAT_OK;
}

#elif defined(PLATFORM_WINDOWS) && defined(COMPILER_MSVC)
    #pragma warning(disable : 4206)
#endif
