#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_file_flush(cplat_file *file, cplat_error *detail_out)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_file_flush)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_file_flush"));

    return real_fn(file, detail_out);
}

MOCK_WEAK_IMPL(int, cplat_file_flush, cplat_file *file, cplat_error *detail_out)
{
    if (_mock_cplat != nullptr)
    {
        return _mock_cplat->cplat_file_flush(file, detail_out);
    }

    return delegate_real_cplat_file_flush(file, detail_out);
}
