#include <testfw.h>
#include <mock_cplat.h>

void delegate_real_cplat_sym_loader_init(cplat_sym_loader_entry *const *fobj_array, size_t fobj_length,
                                         const char *configpath)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_sym_loader_init)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_sym_loader_init"));

    real_fn(fobj_array, fobj_length, configpath);
}

MOCK_WEAK_IMPL(void, cplat_sym_loader_init, cplat_sym_loader_entry *const *fobj_array, size_t fobj_length,
               const char *configpath)
{
    if (_mock_cplat != nullptr)
    {
        _mock_cplat->cplat_sym_loader_init(fobj_array, fobj_length, configpath);
    }
    else
    {
        delegate_real_cplat_sym_loader_init(fobj_array, fobj_length, configpath);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s \"%s\"\n", __func__, configpath != nullptr ? configpath : "(null)");
    }
}
