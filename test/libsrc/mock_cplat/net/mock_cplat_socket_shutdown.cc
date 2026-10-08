#include <testfw.h>
#include <mock_cplat.h>

void delegate_real_cplat_socket_shutdown(cplat_socket sock)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_socket_shutdown)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_socket_shutdown"));

    real_fn(sock);
}

MOCK_WEAK_IMPL(void, cplat_socket_shutdown, cplat_socket sock)
{
    if (_mock_cplat != nullptr)
    {
        _mock_cplat->cplat_socket_shutdown(sock);
    }
    else
    {
        delegate_real_cplat_socket_shutdown(sock);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }
}
