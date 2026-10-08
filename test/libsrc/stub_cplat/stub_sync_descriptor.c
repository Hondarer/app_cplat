/**
 *  @file           stub_sync_descriptor.c
 *  @brief          プロセス間同期ディスクリプターの内部 API を、呼び出し元の単体テスト向けに提供します。
 *
 *  sync_descriptor.c のカバレッジは syncDescriptorTest が担う。
 *  本スタブは identity の往復と種別不一致の拒否だけを行い、ワイヤ形式の詳細は対象にしない。
 */

#include <stdlib.h>
#include <string.h>

#include <cplat/sync/sync_descriptor.h>

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
    if (identity_len > 255U)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    required = 3U + identity_len;
    if (descriptor == NULL || *descriptor_size < required)
    {
        *descriptor_size = required;
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    out = (uint8_t *)descriptor;
    out[0] = kind;
    out[1] = backend;
    out[2] = (uint8_t)identity_len;
    memcpy(out + 3U, identity, identity_len);
    *descriptor_size = required;
    return CPLAT_OK;
}

int cplat_internal_interprocess_sync_descriptor_import(const void *descriptor, const size_t descriptor_size,
                                                       const uint8_t kind, const uint8_t backend, char **identity_out)
{
    const uint8_t *in = (const uint8_t *)descriptor;
    size_t identity_len;
    char *identity;

    if (descriptor == NULL || identity_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (descriptor_size < 3U || in[0] != kind || in[1] != backend)
    {
        return CPLAT_ERR_CORRUPT_DESCRIPTOR;
    }
    identity_len = (size_t)in[2];
    if (identity_len == 0U || descriptor_size != 3U + identity_len)
    {
        return CPLAT_ERR_CORRUPT_DESCRIPTOR;
    }
    identity = (char *)malloc(identity_len + 1U);
    if (identity == NULL)
    {
        return CPLAT_ERR_UNKNOWN;
    }
    memcpy(identity, in + 3U, identity_len);
    identity[identity_len] = '\0';
    *identity_out = identity;
    return CPLAT_OK;
}
