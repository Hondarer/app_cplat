#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_socket_accept(cplat_socket sock, cplat_ipv4_endpoint *peer_out, cplat_socket *sock_out,
                                      cplat_error *detail_out)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_socket_accept)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_socket_accept"));

    return real_fn(sock, peer_out, sock_out, detail_out);
}

MOCK_WEAK_IMPL(int, cplat_socket_accept, cplat_socket sock, cplat_ipv4_endpoint *peer_out, cplat_socket *sock_out,
               cplat_error *detail_out)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_socket_accept(sock, peer_out, sock_out, detail_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_socket_accept(sock, peer_out, sock_out, detail_out);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s", __func__);
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
