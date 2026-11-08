/**
 *******************************************************************************
 *  @file           sync_linux.c
 *  @brief          Linux 向けのスレッドと同期プリミティブを実装します。
 *******************************************************************************
 */

#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif /* _GNU_SOURCE */

#include <cplat/base/platform.h>
#include <cplat/crt/string.h>
#include <cplat/crt/stdlib.h>

#if defined(PLATFORM_LINUX)

    #include <errno.h>
    #include <fcntl.h>
    #include <pthread.h>
    #include <sched.h>
    #include <stdlib.h>
    #include <string.h>
    #include <sys/file.h>
    #include <time.h>
    #include <unistd.h>

    #include <cplat/runtime/abort.h>
    #include <cplat/sync/sync.h>
    #include <cplat/sync/sync_descriptor.h>

struct cplat_local_lock
{
    pthread_mutex_t native;
};

struct cplat_condvar
{
    pthread_cond_t native;
};

/*
 * pthread_rwlock_timedrdlock() などは期限を CLOCK_REALTIME で判定するため、システム時刻の変更で待機時間が変動する。
 * 期限付き待機を CLOCK_MONOTONIC で行えるよう、pthread_rwlock_t は使わずに mutex と条件変数で読み書きロックを構成する。
 * see: https://man7.org/linux/man-pages/man3/pthread_rwlock_timedrdlock.3p.html
 */
struct cplat_local_rwlock
{
    pthread_mutex_t mutex; /* 読み書きの保持状態と待機者数を保護する */
    pthread_cond_t readers_cv;
    pthread_cond_t writers_cv;
    unsigned int active_readers;
    unsigned int waiting_writers;
    int writer_active; /* 排他ロックの保持中は 1 */
    int pad;           /* 明示的アラインメント */
};

struct cplat_thread
{
    pthread_t native;
};

struct cplat_interprocess_lock
{
    char *identity;
    int fd;
    int locked;
};

struct cplat_interprocess_rwlock
{
    char *identity;
    int fd;
    int locked;
};

struct cplat_thread_start_ctx
{
    cplat_thread_fn func;
    void *arg;
};

static int monotonic_ms(uint64_t *milliseconds_out)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    *milliseconds_out = ((uint64_t)ts.tv_sec * 1000U) + ((uint64_t)ts.tv_nsec / 1000000U);

    return CPLAT_OK;
}

static int monotonic_deadline(struct timespec *abs_ts, int timeout_ms)
{
    struct timespec now;

    /* 呼び出し元で負値チェック済みのため、unsigned int として演算する。 */
    unsigned int ms = (unsigned int)timeout_ms;

    /* pthread_cond_timedwait() に渡す絶対時刻を、cond_init_monotonic() と同じ CLOCK_MONOTONIC で算出する。 */
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    now.tv_sec += (time_t)(ms / 1000U);
    now.tv_nsec += (long)((ms % 1000U) * 1000000UL);
    if (now.tv_nsec >= 1000000000L)
    {
        now.tv_sec++;
        now.tv_nsec -= 1000000000L;
    }

    *abs_ts = now;

    return CPLAT_OK;
}

/*
 * pthread_cond_timedwait() の期限は、既定ではシステム時刻 (CLOCK_REALTIME) を基準に判定される。
 * システム時刻の変更で待機時間が変動しないよう、CLOCK_MONOTONIC を指定して条件変数を初期化する。
 * see: https://man7.org/linux/man-pages/man3/pthread_condattr_getclock.3p.html
 */
static int cond_init_monotonic(pthread_cond_t *cond)
{
    pthread_condattr_t attr;
    int rc;

    rc = pthread_condattr_init(&attr);
    if (rc != 0)
    {
        return rc;
    }

    rc = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    if (rc == 0)
    {
        rc = pthread_cond_init(cond, &attr);
    }

    pthread_condattr_destroy(&attr);

    return rc;
}

/*
 * local rwlock の内部 mutex と条件変数は生成時に初期化済みであり、lock と unlock は同じ関数内で対になる。
 * 以降の内部同期ヘルパーでの失敗は内部状態の破壊を意味し、続行すると排他制御が崩れるため異常終了させる。
 */
static void rwlock_state_lock(cplat_local_rwlock *rwlock)
{
    if (pthread_mutex_lock(&rwlock->mutex) != 0)
    {
        CPLAT_ABORT("local rwlock の内部 mutex を取得できません");
    }
}

static void rwlock_state_unlock(cplat_local_rwlock *rwlock)
{
    if (pthread_mutex_unlock(&rwlock->mutex) != 0)
    {
        CPLAT_ABORT("local rwlock の内部 mutex を解放できません");
    }
}

static void rwlock_wake_one(pthread_cond_t *cond)
{
    if (pthread_cond_signal(cond) != 0)
    {
        CPLAT_ABORT("local rwlock の待機者を起床できません");
    }
}

static void rwlock_wake_all(pthread_cond_t *cond)
{
    if (pthread_cond_broadcast(cond) != 0)
    {
        CPLAT_ABORT("local rwlock の待機者を起床できません");
    }
}

static int map_wait_rc(int rc)
{
    if (rc == 0)
    {
        return CPLAT_OK;
    }

    if (rc == ETIMEDOUT)
    {
        return CPLAT_ERR_TIMEOUT;
    }

    if (rc == EBUSY || rc == EAGAIN)
    {
        return CPLAT_ERR_BUSY;
    }

    return CPLAT_ERR_UNKNOWN;
}

static void *thread_start_proc(void *opaque)
{
    struct cplat_thread_start_ctx *ctx = (struct cplat_thread_start_ctx *)opaque;
    cplat_thread_fn func = ctx->func;
    void *arg = ctx->arg;

    /*
     * 起動情報 ctx の所有権は新スレッドへ移っている。
     * func() が戻らずにスレッドが終了してもリークしないよう、引数を退避して呼び出し前に解放する。
     */
    cplat_free(ctx);
    func(arg);

    return NULL;
}

static int open_lock_file(const char *identity, int *fd_out, char **identity_out)
{
    int fd = open(identity, O_RDWR | O_CREAT | O_CLOEXEC, 0666);
    if (fd < 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    char *identity_copy = cplat_strdup(identity);
    if (identity_copy == NULL)
    {
        close(fd);
        return CPLAT_ERR_UNKNOWN;
    }

    *fd_out = fd;
    *identity_out = identity_copy;

    return CPLAT_OK;
}

static int app_lock_open_identity(const char *identity, cplat_interprocess_rwlock **lock)
{
    int fd;
    char *identity_copy;

    if (identity == NULL || identity[0] == '\0' || lock == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    int ret = open_lock_file(identity, &fd, &identity_copy);
    if (ret != CPLAT_OK)
    {
        return ret;
    }

    cplat_interprocess_rwlock *new_lock = (cplat_interprocess_rwlock *)cplat_calloc(1, sizeof(*new_lock));
    if (new_lock == NULL)
    {
        cplat_free(identity_copy);
        close(fd);
        return CPLAT_ERR_UNKNOWN;
    }

    new_lock->fd = fd;
    new_lock->identity = identity_copy;
    *lock = new_lock;

    return CPLAT_OK;
}

static int interprocess_lock_open_identity(const char *identity, cplat_interprocess_lock **lock)
{
    int fd;
    char *identity_copy;

    if (identity == NULL || identity[0] == '\0' || lock == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    int ret = open_lock_file(identity, &fd, &identity_copy);
    if (ret != CPLAT_OK)
    {
        return ret;
    }

    cplat_interprocess_lock *new_lock = (cplat_interprocess_lock *)cplat_calloc(1, sizeof(*new_lock));
    if (new_lock == NULL)
    {
        cplat_free(identity_copy);
        close(fd);
        return CPLAT_ERR_UNKNOWN;
    }

    new_lock->fd = fd;
    new_lock->identity = identity_copy;
    *lock = new_lock;

    return CPLAT_OK;
}

static int take_lock_file(const int fd, const int operation, const int timeout_ms)
{
    uint64_t deadline;
    uint64_t now;

    /*
     * flock() は待機中にシグナルを受けると EINTR で失敗するため、取得できるまで呼び直す。
     * see: https://man7.org/linux/man-pages/man2/flock.2.html
     */
    if (timeout_ms == CPLAT_SYNC_WAIT_FOREVER)
    {
        while (flock(fd, operation) != 0)
        {
            if (errno != EINTR)
            {
                return CPLAT_ERR_UNKNOWN;
            }
        }

        return CPLAT_OK;
    }

    /*
     * flock() には期限付きの待機がないため、LOCK_NB で試行し、取得できなければ 1 [ms] 待機して期限まで繰り返す。
     * 呼び出し元で timeout_ms < 0 を拒否済みのため、ここでの timeout_ms は 0 以上の有限値である。
     * see: https://man7.org/linux/man-pages/man2/flock.2.html
     */
    if (monotonic_ms(&now) != CPLAT_OK)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    deadline = now + (uint64_t)timeout_ms;

    do
    {
        if (flock(fd, operation | LOCK_NB) == 0)
        {
            return CPLAT_OK;
        }

        if (errno != EWOULDBLOCK && errno != EINTR)
        {
            return CPLAT_ERR_UNKNOWN;
        }

        if (timeout_ms == CPLAT_SYNC_NO_WAIT)
        {
            return CPLAT_ERR_BUSY;
        }

        {
            struct timespec sleep_ts = {0, 1000000L}; /* 1 [ms] */
            nanosleep(&sleep_ts, NULL);
        }

        if (monotonic_ms(&now) != CPLAT_OK)
        {
            return CPLAT_ERR_UNKNOWN;
        }
    } while (now < deadline);

    return CPLAT_ERR_TIMEOUT;
}

static int app_lock_take(cplat_interprocess_rwlock *lock, int operation, int timeout_ms)
{
    if (lock == NULL || lock->locked)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    int ret = take_lock_file(lock->fd, operation, timeout_ms);
    if (ret == CPLAT_OK)
    {
        lock->locked = 1;
    }

    return ret;
}

static int interprocess_lock_take(cplat_interprocess_lock *lock, int timeout_ms)
{
    if (lock == NULL || lock->locked)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    int ret = take_lock_file(lock->fd, LOCK_EX, timeout_ms);
    if (ret == CPLAT_OK)
    {
        lock->locked = 1;
    }

    return ret;
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

    if (pthread_mutex_init(&new_mtx->native, NULL) != 0)
    {
        cplat_free(new_mtx);
        return CPLAT_ERR_UNKNOWN;
    }

    *mtx = new_mtx;

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_local_lock_lock(cplat_local_lock *mtx, int timeout_ms)
{
    uint64_t deadline;
    uint64_t now;

    if (mtx == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (timeout_ms < 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (timeout_ms == CPLAT_SYNC_WAIT_FOREVER)
    {
        return map_wait_rc(pthread_mutex_lock(&mtx->native));
    }

    if (timeout_ms == CPLAT_SYNC_NO_WAIT)
    {
        return map_wait_rc(pthread_mutex_trylock(&mtx->native));
    }

    /*
     * pthread_mutex_timedlock() は期限を CLOCK_REALTIME で判定するため使わず、
     * CLOCK_MONOTONIC の期限まで pthread_mutex_trylock() を 1 [ms] 間隔で繰り返す。
     * 上で timeout_ms < 0 を拒否済みのため、ここでの timeout_ms は 0 以上の有限値である。
     * see: https://man7.org/linux/man-pages/man3/pthread_mutex_timedlock.3p.html
     */
    if (monotonic_ms(&now) != CPLAT_OK)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    deadline = now + (uint64_t)timeout_ms;

    do
    {
        int rc = pthread_mutex_trylock(&mtx->native);
        if (rc == 0)
        {
            return CPLAT_OK;
        }

        if (rc != EBUSY)
        {
            return CPLAT_ERR_UNKNOWN;
        }

        {
            struct timespec sleep_ts = {0, 1000000L}; /* 1 [ms] */
            nanosleep(&sleep_ts, NULL);
        }

        if (monotonic_ms(&now) != CPLAT_OK)
        {
            return CPLAT_ERR_UNKNOWN;
        }
    } while (now < deadline);

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

    return map_wait_rc(pthread_mutex_unlock(&mtx->native));
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_local_lock_dispose(cplat_local_lock *mtx)
{
    if (mtx != NULL)
    {
        pthread_mutex_destroy(&mtx->native);
        cplat_free(mtx);
    }
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

    if (cond_init_monotonic(&new_cv->native) != 0)
    {
        cplat_free(new_cv);
        return CPLAT_ERR_UNKNOWN;
    }

    *cv = new_cv;

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_condvar_wait(cplat_condvar *cv, cplat_local_lock *mtx, int timeout_ms)
{
    struct timespec abs_ts;

    if (cv == NULL || mtx == NULL || timeout_ms < 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (timeout_ms == CPLAT_SYNC_WAIT_FOREVER)
    {
        return map_wait_rc(pthread_cond_wait(&cv->native, &mtx->native));
    }

    if (monotonic_deadline(&abs_ts, timeout_ms) != CPLAT_OK)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    return map_wait_rc(pthread_cond_timedwait(&cv->native, &mtx->native, &abs_ts));
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_condvar_signal(cplat_condvar *cv)
{
    if (cv == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    return map_wait_rc(pthread_cond_signal(&cv->native));
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_condvar_broadcast(cplat_condvar *cv)
{
    if (cv == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    return map_wait_rc(pthread_cond_broadcast(&cv->native));
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_condvar_dispose(cplat_condvar *cv)
{
    if (cv != NULL)
    {
        pthread_cond_destroy(&cv->native);
        cplat_free(cv);
    }
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

    if (pthread_mutex_init(&new_lock->mutex, NULL) != 0 || cond_init_monotonic(&new_lock->readers_cv) != 0 ||
        cond_init_monotonic(&new_lock->writers_cv) != 0)
    {
        pthread_mutex_destroy(&new_lock->mutex);
        pthread_cond_destroy(&new_lock->readers_cv);
        pthread_cond_destroy(&new_lock->writers_cv);
        cplat_free(new_lock);
        return CPLAT_ERR_UNKNOWN;
    }

    *rwlock = new_lock;

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_local_rwlock_lock_shared(cplat_local_rwlock *rwlock, int timeout_ms)
{
    struct timespec abs_ts;
    int rc = 0;

    if (rwlock == NULL || timeout_ms < 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (timeout_ms != CPLAT_SYNC_WAIT_FOREVER)
    {
        if (monotonic_deadline(&abs_ts, timeout_ms) != CPLAT_OK)
        {
            return CPLAT_ERR_UNKNOWN;
        }
    }

    rwlock_state_lock(rwlock);

    /*
     * 書き込み中に加え、書き込み待ちがいる間も新しい読み取りを待たせることで、
     * 読み取りが連続しても書き込みロックを取得できなくなる状態を防ぐ。
     */
    while (rwlock->writer_active || rwlock->waiting_writers > 0)
    {
        if (timeout_ms == CPLAT_SYNC_NO_WAIT)
        {
            rwlock_state_unlock(rwlock);
            return CPLAT_ERR_BUSY;
        }

        if (timeout_ms == CPLAT_SYNC_WAIT_FOREVER)
        {
            rc = pthread_cond_wait(&rwlock->readers_cv, &rwlock->mutex);
        }
        else
        {
            rc = pthread_cond_timedwait(&rwlock->readers_cv, &rwlock->mutex, &abs_ts);
        }

        if (rc == ETIMEDOUT)
        {
            rwlock_state_unlock(rwlock);
            return CPLAT_ERR_TIMEOUT;
        }

        if (rc != 0)
        {
            rwlock_state_unlock(rwlock);
            return CPLAT_ERR_UNKNOWN;
        }
    }

    rwlock->active_readers++;
    rwlock_state_unlock(rwlock);

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
    struct timespec abs_ts;
    int rc = 0;

    if (rwlock == NULL || timeout_ms < 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (timeout_ms != CPLAT_SYNC_WAIT_FOREVER)
    {
        if (monotonic_deadline(&abs_ts, timeout_ms) != CPLAT_OK)
        {
            return CPLAT_ERR_UNKNOWN;
        }
    }

    rwlock_state_lock(rwlock);

    /*
     * 待機に入る前に waiting_writers を増やし、この時点から新しい読み取りを止める。
     * 待機を抜けるすべての経路で waiting_writers を減算する。
     */
    rwlock->waiting_writers++;

    while (rwlock->writer_active || rwlock->active_readers > 0)
    {
        if (timeout_ms == CPLAT_SYNC_NO_WAIT)
        {
            rwlock->waiting_writers--;
            rwlock_state_unlock(rwlock);
            return CPLAT_ERR_BUSY;
        }

        if (timeout_ms == CPLAT_SYNC_WAIT_FOREVER)
        {
            rc = pthread_cond_wait(&rwlock->writers_cv, &rwlock->mutex);
        }
        else
        {
            rc = pthread_cond_timedwait(&rwlock->writers_cv, &rwlock->mutex, &abs_ts);
        }

        if (rc == ETIMEDOUT)
        {
            rwlock->waiting_writers--;
            rwlock_state_unlock(rwlock);
            return CPLAT_ERR_TIMEOUT;
        }

        if (rc != 0)
        {
            rwlock->waiting_writers--;
            rwlock_state_unlock(rwlock);
            return CPLAT_ERR_UNKNOWN;
        }
    }

    rwlock->waiting_writers--;
    rwlock->writer_active = 1;
    rwlock_state_unlock(rwlock);

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

    rwlock_state_lock(rwlock);

    if (rwlock->active_readers == 0)
    {
        rwlock_state_unlock(rwlock);
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    rwlock->active_readers--;

    /* 最後の読み取りスレッドが抜けた時点で、待機中の書き込みスレッドがいれば 1 スレッドだけ起床させる。 */
    if (rwlock->active_readers == 0 && rwlock->waiting_writers > 0)
    {
        rwlock_wake_one(&rwlock->writers_cv);
    }

    rwlock_state_unlock(rwlock);

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_local_rwlock_unlock_exclusive(cplat_local_rwlock *rwlock)
{
    if (rwlock == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    rwlock_state_lock(rwlock);

    if (!rwlock->writer_active)
    {
        rwlock_state_unlock(rwlock);
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    rwlock->writer_active = 0;

    /*
     * 書き込み待ちがいれば優先して 1 スレッドだけ起床させ、いない場合に限り、
     * 同時にロックを取得できる読み取り待ちをすべて起床させる。
     */
    if (rwlock->waiting_writers > 0)
    {
        rwlock_wake_one(&rwlock->writers_cv);
    }
    else
    {
        rwlock_wake_all(&rwlock->readers_cv);
    }

    rwlock_state_unlock(rwlock);

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_local_rwlock_dispose(cplat_local_rwlock *rwlock)
{
    if (rwlock != NULL)
    {
        pthread_mutex_destroy(&rwlock->mutex);
        pthread_cond_destroy(&rwlock->readers_cv);
        pthread_cond_destroy(&rwlock->writers_cv);
        cplat_free(rwlock);
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_thread_create(cplat_thread **thread, cplat_thread_fn func, void *arg)
{
    struct cplat_thread_start_ctx *ctx;
    cplat_thread *new_thread;
    int rc;

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
    rc = pthread_create(&new_thread->native, NULL, thread_start_proc, ctx);
    if (rc != 0)
    {
        cplat_free(ctx);
        cplat_free(new_thread);
        return CPLAT_ERR_UNKNOWN;
    }

    *thread = new_thread;

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_thread_join(cplat_thread *thread, int timeout_ms)
{
    struct timespec abs_ts;
    int rc;

    if (thread == NULL || timeout_ms < 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (timeout_ms == CPLAT_SYNC_WAIT_FOREVER)
    {
        rc = pthread_join(thread->native, NULL);
    }
    else
    {
        /*
         * pthread_timedjoin_np() は期限を CLOCK_REALTIME の絶対時刻で受け取るため使わず、
         * CLOCK_MONOTONIC の期限まで pthread_tryjoin_np() を 1 [ms] 間隔で繰り返す。
         * 上で timeout_ms < 0 を拒否済みのため、ここでの timeout_ms は 0 以上の有限値である。
         * see: https://man7.org/linux/man-pages/man3/pthread_tryjoin_np.3.html
         */
        uint64_t now;

        if (monotonic_ms(&now) != CPLAT_OK)
        {
            return CPLAT_ERR_UNKNOWN;
        }

        uint64_t deadline = now + (uint64_t)timeout_ms;

        do
        {
            rc = pthread_tryjoin_np(thread->native, NULL);
            if (rc == 0)
            {
                break;
            }

            if (rc != EBUSY)
            {
                break;
            }

            if (timeout_ms == CPLAT_SYNC_NO_WAIT)
            {
                break;
            }

            abs_ts.tv_sec = 0;
            abs_ts.tv_nsec = 1000000L; /* 1 [ms] */
            nanosleep(&abs_ts, NULL);

            if (monotonic_ms(&now) != CPLAT_OK)
            {
                return CPLAT_ERR_UNKNOWN;
            }
        } while (now < deadline);

        /*
         * 期限までに終了しなかった場合に加え、CPLAT_SYNC_NO_WAIT 指定時に未終了だった場合もタイムアウトとして返す。
         * ロック系の API と異なり、ヘッダーの契約には CPLAT_ERR_BUSY がない。
         */
        if (rc == EBUSY)
        {
            rc = ETIMEDOUT;
        }
    }

    if (rc == ETIMEDOUT)
    {
        return CPLAT_ERR_TIMEOUT;
    }

    if (rc != 0)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    cplat_free(thread);

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_thread_detach(cplat_thread *thread)
{
    if (thread != NULL)
    {
        pthread_detach(thread->native);
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
    if (lock == NULL || !lock->locked)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (flock(lock->fd, LOCK_UN) != 0)
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

        close(lock->fd);
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

    return app_lock_take(lock, LOCK_SH, timeout_ms);
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

    return app_lock_take(lock, LOCK_EX, timeout_ms);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_interprocess_rwlock_try_lock_exclusive(cplat_interprocess_rwlock *lock)
{
    return cplat_interprocess_rwlock_lock_exclusive(lock, CPLAT_SYNC_NO_WAIT);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_interprocess_rwlock_unlock(cplat_interprocess_rwlock *lock)
{
    if (lock == NULL || !lock->locked)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (flock(lock->fd, LOCK_UN) != 0)
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

        close(lock->fd);
        cplat_free(lock->identity);
        cplat_free(lock);
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_call_once(cplat_once_flag *flag, void (*func)(void))
{
    int32_t expected = 0;

    if (flag == NULL || func == NULL)
    {
        return;
    }

    /*
     * state を 0 (未実行) から 1 (実行中) へ遷移させた 1 スレッドだけが func() を実行し、完了後に 2 (実行済み) を書き込む。
     * 他のスレッドは func() の副作用が見えるよう、ACQUIRE 読み込みで 2 になるまで待機する。
     */
    if (cplat_atomic_compare_exchange_i32(&flag->state, &expected, 1, CPLAT_MEMORY_ORDER_ACQ_REL))
    {
        func();
        cplat_atomic_store_i32(&flag->state, 2, CPLAT_MEMORY_ORDER_RELEASE);
        return;
    }

    while (cplat_atomic_load_i32(&flag->state, CPLAT_MEMORY_ORDER_ACQUIRE) != 2)
    {
        sched_yield();
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_thread_yield(void)
{
    (void)sched_yield();
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_sleep_ms(int ms)
{
    struct timespec req;
    struct timespec rem;
    unsigned int ums;

    if (ms <= 0)
    {
        return;
    }

    ums = (unsigned int)ms;
    req.tv_sec = (time_t)(ums / 1000U);
    req.tv_nsec = (long)((ums % 1000U) * 1000000UL);

    while (nanosleep(&req, &rem) == -1 && errno == EINTR)
    {
        req = rem;
    }
}

#elif defined(PLATFORM_WINDOWS) && defined(COMPILER_MSVC)
    #pragma warning(disable : 4206)
#endif
