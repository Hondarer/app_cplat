#include <testfw.h>
#include <mock_cplat.h>

size_t delegate_real_cplat_sym_loader_count(cplat_sym_loader_entry *fobj)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_sym_loader_count)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_sym_loader_count"));

    return real_fn(fobj);
}

MOCK_WEAK_IMPL(size_t, cplat_sym_loader_count, cplat_sym_loader_entry *fobj)
{
    size_t mock_ret = 0u;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_sym_loader_count(fobj);
    }
    else
    {
        mock_ret = delegate_real_cplat_sym_loader_count(fobj);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p", __func__, (void *)fobj);
        if (getTraceLevel() >= TRACE_DETAIL)
        {
            printf(" -> %zu\n", mock_ret);
        }
        else
        {
            printf("\n");
        }
    }

    return mock_ret;
}
