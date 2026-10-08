#include <testfw.h>
#include <mock_cplat.h>

size_t delegate_real_cplat_fwrite(const void *buffer, size_t size, size_t count, FILE *stream, cplat_error *detail_out)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_fwrite)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_fwrite"));

    return real_fn(buffer, size, count, stream, detail_out);
}

MOCK_WEAK_IMPL(size_t, cplat_fwrite, const void *buffer, size_t size, size_t count, FILE *stream,
               cplat_error *detail_out)
{
    if (_mock_cplat != nullptr)
    {
        return _mock_cplat->cplat_fwrite(buffer, size, count, stream, detail_out);
    }

    return delegate_real_cplat_fwrite(buffer, size, count, stream, detail_out);
}
