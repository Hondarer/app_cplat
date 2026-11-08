/* テスト対象ソース ファイルの注入用追加ソース
 * このソースはテスト対象ソースの末尾に結合されます
 * この static 関数へのアクセサーによって
 * テスト プログラムからテスト対象ソースの static 関数にアクセスできます
 */
#ifndef _IN_TEST_SRC
    #include "sync_linux.c"
#endif /* _IN_TEST_SRC */

#include "sync_linux.inject.h"

#if defined(PLATFORM_LINUX)

int test_sync_map_wait_rc(const int rc)
{
    return map_wait_rc(rc);
}

int test_sync_monotonic_ms(uint64_t *milliseconds_out)
{
    return monotonic_ms(milliseconds_out);
}

int test_sync_monotonic_deadline(struct timespec *abs_ts, const int timeout_ms)
{
    return monotonic_deadline(abs_ts, timeout_ms);
}

void test_sync_set_local_rwlock_state(cplat_local_rwlock *rwlock, const int writer_active,
                                      const unsigned int active_readers, const unsigned int waiting_writers)
{
    rwlock->writer_active = writer_active;
    rwlock->active_readers = active_readers;
    rwlock->waiting_writers = waiting_writers;
}

#endif /* PLATFORM_LINUX */
