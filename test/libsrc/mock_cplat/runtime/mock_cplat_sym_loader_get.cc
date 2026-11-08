#include <testfw.h>
#include <mock_cplat.h>

void *delegate_real_cplat_sym_loader_get(cplat_sym_loader_entry *fobj, size_t index)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_sym_loader_get)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_sym_loader_get"));

    return real_fn(fobj, index);
}

MOCK_WEAK_IMPL(void *, cplat_sym_loader_get, cplat_sym_loader_entry *fobj, size_t index)
{
    void *mock_ret = nullptr;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_sym_loader_get(fobj, index);
    }
    else
    {
        mock_ret = delegate_real_cplat_sym_loader_get(fobj, index);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p %zu", __func__, (void *)fobj, index);
        if (getTraceLevel() >= TRACE_DETAIL)
        {
            printf(" -> 0x%p\n", mock_ret);
        }
        else
        {
            printf("\n");
        }
    }

    return mock_ret;
}
