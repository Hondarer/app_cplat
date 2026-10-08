/*
 *  glibc は struct stat の st_mtim と、utimensat および AT_FDCWD を __USE_XOPEN2K8 の
 *  内側に置く。makefw の既定である -std=c17 (厳密 ISO) では __STRICT_ANSI__ が定義され、
 *  _DEFAULT_SOURCE が自動定義されないため、これらが宣言されない。
 *  サブ秒の最終更新日時を扱うために本ファイルでのみ機能テスト マクロを定義する。
 *  see: prod/libsrc/cplat/runtime/memory_lock.c の同種の定義
 */
#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif /* _GNU_SOURCE */

/**
 *******************************************************************************
 *  @file           file_timestamp.c
 *  @brief          ファイルの最終更新日時を取得および設定する API を実装します。
 *
 *  ハンドルを対象とする版とパスを対象とする版を、OS 非依存のインターフェースで提供します。
 *
 *******************************************************************************
 */

#include <cplat/crt/file.h>
#include <cplat/crt/path.h>

#include <cplat/base/error_internal.h>
#include <cplat/base/result.h>
#include <cplat/clock/timespec.h>

#include <errno.h>
#include <stddef.h>

#if defined(PLATFORM_LINUX)
    #include <fcntl.h>
    #include <sys/stat.h>
    #include <time.h>
#elif defined(PLATFORM_WINDOWS)
    #include <cplat/clock/filetime_conv.h>
    #include <cplat/win32/win32.h>

    #include "crt.h"
#endif /* PLATFORM_ */

static int file_is_open(const cplat_file *file)
{
    if (file == NULL)
    {
        return 0;
    }

#if defined(PLATFORM_LINUX)
    return file->handle != -1;
#elif defined(PLATFORM_WINDOWS)
    return file->handle != INVALID_HANDLE_VALUE;
#endif /* PLATFORM_ */
}

#if defined(PLATFORM_LINUX)

/*
 *  utimensat と futimens へ渡す時刻の組を組み立てる。
 *  最終アクセス日時は UTIME_OMIT で据え置き、最終更新日時だけを設定する。
 */
static void build_times(const cplat_timespec *timestamp, struct timespec *times)
{
    times[0].tv_sec = 0;
    times[0].tv_nsec = UTIME_OMIT;
    cplat_timespec_to_native(timestamp, &times[1]);
}

/* struct stat の最終更新日時を cplat_timespec へ写す。 */
static void stat_to_timestamp(const struct stat *file_stat, cplat_timespec *timestamp_out)
{
    cplat_timespec_from_native(&file_stat->st_mtim, timestamp_out);
}

#elif defined(PLATFORM_WINDOWS)

/*
 *  属性アクセスだけを要求してパスを一時的に開く。
 *  他プロセスの読み書きと削除を妨げない共有モードを指定する。
 */
static HANDLE open_for_attributes(const char *path, DWORD desired_access, cplat_error *detail_out, int *failed_out)
{
    HANDLE handle;

    *failed_out = crt_check_wpath_length(path, detail_out);
    if (*failed_out != CPLAT_OK)
    {
        return INVALID_HANDLE_VALUE;
    }

    handle = CreateFileU(path, desired_access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == INVALID_HANDLE_VALUE)
    {
        *failed_out = cplat_internal_error_report_windows_error(detail_out, GetLastError());
    }

    return handle;
}

#endif /* PLATFORM_ */

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_file_get_modified_timestamp(const cplat_file *file, cplat_timespec *timestamp_out, cplat_error *detail_out)
{
    if (!file_is_open(file) || timestamp_out == NULL)
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

#if defined(PLATFORM_LINUX)
    {
        struct stat st;

        if (fstat(file->handle, &st) != 0)
        {
            return cplat_internal_error_report_errno(detail_out, errno);
        }

        stat_to_timestamp(&st, timestamp_out);
        return cplat_internal_error_report_success(detail_out);
    }
#elif defined(PLATFORM_WINDOWS)
    {
        FILETIME write_time;

        if (!GetFileTime(file->handle, NULL, NULL, &write_time))
        {
            return cplat_internal_error_report_windows_error(detail_out, GetLastError());
        }

        cplat_internal_filetime_to_timespec(&write_time, timestamp_out);
        return cplat_internal_error_report_success(detail_out);
    }
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_file_set_modified_timestamp(cplat_file *file, const cplat_timespec *timestamp, cplat_error *detail_out)
{
    if (!file_is_open(file) || timestamp == NULL)
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    /*
     *  両プラットフォームで挙動をそろえるため、書き込みアクセスを要求する。
     *  Linux の futimens は読み取り専用の記述子でも所有者なら成功するため、
     *  厳しい側の Windows に合わせてここで失敗させる。
     */
    if (file->writable == 0)
    {
        return cplat_internal_error_report_errno_as(detail_out, EACCES, CPLAT_ERR_PERMISSION_DENIED);
    }

#if defined(PLATFORM_LINUX)
    {
        struct timespec times[2];

        build_times(timestamp, times);

        if (futimens(file->handle, times) != 0)
        {
            return cplat_internal_error_report_errno(detail_out, errno);
        }

        return cplat_internal_error_report_success(detail_out);
    }
#elif defined(PLATFORM_WINDOWS)
    {
        FILETIME write_time;

        cplat_internal_timespec_to_filetime(timestamp, &write_time);

        if (!SetFileTime(file->handle, NULL, NULL, &write_time))
        {
            return cplat_internal_error_report_windows_error(detail_out, GetLastError());
        }

        return cplat_internal_error_report_success(detail_out);
    }
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_file_get_path_modified_timestamp(const char *path, cplat_timespec *timestamp_out, cplat_error *detail_out)
{
    if (path == NULL || timestamp_out == NULL)
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

#if defined(PLATFORM_LINUX)
    {
        struct stat st;

        if (stat(path, &st) != 0)
        {
            return cplat_internal_error_report_errno(detail_out, errno);
        }

        stat_to_timestamp(&st, timestamp_out);
        return cplat_internal_error_report_success(detail_out);
    }
#elif defined(PLATFORM_WINDOWS)
    {
        HANDLE handle;
        FILETIME write_time;
        BOOL got_time;
        int failed = 0;

        handle = open_for_attributes(path, FILE_READ_ATTRIBUTES, detail_out, &failed);
        if (handle == INVALID_HANDLE_VALUE)
        {
            return failed;
        }

        got_time = GetFileTime(handle, NULL, NULL, &write_time);
        if (!got_time)
        {
            const DWORD error_code = GetLastError();

            (void)CloseHandle(handle);
            return cplat_internal_error_report_windows_error(detail_out, error_code);
        }
        if (!CloseHandle(handle))
        {
            return cplat_internal_error_report_windows_error(detail_out, GetLastError());
        }

        cplat_internal_filetime_to_timespec(&write_time, timestamp_out);
        return cplat_internal_error_report_success(detail_out);
    }
#endif /* PLATFORM_ */
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_file_set_path_modified_timestamp(const char *path, const cplat_timespec *timestamp, cplat_error *detail_out)
{
    if (path == NULL || timestamp == NULL)
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

#if defined(PLATFORM_LINUX)
    {
        struct timespec times[2];

        build_times(timestamp, times);

        if (utimensat(AT_FDCWD, path, times, 0) != 0)
        {
            return cplat_internal_error_report_errno(detail_out, errno);
        }

        return cplat_internal_error_report_success(detail_out);
    }
#elif defined(PLATFORM_WINDOWS)
    {
        HANDLE handle;
        FILETIME write_time;
        BOOL set_time;
        int failed = 0;

        handle = open_for_attributes(path, FILE_WRITE_ATTRIBUTES, detail_out, &failed);
        if (handle == INVALID_HANDLE_VALUE)
        {
            return failed;
        }

        cplat_internal_timespec_to_filetime(timestamp, &write_time);

        set_time = SetFileTime(handle, NULL, NULL, &write_time);
        if (!set_time)
        {
            const DWORD error_code = GetLastError();

            (void)CloseHandle(handle);
            return cplat_internal_error_report_windows_error(detail_out, error_code);
        }
        if (!CloseHandle(handle))
        {
            return cplat_internal_error_report_windows_error(detail_out, GetLastError());
        }

        return cplat_internal_error_report_success(detail_out);
    }
#endif /* PLATFORM_ */
}
