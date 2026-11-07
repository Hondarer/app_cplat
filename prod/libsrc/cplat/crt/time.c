/**
 *******************************************************************************
 *  @file           time.c
 *  @brief          time 系の CRT 関数を抽象化する API を実装します。
 *
 *  UTC 時刻とローカル時刻への変換、およびローカル時刻の文字列化を
 *  スレッド セーフな OS API へ委譲します。
 *
 *******************************************************************************
 */

#include <cplat/crt/time.h>

#include <string.h>

#include <cplat/base/platform.h>
#include <cplat/base/result.h>

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_gmtime(struct tm *utc_tm, const time_t *timep)
{
    if (utc_tm == NULL || timep == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

#if defined(PLATFORM_LINUX)
    if (gmtime_r(timep, utc_tm) == NULL)
    {
        memset(utc_tm, 0, sizeof(*utc_tm));
        return CPLAT_ERR_UNKNOWN;
    }

    return CPLAT_OK;
#elif defined(PLATFORM_WINDOWS)
    if (gmtime_s(utc_tm, timep) != 0)
    {
        memset(utc_tm, 0, sizeof(*utc_tm));
        return CPLAT_ERR_UNKNOWN;
    }

    return CPLAT_OK;
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_localtime(struct tm *local_tm, const time_t *timep)
{
    if (local_tm == NULL || timep == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

#if defined(PLATFORM_LINUX)
    if (localtime_r(timep, local_tm) == NULL)
    {
        memset(local_tm, 0, sizeof(*local_tm));
        return CPLAT_ERR_UNKNOWN;
    }

    return CPLAT_OK;
#elif defined(PLATFORM_WINDOWS)
    if (localtime_s(local_tm, timep) != 0)
    {
        memset(local_tm, 0, sizeof(*local_tm));
        return CPLAT_ERR_UNKNOWN;
    }

    return CPLAT_OK;
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_ctime(char *buf, const size_t buf_size, const time_t *timep)
{
    if (buf == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    /*
     * ctime_r() は格納先に 26 [byte] 以上を要求する。
     * 両プラットフォームで挙動を揃えるため、委譲前に検査する。
     * see: https://man7.org/linux/man-pages/man3/ctime_r.3.html
     */
    if (timep == NULL || buf_size < 26)
    {
        memset(buf, 0, buf_size);
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

#if defined(PLATFORM_LINUX)
    if (ctime_r(timep, buf) == NULL)
    {
        memset(buf, 0, buf_size);
        return CPLAT_ERR_UNKNOWN;
    }

    return CPLAT_OK;
#elif defined(PLATFORM_WINDOWS)
    if (ctime_s(buf, buf_size, timep) != 0)
    {
        memset(buf, 0, buf_size);
        return CPLAT_ERR_UNKNOWN;
    }

    return CPLAT_OK;
#endif /* PLATFORM_ */
}
