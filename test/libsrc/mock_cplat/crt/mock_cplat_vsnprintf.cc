#include <stdarg.h>
#include <stdio.h>
#include <vector>
#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_vsnprintf(char *dest, size_t dest_size, const char *format, va_list args)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_vsnprintf)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_vsnprintf"));

    return real_fn(dest, dest_size, format, args);
}

// mock_cplat_expand_format は mock_cplat_expand_format.cc で定義する

MOCK_WEAK_IMPL(int, cplat_vsnprintf, char *dest, size_t dest_size, const char *format, va_list args)
{
    int mock_ret = -1;

    if (_mock_cplat != nullptr)
    {
        std::vector<char> buf = mock_cplat_expand_format(format, args);

        mock_ret = _mock_cplat->cplat_vsnprintf(dest, dest_size, buf.data());

        if (getTraceLevel() > TRACE_NONE)
        {
            printf("  > %s 0x%p, %zu, %s", __func__, (void *)dest, dest_size, buf.data());
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

    mock_ret = delegate_real_cplat_vsnprintf(dest, dest_size, format, args);

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p, %zu, %s", __func__, (void *)dest, dest_size, format);
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
