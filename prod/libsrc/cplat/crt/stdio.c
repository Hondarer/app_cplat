/**
 *******************************************************************************
 *  @file           stdio.c
 *  @brief          stdio 系の C 標準入出力関数を抽象化する API を実装します。
 *
 *  UTF-8 パスと 64 bit ファイル位置に対応した標準 I/O ラッパーを提供します。
 *
 *  本ファイルの stdio ラッパーは、EINTR の再試行を行いません。
 *  中断時に FILE * の内部状態と読み書き位置が確定しないためです。
 *  適用対象を通常ファイルに限定することで中断を回避しており、端末、パイプ、
 *  ソケットを扱う場合は記述子を扱う API (cplat_read()、cplat_write()) を使用します。
 *
 *******************************************************************************
 */

#include <cplat/crt/stdio.h>
#include <cplat/crt/path.h>

#include <cplat/crt/wchar_conv.h>
#include <cplat/base/error_internal.h>

#include <errno.h>
#include <limits.h>
#include <string.h>

#if defined(PLATFORM_LINUX)
    #include <sys/types.h>
#elif defined(PLATFORM_WINDOWS)
    #include <io.h>
    #include <share.h>
    #include <stdlib.h>
    #include <wchar.h>
#endif /* PLATFORM_ */

/* Doxygen コメントは、ヘッダーに記載 */

FILE *cplat_fopen(const char *path, const char *modes, cplat_error *detail_out)
{
    if (path == NULL || modes == NULL)
    {
        (void)cplat_internal_error_report_errno(detail_out, EINVAL);
        return NULL;
    }

#if defined(PLATFORM_LINUX)
    {
        FILE *fp;
        errno = 0;
        fp = fopen(path, modes);
        if (fp == NULL)
        {
            const int errno_value = errno;

            (void)cplat_internal_error_report_errno(detail_out, errno_value);
        }
        else
        {
            (void)cplat_internal_error_report_success(detail_out);
        }
        return fp;
    }
#elif defined(PLATFORM_WINDOWS)
    {
        wchar_t wpath[PLATFORM_PATH_MAX];
        wchar_t wmodes[64];
        FILE *fp = NULL;
        errno_t err;
        size_t converted;

        if (cplat_utf8_to_wpath(wpath, sizeof(wpath) / sizeof(wpath[0]), path) < 0)
        {
            (void)cplat_internal_error_report_errno(detail_out, ENAMETOOLONG);
            return NULL;
        }

        err = mbstowcs_s(&converted, wmodes, sizeof(wmodes) / sizeof(wmodes[0]), modes, _TRUNCATE);
        if (err != 0)
        {
            (void)cplat_internal_error_report_errno(detail_out, EINVAL);
            return NULL;
        }

        /* Linux の fopen は強制ロックを持たず常に共有可。_wfopen_s は排他オープンとなるため、
         * 挙動をそろえるために _wfsopen + _SH_DENYNO を採用する。
         * see: https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/fsopen-wfsopen */
        errno = 0;
        fp = _wfsopen(wpath, wmodes, _SH_DENYNO);
        if (fp == NULL)
        {
            const int errno_value = errno;

            (void)cplat_internal_error_report_errno(detail_out, errno_value);
            return NULL;
        }

        (void)cplat_internal_error_report_success(detail_out);
        return fp;
    }
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

FILE *cplat_freopen(const char *path, const char *modes, FILE *stream, cplat_error *detail_out)
{
    if (path == NULL || modes == NULL || stream == NULL)
    {
        (void)cplat_internal_error_report_errno(detail_out, EINVAL);
        return NULL;
    }

#if defined(PLATFORM_LINUX)
    {
        FILE *fp;
        errno = 0;
        fp = freopen(path, modes, stream);
        if (fp == NULL)
        {
            const int errno_value = errno;

            (void)cplat_internal_error_report_errno(detail_out, errno_value);
        }
        else
        {
            (void)cplat_internal_error_report_success(detail_out);
        }
        return fp;
    }
#elif defined(PLATFORM_WINDOWS)
    {
        wchar_t wpath[PLATFORM_PATH_MAX];
        wchar_t wmodes[64];
        FILE *new_fp = NULL;
        errno_t err;
        size_t converted;
        int new_fd;
        int old_fd;

        if (cplat_utf8_to_wpath(wpath, sizeof(wpath) / sizeof(wpath[0]), path) < 0)
        {
            (void)cplat_internal_error_report_errno(detail_out, ENAMETOOLONG);
            return NULL;
        }

        err = mbstowcs_s(&converted, wmodes, sizeof(wmodes) / sizeof(wmodes[0]), modes, _TRUNCATE);
        if (err != 0)
        {
            (void)cplat_internal_error_report_errno(detail_out, EINVAL);
            return NULL;
        }

        /* Windows CRT には _fsopen 相当の freopen 版が無いため、共有モードで開いた
         * 新規 FILE* の fd を _dup2 で既存 stream に複製して FILE* を維持する。
         * これにより Linux の freopen と同じく共有可能なオープン挙動になる。
         * see: https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/fsopen-wfsopen */
        errno = 0;
        new_fp = _wfsopen(wpath, wmodes, _SH_DENYNO);
        if (new_fp == NULL)
        {
            const int errno_value = errno;

            (void)cplat_internal_error_report_errno(detail_out, errno_value);
            return NULL;
        }

        (void)fflush(stream);
        new_fd = _fileno(new_fp);
        old_fd = _fileno(stream);
        if (_dup2(new_fd, old_fd) != 0)
        {
            int saved = errno;
            (void)fclose(new_fp);
            (void)cplat_internal_error_report_errno(detail_out, saved);
            return NULL;
        }
        (void)fclose(new_fp); /* new_fd を解放。stream は old_fd を介して新ファイルを保持 */
        clearerr(stream);
        (void)cplat_internal_error_report_success(detail_out);
        return stream;
    }
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_fclose(FILE *stream, cplat_error *detail_out)
{
    int result;

    if (stream == NULL)
    {
        (void)cplat_internal_error_report_errno(detail_out, EINVAL);
        return EOF;
    }

    errno = 0;
    result = fclose(stream);
    if (result != 0)
    {
        int errno_value = errno;

        if (errno_value == 0)
        {
            errno_value = EIO;
        }

        (void)cplat_internal_error_report_errno(detail_out, errno_value);
        return result;
    }

    (void)cplat_internal_error_report_success(detail_out);
    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_fflush(FILE *stream, cplat_error *detail_out)
{
    int result;

    errno = 0;
    result = fflush(stream);
    if (result != 0)
    {
        int errno_value = errno;

        if (errno_value == 0)
        {
            errno_value = EIO;
        }

        (void)cplat_internal_error_report_errno(detail_out, errno_value);
        return result;
    }

    (void)cplat_internal_error_report_success(detail_out);
    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

size_t cplat_fread(void *buffer, const size_t size, const size_t count, FILE *stream, cplat_error *detail_out)
{
    size_t read_count;

    if ((buffer == NULL && size > 0u && count > 0u) || stream == NULL)
    {
        (void)cplat_internal_error_report_errno(detail_out, EINVAL);
        return 0u;
    }

    if (size == 0u || count == 0u)
    {
        /* NOTE: C 標準では、size または count が 0 の fread は 0 を返し、ストリームの状態を変えない。
         * 一方で、ライブラリ関数へ渡すポインタは長さ 0 でも有効である必要があるため、
         * NULL の buffer を fread へ渡さないよう、fread を呼ばずに同じ結果を返す。
         * see: https://en.cppreference.com/w/c/io/fread */
        (void)cplat_internal_error_report_success(detail_out);
        return 0u;
    }

    errno = 0;
    read_count = fread(buffer, size, count, stream);
    if (read_count < count && ferror(stream) != 0)
    {
        /* NOTE: Windows UCRT では NUL の書き込み専用ストリームを fread で読み取ると、
         * ストリームのエラー指示は ferror に反映されても errno が 0 の場合がある。
         * ferror はストリームのエラー指示を返すが、errno はシステム呼び出しのエラー値と
         * 常に一致するとは限らないため、errno == 0 のストリーム エラーを EIO で表す。
         * see: https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/ferror?view=msvc-170
         * see: https://learn.microsoft.com/en-us/cpp/c-runtime-library/errno-doserrno-sys-errlist-and-sys-nerr?view=msvc-170 */
        int errno_value = errno;

        if (errno_value == 0)
        {
            errno_value = EIO;
        }
        (void)cplat_internal_error_report_errno(detail_out, errno_value);
    }
    else
    {
        (void)cplat_internal_error_report_success(detail_out);
    }

    return read_count;
}

/* Doxygen コメントは、ヘッダーに記載 */

size_t cplat_fwrite(const void *buffer, const size_t size, const size_t count, FILE *stream, cplat_error *detail_out)
{
    size_t written_count;

    if ((buffer == NULL && size > 0u && count > 0u) || stream == NULL)
    {
        (void)cplat_internal_error_report_errno(detail_out, EINVAL);
        return 0u;
    }

    if (size == 0u || count == 0u)
    {
        /* NOTE: C 標準では、size または count が 0 の fwrite は 0 を返し、ストリームの状態を変えない。
         * 一方で、ライブラリ関数へ渡すポインタは長さ 0 でも有効である必要があるため、
         * NULL の buffer を fwrite へ渡さないよう、fwrite を呼ばずに同じ結果を返す。
         * see: https://en.cppreference.com/w/c/io/fwrite */
        (void)cplat_internal_error_report_success(detail_out);
        return 0u;
    }

    errno = 0;
    written_count = fwrite(buffer, size, count, stream);
    if (written_count < count)
    {
        int errno_value = errno;

        if (errno_value == 0)
        {
            errno_value = EIO;
        }
        (void)cplat_internal_error_report_errno(detail_out, errno_value);
    }
    else
    {
        (void)cplat_internal_error_report_success(detail_out);
    }

    return written_count;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_remove(const char *path, cplat_error *detail_out)
{
    int result;

    if (path == NULL)
    {
        (void)cplat_internal_error_report_errno(detail_out, EINVAL);
        return -1;
    }

#if defined(PLATFORM_LINUX)
    errno = 0;
    result = remove(path);
#elif defined(PLATFORM_WINDOWS)
    {
        wchar_t wpath[PLATFORM_PATH_MAX];

        if (cplat_utf8_to_wpath(wpath, sizeof(wpath) / sizeof(wpath[0]), path) < 0)
        {
            (void)cplat_internal_error_report_errno(detail_out, ENAMETOOLONG);
            return -1;
        }

        errno = 0;
        result = _wremove(wpath);
    }
#endif /* PLATFORM_ */

    if (result != 0)
    {
        const int errno_value = errno;

        (void)cplat_internal_error_report_errno(detail_out, errno_value);
        return result;
    }

    (void)cplat_internal_error_report_success(detail_out);
    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_rename(const char *oldpath, const char *newpath, cplat_error *detail_out)
{
    if (oldpath == NULL || newpath == NULL)
    {
        (void)cplat_internal_error_report_errno(detail_out, EINVAL);
        return -1;
    }

#if defined(PLATFORM_LINUX)
    {
        int result;

        errno = 0;
        result = rename(oldpath, newpath);
        if (result != 0)
        {
            const int errno_value = errno;

            (void)cplat_internal_error_report_errno(detail_out, errno_value);
            return result;
        }

        (void)cplat_internal_error_report_success(detail_out);
        return result;
    }
#elif defined(PLATFORM_WINDOWS)
    {
        wchar_t woldpath[PLATFORM_PATH_MAX];
        wchar_t wnewpath[PLATFORM_PATH_MAX];

        if (cplat_utf8_to_wpath(woldpath, sizeof(woldpath) / sizeof(woldpath[0]), oldpath) < 0)
        {
            (void)cplat_internal_error_report_errno(detail_out, ENAMETOOLONG);
            return -1;
        }

        if (cplat_utf8_to_wpath(wnewpath, sizeof(wnewpath) / sizeof(wnewpath[0]), newpath) < 0)
        {
            (void)cplat_internal_error_report_errno(detail_out, ENAMETOOLONG);
            return -1;
        }

        if (!MoveFileExW(woldpath, wnewpath, MOVEFILE_REPLACE_EXISTING))
        {
            const DWORD error_code = GetLastError();

            (void)cplat_internal_error_report_windows_error(detail_out, error_code);
            return -1;
        }
        (void)cplat_internal_error_report_success(detail_out);
        return 0;
    }
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_scanf(const char *format, ...)
{
    va_list args;
    int result;

    va_start(args, format);
    result = cplat_vscanf(format, args);
    va_end(args);

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_vscanf(const char *format, va_list args)
{
#if defined(COMPILER_MSVC)
    #pragma warning(suppress : 4996)
#endif /* COMPILER_MSVC */
    return vscanf(format, args);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_fscanf(FILE *stream, const char *format, ...)
{
    va_list args;
    int result;

    va_start(args, format);
    result = cplat_vfscanf(stream, format, args);
    va_end(args);

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_vfscanf(FILE *stream, const char *format, va_list args)
{
#if defined(COMPILER_MSVC)
    #pragma warning(suppress : 4996)
#endif /* COMPILER_MSVC */
    return vfscanf(stream, format, args);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_fprintf(FILE *stream, const char *format, ...)
{
    int result;
    va_list args;

    va_start(args, format);
    result = cplat_vfprintf(stream, format, args);
    va_end(args);

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_vfprintf(FILE *stream, const char *format, va_list args)
{
#if defined(PLATFORM_WINDOWS)
    return vfprintf_s(stream, format, args);
#else
    return vfprintf(stream, format, args);
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_fseek(FILE *stream, const int64_t offset, const int whence)
{
#if defined(PLATFORM_LINUX)
    return fseeko(stream, (off_t)offset, whence);
#elif defined(PLATFORM_WINDOWS)
    return _fseeki64(stream, (__int64)offset, whence);
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

int64_t cplat_ftell(FILE *stream)
{
#if defined(PLATFORM_LINUX)
    return (int64_t)ftello(stream);
#elif defined(PLATFORM_WINDOWS)
    return (int64_t)_ftelli64(stream);
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_snprintf(char *dest, const size_t dest_size, const char *format, ...)
{
    va_list args;
    int result;

    va_start(args, format);
    result = cplat_vsnprintf(dest, dest_size, format, args);
    va_end(args);

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_vsnprintf(char *dest, const size_t dest_size, const char *format, va_list args)
{
    int needed;

    if (dest == NULL || dest_size == 0 || format == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    /* MSVC の UCRT では vsnprintf が C99 準拠であり、切り詰め時も必要文字数を返す。
     * see: https://learn.microsoft.com/cpp/c-runtime-library/reference/vsnprintf-vsnprintf-vsnprintf-l-vsnwprintf-vsnwprintf-l */
    needed = vsnprintf(dest, dest_size, format, args); /* 置換対象外: ラッパーの実体 */

    if (needed < 0)
    {
        dest[0] = '\0';
        return CPLAT_ERR_UNKNOWN;
    }

    /* vsnprintf の戻り値は終端を除いて必要だった文字数であり、書き込んだ文字数ではない。
     * 宛先の容量以上であれば切り詰めが起きている。 */
    if ((size_t)needed >= dest_size)
    {
        dest[0] = '\0';
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_fgets(char *dest, const size_t dest_size, FILE *stream, cplat_error *detail_out)
{
    size_t len;

    if (dest == NULL || dest_size == 0 || stream == NULL || dest_size > (size_t)INT_MAX)
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    dest[0] = '\0';
    errno = 0;
    if (fgets(dest, (int)dest_size, stream) == NULL)
    {
        if (ferror(stream) != 0)
        {
            /* NOTE: Windows UCRT では NUL の書き込み専用ストリームを fgets で読み取ると、
             * ストリームのエラー指示は ferror に反映されても errno が 0 の場合がある。
             * ferror はストリームのエラー指示を返すが、errno はシステム呼び出しのエラー値と
             * 常に一致するとは限らないため、errno == 0 のストリーム エラーを EIO で表す。
             * see: https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/ferror?view=msvc-170
             * see: https://learn.microsoft.com/en-us/cpp/c-runtime-library/errno-doserrno-sys-errlist-and-sys-nerr?view=msvc-170 */
            int errno_value = errno;

            if (errno_value == 0)
            {
                errno_value = EIO;
            }

            return cplat_internal_error_report_errno(detail_out, errno_value);
        }

        /* EOF は OS 呼び出し由来の詳細を持たないため、詳細をクリアして結果コードだけで表す */
        (void)cplat_internal_error_report_success(detail_out);
        return CPLAT_ERR_EOF;
    }

    len = strlen(dest);

    /* 宛先を使い切っており、かつ改行で終わっていない場合は、行の途中で打ち切られている。
     * ファイル末尾が改行なしで終わる場合は EOF フラグが立つため、切り詰めと区別できる。
     * dest_size が 1 のときは len が 0 になり 1 文字も格納できないため、常に切り詰めとして扱う。 */
    if (len + 1 == dest_size && (len == 0 || dest[len - 1] != '\n') && feof(stream) == 0)
    {
        dest[0] = '\0';
        (void)cplat_internal_error_report_success(detail_out);
        return CPLAT_ERR_BUFFER_TOO_SMALL;
    }

    if (len > 0 && dest[len - 1] == '\n')
    {
        len--;
        dest[len] = '\0';
    }
    if (len > 0 && dest[len - 1] == '\r')
    {
        len--;
        dest[len] = '\0';
    }

    return cplat_internal_error_report_success(detail_out);
}
