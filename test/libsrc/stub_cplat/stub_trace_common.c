/**
 *  @file           stub_trace_common.c
 *  @brief          トレース共通ヘルパーの内部 API を、呼び出し元の単体テスト向けに提供します。
 *
 *  trace_common.c のカバレッジは traceCommonTest が担う。
 *  本スタブはタイムスタンプ解決、ISO 8601 整形、レベル文字変換の契約だけを満たす。
 */

#include <cplat/trace/trace_common.h>

static int timestamp_is_valid(const cplat_timespec *timestamp)
{
    return timestamp != NULL && timestamp->tv_nsec >= 0 && timestamp->tv_nsec < 1000000000;
}

int cplat_internal_trace_resolve_timestamp(const cplat_timespec *timestamp, cplat_timespec *resolved_out,
                                           int *fallback_used_out)
{
    if (resolved_out == NULL)
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    if (fallback_used_out != NULL)
    {
        *fallback_used_out = 0;
    }

    if (timestamp != NULL)
    {
        if (timestamp_is_valid(timestamp))
        {
            *resolved_out = *timestamp;
            return CPLAT_OK;
        }
        if (fallback_used_out != NULL)
        {
            *fallback_used_out = 1;
        }
    }

    int ret = cplat_clock_get_realtime(resolved_out);
    if (ret != CPLAT_OK)
    {
        return ret;
    }

    if (timestamp_is_valid(resolved_out))
    {
        return CPLAT_OK;
    }
    return CPLAT_ERR_UNKNOWN;
}

int cplat_internal_trace_format_local_timestamp(char *dest, const size_t dest_size, const cplat_timespec *timestamp)
{
    if (!timestamp_is_valid(timestamp))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }
    return cplat_clock_format_realtime_iso8601_local(dest, dest_size, timestamp);
}

char cplat_internal_trace_level_char(const cplat_trace_level level)
{
    switch (level)
    {
    /* 強制出力かどうかは絞り込みの結果であり、記録の重大度ではないため、表記を分けない */
    case CPLAT_TRACE_LEVEL_FORCE_CRITICAL:
    case CPLAT_TRACE_LEVEL_CRITICAL:
        return 'C';
    case CPLAT_TRACE_LEVEL_FORCE_ERROR:
    case CPLAT_TRACE_LEVEL_ERROR:
        return 'E';
    case CPLAT_TRACE_LEVEL_FORCE_WARNING:
    case CPLAT_TRACE_LEVEL_WARNING:
        return 'W';
    case CPLAT_TRACE_LEVEL_FORCE_INFO:
    case CPLAT_TRACE_LEVEL_INFO:
        return 'I';
    case CPLAT_TRACE_LEVEL_FORCE_VERBOSE:
    case CPLAT_TRACE_LEVEL_VERBOSE:
        return 'V';
    case CPLAT_TRACE_LEVEL_FORCE_DEBUG:
    case CPLAT_TRACE_LEVEL_DEBUG:
        return 'D';
    case CPLAT_TRACE_LEVEL_FORCE_NONE:
    case CPLAT_TRACE_LEVEL_NONE:
    default:
        return 'D';
    }
}
