/**
 *******************************************************************************
 *  @file           socket_watch_linux.c
 *  @brief          Linux の解除可能なソケット待機を実装します。
 *
 *******************************************************************************
 */

#include <cplat/base/platform.h>

#if defined(PLATFORM_LINUX)

    #include <errno.h>
    #include <fcntl.h>
    #include <netinet/in.h>
    #include <poll.h>
    #include <stdatomic.h>
    #include <stdint.h>
    #include <string.h>
    #include <sys/eventfd.h>
    #include <sys/socket.h>
    #include <unistd.h>

    #include <cplat/base/error_internal.h>
    #include <cplat/base/result.h>
    #include <cplat/base/wait.h>
    #include <cplat/clock/clock.h>
    #include <cplat/crt/stdlib.h>
    #include <cplat/net/socket.h>

#define SOCKET_WATCH_CANCEL_CHECK_MS 50U

struct cplat_socket_wait_cancel
{
    int event_fd;
    atomic_int signaled;
};

struct cplat_socket_watch
{
    cplat_socket sock;
};

/**
 *  @brief          解除通知が保持されているかを acquire で取得します。
 */
static int cancel_is_signaled(const cplat_socket_wait_cancel *cancel)
{
    return (cancel != NULL) && (atomic_load_explicit(&cancel->signaled, memory_order_acquire) != 0);
}

/**
 *  @brief          指定した待機期限の残り時間を計算します。
 *  @param[in]      deadline_ms 絶対期限。無期限は 0。
 *  @param[in]      timeout_ms  元のタイムアウト。
 *  @param[out]     remaining_ms 残り時間。
 *  @return         期限まで待機できる場合は 1、期限切れは 0、時計の失敗は -1 を返します。
 */
static int deadline_remaining(const uint64_t deadline_ms, const int timeout_ms, uint64_t *remaining_ms)
{
    uint64_t now;

    if (timeout_ms == CPLAT_WAIT_FOREVER)
    {
        *remaining_ms = UINT64_MAX;
        return 1;
    }

    if (cplat_clock_get_monotonic_ms(&now) != CPLAT_OK)
    {
        return -1;
    }

    if (now >= deadline_ms)
    {
        *remaining_ms = 0U;
        return 0;
    }

    *remaining_ms = deadline_ms - now;
    return 1;
}

/**
 *  @brief          指定した解除状態と絶対期限で poll を実行します。
 *  @param[in]      fds         poll の対象。
 *  @param[in]      count       対象数。
 *  @param[in]      cancel      解除オブジェクト。NULL 可。
 *  @param[in]      timeout_ms  全体の期限。
 *  @param[in]      deadline_ms 正のタイムアウトに対する絶対期限。
 *  @return         poll の結果、タイムアウトは 0、失敗は -1 を返します。
 *
 *  解除通知の検出と 50 [ms] ごとの確認を poll の EINTR 再試行より優先します。
 *  see: https://man7.org/linux/man-pages/man2/eventfd.2.html
 */
static int poll_with_cancel(struct pollfd *fds, const nfds_t count, const cplat_socket_wait_cancel *cancel,
                            const int timeout_ms, const uint64_t deadline_ms)
{
    int no_wait_attempted = 0;

    for (;;)
    {
        uint64_t remaining_ms;
        int remaining_result;
        int wait_ms;
        int poll_result;

        if (cancel_is_signaled(cancel))
        {
            errno = ECANCELED;
            return -2;
        }

        if (timeout_ms == CPLAT_NO_WAIT)
        {
            if (no_wait_attempted != 0)
            {
                return 0;
            }
            remaining_ms = 0U;
        }
        else
        {
            remaining_result = deadline_remaining(deadline_ms, timeout_ms, &remaining_ms);
            if (remaining_result < 0)
            {
                errno = EIO;
                return -1;
            }
            if (remaining_result == 0)
            {
                return 0;
            }
        }

        wait_ms = (timeout_ms == CPLAT_WAIT_FOREVER) ? -1 :
                  (remaining_ms > (uint64_t)INT_MAX ? INT_MAX : (int)remaining_ms);
        if (cancel != NULL && (wait_ms < 0 || wait_ms > (int)SOCKET_WATCH_CANCEL_CHECK_MS))
        {
            wait_ms = (int)SOCKET_WATCH_CANCEL_CHECK_MS;
        }

        poll_result = poll(fds, count, wait_ms);
        no_wait_attempted = 1;
        if (cancel_is_signaled(cancel))
        {
            errno = ECANCELED;
            return -2;
        }
        if (poll_result < 0 && errno == EINTR)
        {
            continue;
        }
        if (poll_result < 0)
        {
            return -1;
        }
        if (poll_result > 0)
        {
            return poll_result;
        }

        remaining_result = deadline_remaining(deadline_ms, timeout_ms, &remaining_ms);
        if (remaining_result < 0)
        {
            errno = EIO;
            return -1;
        }
        if (remaining_result == 0)
        {
            return 0;
        }
        if (timeout_ms == CPLAT_NO_WAIT) return 0;
    }
}

/**
 *  @brief          ソケットを受信または送信可能になるまで待ちます。
 *  @param[in]      watch       対象の登録。
 *  @param[in]      cancel      解除オブジェクト。NULL 可。
 *  @param[in]      timeout_ms  全体の期限。
 *  @param[in]      events      poll へ渡す方向。
 *  @param[out]     terminal_out 終了またはエラーで起床したことを格納します。
 *  @param[out]     detail_out  エラー詳細。NULL 可。
 *  @return         共通結果コードを返します。
 */
static int wait_one_with_deadline(const cplat_socket_watch *watch, const cplat_socket_wait_cancel *cancel,
                                  const int timeout_ms, const uint64_t deadline_ms, const short events,
                                  int *terminal_out, cplat_error *detail_out)
{
    struct pollfd fds[2];
    nfds_t count = 0U;
    int poll_result;

    if ((watch == NULL) || (watch->sock == CPLAT_INVALID_SOCKET) || (timeout_ms < 0) || (terminal_out == NULL))
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }

    *terminal_out = 0;
    if (cancel != NULL)
    {
        fds[count].fd = cancel->event_fd;
        fds[count].events = POLLIN;
        fds[count].revents = 0;
        ++count;
    }
    fds[count].fd = (int)watch->sock;
    fds[count].events = events;
    fds[count].revents = 0;
    ++count;

    for (;;)
    {
        poll_result = poll_with_cancel(fds, count, cancel, timeout_ms, deadline_ms);
        if (poll_result == -2)
        {
            return cplat_internal_error_report_socket_errno_as(detail_out, ECANCELED, CPLAT_ERR_CANCELED);
        }
        if (poll_result < 0)
        {
            const int saved_errno = errno;
            return cplat_internal_error_report_socket_errno(detail_out, saved_errno);
        }
        if (cancel != NULL && ((fds[0].revents & (POLLERR | POLLNVAL)) != 0))
        {
            return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
        }
        if (cancel != NULL && ((fds[0].revents & POLLIN) != 0 || cancel_is_signaled(cancel)))
        {
            return cplat_internal_error_report_socket_errno_as(detail_out, ECANCELED, CPLAT_ERR_CANCELED);
        }
        if (poll_result == 0)
        {
            return cplat_internal_error_report_socket_errno_as(detail_out, ETIMEDOUT, CPLAT_ERR_TIMEOUT);
        }

        {
            const short revents = fds[count - 1U].revents;
            if ((revents & POLLNVAL) != 0)
            {
                return cplat_internal_error_report_socket_errno_as(detail_out, EBADF, CPLAT_ERR_INVALID_ARGUMENT);
            }
            if ((revents & (POLLERR | POLLHUP)) != 0)
            {
                *terminal_out = 1;
            }
            if ((revents & (events | POLLERR | POLLHUP)) != 0)
            {
                return cplat_internal_error_report_success(detail_out);
            }
        }

        fds[count - 1U].revents = 0;
    }
}

/**
 *  @brief          相対タイムアウトから絶対期限を一度だけ求めてソケットを待ちます。
 */
static int wait_one(const cplat_socket_watch *watch, const cplat_socket_wait_cancel *cancel, const int timeout_ms,
                    const short events, int *terminal_out, cplat_error *detail_out)
{
    uint64_t deadline_ms = 0U;
    uint64_t now;

    if (timeout_ms < 0)
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }
    if (timeout_ms != CPLAT_WAIT_FOREVER && timeout_ms != CPLAT_NO_WAIT)
    {
        if (cplat_clock_get_monotonic_ms(&now) != CPLAT_OK)
        {
            return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
        }
        deadline_ms = now + (uint64_t)timeout_ms;
    }

    return wait_one_with_deadline(watch, cancel, timeout_ms, deadline_ms, events, terminal_out, detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_wait_cancel_create(cplat_socket_wait_cancel **cancel_out, cplat_error *detail_out)
{
    cplat_socket_wait_cancel *cancel;
    int event_fd;

    if (cancel_out == NULL)
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }
    *cancel_out = NULL;

    cancel = (cplat_socket_wait_cancel *)cplat_malloc(sizeof(*cancel));
    if (cancel == NULL)
    {
        return cplat_internal_error_report_errno_as(detail_out, ENOMEM, CPLAT_ERR_OUT_OF_MEMORY);
    }

    /* eventfd は非ブロッキングで作り、待機側は読み取らず通知状態を保持します。 */
    event_fd = eventfd(0U, EFD_NONBLOCK | EFD_CLOEXEC);
    if (event_fd < 0)
    {
        const int saved_errno = errno;
        cplat_free(cancel);
        return cplat_internal_error_report_socket_errno(detail_out, saved_errno);
    }

    cancel->event_fd = event_fd;
    atomic_init(&cancel->signaled, 0);
    *cancel_out = cancel;
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_wait_cancel_signal(cplat_socket_wait_cancel *cancel, cplat_error *detail_out)
{
    const uint64_t value = 1U;
    ssize_t written;

    if (cancel == NULL)
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }

    atomic_store_explicit(&cancel->signaled, 1, memory_order_release);
    written = write(cancel->event_fd, &value, sizeof(value));
    if (written < 0 && errno != EAGAIN)
    {
        /* 状態はすでに公開済みであり、50 [ms] ごとの確認で待機側が解除を検出します。 */
    }
    else if (written >= 0 && (size_t)written != sizeof(value))
    {
        /* eventfd は 8 [byte] 単位で読み書きします。短い書き込みでも状態を取り消しません。 */
    }

    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_wait_cancel_reset(cplat_socket_wait_cancel *cancel, cplat_error *detail_out)
{
    uint64_t value;
    ssize_t read_count;

    if (cancel == NULL)
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }

    read_count = read(cancel->event_fd, &value, sizeof(value));
    if (read_count < 0 && errno != EAGAIN)
    {
        return cplat_internal_error_report_socket_errno(detail_out, errno);
    }
    if (read_count >= 0 && (size_t)read_count != sizeof(value))
    {
        return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
    }

    atomic_store_explicit(&cancel->signaled, 0, memory_order_release);
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_socket_wait_cancel_dispose(cplat_socket_wait_cancel *cancel)
{
    cplat_error saved;

    if (cancel == NULL)
    {
        return;
    }

    cplat_error_get_last(&saved);
    /* 解放処理で失敗しても利用側に取れる手段がないため、戻り値は明示的に破棄します。 */
    (void)close(cancel->event_fd);
    cplat_free(cancel);
    cplat_error_set_last(&saved);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_watch_attach(const cplat_socket sock, cplat_socket_watch **watch_out, cplat_error *detail_out)
{
    cplat_socket_watch *watch;
    int flags;

    if (sock == CPLAT_INVALID_SOCKET || watch_out == NULL)
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }
    *watch_out = NULL;

    watch = (cplat_socket_watch *)cplat_malloc(sizeof(*watch));
    if (watch == NULL)
    {
        return cplat_internal_error_report_errno_as(detail_out, ENOMEM, CPLAT_ERR_OUT_OF_MEMORY);
    }

    flags = fcntl((int)sock, F_GETFL);
    if (flags < 0)
    {
        const int saved_errno = errno;
        cplat_free(watch);
        return cplat_internal_error_report_socket_errno(detail_out, saved_errno);
    }
    if ((flags & O_NONBLOCK) == 0 && fcntl((int)sock, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        const int saved_errno = errno;
        cplat_free(watch);
        return cplat_internal_error_report_socket_errno(detail_out, saved_errno);
    }

    watch->sock = sock;
    *watch_out = watch;
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_socket_watch_detach(cplat_socket_watch *watch)
{
    cplat_error saved;

    if (watch == NULL)
    {
        return;
    }

    cplat_error_get_last(&saved);
    cplat_free(watch);
    cplat_error_set_last(&saved);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_watch_wait_readable_multi(cplat_socket_watch *const *watches, const size_t count,
                                          cplat_socket_wait_cancel *cancel, const int timeout_ms,
                                          unsigned char *ready_out, cplat_error *detail_out)
{
    struct pollfd fds[CPLAT_SOCKET_WATCH_WAIT_MAX + 1U];
    size_t watch_indexes[CPLAT_SOCKET_WATCH_WAIT_MAX];
    size_t used = 0U;
    size_t socket_count = 0U;
    uint64_t deadline_ms = 0U;
    uint64_t now;
    int poll_result;
    int any_ready = 0;

    if (watches == NULL || count == 0U || count > CPLAT_SOCKET_WATCH_WAIT_MAX || ready_out == NULL || timeout_ms < 0)
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }
    for (size_t index = 0U; index < count; ++index)
    {
        ready_out[index] = 0U;
        if (watches[index] != NULL)
        {
            if (watches[index]->sock == CPLAT_INVALID_SOCKET)
            {
                return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
            }
            fds[used].fd = (int)watches[index]->sock;
            fds[used].events = POLLIN;
            fds[used].revents = 0;
            watch_indexes[socket_count] = index;
            ++used;
            ++socket_count;
        }
    }
    if (socket_count == 0U && cancel == NULL && timeout_ms == CPLAT_WAIT_FOREVER)
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }
    if (cancel != NULL)
    {
        memmove(&fds[1], &fds[0], used * sizeof(fds[0]));
        fds[0].fd = cancel->event_fd;
        fds[0].events = POLLIN;
        fds[0].revents = 0;
        ++used;
    }
    if (timeout_ms != CPLAT_WAIT_FOREVER && timeout_ms != CPLAT_NO_WAIT)
    {
        if (cplat_clock_get_monotonic_ms(&now) != CPLAT_OK)
        {
            return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
        }
        deadline_ms = now + (uint64_t)timeout_ms;
    }

    poll_result = poll_with_cancel(fds, (nfds_t)used, cancel, timeout_ms, deadline_ms);
    if (poll_result == -2 || cancel_is_signaled(cancel) ||
        (cancel != NULL && (fds[0].revents & POLLIN) != 0))
    {
        return cplat_internal_error_report_socket_errno_as(detail_out, ECANCELED, CPLAT_ERR_CANCELED);
    }
    if (poll_result < 0)
    {
        return cplat_internal_error_report_socket_errno(detail_out, errno);
    }
    if (poll_result == 0)
    {
        return cplat_internal_error_report_socket_errno_as(detail_out, ETIMEDOUT, CPLAT_ERR_TIMEOUT);
    }

    if (cancel != NULL && (fds[0].revents & (POLLERR | POLLNVAL)) != 0)
    {
        return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
    }
    for (size_t index = 0U; index < socket_count; ++index)
    {
        const nfds_t fd_index = (nfds_t)index + (cancel != NULL ? 1U : 0U);
        const short revents = fds[fd_index].revents;
        if ((revents & POLLNVAL) != 0)
        {
            return cplat_internal_error_report_socket_errno_as(detail_out, EBADF, CPLAT_ERR_INVALID_ARGUMENT);
        }
        if ((revents & (POLLIN | POLLERR | POLLHUP)) != 0)
        {
            ready_out[watch_indexes[index]] = 1U;
            any_ready = 1;
        }
    }

    if (cancel_is_signaled(cancel))
    {
        memset(ready_out, 0, count);
        return cplat_internal_error_report_socket_errno_as(detail_out, ECANCELED, CPLAT_ERR_CANCELED);
    }
    return any_ready ? cplat_internal_error_report_success(detail_out) :
                       cplat_internal_error_report_socket_errno_as(detail_out, ETIMEDOUT, CPLAT_ERR_TIMEOUT);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_watch_wait_writable(cplat_socket_watch *watch, cplat_socket_wait_cancel *cancel,
                                     const int timeout_ms, cplat_error *detail_out)
{
    int terminal;
    return wait_one(watch, cancel, timeout_ms, POLLOUT, &terminal, detail_out);
}

/**
 *  @brief          登録済みソケットへ送信を 1 回実行し、送信可能の競合状態を報告します。
 */
static int send_once(cplat_socket_watch *watch, const void *buf, const size_t len, size_t *sent_out,
                     cplat_error *detail_out)
{
    ssize_t sent;

    do
    {
        sent = send((int)watch->sock, buf, len, MSG_NOSIGNAL);
    } while (sent < 0 && errno == EINTR);

    if (sent < 0)
    {
        return cplat_internal_error_report_socket_errno(detail_out, errno);
    }
    if ((size_t)sent > len)
    {
        return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
    }
    *sent_out = (size_t)sent;
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_watch_send(cplat_socket_watch *watch, const void *buf, const size_t len, size_t *sent_out,
                            cplat_error *detail_out)
{
    if (watch == NULL || watch->sock == CPLAT_INVALID_SOCKET || buf == NULL || sent_out == NULL ||
        len > CPLAT_SOCKET_MAX_TRANSFER)
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }
    *sent_out = 0U;
    return send_once(watch, buf, len, sent_out, detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_watch_sendto(cplat_socket_watch *watch, const void *buf, const size_t len,
                              const cplat_ipv4_endpoint *endpoint, size_t *sent_out, cplat_error *detail_out)
{
    struct sockaddr_in native;
    ssize_t sent;

    if (watch == NULL || watch->sock == CPLAT_INVALID_SOCKET || buf == NULL || endpoint == NULL || sent_out == NULL ||
        len > CPLAT_SOCKET_MAX_TRANSFER)
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }
    *sent_out = 0U;
    memset(&native, 0, sizeof(native));
    native.sin_family = AF_INET;
    memcpy(&native.sin_addr.s_addr, &endpoint->address, sizeof(native.sin_addr.s_addr));
    memcpy(&native.sin_port, &endpoint->port, sizeof(native.sin_port));
    do
    {
        sent = sendto((int)watch->sock, buf, len, MSG_NOSIGNAL, (const struct sockaddr *)&native,
                      (socklen_t)sizeof(native));
    } while (sent < 0 && errno == EINTR);
    if (sent < 0)
    {
        return cplat_internal_error_report_socket_errno(detail_out, errno);
    }
    if ((size_t)sent > len)
    {
        return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
    }
    *sent_out = (size_t)sent;
    return cplat_internal_error_report_success(detail_out);
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_watch_sendto_multi(cplat_socket_watch *watch, const cplat_socket_datagram *datagrams,
                                    const size_t count, size_t *sent_count_out, cplat_error *detail_out)
{
    struct mmsghdr messages[CPLAT_SOCKET_SENDTO_MULTI_MAX];
    struct iovec vectors[CPLAT_SOCKET_SENDTO_MULTI_MAX];
    struct sockaddr_in natives[CPLAT_SOCKET_SENDTO_MULTI_MAX];
    size_t index;
    size_t full_count;
    int sent;

    if (sent_count_out != NULL)
    {
        *sent_count_out = 0U;
    }
    if (watch == NULL || watch->sock == CPLAT_INVALID_SOCKET || datagrams == NULL || sent_count_out == NULL ||
        count == 0U || count > CPLAT_SOCKET_SENDTO_MULTI_MAX)
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }
    for (index = 0U; index < count; index++)
    {
        if (datagrams[index].buf == NULL || datagrams[index].endpoint == NULL ||
            datagrams[index].len > CPLAT_SOCKET_MAX_TRANSFER)
        {
            return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
        }
    }

    memset(messages, 0, sizeof(messages[0]) * count);
    memset(natives, 0, sizeof(natives[0]) * count);
    for (index = 0U; index < count; index++)
    {
        natives[index].sin_family = AF_INET;
        memcpy(&natives[index].sin_addr.s_addr, &datagrams[index].endpoint->address,
               sizeof(natives[index].sin_addr.s_addr));
        memcpy(&natives[index].sin_port, &datagrams[index].endpoint->port, sizeof(natives[index].sin_port));
        /* iovec は書き込み可能なポインターを持つが、送信では buf を変更しない。 */
        vectors[index].iov_base = (void *)(uintptr_t)datagrams[index].buf;
        vectors[index].iov_len = datagrams[index].len;
        messages[index].msg_hdr.msg_name = &natives[index];
        messages[index].msg_hdr.msg_namelen = (socklen_t)sizeof(natives[index]);
        messages[index].msg_hdr.msg_iov = &vectors[index];
        messages[index].msg_hdr.msg_iovlen = 1U;
    }

    /*
     * sendmmsg() は、途中のデータグラムで失敗すると、それまでに送れた数を返し、失敗の理由を返さない。
     * 先頭で失敗した場合だけ -1 と errno を返す。
     * see: https://man7.org/linux/man-pages/man2/sendmmsg.2.html
     */
    do
    {
        sent = sendmmsg((int)watch->sock, messages, (unsigned int)count, MSG_NOSIGNAL);
    } while (sent < 0 && errno == EINTR);
    if (sent < 0)
    {
        return cplat_internal_error_report_socket_errno(detail_out, errno);
    }
    if ((size_t)sent > count)
    {
        return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
    }

    /* 全量を送れなかったデータグラムは送れなかったものとし、その手前までを送れた数とする。 */
    full_count = 0U;
    while (full_count < (size_t)sent && (size_t)messages[full_count].msg_len == datagrams[full_count].len)
    {
        full_count++;
    }
    if (full_count == 0U)
    {
        return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
    }
    *sent_count_out = full_count;
    return cplat_internal_error_report_success(detail_out);
}

/**
 *  @brief          解除、完了、期限の順で全量送受信の各確認点を処理します。
 *
 *  送信では send_buf、受信では recv_buf を使います。使わない側には NULL を渡します。
 *  送信の領域の const を保つため、領域の引数を送信用と受信用に分けています。
 */
static int transfer_all(cplat_socket_watch *watch, const void *send_buf, void *recv_buf, const size_t len,
                        cplat_socket_wait_cancel *cancel, const int timeout_ms, size_t *transferred_out,
                        const int sending, cplat_error *detail_out)
{
    size_t transferred = 0U;
    uint64_t deadline_ms = 0U;
    uint64_t now;
    int terminal_hint = 0;

    if (watch == NULL || watch->sock == CPLAT_INVALID_SOCKET || (sending ? (send_buf == NULL) : (recv_buf == NULL)) ||
        transferred_out == NULL || len > CPLAT_SOCKET_MAX_TRANSFER || timeout_ms < 0)
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }
    *transferred_out = 0U;
    if (timeout_ms != CPLAT_WAIT_FOREVER)
    {
        if (cplat_clock_get_monotonic_ms(&now) != CPLAT_OK)
        {
            return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
        }
        deadline_ms = now + (uint64_t)timeout_ms;
    }

    for (;;)
    {
        uint64_t remaining_ms;
        int deadline_result;
        ssize_t result;
        const size_t remaining = len - transferred;

        if (cancel_is_signaled(cancel))
        {
            *transferred_out = transferred;
            return cplat_internal_error_report_socket_errno_as(detail_out, ECANCELED, CPLAT_ERR_CANCELED);
        }
        if (transferred == len)
        {
            *transferred_out = transferred;
            return cplat_internal_error_report_success(detail_out);
        }
        deadline_result = deadline_remaining(deadline_ms, timeout_ms, &remaining_ms);
        if (deadline_result < 0)
        {
            *transferred_out = transferred;
            return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
        }
        if (deadline_result == 0)
        {
            *transferred_out = transferred;
            return cplat_internal_error_report_socket_errno_as(detail_out, ETIMEDOUT, CPLAT_ERR_TIMEOUT);
        }

        if (sending)
        {
            result = send((int)watch->sock, (const uint8_t *)send_buf + transferred, remaining, MSG_NOSIGNAL);
        }
        else
        {
            result = recv((int)watch->sock, (uint8_t *)recv_buf + transferred, remaining, 0);
        }
        if (result < 0)
        {
            const int saved_errno = errno;
            if (saved_errno == EINTR)
            {
                continue;
            }
            if (saved_errno != EAGAIN
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
                && saved_errno != EWOULDBLOCK
#endif
            )
            {
                *transferred_out = transferred;
                return cplat_internal_error_report_socket_errno(detail_out, saved_errno);
            }
            if (terminal_hint != 0)
            {
                *transferred_out = transferred;
                return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
            }

            deadline_result = deadline_remaining(deadline_ms, timeout_ms, &remaining_ms);
            if (deadline_result <= 0)
            {
                *transferred_out = transferred;
                return deadline_result == 0 ?
                           cplat_internal_error_report_socket_errno_as(detail_out, ETIMEDOUT, CPLAT_ERR_TIMEOUT) :
                           cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
            }
            {
                int terminal = 0;
                const int wait_result = wait_one_with_deadline(watch, cancel, timeout_ms, deadline_ms,
                                                               sending ? POLLOUT : POLLIN, &terminal, detail_out);
                if (wait_result != CPLAT_OK)
                {
                    *transferred_out = transferred;
                    return wait_result;
                }
                terminal_hint = terminal;
            }
            continue;
        }
        if (result == 0)
        {
            if (!sending)
            {
                *transferred_out = transferred;
                return cplat_internal_error_report_socket_errno_as(detail_out, 0, CPLAT_ERR_EOF);
            }
            *transferred_out = transferred;
            return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
        }
        if ((size_t)result > remaining)
        {
            *transferred_out = transferred;
            return cplat_internal_error_report_errno_as(detail_out, EIO, CPLAT_ERR_UNKNOWN);
        }
        transferred += (size_t)result;
        terminal_hint = 0;
    }
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_watch_send_all(cplat_socket_watch *watch, const void *buf, const size_t len,
                                cplat_socket_wait_cancel *cancel, const int timeout_ms, size_t *sent_out,
                                cplat_error *detail_out)
{
    size_t transferred = 0U;
    const int result = transfer_all(watch, buf, NULL, len, cancel, timeout_ms, &transferred, 1, detail_out);
    if (sent_out != NULL)
    {
        *sent_out = transferred;
    }
    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_watch_recv_all(cplat_socket_watch *watch, void *buf, const size_t len,
                                cplat_socket_wait_cancel *cancel, const int timeout_ms, size_t *received_out,
                                cplat_error *detail_out)
{
    size_t transferred = 0U;
    const int result = transfer_all(watch, NULL, buf, len, cancel, timeout_ms, &transferred, 0, detail_out);
    if (received_out != NULL)
    {
        *received_out = transferred;
    }
    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_socket_watch_connect(cplat_socket_watch *watch, const cplat_ipv4_endpoint *endpoint,
                               cplat_socket_wait_cancel *cancel, const int timeout_ms, cplat_error *detail_out)
{
    struct sockaddr_in native;
    int connect_result;
    int connect_errno;
    int terminal;

    if (watch == NULL || watch->sock == CPLAT_INVALID_SOCKET || endpoint == NULL || timeout_ms < 0)
    {
        return cplat_internal_error_report_errno_as(detail_out, EINVAL, CPLAT_ERR_INVALID_ARGUMENT);
    }
    if (cancel_is_signaled(cancel))
    {
        return cplat_internal_error_report_socket_errno_as(detail_out, ECANCELED, CPLAT_ERR_CANCELED);
    }

    memset(&native, 0, sizeof(native));
    native.sin_family = AF_INET;
    memcpy(&native.sin_addr.s_addr, &endpoint->address, sizeof(native.sin_addr.s_addr));
    memcpy(&native.sin_port, &endpoint->port, sizeof(native.sin_port));
    connect_result = connect((int)watch->sock, (const struct sockaddr *)&native, (socklen_t)sizeof(native));
    if (connect_result == 0)
    {
        return cplat_internal_error_report_success(detail_out);
    }

    connect_errno = errno;
    if (connect_errno != EINPROGRESS && connect_errno != EALREADY && connect_errno != EINTR)
    {
        return cplat_internal_error_report_socket_errno(detail_out, connect_errno);
    }

    {
        const int wait_result = wait_one(watch, cancel, timeout_ms, POLLOUT, &terminal, detail_out);
        if (wait_result != CPLAT_OK)
        {
            return wait_result;
        }
    }
    {
        int socket_error = 0;
        socklen_t error_len = (socklen_t)sizeof(socket_error);
        if (getsockopt((int)watch->sock, SOL_SOCKET, SO_ERROR, &socket_error, &error_len) != 0)
        {
            return cplat_internal_error_report_socket_errno(detail_out, errno);
        }
        if (socket_error != 0)
        {
            return cplat_internal_error_report_socket_errno(detail_out, socket_error);
        }
    }
    return cplat_internal_error_report_success(detail_out);
}

#endif /* PLATFORM_LINUX */
