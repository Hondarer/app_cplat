#include <testfw.h>
#include <mock_cplat.h>

const char *delegate_real_cplat_path_extension(const char *path)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_path_extension)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_path_extension"));

    return real_fn(path);
}

MOCK_WEAK_IMPL(const char *, cplat_path_extension, const char *path)
{
    const char *mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_path_extension(path);
    }
    else
    {
        mock_ret = delegate_real_cplat_path_extension(path);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }

    return mock_ret;
}
