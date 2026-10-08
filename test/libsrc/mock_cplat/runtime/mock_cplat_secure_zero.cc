#include <testfw.h>
#include <mock_cplat.h>

void delegate_real_cplat_secure_zero(void *buf, size_t size)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_secure_zero)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_secure_zero"));

    real_fn(buf, size);
}

MOCK_WEAK_IMPL(void, cplat_secure_zero, void *buf, size_t size)
{
    if (_mock_cplat != nullptr)
    {
        _mock_cplat->cplat_secure_zero(buf, size);
    }
    else
    {
        delegate_real_cplat_secure_zero(buf, size);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p size=%zu\n", __func__, buf, size);
    }
}
