#include <testfw.h>
#include <mock_cplat.h>

int delegate_real_cplat_socket_send(cplat_socket sock, const void *buf, size_t len, size_t *sent_out,
                                    cplat_error *detail_out)
{
    static auto real_fn =
        reinterpret_cast<decltype(&cplat_socket_send)>(resolveSharedSymbolOrExit(kLibCplatName, "cplat_socket_send"));

    return real_fn(sock, buf, len, sent_out, detail_out);
}

MOCK_WEAK_IMPL(int, cplat_socket_send, cplat_socket sock, const void *buf, size_t len, size_t *sent_out,
               cplat_error *detail_out)
{
    int mock_ret = CPLAT_ERR_UNKNOWN;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_socket_send(sock, buf, len, sent_out, detail_out);
    }
    else
    {
        mock_ret = delegate_real_cplat_socket_send(sock, buf, len, sent_out, detail_out);
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
