/**
 *******************************************************************************
 *  @file           fcntl.c
 *  @brief          UTF-8 パスに対応したファイル記述子オープン API を実装します。
 *
 *  OS ごとの文字コード変換と open() 呼び出しの差異を吸収します。
 *
 *******************************************************************************
 */

#include <cplat/crt/fcntl.h>
#include <cplat/crt/path.h>

#include <cplat/crt/wchar_conv.h>
#include <cplat/win32/wchar_conv_internal.h>
#include <cplat/base/error_internal.h>

#include <errno.h>

#if defined(PLATFORM_LINUX)
    #include <unistd.h>
#elif defined(PLATFORM_WINDOWS)
    #include <share.h>
#endif /* PLATFORM_ */

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_open(const char *path, const int flags, const int mode, cplat_error *detail_out)
{
    int fd;

    if (path == NULL)
    {
        cplat_internal_error_record_errno(detail_out, EINVAL);
        return -1;
    }

#if defined(PLATFORM_LINUX)
    errno = 0;

    /* FIFO のオープンなど待機を伴う場合はシグナルで中断されうる。中断を吸収する。 */
    do
    {
        fd = open(path, flags, mode);
    } while ((fd < 0) && (errno == EINTR));
#elif defined(PLATFORM_WINDOWS)
    {
        wchar_t wpath[PLATFORM_PATH_MAX];
        errno_t err;
        int rc;

        fd = -1;

        rc = cplat_utf8_to_wpath(wpath, sizeof(wpath) / sizeof(wpath[0]), NULL, path);
        if (rc != CPLAT_OK)
        {
            cplat_internal_error_record_errno(detail_out, cplat_internal_errno_from_conv_result(rc, 1));
            return -1;
        }

        err = _wsopen_s(&fd, wpath, flags, _SH_DENYNO, mode);
        if (err != 0)
        {
            cplat_internal_error_record_errno(detail_out, (int)err);
            return -1;
        }
    }
#endif /* PLATFORM_ */

    if (fd < 0)
    {
        const int errno_value = errno;

        cplat_internal_error_record_errno(detail_out, errno_value);
    }
    else
    {
        cplat_internal_error_record_success(detail_out);
    }

    return fd;
}
