#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_file_set_modified_timestamp(cplat_file *file, const cplat_timespec *timestamp,
                                                    cplat_error *detail_out)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_file_set_modified_timestamp)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_file_set_modified_timestamp"));

    return real_fn(file, timestamp, detail_out);
}

MOCK_WEAK_IMPL(int, cplat_file_set_modified_timestamp, cplat_file *file, const cplat_timespec *timestamp,
               cplat_error *detail_out)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_file_set_modified_timestamp(file, timestamp, detail_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_file_set_modified_timestamp(file, timestamp, detail_out);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p", __func__, (void *)file);
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
