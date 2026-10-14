/**
 *******************************************************************************
 *  @file           abort.c
 *  @brief          継続が危険な状態でプロセスを異常終了する処理を実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/10/12
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/base/result.h>
#include <cplat/crt/stdlib.h>
#include <cplat/runtime/abort.h>
#include <cplat/sync/atomic.h>
#include <cplat/sync/sync.h>

#include <stdio.h>
#include <stdlib.h>

typedef struct abort_callback_entry
{
    cplat_abort_fn callback;
    void *context;
    struct abort_callback_entry *next;
} abort_callback_entry;

/* 異常終了の処理中にロックを取らずに辿れるよう、先頭へ追加するだけの単方向リストとする。
   登録の解除を提供しないため、要素は解放しない。 */
static cplat_atomic_ptr s_abort_callbacks = CPLAT_ATOMIC_INIT(NULL);
static cplat_atomic_i32 s_abort_started = CPLAT_ATOMIC_INIT(0);
static THREAD_LOCAL int s_abort_running_on_this_thread = 0;

static const char *non_null_text(const char *text)
{
    if (text == NULL)
    {
        return "";
    }
    return text;
}

int cplat_abort_register(cplat_abort_fn callback, void *context)
{
    abort_callback_entry *entry;
    void *head;

    if (callback == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    entry = (abort_callback_entry *)cplat_malloc(sizeof(*entry));
    if (entry == NULL)
    {
        return CPLAT_ERR_OUT_OF_MEMORY;
    }
    entry->callback = callback;
    entry->context = context;

    head = cplat_atomic_load_ptr(&s_abort_callbacks, CPLAT_MEMORY_ORDER_ACQUIRE);
    do
    {
        entry->next = (abort_callback_entry *)head;
    } while (cplat_atomic_compare_exchange_ptr(&s_abort_callbacks, &head, entry, CPLAT_MEMORY_ORDER_ACQ_REL) == 0);

    return CPLAT_OK;
}

static const char *kind_prefix(const cplat_abort_kind kind)
{
    switch (kind)
    {
    case CPLAT_ABORT_KIND_ASSERTION_FAILED:
        return "CPLAT_ASSERT failed";
    case CPLAT_ABORT_KIND_REQUESTED:
        return "CPLAT_ABORT";
    default:
        /* 定義外の種別でも異常終了の処理は続ける。理由は書き出せるため、接頭辞だけを汎用にする */
        return "cplat abort";
    }
}

void cplat_abort_at(const cplat_abort_kind kind, const char *message, const char *file, const int line,
                    const char *function)
{
    cplat_abort_event event = {0};
    const abort_callback_entry *entry;

    event.message = non_null_text(message);
    event.file = non_null_text(file);
    event.function = non_null_text(function);
    event.line = line;
    event.kind = kind;

    (void)fprintf(stderr, "%s: %s, file %s, line %d, function %s\n", kind_prefix(event.kind), event.message,
                  event.file, event.line, event.function);
    (void)fflush(stderr);

    if (s_abort_running_on_this_thread != 0)
    {
        /* コールバック内で再び異常終了を始めた。残りのコールバックは呼ばずに終了する */
        abort();
    }
    s_abort_running_on_this_thread = 1;

    if (cplat_atomic_exchange_i32(&s_abort_started, 1, CPLAT_MEMORY_ORDER_ACQ_REL) != 0)
    {
        /* 別のスレッドが異常終了の処理中。そのスレッドの abort() でプロセスが終了するまで待つ */
        for (;;)
        {
            cplat_sleep_ms(1000);
        }
    }

    entry = (const abort_callback_entry *)cplat_atomic_load_ptr(&s_abort_callbacks, CPLAT_MEMORY_ORDER_ACQUIRE);
    while (entry != NULL)
    {
        entry->callback(&event, entry->context);
        entry = entry->next;
    }

    abort();
}
