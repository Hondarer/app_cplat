/**
 *******************************************************************************
 *  @file           mmap_windows.c
 *  @brief          Windows 向けのメモリ マップド ファイルを実装します。
 *******************************************************************************
 */

#include <cplat/base/platform.h>
#include <cplat/crt/stdlib.h>

#if defined(PLATFORM_WINDOWS)

    #include <errno.h>
    #include <stdlib.h>

    #include <cplat/base/windows_sdk.h>
    #include <cplat/crt/file.h>
    #include <cplat/crt/stdio.h>
    #include <cplat/base/error_internal.h>
    #include <cplat/mmap/mmap.h>

struct cplat_mmap
{
    void *address;
    size_t size;
    cplat_file file;
    HANDLE mapping_handle;
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
    DWORD protect;
    DWORD map_access;
    LARGE_INTEGER size_li;
    void *address;

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
        protect = PAGE_READWRITE;
    }
    else
    {
        protect = PAGE_READONLY;
    }
    size_li.QuadPart = (LONGLONG)size;
    new_map->mapping_handle =
        CreateFileMappingA(new_map->file.handle, NULL, protect, (DWORD)size_li.HighPart, (DWORD)size_li.LowPart, NULL);
    if (new_map->mapping_handle == NULL)
    {
        const DWORD error_code = GetLastError();

        (void)cplat_file_close(&new_map->file, NULL);
        cplat_free(new_map);
        return cplat_internal_error_report_windows_error(detail_out, error_code);
    }

    if (access == CPLAT_MMAP_ACCESS_READ_WRITE)
    {
        map_access = FILE_MAP_WRITE;
    }
    else
    {
        map_access = FILE_MAP_READ;
    }
    address = MapViewOfFile(new_map->mapping_handle, map_access, 0, 0, size);
    if (address == NULL)
    {
        const DWORD error_code = GetLastError();

        CloseHandle(new_map->mapping_handle);
        (void)cplat_file_close(&new_map->file, NULL);
        cplat_free(new_map);
        return cplat_internal_error_report_windows_error(detail_out, error_code);
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

    if (!FlushViewOfFile(target, target_len))
    {
        return cplat_internal_error_report_windows_error(detail_out, GetLastError());
    }
    if (!FlushFileBuffers(map->file.handle))
    {
        return cplat_internal_error_report_windows_error(detail_out, GetLastError());
    }
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_mmap_detach(cplat_mmap *map, cplat_error *detail_out)
{
    DWORD first_error = ERROR_SUCCESS;
    int close_result;

    if (map == NULL)
    {
        return cplat_internal_error_report_success(detail_out);
    }
    if (!UnmapViewOfFile(map->address))
    {
        first_error = GetLastError();
    }
    if (!CloseHandle(map->mapping_handle) && first_error == ERROR_SUCCESS)
    {
        first_error = GetLastError();
    }
    close_result = cplat_file_close(&map->file, detail_out);
    cplat_free(map);
    if (first_error != ERROR_SUCCESS)
    {
        return cplat_internal_error_report_windows_error(detail_out, first_error);
    }
    if (close_result != CPLAT_OK)
    {
        return close_result;
    }
    return cplat_internal_error_report_success(detail_out);
}

#endif /* PLATFORM_WINDOWS */
