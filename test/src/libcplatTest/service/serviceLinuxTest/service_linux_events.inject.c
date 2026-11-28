#ifndef _IN_TEST_SRC
    #include "service_linux_events.c"
#endif
#include "service_linux_events.inject.h"

void test_svc_linux_events_set_phase(int phase)
{
    cplat_internal_service_linux_events_notify_lock();
    s_phase = (svc_linux_events_phase)phase;
    cplat_internal_service_linux_events_notify_unlock();
}

int test_svc_linux_events_get_phase(void)
{
    int phase;
    cplat_internal_service_linux_events_notify_lock();
    phase = (int)s_phase;
    cplat_internal_service_linux_events_notify_unlock();
    return phase;
}

void *test_svc_linux_events_notify_lock(void)
{
    return s_ctx.notify_lock;
}

int test_svc_linux_events_get_join_timeout_ms(void)
{
    return s_ctx.join_timeout_ms;
}

void test_svc_linux_events_run_watchdog_timer(void)
{
    (void)on_watchdog_timer(s_ctx.watchdog_source, 0U, NULL);
}

void test_svc_linux_events_set_definition(const cplat_service_definition *def)
{
    s_ctx.def = def;
}

void test_svc_linux_events_dispatch_event(const cplat_service_event_info *info)
{
    dispatch_event_while_running(info);
}

static svc_inhibit_lock *test_inhibit_lock(int which)
{
    return (which == 0) ? &s_ctx.inhibit_sleep : &s_ctx.inhibit_shutdown;
}

void test_svc_linux_events_set_bus(void *bus)
{
    s_ctx.bus = (sd_bus *)bus;
}

void test_svc_linux_events_reset_inhibit(void)
{
    s_ctx.inhibit_sleep.slot = NULL;
    s_ctx.inhibit_sleep.fd = -1;
    s_ctx.inhibit_shutdown.slot = NULL;
    s_ctx.inhibit_shutdown.fd = -1;
    s_ctx.bus = NULL;
}

int test_svc_linux_events_inhibit_fd(int which)
{
    return test_inhibit_lock(which)->fd;
}

void *test_svc_linux_events_inhibit_slot(int which)
{
    return test_inhibit_lock(which)->slot;
}

void test_svc_linux_events_set_inhibit_fd(int which, int fd)
{
    test_inhibit_lock(which)->fd = fd;
}

void test_svc_linux_events_set_inhibit_slot(int which, void *slot)
{
    test_inhibit_lock(which)->slot = (sd_bus_slot *)slot;
}

void test_svc_linux_events_acquire_inhibit_sync(int which)
{
    acquire_inhibit_lock_sync(test_inhibit_lock(which));
}

void test_svc_linux_events_request_inhibit_async(int which)
{
    request_inhibit_lock_async(test_inhibit_lock(which));
}

int test_svc_linux_events_build_inhibit_message(const char *what, void **message)
{
    sd_bus_message *created = NULL;
    int rc;

    rc = build_inhibit_message(what, &created);
    *message = created;

    return rc;
}

int test_svc_linux_events_store_inhibit_fd(int which, void *reply)
{
    return store_inhibit_fd(test_inhibit_lock(which), (sd_bus_message *)reply);
}

int test_svc_linux_events_on_inhibit_reply(int which, void *message, void *ret_error)
{
    return on_inhibit_reply((sd_bus_message *)message, test_inhibit_lock(which), (sd_bus_error *)ret_error);
}

void test_svc_linux_events_handle_prepare_for_sleep(int starting)
{
    handle_prepare_for_sleep(starting);
}

void test_svc_linux_events_handle_prepare_for_shutdown(int starting)
{
    handle_prepare_for_shutdown(starting);
}

void test_svc_linux_events_release_inhibit_locks(void)
{
    release_inhibit_locks();
}
