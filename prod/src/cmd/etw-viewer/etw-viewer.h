#ifndef ETW_VIEWER_PRIVATE_H
#define ETW_VIEWER_PRIVATE_H

#include <cplat/trace/etw.h>
#include <stddef.h>
#include <stdint.h>

/** @brief ETW セッション名を格納する配列の大きさ [byte] (終端 NUL を含む) です。 */
#define ETW_VIEWER_SESSION_NAME_MAX 128U

typedef struct etw_viewer_options
{
    uint32_t process_id_filter;
    int has_process_id_filter;
    int need_help;
} etw_viewer_options;

typedef struct etw_viewer_context
{
    uint32_t process_id_filter;
    int has_process_id_filter;
} etw_viewer_context;

#ifdef __cplusplus
extern "C"
{
#endif

    void etw_viewer_options_init(etw_viewer_options *options);
    int etw_viewer_build_default_session_name(unsigned long process_id, char *buffer, size_t buffer_size);
    const char *etw_viewer_level_name(int level);
    int etw_viewer_format_timestamp_utc(int64_t timestamp_100ns, char *buffer, size_t buffer_size);
    void etw_viewer_handle_event(const cplat_etw_event *event, void *context);

#ifdef __cplusplus
}
#endif

#endif /* ETW_VIEWER_PRIVATE_H */
