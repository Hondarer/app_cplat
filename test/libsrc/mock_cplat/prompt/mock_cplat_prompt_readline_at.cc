#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_prompt_readline_at(cplat_prompt *p, char *buf, size_t buf_size, const char *prompt_str,
                                           const char *file, int line)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_prompt_readline_at)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_prompt_readline_at"));

    return real_fn(p, buf, buf_size, prompt_str, file, line);
}

MOCK_WEAK_IMPL(int, cplat_prompt_readline_at, cplat_prompt *p, char *buf, size_t buf_size, const char *prompt_str,
               const char *file, int line)
{
    int mock_ret = 0;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_prompt_readline_at(p, buf, buf_size, prompt_str, file, line);
    }
    else
    {
        mock_ret = delegate_real_cplat_prompt_readline_at(p, buf, buf_size, prompt_str, file, line);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s \"%s\"", __func__, prompt_str != nullptr ? prompt_str : "");
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
