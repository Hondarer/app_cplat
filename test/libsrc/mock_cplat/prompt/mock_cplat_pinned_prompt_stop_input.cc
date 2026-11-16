#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_pinned_prompt_stop_input(cplat_pinned_prompt *screen)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_pinned_prompt_stop_input)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_pinned_prompt_stop_input"));

    return real_fn(screen);
}

MOCK_WEAK_IMPL(int, cplat_pinned_prompt_stop_input, cplat_pinned_prompt *screen)
{
    int mock_ret = -1;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_pinned_prompt_stop_input(screen);
    }
    else
    {
        mock_ret = delegate_real_cplat_pinned_prompt_stop_input(screen);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p", __func__, (void *)screen);

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
