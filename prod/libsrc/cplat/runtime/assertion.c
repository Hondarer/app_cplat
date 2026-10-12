/**
 *******************************************************************************
 *  @file           assertion.c
 *  @brief          継続が危険な状態を検出してプロセスを異常終了する処理を実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/10/12
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/base/result.h>
#include <cplat/crt/stdlib.h>
#include <cplat/runtime/assertion.h>
#include <cplat/sync/atomic.h>
#include <cplat/sync/sync.h>

#include <stdio.h>
#include <stdlib.h>

typedef struct assert_callback_entry
{
    cplat_assert_fn callback;
    void *context;
    struct assert_callback_entry *next;
} assert_callback_entry;

/* 異常終了の処理中にロックを取らずに辿れるよう、先頭へ追加するだけの単方向リストとする。
   登録の解除を提供しないため、要素は解放しない。 */
static cplat_atomic_ptr s_assert_callbacks = CPLAT_ATOMIC_INIT(NULL);
static cplat_atomic_i32 s_assert_started = CPLAT_ATOMIC_INIT(0);
static THREAD_LOCAL int s_assert_running_on_this_thread = 0;

static const char *non_null_text(const char *text)
{
    if (text == NULL)
    {
        return "";
    }
    return text;
}

int cplat_assert_register(cplat_assert_fn callback, void *context)
{
    assert_callback_entry *entry;
    void *head;

    if (callback == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    entry = (assert_callback_entry *)cplat_malloc(sizeof(*entry));
    if (entry == NULL)
    {
        return CPLAT_ERR_OUT_OF_MEMORY;
    }
    entry->callback = callback;
    entry->context = context;

    head = cplat_atomic_load_ptr(&s_assert_callbacks, CPLAT_MEMORY_ORDER_ACQUIRE);
    do
    {
        entry->next = (assert_callback_entry *)head;
    } while (cplat_atomic_compare_exchange_ptr(&s_assert_callbacks, &head, entry, CPLAT_MEMORY_ORDER_ACQ_REL) == 0);

    return CPLAT_OK;
}

void cplat_assert_fail_at(const char *expression, const char *file, const int line, const char *function)
{
    cplat_assert_event event;
    const assert_callback_entry *entry;

    event.expression = non_null_text(expression);
    event.file = non_null_text(file);
    event.line = line;
    event.function = non_null_text(function);

    (void)fprintf(stderr, "CPLAT_ASSERT failed: %s, file %s, line %d, function %s\n", event.expression, event.file,
                  event.line, event.function);
    (void)fflush(stderr);

    if (s_assert_running_on_this_thread != 0)
    {
        /* コールバック内で再び条件が偽になった。残りのコールバックは呼ばずに終了する */
        abort();
    }
    s_assert_running_on_this_thread = 1;

    if (cplat_atomic_exchange_i32(&s_assert_started, 1, CPLAT_MEMORY_ORDER_ACQ_REL) != 0)
    {
        /* 別のスレッドが異常終了の処理中。そのスレッドの abort() でプロセスが終了するまで待つ */
        for (;;)
        {
            cplat_sleep_ms(1000);
        }
    }

    entry = (const assert_callback_entry *)cplat_atomic_load_ptr(&s_assert_callbacks, CPLAT_MEMORY_ORDER_ACQUIRE);
    while (entry != NULL)
    {
        entry->callback(&event, entry->context);
        entry = entry->next;
    }

    abort();
}
