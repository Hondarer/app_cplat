#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_sym_loader_is_failed(cplat_sym_loader_entry *fobj)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_sym_loader_is_failed)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_sym_loader_is_failed"));

    return real_fn(fobj);
}

MOCK_WEAK_IMPL(int, cplat_sym_loader_is_failed, cplat_sym_loader_entry *fobj)
{
    int mock_ret = 0;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_sym_loader_is_failed(fobj);
    }
    else
    {
        mock_ret = delegate_real_cplat_sym_loader_is_failed(fobj);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p", __func__, (void *)fobj);
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
