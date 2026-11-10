#ifndef CPLAT_SERVICE_API_TEST_FAKE_H
#define CPLAT_SERVICE_API_TEST_FAKE_H
#include <cplat/base/windows_sdk.h>
#ifdef __cplusplus
extern "C"
{
#endif
#include <cplat/win32/win32.h>
#ifdef __cplusplus
}
#endif
#if defined(PLATFORM_WINDOWS)
    #ifdef __cplusplus
extern "C"
{
    #endif
    BOOL fake_ChangeServiceConfig2W(SC_HANDLE service, DWORD level, LPVOID info);
    BOOL fake_StartServiceCtrlDispatcherW(const SERVICE_TABLE_ENTRYW *table);
    #ifdef __cplusplus
}
    #endif
    #define ChangeServiceConfig2W       fake_ChangeServiceConfig2W
    #define StartServiceCtrlDispatcherW fake_StartServiceCtrlDispatcherW
#endif /* PLATFORM_WINDOWS */
#endif /* CPLAT_SERVICE_API_TEST_FAKE_H */
