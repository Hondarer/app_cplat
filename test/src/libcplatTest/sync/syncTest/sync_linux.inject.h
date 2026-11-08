/* テスト対象ソース ファイルの注入用追加ヘッダー
 * このヘッダーをテスト プログラムが参照することで
 * テスト プログラムからテスト対象ソースの static 関数にアクセスできます
 */
#ifndef SYNC_LINUX_INJECT_H
#define SYNC_LINUX_INJECT_H

#include <cplat/base/platform.h>

#if defined(PLATFORM_LINUX)

    #include <time.h>
    #include <cplat/sync/sync.h>

    #ifdef __cplusplus
extern "C"
{
    #endif /* __cplusplus */

    extern int test_sync_map_wait_rc(int rc);
    extern int test_sync_monotonic_ms(uint64_t *milliseconds_out);
    extern int test_sync_monotonic_deadline(struct timespec *abs_ts, int timeout_ms);
    extern void test_sync_set_local_rwlock_state(cplat_local_rwlock *rwlock, int writer_active,
                                                 unsigned int active_readers, unsigned int waiting_writers);

    #ifdef __cplusplus
}
    #endif /* __cplusplus */

#endif /* PLATFORM_LINUX */

#endif /* SYNC_LINUX_INJECT_H */
