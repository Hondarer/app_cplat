#include <stdarg.h>
#include <stdio.h>
#include <vector>
#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_pinned_prompt_readline_fmt_at(cplat_pinned_prompt *screen, char *buf, size_t buf_size,
                                                      const char *file, int line, const char *fmt, va_list args)
{
    /* 実装側は必要長に応じてバッファーを伸長するため、モックでも切り詰めずに展開する */
    std::vector<char> prompt = mock_cplat_expand_format(fmt, args);

    static auto real_fn = reinterpret_cast<decltype(&cplat_pinned_prompt_readline_at)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_pinned_prompt_readline_at"));

    return real_fn(screen, buf, buf_size, prompt.data(), file, line);
}

MOCK_WEAK_IMPL(int, cplat_pinned_prompt_readline_fmt_at, cplat_pinned_prompt *screen, char *buf, size_t buf_size,
               const char *file, int line, const char *fmt, ...)
{
    int mock_ret = 0;
    va_list args;

    va_start(args, fmt);

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_pinned_prompt_readline_fmt_at(screen, buf, buf_size, file, line, fmt, args);
    }
    else
    {
        mock_ret = delegate_real_cplat_pinned_prompt_readline_fmt_at(screen, buf, buf_size, file, line, fmt, args);
    }

    va_end(args);

    if (getTraceLevel() > TRACE_NONE)
    {
        const char *fmt_text = fmt;

        if (fmt_text == nullptr)
        {
            fmt_text = "";
        }
        printf("  > %s \"%s\"", __func__, fmt_text);
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
