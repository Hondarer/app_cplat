/**
 *******************************************************************************
 *  @file           crypto_linux.c
 *  @brief          OpenSSL の AES-256-GCM で暗号化および復号する Linux 向け機能を実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/03/12
 *  @version        1.0.0
 *
 *  OpenSSL の EVP インターフェイスを使用して AES-256-GCM 暗号化・復号を実装します。\n
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
    #include <limits.h>

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

    if (src_len > (size_t)INT_MAX || (aad != NULL && aad_len > (size_t)INT_MAX) ||
        src_len > SIZE_MAX - CPLAT_CRYPTO_TAG_SIZE)
    {
        return CPLAT_ERR_OUT_OF_RANGE;
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

    /*
     * ノンス長の設定は、ノンスを渡すより前にしかできない。
     * 先に暗号アルゴリズムだけを指定し、既定と同じノンス長を明示してから鍵とノンスを渡す。
     * see: https://docs.openssl.org/3.5/man3/EVP_EncryptInit/
     */
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return CPLAT_ERR_UNKNOWN;
    }

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

    /*
     * 追加認証データは、出力先を NULL にした EVP_EncryptUpdate() で渡す。
     * ヘッダーの改ざん検知に使う。
     * see: https://docs.openssl.org/3.5/man3/EVP_EncryptInit/
     */
    if (aad != NULL && aad_len > 0)
    {
        if (EVP_EncryptUpdate(ctx, NULL, &outl, aad, (int)aad_len) != 1)
        {
            EVP_CIPHER_CTX_free(ctx);
            return CPLAT_ERR_UNKNOWN;
        }
    }

    /*
     * 平文を暗号化する。dst と src が同一でも暗号化できる。
     * 平文が 0 [byte] のときは EVP_EncryptUpdate() を呼ばない。
     * そのとき終端処理の書き込み位置を決める outl を 0 にする。
     */
    outl = 0;

    if (src_len > 0)
    {
        if (EVP_EncryptUpdate(ctx, dst, &outl, src, (int)src_len) != 1)
        {
            EVP_CIPHER_CTX_free(ctx);
            return CPLAT_ERR_UNKNOWN;
        }
    }

    /*
     * GCM では EVP_EncryptFinal_ex() は追加の暗号文を出力しない。final_len は 0 になる。
     * see: https://wiki.openssl.org/index.php/EVP_Authenticated_Encryption_and_Decryption
     */
    if (EVP_EncryptFinal_ex(ctx, dst + outl, &final_len) != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return CPLAT_ERR_UNKNOWN;
    }

    /*
     * 認証タグは終端処理の後でなければ取り出せない。暗号文の直後へ書き込む。
     * see: https://docs.openssl.org/3.5/man3/EVP_EncryptInit/
     */
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
    if (plain_len > (size_t)INT_MAX || (aad != NULL && aad_len > (size_t)INT_MAX))
    {
        return CPLAT_ERR_OUT_OF_RANGE;
    }

    if (plain_len > 0 && *dst_len < plain_len)
    {
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL)
    {
        return CPLAT_ERR_OUT_OF_MEMORY;
    }

    /*
     * ノンス長の設定は、ノンスを渡すより前にしかできない。
     * 先に暗号アルゴリズムだけを指定し、既定と同じノンス長を明示してから鍵とノンスを渡す。
     * see: https://docs.openssl.org/3.5/man3/EVP_EncryptInit/
     */
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

    /*
     * 追加認証データは、出力先を NULL にした EVP_DecryptUpdate() で渡す。
     * ヘッダーの改ざん検知に使う。
     * see: https://docs.openssl.org/3.5/man3/EVP_EncryptInit/
     */
    if (aad != NULL && aad_len > 0)
    {
        if (EVP_DecryptUpdate(ctx, NULL, &outl, aad, (int)aad_len) != 1)
        {
            EVP_CIPHER_CTX_free(ctx);
            return CPLAT_ERR_UNKNOWN;
        }
    }

    /*
     * 平文が 0 [byte] のときは EVP_DecryptUpdate() を呼ばない。
     * そのとき終端処理の書き込み位置を決める outl を 0 にする。
     */
    outl = 0;

    if (plain_len > 0)
    {
        if (EVP_DecryptUpdate(ctx, dst, &outl, src, (int)plain_len) != 1)
        {
            EVP_CIPHER_CTX_free(ctx);
            return CPLAT_ERR_UNKNOWN;
        }
    }

    /*
     * 認証タグは暗号文の末尾 CPLAT_CRYPTO_TAG_SIZE [byte] にある。
     * EVP_CIPHER_CTX_ctrl() は void * を要求するため、const を外すために
     * ローカル バッファーへコピーしてから渡す (-Wcast-qual 回避)。
     * タグの設定は EVP_DecryptFinal_ex() より前に行う。
     * see: https://docs.openssl.org/3.5/man3/EVP_EncryptInit/
     */
    {
        uint8_t tag_buf[CPLAT_CRYPTO_TAG_SIZE];
        memcpy(tag_buf, src + plain_len, CPLAT_CRYPTO_TAG_SIZE);

        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, (int)CPLAT_CRYPTO_TAG_SIZE, tag_buf) != 1)
        {
            EVP_CIPHER_CTX_free(ctx);
            return CPLAT_ERR_UNKNOWN;
        }
    }

    /* EVP_DecryptFinal_ex() で認証タグを検証する。失敗時は CPLAT_ERR_AUTHENTICATION_FAILED を返す。 */
    if (EVP_DecryptFinal_ex(ctx, dst + outl, &final_len) != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return CPLAT_ERR_AUTHENTICATION_FAILED;
    }

    *dst_len = plain_len;

    EVP_CIPHER_CTX_free(ctx);

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_passphrase_to_key(uint8_t *key_out, const uint8_t *passphrase, const size_t passphrase_len)
{
    EVP_MD_CTX *ctx;

    /*
     * EVP_DigestFinal_ex() は、書き込んだダイジェストのバイト数を len に格納する。
     * 呼び出し前の len は、出力バッファーの容量としては使われない。
     * see: https://docs.openssl.org/master/man3/EVP_DigestInit/
     */
    unsigned int len = CPLAT_CRYPTO_KEY_SIZE;

    if (key_out == NULL || (passphrase == NULL && passphrase_len > 0))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (passphrase_len > (size_t)UINT32_MAX)
    {
        return CPLAT_ERR_OUT_OF_RANGE;
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
            EVP_DigestFinal_ex(ctx, key_out, &len) != 1)
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
