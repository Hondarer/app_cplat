#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_pinned_prompt_status_enable(cplat_pinned_prompt *screen,
                                                    cplat_pinned_prompt_status_position position, int enable)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_pinned_prompt_status_enable)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_pinned_prompt_status_enable"));

    return real_fn(screen, position, enable);
}

MOCK_WEAK_IMPL(int, cplat_pinned_prompt_status_enable, cplat_pinned_prompt *screen,
               cplat_pinned_prompt_status_position position, int enable)
{
    int mock_ret = -1;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_pinned_prompt_status_enable(screen, position, enable);
    }
    else
    {
        mock_ret = delegate_real_cplat_pinned_prompt_status_enable(screen, position, enable);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p, %d, %d", __func__, (void *)screen, (int)position, enable);
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
