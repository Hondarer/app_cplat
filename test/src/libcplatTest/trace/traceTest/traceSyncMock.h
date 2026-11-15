#ifndef TRACE_SYNC_MOCK_H
#define TRACE_SYNC_MOCK_H

#include <mock_cplat.h>
#include <cstdint>

inline void set_trace_sync_mock_defaults(Mock_cplat &mock_cplat)
{
    using testing::_;
    using testing::Return;

    ON_CALL(mock_cplat, cplat_call_once(_, _))
        .WillByDefault(
            [](cplat_once_flag *flag, cplat_once_fn fn)
            {
                if (cplat_atomic_load_i32(&flag->state, CPLAT_MEMORY_ORDER_RELAXED) == 0)
                {
                    cplat_atomic_store_i32(&flag->state, 1, CPLAT_MEMORY_ORDER_RELAXED);
                    fn();
                    cplat_atomic_store_i32(&flag->state, 2, CPLAT_MEMORY_ORDER_RELAXED);
                }
            });
    ON_CALL(mock_cplat, cplat_local_lock_create(_))
        .WillByDefault(
            [](cplat_local_lock **lock)
            {
                *lock = reinterpret_cast<cplat_local_lock *>(static_cast<uintptr_t>(0x3100));
                return CPLAT_OK;
            });
    ON_CALL(mock_cplat, cplat_local_lock_lock(_)).WillByDefault(Return());
    ON_CALL(mock_cplat, cplat_local_lock_unlock(_)).WillByDefault(Return());
    ON_CALL(mock_cplat, cplat_local_lock_dispose(_)).WillByDefault(Return());
    ON_CALL(mock_cplat, cplat_local_rwlock_create(_))
        .WillByDefault(
            [](cplat_local_rwlock **lock)
            {
                *lock = reinterpret_cast<cplat_local_rwlock *>(static_cast<uintptr_t>(0x3200));
                return CPLAT_OK;
            });
    ON_CALL(mock_cplat, cplat_local_rwlock_lock_shared(_)).WillByDefault(Return());
    ON_CALL(mock_cplat, cplat_local_rwlock_timed_lock_shared(_, _)).WillByDefault(Return(CPLAT_OK));
    ON_CALL(mock_cplat, cplat_local_rwlock_lock_exclusive(_)).WillByDefault(Return());
    ON_CALL(mock_cplat, cplat_local_rwlock_unlock_shared(_)).WillByDefault(Return());
    ON_CALL(mock_cplat, cplat_local_rwlock_unlock_exclusive(_)).WillByDefault(Return());
    ON_CALL(mock_cplat, cplat_local_rwlock_dispose(_)).WillByDefault(Return());
}

#endif /* TRACE_SYNC_MOCK_H */
