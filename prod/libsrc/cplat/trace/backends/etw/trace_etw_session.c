/**
 *******************************************************************************
 *  @file           trace_etw_session.c
 *  @brief          リアルタイム ETW セッションを操作する API を実装します。
 *
 *  ETW セッションの開始、イベント データの復元、ワーカー スレッドの停止を管理します。
 *
 *******************************************************************************
 */

#include <cplat/base/platform.h>
#include <cplat/crt/stdlib.h>

#if defined(PLATFORM_WINDOWS)

    #include <cplat/base/windows_sdk.h>
    #include <cplat/crt/wchar_conv.h>
    #include <cplat/crt/string.h>
    #include <string.h> /* memset */
    #include <cplat/sync/sync.h>
    #include <evntcons.h>
    #include <evntrace.h>
    #include <tdh.h>
    #pragma comment(lib, "Tdh.lib")
    #include <cplat/trace/etw.h>
    #include <stdio.h>
    #include <stdlib.h>

    #ifndef INVALID_PROCESSTRACE_HANDLE
        #define INVALID_PROCESSTRACE_HANDLE ((TRACEHANDLE)INVALID_HANDLE_VALUE)
    #endif /* INVALID_PROCESSTRACE_HANDLE */

/**
 *  @brief  ETW セッション構造体 (内部定義) です。
 */
struct cplat_etw_session
{
    /** トレース セッション ハンドル。 */
    TRACEHANDLE session_handle;
    /** トレース処理ハンドル。 */
    TRACEHANDLE trace_handle;
    /** ProcessTrace ワーカー スレッド。 */
    cplat_thread *thread_handle;
    /** イベント受信コールバック。 */
    cplat_etw_event_fn callback;
    /** コールバックに渡すユーザー データ。 */
    void *context;
    /** セッション プロパティ (可変長)。 */
    EVENT_TRACE_PROPERTIES *properties;
    /** セッション名 (ワイド文字列)。 */
    wchar_t *session_name_w;
    /** プロバイダー GUID (イベント フィルタリング用)。 */
    GUID provider_guid;
};

/**
 *  @brief  構築途中のセッションが確保した資源を解放します。
 *
 *  トレース ハンドル、セッション ハンドル、ワイド文字列、プロパティ領域の順に解放する。\n
 *  未確保のメンバーは初期値のまま解放をスキップするため、構築のどの段階からでも呼び出せる。
 */
static void dispose_session(cplat_etw_session *session)
{
    if (session != NULL)
    {
        if (session->trace_handle != INVALID_PROCESSTRACE_HANDLE)
        {
            CloseTrace(session->trace_handle);
        }
        if (session->session_handle != 0)
        {
            ControlTraceW(session->session_handle, NULL, session->properties, EVENT_TRACE_CONTROL_STOP);
        }
        cplat_free(session->session_name_w);
        cplat_free(session->properties);
        cplat_free(session);
    }
}

/**
 *  @brief  GUID 文字列 "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" をパースします。
 *  @return 成功 0 / 失敗 -1。
 */
static int parse_guid(const char *str, GUID *guid)
{
    unsigned int d[11];
    int n;

    if (str == NULL || guid == NULL)
    {
        return -1;
    }

    n = cplat_sscanf(str, "%8x-%4x-%4x-%2x%2x-%2x%2x%2x%2x%2x%2x", &d[0], &d[1], &d[2], &d[3], &d[4], &d[5], &d[6],
                     &d[7], &d[8], &d[9], &d[10]);

    if (n != 11)
    {
        return -1;
    }

    guid->Data1 = (ULONG)d[0];
    guid->Data2 = (USHORT)d[1];
    guid->Data3 = (USHORT)d[2];
    guid->Data4[0] = (UCHAR)d[3];
    guid->Data4[1] = (UCHAR)d[4];
    guid->Data4[2] = (UCHAR)d[5];
    guid->Data4[3] = (UCHAR)d[6];
    guid->Data4[4] = (UCHAR)d[7];
    guid->Data4[5] = (UCHAR)d[8];
    guid->Data4[6] = (UCHAR)d[9];
    guid->Data4[7] = (UCHAR)d[10];
    return 0;
}

/**
 *  @brief  GUID 一致判定です。
 */
static int guid_equal(const GUID *a, const GUID *b)
{
    return a->Data1 == b->Data1 && a->Data2 == b->Data2 && a->Data3 == b->Data3 && a->Data4[0] == b->Data4[0] &&
           a->Data4[1] == b->Data4[1] && a->Data4[2] == b->Data4[2] && a->Data4[3] == b->Data4[3] &&
           a->Data4[4] == b->Data4[4] && a->Data4[5] == b->Data4[5] && a->Data4[6] == b->Data4[6] &&
           a->Data4[7] == b->Data4[7];
}

/**
 *  @brief  TRACE_EVENT_INFO を取得します。
 *  @return 成功時は確保済みポインター。失敗時は NULL。
 */
static TRACE_EVENT_INFO *get_trace_event_info(PEVENT_RECORD pEvent)
{
    TRACE_EVENT_INFO *info = NULL;
    ULONG size = 0;
    ULONG status;

    status = TdhGetEventInformation(pEvent, 0, NULL, NULL, &size);
    if (status != ERROR_INSUFFICIENT_BUFFER || size == 0)
    {
        return NULL;
    }

    info = (TRACE_EVENT_INFO *)cplat_malloc(size);
    if (info == NULL)
    {
        return NULL;
    }

    status = TdhGetEventInformation(pEvent, 0, NULL, info, &size);
    if (status != ERROR_SUCCESS)
    {
        cplat_free(info);
        return NULL;
    }

    return info;
}

/**
 *  @brief  TRACE_EVENT_INFO からイベント名を取得します。
 *  @return イベント名。取得できない場合は NULL。
 */
static const wchar_t *get_event_name(const TRACE_EVENT_INFO *info)
{
    if (info == NULL || info->EventNameOffset == 0)
    {
        return NULL;
    }

    return (const wchar_t *)((const unsigned char *)info + info->EventNameOffset);
}

/**
 *  @brief  UserData 上の null 終端 ANSI 文字列を 1 つ読み取ります。
 *  @return 成功 0 / 失敗 -1。
 */
static int read_ansi_string_field(const unsigned char *cursor, const USHORT remaining, const char **text_out,
                                  USHORT *consumed_out)
{
    USHORT i;

    if (cursor == NULL || text_out == NULL || consumed_out == NULL)
    {
        return -1;
    }

    for (i = 0; i < remaining; i++)
    {
        if (cursor[i] == '\0')
        {
            *text_out = (const char *)cursor;
            *consumed_out = (USHORT)(i + 1U);
            return 0;
        }
    }

    return -1;
}

/**
 *  @brief  UserData 上の uint32 値を 1 つ読み取ります。
 *  @return 成功 0 / 失敗 -1。
 */
static int read_uint32_field(const unsigned char *cursor, const USHORT remaining, uint32_t *value_out,
                             USHORT *consumed_out)
{
    if (cursor == NULL || value_out == NULL || consumed_out == NULL || remaining < 4U)
    {
        return -1;
    }

    *value_out =
        (uint32_t)cursor[0] | ((uint32_t)cursor[1] << 8) | ((uint32_t)cursor[2] << 16) | ((uint32_t)cursor[3] << 24);
    *consumed_out = 4U;
    return 0;
}

/**
 *  @brief  TraceLogging payload から Service / Message を復元します。
 *
 *  TdhGetEventInformation で得たプロパティ順に ANSI 文字列を読み進める。
 *  Service / Message が存在しないイベントは out_* を NULL のまま返します。
 */
static void extract_event_fields(PEVENT_RECORD pEvent, const TRACE_EVENT_INFO *info, const char **service_out,
                                 const char **message_out, uint32_t *process_id_out)
{
    const unsigned char *cursor;
    USHORT remaining;
    ULONG count;
    ULONG i;

    if (service_out != NULL)
    {
        *service_out = NULL;
    }
    if (message_out != NULL)
    {
        *message_out = NULL;
    }
    if (process_id_out != NULL)
    {
        *process_id_out = 0U;
    }
    if (pEvent == NULL || message_out == NULL)
    {
        return;
    }

    if (pEvent->UserData == NULL || pEvent->UserDataLength == 0)
    {
        return;
    }

    if (info == NULL || info->TopLevelPropertyCount == 0)
    {
        return;
    }

    cursor = (const unsigned char *)pEvent->UserData;
    remaining = pEvent->UserDataLength;
    count = info->TopLevelPropertyCount;

    for (i = 0; i < count && remaining > 0; i++)
    {
        const EVENT_PROPERTY_INFO *prop;
        const wchar_t *name;
        const char *text;
        uint32_t process_id_value;
        USHORT consumed;

        prop = &info->EventPropertyInfoArray[i];
        if ((prop->Flags & PropertyStruct) != 0)
        {
            continue;
        }
        if (prop->NameOffset == 0)
        {
            continue;
        }
        name = (const wchar_t *)((const unsigned char *)info + prop->NameOffset);
        if (name == NULL)
        {
            continue;
        }

        if (prop->nonStructType.InType == TDH_INTYPE_ANSISTRING)
        {
            if (read_ansi_string_field(cursor, remaining, &text, &consumed) != 0)
            {
                break;
            }

            if (wcscmp(name, L"Service") == 0)
            {
                if (service_out != NULL)
                {
                    *service_out = text;
                }
            }
            else if (wcscmp(name, L"Message") == 0)
            {
                *message_out = text;
            }
        }
        else if (prop->nonStructType.InType == TDH_INTYPE_UINT32)
        {
            if (read_uint32_field(cursor, remaining, &process_id_value, &consumed) != 0)
            {
                break;
            }

            if (wcscmp(name, L"ProcessId") == 0 && process_id_out != NULL)
            {
                *process_id_out = process_id_value;
            }
        }
        else
        {
            break;
        }

        cursor += consumed;
        remaining = (USHORT)(remaining - consumed);
    }
}

/**
 *  @brief  ETW イベント レコード コールバック (ProcessTrace から呼び出される) です。
 *
 *  プロバイダー GUID でフィルタリングし、UserData を null 終端文字列として読み取ります。
 *  TraceLoggingString は UserData に null 終端 ANSI 文字列を直接格納します。
 */
static VOID WINAPI event_record_callback(PEVENT_RECORD pEvent)
{
    cplat_etw_session *session;
    cplat_etw_event event;
    TRACE_EVENT_INFO *info;
    const wchar_t *event_name_w;
    char *event_name_utf8;
    const char *service;
    const char *message;
    uint32_t payload_process_id;

    if (pEvent == NULL)
    {
        return;
    }

    session = (cplat_etw_session *)pEvent->UserContext;
    if (session == NULL || session->callback == NULL)
    {
        return;
    }

    /* プロバイダー GUID でフィルタリング */
    if (!guid_equal(&pEvent->EventHeader.ProviderId, &session->provider_guid))
    {
        return;
    }

    info = get_trace_event_info(pEvent);
    event_name_w = get_event_name(info);
    if (event_name_w == NULL || wcscmp(event_name_w, L"Trace") != 0)
    {
        cplat_free(info);
        return;
    }
    event_name_utf8 = cplat_wstr_to_utf8_alloc(event_name_w);

    service = NULL;
    message = NULL;
    payload_process_id = 0U;
    extract_event_fields(pEvent, info, &service, &message, &payload_process_id);
    cplat_free(info);

    event.level = pEvent->EventHeader.EventDescriptor.Level;
    if (payload_process_id != 0U)
    {
        event.process_id = payload_process_id;
    }
    else
    {
        event.process_id = pEvent->EventHeader.ProcessId;
    }
    event.event_name = event_name_utf8;
    event.service = service;
    event.message = message;
    event.timestamp_100ns = pEvent->EventHeader.TimeStamp.QuadPart;

    session->callback(&event, session->context);
    cplat_free(event_name_utf8);
}

/**
 *  @brief  ProcessTrace ワーカー スレッド関数です。
 */
static void trace_thread_proc(void *param)
{
    cplat_etw_session *session = (cplat_etw_session *)param;

    ProcessTrace(&session->trace_handle, 1, NULL, NULL);
    return;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_etw_session_check_access(void)
{
    static const wchar_t probe_name[] = L"EtwUtil_AccessProbe";
    size_t props_size;
    EVENT_TRACE_PROPERTIES *props;
    TRACEHANDLE handle = 0;
    ULONG status;
    int result;

    props_size = sizeof(EVENT_TRACE_PROPERTIES) + sizeof(probe_name);
    props = (EVENT_TRACE_PROPERTIES *)cplat_malloc(props_size);
    if (props == NULL)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    memset(props, 0, props_size);
    props->Wnode.BufferSize = (ULONG)props_size;
    props->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    props->Wnode.ClientContext = 1;
    props->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    props->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

    status = StartTraceW(&handle, probe_name, props);

    if (status == ERROR_SUCCESS)
    {
        ControlTraceW(handle, NULL, props, EVENT_TRACE_CONTROL_STOP);
        result = CPLAT_OK;
    }
    else if (status == ERROR_ACCESS_DENIED)
    {
        result = CPLAT_ERR_PERMISSION_DENIED;
    }
    else
    {
        result = CPLAT_ERR_UNKNOWN;
    }

    cplat_free(props);
    return result;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_etw_session_start(const char *session_name, const char *provider_guid_str, cplat_etw_event_fn callback,
                            void *context, cplat_etw_session **session_out)
{
    cplat_etw_session *session = NULL;
    GUID provider_guid;
    ULONG status;
    size_t name_len_w;
    size_t props_size;
    ENABLE_TRACE_PARAMETERS etp = {0};

    if (session_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    *session_out = NULL;

    if (session_name == NULL || provider_guid_str == NULL || callback == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    if (parse_guid(provider_guid_str, &provider_guid) != 0)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    session = (cplat_etw_session *)cplat_malloc(sizeof(cplat_etw_session));
    if (session == NULL)
    {
        return CPLAT_ERR_UNKNOWN;
    }

    memset(session, 0, sizeof(cplat_etw_session));
    session->callback = callback;
    session->context = context;
    session->session_handle = 0;
    session->trace_handle = INVALID_PROCESSTRACE_HANDLE;
    session->thread_handle = NULL;
    session->properties = NULL;
    session->session_name_w = NULL;
    session->provider_guid = provider_guid;

    /* セッション名をワイド文字列に変換して確保 */
    session->session_name_w = cplat_utf8_to_wstr_alloc(session_name);
    if (session->session_name_w == NULL)
    {
        dispose_session(session);
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    name_len_w = wcslen(session->session_name_w) + 1;

    /* EVENT_TRACE_PROPERTIES を確保 (セッション名領域を含む) */
    props_size = sizeof(EVENT_TRACE_PROPERTIES) + (name_len_w * sizeof(wchar_t));
    session->properties = (EVENT_TRACE_PROPERTIES *)cplat_malloc(props_size);
    if (session->properties == NULL)
    {
        dispose_session(session);
        return CPLAT_ERR_UNKNOWN;
    }

    /* リアルタイム セッションを開始 */
    memset(session->properties, 0, props_size);
    session->properties->Wnode.BufferSize = (ULONG)props_size;
    session->properties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    session->properties->Wnode.ClientContext = 1; /* QPC */
    session->properties->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    session->properties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
    session->properties->FlushTimer = 1;

    status = StartTraceW(&session->session_handle, session->session_name_w, session->properties);

    if (status == ERROR_ACCESS_DENIED)
    {
        session->session_handle = 0;
        dispose_session(session);
        return CPLAT_ERR_PERMISSION_DENIED;
    }

    if (status != ERROR_SUCCESS)
    {
        session->session_handle = 0;
        dispose_session(session);
        return CPLAT_ERR_UNKNOWN;
    }

    /* プロバイダーを有効化 */
    etp.Version = ENABLE_TRACE_PARAMETERS_VERSION_2;
    status = EnableTraceEx2(session->session_handle, &provider_guid, EVENT_CONTROL_CODE_ENABLE_PROVIDER, 5,
                            0xFFFFFFFFFFFFFFFF, 0, 0, &etp);
    if (status != ERROR_SUCCESS)
    {
        dispose_session(session);
        return CPLAT_ERR_UNKNOWN;
    }

    /* トレースをオープンしワーカー スレッドを起動 */
    {
        EVENT_TRACE_LOGFILEW trace_logfile = {0};
        trace_logfile.LoggerName = session->session_name_w;
        trace_logfile.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
        trace_logfile.EventRecordCallback = event_record_callback;
        trace_logfile.Context = session;

        session->trace_handle = OpenTraceW(&trace_logfile);
    }
    if (session->trace_handle == INVALID_PROCESSTRACE_HANDLE)
    {
        dispose_session(session);
        return CPLAT_ERR_UNKNOWN;
    }

    if (cplat_thread_create(&session->thread_handle, trace_thread_proc, session) != CPLAT_OK)
    {
        dispose_session(session);
        return CPLAT_ERR_UNKNOWN;
    }

    *session_out = session;
    return CPLAT_OK;
}

/* Doxygen コメントは、ヘッダーに記載 */

void cplat_etw_session_stop(cplat_etw_session *session)
{
    if (session == NULL)
    {
        return;
    }

    /* セッション停止 (バッファー フラッシュ → ProcessTrace が残イベントを処理して戻る) */
    if (session->session_handle != 0 && session->properties != NULL)
    {
        ControlTraceW(session->session_handle, NULL, session->properties, EVENT_TRACE_CONTROL_STOP);
    }

    /* ワーカー スレッド join */
    if (session->thread_handle != NULL)
    {
        cplat_thread_join(session->thread_handle, CPLAT_SYNC_WAIT_FOREVER);
    }

    /* トレース ハンドル クローズ */
    if (session->trace_handle != INVALID_PROCESSTRACE_HANDLE)
    {
        CloseTrace(session->trace_handle);
    }

    cplat_free(session->session_name_w);
    cplat_free(session->properties);
    cplat_free(session);
}

#endif /* PLATFORM_WINDOWS */
