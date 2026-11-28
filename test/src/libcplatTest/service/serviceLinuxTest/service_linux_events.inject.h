#ifndef SERVICE_LINUX_EVENTS_INJECT_H
#define SERVICE_LINUX_EVENTS_INJECT_H

#include <cplat/service/service.h>

#ifdef __cplusplus
extern "C"
{
#endif
    void test_svc_linux_events_set_phase(int phase);
    int test_svc_linux_events_get_phase(void);
    void *test_svc_linux_events_notify_lock(void);
    int test_svc_linux_events_get_join_timeout_ms(void);
    void test_svc_linux_events_run_watchdog_timer(void);
    void test_svc_linux_events_set_definition(const cplat_service_definition *def);
    void test_svc_linux_events_dispatch_event(const cplat_service_event_info *info);
    void test_svc_linux_events_set_bus(void *bus);
    void test_svc_linux_events_reset_inhibit(void);
    int test_svc_linux_events_inhibit_fd(int which);
    void *test_svc_linux_events_inhibit_slot(int which);
    void test_svc_linux_events_set_inhibit_fd(int which, int fd);
    void test_svc_linux_events_set_inhibit_slot(int which, void *slot);
    void test_svc_linux_events_acquire_inhibit_sync(int which);
    void test_svc_linux_events_request_inhibit_async(int which);
    int test_svc_linux_events_build_inhibit_message(const char *what, void **message);
    int test_svc_linux_events_store_inhibit_fd(int which, void *reply);
    int test_svc_linux_events_on_inhibit_reply(int which, void *message, void *ret_error);
    void test_svc_linux_events_handle_prepare_for_sleep(int starting);
    void test_svc_linux_events_handle_prepare_for_shutdown(int starting);
    void test_svc_linux_events_release_inhibit_locks(void);
#ifdef __cplusplus
}
#endif

#endif /* SERVICE_LINUX_EVENTS_INJECT_H */
