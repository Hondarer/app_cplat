#include <testfw.h>
#include <mock_cplat.h>

void delegate_real_cplat_socket_watch_detach(cplat_socket_watch *watch)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_socket_watch_detach)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_socket_watch_detach"));

    real_fn(watch);
}

MOCK_WEAK_IMPL(void, cplat_socket_watch_detach, cplat_socket_watch *watch)
{
    if (_mock_cplat != nullptr)
    {
        _mock_cplat->cplat_socket_watch_detach(watch);
    }
    else
    {
        delegate_real_cplat_socket_watch_detach(watch);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }
}