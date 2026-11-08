#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_sym_loader_get(cplat_sym_loader_entry *fobj, size_t index, void **address_out)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_sym_loader_get)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_sym_loader_get"));

    return real_fn(fobj, index, address_out);
}

MOCK_WEAK_IMPL(int, cplat_sym_loader_get, cplat_sym_loader_entry *fobj, size_t index, void **address_out)
{
    int mock_ret = 0;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_sym_loader_get(fobj, index, address_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_sym_loader_get(fobj, index, address_out);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p %zu", __func__, (void *)fobj, index);
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
