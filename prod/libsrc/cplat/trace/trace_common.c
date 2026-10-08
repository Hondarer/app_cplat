/**
 *******************************************************************************
 *  @file           trace_common.c
 *  @brief          トレース機能の共通ヘルパー (内部共有) を実装します。
 *
 *  tracer 本体と各バックエンドで重複していたタイムスタンプ解決と
 *  トレース レベル表現の変換を、本ファイルの 1 実装に集約します。
 *******************************************************************************
 */

#include <cplat/trace/trace_common.h>

/**
 *  @brief          タイムスタンプが有効範囲か判定します。
 *  @param[in]      timestamp  判定対象のタイムスタンプです。NULL も指定できます。
 *  @return         有効な場合は 1、NULL または範囲外の場合は 0 を返します。
 */
static int trace_timestamp_is_valid(const cplat_timespec *timestamp)
{
    return timestamp != NULL && timestamp->tv_nsec >= 0 && timestamp->tv_nsec < 1000000000;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_trace_resolve_timestamp(const cplat_timespec *timestamp, cplat_timespec *resolved,
                                           int *fallback_used)
{
    if (resolved == NULL)
    {
        return -1;
    }
    if (fallback_used != NULL)
    {
        *fallback_used = 0;
    }

    if (timestamp != NULL)
    {
        if (trace_timestamp_is_valid(timestamp))
        {
            *resolved = *timestamp;
            return 0;
        }
        if (fallback_used != NULL)
        {
            *fallback_used = 1;
        }
    }

    cplat_clock_get_realtime(resolved);
    if (trace_timestamp_is_valid(resolved))
    {
        return 0;
    }
    return -1;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_trace_format_local_timestamp(char *buf, const size_t buf_size, const cplat_timespec *timestamp)
{
    if (!trace_timestamp_is_valid(timestamp))
    {
        return -1;
    }
    return cplat_clock_format_realtime_iso8601_local(buf, buf_size, timestamp);
}

/* Doxygen コメントは、ヘッダーに記載 */

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
