#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_crypto_decrypt(uint8_t *dst, size_t *dst_len, const uint8_t *src, size_t src_len,
                                       const uint8_t *key, const uint8_t *nonce, const uint8_t *aad, size_t aad_len)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_crypto_decrypt)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_crypto_decrypt"));

    return real_fn(dst, dst_len, src, src_len, key, nonce, aad, aad_len);
}

MOCK_WEAK_IMPL(int, cplat_crypto_decrypt, uint8_t *dst, size_t *dst_len, const uint8_t *src, size_t src_len,
               const uint8_t *key, const uint8_t *nonce, const uint8_t *aad, size_t aad_len)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_crypto_decrypt(dst, dst_len, src, src_len, key, nonce, aad, aad_len);
    }
    else
    {
        mock_ret = delegate_real_cplat_crypto_decrypt(dst, dst_len, src, src_len, key, nonce, aad, aad_len);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s", __func__);
        if (getTraceLevel() >= TRACE_DETAIL)
        {
            printf(" -> %d\n", mock_ret);
        }
        else
        {
            printf("\n");
        }
    }

    return mock_ret;
}
