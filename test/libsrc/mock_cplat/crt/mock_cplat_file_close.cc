#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_file_close(cplat_file *file, cplat_error *detail_out)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_file_close)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_file_close"));

    return real_fn(file, detail_out);
}

MOCK_WEAK_IMPL(int, cplat_file_close, cplat_file *file, cplat_error *detail_out)
{
    int mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_file_close(file, detail_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_file_close(file, detail_out);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p\n", __func__, (void *)file);
    }

    return mock_ret;
}
