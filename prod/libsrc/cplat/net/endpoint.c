#include <cplat/net/endpoint.h>

/* Doxygen コメントは、ヘッダーに記載 */

uint32_t cplat_ipv4_addr_loopback(void)
{
    return cplat_hton32(UINT32_C(0x7f000001));
}
