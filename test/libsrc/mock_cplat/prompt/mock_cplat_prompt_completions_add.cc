#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_prompt_completions_add(cplat_prompt_completions *completions, size_t start,
                                               const char *candidate)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_prompt_completions_add)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_prompt_completions_add"));
    return real_fn(completions, start, candidate);
}

MOCK_WEAK_IMPL(int, cplat_prompt_completions_add, cplat_prompt_completions *completions, size_t start,
               const char *candidate)
{
    int mock_ret;
    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_prompt_completions_add(completions, start, candidate);
    }
    else
    {
        mock_ret = delegate_real_cplat_prompt_completions_add(completions, start, candidate);
    }
    return mock_ret;
}
