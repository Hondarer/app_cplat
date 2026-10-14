#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_abort_register(cplat_abort_fn callback, void *context)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_abort_register)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_abort_register"));

    return real_fn(callback, context);
}

MOCK_WEAK_IMPL(int, cplat_abort_register, cplat_abort_fn callback, void *context)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_abort_register(callback, context);
    }
    else
    {
        mock_ret = delegate_real_cplat_abort_register(callback, context);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s", __func__);
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
