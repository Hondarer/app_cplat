#include <testfw.h>
#include <mock_cplat.h>

void delegate_real_cplat_file_init(cplat_file *file)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_file_init)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_file_init"));

    real_fn(file);
}

MOCK_WEAK_IMPL(void, cplat_file_init, cplat_file *file)
{
    if (_mock_cplat != nullptr)
    {
        _mock_cplat->cplat_file_init(file);
    }
    else
    {
        delegate_real_cplat_file_init(file);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p\n", __func__, (void *)file);
    }
}
