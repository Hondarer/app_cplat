#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_pinned_prompt_completion_set(cplat_pinned_prompt *screen, cplat_prompt_completion_fn fn,
                                                     void *user_data)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_pinned_prompt_completion_set)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_pinned_prompt_completion_set"));
    return real_fn(screen, fn, user_data);
}

MOCK_WEAK_IMPL(int, cplat_pinned_prompt_completion_set, cplat_pinned_prompt *screen, cplat_prompt_completion_fn fn,
               void *user_data)
{
    int mock_ret;
    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_pinned_prompt_completion_set(screen, fn, user_data);
    }
    else
    {
        mock_ret = delegate_real_cplat_pinned_prompt_completion_set(screen, fn, user_data);
    }
    return mock_ret;
}
