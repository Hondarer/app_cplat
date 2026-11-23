#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_utf8_to_wstr(wchar_t *dest, size_t dest_size, size_t *count_out, const char *utf8_text)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_utf8_to_wstr)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_utf8_to_wstr"));
    return real_fn(dest, dest_size, count_out, utf8_text);
}

MOCK_WEAK_IMPL(int, cplat_utf8_to_wstr, wchar_t *dest, size_t dest_size, size_t *count_out, const char *utf8_text)
{
    return _mock_cplat != nullptr ? _mock_cplat->cplat_utf8_to_wstr(dest, dest_size, count_out, utf8_text)
                                  : delegate_real_cplat_utf8_to_wstr(dest, dest_size, count_out, utf8_text);
}

int delegate_real_cplat_wstr_to_utf8(char *dest, size_t dest_size, size_t *count_out, const wchar_t *wtext)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_wstr_to_utf8)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_wstr_to_utf8"));
    return real_fn(dest, dest_size, count_out, wtext);
}

MOCK_WEAK_IMPL(int, cplat_wstr_to_utf8, char *dest, size_t dest_size, size_t *count_out, const wchar_t *wtext)
{
    return _mock_cplat != nullptr ? _mock_cplat->cplat_wstr_to_utf8(dest, dest_size, count_out, wtext)
                                  : delegate_real_cplat_wstr_to_utf8(dest, dest_size, count_out, wtext);
}

int delegate_real_cplat_utf8_to_wpath(wchar_t *dest, size_t dest_size, size_t *count_out, const char *utf8_path)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_utf8_to_wpath)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_utf8_to_wpath"));
    return real_fn(dest, dest_size, count_out, utf8_path);
}

MOCK_WEAK_IMPL(int, cplat_utf8_to_wpath, wchar_t *dest, size_t dest_size, size_t *count_out, const char *utf8_path)
{
    return _mock_cplat != nullptr ? _mock_cplat->cplat_utf8_to_wpath(dest, dest_size, count_out, utf8_path)
                                  : delegate_real_cplat_utf8_to_wpath(dest, dest_size, count_out, utf8_path);
}

int delegate_real_cplat_wpath_to_utf8(char *dest, size_t dest_size, size_t *count_out, const wchar_t *wpath)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_wpath_to_utf8)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_wpath_to_utf8"));
    return real_fn(dest, dest_size, count_out, wpath);
}

MOCK_WEAK_IMPL(int, cplat_wpath_to_utf8, char *dest, size_t dest_size, size_t *count_out, const wchar_t *wpath)
{
    return _mock_cplat != nullptr ? _mock_cplat->cplat_wpath_to_utf8(dest, dest_size, count_out, wpath)
                                  : delegate_real_cplat_wpath_to_utf8(dest, dest_size, count_out, wpath);
}

int delegate_real_cplat_utf8_to_wstr_alloc(wchar_t **wtext_out, const char *utf8_text)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_utf8_to_wstr_alloc)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_utf8_to_wstr_alloc"));
    return real_fn(wtext_out, utf8_text);
}

MOCK_WEAK_IMPL(int, cplat_utf8_to_wstr_alloc, wchar_t **wtext_out, const char *utf8_text)
{
    return _mock_cplat != nullptr ? _mock_cplat->cplat_utf8_to_wstr_alloc(wtext_out, utf8_text)
                                  : delegate_real_cplat_utf8_to_wstr_alloc(wtext_out, utf8_text);
}

int delegate_real_cplat_wstr_to_utf8_alloc(char **utf8_text_out, const wchar_t *wtext)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_wstr_to_utf8_alloc)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_wstr_to_utf8_alloc"));
    return real_fn(utf8_text_out, wtext);
}

MOCK_WEAK_IMPL(int, cplat_wstr_to_utf8_alloc, char **utf8_text_out, const wchar_t *wtext)
{
    return _mock_cplat != nullptr ? _mock_cplat->cplat_wstr_to_utf8_alloc(utf8_text_out, wtext)
                                  : delegate_real_cplat_wstr_to_utf8_alloc(utf8_text_out, wtext);
}
