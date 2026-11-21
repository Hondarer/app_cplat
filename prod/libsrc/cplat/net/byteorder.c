/**
 *******************************************************************************
 *  @file           byteorder.c
 *  @brief          ホスト バイト オーダーとネットワーク バイト オーダーを相互変換します。
 *  @author         Tetsuo Honda
 *  @date           2026/08/15
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/net/byteorder.h>
#include <string.h>

/* Doxygen コメントは、ヘッダーに記載 */

uint16_t cplat_hton16(const uint16_t value)
{
    uint8_t bytes[sizeof(value)];
    uint16_t result;

    bytes[0] = (uint8_t)(((uint32_t)value >> 8U) & UINT32_C(0xFF));
    bytes[1] = (uint8_t)((uint32_t)value & UINT32_C(0xFF));
    memcpy(&result, bytes, sizeof(result));

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

uint16_t cplat_ntoh16(const uint16_t value)
{
    uint8_t bytes[sizeof(value)];

    memcpy(bytes, &value, sizeof(value));

    return (uint16_t)(((uint16_t)bytes[0] << 8) | (uint16_t)bytes[1]);
}

/* Doxygen コメントは、ヘッダーに記載 */

uint32_t cplat_hton32(const uint32_t value)
{
    uint8_t bytes[sizeof(value)];
    uint32_t result;

    bytes[0] = (uint8_t)((value >> 24) & 0xFFU);
    bytes[1] = (uint8_t)((value >> 16) & 0xFFU);
    bytes[2] = (uint8_t)((value >> 8) & 0xFFU);
    bytes[3] = (uint8_t)(value & 0xFFU);
    memcpy(&result, bytes, sizeof(result));

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

uint32_t cplat_ntoh32(const uint32_t value)
{
    uint8_t bytes[sizeof(value)];

    memcpy(bytes, &value, sizeof(value));

    return (((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) | ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3]);
}

/* Doxygen コメントは、ヘッダーに記載 */

uint64_t cplat_hton64(const uint64_t value)
{
    uint8_t bytes[sizeof(value)];
    uint64_t result;

    bytes[0] = (uint8_t)((value >> 56) & 0xFFU);
    bytes[1] = (uint8_t)((value >> 48) & 0xFFU);
    bytes[2] = (uint8_t)((value >> 40) & 0xFFU);
    bytes[3] = (uint8_t)((value >> 32) & 0xFFU);
    bytes[4] = (uint8_t)((value >> 24) & 0xFFU);
    bytes[5] = (uint8_t)((value >> 16) & 0xFFU);
    bytes[6] = (uint8_t)((value >> 8) & 0xFFU);
    bytes[7] = (uint8_t)(value & 0xFFU);
    memcpy(&result, bytes, sizeof(result));

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

uint64_t cplat_ntoh64(const uint64_t value)
{
    uint8_t bytes[sizeof(value)];

    memcpy(bytes, &value, sizeof(value));

    return (((uint64_t)bytes[0] << 56) | ((uint64_t)bytes[1] << 48) | ((uint64_t)bytes[2] << 40) |
            ((uint64_t)bytes[3] << 32) | ((uint64_t)bytes[4] << 24) | ((uint64_t)bytes[5] << 16) |
            ((uint64_t)bytes[6] << 8) | (uint64_t)bytes[7]);
}
