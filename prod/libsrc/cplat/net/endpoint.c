#include <cplat/net/endpoint.h>

/* IPv4 ループバック アドレス 127.0.0.1 のホスト バイト オーダー表現 (RFC 1122 / RFC 5735) */
#define IPV4_LOOPBACK_HOST_ORDER UINT32_C(0x7f000001)

/* Doxygen コメントは、ヘッダーに記載 */

uint32_t cplat_ipv4_addr_loopback(void)
{
    return cplat_hton32(IPV4_LOOPBACK_HOST_ORDER);
}
