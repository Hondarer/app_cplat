/* テスト対象ソース ファイルの注入用追加ソース
 * このソースはテスト対象ソースの末尾に結合されます
 * この static 関数へのアクセサーによって
 * テスト プログラムからテスト対象ソースの static 関数にアクセスできます
 */
#ifndef _IN_TEST_SRC
    #include "sym_loader_resolve.c"
#endif /* _IN_TEST_SRC */

#include "sym_loader_resolve.inject.h"

int test_sym_loader_ensure_entry_lock_initialized(cplat_sym_loader_entry *fobj)
{
    return ensure_entry_lock_initialized(fobj);
}

int test_sym_loader_publish_resolved_pointer(cplat_sym_loader_entry *fobj, void *func_ptr)
{
    cplat_sym_loader_entry_impl *impl;
    cplat_sym_loader_usable *usable;

    impl = (cplat_sym_loader_entry_impl *)cplat_calloc(1u, sizeof(*impl));
    if (impl == NULL)
    {
        return -1;
    }

    usable = (cplat_sym_loader_usable *)cplat_calloc(1u, sizeof(*usable));
    if (usable == NULL)
    {
        cplat_free(impl);
        return -1;
    }

    usable[0].func_ptr = func_ptr;
    impl->usable = usable;
    impl->usable_count = 1u;
    fobj->impl = impl;
    cplat_atomic_store_i32(&fobj->failed, 0, CPLAT_MEMORY_ORDER_RELEASE);
    cplat_atomic_store_i32(&fobj->resolved, 1, CPLAT_MEMORY_ORDER_RELEASE);
    return 0;
}

#if defined(PLATFORM_LINUX)
void test_sym_loader_set_entry_lock_wait_hook(void (*hook)(cplat_sym_loader_entry *fobj))
{
    if (hook == NULL)
    {
        s_entry_lock_wait_hook = wait_for_entry_lock_initialization;
    }
    else
    {
        s_entry_lock_wait_hook = hook;
    }
}

void test_sym_loader_default_entry_lock_wait(cplat_sym_loader_entry *fobj)
{
    wait_for_entry_lock_initialization(fobj);
}
#endif /* PLATFORM_LINUX */
