#include <testfw.h>
#include <mock_cplat.h>

#define DEFINE_SYNC_RET(rettype, name, call_args, ...) \
    rettype delegate_real_##name(__VA_ARGS__) \
    { \
        static auto real_fn = reinterpret_cast<decltype(&name)>(resolveSharedSymbolOrExit(kLibCplatName, #name)); \
        return real_fn call_args; \
    } \
    MOCK_WEAK_IMPL(rettype, name, __VA_ARGS__) \
    { \
        rettype mock_ret; \
        if (_mock_cplat != nullptr) \
        { \
            mock_ret = _mock_cplat->name call_args; \
        } \
        else \
        { \
            mock_ret = delegate_real_##name call_args; \
        } \
        if (getTraceLevel() > TRACE_NONE) \
        { \
            printf("  > %s -> %d\n", __func__, (int)mock_ret); \
        } \
        return mock_ret; \
    }

#define DEFINE_SYNC_VOID(name, call_args, ...) \
    void delegate_real_##name(__VA_ARGS__) \
    { \
        static auto real_fn = reinterpret_cast<decltype(&name)>(resolveSharedSymbolOrExit(kLibCplatName, #name)); \
        real_fn call_args; \
    } \
    MOCK_WEAK_IMPL(void, name, __VA_ARGS__) \
    { \
        if (_mock_cplat != nullptr) \
        { \
            _mock_cplat->name call_args; \
        } \
        else \
        { \
            delegate_real_##name call_args; \
        } \
        if (getTraceLevel() > TRACE_NONE) \
        { \
            printf("  > %s\n", __func__); \
        } \
    }

DEFINE_SYNC_RET(int, cplat_local_lock_create, (mtx), cplat_local_lock **mtx)
DEFINE_SYNC_RET(int, cplat_local_lock_lock, (mtx, timeout_ms), cplat_local_lock *mtx, int timeout_ms)
DEFINE_SYNC_RET(int, cplat_local_lock_try_lock, (mtx), cplat_local_lock *mtx)
DEFINE_SYNC_RET(int, cplat_local_lock_unlock, (mtx), cplat_local_lock *mtx)
DEFINE_SYNC_VOID(cplat_local_lock_dispose, (mtx), cplat_local_lock *mtx)

DEFINE_SYNC_RET(int, cplat_condvar_create, (cv), cplat_condvar **cv)
DEFINE_SYNC_RET(int, cplat_condvar_wait, (cv, mtx, timeout_ms), cplat_condvar *cv, cplat_local_lock *mtx,
                int timeout_ms)
DEFINE_SYNC_RET(int, cplat_condvar_signal, (cv), cplat_condvar *cv)
DEFINE_SYNC_RET(int, cplat_condvar_broadcast, (cv), cplat_condvar *cv)
DEFINE_SYNC_VOID(cplat_condvar_dispose, (cv), cplat_condvar *cv)

DEFINE_SYNC_RET(int, cplat_local_rwlock_create, (rwlock), cplat_local_rwlock **rwlock)
DEFINE_SYNC_RET(int, cplat_local_rwlock_lock_shared, (rwlock, timeout_ms), cplat_local_rwlock *rwlock, int timeout_ms)
DEFINE_SYNC_RET(int, cplat_local_rwlock_try_lock_shared, (rwlock), cplat_local_rwlock *rwlock)
DEFINE_SYNC_RET(int, cplat_local_rwlock_lock_exclusive, (rwlock, timeout_ms), cplat_local_rwlock *rwlock,
                int timeout_ms)
DEFINE_SYNC_RET(int, cplat_local_rwlock_try_lock_exclusive, (rwlock), cplat_local_rwlock *rwlock)
DEFINE_SYNC_RET(int, cplat_local_rwlock_unlock_shared, (rwlock), cplat_local_rwlock *rwlock)
DEFINE_SYNC_RET(int, cplat_local_rwlock_unlock_exclusive, (rwlock), cplat_local_rwlock *rwlock)
DEFINE_SYNC_VOID(cplat_local_rwlock_dispose, (rwlock), cplat_local_rwlock *rwlock)

DEFINE_SYNC_RET(int, cplat_thread_create, (thread, func, arg), cplat_thread **thread, cplat_thread_fn func, void *arg)
DEFINE_SYNC_RET(int, cplat_thread_join, (thread, timeout_ms), cplat_thread *thread, int timeout_ms)
DEFINE_SYNC_VOID(cplat_thread_detach, (thread), cplat_thread *thread)

DEFINE_SYNC_RET(int, cplat_interprocess_lock_open, (identity, lock), const char *identity,
                cplat_interprocess_lock **lock)
DEFINE_SYNC_RET(int, cplat_interprocess_lock_import_descriptor, (descriptor, descriptor_size, lock),
                const void *descriptor, size_t descriptor_size, cplat_interprocess_lock **lock)
DEFINE_SYNC_RET(int, cplat_interprocess_lock_export_descriptor, (lock, descriptor, descriptor_size),
                const cplat_interprocess_lock *lock, void *descriptor, size_t *descriptor_size)
DEFINE_SYNC_RET(int, cplat_interprocess_lock_lock, (lock, timeout_ms), cplat_interprocess_lock *lock, int timeout_ms)
DEFINE_SYNC_RET(int, cplat_interprocess_lock_try_lock, (lock), cplat_interprocess_lock *lock)
DEFINE_SYNC_RET(int, cplat_interprocess_lock_unlock, (lock), cplat_interprocess_lock *lock)
DEFINE_SYNC_VOID(cplat_interprocess_lock_dispose, (lock), cplat_interprocess_lock *lock)

DEFINE_SYNC_RET(int, cplat_interprocess_rwlock_open, (identity, lock), const char *identity,
                cplat_interprocess_rwlock **lock)
DEFINE_SYNC_RET(int, cplat_interprocess_rwlock_import_descriptor, (descriptor, descriptor_size, lock),
                const void *descriptor, size_t descriptor_size, cplat_interprocess_rwlock **lock)
DEFINE_SYNC_RET(int, cplat_interprocess_rwlock_export_descriptor, (lock, descriptor, descriptor_size),
                const cplat_interprocess_rwlock *lock, void *descriptor, size_t *descriptor_size)
DEFINE_SYNC_RET(int, cplat_interprocess_rwlock_lock_shared, (lock, timeout_ms), cplat_interprocess_rwlock *lock,
                int timeout_ms)
DEFINE_SYNC_RET(int, cplat_interprocess_rwlock_try_lock_shared, (lock), cplat_interprocess_rwlock *lock)
DEFINE_SYNC_RET(int, cplat_interprocess_rwlock_lock_exclusive, (lock, timeout_ms), cplat_interprocess_rwlock *lock,
                int timeout_ms)
DEFINE_SYNC_RET(int, cplat_interprocess_rwlock_try_lock_exclusive, (lock), cplat_interprocess_rwlock *lock)
DEFINE_SYNC_RET(int, cplat_interprocess_rwlock_unlock, (lock), cplat_interprocess_rwlock *lock)
DEFINE_SYNC_VOID(cplat_interprocess_rwlock_dispose, (lock), cplat_interprocess_rwlock *lock)
