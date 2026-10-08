#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_file_read(cplat_file *file, void *buf, size_t len, size_t *read_out, cplat_error *detail_out)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_file_read)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_file_read"));

    return real_fn(file, buf, len, read_out, detail_out);
}

MOCK_WEAK_IMPL(int, cplat_file_read, cplat_file *file, void *buf, size_t len, size_t *read_out, cplat_error *detail_out)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_file_read(file, buf, len, read_out, detail_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_file_read(file, buf, len, read_out, detail_out);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s len=%zu", __func__, len);
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
