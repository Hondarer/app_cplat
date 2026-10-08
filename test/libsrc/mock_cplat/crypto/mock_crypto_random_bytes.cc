#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_random_bytes(void *buf, size_t size)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_random_bytes)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_random_bytes"));

    return real_fn(buf, size);
}

MOCK_WEAK_IMPL(int, cplat_random_bytes, void *buf, size_t size)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_random_bytes(buf, size);
    }
    else
    {
        mock_ret = delegate_real_cplat_random_bytes(buf, size);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s size=%zu", __func__, size);
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
