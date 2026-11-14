#include <inttypes.h>
#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_fseek(FILE *stream, int64_t offset, int whence)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_fseek)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_fseek"));

    return real_fn(stream, offset, whence);
}

MOCK_WEAK_IMPL(int, cplat_fseek, FILE *stream, int64_t offset, int whence)
{
    int mock_ret = -1;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_fseek(stream, offset, whence);
    }
    else
    {
        mock_ret = delegate_real_cplat_fseek(stream, offset, whence);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p, %" PRId64 ", %d", __func__, (void *)stream, offset, whence);
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
