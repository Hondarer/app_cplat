#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_pinned_prompt_status_set(cplat_pinned_prompt *screen,
                                                 cplat_pinned_prompt_status_position position,
                                                 cplat_pinned_prompt_status_align align, const char *content)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_pinned_prompt_status_set)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_pinned_prompt_status_set"));

    return real_fn(screen, position, align, content);
}

MOCK_WEAK_IMPL(int, cplat_pinned_prompt_status_set, cplat_pinned_prompt *screen,
               cplat_pinned_prompt_status_position position, cplat_pinned_prompt_status_align align,
               const char *content)
{
    int mock_ret = -1;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_pinned_prompt_status_set(screen, position, align, content);
    }
    else
    {
        mock_ret = delegate_real_cplat_pinned_prompt_status_set(screen, position, align, content);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p, %d, %d, \"%s\"", __func__, (void *)screen, (int)position, (int)align,
               content != nullptr ? content : "");
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
