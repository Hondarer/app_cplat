#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_sym_loader_init(cplat_sym_loader_entry *const *fobj_array, size_t fobj_length,
                                         const char *configpath)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_sym_loader_init)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_sym_loader_init"));

    return real_fn(fobj_array, fobj_length, configpath);
}

MOCK_WEAK_IMPL(int, cplat_sym_loader_init, cplat_sym_loader_entry *const *fobj_array, size_t fobj_length,
               const char *configpath)
{
    int mock_ret;
    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_sym_loader_init(fobj_array, fobj_length, configpath);
    }
    else
    {
        mock_ret = delegate_real_cplat_sym_loader_init(fobj_array, fobj_length, configpath);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s \"%s\"\n", __func__, configpath != nullptr ? configpath : "(null)");
    }
    return mock_ret;
}
