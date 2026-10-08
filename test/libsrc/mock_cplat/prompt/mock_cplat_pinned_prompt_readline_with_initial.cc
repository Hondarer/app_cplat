#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_pinned_prompt_readline_with_initial_at(cplat_pinned_prompt *screen, char *buf, size_t buf_size,
                                                               const char *prompt_str, const char *initial_text,
                                                               const char *file, int line)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_pinned_prompt_readline_with_initial_at)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_pinned_prompt_readline_with_initial_at"));

    return real_fn(screen, buf, buf_size, prompt_str, initial_text, file, line);
}

MOCK_WEAK_IMPL(int, cplat_pinned_prompt_readline_with_initial_at, cplat_pinned_prompt *screen, char *buf,
               size_t buf_size, const char *prompt_str, const char *initial_text, const char *file, int line)
{
    int mock_ret = 0;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_pinned_prompt_readline_with_initial_at(screen, buf, buf_size, prompt_str,
                                                                             initial_text, file, line);
    }
    else
    {
        mock_ret = delegate_real_cplat_pinned_prompt_readline_with_initial_at(screen, buf, buf_size, prompt_str,
                                                                              initial_text, file, line);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s \"%s\" \"%s\"", __func__, prompt_str != nullptr ? prompt_str : "",
               initial_text != nullptr ? initial_text : "");
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
