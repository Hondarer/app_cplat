/* テスト対象ソース ファイルの注入用追加ソース
 * このソースはテスト対象ソースの末尾に結合されます
 * この static 関数へのアクセサーによって
 * テスト プログラムからテスト対象ソースの static 関数にアクセスできます
 */
#ifndef _IN_TEST_SRC
    #include "trace_file.c"
#endif /* _IN_TEST_SRC */

#include "trace_file.inject.h"

#include <string.h>

cplat_trace_file_sink *test_trace_file_sink_create_unregistered(const char *path, const size_t max_bytes,
                                                                const int generations, const int flags)
{
    return create_new_sink(path, strlen(path), max_bytes, generations, flags);
}

/* テスト実行中のスレッドが静止した状態で、登録表と初回初期化の状態を戻す。 */
void test_trace_file_reset_registry(void)
{
    const cplat_once_flag initial_once = {0};

    while (s_sink_registry.count > 0)
    {
        struct sink_registry_entry *entry = &s_sink_registry.items[s_sink_registry.count - 1];

        cplat_free(entry->key);
        free_sink(entry->sink);
        s_sink_registry.count--;
    }

    cplat_free(s_sink_registry.items);
    s_sink_registry.items = NULL;
    s_sink_registry.capacity = 0;

    if (s_sink_registry_lock != NULL)
    {
        cplat_local_lock_dispose(s_sink_registry_lock);
        s_sink_registry_lock = NULL;
    }

    s_sink_registry_lock_result = CPLAT_ERR_UNKNOWN;
    s_sink_registry_lock_once = initial_once;
}
