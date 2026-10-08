#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_pinned_prompt_write(cplat_pinned_prompt *screen, cplat_pinned_prompt_channel channel,
                                            const void *data, size_t size, size_t *written_out)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_pinned_prompt_write)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_pinned_prompt_write"));

    return real_fn(screen, channel, data, size, written_out);
}

MOCK_WEAK_IMPL(int, cplat_pinned_prompt_write, cplat_pinned_prompt *screen, cplat_pinned_prompt_channel channel,
               const void *data, size_t size, size_t *written_out)
{
    int mock_ret = 0;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_pinned_prompt_write(screen, channel, data, size, written_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_pinned_prompt_write(screen, channel, data, size, written_out);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p, %d, 0x%p, %zu", __func__, (void *)screen, (int)channel, data, size);
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
