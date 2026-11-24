#include <testfw.h>
#include <mock_cplat.h>

void delegate_real_cplat_socket_wait_cancel_dispose(cplat_socket_wait_cancel *cancel)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_socket_wait_cancel_dispose)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_socket_wait_cancel_dispose"));

    real_fn(cancel);
}

MOCK_WEAK_IMPL(void, cplat_socket_wait_cancel_dispose, cplat_socket_wait_cancel *cancel)
{
    if (_mock_cplat != nullptr)
    {
        _mock_cplat->cplat_socket_wait_cancel_dispose(cancel);
    }
    else
    {
        delegate_real_cplat_socket_wait_cancel_dispose(cancel);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s\n", __func__);
    }
}