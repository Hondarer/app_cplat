#include <testfw.h>
#include <mock_cplat.h>

void delegate_real_cplat_call_once(cplat_once_flag *flag, cplat_once_fn func)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_call_once)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_call_once"));

    real_fn(flag, func);
}

MOCK_WEAK_IMPL(void, cplat_call_once, cplat_once_flag *flag, cplat_once_fn func)
{
    if (_mock_cplat != nullptr)
    {
        _mock_cplat->cplat_call_once(flag, func);
    }
    else
    {
        delegate_real_cplat_call_once(flag, func);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p", __func__, (void *)func);
        if (getTraceLevel() >= TRACE_DETAIL)
        {
            const int32_t state =
                (flag != nullptr) ? cplat_atomic_load_i32(&flag->state, CPLAT_MEMORY_ORDER_RELAXED) : -1;
            printf(" -> %d\n", state);
        }
        else
        {
            printf("\n");
        }
    }
}
