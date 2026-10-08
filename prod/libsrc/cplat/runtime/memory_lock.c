#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif

/**
 *******************************************************************************
 *  @file           memory_lock.c
 *  @brief          自プロセスのメモリ ロックと scope 解放を OS API へ委譲します。
 *  @author         Tetsuo Honda
 *  @date           2026/07/05
 *
 *  本ファイルは、範囲ロック API と自プロセス全体の現在分ロック API を実装します。\n
 *  self scope API は OS のプロセス単位ロック状態を複数の呼び出しに対応させるため、cplat の
 *  local lock で内部状態を保護します。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *******************************************************************************
 */

#include <cplat/runtime/memory_lock.h>
#include <cplat/crt/stdlib.h>
#include <cplat/base/result_internal.h>
#include <cplat/sync/sync.h>

#include <stdint.h>
#include <stdlib.h>

#if defined(PLATFORM_LINUX)
    #include <errno.h>
    #include <pthread.h>
    #include <string.h>
    #include <sys/mman.h>
#elif defined(PLATFORM_WINDOWS)
    #include <cplat/base/windows_sdk.h>
#endif

/** cplat_memory_lock_self_options::flags で受け付ける bit の集合。 */
#define CPLAT_MEMORY_LOCK_KNOWN_FLAGS (CPLAT_MEMORY_LOCK_CURRENT | CPLAT_MEMORY_LOCK_FUTURE | CPLAT_MEMORY_LOCK_ONFAULT)
/** stack prefault で 1 回にアクセスするスタック サイズ。 */
#define CPLAT_MEMORY_LOCK_STACK_PREFAULT_CHUNK 4096U
/** stack overflow を避けるために残すスタック サイズ。 */
#define CPLAT_MEMORY_LOCK_STACK_SAFETY_MARGIN (64U * 1024U)

#if defined(PLATFORM_WINDOWS)
/**
 *  @brief          Windows でロック済みメモリ範囲を表します。
 */
typedef struct cplat_memory_lock_range_entry
{
    uintptr_t base;   /**< 範囲の先頭アドレス。 */
    uintptr_t end;    /**< 範囲の終端アドレス。 */
    size_t ref_count; /**< scope から参照されている数。 */
} cplat_memory_lock_range_entry;
#endif

/**
 *  @brief          cplat_memory_lock_self() の成功結果を解放するための内部情報です。
 */
struct cplat_memory_lock_scope
{
#if defined(PLATFORM_WINDOWS)
    cplat_memory_lock_range_entry *entries; /**< 本 scope が参照するロック範囲の配列。 */
    size_t count;                           /**< entries の有効要素数。 */
    size_t capacity;                        /**< entries の確保済み要素数。 */
#else
    int locked_all; /**< mlockall() が成功した scope であることを示す値。 */
    int flags;      /**< mlockall() へ渡した cplat の flag。 */
#endif
};

static cplat_local_lock *s_memory_lock_lock;          /**< self scope の共有状態を保護する local lock。 */
static cplat_once_flag s_memory_lock_lock_once = {0}; /**< local lock 初期化を 1 回だけ実行する once flag。 */

#if defined(PLATFORM_LINUX)
static size_t s_linux_self_lock_scope_count; /**< Linux で成功中の self scope 数。 */
#elif defined(PLATFORM_WINDOWS)
static cplat_memory_lock_range_entry *s_windows_locked_ranges; /**< Windows でロック済み範囲を保持する registry。 */
static size_t s_windows_locked_range_count;                    /**< registry の有効要素数。 */
static size_t s_windows_locked_range_capacity;                 /**< registry の確保済み要素数。 */
#endif

/**
 *  @brief          メモリ ロック内部状態を保護する local lock を初期化します。
 */
static void init_memory_lock_lock(void)
{
    (void)cplat_local_lock_create(&s_memory_lock_lock);
}

/**
 *  @brief          メモリ ロック内部状態を保護する local lock を取得します。
 *  @return         取得できた場合は CPLAT_OK、それ以外はエラー結果を返します。
 */
static int memory_lock_lock(void)
{
    int result = CPLAT_OK;

    cplat_call_once(&s_memory_lock_lock_once, init_memory_lock_lock);
    if (s_memory_lock_lock == NULL)
    {
        result = CPLAT_ERR_UNKNOWN;
    }
    else if (cplat_local_lock_lock(s_memory_lock_lock, CPLAT_SYNC_WAIT_FOREVER) != CPLAT_OK)
    {
        result = CPLAT_ERR_UNKNOWN;
    }

    return result;
}

/**
 *  @brief          メモリ ロック内部状態を保護する local lock を解放します。
 */
static void memory_lock_unlock(void)
{
    if (s_memory_lock_lock != NULL)
    {
        (void)cplat_local_lock_unlock(s_memory_lock_lock);
    }
}

/**
 *  @brief          指定サイズ分のスタック ページへアクセスします。
 *  @param[in]      remaining 追加でアクセスするスタック サイズ。
 */
static NO_INLINE void prefault_stack_recursive(size_t remaining)
{
    volatile unsigned char stack_page[CPLAT_MEMORY_LOCK_STACK_PREFAULT_CHUNK];

    stack_page[0] = 0U;
    stack_page[sizeof(stack_page) - 1U] = 0U;

    if (remaining > sizeof(stack_page))
    {
        prefault_stack_recursive(remaining - sizeof(stack_page));
    }

    stack_page[0] = (unsigned char)remaining;
}

#if defined(PLATFORM_LINUX)
/**
 *  @brief          errno をメモリ ロック結果コードへ変換します。
 *  @param[in]      error_no errno の値。
 *  @return         対応する結果コードを返します。
 *
 *  mlock() 系の ENOMEM はロック可能量の上限超過を意味するため、共通の
 *  errno マッピングとは別に @ref CPLAT_ERR_LIMIT_EXCEEDED として扱います。\n
 *  それ以外の errno は cplat_internal_result_from_errno() の分類に委譲します。
 */
static int map_errno_to_memory_lock_result(int error_no)
{
    int result;

    if (error_no == ENOMEM)
    {
        result = CPLAT_ERR_LIMIT_EXCEEDED;
    }
    else
    {
        result = cplat_internal_result_from_errno(error_no);
    }

    return result;
}

/**
 *  @brief          cplat の self lock flag を Linux の mlockall() flag へ変換します。
 *  @param[in]      flags         cplat の self lock flag。
 *  @param[out]     native_flags  Linux の mlockall() flag の格納先。NULL 可。
 *  @return         変換成功時は 0、不正 flag は -1、未対応 flag は 1 を返します。
 */
static int convert_flags_to_mlockall_flags(int flags, int *native_flags)
{
    int result = 0;
    int converted = 0;

    if ((flags == 0) || ((flags & ~CPLAT_MEMORY_LOCK_KNOWN_FLAGS) != 0))
    {
        result = -1;
    }
    else
    {
        if ((flags & CPLAT_MEMORY_LOCK_CURRENT) != 0)
        {
            converted |= MCL_CURRENT;
        }
        if ((flags & CPLAT_MEMORY_LOCK_FUTURE) != 0)
        {
            converted |= MCL_FUTURE;
        }
        if ((flags & CPLAT_MEMORY_LOCK_ONFAULT) != 0)
        {
    #if defined(MCL_ONFAULT)
            converted |= MCL_ONFAULT;
    #else
            result = 1;
    #endif
        }
    }

    if ((result == 0) && (native_flags != NULL))
    {
        *native_flags = converted;
    }

    return result;
}

/**
 *  @brief          Linux で呼び出しスレッドのスタックをロック前に committed page 化します。
 *  @param[in]      stack_prefault_bytes 追加でアクセスするスタック サイズ。0 可。
 *  @return         成功時は CPLAT_OK、それ以外はエラー結果を返します。
 */
static int prefault_stack(size_t stack_prefault_bytes)
{
    int result = CPLAT_OK;

    if (stack_prefault_bytes > 0U)
    {
        pthread_attr_t attr;
        void *stack_addr = NULL;
        size_t stack_size = 0U;
        unsigned char marker = 0U;

        int attr_result = pthread_getattr_np(pthread_self(), &attr);
        if (attr_result != 0)
        {
            result = CPLAT_ERR_UNKNOWN;
        }
        else
        {
            int stack_result = pthread_attr_getstack(&attr, &stack_addr, &stack_size);
            (void)pthread_attr_destroy(&attr);

            if (stack_result != 0)
            {
                result = CPLAT_ERR_UNKNOWN;
            }
            else
            {
                uintptr_t stack_low = (uintptr_t)stack_addr;
                uintptr_t stack_high = stack_low + stack_size;
                uintptr_t current = (uintptr_t)&marker;
                size_t available = 0U;

                if ((current > stack_low) && (current < stack_high))
                {
                    available = (size_t)(current - stack_low);
                }

                if ((available <= CPLAT_MEMORY_LOCK_STACK_SAFETY_MARGIN) ||
                    (stack_prefault_bytes > (available - CPLAT_MEMORY_LOCK_STACK_SAFETY_MARGIN)))
                {
                    result = CPLAT_ERR_LIMIT_EXCEEDED;
                }
                else
                {
                    prefault_stack_recursive(stack_prefault_bytes);
                }
            }
        }
    }

    return result;
}
#elif defined(PLATFORM_WINDOWS)
/**
 *  @brief          Windows エラー コードをメモリ ロック結果コードへ変換します。
 *  @param[in]      error_code GetLastError() で取得したエラー コード。
 *  @return         対応する結果コードを返します。
 *
 *  ワーキング セットやコミット上限に関するエラーはロック可能量の上限超過を意味するため、
 *  共通の Windows エラー マッピングとは別に @ref CPLAT_ERR_LIMIT_EXCEEDED として扱います。\n
 *  それ以外のエラーは cplat_internal_result_from_windows_error() の分類に委譲します。
 */
static int map_windows_error_to_memory_lock_result(unsigned long error_code)
{
    int result;

    if ((error_code == ERROR_NOT_ENOUGH_MEMORY) || (error_code == ERROR_WORKING_SET_QUOTA) ||
        (error_code == ERROR_COMMITMENT_LIMIT) || (error_code == ERROR_NO_SYSTEM_RESOURCES))
    {
        result = CPLAT_ERR_LIMIT_EXCEEDED;
    }
    else
    {
        result = cplat_internal_result_from_windows_error(error_code);
    }

    return result;
}

/**
 *  @brief          VirtualLock() の対象にできる committed region か判定します。
 *  @param[in]      info VirtualQuery() で取得したメモリ情報。
 *  @return         ロック対象にできる場合は 1、それ以外は 0 を返します。
 */
static int is_lockable_region(const MEMORY_BASIC_INFORMATION *info)
{
    int lockable = 0;

    if ((info->State == MEM_COMMIT) && ((info->Protect & PAGE_NOACCESS) == 0) && ((info->Protect & PAGE_GUARD) == 0))
    {
        lockable = 1;
    }

    return lockable;
}

/**
 *  @brief          self scope が参照するロック範囲を追加します。
 *  @param[in,out]  scope 範囲を追加する self scope。
 *  @param[in]      base  範囲の先頭アドレス。
 *  @param[in]      end   範囲の終端アドレス。
 *  @return         成功時は CPLAT_OK、それ以外はエラー結果を返します。
 */
static int append_scope_range(cplat_memory_lock_scope *scope, uintptr_t base, uintptr_t end)
{
    int result = CPLAT_OK;

    if (scope->count == scope->capacity)
    {
        size_t new_capacity = 16U;
        if (scope->capacity > 0U)
        {
            new_capacity = scope->capacity * 2U;
        }

        cplat_memory_lock_range_entry *new_entries =
            (cplat_memory_lock_range_entry *)cplat_realloc(scope->entries, new_capacity, sizeof(*new_entries));
        if (new_entries == NULL)
        {
            result = CPLAT_ERR_UNKNOWN;
        }
        else
        {
            scope->entries = new_entries;
            scope->capacity = new_capacity;
        }
    }

    if (result == CPLAT_OK)
    {
        scope->entries[scope->count].base = base;
        scope->entries[scope->count].end = end;
        scope->entries[scope->count].ref_count = 0U;
        scope->count++;
    }

    return result;
}

/**
 *  @brief          Windows ロック範囲 registry の容量を確保します。
 *  @param[in]      required_capacity 必要な要素数。
 *  @return         成功時は CPLAT_OK、それ以外はエラー結果を返します。
 */
static int ensure_windows_registry_capacity(size_t required_capacity)
{
    int result = CPLAT_OK;

    if (required_capacity > s_windows_locked_range_capacity)
    {
        size_t new_capacity = 16U;
        if (s_windows_locked_range_capacity > 0U)
        {
            new_capacity = s_windows_locked_range_capacity;
        }
        while ((result == CPLAT_OK) && (new_capacity < required_capacity))
        {
            if (new_capacity > ((size_t)-1) / 2U)
            {
                result = CPLAT_ERR_LIMIT_EXCEEDED;
            }
            else
            {
                new_capacity *= 2U;
            }
        }

        if (result == CPLAT_OK)
        {
            cplat_memory_lock_range_entry *new_ranges = (cplat_memory_lock_range_entry *)cplat_realloc(
                s_windows_locked_ranges, new_capacity, sizeof(*new_ranges));
            if (new_ranges == NULL)
            {
                result = CPLAT_ERR_UNKNOWN;
            }
            else
            {
                s_windows_locked_ranges = new_ranges;
                s_windows_locked_range_capacity = new_capacity;
            }
        }
    }

    return result;
}

/**
 *  @brief          Windows ロック範囲 registry へ範囲を挿入します。
 *  @param[in]      index     挿入位置。
 *  @param[in]      base      範囲の先頭アドレス。
 *  @param[in]      end       範囲の終端アドレス。
 *  @param[in]      ref_count 初期参照数。
 *  @return         成功時は CPLAT_OK、それ以外はエラー結果を返します。
 */
static int insert_windows_registry_range(size_t index, uintptr_t base, uintptr_t end, size_t ref_count)
{
    int result = ensure_windows_registry_capacity(s_windows_locked_range_count + 1U);

    if (result == CPLAT_OK)
    {
        size_t move_index = s_windows_locked_range_count;

        while (move_index > index)
        {
            s_windows_locked_ranges[move_index] = s_windows_locked_ranges[move_index - 1U];
            move_index--;
        }
        s_windows_locked_ranges[index].base = base;
        s_windows_locked_ranges[index].end = end;
        s_windows_locked_ranges[index].ref_count = ref_count;
        s_windows_locked_range_count++;
    }

    return result;
}

/**
 *  @brief          Windows ロック範囲 registry から範囲を削除します。
 *  @param[in]      index 削除位置。
 */
static void remove_windows_registry_range(size_t index)
{
    size_t move_index = index;

    while ((move_index + 1U) < s_windows_locked_range_count)
    {
        s_windows_locked_ranges[move_index] = s_windows_locked_ranges[move_index + 1U];
        move_index++;
    }
    s_windows_locked_range_count--;
}

/**
 *  @brief          registry 内の範囲を指定境界で分割します。
 *  @param[in]      boundary 分割境界のアドレス。
 *  @return         成功時は CPLAT_OK、それ以外はエラー結果を返します。
 */
static int split_windows_registry_at(uintptr_t boundary)
{
    int result = CPLAT_OK;
    size_t index = 0U;

    while ((result == CPLAT_OK) && (index < s_windows_locked_range_count))
    {
        cplat_memory_lock_range_entry *entry = &s_windows_locked_ranges[index];

        if ((entry->base < boundary) && (boundary < entry->end))
        {
            uintptr_t old_end = entry->end;
            size_t ref_count = entry->ref_count;

            entry->end = boundary;
            result = insert_windows_registry_range(index + 1U, boundary, old_end, ref_count);
        }
        index++;
    }

    return result;
}

/**
 *  @brief          指定アドレスを含む registry 範囲を検索します。
 *  @param[in]      address   検索するアドレス。
 *  @param[out]     index_out 見つかった要素番号の格納先。NULL 可。
 *  @return         見つかった場合は 1、それ以外は 0 を返します。
 */
static int find_windows_registry_range(uintptr_t address, size_t *index_out)
{
    int found = 0;
    size_t index = 0U;

    while ((found == 0) && (index < s_windows_locked_range_count))
    {
        if ((s_windows_locked_ranges[index].base <= address) && (address < s_windows_locked_ranges[index].end))
        {
            found = 1;
            if (index_out != NULL)
            {
                *index_out = index;
            }
        }
        else
        {
            index++;
        }
    }

    return found;
}

/**
 *  @brief          指定アドレスより後にある次の registry 範囲の先頭を返します。
 *  @param[in]      address 探索開始アドレス。
 *  @param[in]      end     探索上限アドレス。
 *  @return         次の registry 範囲の先頭、または @p end を返します。
 */
static uintptr_t find_next_windows_registry_base(uintptr_t address, uintptr_t end)
{
    uintptr_t next = end;
    size_t index = 0U;

    while (index < s_windows_locked_range_count)
    {
        uintptr_t base = s_windows_locked_ranges[index].base;

        if ((base > address) && (base < next))
        {
            next = base;
        }
        index++;
    }

    return next;
}

/**
 *  @brief          registry の整列順を保つ挿入位置を返します。
 *  @param[in]      base 挿入する範囲の先頭アドレス。
 *  @return         挿入位置を返します。
 */
static size_t find_windows_registry_insert_index(uintptr_t base)
{
    size_t index = 0U;

    while ((index < s_windows_locked_range_count) && (s_windows_locked_ranges[index].base < base))
    {
        index++;
    }

    return index;
}

/**
 *  @brief          整数表現のアドレスを Windows API に渡すポインターへ変換します。
 *  @param[in]      address 整数表現のアドレス。
 *  @return         Windows API に渡すポインターを返します。
 */
static void *pointer_from_uintptr(uintptr_t address)
{
    return (void *)address;
}

/**
 *  @brief          self scope が参照している Windows ロック範囲を解放します。
 *  @param[in,out]  scope 解放する self scope。
 *  @return         成功時は CPLAT_OK、それ以外はエラー結果を返します。
 *
 *  registry の ref-count を減算し、0 になった範囲だけ VirtualUnlock() へ渡します。
 */
static int release_windows_scope_ranges(cplat_memory_lock_scope *scope)
{
    size_t index = scope->count;
    int result = CPLAT_OK;

    while (index > 0U)
    {
        size_t registry_index = 0U;
        cplat_memory_lock_range_entry *entry = NULL;

        index--;
        if (find_windows_registry_range(scope->entries[index].base, &registry_index) == 0)
        {
            result = CPLAT_ERR_UNKNOWN;
        }
        else
        {
            entry = &s_windows_locked_ranges[registry_index];
            if ((entry->base != scope->entries[index].base) || (entry->end != scope->entries[index].end) ||
                (entry->ref_count == 0U))
            {
                result = CPLAT_ERR_UNKNOWN;
            }
            else
            {
                entry->ref_count--;
                if (entry->ref_count == 0U)
                {
                    SIZE_T size = (SIZE_T)(entry->end - entry->base);
                    if (VirtualUnlock(pointer_from_uintptr(entry->base), size) == 0)
                    {
                        result = map_windows_error_to_memory_lock_result(GetLastError());
                    }
                    remove_windows_registry_range(registry_index);
                }
            }
        }
    }

    scope->count = 0U;

    return result;
}

/**
 *  @brief          Windows の registry と OS の両方で指定範囲を取得します。
 *  @param[in,out]  scope 範囲取得に成功した単位を記録する self scope。
 *  @param[in]      base  取得する範囲の先頭アドレス。
 *  @param[in]      end   取得する範囲の終端アドレス。
 *  @return         成功時は CPLAT_OK、それ以外はエラー結果を返します。
 *
 *  既存 registry 範囲は ref-count を増やし、未登録範囲だけ VirtualLock() へ渡します。\n
 *  途中で失敗した場合は、本関数内で取得済みの範囲を release_windows_scope_ranges() で戻します。
 */
static int acquire_windows_range(cplat_memory_lock_scope *scope, uintptr_t base, uintptr_t end)
{
    int result = CPLAT_OK;
    uintptr_t current = base;

    if (base >= end)
    {
        result = CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (result == CPLAT_OK)
    {
        result = split_windows_registry_at(base);
    }
    if (result == CPLAT_OK)
    {
        result = split_windows_registry_at(end);
    }

    while ((result == CPLAT_OK) && (current < end))
    {
        size_t registry_index = 0U;

        if (find_windows_registry_range(current, &registry_index) != 0)
        {
            uintptr_t range_end = s_windows_locked_ranges[registry_index].end;
            if (range_end > end)
            {
                range_end = end;
            }

            s_windows_locked_ranges[registry_index].ref_count++;
            result = append_scope_range(scope, current, range_end);
            if (result != CPLAT_OK)
            {
                s_windows_locked_ranges[registry_index].ref_count--;
            }
            current = range_end;
        }
        else
        {
            uintptr_t range_end = find_next_windows_registry_base(current, end);
            SIZE_T range_size = (SIZE_T)(range_end - current);

            if (VirtualLock(pointer_from_uintptr(current), range_size) == 0)
            {
                result = map_windows_error_to_memory_lock_result(GetLastError());
            }
            else
            {
                size_t insert_index = find_windows_registry_insert_index(current);

                result = insert_windows_registry_range(insert_index, current, range_end, 1U);
                if (result != CPLAT_OK)
                {
                    (void)VirtualUnlock(pointer_from_uintptr(current), range_size);
                }
                else
                {
                    result = append_scope_range(scope, current, range_end);
                    if (result != CPLAT_OK)
                    {
                        (void)VirtualUnlock(pointer_from_uintptr(current), range_size);
                        remove_windows_registry_range(insert_index);
                    }
                }
            }
            current = range_end;
        }
    }

    if (result != CPLAT_OK)
    {
        (void)release_windows_scope_ranges(scope);
    }

    return result;
}

/**
 *  @brief          Windows で呼び出しスレッドのスタックをロック前に committed page 化します。
 *  @param[in]      stack_prefault_bytes 追加でアクセスするスタック サイズ。0 可。
 *  @return         成功時は CPLAT_OK、それ以外はエラー結果を返します。
 */
static int prefault_stack(size_t stack_prefault_bytes)
{
    int result = CPLAT_OK;

    if (stack_prefault_bytes > 0U)
    {
        ULONG_PTR stack_low = 0U;
        ULONG_PTR stack_high = 0U;
        unsigned char marker = 0U;

        GetCurrentThreadStackLimits(&stack_low, &stack_high);

        uintptr_t current = (uintptr_t)&marker;
        size_t available = 0U;
        if ((current > (uintptr_t)stack_low) && (current < (uintptr_t)stack_high))
        {
            available = (size_t)(current - (uintptr_t)stack_low);
        }

        if ((available <= CPLAT_MEMORY_LOCK_STACK_SAFETY_MARGIN) ||
            (stack_prefault_bytes > (available - CPLAT_MEMORY_LOCK_STACK_SAFETY_MARGIN)))
        {
            result = CPLAT_ERR_LIMIT_EXCEEDED;
        }
        else
        {
            prefault_stack_recursive(stack_prefault_bytes);
        }
    }

    return result;
}
#else
/**
 *  @brief          未対応プラットフォームで stack prefault の結果を返します。
 *  @param[in]      stack_prefault_bytes 追加でアクセスするスタック サイズ。0 可。
 *  @return         0 の場合は CPLAT_OK、それ以外は CPLAT_ERR_UNSUPPORTED を返します。
 */
static int prefault_stack(size_t stack_prefault_bytes)
{
    int result = CPLAT_OK;

    if (stack_prefault_bytes > 0U)
    {
        result = CPLAT_ERR_UNSUPPORTED;
    }

    return result;
}
#endif

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_memory_lock_range(const void *address, size_t size)
{
    int result = CPLAT_OK;

    if ((address == NULL) || (size == 0U))
    {
        result = CPLAT_ERR_INVALID_ARGUMENT;
    }
    else
    {
#if defined(PLATFORM_LINUX)
        if (mlock(address, size) != 0)
        {
            result = map_errno_to_memory_lock_result(errno);
        }
#elif defined(PLATFORM_WINDOWS)
        /* VirtualLock() はメモリ内容を書き換えないが、Windows API の型に合わせる。 */
        LPVOID lock_address = (LPVOID)(uintptr_t)address;
        if (VirtualLock(lock_address, size) == 0)
        {
            result = map_windows_error_to_memory_lock_result(GetLastError());
        }
#else
        result = CPLAT_ERR_UNSUPPORTED;
#endif
    }

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_memory_unlock_range(const void *address, size_t size)
{
    int result = CPLAT_OK;

    if ((address == NULL) || (size == 0U))
    {
        result = CPLAT_ERR_INVALID_ARGUMENT;
    }
    else
    {
#if defined(PLATFORM_LINUX)
        if (munlock(address, size) != 0)
        {
            result = map_errno_to_memory_lock_result(errno);
        }
#elif defined(PLATFORM_WINDOWS)
        /* VirtualUnlock() はメモリ内容を書き換えないが、Windows API の型に合わせる。 */
        LPVOID unlock_address = (LPVOID)(uintptr_t)address;
        if (VirtualUnlock(unlock_address, size) == 0)
        {
            result = map_windows_error_to_memory_lock_result(GetLastError());
        }
#else
        result = CPLAT_ERR_UNSUPPORTED;
#endif
    }

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_memory_lock_self(const cplat_memory_lock_self_options *options, cplat_memory_lock_scope **scope)
{
    int result = CPLAT_OK;

    if ((options == NULL) || (scope == NULL) || (options->flags == 0) ||
        ((options->flags & ~CPLAT_MEMORY_LOCK_KNOWN_FLAGS) != 0))
    {
        result = CPLAT_ERR_INVALID_ARGUMENT;
    }
    else
    {
        *scope = NULL;

        result = prefault_stack(options->stack_prefault_bytes);

        if (result != CPLAT_OK)
        {
            return result;
        }

#if defined(PLATFORM_LINUX)
        int native_flags = 0;
        (void)convert_flags_to_mlockall_flags(options->flags, &native_flags);
        {
            cplat_memory_lock_scope *new_scope =
                (cplat_memory_lock_scope *)cplat_calloc(1U, sizeof(cplat_memory_lock_scope));
            if (new_scope == NULL)
            {
                result = CPLAT_ERR_UNKNOWN;
            }
            else
            {
                result = memory_lock_lock();
                if (result == CPLAT_OK)
                {
                    if (mlockall(native_flags) != 0)
                    {
                        result = map_errno_to_memory_lock_result(errno);
                    }
                    else
                    {
                        s_linux_self_lock_scope_count++;
                        new_scope->locked_all = 1;
                        new_scope->flags = options->flags;
                        *scope = new_scope;
                    }
                    memory_lock_unlock();
                }

                if (result != CPLAT_OK)
                {
                    cplat_free(new_scope);
                }
            }
        }
#elif defined(PLATFORM_WINDOWS)
        if (options->flags != CPLAT_MEMORY_LOCK_CURRENT)
        {
            result = CPLAT_ERR_UNSUPPORTED;
        }
        else
        {
            cplat_memory_lock_scope *new_scope =
                (cplat_memory_lock_scope *)cplat_calloc(1U, sizeof(cplat_memory_lock_scope));
            if (new_scope == NULL)
            {
                result = CPLAT_ERR_UNKNOWN;
            }
            else
            {
                SYSTEM_INFO system_info;

                result = memory_lock_lock();
                if (result == CPLAT_OK)
                {
                    GetSystemInfo(&system_info);

                    uintptr_t current = (uintptr_t)system_info.lpMinimumApplicationAddress;
                    uintptr_t maximum = (uintptr_t)system_info.lpMaximumApplicationAddress;
                    size_t page_size = (size_t)system_info.dwPageSize;

                    while ((result == CPLAT_OK) && (current < maximum))
                    {
                        MEMORY_BASIC_INFORMATION info;
                        SIZE_T query_size = VirtualQuery((LPCVOID)current, &info, sizeof(info));
                        if (query_size == 0U)
                        {
                            current += page_size;
                        }
                        else
                        {
                            uintptr_t base = (uintptr_t)info.BaseAddress;
                            uintptr_t next = base + (uintptr_t)info.RegionSize;

                            if (is_lockable_region(&info) != 0)
                            {
                                result = acquire_windows_range(new_scope, base, next);
                            }

                            if (next <= current)
                            {
                                current += page_size;
                            }
                            else
                            {
                                current = next;
                            }
                        }
                    }

                    if (result == CPLAT_OK)
                    {
                        *scope = new_scope;
                    }
                    memory_lock_unlock();
                }

                if (result != CPLAT_OK)
                {
                    cplat_free(new_scope->entries);
                    cplat_free(new_scope);
                }
            }
        }
#else
        result = CPLAT_ERR_UNSUPPORTED;
#endif
    }

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_memory_lock_scope_release(cplat_memory_lock_scope *scope)
{
    int result = CPLAT_OK;

    if (scope != NULL)
    {
#if defined(PLATFORM_LINUX)
        if (scope->locked_all != 0)
        {
            result = memory_lock_lock();
            if (result == CPLAT_OK)
            {
                if (s_linux_self_lock_scope_count == 0U)
                {
                    result = CPLAT_ERR_UNKNOWN;
                }
                else
                {
                    s_linux_self_lock_scope_count--;
                    if (s_linux_self_lock_scope_count == 0U)
                    {
                        if (munlockall() != 0)
                        {
                            result = map_errno_to_memory_lock_result(errno);
                        }
                    }
                }
                memory_lock_unlock();
            }
        }
        cplat_free(scope);
#elif defined(PLATFORM_WINDOWS)
        result = memory_lock_lock();
        if (result == CPLAT_OK)
        {
            result = release_windows_scope_ranges(scope);
            memory_lock_unlock();
        }
        cplat_free(scope->entries);
        cplat_free(scope);
#else
        cplat_free(scope);
        result = CPLAT_ERR_UNSUPPORTED;
#endif
    }

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_secure_zero(void *buf, const size_t size)
{
    if (buf == NULL || size == 0U)
    {
        return;
    }

#if defined(PLATFORM_LINUX)
    explicit_bzero(buf, size);
#elif defined(PLATFORM_WINDOWS)
    SecureZeroMemory(buf, size);
#endif /* PLATFORM_ */
}
