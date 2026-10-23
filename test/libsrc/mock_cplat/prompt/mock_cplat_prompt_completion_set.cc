#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_prompt_completion_set(cplat_prompt *prompt, cplat_prompt_completion_fn fn, void *user_data)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_prompt_completion_set)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_prompt_completion_set"));
    return real_fn(prompt, fn, user_data);
}

MOCK_WEAK_IMPL(int, cplat_prompt_completion_set, cplat_prompt *prompt, cplat_prompt_completion_fn fn, void *user_data)
{
    int mock_ret;
    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_prompt_completion_set(prompt, fn, user_data);
    }
    else
    {
        mock_ret = delegate_real_cplat_prompt_completion_set(prompt, fn, user_data);
    }
    return mock_ret;
}
