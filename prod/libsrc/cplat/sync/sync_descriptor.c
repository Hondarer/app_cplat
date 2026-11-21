/**
 *******************************************************************************
 *  @file           sync_descriptor.c
 *  @brief          プロセス間同期ディスクリプターの直列化機能 (内部共有) を実装します。
 *
 *  ワイヤ フォーマットの詳細は sync_descriptor.h を参照してください。
 *  プラットフォーム非依存であり、Linux と Windows の双方から使用します。
 *******************************************************************************
 */

#include <assert.h>
#include <stdlib.h>
#include <cplat/crt/stdlib.h>
#include <string.h>

#include <cplat/sync/sync_descriptor.h>

/** ディスクリプターの先頭に置くマジック文字列。 */
static const char DESCRIPTOR_MAGIC[4] = {'C', 'U', 'L', 'K'};

/** ヘッダー内の先頭マジック文字列のオフセット [byte] です。 */
#define SYNC_DESC_OFFSET_MAGIC 0U

/** ヘッダー内のフォーマット バージョンのオフセット [byte] です。 */
#define SYNC_DESC_OFFSET_VERSION (SYNC_DESC_OFFSET_MAGIC + (unsigned int)sizeof(DESCRIPTOR_MAGIC))

/** ヘッダー内の同期オブジェクト種別のオフセット [byte] です。 */
#define SYNC_DESC_OFFSET_KIND (SYNC_DESC_OFFSET_VERSION + 1U)

/** ヘッダー内のバックエンド種別のオフセット [byte] です。 */
#define SYNC_DESC_OFFSET_BACKEND (SYNC_DESC_OFFSET_KIND + 1U)

/** ヘッダー内の 1 バイト予約領域のオフセット [byte] です。 */
#define SYNC_DESC_OFFSET_RESERVED1 (SYNC_DESC_OFFSET_BACKEND + 1U)

/** ヘッダー内の identity 長 (uint32 リトル エンディアン) のオフセット [byte] です。 */
#define SYNC_DESC_OFFSET_IDENTITY_LEN (SYNC_DESC_OFFSET_RESERVED1 + 1U)

/** ヘッダー内の identity 長フィールドのバイト数 [byte] です。 */
#define SYNC_DESC_IDENTITY_LEN_SIZE ((unsigned int)sizeof(uint32_t))

/** ヘッダー内の末尾予約領域のオフセット [byte] です。 */
#define SYNC_DESC_OFFSET_RESERVED2 (SYNC_DESC_OFFSET_IDENTITY_LEN + SYNC_DESC_IDENTITY_LEN_SIZE)

/** ヘッダー内の末尾予約領域のバイト数 [byte] です。 */
#define SYNC_DESC_RESERVED2_SIZE 8U

static_assert((SYNC_DESC_OFFSET_RESERVED2 + SYNC_DESC_RESERVED2_SIZE) == CPLAT_INTERPROCESS_SYNC_DESCRIPTOR_HEADER_SIZE,
              "cplat: sync descriptor header layout must match header size");

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_interprocess_sync_descriptor_export(const char *identity, const uint8_t kind, const uint8_t backend,
                                                       void *descriptor, size_t *descriptor_size)
{
    uint8_t *out;
    size_t identity_len;
    size_t required;

    if (identity == NULL || descriptor_size == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    identity_len = strlen(identity);
    required = CPLAT_INTERPROCESS_SYNC_DESCRIPTOR_HEADER_SIZE + identity_len;
    if (descriptor == NULL || *descriptor_size < required)
    {
        *descriptor_size = required;
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    out = (uint8_t *)descriptor;
    memcpy(out + SYNC_DESC_OFFSET_MAGIC, DESCRIPTOR_MAGIC, sizeof(DESCRIPTOR_MAGIC));
    out[SYNC_DESC_OFFSET_VERSION] = CPLAT_INTERPROCESS_SYNC_DESCRIPTOR_VERSION;
    out[SYNC_DESC_OFFSET_KIND] = kind;
    out[SYNC_DESC_OFFSET_BACKEND] = backend;
    out[SYNC_DESC_OFFSET_RESERVED1] = 0U;

    /* identity 長をリトル エンディアンで格納する。 */
    out[SYNC_DESC_OFFSET_IDENTITY_LEN + 0U] = (uint8_t)(identity_len & 0xffU);
    out[SYNC_DESC_OFFSET_IDENTITY_LEN + 1U] = (uint8_t)((identity_len >> 8U) & 0xffU);
    out[SYNC_DESC_OFFSET_IDENTITY_LEN + 2U] = (uint8_t)((identity_len >> 16U) & 0xffU);
    out[SYNC_DESC_OFFSET_IDENTITY_LEN + 3U] = (uint8_t)((identity_len >> 24U) & 0xffU);
    memset(out + SYNC_DESC_OFFSET_RESERVED2, 0, SYNC_DESC_RESERVED2_SIZE); /* offset 12-19 の予約領域を 0 で埋める */
    memcpy(out + CPLAT_INTERPROCESS_SYNC_DESCRIPTOR_HEADER_SIZE, identity, identity_len);
    *descriptor_size = required;

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_interprocess_sync_descriptor_import(const void *descriptor, const size_t descriptor_size,
                                                       const uint8_t kind, const uint8_t backend, char **identity_out)
{
    const uint8_t *in = (const uint8_t *)descriptor;
    uint32_t identity_len;
    char *identity;

    if (descriptor == NULL || identity_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    /* ヘッダー長、マジック文字列、種別、バックエンドの一致を確認する。 */
    if (descriptor_size < CPLAT_INTERPROCESS_SYNC_DESCRIPTOR_HEADER_SIZE ||
        memcmp(in + SYNC_DESC_OFFSET_MAGIC, DESCRIPTOR_MAGIC, sizeof(DESCRIPTOR_MAGIC)) != 0 ||
        in[SYNC_DESC_OFFSET_KIND] != kind || in[SYNC_DESC_OFFSET_BACKEND] != backend)
    {
        return CPLAT_ERR_CORRUPT_DESCRIPTOR;
    }

    /* フォーマット バージョンの一致を確認する。 */
    if (in[SYNC_DESC_OFFSET_VERSION] != CPLAT_INTERPROCESS_SYNC_DESCRIPTOR_VERSION)
    {
        return CPLAT_ERR_VERSION_MISMATCH;
    }

    /* リトル エンディアンのバイト列から identity 長を復元し、ディスクリプター長との整合性を確認する。 */
    identity_len = (uint32_t)in[SYNC_DESC_OFFSET_IDENTITY_LEN + 0U] |
                   ((uint32_t)in[SYNC_DESC_OFFSET_IDENTITY_LEN + 1U] << 8U) |
                   ((uint32_t)in[SYNC_DESC_OFFSET_IDENTITY_LEN + 2U] << 16U) |
                   ((uint32_t)in[SYNC_DESC_OFFSET_IDENTITY_LEN + 3U] << 24U);
    if (identity_len == 0 || descriptor_size != (CPLAT_INTERPROCESS_SYNC_DESCRIPTOR_HEADER_SIZE + (size_t)identity_len))
    {
        return CPLAT_ERR_CORRUPT_DESCRIPTOR;
    }

    /* NUL 終端を含めた領域を確保し、identity 文字列を取り出す。 */
    identity = (char *)cplat_malloc((size_t)identity_len + 1U);
    if (identity == NULL)
    {
        return CPLAT_ERR_OUT_OF_MEMORY;
    }

    memcpy(identity, in + CPLAT_INTERPROCESS_SYNC_DESCRIPTOR_HEADER_SIZE, identity_len);
    identity[identity_len] = '\0';
    *identity_out = identity;

    return CPLAT_OK;
}
