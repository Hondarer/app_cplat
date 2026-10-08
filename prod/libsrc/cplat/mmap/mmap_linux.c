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

static int open_backing_file(const char *path, cplat_mmap_access access, size_t create_size, cplat_file *file,
                             size_t *size_out, cplat_error *detail_out)
{
    int open_result;

    cplat_file_init(file);

    if (access == CPLAT_MMAP_ACCESS_READ_ONLY)
    {
        open_result = cplat_file_open(file, path, CPLAT_FILE_OPEN_READ, detail_out);
        if (open_result != CPLAT_OK)
        {
            return open_result;
        }
        open_result = cplat_file_get_size(file, size_out, detail_out);
        if (open_result != CPLAT_OK)
        {
            (void)cplat_file_close(file, NULL);
            return open_result;
        }
        return cplat_internal_error_report_success(detail_out);
    }

    /* 新規作成のみ許可するオープンをまず試みる。成功すれば新規作成と判定できる。 */
    open_result = cplat_file_open(
        file, path, CPLAT_FILE_OPEN_READ | CPLAT_FILE_OPEN_WRITE | CPLAT_FILE_OPEN_CREATE | CPLAT_FILE_OPEN_CREATE_NEW,
        detail_out);
    if (open_result == 0)
    {
        if (create_size == 0)
        {
            (void)cplat_file_close(file, NULL);
            (void)cplat_remove(path, NULL);
            return cplat_internal_error_report_errno(detail_out, EINVAL);
        }
        open_result = cplat_file_set_size(file, create_size, detail_out);
        if (open_result != CPLAT_OK)
        {
            (void)cplat_file_close(file, NULL);
            (void)cplat_remove(path, NULL);
            return open_result;
        }
        *size_out = create_size;
        return cplat_internal_error_report_success(detail_out);
    }

    /* 新規作成に失敗した場合は既存ファイルとみなし、CREATE_NEW を外して再オープンする。 */
    open_result = cplat_file_open(file, path, CPLAT_FILE_OPEN_READ | CPLAT_FILE_OPEN_WRITE, detail_out);
    if (open_result != CPLAT_OK)
    {
        return open_result;
    }
    open_result = cplat_file_get_size(file, size_out, detail_out);
    if (open_result != CPLAT_OK)
    {
        (void)cplat_file_close(file, NULL);
        return open_result;
    }
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_mmap_attach(const char *path, cplat_mmap_access access, size_t create_size, cplat_mmap **map,
                      cplat_error *detail_out)
{
    cplat_mmap *new_map;
    int result;
    size_t size = 0;
    void *address;
    int prot;

    if (path == NULL || path[0] == '\0' || map == NULL ||
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

    if (size == 0)
    {
        (void)cplat_file_close(&new_map->file, NULL);
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
    address = mmap(NULL, size, prot, MAP_SHARED, new_map->file.handle, 0);
    if (address == MAP_FAILED)
    {
        const int errno_value = errno;

        (void)cplat_file_close(&new_map->file, NULL);
        cplat_free(new_map);
        return cplat_internal_error_report_errno(detail_out, errno_value);
    }

    new_map->address = address;
    new_map->size = size;
    *map = new_map;
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
    #pragma warning(disable : 4206)
#endif
