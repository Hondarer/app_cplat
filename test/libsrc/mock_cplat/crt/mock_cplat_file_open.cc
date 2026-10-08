#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_file_open(cplat_file *file, const char *path, int flags, cplat_error *detail_out)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_file_open)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_file_open"));

    return real_fn(file, path, flags, detail_out);
}

MOCK_WEAK_IMPL(int, cplat_file_open, cplat_file *file, const char *path, int flags, cplat_error *detail_out)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_file_open(file, path, flags, detail_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_file_open(file, path, flags, detail_out);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p, %s, %d", __func__, (void *)file, path, flags);
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
