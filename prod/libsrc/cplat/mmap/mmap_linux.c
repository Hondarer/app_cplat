/**
 *******************************************************************************
 *  @file           mmap_linux.c
 *  @brief          Linux 向けのメモリ マップド ファイルを実装します。
 *******************************************************************************
 */

#include <cplat/base/platform.h>
#include <cplat/crt/stdlib.h>

#if defined(PLATFORM_LINUX)

    #include <errno.h>
    #include <stdlib.h>
    #include <sys/mman.h>

    #include <cplat/crt/file.h>
    #include <cplat/crt/stdio.h>
    #include <cplat/base/error_internal.h>
    #include <cplat/mmap/mmap.h>

struct cplat_mmap
{
    void *address;
    size_t size;
    cplat_file file;
};

/*
 * 対応付けの対象ファイルを開き、対応付ける長さを size_out へ格納する。
 * 読み取り専用では既存ファイルを開き、その長さを格納する。
 * 読み書きでは、ファイルがなければ create_size の長さで作成し、あれば現在の長さを格納する。
 * 成功時は CPLAT_OK を返す。失敗時は共通結果コードを返す。
 * 新規作成のあと長さを確定できない場合は、作成したファイルを削除してから、その失敗を返す。
 * 長さ 0 は対応付けの対象外である。正本は app/cplat/docs/functional-spec/mmap.md の「適用範囲外」節である。
 */
static int open_backing_file(const char *path, cplat_mmap_access access, size_t create_size, cplat_file *file,
                             size_t *size_out, cplat_error *detail_out)
{
    int ret;

    /*
     * cplat_file_open() は開く前に既存のハンドルを閉じる。
     * ハンドル 0 は未オープンではないため、先に無効値へ初期化する。
     */
    cplat_file_init(file);

    if (access == CPLAT_MMAP_ACCESS_READ_ONLY)
    {
        ret = cplat_file_open(file, path, CPLAT_FILE_OPEN_READ, detail_out);
        if (ret != CPLAT_OK)
        {
            return ret;
        }

        ret = cplat_file_get_size(file, size_out, detail_out);
        if (ret != CPLAT_OK)
        {
            (void)cplat_file_close(file, NULL); /* サイズ取得の失敗を返すため、クローズの結果は使わない */
            return ret;
        }

        return cplat_internal_error_report_success(detail_out);
    }

    /* 新規作成のみ許可するオープンをまず試みる。成功すれば新規作成と判定できる。 */
    ret = cplat_file_open(
        file, path, CPLAT_FILE_OPEN_READ | CPLAT_FILE_OPEN_WRITE | CPLAT_FILE_OPEN_CREATE | CPLAT_FILE_OPEN_CREATE_NEW,
        detail_out);
    if (ret == CPLAT_OK)
    {
        if (create_size == 0)
        {
            /* 閉じる処理と削除の戻り値は、長さ 0 の失敗を返すため使わない。 */
            (void)cplat_file_close(file, NULL);
            (void)cplat_remove(path, NULL);
            return cplat_internal_error_report_errno(detail_out, EINVAL);
        }

        ret = cplat_file_set_size(file, create_size, detail_out);
        if (ret != CPLAT_OK)
        {
            /* 閉じる処理と削除の戻り値は、サイズ設定の失敗を返すため使わない。 */
            (void)cplat_file_close(file, NULL);
            (void)cplat_remove(path, NULL);
            return ret;
        }

        *size_out = create_size;
        return cplat_internal_error_report_success(detail_out);
    }

    /* 新規作成に失敗した場合は既存ファイルとみなし、CREATE_NEW を外して再オープンする。 */
    ret = cplat_file_open(file, path, CPLAT_FILE_OPEN_READ | CPLAT_FILE_OPEN_WRITE, detail_out);
    if (ret != CPLAT_OK)
    {
        return ret;
    }

    ret = cplat_file_get_size(file, size_out, detail_out);
    if (ret != CPLAT_OK)
    {
        (void)cplat_file_close(file, NULL); /* サイズ取得の失敗を返すため、クローズの結果は使わない */
        return ret;
    }

    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_mmap_attach(const char *path, cplat_mmap_access access, size_t create_size, cplat_mmap **map_out,
                      cplat_error *detail_out)
{
    cplat_mmap *new_map;
    int result;
    size_t size = 0;
    void *address;
    int prot;

    if (path == NULL || path[0] == '\0' || map_out == NULL ||
        (access != CPLAT_MMAP_ACCESS_READ_ONLY && access != CPLAT_MMAP_ACCESS_READ_WRITE))
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    new_map = (cplat_mmap *)cplat_calloc(1, sizeof(*new_map));
    if (new_map == NULL)
    {
        return cplat_internal_error_report_errno(detail_out, ENOMEM);
    }

    result = open_backing_file(path, access, create_size, &new_map->file, &size, detail_out);
    if (result != CPLAT_OK)
    {
        cplat_free(new_map);
        return result;
    }

    /*
     * mmap() は長さ 0 の対応付けを受け付けない。
     * see: https://man7.org/linux/man-pages/man2/mmap.2.html
     */
    if (size == 0)
    {
        (void)cplat_file_close(&new_map->file, NULL); /* 長さ 0 の失敗を返すため、クローズの結果は使わない */
        cplat_free(new_map);
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    if (access == CPLAT_MMAP_ACCESS_READ_WRITE)
    {
        prot = PROT_READ | PROT_WRITE;
    }
    else
    {
        prot = PROT_READ;
    }

    /*
     * 対応付けた変更をファイルへ書き戻すため、MAP_SHARED で対応付ける。
     * see: https://man7.org/linux/man-pages/man2/mmap.2.html
     */
    address = mmap(NULL, size, prot, MAP_SHARED, new_map->file.handle, 0);
    if (address == MAP_FAILED)
    {
        /*
         * mmap() の errno は、後続のクローズが errno を変える前に保持する。
         * クローズの戻り値は、mmap() の失敗を返すため使わない。
         * see: https://man7.org/linux/man-pages/man3/errno.3.html
         */
        const int errno_value = errno;

        (void)cplat_file_close(&new_map->file, NULL);
        cplat_free(new_map);
        return cplat_internal_error_report_errno(detail_out, errno_value);
    }

    new_map->address = address;
    new_map->size = size;
    *map_out = new_map;

    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

void *cplat_mmap_get_address(const cplat_mmap *map)
{
    if (map == NULL)
    {
        return NULL;
    }

    return map->address;
}

/* Doxygen コメントは、ヘッダーに記載 */

size_t cplat_mmap_get_size(const cplat_mmap *map)
{
    if (map == NULL)
    {
        return 0;
    }

    return map->size;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_mmap_flush(cplat_mmap *map, void *address, size_t length, cplat_error *detail_out)
{
    void *target;
    size_t target_len;

    if (map == NULL)
    {
        return cplat_internal_error_report_errno(detail_out, EINVAL);
    }

    if (address == NULL)
    {
        target = map->address;
        target_len = map->size;
    }
    else
    {
        target = address;
        target_len = length;
    }

    if (msync(target, target_len, MS_SYNC) != 0)
    {
        return cplat_internal_error_report_errno(detail_out, errno);
    }

    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_mmap_detach(cplat_mmap *map, cplat_error *detail_out)
{
    int unmap_result;
    int unmap_errno = 0;
    int close_result;

    if (map == NULL)
    {
        return cplat_internal_error_report_success(detail_out);
    }

    /*
     * 解除に失敗してもファイルは閉じ、ハンドルは解放する。
     * munmap() の errno は、後続のクローズが errno を変える前に保持し、クローズの失敗より先に返す。
     * see: https://man7.org/linux/man-pages/man3/errno.3.html
     */
    unmap_result = munmap(map->address, map->size);
    if (unmap_result != 0)
    {
        unmap_errno = errno;
    }

    close_result = cplat_file_close(&map->file, detail_out);
    cplat_free(map);

    if (unmap_result != 0)
    {
        return cplat_internal_error_report_errno(detail_out, unmap_errno);
    }

    if (close_result != CPLAT_OK)
    {
        return close_result;
    }

    return cplat_internal_error_report_success(detail_out);
}

#elif defined(PLATFORM_WINDOWS) && defined(COMPILER_MSVC)
    /*
     * MSVC は空の翻訳単位を C4206 として警告する。
     * 正本は app/cplat/docs/platform-abstraction-guideline.md の「差異が大きい実装はファイル分割する」節である。
     */
    #pragma warning(disable : 4206)
#endif /* PLATFORM_ */
