#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_sym_loader_reject_get(cplat_sym_loader_entry *fobj, size_t index, const char **lib_name,
                                              const char **func_name, int *reason)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_sym_loader_reject_get)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_sym_loader_reject_get"));

    return real_fn(fobj, index, lib_name, func_name, reason);
}

MOCK_WEAK_IMPL(int, cplat_sym_loader_reject_get, cplat_sym_loader_entry *fobj, size_t index, const char **lib_name,
               const char **func_name, int *reason)
{
    int mock_ret = -1;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_sym_loader_reject_get(fobj, index, lib_name, func_name, reason);
    }
    else
    {
        mock_ret = delegate_real_cplat_sym_loader_reject_get(fobj, index, lib_name, func_name, reason);
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
