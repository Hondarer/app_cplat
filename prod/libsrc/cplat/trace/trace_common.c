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
    return timestamp != NULL && timestamp->tv_nsec >= 0 && timestamp->tv_nsec < 1000000000; /* tv_nsec は 1 [s] 未満 */
}

/* Doxygen コメントは、ヘッダーに記載 */

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

    /*
     * 指定されたタイムスタンプが有効であれば採用する。
     * 無効な場合は代替フラグを立てて現在時刻による解決へフォールバックする。
     */
    if (timestamp != NULL)
    {
        if (trace_timestamp_is_valid(timestamp))
        {
            *resolved_out = *timestamp;
            return CPLAT_OK;
        }

        if (fallback_used_out != NULL)
        {
            *fallback_used_out = 1;
        }
    }

    /* タイムスタンプが未指定または無効値の場合、現在時刻を取得して解決する。 */
    int ret = cplat_clock_get_realtime(resolved_out);
    if (ret != CPLAT_OK)
    {
        return ret;
    }

    if (trace_timestamp_is_valid(resolved_out))
    {
        return CPLAT_OK;
    }

    return CPLAT_ERR_UNKNOWN;
}

/* Doxygen コメントは、ヘッダーに記載 */

int cplat_internal_trace_format_local_timestamp(char *dest, const size_t dest_size, const cplat_timespec *timestamp)
{
    if (!trace_timestamp_is_valid(timestamp))
    {
        return CPLAT_ERR_INVALID_ARGUMENT;
    }

    return cplat_clock_format_realtime_iso8601_local(dest, dest_size, timestamp);
}

/* Doxygen コメントは、ヘッダーに記載 */

char cplat_internal_trace_level_char(const cplat_trace_level level)
{
    switch (level)
    {
    /*
     * 強制出力かどうかは絞り込みの結果であり、記録の重大度ではないため、表記を分けない。
     * 正本は app/cplat/docs/functional-spec/trace.md の「詳細度によらない出力」節である。
     */
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
