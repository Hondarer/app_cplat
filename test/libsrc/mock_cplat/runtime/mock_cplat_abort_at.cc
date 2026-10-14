#include <testfw.h>
#include <mock_cplat.h>

#include <cstdlib>

void delegate_real_cplat_abort_at(cplat_abort_kind kind, const char *message, const char *file,
                                  int line, const char *function)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_abort_at)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_abort_at"));

    real_fn(kind, message, file, line, function);
}

MOCK_WEAK_IMPL(void, cplat_abort_at, cplat_abort_kind kind, const char *message, const char *file,
               int line, const char *function)
{
    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }

    if (_mock_cplat != nullptr)
    {
        _mock_cplat->cplat_abort_at(kind, message, file, line, function);
    }
    else
    {
        delegate_real_cplat_abort_at(kind, message, file, line, function);
    }

    /* 公開ヘッダーが戻らない関数として宣言しているため、mock から戻った場合も終了する */
    abort();
}
