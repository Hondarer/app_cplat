/**
 *******************************************************************************
 *  @file           path_identity.c
 *  @brief          2 つのパスが同じファイル システムの実体を指すかを判定する API を実装します。
 *
 *  パスの文字列ではなく、OS のファイルの識別子で比べます。\n
 *  Linux は stat() の st_dev と st_ino、Windows は GetFileInformationByHandleEx() の
 *  FileIdInfo (ボリュームのシリアル番号と 128 ビットのファイル識別子) を使います。
 *
 *******************************************************************************
 */

#include <cplat/crt/path.h>
#include <cplat/base/error_internal.h>
#include <cplat/base/result.h>

#include <errno.h>
#include <string.h>

#if defined(PLATFORM_LINUX)
    #include <sys/stat.h>
    #include <sys/types.h>
#elif defined(PLATFORM_WINDOWS)
    #include <cplat/base/windows_sdk.h>
    #include <cplat/crt/wchar_conv.h>
    #include <cplat/win32/wchar_conv_internal.h>
#endif /* PLATFORM_ */

/* 1 つのパスが指す実体の識別子。 */
typedef struct path_identity
{
#if defined(PLATFORM_LINUX)
    dev_t device; /* 実体を含むデバイスの番号 (st_dev)。 */
    ino_t inode;  /* デバイス内の実体の番号 (st_ino)。 */
#elif defined(PLATFORM_WINDOWS)
    FILE_ID_INFO info; /* ボリュームのシリアル番号と 128 ビットのファイル識別子。 */
#endif /* PLATFORM_ */
} path_identity;

static int query_path_identity(const char *path, path_identity *identity_out, int *exists_out, cplat_error *detail_out);
static int path_identity_equal(const path_identity *lhs, const path_identity *rhs);
#if defined(PLATFORM_LINUX)
static int report_query_errno(cplat_error *detail_out, int errno_value);
#elif defined(PLATFORM_WINDOWS)
static int report_query_windows_error(cplat_error *detail_out, DWORD error_code);
#endif /* PLATFORM_ */
static int is_reported_query_result(int result);

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_path_is_same_file(const char *lhs, const char *rhs, int *same_out, cplat_error *detail_out)
{
    path_identity lhs_identity;
    path_identity rhs_identity;
    int lhs_exists = 0;
    int rhs_exists = 0;
    int ret;

    if (lhs == NULL || rhs == NULL || same_out == NULL || lhs[0] == '\0' || rhs[0] == '\0')
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    ret = query_path_identity(lhs, &lhs_identity, &lhs_exists, detail_out);
    if (ret != CPLAT_OK)
    {
        return ret;
    }

    /* 片方が存在しなければ、もう一方を調べるまでもなく異なる実体である。 */
    if (lhs_exists == 0)
    {
        *same_out = 0;
        return cplat_internal_error_report_success(detail_out);
    }

    ret = query_path_identity(rhs, &rhs_identity, &rhs_exists, detail_out);
    if (ret != CPLAT_OK)
    {
        return ret;
    }

    if (rhs_exists == 0)
    {
        *same_out = 0;
        return cplat_internal_error_report_success(detail_out);
    }

    *same_out = path_identity_equal(&lhs_identity, &rhs_identity);
    return cplat_internal_error_report_success(detail_out);
}

/*
 * パスが指す実体の識別子を取得する。シンボリック リンクは解決する。
 * 実体があれば *exists_out に 1 を格納し、*identity_out に識別子を格納して CPLAT_OK を返す。
 * パスが存在しなければ *exists_out に 0 を格納して CPLAT_OK を返す。
 * それ以外の失敗は、結果コードを返し、OS のエラー値を detail_out に記録する。
 */
static int query_path_identity(const char *path, path_identity *identity_out, int *exists_out, cplat_error *detail_out)
{
#if defined(PLATFORM_LINUX)
    struct stat st;

    if (stat(path, &st) != 0)
    {
        const int errno_value = errno;

        /* 途中の要素がディレクトリでない場合 (ENOTDIR) も、そのパスの実体はない。 */
        if (errno_value == ENOENT || errno_value == ENOTDIR)
        {
            *exists_out = 0;
            return CPLAT_OK;
        }

        return report_query_errno(detail_out, errno_value);
    }

    identity_out->device = st.st_dev;
    identity_out->inode = st.st_ino;
    *exists_out = 1;
    return CPLAT_OK;
#elif defined(PLATFORM_WINDOWS)
    wchar_t wpath[PLATFORM_PATH_MAX];
    HANDLE handle;
    BOOL got_info;
    int rc;

    rc = cplat_utf8_to_wpath(wpath, sizeof(wpath) / sizeof(wpath[0]), NULL, path);
    if (rc != CPLAT_OK)
    {
        return cplat_internal_error_report_errno(detail_out, cplat_internal_errno_from_conv_result(rc, 1));
    }

    /*
     * アクセス権 0 は、属性の照会だけを行い、ファイルの内容を開かない。すべての共有モードを指定し、
     * 他プロセスが開いているファイルも妨げずに照会する。FILE_FLAG_BACKUP_SEMANTICS はディレクトリを開くために要る。
     * FILE_FLAG_OPEN_REPARSE_POINT を指定しないため、シンボリック リンクはリンク先の実体を開く。
     * see: https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew
     */
    handle = CreateFileW(wpath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                         FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (handle == INVALID_HANDLE_VALUE)
    {
        const DWORD error_code = GetLastError();

        if (error_code == ERROR_FILE_NOT_FOUND || error_code == ERROR_PATH_NOT_FOUND)
        {
            *exists_out = 0;
            return CPLAT_OK;
        }

        return report_query_windows_error(detail_out, error_code);
    }

    /*
     * FileIdInfo は、ReFS の 128 ビットのファイル識別子を切り詰めずに返す。
     * GetFileInformationByHandle() の nFileIndexHigh と nFileIndexLow (64 ビット) は、ReFS で一意とは限らない。
     * see: https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_id_info
     */
    got_info = GetFileInformationByHandleEx(handle, FileIdInfo, &identity_out->info, (DWORD)sizeof(identity_out->info));
    if (!got_info)
    {
        const DWORD error_code = GetLastError();

        (void)CloseHandle(handle); /* 識別子の取得の失敗を返す経路の後始末のため、戻り値は明示的に破棄する */
        return report_query_windows_error(detail_out, error_code);
    }

    if (!CloseHandle(handle))
    {
        return report_query_windows_error(detail_out, GetLastError());
    }

    *exists_out = 1;
    return CPLAT_OK;
#endif /* PLATFORM_ */
}

/*
 * 2 つの識別子が同じ実体を表すかを判定する。同じなら 1、異なれば 0 を返す。
 */
static int path_identity_equal(const path_identity *lhs, const path_identity *rhs)
{
#if defined(PLATFORM_LINUX)
    return (lhs->device == rhs->device && lhs->inode == rhs->inode) ? 1 : 0;
#elif defined(PLATFORM_WINDOWS)
    if (lhs->info.VolumeSerialNumber != rhs->info.VolumeSerialNumber)
    {
        return 0;
    }

    if (memcmp(lhs->info.FileId.Identifier, rhs->info.FileId.Identifier, sizeof(lhs->info.FileId.Identifier)) != 0)
    {
        return 0;
    }

    return 1;
#endif /* PLATFORM_ */
}

#if defined(PLATFORM_LINUX)
/*
 * stat() の失敗の errno を記録し、公開ヘッダーに記載した結果コードへ分類して返す。
 * 権限の拒否、パスの長さの超過、メモリ不足以外は CPLAT_ERR_UNKNOWN とし、errno は詳細に残す。
 */
static int report_query_errno(cplat_error *detail_out, const int errno_value)
{
    const int result = cplat_internal_error_report_errno(detail_out, errno_value);

    if (is_reported_query_result(result))
    {
        return result;
    }

    return cplat_internal_error_report_errno_as(detail_out, errno_value, CPLAT_ERR_UNKNOWN);
}
#elif defined(PLATFORM_WINDOWS)
/*
 * CreateFileW()、GetFileInformationByHandleEx()、CloseHandle() の失敗の Win32 エラーを記録し、
 * 公開ヘッダーに記載した結果コードへ分類して返す。
 * 権限の拒否、パスの長さの超過、メモリ不足以外は CPLAT_ERR_UNKNOWN とし、Win32 エラーは詳細に残す。
 */
static int report_query_windows_error(cplat_error *detail_out, const DWORD error_code)
{
    const int result = cplat_internal_error_report_windows_error(detail_out, (unsigned long)error_code);

    if (is_reported_query_result(result))
    {
        return result;
    }

    return cplat_internal_error_report_windows_error_as(detail_out, (unsigned long)error_code, CPLAT_ERR_UNKNOWN);
}
#endif /* PLATFORM_ */

/*
 * OS の照会の失敗の結果コードのうち、そのまま返すものかを判定する。返すものなら 0 以外を返す。
 */
static int is_reported_query_result(const int result)
{
    return result == CPLAT_ERR_PERMISSION_DENIED || result == CPLAT_ERR_BUFFER_TOO_SMALL ||
           result == CPLAT_ERR_OUT_OF_MEMORY;
}
