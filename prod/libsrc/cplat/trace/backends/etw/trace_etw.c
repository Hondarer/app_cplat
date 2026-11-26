/**
 *******************************************************************************
 *  @file           trace_etw.c
 *  @brief          ETW トレース プロバイダーを実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/04/03
 *  @version        1.0.0
 *
 *  Windows TraceLogging ベースの ETW プロバイダーを提供します。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/base/platform.h>
#include <cplat/crt/stdlib.h>

#if defined(PLATFORM_WINDOWS)

    #include <cplat/base/result.h>
    #include <cplat/base/windows_sdk.h>
    #include <TraceLoggingProvider.h>
    #include <evntrace.h>
    #include <cplat/trace/etw.h>
    #include <stdlib.h>

    #include <cplat/trace/backends/etw/etw_internal.h>

/**
 *  @brief  ETW プロバイダー ハンドル構造体 (内部定義) です。
 */
struct cplat_etw_provider
{
    /** TraceLogging プロバイダー参照。 */
    cplat_etw_provider_ref_t provider_ref;
};

static void write_trace_event(cplat_etw_provider_ref_t ref, const int level, const char *service, const char *message);

/* Doxygen コメントは、ヘッダーに記載 */

cplat_etw_provider *cplat_etw_provider_create(cplat_etw_provider_ref_t provider_ref)
{
    cplat_etw_provider *handle;
    TLG_STATUS status;

    if (provider_ref == NULL)
    {
        return NULL;
    }

    handle = (cplat_etw_provider *)cplat_malloc(sizeof(cplat_etw_provider));
    if (handle == NULL)
    {
        return NULL;
    }

    handle->provider_ref = provider_ref;

    status = TraceLoggingRegister(provider_ref);
    if (status != S_OK)
    {
        cplat_free(handle);
        return NULL;
    }

    return handle;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_etw_provider_write(cplat_etw_provider *handle, const int level, const char *service, const char *message)
{
    if (handle == NULL || message == NULL)
    {
        return CPLAT_OK;
    }

    write_trace_event(handle->provider_ref, level, service, message);

    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_etw_provider_dispose(cplat_etw_provider *handle)
{
    if (handle == NULL)
    {
        return;
    }

    TraceLoggingUnregister(handle->provider_ref);
    cplat_free(handle);
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_internal_etw_provider_dispose_on_shutdown(cplat_etw_provider *handle, const cplat_shutdown_event *event)
{
    if (handle == NULL)
    {
        return;
    }

    if (event == NULL || event->reason == CPLAT_SHUTDOWN_REASON_NORMAL_EXIT)
    {
        TraceLoggingUnregister(handle->provider_ref);
    }

    cplat_free(handle);
}

/**
 *  @brief          ETW イベントを書き込む内部関数です。
 *
 *  service が NULL の場合は Service フィールドを含めありません。
 *
 *  @param[in]      ref     TraceLogging プロバイダー参照。
 *  @param[in]      level   トレース レベル (1=Critical 〜 5=Verbose)。
 *  @param[in]      service サービス名 (NULL 可)。NULL の場合 Service フィールドを省略。
 *  @param[in]      message メッセージ文字列。
 */
static void write_trace_event(cplat_etw_provider_ref_t ref, const int level, const char *service, const char *message)
{
    uint32_t process_id = (uint32_t)GetCurrentProcessId();

    /*
     * TraceLoggingLevel() はコンパイル時定数しか受け付けないため、レベルごとに書き分ける。
     * 明示したレベル以外は VERBOSE の ETW レベルで書く。
     * DEBUG と VERBOSE を同じ詳細度にする。
     * 正本は app/cplat/prod/libsrc/cplat/trace/README.md の「トレース レベル」節である。
     * see: https://learn.microsoft.com/windows/win32/api/traceloggingprovider/nf-traceloggingprovider-tracelogginglevel
     */
    if (service != NULL)
    {
        switch (level)
        {
        case TRACE_LEVEL_CRITICAL:
            TraceLoggingWrite(ref, "Trace", TraceLoggingLevel(TRACE_LEVEL_CRITICAL),
                              TraceLoggingString(service, "Service"), TraceLoggingString(message, "Message"),
                              TraceLoggingUInt32(process_id, "ProcessId"));
            break;
        case TRACE_LEVEL_ERROR:
            TraceLoggingWrite(ref, "Trace", TraceLoggingLevel(TRACE_LEVEL_ERROR),
                              TraceLoggingString(service, "Service"), TraceLoggingString(message, "Message"),
                              TraceLoggingUInt32(process_id, "ProcessId"));
            break;
        case TRACE_LEVEL_WARNING:
            TraceLoggingWrite(ref, "Trace", TraceLoggingLevel(TRACE_LEVEL_WARNING),
                              TraceLoggingString(service, "Service"), TraceLoggingString(message, "Message"),
                              TraceLoggingUInt32(process_id, "ProcessId"));
            break;
        case TRACE_LEVEL_INFORMATION:
            TraceLoggingWrite(ref, "Trace", TraceLoggingLevel(TRACE_LEVEL_INFORMATION),
                              TraceLoggingString(service, "Service"), TraceLoggingString(message, "Message"),
                              TraceLoggingUInt32(process_id, "ProcessId"));
            break;
        default:
            TraceLoggingWrite(ref, "Trace", TraceLoggingLevel(TRACE_LEVEL_VERBOSE),
                              TraceLoggingString(service, "Service"), TraceLoggingString(message, "Message"),
                              TraceLoggingUInt32(process_id, "ProcessId"));
            break;
        }
    }
    else
    {
        switch (level)
        {
        case TRACE_LEVEL_CRITICAL:
            TraceLoggingWrite(ref, "Trace", TraceLoggingLevel(TRACE_LEVEL_CRITICAL),
                              TraceLoggingString(message, "Message"), TraceLoggingUInt32(process_id, "ProcessId"));
            break;
        case TRACE_LEVEL_ERROR:
            TraceLoggingWrite(ref, "Trace", TraceLoggingLevel(TRACE_LEVEL_ERROR),
                              TraceLoggingString(message, "Message"), TraceLoggingUInt32(process_id, "ProcessId"));
            break;
        case TRACE_LEVEL_WARNING:
            TraceLoggingWrite(ref, "Trace", TraceLoggingLevel(TRACE_LEVEL_WARNING),
                              TraceLoggingString(message, "Message"), TraceLoggingUInt32(process_id, "ProcessId"));
            break;
        case TRACE_LEVEL_INFORMATION:
            TraceLoggingWrite(ref, "Trace", TraceLoggingLevel(TRACE_LEVEL_INFORMATION),
                              TraceLoggingString(message, "Message"), TraceLoggingUInt32(process_id, "ProcessId"));
            break;
        default:
            TraceLoggingWrite(ref, "Trace", TraceLoggingLevel(TRACE_LEVEL_VERBOSE),
                              TraceLoggingString(message, "Message"), TraceLoggingUInt32(process_id, "ProcessId"));
            break;
        }
    }
}

#endif /* PLATFORM_WINDOWS */
