#include <mock_cplat.h>
#include <testfw.h>

int delegate_real_cplat_sym_loader_resolve(cplat_sym_loader_entry *fobj, void **function_out)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_sym_loader_resolve)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_sym_loader_resolve"));
    return real_fn(fobj, function_out);
}

MOCK_WEAK_IMPL(int, cplat_sym_loader_resolve, cplat_sym_loader_entry *fobj, void **function_out)
{
    int mock_ret;
    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_sym_loader_resolve(fobj, function_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_sym_loader_resolve(fobj, function_out);
    }
    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s -> %d\n", __func__, mock_ret);
    }
    return mock_ret;
}
