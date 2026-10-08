#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_WINDOWS)

int delegate_real_cplat_utf8_to_wpath(wchar_t *wbuf, size_t wbuf_count, const char *utf8_path)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_utf8_to_wpath)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_utf8_to_wpath"));

    return real_fn(wbuf, wbuf_count, utf8_path);
}

MOCK_WEAK_IMPL(int, cplat_utf8_to_wpath, wchar_t *wbuf, size_t wbuf_count, const char *utf8_path)
{
    if (_mock_cplat != nullptr)
    {
        return _mock_cplat->cplat_utf8_to_wpath(wbuf, wbuf_count, utf8_path);
    }
    else
    {
        return delegate_real_cplat_utf8_to_wpath(wbuf, wbuf_count, utf8_path);
    }
}

int delegate_real_cplat_utf8_to_wstr(wchar_t *wbuf, size_t wbuf_count, const char *utf8_text)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_utf8_to_wstr)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_utf8_to_wstr"));

    return real_fn(wbuf, wbuf_count, utf8_text);
}

MOCK_WEAK_IMPL(int, cplat_utf8_to_wstr, wchar_t *wbuf, size_t wbuf_count, const char *utf8_text)
{
    if (_mock_cplat != nullptr)
    {
        return _mock_cplat->cplat_utf8_to_wstr(wbuf, wbuf_count, utf8_text);
    }
    else
    {
        return delegate_real_cplat_utf8_to_wstr(wbuf, wbuf_count, utf8_text);
    }
}

int delegate_real_cplat_wpath_to_utf8(char *dest, size_t dest_size, const wchar_t *wpath)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_wpath_to_utf8)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_wpath_to_utf8"));

    return real_fn(dest, dest_size, wpath);
}

MOCK_WEAK_IMPL(int, cplat_wpath_to_utf8, char *dest, size_t dest_size, const wchar_t *wpath)
{
    if (_mock_cplat != nullptr)
    {
        return _mock_cplat->cplat_wpath_to_utf8(dest, dest_size, wpath);
    }
    else
    {
        return delegate_real_cplat_wpath_to_utf8(dest, dest_size, wpath);
    }
}

int delegate_real_cplat_wstr_to_utf8(char *dest, size_t dest_size, const wchar_t *wtext)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_wstr_to_utf8)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_wstr_to_utf8"));

    return real_fn(dest, dest_size, wtext);
}

MOCK_WEAK_IMPL(int, cplat_wstr_to_utf8, char *dest, size_t dest_size, const wchar_t *wtext)
{
    if (_mock_cplat != nullptr)
    {
        return _mock_cplat->cplat_wstr_to_utf8(dest, dest_size, wtext);
    }
    else
    {
        return delegate_real_cplat_wstr_to_utf8(dest, dest_size, wtext);
    }
}

wchar_t *delegate_real_cplat_utf8_to_wstr_alloc(const char *utf8_text)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_utf8_to_wstr_alloc)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_utf8_to_wstr_alloc"));

    return real_fn(utf8_text);
}

MOCK_WEAK_IMPL(wchar_t *, cplat_utf8_to_wstr_alloc, const char *utf8_text)
{
    if (_mock_cplat != nullptr)
    {
        return _mock_cplat->cplat_utf8_to_wstr_alloc(utf8_text);
    }
    else
    {
        return delegate_real_cplat_utf8_to_wstr_alloc(utf8_text);
    }
}

char *delegate_real_cplat_wstr_to_utf8_alloc(const wchar_t *wtext)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_wstr_to_utf8_alloc)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_wstr_to_utf8_alloc"));

    return real_fn(wtext);
}

MOCK_WEAK_IMPL(char *, cplat_wstr_to_utf8_alloc, const wchar_t *wtext)
{
    if (_mock_cplat != nullptr)
    {
        return _mock_cplat->cplat_wstr_to_utf8_alloc(wtext);
    }
    else
    {
        return delegate_real_cplat_wstr_to_utf8_alloc(wtext);
    }
}

#endif /* PLATFORM_WINDOWS */
