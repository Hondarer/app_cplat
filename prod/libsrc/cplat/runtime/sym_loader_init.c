/**
 *******************************************************************************
 *  @file           sym_loader_init.c
 *  @brief          JSONC 設定ファイルから cplat_sym_loader_entry の移譲先を読み込みます。
 *  @author         c-modenization-kit sample team
 *  @date           2026/02/23
 *  @version        1.0.0
 *
 *  JSONC ファイルから func_key と、lib / func の組を読み込みます。\n
 *  値がオブジェクトなら組は 1 件、配列なら要素ごとに 1 件です。\n
 *  同じキーの後のオブジェクトまたは配列は、前の組を置き換えます。\n
 *
 *  ファイル未存在、読取失敗、JSONC 解析失敗は、エントリを失敗にしません。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/crt/stdio.h>
#include <cplat/crt/stdlib.h>
#include <cplat/crt/string.h>
#include <cplat/runtime/sym_loader.h>
#include <cplat/sync/atomic.h>
#include <cJSON_JSONC.h>

#include <stdint.h>
#include <string.h>

#include "sym_loader_internal.h"

/** 設定ファイル全体の最大バイト数 (NUL 終端を含まない)。 */
#define CONFIG_FILE_MAX ((int64_t)(1024 * 1024))

static void close_module_handle(CPLAT_MODULE_HANDLE handle)
{
    if (handle == NULL)
    {
        return;
    }

#if defined(PLATFORM_LINUX)
    dlclose(handle);
#elif defined(PLATFORM_WINDOWS)
    FreeLibrary(handle);
#endif /* PLATFORM_ */
}

static void free_name_pairs(cplat_sym_loader_name_pair *list, size_t count)
{
    size_t index;

    if (list == NULL)
    {
        return;
    }

    for (index = 0; index < count; index++)
    {
        cplat_free(list[index].lib_name);
        cplat_free(list[index].func_name);
    }

    cplat_free(list);
}

static void free_reject_items(cplat_sym_loader_reject_item *list, size_t count)
{
    size_t index;

    if (list == NULL)
    {
        return;
    }

    for (index = 0; index < count; index++)
    {
        cplat_free(list[index].lib_name);
        cplat_free(list[index].func_name);
    }

    cplat_free(list);
}

static void free_usable_items(cplat_sym_loader_usable *list, size_t count)
{
    size_t index;

    if (list == NULL)
    {
        return;
    }

    for (index = 0; index < count; index++)
    {
        close_module_handle(list[index].handle);
        cplat_free(list[index].lib_name);
        cplat_free(list[index].func_name);
    }

    cplat_free(list);
}

/* 解決済みのハンドルと、未公開の名前の列を捨てます。設定パスは残します。 */
static void release_loaded_lists(cplat_sym_loader_entry *entry)
{
    cplat_sym_loader_entry_impl *impl = entry->impl;

    if (impl == NULL)
    {
        return;
    }

    free_name_pairs(impl->pending, impl->pending_count);
    impl->pending = NULL;
    impl->pending_count = 0;
    free_reject_items(impl->duplicates, impl->duplicate_count);
    impl->duplicates = NULL;
    impl->duplicate_count = 0;
    free_usable_items(impl->usable, impl->usable_count);
    impl->usable = NULL;
    impl->usable_count = 0;
    free_reject_items(impl->rejects, impl->reject_count);
    impl->rejects = NULL;
    impl->reject_count = 0;
    impl->is_default = 0;
    cplat_atomic_store_i32(&entry->resolved, 0, CPLAT_MEMORY_ORDER_RELEASE);
}

static cplat_sym_loader_entry *find_entry(cplat_sym_loader_entry *const *fobj_array, size_t fobj_length,
                                          const char *func_key)
{
    size_t index;

    if (fobj_array == NULL)
    {
        return NULL;
    }

    for (index = 0; index < fobj_length; index++)
    {
        cplat_sym_loader_entry *cache = fobj_array[index];

        if ((cache == NULL) || (cache->func_key == NULL))
        {
            continue;
        }

        if (strcmp(cache->func_key, func_key) == 0)
        {
            return cache;
        }
    }

    return NULL;
}

static int ensure_impl(cplat_sym_loader_entry *entry, const char *configpath)
{
    int created = 0;

    if (entry->impl == NULL)
    {
        entry->impl = (cplat_sym_loader_entry_impl *)cplat_calloc(1u, sizeof(*entry->impl));
        if (entry->impl == NULL)
        {
            return -1;
        }

        created = 1;
    }

    if ((entry->impl->config_path == NULL) && (configpath != NULL) && (configpath[0] != '\0'))
    {
        char *path_copy = cplat_strdup(configpath);

        if (path_copy == NULL)
        {
            if (created != 0)
            {
                cplat_free(entry->impl);
                entry->impl = NULL;
            }

            return -1;
        }

        entry->impl->config_path = path_copy;
    }

    return 0;
}

static void mark_storage_failure(cplat_sym_loader_entry *entry, const char *configpath)
{
    cplat_atomic_store_i32(&entry->resolved, 0, CPLAT_MEMORY_ORDER_RELEASE);
    cplat_atomic_store_i32(&entry->failed, 1, CPLAT_MEMORY_ORDER_RELEASE);

    if (ensure_impl(entry, configpath) != 0)
    {
        /* パスを残せない既存の管理構造体は、空の成功として公開しない。 */
        if (entry->impl != NULL)
        {
            entry->impl->needs_reload = 1;
            entry->impl->failed_this_apply = 1;
        }

        return;
    }

    entry->impl->needs_reload = 1;
    entry->impl->failed_this_apply = 1;
}

static int append_name_pair(cplat_sym_loader_name_pair **list, size_t *count, const char *lib_name,
                            const char *func_name)
{
    cplat_sym_loader_name_pair *grown;
    char *lib_copy;
    char *func_copy;

    if (*count == SIZE_MAX)
    {
        return -1;
    }

    lib_copy = cplat_strdup(lib_name);
    if (lib_copy == NULL)
    {
        return -1;
    }

    func_copy = cplat_strdup(func_name);
    if (func_copy == NULL)
    {
        cplat_free(lib_copy);
        return -1;
    }

    grown = (cplat_sym_loader_name_pair *)cplat_realloc(*list, *count + 1u, sizeof(*grown));
    if (grown == NULL)
    {
        cplat_free(lib_copy);
        cplat_free(func_copy);
        return -1;
    }

    *list = grown;
    grown[*count].lib_name = lib_copy;
    grown[*count].func_name = func_copy;
    *count += 1u;
    return 0;
}

/* 文字列の組を診断の列へ移します。失敗したときは、元の文字列の所有権を戻しません。 */
static int move_name_to_duplicates(cplat_sym_loader_entry_impl *impl, char *lib_name, char *func_name)
{
    cplat_sym_loader_reject_item *grown;

    if (impl->duplicate_count == SIZE_MAX)
    {
        return -1;
    }

    grown = (cplat_sym_loader_reject_item *)cplat_realloc(impl->duplicates, impl->duplicate_count + 1u, sizeof(*grown));
    if (grown == NULL)
    {
        return -1;
    }

    impl->duplicates = grown;
    grown[impl->duplicate_count].lib_name = lib_name;
    grown[impl->duplicate_count].func_name = func_name;
    grown[impl->duplicate_count].reason = CPLAT_SYM_LOADER_REJECT_DUPLICATE_KEY;
    impl->duplicate_count += 1u;
    return 0;
}

static int move_pending_to_duplicates(cplat_sym_loader_entry_impl *impl)
{
    size_t index = 0;

    while (index < impl->pending_count)
    {
        char *lib_name = impl->pending[index].lib_name;
        char *func_name = impl->pending[index].func_name;

        if (move_name_to_duplicates(impl, lib_name, func_name) != 0)
        {
            return -1;
        }

        impl->pending[index].lib_name = NULL;
        impl->pending[index].func_name = NULL;
        index++;
    }

    cplat_free(impl->pending);
    impl->pending = NULL;
    impl->pending_count = 0;
    return 0;
}

/* list の所有権を受け取ります。失敗したときも解放します。 */
static void install_targets(cplat_sym_loader_entry *const *fobj_array, size_t fobj_length, const char *func_key,
                            cplat_sym_loader_name_pair *list, size_t count, const char *configpath)
{
    cplat_sym_loader_entry *entry = find_entry(fobj_array, fobj_length, func_key);

    if (entry == NULL)
    {
        free_name_pairs(list, count);
        return;
    }

    if ((entry->impl != NULL) && (entry->impl->failed_this_apply != 0))
    {
        free_name_pairs(list, count);
        return;
    }

    if (ensure_impl(entry, configpath) != 0)
    {
        free_name_pairs(list, count);
        mark_storage_failure(entry, configpath);
        return;
    }

    if (entry->impl->adopted_in_apply != 0)
    {
        if (move_pending_to_duplicates(entry->impl) != 0)
        {
            free_name_pairs(list, count);
            mark_storage_failure(entry, configpath);
            return;
        }
    }
    else
    {
        release_loaded_lists(entry);
    }

    entry->impl->pending = list;
    entry->impl->pending_count = count;
    entry->impl->adopted_in_apply = 1;
    entry->impl->needs_reload = 0;
    entry->impl->failed_this_apply = 0;
    entry->impl->is_default = 0;
    cplat_atomic_store_i32(&entry->failed, 0, CPLAT_MEMORY_ORDER_RELEASE);
    cplat_atomic_store_i32(&entry->resolved, 0, CPLAT_MEMORY_ORDER_RELEASE);
}

/*
 * オブジェクト 1 件を組として追加します。
 * 1 は追加、0 は文字列の組として読めない、 -1 は確保失敗です。
 * lib と func がどちらも文字列のときは、片方の取得が NULL でも両方を呼び出します。
 */
static int append_object_value(const cJSON *object, cplat_sym_loader_name_pair **list, size_t *count)
{
    const cJSON *lib_item;
    const cJSON *func_item;
    const char *lib_name;
    const char *func_name;

    lib_item = cJSON_GetObjectItemCaseSensitive(object, "lib");
    func_item = cJSON_GetObjectItemCaseSensitive(object, "func");
    if ((cJSON_IsString(lib_item) == 0) || (cJSON_IsString(func_item) == 0))
    {
        return 0;
    }

    lib_name = cJSON_GetStringValue(lib_item);
    func_name = cJSON_GetStringValue(func_item);
    if ((lib_name == NULL) || (func_name == NULL))
    {
        return 0;
    }

    if (append_name_pair(list, count, lib_name, func_name) != 0)
    {
        return -1;
    }

    return 1;
}

static int append_array_targets(const cJSON *array, cplat_sym_loader_name_pair **list, size_t *count)
{
    const cJSON *element;

    for (element = array->child; element != NULL; element = element->next)
    {
        int appended;

        if (cJSON_IsObject(element) == 0)
        {
            continue;
        }

        appended = append_object_value(element, list, count);
        if (appended < 0)
        {
            return -1;
        }
    }

    return 0;
}

static void mark_key_storage_failure(cplat_sym_loader_entry *const *fobj_array, size_t fobj_length,
                                     const char *func_key, const char *configpath)
{
    cplat_sym_loader_entry *entry = find_entry(fobj_array, fobj_length, func_key);

    if (entry == NULL)
    {
        return;
    }

    if ((entry->impl != NULL) && (entry->impl->failed_this_apply != 0))
    {
        return;
    }

    mark_storage_failure(entry, configpath);
}

static void handle_property(const cJSON *item, cplat_sym_loader_entry *const *fobj_array, size_t fobj_length,
                            const char *configpath)
{
    const char *func_key;
    cplat_sym_loader_name_pair *list = NULL;
    size_t count = 0;

    func_key = item->string;
    if ((func_key == NULL) || (func_key[0] == '\0'))
    {
        return;
    }

    /* オブジェクトを先に判定し、配列の既定戻り値 0 がオブジェクトの走査を止めないようにする。 */
    if (cJSON_IsObject(item) != 0)
    {
        if (append_object_value(item, &list, &count) < 0)
        {
            free_name_pairs(list, count);
            mark_key_storage_failure(fobj_array, fobj_length, func_key, configpath);
            return;
        }

        install_targets(fobj_array, fobj_length, func_key, list, count, configpath);
        return;
    }

    if (cJSON_IsArray(item) != 0)
    {
        if (append_array_targets(item, &list, &count) != 0)
        {
            free_name_pairs(list, count);
            mark_key_storage_failure(fobj_array, fobj_length, func_key, configpath);
            return;
        }

        install_targets(fobj_array, fobj_length, func_key, list, count, configpath);
    }
}

static void begin_apply(cplat_sym_loader_entry *const *fobj_array, size_t fobj_length)
{
    size_t index;

    if (fobj_array == NULL)
    {
        return;
    }

    for (index = 0; index < fobj_length; index++)
    {
        cplat_sym_loader_entry *entry = fobj_array[index];

        if ((entry == NULL) || (entry->impl == NULL))
        {
            continue;
        }

        entry->impl->adopted_in_apply = 0;
        entry->impl->failed_this_apply = 0;
    }
}

/* 読み直せたファイルにキーが無いエントリは、移譲先 0 件として再試行を終えます。 */
static void finish_absent_keys(cplat_sym_loader_entry *const *fobj_array, size_t fobj_length)
{
    size_t index;

    if (fobj_array == NULL)
    {
        return;
    }

    for (index = 0; index < fobj_length; index++)
    {
        cplat_sym_loader_entry *entry = fobj_array[index];
        cplat_sym_loader_entry_impl *impl;

        if ((entry == NULL) || (entry->impl == NULL))
        {
            continue;
        }

        impl = entry->impl;
        if ((impl->needs_reload == 0) || (impl->adopted_in_apply != 0) || (impl->failed_this_apply != 0))
        {
            continue;
        }

        release_loaded_lists(entry);
        impl->needs_reload = 0;
        cplat_atomic_store_i32(&entry->failed, 0, CPLAT_MEMORY_ORDER_RELEASE);
    }
}

int cplat_sym_loader_apply_config(cplat_sym_loader_entry *const *fobj_array, size_t fobj_length,
                                  const char *configpath)
{
    FILE *fp;
    char *buffer;
    cJSON *root;
    int64_t size;
    size_t file_size;
    size_t read_count;
    const cJSON *entry;
    int applied;

    if ((configpath == NULL) || (configpath[0] == '\0'))
    {
        return 1;
    }

    begin_apply(fobj_array, fobj_length);

    fp = cplat_fopen(configpath, "rb", NULL);
    if (fp == NULL)
    {
        return 1;
    }

    buffer = NULL;
    root = NULL;
    applied = 1;

    if (cplat_fseek(fp, 0, SEEK_END) != 0)
    {
        goto out_close;
    }

    size = cplat_ftell(fp);

    /* ファイル サイズが 0 以下、または上限の CONFIG_FILE_MAX を超える場合は読み込みを中止する。 */
    if ((size <= 0) || (size > CONFIG_FILE_MAX))
    {
        goto out_close;
    }

    if (cplat_fseek(fp, 0, SEEK_SET) != 0)
    {
        goto out_close;
    }

    file_size = (size_t)size;
    buffer = (char *)cplat_malloc(file_size + 1u);
    if (buffer == NULL)
    {
        goto out_close;
    }

    read_count = cplat_fread(buffer, 1u, file_size, fp, NULL);
    if (read_count != file_size)
    {
        goto out_free_buffer;
    }

    buffer[file_size] = '\0';

    root = cJSON_ParseJSONCWithLength(buffer, file_size);
    if ((root == NULL) || (cJSON_IsObject(root) == 0))
    {
        goto out_free_json;
    }

    for (entry = root->child; entry != NULL; entry = entry->next)
    {
        handle_property(entry, fobj_array, fobj_length, configpath);
    }

    finish_absent_keys(fobj_array, fobj_length);
    applied = 0;

out_free_json:
    cJSON_Delete(root);
    root = NULL;
out_free_buffer:
    cplat_free(buffer);
    buffer = NULL;
out_close:
    (void)cplat_fclose(fp, NULL);
    return applied;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_sym_loader_init(cplat_sym_loader_entry *const *fobj_array, const size_t fobj_length, const char *configpath)
{
    if ((configpath == NULL) || (configpath[0] == '\0'))
    {
        return;
    }

    (void)cplat_sym_loader_apply_config(fobj_array, fobj_length, configpath);
}
