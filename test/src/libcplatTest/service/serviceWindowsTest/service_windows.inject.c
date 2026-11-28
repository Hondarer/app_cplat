#ifndef _IN_TEST_SRC
    #include "service_windows.c"
#endif
#include "service_windows.inject.h"

#if defined(PLATFORM_WINDOWS)

void test_service_windows_set_status_handle(void *handle)
{
    s_status_handle = (SERVICE_STATUS_HANDLE)handle;
    memset(&s_status, 0, sizeof(s_status));
}

void test_service_windows_set_status_lock(void *lock)
{
    s_status_lock = (cplat_local_lock *)lock;
}

#endif /* PLATFORM_WINDOWS */
