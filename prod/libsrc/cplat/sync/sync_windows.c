/**
 *******************************************************************************
 *  @file           sync_windows.c
 *  @brief          Windows 向けのスレッドと同期プリミティブを実装します。
 *******************************************************************************
 */

#include <cplat/base/platform.h>
#include <cplat/crt/stdlib.h>

#if defined(PLATFORM_WINDOWS)

    #include <process.h>
    #include <stdlib.h>
    #include <string.h>

    #include <cplat/base/windows_sdk.h>
    #include <cplat/sync/sync.h>
    #include <cplat/sync/sync_descriptor.h>
    #include <cplat/win32/win32.h>

    #define INTERPROCESS_SYNC_RANGE_LOW  1U
    #define INTERPROCESS_SYNC_RANGE_HIGH 0U

struct cplat_local_lock
{
    SRWLOCK native;
};

struct cplat_condvar
{
    CONDITION_VARIABLE native;
};

struct cplat_local_rwlock
{
    CRITICAL_SECTION mutex;        /* 読み書きの保持状態と待機者数を保護する */
    CONDITION_VARIABLE readers_cv; /* 読み取り待機用の条件変数 */
    CONDITION_VARIABLE writers_cv; /* 書き込み待機用の条件変数 */
    unsigned int active_readers;   /* 共有ロックを保持しているリーダー数 */
    unsigned int waiting_writers;  /* 排他ロックを待機しているライター数 */
    int writer_active;             /* 排他ロックの保持中は 1 */
};

struct cplat_thread
{
    HANDLE native;
};

struct cplat_interprocess_lock
{
    HANDLE handle;
    char *identity;
    int locked;
};

struct cplat_interprocess_rwlock
{
    HANDLE handle;
    char *identity;
    int locked;
};

struct cplat_thread_start_ctx
{
    cplat_thread_fn func;
    void *arg;
};

/*
 * _beginthreadex() のスレッド起動用関数。起動コンテキストを解放した後に利用者の関数を実行する。
 * 戻り値はスレッドの終了コード (常に 0) である。
 */
static unsigned __stdcall thread_start_proc(void *opaque)
{
    struct cplat_thread_start_ctx *ctx = (struct cplat_thread_start_ctx *)opaque;
    cplat_thread_fn func = ctx->func;
    void *arg = ctx->arg;

    cplat_free(ctx);
    func(arg);

    return 0U;
}

/*
 * Windows API の待機状態コードを cplat の結果コードへ変換する。
 * WAIT_OBJECT_0 を CPLAT_OK、WAIT_TIMEOUT を CPLAT_ERR_TIMEOUT、それ以外を CPLAT_ERR_UNKNOWN へ変換する。
 */
static int wait_status_to_result(DWORD status)
{
    if (status == WAIT_OBJECT_0)
    {
        return CPLAT_OK;
    }

    if (status == WAIT_TIMEOUT)
    {
        return CPLAT_ERR_TIMEOUT;
    }

    return CPLAT_ERR_UNKNOWN;
}

static char *dup_string(const char *s)
{
    size_t len;
    char *copy;

    len = strlen(s);
    copy = (char *)cplat_malloc(len + 1U);
    if (copy != NULL)
    {
        memcpy(copy, s, len + 1U);
    }

    return copy;
}

/*
 * 指定された識別文字列でプロセス間リーダー ライター ロック用のファイルを開き、管理構造体を生成する。
 * 成功時は CPLAT_OK、引数不正時は CPLAT_ERR_INVALID_ARGUMENT、
 * ファイル オープンまたはメモリ確保失敗時は CPLAT_ERR_UNKNOWN を返す。
 */
static int app_lock_open_identity(const char *identity, cplat_interprocess_rwlock **lock)
{
    cplat_interprocess_rwlock *new_lock;
    HANDLE handle;
    char *identity_copy;

    if (identity == NULL || identity[0] == '\0' || lock == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    handle = CreateFileU(identity, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                         NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == INVALID_HANDLE_VALUE)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    identity_copy = dup_string(identity);
    if (identity_copy == NULL)
    {
        CloseHandle(handle);
        return CPLAT_ERR_UNKNOWN;
    }

    new_lock = (cplat_interprocess_rwlock *)cplat_calloc(1, sizeof(*new_lock));
    if (new_lock == NULL)
    {
        cplat_free(identity_copy);
        CloseHandle(handle);
        return CPLAT_ERR_UNKNOWN;
    }

    new_lock->handle = handle;
    new_lock->identity = identity_copy;
    *lock = new_lock;

    return CPLAT_OK;
}

/*
 * 指定された識別文字列でプロセス間ロック用のファイルを開き、管理構造体を生成する。
 * 成功時は CPLAT_OK、引数不正時は CPLAT_ERR_INVALID_ARGUMENT、
 * ファイル オープンまたはメモリ確保失敗時は CPLAT_ERR_UNKNOWN を返す。
 */
static int interprocess_lock_open_identity(const char *identity, cplat_interprocess_lock **lock)
{
    cplat_interprocess_lock *new_lock;
    HANDLE handle;
    char *identity_copy;

    if (identity == NULL || identity[0] == '\0' || lock == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    handle = CreateFileU(identity, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                         NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == INVALID_HANDLE_VALUE)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    identity_copy = dup_string(identity);
    if (identity_copy == NULL)
    {
        CloseHandle(handle);
        return CPLAT_ERR_UNKNOWN;
    }

    new_lock = (cplat_interprocess_lock *)cplat_calloc(1, sizeof(*new_lock));
    if (new_lock == NULL)
    {
        cplat_free(identity_copy);
        CloseHandle(handle);
        return CPLAT_ERR_UNKNOWN;
    }

    new_lock->handle = handle;
    new_lock->identity = identity_copy;
    *lock = new_lock;

    return CPLAT_OK;
}

/*
 * プロセス間リーダー ライター ロックのファイル ロックを指定された待機期限で取得する。
 * 成功時は CPLAT_OK、引数不正時は CPLAT_ERR_INVALID_ARGUMENT、待機なしで取得できない場合は CPLAT_ERR_BUSY、
 * 期限切れ時は CPLAT_ERR_TIMEOUT、その他の失敗時は CPLAT_ERR_UNKNOWN を返す。
 */
static int app_lock_take(cplat_interprocess_rwlock *lock, DWORD flags, int timeout_ms)
{
    ULONGLONG deadline;
    OVERLAPPED ov = {0};

    if (lock == NULL || lock->locked)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    deadline = GetTickCount64() + (ULONGLONG)timeout_ms;

    /*
     * LockFileEx() には期限付き同期待機がないため、LOCKFILE_FAIL_IMMEDIATELY で試行し、
     * 取得できなければ 1 [ms] 待機して期限まで繰り返す。
     * see: https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-lockfileex
     */
    do
    {
        if (LockFileEx(lock->handle, flags | LOCKFILE_FAIL_IMMEDIATELY, 0, INTERPROCESS_SYNC_RANGE_LOW,
                       INTERPROCESS_SYNC_RANGE_HIGH, &ov))
        {
            lock->locked = 1;
            return CPLAT_OK;
        }

        if (timeout_ms == CPLAT_SYNC_NO_WAIT)
        {
            return CPLAT_ERR_BUSY;
        }

        if (GetLastError() != ERROR_LOCK_VIOLATION)
        {
            return CPLAT_ERR_UNKNOWN;
        }

        Sleep(1); /* 1 [ms] */
    } while (timeout_ms == CPLAT_SYNC_WAIT_FOREVER || GetTickCount64() < deadline);

    return CPLAT_ERR_TIMEOUT;
}

/*
 * プロセス間ロックのファイル ロックを排他モードかつ指定された待機期限で取得する。
 * 成功時は CPLAT_OK、引数不正時は CPLAT_ERR_INVALID_ARGUMENT、待機なしで取得できない場合は CPLAT_ERR_BUSY、
 * 期限切れ時は CPLAT_ERR_TIMEOUT、その他の失敗時は CPLAT_ERR_UNKNOWN を返す。
 */
static int interprocess_lock_take(cplat_interprocess_lock *lock, int timeout_ms)
{
    ULONGLONG deadline;
    OVERLAPPED ov = {0};

    if (lock == NULL || lock->locked)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    deadline = GetTickCount64() + (ULONGLONG)timeout_ms;

    /*
     * LockFileEx() には期限付き同期待機がないため、LOCKFILE_FAIL_IMMEDIATELY で試行し、
     * 取得できなければ 1 [ms] 待機して期限まで繰り返す。
     * see: https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-lockfileex
     */
    do
    {
        if (LockFileEx(lock->handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0,
                       INTERPROCESS_SYNC_RANGE_LOW, INTERPROCESS_SYNC_RANGE_HIGH, &ov))
        {
            lock->locked = 1;
            return CPLAT_OK;
        }

        if (timeout_ms == CPLAT_SYNC_NO_WAIT)
        {
            return CPLAT_ERR_BUSY;
        }

        if (GetLastError() != ERROR_LOCK_VIOLATION)
        {
            return CPLAT_ERR_UNKNOWN;
        }

        Sleep(1); /* 1 [ms] */
    } while (timeout_ms == CPLAT_SYNC_WAIT_FOREVER || GetTickCount64() < deadline);

    return CPLAT_ERR_TIMEOUT;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_local_lock_create(cplat_local_lock **mtx)
{
    cplat_local_lock *new_mtx;

    if (mtx == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    new_mtx = (cplat_local_lock *)cplat_calloc(1, sizeof(*new_mtx));
    if (new_mtx == NULL)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    InitializeSRWLock(&new_mtx->native);
    *mtx = new_mtx;

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_local_lock_lock(cplat_local_lock *mtx, int timeout_ms)
{
    ULONGLONG deadline;

    if (mtx == NULL || timeout_ms < 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (timeout_ms == CPLAT_SYNC_WAIT_FOREVER)
    {
        AcquireSRWLockExclusive(&mtx->native);
        return CPLAT_OK;
    }

    deadline = GetTickCount64() + (ULONGLONG)timeout_ms;

    /*
     * SRWLOCK には期限付き待機がないため、TryAcquireSRWLockExclusive() で試行し、
     * 取得できなければ 1 [ms] 待機して期限まで繰り返す。
     * see: https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-tryacquiresrwlockexclusive
     */
    do
    {
        if (TryAcquireSRWLockExclusive(&mtx->native))
        {
            return CPLAT_OK;
        }

        if (timeout_ms == CPLAT_SYNC_NO_WAIT)
        {
            return CPLAT_ERR_BUSY;
        }

        Sleep(1); /* 1 [ms] */
    } while (GetTickCount64() < deadline);

    return CPLAT_ERR_TIMEOUT;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_local_lock_try_lock(cplat_local_lock *mtx)
{
    return cplat_local_lock_lock(mtx, CPLAT_SYNC_NO_WAIT);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_local_lock_unlock(cplat_local_lock *mtx)
{
    if (mtx == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    ReleaseSRWLockExclusive(&mtx->native);

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_local_lock_dispose(cplat_local_lock *mtx)
{
    cplat_free(mtx);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_condvar_create(cplat_condvar **cv)
{
    cplat_condvar *new_cv;

    if (cv == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    new_cv = (cplat_condvar *)cplat_calloc(1, sizeof(*new_cv));
    if (new_cv == NULL)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    InitializeConditionVariable(&new_cv->native);
    *cv = new_cv;

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_condvar_wait(cplat_condvar *cv, cplat_local_lock *mtx, int timeout_ms)
{
    DWORD wait_ms;

    if (cv == NULL || mtx == NULL || timeout_ms < 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (timeout_ms == CPLAT_SYNC_WAIT_FOREVER)
    {
        wait_ms = INFINITE;
    }
    else
    {
        wait_ms = (DWORD)timeout_ms;
    }

    if (SleepConditionVariableSRW(&cv->native, &mtx->native, wait_ms, 0))
    {
        return CPLAT_OK;
    }

    if (GetLastError() == ERROR_TIMEOUT)
    {
        return CPLAT_ERR_TIMEOUT;
    }
    else
    {
        return CPLAT_ERR_UNKNOWN;
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_condvar_signal(cplat_condvar *cv)
{
    if (cv == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    WakeConditionVariable(&cv->native);

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_condvar_broadcast(cplat_condvar *cv)
{
    if (cv == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    WakeAllConditionVariable(&cv->native);

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_condvar_dispose(cplat_condvar *cv)
{
    cplat_free(cv);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_local_rwlock_create(cplat_local_rwlock **rwlock)
{
    cplat_local_rwlock *new_lock;

    if (rwlock == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    new_lock = (cplat_local_rwlock *)cplat_calloc(1, sizeof(*new_lock));
    if (new_lock == NULL)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    InitializeCriticalSection(&new_lock->mutex);
    InitializeConditionVariable(&new_lock->readers_cv);
    InitializeConditionVariable(&new_lock->writers_cv);
    *rwlock = new_lock;

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_local_rwlock_lock_shared(cplat_local_rwlock *rwlock, int timeout_ms)
{
    DWORD wait_ms;

    if (rwlock == NULL || timeout_ms < 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (timeout_ms == CPLAT_SYNC_WAIT_FOREVER)
    {
        wait_ms = INFINITE;
    }
    else
    {
        wait_ms = (DWORD)timeout_ms;
    }

    EnterCriticalSection(&rwlock->mutex);

    /* 排他ロックが保持中、または待機中の書き込みが存在する間、条件変数で待機する。 */
    while (rwlock->writer_active || rwlock->waiting_writers > 0)
    {
        if (timeout_ms == CPLAT_SYNC_NO_WAIT)
        {
            LeaveCriticalSection(&rwlock->mutex);
            return CPLAT_ERR_BUSY;
        }

        if (!SleepConditionVariableCS(&rwlock->readers_cv, &rwlock->mutex, wait_ms))
        {
            LeaveCriticalSection(&rwlock->mutex);

            if (GetLastError() == ERROR_TIMEOUT)
            {
                return CPLAT_ERR_TIMEOUT;
            }
            else
            {
                return CPLAT_ERR_UNKNOWN;
            }
        }
    }

    rwlock->active_readers++;
    LeaveCriticalSection(&rwlock->mutex);

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_local_rwlock_try_lock_shared(cplat_local_rwlock *rwlock)
{
    return cplat_local_rwlock_lock_shared(rwlock, CPLAT_SYNC_NO_WAIT);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_local_rwlock_lock_exclusive(cplat_local_rwlock *rwlock, int timeout_ms)
{
    DWORD wait_ms;

    if (rwlock == NULL || timeout_ms < 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (timeout_ms == CPLAT_SYNC_WAIT_FOREVER)
    {
        wait_ms = INFINITE;
    }
    else
    {
        wait_ms = (DWORD)timeout_ms;
    }

    EnterCriticalSection(&rwlock->mutex);
    rwlock->waiting_writers++;

    /* 排他ロックまたは共有ロックが保持されている間、条件変数で待機する。 */
    while (rwlock->writer_active || rwlock->active_readers > 0)
    {
        if (timeout_ms == CPLAT_SYNC_NO_WAIT)
        {
            rwlock->waiting_writers--;
            LeaveCriticalSection(&rwlock->mutex);
            return CPLAT_ERR_BUSY;
        }

        if (!SleepConditionVariableCS(&rwlock->writers_cv, &rwlock->mutex, wait_ms))
        {
            rwlock->waiting_writers--;
            LeaveCriticalSection(&rwlock->mutex);

            if (GetLastError() == ERROR_TIMEOUT)
            {
                return CPLAT_ERR_TIMEOUT;
            }
            else
            {
                return CPLAT_ERR_UNKNOWN;
            }
        }
    }

    rwlock->waiting_writers--;
    rwlock->writer_active = 1;
    LeaveCriticalSection(&rwlock->mutex);

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_local_rwlock_try_lock_exclusive(cplat_local_rwlock *rwlock)
{
    return cplat_local_rwlock_lock_exclusive(rwlock, CPLAT_SYNC_NO_WAIT);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_local_rwlock_unlock_shared(cplat_local_rwlock *rwlock)
{
    if (rwlock == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    EnterCriticalSection(&rwlock->mutex);

    if (rwlock->active_readers == 0)
    {
        LeaveCriticalSection(&rwlock->mutex);
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    rwlock->active_readers--;

    /* 最後の読み取りが解除され、待機中の書き込みが存在する場合は書き込みスレッドを起床させる。 */
    if (rwlock->active_readers == 0 && rwlock->waiting_writers > 0)
    {
        WakeConditionVariable(&rwlock->writers_cv);
    }

    LeaveCriticalSection(&rwlock->mutex);

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_local_rwlock_unlock_exclusive(cplat_local_rwlock *rwlock)
{
    if (rwlock == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    EnterCriticalSection(&rwlock->mutex);

    if (!rwlock->writer_active)
    {
        LeaveCriticalSection(&rwlock->mutex);
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    rwlock->writer_active = 0;

    /* 待機中の書き込みが存在すれば 1 スレッドを起こし、存在しなければ待機中の全読み取りを起こす。 */
    if (rwlock->waiting_writers > 0)
    {
        WakeConditionVariable(&rwlock->writers_cv);
    }
    else
    {
        WakeAllConditionVariable(&rwlock->readers_cv);
    }

    LeaveCriticalSection(&rwlock->mutex);

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_local_rwlock_dispose(cplat_local_rwlock *rwlock)
{
    if (rwlock != NULL)
    {
        DeleteCriticalSection(&rwlock->mutex);
        cplat_free(rwlock);
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_thread_create(cplat_thread **thread, cplat_thread_fn func, void *arg)
{
    struct cplat_thread_start_ctx *ctx;
    cplat_thread *new_thread;
    uintptr_t handle;

    if (thread == NULL || func == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    new_thread = (cplat_thread *)cplat_calloc(1, sizeof(*new_thread));
    ctx = (struct cplat_thread_start_ctx *)cplat_malloc(sizeof(*ctx));
    if (new_thread == NULL || ctx == NULL)
    {
        cplat_free(new_thread);
        cplat_free(ctx);
        return CPLAT_ERR_UNKNOWN;
    }

    ctx->func = func;
    ctx->arg = arg;
    handle = _beginthreadex(NULL, 0U, thread_start_proc, ctx, 0U, NULL);
    if (handle == 0U)
    {
        cplat_free(ctx);
        cplat_free(new_thread);
        return CPLAT_ERR_UNKNOWN;
    }

    new_thread->native = (HANDLE)handle;
    *thread = new_thread;

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_thread_join(cplat_thread *thread, int timeout_ms)
{
    DWORD status;
    DWORD wait_ms;

    if (thread == NULL || timeout_ms < 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (timeout_ms == CPLAT_SYNC_WAIT_FOREVER)
    {
        wait_ms = INFINITE;
    }
    else
    {
        wait_ms = (DWORD)timeout_ms;
    }

    status = WaitForSingleObject(thread->native, wait_ms);
    if (status == WAIT_OBJECT_0)
    {
        CloseHandle(thread->native);
        cplat_free(thread);
        return CPLAT_OK;
    }

    return wait_status_to_result(status);
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_thread_detach(cplat_thread *thread)
{
    if (thread != NULL)
    {
        CloseHandle(thread->native);
        cplat_free(thread);
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_interprocess_lock_open(const char *identity, cplat_interprocess_lock **lock)
{
    return interprocess_lock_open_identity(identity, lock);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_interprocess_lock_export_descriptor(const cplat_interprocess_lock *lock, void *descriptor,
                                              size_t *descriptor_size)
{
    if (lock == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    return cplat_internal_interprocess_sync_descriptor_export(lock->identity, CPLAT_INTERPROCESS_SYNC_KIND_LOCK,
                                                              (uint8_t)CPLAT_INTERPROCESS_SYNC_BACKEND_LOCK_FILE,
                                                              descriptor, descriptor_size);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_interprocess_lock_import_descriptor(const void *descriptor, size_t descriptor_size,
                                              cplat_interprocess_lock **lock)
{
    char *identity;
    int result;

    if (lock == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    result = cplat_internal_interprocess_sync_descriptor_import(
        descriptor, descriptor_size, CPLAT_INTERPROCESS_SYNC_KIND_LOCK,
        (uint8_t)CPLAT_INTERPROCESS_SYNC_BACKEND_LOCK_FILE, &identity);
    if (result != CPLAT_OK)
    {
        return result;
    }

    result = interprocess_lock_open_identity(identity, lock);
    cplat_free(identity);

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_interprocess_lock_lock(cplat_interprocess_lock *lock, int timeout_ms)
{
    if (timeout_ms < 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    return interprocess_lock_take(lock, timeout_ms);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_interprocess_lock_try_lock(cplat_interprocess_lock *lock)
{
    return cplat_interprocess_lock_lock(lock, CPLAT_SYNC_NO_WAIT);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_interprocess_lock_unlock(cplat_interprocess_lock *lock)
{
    OVERLAPPED ov = {0};

    if (lock == NULL || !lock->locked)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (!UnlockFileEx(lock->handle, 0, INTERPROCESS_SYNC_RANGE_LOW, INTERPROCESS_SYNC_RANGE_HIGH, &ov))
    {
        return CPLAT_ERR_UNKNOWN;
    }

    lock->locked = 0;

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_interprocess_lock_dispose(cplat_interprocess_lock *lock)
{
    if (lock != NULL)
    {
        if (lock->locked)
        {
            (void)cplat_interprocess_lock_unlock(lock);
        }

        CloseHandle(lock->handle);
        cplat_free(lock->identity);
        cplat_free(lock);
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_interprocess_rwlock_open(const char *identity, cplat_interprocess_rwlock **lock)
{
    return app_lock_open_identity(identity, lock);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_interprocess_rwlock_export_descriptor(const cplat_interprocess_rwlock *lock, void *descriptor,
                                                size_t *descriptor_size)
{
    if (lock == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    return cplat_internal_interprocess_sync_descriptor_export(lock->identity, CPLAT_INTERPROCESS_SYNC_KIND_RWLOCK,
                                                              (uint8_t)CPLAT_INTERPROCESS_SYNC_BACKEND_LOCK_FILE,
                                                              descriptor, descriptor_size);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_interprocess_rwlock_import_descriptor(const void *descriptor, size_t descriptor_size,
                                                cplat_interprocess_rwlock **lock)
{
    char *identity;
    int result;

    if (lock == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    result = cplat_internal_interprocess_sync_descriptor_import(
        descriptor, descriptor_size, CPLAT_INTERPROCESS_SYNC_KIND_RWLOCK,
        (uint8_t)CPLAT_INTERPROCESS_SYNC_BACKEND_LOCK_FILE, &identity);
    if (result != CPLAT_OK)
    {
        return result;
    }

    result = app_lock_open_identity(identity, lock);
    cplat_free(identity);

    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_interprocess_rwlock_lock_shared(cplat_interprocess_rwlock *lock, int timeout_ms)
{
    if (timeout_ms < 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    return app_lock_take(lock, 0U, timeout_ms);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_interprocess_rwlock_try_lock_shared(cplat_interprocess_rwlock *lock)
{
    return cplat_interprocess_rwlock_lock_shared(lock, CPLAT_SYNC_NO_WAIT);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_interprocess_rwlock_lock_exclusive(cplat_interprocess_rwlock *lock, int timeout_ms)
{
    if (timeout_ms < 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    return app_lock_take(lock, LOCKFILE_EXCLUSIVE_LOCK, timeout_ms);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_interprocess_rwlock_try_lock_exclusive(cplat_interprocess_rwlock *lock)
{
    return cplat_interprocess_rwlock_lock_exclusive(lock, CPLAT_SYNC_NO_WAIT);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_interprocess_rwlock_unlock(cplat_interprocess_rwlock *lock)
{
    OVERLAPPED ov = {0};

    if (lock == NULL || !lock->locked)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (!UnlockFileEx(lock->handle, 0, INTERPROCESS_SYNC_RANGE_LOW, INTERPROCESS_SYNC_RANGE_HIGH, &ov))
    {
        return CPLAT_ERR_UNKNOWN;
    }

    lock->locked = 0;

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_interprocess_rwlock_dispose(cplat_interprocess_rwlock *lock)
{
    if (lock != NULL)
    {
        if (lock->locked)
        {
            (void)cplat_interprocess_rwlock_unlock(lock);
        }

        CloseHandle(lock->handle);
        cplat_free(lock->identity);
        cplat_free(lock);
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_call_once(cplat_once_flag *flag, void (*func)(void))
{
    if (flag == NULL || func == NULL)
    {
        return;
    }

    int32_t expected = 0;

    /* 未実行状態 (0) から実行中 (1) への遷移に成功したスレッドのみが初期化関数を実行する。 */
    if (cplat_atomic_compare_exchange_i32(&flag->state, &expected, 1, CPLAT_MEMORY_ORDER_ACQ_REL))
    {
        func();
        cplat_atomic_store_i32(&flag->state, 2, CPLAT_MEMORY_ORDER_RELEASE);
        return;
    }

    /* 他スレッドが初期化を完了 (2) するまでスレッド スイッチで待機する。 */
    while (cplat_atomic_load_i32(&flag->state, CPLAT_MEMORY_ORDER_ACQUIRE) != 2)
    {
        SwitchToThread();
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_thread_yield(void)
{
    (void)SwitchToThread();
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_sleep_ms(int ms)
{
    if (ms <= 0)
    {
        return;
    }

    Sleep((DWORD)ms);
}

#endif /* PLATFORM_WINDOWS */
