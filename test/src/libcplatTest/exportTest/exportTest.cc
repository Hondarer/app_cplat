#include <testfw.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

#include <cplat/base/error.h>
#include <cplat/base/platform.h>
#include <cplat/argparser/argparser.h>
#include <cplat/clock/clock.h>
#include <cplat/clock/timespec.h>
#include <cplat/compress/compress.h>
#include <cplat/console/console.h>
#include <cplat/crt/fcntl.h>
#include <cplat/crt/file.h>
#include <cplat/crt/path.h>
#include <cplat/crt/stdio.h>
#include <cplat/crt/stdlib.h>
#include <cplat/crt/string.h>
#include <cplat/crt/sys/stat.h>
#include <cplat/crt/time.h>
#include <cplat/crt/unistd.h>
#include <cplat/base/error_message.h>
#include <cplat/crypto/crypto.h>
#include <cplat/crypto/random.h>
#include <cplat/net/byteorder.h>
#include <cplat/net/endpoint.h>
#include <cplat/net/socket.h>
#include <cplat/hashtable/hashtable.h>
#include <cplat/mmap/mmap.h>
#include <cplat/prompt/pinned_prompt.h>
#include <cplat/prompt/prompt.h>
#include <cplat/runtime/elevated_process.h>
#include <cplat/locale/ui_language.h>
#include <cplat/runtime/host.h>
#include <cplat/runtime/memory_lock.h>
#include <cplat/regex/regex.h>

#include <cplat/runtime/module.h>
#include <cplat/runtime/process.h>
#include <cplat/runtime/shutdown.h>
#include <cplat/runtime/sym_loader.h>
#include <cplat/string_catalog/filter.h>
#include <cplat/string_catalog/string_catalog.h>
#include <cplat/sync/sync.h>
#include <cplat/trace/trace_file.h>
#include <cplat/trace/tracer.h>

#if defined(PLATFORM_WINDOWS)
    #include <cplat/crt/wchar_conv.h>
    #include <cplat/trace/etw.h>
    #include <cplat/trace/eventlog.h>
    #include <cplat/win32/win32.h>
#elif defined(PLATFORM_LINUX)
    #include <cplat/trace/syslog.h>
#endif /* PLATFORM_ */

// libcplat が公開エクスポートすべき関数の一覧。
// 関数の追加・削除は、このテーブルのみを編集する。
// 名前一致チェックとシグネチャの static_assert は、いずれも本テーブルから生成する。
#define CPLAT_EXPORT_TABLE_COMMON(EXPORT_ENTRY) \
    /* cplat/argparser/argparser.h */ \
    EXPORT_ENTRY(cplat_argparser_handle_create, \
                 cplat_argparser *(CPLAT_API *)(int argc, char *const *argv, const cplat_argparser_options *options)) \
    EXPORT_ENTRY(cplat_argparser_init, void(CPLAT_API *)(int argc, char *const *argv, const char *description)) \
    EXPORT_ENTRY(cplat_argparser_handle_dispose, void(CPLAT_API *)(cplat_argparser * parser)) \
    EXPORT_ENTRY(cplat_argparser_handle_register_flag, \
                 int(CPLAT_API *)(cplat_argparser * parser, const char *short_name, const char *long_name, \
                                  const char *description, int *storage)) \
    EXPORT_ENTRY(cplat_argparser_register_flag, int(CPLAT_API *)(const char *short_name, const char *long_name, \
                                                                 const char *description, int *storage)) \
    EXPORT_ENTRY(cplat_argparser_handle_register_option_int, \
                 int(CPLAT_API *)(cplat_argparser * parser, const char *short_name, const char *long_name, \
                                  const char *value_name, const char *description, unsigned int flags, int *storage)) \
    EXPORT_ENTRY(cplat_argparser_register_option_int, \
                 int(CPLAT_API *)(const char *short_name, const char *long_name, const char *value_name, \
                                  const char *description, unsigned int flags, int *storage)) \
    EXPORT_ENTRY(cplat_argparser_handle_register_option_string, \
                 int(CPLAT_API *)(cplat_argparser * parser, const char *short_name, const char *long_name, \
                                  const char *value_name, const char *description, unsigned int flags, \
                                  const char **storage)) \
    EXPORT_ENTRY(cplat_argparser_register_option_string, \
                 int(CPLAT_API *)(const char *short_name, const char *long_name, const char *value_name, \
                                  const char *description, unsigned int flags, const char **storage)) \
    EXPORT_ENTRY(cplat_argparser_handle_register_option_int_array, \
                 int(CPLAT_API *)(cplat_argparser * parser, const char *short_name, const char *long_name, \
                                  const char *value_name, const char *description, unsigned int flags, int *storage, \
                                  size_t capacity, size_t *count)) \
    EXPORT_ENTRY(cplat_argparser_register_option_int_array, \
                 int(CPLAT_API *)(const char *short_name, const char *long_name, const char *value_name, \
                                  const char *description, unsigned int flags, int *storage, size_t capacity, \
                                  size_t *count)) \
    EXPORT_ENTRY(cplat_argparser_handle_register_option_string_array, \
                 int(CPLAT_API *)(cplat_argparser * parser, const char *short_name, const char *long_name, \
                                  const char *value_name, const char *description, unsigned int flags, \
                                  const char **storage, size_t capacity, size_t *count)) \
    EXPORT_ENTRY(cplat_argparser_register_option_string_array, \
                 int(CPLAT_API *)(const char *short_name, const char *long_name, const char *value_name, \
                                  const char *description, unsigned int flags, const char **storage, size_t capacity, \
                                  size_t *count)) \
    EXPORT_ENTRY(cplat_argparser_handle_register_positional_int, \
                 int(CPLAT_API *)(cplat_argparser * parser, const char *name, const char *description, \
                                  unsigned int flags, int *storage)) \
    EXPORT_ENTRY(cplat_argparser_register_positional_int, \
                 int(CPLAT_API *)(const char *name, const char *description, unsigned int flags, int *storage)) \
    EXPORT_ENTRY(cplat_argparser_handle_register_positional_string, \
                 int(CPLAT_API *)(cplat_argparser * parser, const char *name, const char *description, \
                                  unsigned int flags, const char **storage)) \
    EXPORT_ENTRY( \
        cplat_argparser_register_positional_string, \
        int(CPLAT_API *)(const char *name, const char *description, unsigned int flags, const char **storage)) \
    EXPORT_ENTRY(cplat_argparser_handle_register_positional_int_array, \
                 int(CPLAT_API *)(cplat_argparser * parser, const char *name, const char *description, \
                                  unsigned int flags, int *storage, size_t capacity, size_t *count)) \
    EXPORT_ENTRY(cplat_argparser_register_positional_int_array, \
                 int(CPLAT_API *)(const char *name, const char *description, unsigned int flags, int *storage, \
                                  size_t capacity, size_t *count)) \
    EXPORT_ENTRY(cplat_argparser_handle_register_positional_string_array, \
                 int(CPLAT_API *)(cplat_argparser * parser, const char *name, const char *description, \
                                  unsigned int flags, const char **storage, size_t capacity, size_t *count)) \
    EXPORT_ENTRY(cplat_argparser_register_positional_string_array, \
                 int(CPLAT_API *)(const char *name, const char *description, unsigned int flags, const char **storage, \
                                  size_t capacity, size_t *count)) \
    EXPORT_ENTRY(cplat_argparser_handle_parse, int(CPLAT_API *)(cplat_argparser * parser)) \
    EXPORT_ENTRY(cplat_argparser_parse, int(CPLAT_API *)(void)) \
    EXPORT_ENTRY(cplat_argparser_handle_get_error, int(CPLAT_API *)(const cplat_argparser *parser)) \
    EXPORT_ENTRY(cplat_argparser_get_error, int(CPLAT_API *)(void)) \
    EXPORT_ENTRY(cplat_argparser_handle_get_error_target, const char *(CPLAT_API *)(const cplat_argparser *parser)) \
    EXPORT_ENTRY(cplat_argparser_get_error_target, const char *(CPLAT_API *)(void)) \
    EXPORT_ENTRY(cplat_argparser_handle_get_error_index, int(CPLAT_API *)(const cplat_argparser *parser)) \
    EXPORT_ENTRY(cplat_argparser_get_error_index, int(CPLAT_API *)(void)) \
    EXPORT_ENTRY(cplat_argparser_handle_get_error_message, \
                 int(CPLAT_API *)(const cplat_argparser *parser, char *buffer, size_t buffer_size)) \
    EXPORT_ENTRY(cplat_argparser_get_error_message, int(CPLAT_API *)(char *buffer, size_t buffer_size)) \
    EXPORT_ENTRY(cplat_argparser_handle_get_usage, int(CPLAT_API *)(const cplat_argparser *parser, char *buffer, \
                                                                    size_t buffer_size, size_t *required_size)) \
    EXPORT_ENTRY(cplat_argparser_get_usage, int(CPLAT_API *)(char *buffer, size_t buffer_size, size_t *required_size)) \
    EXPORT_ENTRY(cplat_argparser_handle_print_usage, int(CPLAT_API *)(const cplat_argparser *parser, FILE *stream)) \
    EXPORT_ENTRY(cplat_argparser_print_usage, int(CPLAT_API *)(FILE * stream)) \
    EXPORT_ENTRY(cplat_argparser_handle_print_error_messages, \
                 int(CPLAT_API *)(const cplat_argparser *parser, FILE *stream)) \
    EXPORT_ENTRY(cplat_argparser_print_error_messages, int(CPLAT_API *)(FILE * stream)) \
    EXPORT_ENTRY(cplat_argparser_handle_get_register_error_count, size_t(CPLAT_API *)(const cplat_argparser *parser)) \
    EXPORT_ENTRY(cplat_argparser_get_register_error_count, size_t(CPLAT_API *)(void)) \
    EXPORT_ENTRY(cplat_argparser_handle_get_register_error, \
                 int(CPLAT_API *)(const cplat_argparser *parser, size_t index)) \
    EXPORT_ENTRY(cplat_argparser_get_register_error, int(CPLAT_API *)(size_t index)) \
    EXPORT_ENTRY(cplat_argparser_handle_get_register_error_target, \
                 const char *(CPLAT_API *)(const cplat_argparser *parser, size_t index)) \
    EXPORT_ENTRY(cplat_argparser_get_register_error_target, const char *(CPLAT_API *)(size_t index)) \
    EXPORT_ENTRY(cplat_argparser_handle_get_register_error_message, \
                 int(CPLAT_API *)(const cplat_argparser *parser, size_t index, char *buffer, size_t buffer_size)) \
    EXPORT_ENTRY(cplat_argparser_get_register_error_message, \
                 int(CPLAT_API *)(size_t index, char *buffer, size_t buffer_size)) \
    EXPORT_ENTRY(cplat_argparser_handle_print_register_error_messages, \
                 int(CPLAT_API *)(const cplat_argparser *parser, FILE *stream)) \
    EXPORT_ENTRY(cplat_argparser_print_register_error_messages, int(CPLAT_API *)(FILE * stream)) \
    /* cplat/hashtable/hashtable.h */ \
    EXPORT_ENTRY(cplat_hashtable_required_size, \
                 int(CPLAT_API *)(const cplat_hashtable_config *config, size_t *mgmt_size_out, size_t *data_size_out)) \
    EXPORT_ENTRY(cplat_hashtable_create, \
                 int(CPLAT_API *)(const cplat_hashtable_config *config, void *buf_mgmt, size_t buf_mgmt_size, \
                                  void *buf_data, size_t buf_data_size, cplat_hashtable **ht_out)) \
    EXPORT_ENTRY(cplat_hashtable_create_growable, \
                 int(CPLAT_API *)(const cplat_hashtable_config *initial_config, \
                                  const cplat_hashtable_growth_config *growth_config, cplat_hashtable **ht_out)) \
    EXPORT_ENTRY(cplat_hashtable_attach, int(CPLAT_API *)(void *buf_mgmt, size_t buf_mgmt_size, void *buf_data, \
                                                          size_t buf_data_size, cplat_hashtable **ht_out)) \
    EXPORT_ENTRY(cplat_hashtable_validate, int(CPLAT_API *)(const cplat_hashtable *ht)) \
    EXPORT_ENTRY(cplat_hashtable_get_config_ref, \
                 int(CPLAT_API *)(const cplat_hashtable *ht, const cplat_hashtable_config **config_out)) \
    EXPORT_ENTRY(cplat_hashtable_get_config_val, \
                 int(CPLAT_API *)(const cplat_hashtable *ht, cplat_hashtable_config *config_out)) \
    EXPORT_ENTRY(cplat_hashtable_buffer_size, \
                 int(CPLAT_API *)(const cplat_hashtable *ht, size_t *mgmt_size_out, size_t *data_size_out)) \
    EXPORT_ENTRY(cplat_hashtable_buffer_ref, \
                 int(CPLAT_API *)(const cplat_hashtable *ht, const void **mgmt_out, const void **data_out)) \
    EXPORT_ENTRY(cplat_hashtable_add, int(CPLAT_API *)(cplat_hashtable * ht, const void *key, const void *value, \
                                                       cplat_hashtable_add_deleted_policy deleted_policy)) \
    EXPORT_ENTRY(cplat_hashtable_upsert, \
                 int(CPLAT_API *)(cplat_hashtable * ht, const void *key, const void *value, int *inserted_out)) \
    EXPORT_ENTRY(cplat_hashtable_insert_direct, \
                 int(CPLAT_API *)(cplat_hashtable * ht, uint64_t record, const void *key, int status, \
                                  const void *value, const cplat_timespec *timestamp, uint64_t generation)) \
    EXPORT_ENTRY(cplat_hashtable_update, int(CPLAT_API *)(cplat_hashtable * ht, const void *key, const void *value)) \
    EXPORT_ENTRY(cplat_hashtable_update_rec, \
                 int(CPLAT_API *)(cplat_hashtable * ht, uint64_t record, const void *value)) \
    EXPORT_ENTRY(cplat_hashtable_find_value_ref, \
                 int(CPLAT_API *)(const cplat_hashtable *ht, const void *key, const void **value_out)) \
    EXPORT_ENTRY(cplat_hashtable_find_value_copy, \
                 int(CPLAT_API *)(const cplat_hashtable *ht, const void *key, void *dest, size_t dest_size, \
                                  size_t *required_size_out)) \
    EXPORT_ENTRY(cplat_hashtable_find_recno, \
                 int(CPLAT_API *)(const cplat_hashtable *ht, const void *key, uint64_t *record_out)) \
    EXPORT_ENTRY(cplat_hashtable_get_key_ref, \
                 int(CPLAT_API *)(const cplat_hashtable *ht, uint64_t record, const void **key_out)) \
    EXPORT_ENTRY(cplat_hashtable_get_key_copy, \
                 int(CPLAT_API *)(const cplat_hashtable *ht, uint64_t record, void *dest, size_t dest_size, \
                                  size_t *required_size_out)) \
    EXPORT_ENTRY(cplat_hashtable_get_value_ref, \
                 int(CPLAT_API *)(const cplat_hashtable *ht, uint64_t record, const void **value_out)) \
    EXPORT_ENTRY(cplat_hashtable_get_value_copy, \
                 int(CPLAT_API *)(const cplat_hashtable *ht, uint64_t record, void *dest, size_t dest_size, \
                                  size_t *required_size_out)) \
    EXPORT_ENTRY(cplat_hashtable_get_status, \
                 int(CPLAT_API *)(const cplat_hashtable *ht, uint64_t record, int *status_out)) \
    EXPORT_ENTRY(cplat_hashtable_next_record, \
                 int(CPLAT_API *)(const cplat_hashtable *ht, uint64_t from, unsigned int status_mask, \
                                  uint64_t *record_out, int *has_record_out)) \
    EXPORT_ENTRY(cplat_hashtable_get_timestamp_ref, \
                 int(CPLAT_API *)(const cplat_hashtable *ht, uint64_t record, const cplat_timespec **timestamp_out)) \
    EXPORT_ENTRY(cplat_hashtable_get_timestamp_val, \
                 int(CPLAT_API *)(const cplat_hashtable *ht, uint64_t record, cplat_timespec *timestamp_out)) \
    EXPORT_ENTRY(cplat_hashtable_get_generation, \
                 int(CPLAT_API *)(const cplat_hashtable *ht, uint64_t record, uint64_t *generation_out)) \
    EXPORT_ENTRY(cplat_hashtable_get_table_timestamp_ref, \
                 int(CPLAT_API *)(const cplat_hashtable *ht, const cplat_timespec **timestamp_out)) \
    EXPORT_ENTRY(cplat_hashtable_get_table_timestamp_val, \
                 int(CPLAT_API *)(const cplat_hashtable *ht, cplat_timespec *timestamp_out)) \
    EXPORT_ENTRY(cplat_hashtable_get_table_generation, \
                 int(CPLAT_API *)(const cplat_hashtable *ht, uint64_t *generation_out)) \
    EXPORT_ENTRY(cplat_hashtable_find_timestamp_ref, \
                 int(CPLAT_API *)(const cplat_hashtable *ht, const void *key, const cplat_timespec **timestamp_out)) \
    EXPORT_ENTRY(cplat_hashtable_find_timestamp_val, \
                 int(CPLAT_API *)(const cplat_hashtable *ht, const void *key, cplat_timespec *timestamp_out)) \
    EXPORT_ENTRY(cplat_hashtable_find_generation, \
                 int(CPLAT_API *)(const cplat_hashtable *ht, const void *key, uint64_t *generation_out)) \
    EXPORT_ENTRY(cplat_hashtable_count_status, int(CPLAT_API *)(const cplat_hashtable *ht, size_t *in_use_out, \
                                                                size_t *deleted_out, size_t *empty_out)) \
    EXPORT_ENTRY(cplat_hashtable_count, int(CPLAT_API *)(const cplat_hashtable *ht, size_t *count_out)) \
    EXPORT_ENTRY(cplat_hashtable_deleted_count, int(CPLAT_API *)(const cplat_hashtable *ht, size_t *count_out)) \
    EXPORT_ENTRY(cplat_hashtable_empty_count, int(CPLAT_API *)(const cplat_hashtable *ht, size_t *count_out)) \
    EXPORT_ENTRY(cplat_hashtable_delete, int(CPLAT_API *)(cplat_hashtable * ht, const void *key)) \
    EXPORT_ENTRY(cplat_hashtable_delete_rec, int(CPLAT_API *)(cplat_hashtable * ht, uint64_t record)) \
    EXPORT_ENTRY(cplat_hashtable_push_deleted, int(CPLAT_API *)(cplat_hashtable * ht)) \
    EXPORT_ENTRY(cplat_hashtable_purge_deleted, int(CPLAT_API *)(cplat_hashtable * ht)) \
    EXPORT_ENTRY(cplat_hashtable_compact, int(CPLAT_API *)(cplat_hashtable * ht)) \
    EXPORT_ENTRY(cplat_hashtable_resize, \
                 int(CPLAT_API *)(cplat_hashtable * ht, const cplat_hashtable_config *new_config)) \
    EXPORT_ENTRY(cplat_hashtable_rebuild_into, \
                 int(CPLAT_API *)(const cplat_hashtable *src, const cplat_hashtable_config *new_config, \
                                  void *buf_mgmt, size_t buf_mgmt_size, void *buf_data, size_t buf_data_size, \
                                  cplat_hashtable **ht_out)) \
    EXPORT_ENTRY(cplat_hashtable_clear, int(CPLAT_API *)(cplat_hashtable * ht)) \
    EXPORT_ENTRY(cplat_hashtable_dispose, void(CPLAT_API *)(cplat_hashtable * ht)) \
    /* cplat/clock/clock.h */ \
    EXPORT_ENTRY(cplat_clock_get_monotonic_ms, uint64_t(CPLAT_API *)(void)) \
    EXPORT_ENTRY(cplat_clock_get_monotonic, void(CPLAT_API *)(cplat_timespec * ts)) \
    EXPORT_ENTRY(cplat_clock_get_realtime, void(CPLAT_API *)(cplat_timespec * ts)) \
    EXPORT_ENTRY(cplat_clock_format_realtime_iso8601_local, \
                 int(CPLAT_API *)(char *buf, size_t buf_size, const cplat_timespec *timestamp)) \
    EXPORT_ENTRY(cplat_clock_format_realtime_iso8601_utc, \
                 int(CPLAT_API *)(char *buf, size_t buf_size, const cplat_timespec *timestamp)) \
    EXPORT_ENTRY(cplat_clock_get_realtime_utc, void(CPLAT_API *)(struct tm * utc_tm, int32_t *tv_nsec)) \
    EXPORT_ENTRY(cplat_clock_get_realtime_deadline_ms, \
                 void(CPLAT_API *)(uint64_t timeout_ms, struct timespec *abs_timeout)) \
    /* cplat/clock/timespec.h */ \
    EXPORT_ENTRY(cplat_timespec_normalize, void(CPLAT_API *)(cplat_timespec * ts)) \
    EXPORT_ENTRY(cplat_timespec_add, \
                 void(CPLAT_API *)(const cplat_timespec *a, const cplat_timespec *b, cplat_timespec *result)) \
    EXPORT_ENTRY(cplat_timespec_sub, \
                 void(CPLAT_API *)(const cplat_timespec *a, const cplat_timespec *b, cplat_timespec *result)) \
    EXPORT_ENTRY(cplat_timespec_cmp, int(CPLAT_API *)(const cplat_timespec *a, const cplat_timespec *b)) \
    EXPORT_ENTRY(cplat_timespec_add_ms, \
                 void(CPLAT_API *)(const cplat_timespec *ts, uint64_t timeout_ms, cplat_timespec *result)) \
    EXPORT_ENTRY(cplat_timespec_diff_ms, int64_t(CPLAT_API *)(const cplat_timespec *end, const cplat_timespec *start)) \
    EXPORT_ENTRY(cplat_timespec_to_native, void(CPLAT_API *)(const cplat_timespec *ts, struct timespec *native)) \
    EXPORT_ENTRY(cplat_timespec_from_native, void(CPLAT_API *)(const struct timespec *native, cplat_timespec *ts)) \
    /* cplat/compress/compress.h */ \
    EXPORT_ENTRY(cplat_compress, int(CPLAT_API *)(uint8_t *dst, size_t *dst_len, const uint8_t *src, size_t src_len)) \
    EXPORT_ENTRY(cplat_decompress, \
                 int(CPLAT_API *)(uint8_t *dst, size_t *dst_len, const uint8_t *src, size_t src_len)) \
    /* cplat/console/console.h */ \
    EXPORT_ENTRY(cplat_console_init, void(CPLAT_API *)(void)) \
    EXPORT_ENTRY(cplat_console_dispose, void(CPLAT_API *)(void)) \
    EXPORT_ENTRY(cplat_console_attach_parent, int(CPLAT_API *)(int *argc, char **argv, int *attached_out)) \
    EXPORT_ENTRY(cplat_console_write, int(CPLAT_API *)(cplat_stream stream, const char *text)) \
    /* cplat/crt/fcntl.h */ \
    EXPORT_ENTRY(cplat_open, int(CPLAT_API *)(const char *path, int flags, int mode, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_open_fmt, \
                 int(CPLAT_API *)(int flags, int mode, cplat_error *detail_out, const char *format, ...)) \
    EXPORT_ENTRY(cplat_vopen_fmt, \
                 int(CPLAT_API *)(int flags, int mode, cplat_error *detail_out, const char *format, va_list args)) \
    /* cplat/crt/file.h */ \
    EXPORT_ENTRY(cplat_file_init, void(CPLAT_API *)(cplat_file * file)) \
    EXPORT_ENTRY(cplat_file_open, \
                 int(CPLAT_API *)(cplat_file * file, const char *path, int flags, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_file_write, \
                 int(CPLAT_API *)(cplat_file * file, const void *buf, size_t len, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_file_read, int(CPLAT_API *)(cplat_file * file, void *buf, size_t len, size_t *read_out, \
                                                   cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_file_get_size, \
                 int(CPLAT_API *)(const cplat_file *file, size_t *size_out, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_file_set_size, int(CPLAT_API *)(cplat_file * file, size_t size, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_file_get_id, \
                 int(CPLAT_API *)(const cplat_file *file, cplat_file_id *id_out, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_file_get_path_id, \
                 int(CPLAT_API *)(const char *path, cplat_file_id *id_out, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_file_get_modified_timestamp, \
                 int(CPLAT_API *)(const cplat_file *file, cplat_timespec *timestamp_out, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_file_set_modified_timestamp, \
                 int(CPLAT_API *)(cplat_file * file, const cplat_timespec *timestamp, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_file_get_path_modified_timestamp, \
                 int(CPLAT_API *)(const char *path, cplat_timespec *timestamp_out, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_file_set_path_modified_timestamp, \
                 int(CPLAT_API *)(const char *path, const cplat_timespec *timestamp, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_file_flush, int(CPLAT_API *)(cplat_file * file, cplat_error * detail_out)) \
    EXPORT_ENTRY(cplat_file_close, int(CPLAT_API *)(cplat_file * file, cplat_error * detail_out)) \
    /* cplat/crt/path.h */ \
    EXPORT_ENTRY(cplat_path_normalize_sep, char *(CPLAT_API *)(char *path)) \
    EXPORT_ENTRY(cplat_path_get_full, \
                 int(CPLAT_API *)(char *path_out, size_t path_size, cplat_error *detail_out, const char *path)) \
    EXPORT_ENTRY(cplat_path_equal, \
                 int(CPLAT_API *)(const char *lhs, const char *rhs, int *equal_out, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_path_get_temp_dir, int(CPLAT_API *)(char *path_out, size_t path_size, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_path_concat_n, \
                 int(CPLAT_API *)(char *path_out, size_t path_size, cplat_error *detail_out, size_t part_count, ...)) \
    EXPORT_ENTRY(cplat_vpath_concat_n, int(CPLAT_API *)(char *path_out, size_t path_size, cplat_error *detail_out, \
                                                        size_t part_count, va_list args)) \
    EXPORT_ENTRY(cplat_path_basename, const char *(CPLAT_API *)(const char *path)) \
    EXPORT_ENTRY(cplat_path_dirname, \
                 int(CPLAT_API *)(char *path_out, size_t path_size, cplat_error *detail_out, const char *path)) \
    EXPORT_ENTRY(cplat_path_extension, const char *(CPLAT_API *)(const char *path)) \
    EXPORT_ENTRY(cplat_path_strip_extension, \
                 int(CPLAT_API *)(char *path_out, size_t path_size, cplat_error *detail_out, const char *path)) \
    EXPORT_ENTRY(cplat_path_join_n, \
                 int(CPLAT_API *)(char *path_out, size_t path_size, cplat_error *detail_out, size_t part_count, ...)) \
    EXPORT_ENTRY(cplat_vpath_join_n, int(CPLAT_API *)(char *path_out, size_t path_size, cplat_error *detail_out, \
                                                      size_t part_count, va_list args)) \
    /* cplat/crt/stdio.h */ \
    EXPORT_ENTRY(cplat_fopen, FILE *(CPLAT_API *)(const char *path, const char *modes, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_freopen, \
                 FILE *(CPLAT_API *)(const char *path, const char *modes, FILE *stream, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_fclose, int(CPLAT_API *)(FILE * stream, cplat_error * detail_out)) \
    EXPORT_ENTRY(cplat_fflush, int(CPLAT_API *)(FILE * stream, cplat_error * detail_out)) \
    EXPORT_ENTRY(cplat_fread, \
                 size_t(CPLAT_API *)(void *buffer, size_t size, size_t count, FILE *stream, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_fwrite, size_t(CPLAT_API *)(const void *buffer, size_t size, size_t count, FILE *stream, \
                                                   cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_remove, int(CPLAT_API *)(const char *path, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_rename, int(CPLAT_API *)(const char *oldpath, const char *newpath, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_scanf, int(CPLAT_API *)(const char *format, ...)) \
    EXPORT_ENTRY(cplat_vscanf, int(CPLAT_API *)(const char *format, va_list args)) \
    EXPORT_ENTRY(cplat_fscanf, int(CPLAT_API *)(FILE * stream, const char *format, ...)) \
    EXPORT_ENTRY(cplat_vfscanf, int(CPLAT_API *)(FILE * stream, const char *format, va_list args)) \
    EXPORT_ENTRY(cplat_snprintf, int(CPLAT_API *)(char *dest, size_t dest_size, const char *format, ...)) \
    EXPORT_ENTRY(cplat_vsnprintf, int(CPLAT_API *)(char *dest, size_t dest_size, const char *format, va_list args)) \
    EXPORT_ENTRY(cplat_fgets, int(CPLAT_API *)(char *dest, size_t dest_size, FILE *stream, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_fprintf, int(CPLAT_API *)(FILE * stream, const char *format, ...)) \
    EXPORT_ENTRY(cplat_vfprintf, int(CPLAT_API *)(FILE * stream, const char *format, va_list args)) \
    EXPORT_ENTRY(cplat_fseek, int(CPLAT_API *)(FILE * stream, int64_t offset, int whence)) \
    EXPORT_ENTRY(cplat_ftell, int64_t(CPLAT_API *)(FILE * stream)) \
    EXPORT_ENTRY(cplat_fopen_fmt, \
                 FILE *(CPLAT_API *)(const char *modes, cplat_error *detail_out, const char *format, ...)) \
    EXPORT_ENTRY(cplat_vfopen_fmt, \
                 FILE *(CPLAT_API *)(const char *modes, cplat_error *detail_out, const char *format, va_list args)) \
    EXPORT_ENTRY(cplat_remove_fmt, int(CPLAT_API *)(cplat_error * detail_out, const char *format, ...)) \
    EXPORT_ENTRY(cplat_vremove_fmt, int(CPLAT_API *)(cplat_error * detail_out, const char *format, va_list args)) \
    EXPORT_ENTRY(cplat_fopen_temp, FILE *(CPLAT_API *)(const char *prefix, const char *modes, char *path_out, \
                                                       size_t path_size, cplat_error *detail_out)) \
    /* cplat/crt/stdlib.h */ \
    EXPORT_ENTRY(cplat_getenv, int(CPLAT_API *)(const char *name, char *buf, size_t buf_size, int *exists_out, \
                                                cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_setenv, \
                 int(CPLAT_API *)(const char *name, const char *value, int overwrite, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_unsetenv, int(CPLAT_API *)(const char *name, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_parse_int64, int(CPLAT_API *)(int64_t *value_out, const char *text, int base)) \
    EXPORT_ENTRY(cplat_parse_uint64, int(CPLAT_API *)(uint64_t *value_out, const char *text, int base)) \
    EXPORT_ENTRY(cplat_parse_int, int(CPLAT_API *)(int *value_out, const char *text, int base)) \
    EXPORT_ENTRY(cplat_parse_double, int(CPLAT_API *)(double *value_out, const char *text)) \
    EXPORT_ENTRY(cplat_malloc, void *(CPLAT_API *)(size_t size)) \
    EXPORT_ENTRY(cplat_malloc_zerofill, void *(CPLAT_API *)(size_t size)) \
    EXPORT_ENTRY(cplat_calloc, void *(CPLAT_API *)(size_t count, size_t size)) \
    EXPORT_ENTRY(cplat_realloc, void *(CPLAT_API *)(void *ptr, size_t count, size_t size)) \
    EXPORT_ENTRY(cplat_realloc_zerofill, void *(CPLAT_API *)(void *ptr, size_t old_count, size_t count, size_t size)) \
    EXPORT_ENTRY(cplat_free, void(CPLAT_API *)(void *ptr)) \
    /* cplat/crt/string.h */ \
    EXPORT_ENTRY(cplat_strcpy, int(CPLAT_API *)(char *dest, size_t dest_size, const char *src)) \
    EXPORT_ENTRY(cplat_strncpy, int(CPLAT_API *)(char *dest, size_t dest_size, const char *src, size_t count)) \
    EXPORT_ENTRY(cplat_strcat, int(CPLAT_API *)(char *dest, size_t dest_size, const char *src)) \
    EXPORT_ENTRY(cplat_strncat, int(CPLAT_API *)(char *dest, size_t dest_size, const char *src, size_t count)) \
    EXPORT_ENTRY(cplat_strtok_r, char *(CPLAT_API *)(char *str, const char *delim, char **saveptr)) \
    EXPORT_ENTRY(cplat_strdup, char *(CPLAT_API *)(const char *src)) \
    EXPORT_ENTRY(cplat_strcasecmp, int(CPLAT_API *)(const char *lhs, const char *rhs)) \
    EXPORT_ENTRY(cplat_strncasecmp, int(CPLAT_API *)(const char *lhs, const char *rhs, size_t count)) \
    EXPORT_ENTRY(cplat_wcscpy, int(CPLAT_API *)(wchar_t * dest, size_t dest_size, const wchar_t *src)) \
    EXPORT_ENTRY(cplat_sscanf, int(CPLAT_API *)(const char *buffer, const char *format, ...)) \
    EXPORT_ENTRY(cplat_vsscanf, int(CPLAT_API *)(const char *buffer, const char *format, va_list args)) \
    /* cplat/crt/sys/stat.h */ \
    EXPORT_ENTRY(cplat_stat, int(CPLAT_API *)(cplat_file_stat_t * buf, cplat_error * detail_out, const char *path)) \
    EXPORT_ENTRY(cplat_mkdir, int(CPLAT_API *)(const char *path, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_makedirs, int(CPLAT_API *)(const char *path, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_rmdir, int(CPLAT_API *)(const char *path, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_stat_fmt, \
                 int(CPLAT_API *)(cplat_file_stat_t * buf, cplat_error * detail_out, const char *format, ...)) \
    EXPORT_ENTRY(cplat_vstat_fmt, int(CPLAT_API *)(cplat_file_stat_t * buf, cplat_error * detail_out, \
                                                   const char *format, va_list args)) \
    EXPORT_ENTRY(cplat_mkdir_fmt, int(CPLAT_API *)(cplat_error * detail_out, const char *format, ...)) \
    EXPORT_ENTRY(cplat_vmkdir_fmt, int(CPLAT_API *)(cplat_error * detail_out, const char *format, va_list args)) \
    EXPORT_ENTRY(cplat_file_stat_is_regular, int(CPLAT_API *)(const cplat_file_stat_t *file_stat)) \
    /* cplat/crt/time.h */ \
    EXPORT_ENTRY(cplat_gmtime, int(CPLAT_API *)(struct tm * utc_tm, const time_t *timep)) \
    EXPORT_ENTRY(cplat_localtime, int(CPLAT_API *)(struct tm * local_tm, const time_t *timep)) \
    EXPORT_ENTRY(cplat_ctime, int(CPLAT_API *)(char *buf, size_t buf_size, const time_t *timep)) \
    /* cplat/crt/unistd.h */ \
    EXPORT_ENTRY(cplat_isatty, int(CPLAT_API *)(cplat_stream stream)) \
    EXPORT_ENTRY(cplat_lseek, int64_t(CPLAT_API *)(int fd, int64_t offset, int whence, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_close, int(CPLAT_API *)(int fd, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_dup, int(CPLAT_API *)(int fd, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_dup2, int(CPLAT_API *)(int oldfd, int newfd, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_read, int64_t(CPLAT_API *)(int fd, void *buf, size_t count, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_write, int64_t(CPLAT_API *)(int fd, const void *buf, size_t count, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_access, int(CPLAT_API *)(const char *path, int mode, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_access_fmt, int(CPLAT_API *)(int mode, cplat_error *detail_out, const char *format, ...)) \
    EXPORT_ENTRY(cplat_vaccess_fmt, \
                 int(CPLAT_API *)(int mode, cplat_error *detail_out, const char *format, va_list args)) \
    /* cplat/crypto/crypto.h */ \
    EXPORT_ENTRY(cplat_crypto_encrypt, \
                 int(CPLAT_API *)(uint8_t *dst, size_t *dst_len, const uint8_t *src, size_t src_len, \
                                  const uint8_t *key, const uint8_t *nonce, const uint8_t *aad, size_t aad_len)) \
    EXPORT_ENTRY(cplat_crypto_decrypt, \
                 int(CPLAT_API *)(uint8_t *dst, size_t *dst_len, const uint8_t *src, size_t src_len, \
                                  const uint8_t *key, const uint8_t *nonce, const uint8_t *aad, size_t aad_len)) \
    EXPORT_ENTRY(cplat_passphrase_to_key, \
                 int(CPLAT_API *)(uint8_t *key, const uint8_t *passphrase, size_t passphrase_len)) \
    /* cplat/base/error.h */ \
    EXPORT_ENTRY(cplat_error_clear, void(CPLAT_API *)(cplat_error * error)) \
    EXPORT_ENTRY(cplat_error_capture_errno, void(CPLAT_API *)(cplat_error * error, int errno_value)) \
    EXPORT_ENTRY(cplat_error_capture_current_errno, void(CPLAT_API *)(cplat_error * error)) \
    EXPORT_ENTRY(cplat_error_get_last, void(CPLAT_API *)(cplat_error * error_out)) \
    EXPORT_ENTRY(cplat_error_set_last, void(CPLAT_API *)(const cplat_error *error)) \
    EXPORT_ENTRY(cplat_error_clear_last, void(CPLAT_API *)(void)) \
    EXPORT_ENTRY(cplat_error_is_set, int(CPLAT_API *)(const cplat_error *error)) \
    EXPORT_ENTRY(cplat_error_get_domain, cplat_error_domain(CPLAT_API *)(const cplat_error *error)) \
    EXPORT_ENTRY(cplat_error_get_errno, int(CPLAT_API *)(const cplat_error *error)) \
    EXPORT_ENTRY(cplat_error_to_result, int(CPLAT_API *)(const cplat_error *error)) \
    EXPORT_ENTRY(cplat_error_get_cause, cplat_error_cause(CPLAT_API *)(const cplat_error *error)) \
    EXPORT_ENTRY(cplat_error_is, int(CPLAT_API *)(const cplat_error *error, cplat_error_cause cause)) \
    /* cplat/base/error_message.h */ \
    EXPORT_ENTRY(cplat_result_to_string, const char *(CPLAT_API *)(int result)) \
    EXPORT_ENTRY(cplat_error_message, int(CPLAT_API *)(char *buf, size_t buf_size, const cplat_error *error)) \
    /* cplat/crypto/random.h */ \
    EXPORT_ENTRY(cplat_random_bytes, int(CPLAT_API *)(void *buf, size_t size)) \
    /* cplat/locale/ui_language.h */ \
    EXPORT_ENTRY(cplat_ui_language_get_tag, int(CPLAT_API *)(char *tag_out, size_t tag_size)) \
    /* cplat/mmap/mmap.h */ \
    EXPORT_ENTRY(cplat_mmap_attach, int(CPLAT_API *)(const char *path, cplat_mmap_access access, size_t create_size, \
                                                     cplat_mmap **map, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_mmap_get_address, void *(CPLAT_API *)(const cplat_mmap *map)) \
    EXPORT_ENTRY(cplat_mmap_get_size, size_t(CPLAT_API *)(const cplat_mmap *map)) \
    EXPORT_ENTRY(cplat_mmap_flush, \
                 int(CPLAT_API *)(cplat_mmap * map, void *address, size_t length, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_mmap_detach, int(CPLAT_API *)(cplat_mmap * map, cplat_error * detail_out)) \
    /* cplat/net/byteorder.h */ \
    EXPORT_ENTRY(cplat_hton16, uint16_t(CPLAT_API *)(uint16_t value)) \
    EXPORT_ENTRY(cplat_ntoh16, uint16_t(CPLAT_API *)(uint16_t value)) \
    EXPORT_ENTRY(cplat_hton32, uint32_t(CPLAT_API *)(uint32_t value)) \
    EXPORT_ENTRY(cplat_ntoh32, uint32_t(CPLAT_API *)(uint32_t value)) \
    EXPORT_ENTRY(cplat_hton64, uint64_t(CPLAT_API *)(uint64_t value)) \
    EXPORT_ENTRY(cplat_ntoh64, uint64_t(CPLAT_API *)(uint64_t value)) \
    /* cplat/net/endpoint.h */ \
    EXPORT_ENTRY(cplat_ipv4_parse, int(CPLAT_API *)(const char *text, uint32_t *address_out)) \
    EXPORT_ENTRY(cplat_ipv4_resolve, \
                 int(CPLAT_API *)(const char *text, uint32_t *address_out, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_ipv4_to_string, \
                 int(CPLAT_API *)(uint32_t address, char *buffer, size_t buffer_size, cplat_error *detail_out)) \
    /* cplat/net/socket.h */ \
    EXPORT_ENTRY(cplat_socket_open, \
                 int(CPLAT_API *)(cplat_socket_kind kind, cplat_socket * sock_out, cplat_error * detail_out)) \
    EXPORT_ENTRY(cplat_socket_close, void(CPLAT_API *)(cplat_socket sock)) \
    EXPORT_ENTRY(cplat_socket_shutdown, void(CPLAT_API *)(cplat_socket sock)) \
    EXPORT_ENTRY(cplat_socket_bind, \
                 int(CPLAT_API *)(cplat_socket sock, const cplat_ipv4_endpoint *endpoint, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_socket_listen, int(CPLAT_API *)(cplat_socket sock, int backlog, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_socket_accept, int(CPLAT_API *)(cplat_socket sock, cplat_ipv4_endpoint * peer_out, \
                                                       cplat_socket * sock_out, cplat_error * detail_out)) \
    EXPORT_ENTRY(cplat_socket_connect, \
                 int(CPLAT_API *)(cplat_socket sock, const cplat_ipv4_endpoint *endpoint, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_socket_get_pending_error, int(CPLAT_API *)(cplat_socket sock, cplat_error * detail_out)) \
    EXPORT_ENTRY(cplat_socket_set_nonblocking, \
                 int(CPLAT_API *)(cplat_socket sock, int enable, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_socket_set_reuse_address, \
                 int(CPLAT_API *)(cplat_socket sock, int enable, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_socket_set_broadcast, int(CPLAT_API *)(cplat_socket sock, int enable, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_socket_set_multicast_interface, \
                 int(CPLAT_API *)(cplat_socket sock, uint32_t interface_address, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_socket_join_multicast_group, \
                 int(CPLAT_API *)(cplat_socket sock, uint32_t group_address, uint32_t interface_address, \
                                  cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_socket_leave_multicast_group, \
                 int(CPLAT_API *)(cplat_socket sock, uint32_t group_address, uint32_t interface_address, \
                                  cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_socket_send, int(CPLAT_API *)(cplat_socket sock, const void *buf, size_t len, size_t *sent_out, \
                                                     cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_socket_recv, int(CPLAT_API *)(cplat_socket sock, void *buf, size_t len, size_t *received_out, \
                                                     cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_socket_sendto, \
                 int(CPLAT_API *)(cplat_socket sock, const void *buf, size_t len, const cplat_ipv4_endpoint *endpoint, \
                                  size_t *sent_out, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_socket_recvfrom, \
                 int(CPLAT_API *)(cplat_socket sock, void *buf, size_t len, cplat_ipv4_endpoint *peer_out, \
                                  size_t *received_out, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_socket_send_all, \
                 int(CPLAT_API *)(cplat_socket sock, const void *buf, size_t len, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_socket_recv_all, \
                 int(CPLAT_API *)(cplat_socket sock, void *buf, size_t len, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_socket_wait_readable, \
                 int(CPLAT_API *)(cplat_socket sock, int timeout_ms, int *ready_out, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_socket_wait_writable, \
                 int(CPLAT_API *)(cplat_socket sock, int timeout_ms, int *ready_out, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_socket_wait_readable_multi, \
                 int(CPLAT_API *)(const cplat_socket *socks, size_t count, int timeout_ms, unsigned char *ready_out, \
                                  cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_socket_shutdown_receive, int(CPLAT_API *)(cplat_socket * sock_inout, cplat_error * detail_out)) \
    /* cplat/prompt/pinned_prompt.h */ \
    EXPORT_ENTRY(cplat_pinned_prompt_create, \
                 cplat_pinned_prompt *(CPLAT_API *)(const cplat_pinned_prompt_options *options)) \
    EXPORT_ENTRY(cplat_pinned_prompt_dispose, void(CPLAT_API *)(cplat_pinned_prompt * screen)) \
    EXPORT_ENTRY(cplat_pinned_prompt_readline_at, \
                 int(CPLAT_API *)(cplat_pinned_prompt * screen, char *buf, size_t buf_size, const char *prompt_str, \
                                  const char *file, int line)) \
    EXPORT_ENTRY(cplat_pinned_prompt_readline_with_initial_at, \
                 int(CPLAT_API *)(cplat_pinned_prompt * screen, char *buf, size_t buf_size, const char *prompt_str, \
                                  const char *initial_text, const char *file, int line)) \
    EXPORT_ENTRY(cplat_pinned_prompt_readline_fmt_at, \
                 int(CPLAT_API *)(cplat_pinned_prompt * screen, char *buf, size_t buf_size, const char *file, \
                                  int line, const char *fmt, ...)) \
    EXPORT_ENTRY(cplat_pinned_prompt_write, \
                 int(CPLAT_API *)(cplat_pinned_prompt * screen, cplat_pinned_prompt_channel channel, const void *data, \
                                  size_t size, size_t *written_out)) \
    EXPORT_ENTRY( \
        cplat_pinned_prompt_printf, \
        int(CPLAT_API *)(cplat_pinned_prompt * screen, cplat_pinned_prompt_channel channel, const char *fmt, ...)) \
    EXPORT_ENTRY( \
        cplat_pinned_prompt_status_enable, \
        int(CPLAT_API *)(cplat_pinned_prompt * screen, cplat_pinned_prompt_status_position position, int enable)) \
    EXPORT_ENTRY(cplat_pinned_prompt_status_set, \
                 int(CPLAT_API *)(cplat_pinned_prompt * screen, cplat_pinned_prompt_status_position position, \
                                  cplat_pinned_prompt_status_align align, const char *content)) \
    /* cplat/prompt/prompt.h */ \
    EXPORT_ENTRY(cplat_prompt_create, cplat_prompt *(CPLAT_API *)(const cplat_prompt_options *options)) \
    EXPORT_ENTRY(cplat_prompt_dispose, void(CPLAT_API *)(cplat_prompt * prompt)) \
    EXPORT_ENTRY(cplat_prompt_readline_at, int(CPLAT_API *)(cplat_prompt * prompt, char *buf, size_t buf_size, \
                                                            const char *prompt_str, const char *file, int line)) \
    EXPORT_ENTRY(cplat_prompt_readline_with_initial_at, \
                 int(CPLAT_API *)(cplat_prompt * prompt, char *buf, size_t buf_size, const char *prompt_str, \
                                  const char *initial_text, const char *file, int line)) \
    EXPORT_ENTRY(cplat_prompt_readline_fmt_at, int(CPLAT_API *)(cplat_prompt * p, char *buf, size_t buf_size, \
                                                                const char *file, int line, const char *fmt, ...)) \
    /* cplat/runtime/elevated_process.h */ \
    EXPORT_ENTRY(cplat_elevated_process_is_elevated, int(CPLAT_API *)(int *elevated)) \
    EXPORT_ENTRY(cplat_elevated_process_run_if_needed, \
                 int(CPLAT_API *)(const char *arguments, int *exit_code, int *handled)) \
    EXPORT_ENTRY(cplat_elevated_process_run_with_result, \
                 int(CPLAT_API *)(const char *arguments, int *exit_code, int *handled, char *result_message, \
                                  size_t result_message_size)) \
    EXPORT_ENTRY(cplat_elevated_process_extract_result_target, \
                 int(CPLAT_API *)(int *argc, char **argv, int *detected_out)) \
    EXPORT_ENTRY(cplat_elevated_process_report_result, int(CPLAT_API *)(const char *message)) \
    EXPORT_ENTRY(cplat_elevated_process_run_piped, \
                 int(CPLAT_API *)(const char *arguments, cplat_elevated_process_output_fn output_fn, void *context, \
                                  int *exit_code, int *handled)) \
    EXPORT_ENTRY(cplat_elevated_process_attach_output_pipes, \
                 int(CPLAT_API *)(int *argc, char **argv, int *attached_out)) \
    /* cplat/regex/regex.h */ \
    EXPORT_ENTRY(cplat_regex_create, int(CPLAT_API *)(const char *pattern, unsigned int flags, \
                                                      cplat_regex **regex_out, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_regex_dispose, void(CPLAT_API *)(cplat_regex * regex)) \
    EXPORT_ENTRY(cplat_regex_get_group_count, size_t(CPLAT_API *)(const cplat_regex *regex)) \
    EXPORT_ENTRY(cplat_regex_search, \
                 int(CPLAT_API *)(const cplat_regex *regex, const char *text, size_t text_len, size_t start_offset, \
                                  unsigned int match_flags, cplat_regex_match *matches_out, size_t matches_capacity, \
                                  int *matched_out, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_regex_matches, \
                 int(CPLAT_API *)(const cplat_regex *regex, const char *text, size_t text_len, \
                                  unsigned int match_flags, cplat_regex_match *matches_out, size_t matches_capacity, \
                                  int *matched_out, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_regex_replace, \
                 int(CPLAT_API *)(const cplat_regex *regex, const char *text, size_t text_len, \
                                  const char *replacement, unsigned int flags, char *result_out, size_t result_size, \
                                  size_t *required_size_out, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_regex_iter_create, \
                 int(CPLAT_API *)(const cplat_regex *regex, const char *text, size_t text_len, \
                                  unsigned int match_flags, cplat_regex_iter **iter_out, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_regex_iter_next, \
                 int(CPLAT_API *)(cplat_regex_iter * iter, cplat_regex_match * matches_out, size_t matches_capacity, \
                                  int *has_match_out, cplat_error *detail_out)) \
    EXPORT_ENTRY(cplat_regex_iter_dispose, void(CPLAT_API *)(cplat_regex_iter * iter)) \
    EXPORT_ENTRY(cplat_regex_split, \
                 int(CPLAT_API *)(const cplat_regex *regex, const char *text, size_t text_len, size_t max_parts, \
                                  unsigned int match_flags, cplat_regex_match *parts_out, size_t parts_capacity, \
                                  size_t *part_count_out, cplat_error *detail_out)) \
    /* cplat/runtime/memory_lock.h */ \
    EXPORT_ENTRY(cplat_memory_lock_range, int(CPLAT_API *)(const void *address, size_t size)) \
    EXPORT_ENTRY(cplat_memory_unlock_range, int(CPLAT_API *)(const void *address, size_t size)) \
    EXPORT_ENTRY(cplat_memory_lock_self, \
                 int(CPLAT_API *)(const cplat_memory_lock_self_options *options, cplat_memory_lock_scope **scope)) \
    EXPORT_ENTRY(cplat_memory_lock_scope_release, int(CPLAT_API *)(cplat_memory_lock_scope * scope)) \
    EXPORT_ENTRY(cplat_secure_zero, void(CPLAT_API *)(void *buf, size_t size)) \
    /* cplat/runtime/host.h */ \
    EXPORT_ENTRY(cplat_host_get_name, int(CPLAT_API *)(char *name_out, size_t name_size)) \
    /* cplat/runtime/module.h */ \
    EXPORT_ENTRY(cplat_module_get_path, int(CPLAT_API *)(char *path_out, size_t path_size, const void *func_addr)) \
    EXPORT_ENTRY(cplat_module_get_basename, \
                 int(CPLAT_API *)(char *basename_out, size_t basename_size, const void *func_addr)) \
    /* cplat/runtime/process.h */ \
    EXPORT_ENTRY(cplat_process_get_executable_path, int(CPLAT_API *)(char *path_out, size_t path_size)) \
    EXPORT_ENTRY(cplat_process_get_pid, uint32_t(CPLAT_API *)(void)) \
    EXPORT_ENTRY(cplat_process_get_tid, uint32_t(CPLAT_API *)(void)) \
    EXPORT_ENTRY(cplat_process_start, int(CPLAT_API *)(const cplat_process_options *options, cplat_process **process)) \
    EXPORT_ENTRY(cplat_process_wait, int(CPLAT_API *)(cplat_process * process, int timeout_ms)) \
    EXPORT_ENTRY(cplat_process_get_exit_code, int(CPLAT_API *)(cplat_process * process, int *exit_code)) \
    EXPORT_ENTRY(cplat_process_terminate, int(CPLAT_API *)(cplat_process * process)) \
    EXPORT_ENTRY(cplat_process_dispose, void(CPLAT_API *)(cplat_process * process)) \
    EXPORT_ENTRY(cplat_process_run_sync, \
                 int(CPLAT_API *)(const cplat_process_options *options, int timeout_ms, int *exit_code)) \
    /* cplat/runtime/shutdown.h */ \
    EXPORT_ENTRY(cplat_shutdown_register, int(CPLAT_API *)(cplat_shutdown_fn callback, void *context)) \
    EXPORT_ENTRY(cplat_shutdown_request_register, int(CPLAT_API *)(cplat_shutdown_fn callback, void *context)) \
    EXPORT_ENTRY(cplat_exit, void(CPLAT_API *)(int code)) \
    EXPORT_ENTRY(cplat_shutdown_invoke_for_test, \
                 int(CPLAT_API *)(const cplat_shutdown_event *event, int *invoked_out)) \
    EXPORT_ENTRY(cplat_shutdown_request_invoke_for_test, \
                 int(CPLAT_API *)(const cplat_shutdown_event *event, int *invoked_out)) \
    EXPORT_ENTRY(cplat_shutdown_reset_for_test, void(CPLAT_API *)(void)) \
    /* cplat/runtime/sym_loader.h */ \
    EXPORT_ENTRY(cplat_sym_loader_resolve, void *(CPLAT_API *)(cplat_sym_loader_entry * fobj)) \
    EXPORT_ENTRY(cplat_sym_loader_is_default, int(CPLAT_API *)(cplat_sym_loader_entry * fobj)) \
    EXPORT_ENTRY(cplat_sym_loader_init, void(CPLAT_API *)(cplat_sym_loader_entry *const *fobj_array, \
                                                          size_t fobj_length, const char *configpath)) \
    EXPORT_ENTRY(cplat_sym_loader_dispose, \
                 void(CPLAT_API *)(cplat_sym_loader_entry *const *fobj_array, size_t fobj_length)) \
    EXPORT_ENTRY(cplat_sym_loader_info, \
                 int(CPLAT_API *)(cplat_sym_loader_entry *const *fobj_array, size_t fobj_length)) \
    /* cplat/string_catalog/string_catalog.h */ \
    EXPORT_ENTRY(cplat_string_catalog_set_language, int(CPLAT_API *)(cplat_string_catalog_language language)) \
    EXPORT_ENTRY(cplat_string_catalog_get_language, cplat_string_catalog_language(CPLAT_API *)(void)) \
    EXPORT_ENTRY(cplat_string_catalog_language_from_tag, \
                 int(CPLAT_API *)(const char *tag, cplat_string_catalog_language *language_out)) \
    EXPORT_ENTRY(cplat_string_catalog_format, int(CPLAT_API *)(const cplat_string_catalog *catalog, char *dest, \
                                                               size_t dest_size, int string_key, ...)) \
    EXPORT_ENTRY(cplat_string_catalog_vformat, int(CPLAT_API *)(const cplat_string_catalog *catalog, char *dest, \
                                                                size_t dest_size, int string_key, va_list args)) \
    EXPORT_ENTRY(cplat_string_catalog_verify, \
                 int(CPLAT_API *)(const cplat_string_catalog *catalog, int *string_key_out, \
                                  cplat_string_catalog_language *language_out)) \
    EXPORT_ENTRY(cplat_string_catalog_get_entry, \
                 const cplat_string_catalog_entry *(CPLAT_API *)(const cplat_string_catalog *catalog, int string_key)) \
    EXPORT_ENTRY(cplat_string_catalog_get_category, \
                 int(CPLAT_API *)(const cplat_string_catalog *catalog, int string_key)) \
    EXPORT_ENTRY(cplat_string_catalog_get_id, \
                 const char *(CPLAT_API *)(const cplat_string_catalog *catalog, int string_key)) \
    EXPORT_ENTRY(cplat_string_catalog_get_note, \
                 const char *(CPLAT_API *)(const cplat_string_catalog *catalog, int string_key)) \
    /* cplat/string_catalog/filter.h */ \
    EXPORT_ENTRY(cplat_string_catalog_filter_compile, \
                 int(CPLAT_API *)(const char *lines, size_t line_count, size_t line_width, size_t line_capacity, \
                                  void *image_out, size_t image_size, \
                                  cplat_string_catalog_filter_diagnostic *diagnostics, size_t diagnostic_capacity, \
                                  size_t *invalid_count_out)) \
    EXPORT_ENTRY(cplat_string_catalog_filter_compile_line, \
                 int(CPLAT_API *)(void *image, size_t image_size, size_t line_index, const char *text, \
                                  cplat_string_catalog_filter_diagnostic *diagnostic_out)) \
    EXPORT_ENTRY(cplat_string_catalog_filter_insert_line, \
                 int(CPLAT_API *)(void *image, size_t image_size, size_t line_index, const char *text, \
                                  cplat_string_catalog_filter_diagnostic *diagnostic_out)) \
    EXPORT_ENTRY(cplat_string_catalog_filter_remove_line, \
                 int(CPLAT_API *)(void *image, size_t image_size, size_t line_index)) \
    EXPORT_ENTRY(cplat_string_catalog_filter_validate, int(CPLAT_API *)(const void *image, size_t image_size)) \
    EXPORT_ENTRY(cplat_string_catalog_filter_get_info, \
                 int(CPLAT_API *)(const void *image, size_t image_size, cplat_string_catalog_filter_info *info_out)) \
    EXPORT_ENTRY( \
        cplat_string_catalog_filter_decompile_line, \
        int(CPLAT_API *)(const void *image, size_t image_size, size_t line_index, char *dest, size_t dest_size)) \
    EXPORT_ENTRY( \
        cplat_string_catalog_filter_slot_create, \
        int(CPLAT_API *)(const cplat_string_catalog *catalog, const cplat_string_catalog_filter_key_name *key_names, \
                         size_t key_name_count, const cplat_string_catalog_filter_category_names *category_names, \
                         size_t line_capacity, size_t line_width, cplat_string_catalog_filter_slot **slot_out)) \
    EXPORT_ENTRY(cplat_string_catalog_filter_slot_dispose, \
                 void(CPLAT_API *)(cplat_string_catalog_filter_slot * *slot)) \
    EXPORT_ENTRY(cplat_string_catalog_filter_slot_get_catalog, \
                 const cplat_string_catalog *(CPLAT_API *)(const cplat_string_catalog_filter_slot *slot)) \
    EXPORT_ENTRY(cplat_string_catalog_filter_get_catalog_id, \
                 int(CPLAT_API *)(const cplat_string_catalog *catalog, uint64_t *catalog_id_out)) \
    EXPORT_ENTRY(cplat_string_catalog_filter_source_publish, \
                 int(CPLAT_API *)(void *source, size_t source_size, const void *image, size_t image_size, \
                                  uint64_t catalog_id, const cplat_string_catalog_filter_source_lock *lock, \
                                  uint64_t *revision_out)) \
    EXPORT_ENTRY(cplat_string_catalog_filter_source_get_info, int(CPLAT_API *)(const void *source, size_t source_size, cplat_string_catalog_filter_source_info *info_out)) \
    EXPORT_ENTRY(cplat_string_catalog_filter_slot_attach_source, \
                 int(CPLAT_API *)(cplat_string_catalog_filter_slot * slot, const void *source, size_t source_size, \
                                  const cplat_string_catalog_filter_source_lock *lock)) \
    EXPORT_ENTRY(cplat_string_catalog_filter_slot_get_source_status, int(CPLAT_API *)(cplat_string_catalog_filter_slot *slot, cplat_string_catalog_filter_source_status *status_out)) \
    EXPORT_ENTRY(cplat_string_catalog_filter_slot_apply, \
                 int(CPLAT_API *)(cplat_string_catalog_filter_slot * slot, const void *image, size_t image_size, \
                                  cplat_string_catalog_filter_diagnostic *diagnostics, size_t diagnostic_capacity, \
                                  size_t *invalid_count_out)) \
    EXPORT_ENTRY(cplat_string_catalog_filter_slot_check, \
                 int(CPLAT_API *)(cplat_string_catalog_filter_slot * slot, const void *image, size_t image_size, \
                                  cplat_string_catalog_filter_diagnostic *diagnostics, size_t diagnostic_capacity, \
                                  size_t *invalid_count_out, cplat_string_catalog_filter_warning *warnings, \
                                  size_t warning_capacity, size_t *warning_count_out)) \
    EXPORT_ENTRY(cplat_string_catalog_filter_slot_snapshot, \
                 int(CPLAT_API *)(cplat_string_catalog_filter_slot * slot, void *image_out, size_t image_size)) \
    EXPORT_ENTRY(cplat_string_catalog_filter_slot_get_line_error, \
                 int(CPLAT_API *)(cplat_string_catalog_filter_slot * slot, size_t line_index, \
                                  cplat_string_catalog_filter_line_error *error_out)) \
    EXPORT_ENTRY( \
        cplat_string_catalog_filter_slot_describe_line, \
        int(CPLAT_API *)(cplat_string_catalog_filter_slot * slot, size_t line_index, char *dest, size_t dest_size)) \
    EXPORT_ENTRY(cplat_string_catalog_filter_slot_test, \
                 int(CPLAT_API *)(cplat_string_catalog_filter_slot * slot, int string_key, \
                                  cplat_string_catalog_filter_state *state_out)) \
    EXPORT_ENTRY(cplat_string_catalog_filter_slot_vformat, \
                 int(CPLAT_API *)(cplat_string_catalog_filter_slot * slot, char *dest, size_t dest_size, \
                                  int *matched_out, int string_key, va_list args)) \
    EXPORT_ENTRY(cplat_string_catalog_filter_slot_format, \
                 int(CPLAT_API *)(cplat_string_catalog_filter_slot * slot, char *dest, size_t dest_size, \
                                  int *matched_out, int string_key, ...)) \
    /* cplat/sync/sync.h */ \
    EXPORT_ENTRY(cplat_local_lock_create, int(CPLAT_API *)(cplat_local_lock * *mtx)) \
    EXPORT_ENTRY(cplat_local_lock_lock, int(CPLAT_API *)(cplat_local_lock * mtx, int timeout_ms)) \
    EXPORT_ENTRY(cplat_local_lock_try_lock, int(CPLAT_API *)(cplat_local_lock * mtx)) \
    EXPORT_ENTRY(cplat_local_lock_unlock, int(CPLAT_API *)(cplat_local_lock * mtx)) \
    EXPORT_ENTRY(cplat_local_lock_dispose, void(CPLAT_API *)(cplat_local_lock * mtx)) \
    EXPORT_ENTRY(cplat_condvar_create, int(CPLAT_API *)(cplat_condvar * *cv)) \
    EXPORT_ENTRY(cplat_condvar_wait, int(CPLAT_API *)(cplat_condvar * cv, cplat_local_lock * mtx, int timeout_ms)) \
    EXPORT_ENTRY(cplat_condvar_signal, int(CPLAT_API *)(cplat_condvar * cv)) \
    EXPORT_ENTRY(cplat_condvar_broadcast, int(CPLAT_API *)(cplat_condvar * cv)) \
    EXPORT_ENTRY(cplat_condvar_dispose, void(CPLAT_API *)(cplat_condvar * cv)) \
    EXPORT_ENTRY(cplat_local_rwlock_create, int(CPLAT_API *)(cplat_local_rwlock * *rwlock)) \
    EXPORT_ENTRY(cplat_local_rwlock_lock_shared, int(CPLAT_API *)(cplat_local_rwlock * rwlock, int timeout_ms)) \
    EXPORT_ENTRY(cplat_local_rwlock_try_lock_shared, int(CPLAT_API *)(cplat_local_rwlock * rwlock)) \
    EXPORT_ENTRY(cplat_local_rwlock_lock_exclusive, int(CPLAT_API *)(cplat_local_rwlock * rwlock, int timeout_ms)) \
    EXPORT_ENTRY(cplat_local_rwlock_try_lock_exclusive, int(CPLAT_API *)(cplat_local_rwlock * rwlock)) \
    EXPORT_ENTRY(cplat_local_rwlock_unlock_shared, int(CPLAT_API *)(cplat_local_rwlock * rwlock)) \
    EXPORT_ENTRY(cplat_local_rwlock_unlock_exclusive, int(CPLAT_API *)(cplat_local_rwlock * rwlock)) \
    EXPORT_ENTRY(cplat_local_rwlock_dispose, void(CPLAT_API *)(cplat_local_rwlock * rwlock)) \
    EXPORT_ENTRY(cplat_thread_create, int(CPLAT_API *)(cplat_thread * *thread, cplat_thread_fn func, void *arg)) \
    EXPORT_ENTRY(cplat_thread_join, int(CPLAT_API *)(cplat_thread * thread, int timeout_ms)) \
    EXPORT_ENTRY(cplat_thread_detach, void(CPLAT_API *)(cplat_thread * thread)) \
    EXPORT_ENTRY(cplat_interprocess_lock_open, int(CPLAT_API *)(const char *identity, cplat_interprocess_lock **lock)) \
    EXPORT_ENTRY(cplat_interprocess_lock_import_descriptor, \
                 int(CPLAT_API *)(const void *descriptor, size_t descriptor_size, cplat_interprocess_lock **lock)) \
    EXPORT_ENTRY(cplat_interprocess_lock_export_descriptor, \
                 int(CPLAT_API *)(const cplat_interprocess_lock *lock, void *descriptor, size_t *descriptor_size)) \
    EXPORT_ENTRY(cplat_interprocess_lock_lock, int(CPLAT_API *)(cplat_interprocess_lock * lock, int timeout_ms)) \
    EXPORT_ENTRY(cplat_interprocess_lock_try_lock, int(CPLAT_API *)(cplat_interprocess_lock * lock)) \
    EXPORT_ENTRY(cplat_interprocess_lock_unlock, int(CPLAT_API *)(cplat_interprocess_lock * lock)) \
    EXPORT_ENTRY(cplat_interprocess_lock_dispose, void(CPLAT_API *)(cplat_interprocess_lock * lock)) \
    EXPORT_ENTRY(cplat_interprocess_rwlock_open, \
                 int(CPLAT_API *)(const char *identity, cplat_interprocess_rwlock **lock)) \
    EXPORT_ENTRY(cplat_interprocess_rwlock_import_descriptor, \
                 int(CPLAT_API *)(const void *descriptor, size_t descriptor_size, cplat_interprocess_rwlock **lock)) \
    EXPORT_ENTRY(cplat_interprocess_rwlock_export_descriptor, \
                 int(CPLAT_API *)(const cplat_interprocess_rwlock *lock, void *descriptor, size_t *descriptor_size)) \
    EXPORT_ENTRY(cplat_interprocess_rwlock_lock_shared, \
                 int(CPLAT_API *)(cplat_interprocess_rwlock * lock, int timeout_ms)) \
    EXPORT_ENTRY(cplat_interprocess_rwlock_try_lock_shared, int(CPLAT_API *)(cplat_interprocess_rwlock * lock)) \
    EXPORT_ENTRY(cplat_interprocess_rwlock_lock_exclusive, \
                 int(CPLAT_API *)(cplat_interprocess_rwlock * lock, int timeout_ms)) \
    EXPORT_ENTRY(cplat_interprocess_rwlock_try_lock_exclusive, int(CPLAT_API *)(cplat_interprocess_rwlock * lock)) \
    EXPORT_ENTRY(cplat_interprocess_rwlock_unlock, int(CPLAT_API *)(cplat_interprocess_rwlock * lock)) \
    EXPORT_ENTRY(cplat_interprocess_rwlock_dispose, void(CPLAT_API *)(cplat_interprocess_rwlock * lock)) \
    EXPORT_ENTRY(cplat_call_once, void(CPLAT_API *)(cplat_once_flag * flag, cplat_once_fn func)) \
    EXPORT_ENTRY(cplat_sleep_ms, void(CPLAT_API *)(int ms)) \
    /* cplat/trace/trace_file.h */ \
    EXPORT_ENTRY(cplat_trace_file_sink_create, \
                 cplat_trace_file_sink *(CPLAT_API *)(const char *path, size_t max_bytes, int generations, int flags)) \
    EXPORT_ENTRY(cplat_trace_file_sink_write, int(CPLAT_API *)(cplat_trace_file_sink * handle, int level, \
                                                               const cplat_timespec *timestamp, const char *message)) \
    EXPORT_ENTRY(cplat_trace_file_sink_dispose, void(CPLAT_API *)(cplat_trace_file_sink * handle)) \
    /* cplat/trace/tracer.h */ \
    EXPORT_ENTRY(cplat_tracer_create, cplat_tracer *(CPLAT_API *)(cplat_tracer_concurrency_mode)) \
    EXPORT_ENTRY(cplat_tracer_start, int(CPLAT_API *)(cplat_tracer * handle)) \
    EXPORT_ENTRY(cplat_tracer_stop, int(CPLAT_API *)(cplat_tracer * handle)) \
    EXPORT_ENTRY(cplat_tracer_get_state, cplat_tracer_state(CPLAT_API *)(cplat_tracer * handle)) \
    EXPORT_ENTRY(cplat_tracer_write_at, int(CPLAT_API *)(cplat_tracer * handle, cplat_trace_level level, \
                                                         const cplat_timespec *timestamp, const char *message)) \
    EXPORT_ENTRY(cplat_tracer_writef_at, int(CPLAT_API *)(cplat_tracer * handle, cplat_trace_level level, \
                                                          const cplat_timespec *timestamp, const char *format, ...)) \
    EXPORT_ENTRY(cplat_tracer_vwritef_at, \
                 int(CPLAT_API *)(cplat_tracer * handle, cplat_trace_level level, const cplat_timespec *timestamp, \
                                  const char *format, va_list args)) \
    EXPORT_ENTRY(cplat_tracer_write_hex_at, \
                 int(CPLAT_API *)(cplat_tracer * handle, cplat_trace_level level, const cplat_timespec *timestamp, \
                                  const void *data, size_t size, const char *message)) \
    EXPORT_ENTRY(cplat_tracer_write_hexf_at, \
                 int(CPLAT_API *)(cplat_tracer * handle, cplat_trace_level level, const cplat_timespec *timestamp, \
                                  const void *data, size_t size, const char *format, ...)) \
    EXPORT_ENTRY(cplat_tracer_vwrite_hexf_at, \
                 int(CPLAT_API *)(cplat_tracer * handle, cplat_trace_level level, const cplat_timespec *timestamp, \
                                  const void *data, size_t size, const char *format, va_list args)) \
    EXPORT_ENTRY(cplat_tracer_hex_sep, const char *(CPLAT_API *)(const char *message)) \
    EXPORT_ENTRY(cplat_tracer_hex_msg, const char *(CPLAT_API *)(const char *message)) \
    EXPORT_ENTRY(cplat_tracer_write_with_source, \
                 int(CPLAT_API *)(cplat_tracer * handle, cplat_trace_level level, const cplat_timespec *timestamp, \
                                  const char *file, int line, const char *message)) \
    EXPORT_ENTRY(cplat_tracer_set_name, int(CPLAT_API *)(cplat_tracer * handle, const char *name, int64_t identifier)) \
    EXPORT_ENTRY(cplat_tracer_get_name, int(CPLAT_API *)(cplat_tracer * handle, char *name_out, size_t name_size)) \
    EXPORT_ENTRY(cplat_tracer_get_identifier, int64_t(CPLAT_API *)(cplat_tracer * handle)) \
    EXPORT_ENTRY(cplat_tracer_set_file_name, \
                 int(CPLAT_API *)(cplat_tracer * handle, const char *name, int64_t identifier)) \
    EXPORT_ENTRY(cplat_tracer_get_file_name, \
                 int(CPLAT_API *)(cplat_tracer * handle, char *file_name_out, size_t file_name_size)) \
    EXPORT_ENTRY(cplat_tracer_get_file_identifier, int64_t(CPLAT_API *)(cplat_tracer * handle)) \
    EXPORT_ENTRY(cplat_tracer_get_os_level, cplat_trace_level(CPLAT_API *)(cplat_tracer * handle)) \
    EXPORT_ENTRY(cplat_tracer_set_os_level, int(CPLAT_API *)(cplat_tracer * handle, cplat_trace_level level)) \
    EXPORT_ENTRY(cplat_tracer_get_etw_level, cplat_trace_level(CPLAT_API *)(cplat_tracer * handle)) \
    EXPORT_ENTRY(cplat_tracer_set_etw_level, int(CPLAT_API *)(cplat_tracer * handle, cplat_trace_level level)) \
    EXPORT_ENTRY(cplat_tracer_get_file_level, cplat_trace_level(CPLAT_API *)(cplat_tracer * handle)) \
    EXPORT_ENTRY(cplat_tracer_set_file_level, \
                 int(CPLAT_API *)(cplat_tracer * handle, const char *path, cplat_trace_level level, size_t max_bytes, \
                                  int generations, int flags)) \
    EXPORT_ENTRY(cplat_tracer_get_stderr_level, cplat_trace_level(CPLAT_API *)(cplat_tracer * handle)) \
    EXPORT_ENTRY(cplat_tracer_set_stderr_level, int(CPLAT_API *)(cplat_tracer * handle, cplat_trace_level level)) \
    EXPORT_ENTRY(cplat_tracer_dispose, void(CPLAT_API *)(cplat_tracer * *handle)) \
    EXPORT_ENTRY( \
        cplat_tracer_set_hook, \
        cplat_tracer_hook_entry *(CPLAT_API *)(cplat_tracer * handle, cplat_tracer_hook_fn fn, void *context)) \
    EXPORT_ENTRY(cplat_tracer_remove_hook, \
                 void(CPLAT_API *)(cplat_tracer * handle, cplat_tracer_hook_entry * hook_entry)) \
    EXPORT_ENTRY(cplat_tracer_call_next_hook, \
                 void(CPLAT_API *)(cplat_tracer_hook_entry * prev, cplat_tracer * handle, cplat_trace_level level, \
                                   const cplat_timespec *timestamp, const char *message))

#if defined(PLATFORM_WINDOWS)
    #define CPLAT_EXPORT_TABLE_PLATFORM(EXPORT_ENTRY) \
        /* cplat/crt/wchar_conv.h */ \
        EXPORT_ENTRY(cplat_utf8_to_wpath, int(CPLAT_API *)(wchar_t * wbuf, size_t wbuf_count, const char *utf8_path)) \
        EXPORT_ENTRY(cplat_utf8_to_wstr, int(CPLAT_API *)(wchar_t * wbuf, size_t wbuf_count, const char *utf8_text)) \
        EXPORT_ENTRY(cplat_wpath_to_utf8, int(CPLAT_API *)(char *dest, size_t dest_size, const wchar_t *wpath)) \
        EXPORT_ENTRY(cplat_wstr_to_utf8, int(CPLAT_API *)(char *dest, size_t dest_size, const wchar_t *wtext)) \
        EXPORT_ENTRY(cplat_utf8_to_wstr_alloc, wchar_t *(CPLAT_API *)(const char *utf8_text)) \
        EXPORT_ENTRY(cplat_wstr_to_utf8_alloc, char *(CPLAT_API *)(const wchar_t *wtext)) \
        /* cplat/base/error.h */ \
        EXPORT_ENTRY(cplat_error_capture_windows_error, \
                     void(CPLAT_API *)(cplat_error * error, unsigned long error_code)) \
        EXPORT_ENTRY(cplat_error_capture_current_windows_error, void(CPLAT_API *)(cplat_error * error)) \
        EXPORT_ENTRY(cplat_error_get_windows_error, unsigned long(CPLAT_API *)(const cplat_error *error)) \
        /* cplat/trace/etw.h */ \
        EXPORT_ENTRY(cplat_etw_provider_create, \
                     cplat_etw_provider *(CPLAT_API *)(cplat_etw_provider_ref_t provider_ref)) \
        EXPORT_ENTRY(cplat_etw_provider_write, int(CPLAT_API *)(cplat_etw_provider * handle, int level, \
                                                                const char *service, const char *message)) \
        EXPORT_ENTRY(cplat_etw_provider_dispose, void(CPLAT_API *)(cplat_etw_provider * handle)) \
        EXPORT_ENTRY(cplat_etw_session_check_access, int(CPLAT_API *)(void)) \
        EXPORT_ENTRY(cplat_etw_session_start, \
                     int(CPLAT_API *)(const char *session_name, const char *provider_guid_str, \
                                      cplat_etw_event_fn callback, void *context, cplat_etw_session **session_out)) \
        EXPORT_ENTRY(cplat_etw_session_stop, void(CPLAT_API *)(cplat_etw_session * session)) \
        /* cplat/trace/eventlog.h */ \
        EXPORT_ENTRY(cplat_eventlog_sink_create, cplat_eventlog_sink *(CPLAT_API *)(const char *source_name)) \
        EXPORT_ENTRY(cplat_eventlog_sink_write, \
                     int(CPLAT_API *)(cplat_eventlog_sink * handle, int level, int64_t file_identifier, \
                                      const char *instance_name, int64_t instance_identifier, const char *message)) \
        EXPORT_ENTRY(cplat_eventlog_sink_dispose, void(CPLAT_API *)(cplat_eventlog_sink * handle)) \
        EXPORT_ENTRY(cplat_eventlog_register_source, \
                     int(CPLAT_API *)(const char *source_name, const char *message_file_path)) \
        EXPORT_ENTRY(cplat_eventlog_unregister_source, int(CPLAT_API *)(const char *source_name)) \
        /* cplat/win32/win32.h */ \
        EXPORT_ENTRY(CreateFileU, \
                     HANDLE(CPLAT_API *)(const char *utf8_path, DWORD desired_access, DWORD share_mode, \
                                         LPSECURITY_ATTRIBUTES security_attributes, DWORD creation_disposition, \
                                         DWORD flags_and_attributes, HANDLE template_file)) \
        EXPORT_ENTRY(CreateNamedPipeU, \
                     HANDLE(CPLAT_API *)(const char *utf8_name, DWORD open_mode, DWORD pipe_mode, DWORD max_instances, \
                                         DWORD out_buffer_size, DWORD in_buffer_size, DWORD default_timeout, \
                                         LPSECURITY_ATTRIBUTES security_attributes)) \
        EXPORT_ENTRY(GetModuleFileNameU, DWORD(CPLAT_API *)(HMODULE module, char *utf8_buf, DWORD size)) \
        EXPORT_ENTRY(WriteConsoleU, BOOL(CPLAT_API *)(HANDLE console, const char *utf8_text, DWORD utf8_length, \
                                                      DWORD *written_length, void *reserved)) \
        EXPORT_ENTRY(GetVolumePathNameU, BOOL(CPLAT_API *)(const char *utf8_path, char *utf8_volume_root, DWORD size)) \
        EXPORT_ENTRY(GetVolumeInformationU, \
                     BOOL(CPLAT_API *)(const char *utf8_root_path, char *utf8_volume_name, DWORD volume_name_size, \
                                       DWORD *serial_number, DWORD *max_component_length, DWORD *file_system_flags, \
                                       char *utf8_file_system_name, DWORD file_system_name_size)) \
        EXPORT_ENTRY(LoadLibraryU, HMODULE(CPLAT_API *)(const char *utf8_file_name)) \
        EXPORT_ENTRY(CreateProcessU, \
                     BOOL(CPLAT_API *)(const char *utf8_application_name, const char *utf8_command_line, \
                                       LPSECURITY_ATTRIBUTES process_attributes, \
                                       LPSECURITY_ATTRIBUTES thread_attributes, BOOL inherit_handles, \
                                       DWORD creation_flags, LPVOID environment, const char *utf8_current_directory, \
                                       LPSTARTUPINFOW startup_info, LPPROCESS_INFORMATION process_information)) \
        EXPORT_ENTRY(OpenSCManagerU, SC_HANDLE(CPLAT_API *)(const char *utf8_machine_name, \
                                                            const char *utf8_database_name, DWORD desired_access)) \
        EXPORT_ENTRY(CreateServiceU, \
                     SC_HANDLE(CPLAT_API *)(SC_HANDLE scm, const char *utf8_service_name, \
                                            const char *utf8_display_name, DWORD desired_access, DWORD service_type, \
                                            DWORD start_type, DWORD error_control, const char *utf8_binary_path_name, \
                                            const char *utf8_load_order_group, LPDWORD tag_id, \
                                            const char *utf8_dependencies, const char *utf8_service_start_name, \
                                            const char *utf8_password)) \
        EXPORT_ENTRY(OpenServiceU, \
                     SC_HANDLE(CPLAT_API *)(SC_HANDLE scm, const char *utf8_service_name, DWORD desired_access)) \
        EXPORT_ENTRY(ChangeServiceConfig2U, \
                     BOOL(CPLAT_API *)(SC_HANDLE service, DWORD info_level, const char *utf8_text)) \
        EXPORT_ENTRY(RegisterServiceCtrlHandlerExU, \
                     SERVICE_STATUS_HANDLE(CPLAT_API *)(const char *utf8_service_name, \
                                                        LPHANDLER_FUNCTION_EX handler_proc, LPVOID context)) \
        EXPORT_ENTRY(StartServiceCtrlDispatcherU, BOOL(CPLAT_API *)(const cplat_service_entry_u *service_table))
#elif defined(PLATFORM_LINUX)
    #define CPLAT_EXPORT_TABLE_PLATFORM(EXPORT_ENTRY) \
        /* cplat/trace/syslog.h */ \
        EXPORT_ENTRY(cplat_syslog_sink_create, cplat_syslog_sink *(CPLAT_API *)(const char *ident, int facility)) \
        EXPORT_ENTRY(cplat_syslog_sink_write, int(CPLAT_API *)(cplat_syslog_sink * handle, int level, \
                                                               const cplat_timespec *timestamp, const char *message)) \
        EXPORT_ENTRY(cplat_syslog_sink_rename, int(CPLAT_API *)(cplat_syslog_sink * handle, const char *new_ident)) \
        EXPORT_ENTRY(cplat_syslog_sink_dispose, void(CPLAT_API *)(cplat_syslog_sink * handle))
#endif /* PLATFORM_ */

// libcplat が公開エクスポートすべき変数の一覧。
// 現時点ではエントリなし (公開ヘッダーに dllexport 付きの変数エクスポートが存在しないため)。
// 公開ヘッダーへ変数エクスポートを追加する場合は、ここへ X(変数名, 型 *) の形で登録する。
// decltype(&name) はオブジェクトに対しても型 * を返すため、関数と同じ
// static_assert / kExpectedExportNames / symbol_names_match の仕組みがそのまま使える。
#define CPLAT_EXPORT_VARIABLE_TABLE(EXPORT_ENTRY)

#define CPLAT_EXPORT_TABLE(EXPORT_ENTRY) \
    CPLAT_EXPORT_TABLE_COMMON(EXPORT_ENTRY) \
    CPLAT_EXPORT_TABLE_PLATFORM(EXPORT_ENTRY) \
    CPLAT_EXPORT_VARIABLE_TABLE(EXPORT_ENTRY)

// テーブルからシグネチャの static_assert と期待シンボル名一覧を生成する。
// 定型マクロ (TESTFW_EXPORT_STATIC_ASSERT_ENTRY/TESTFW_EXPORT_NAME_ENTRY) は
// framework/testfw/include/export_check.h 側の共通定義を使う。
#if defined(PLATFORM_LINUX)
    // GCC は format 属性付き関数ポインター型を std::is_same のテンプレート引数にすると、
    // ABI シグネチャ比較の対象外である format 属性を無視したことを警告するため、この比較だけ抑制する。
    // see: https://gcc.gnu.org/onlinedocs/gcc-8.5.0/gcc/Warning-Options.html#index-Wignored-attributes
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wignored-attributes"
#endif /* PLATFORM_LINUX */
CPLAT_EXPORT_TABLE(TESTFW_EXPORT_STATIC_ASSERT_ENTRY)
#if defined(PLATFORM_LINUX)
    #pragma GCC diagnostic pop
#endif /* PLATFORM_LINUX */

static const char *const kExpectedExportNames[] = {CPLAT_EXPORT_TABLE(TESTFW_EXPORT_NAME_ENTRY)};

static const std::map<std::string, std::string> kExpectedExportSignatures = {
    CPLAT_EXPORT_TABLE(TESTFW_EXPORT_SIGNATURE_ENTRY)};

class exportTest : public Test
{
  protected:
    std::string workspace_root;
    std::string dll_path;

    // [サブ手順 名前=exportTest.SetUp]
    void SetUp() override
    {
        workspace_root = findWorkspaceRoot();
        ASSERT_FALSE(workspace_root.empty()) << "ワークスペースルートが見つかりません";
        // [状態確認] - `workspace_root.empty()` が false であること。
        dll_path = workspace_root + "/app/cplat/prod/lib/libcplat" TESTFW_SHARED_LIBRARY_EXTENSION;
    }
    // [サブ手順終了]
};

// libcplat のエクスポート シンボル名が期待値テーブルと完全一致することの確認
// [サブ手順参照 名前=exportTest.SetUp]
TEST_F(exportTest, symbol_names_match)
{
    // Arrange
    std::set<std::string> expected(
        std::begin(kExpectedExportNames),
        std::end(kExpectedExportNames)); // [状態] - CPLAT_EXPORT_TABLE から期待シンボル名一覧を構築する。
#if defined(PLATFORM_WINDOWS)
    // _ident_manifest_libcplat_dll は gen_ident_manifest.py が自動生成するビルド識別データであり、
    // 関数ではないためシグネチャ検証の対象外としつつ、名前一致の期待値には含める。
    expected.insert(testing::identManifestSymbolName(
        "libcplat" TESTFW_SHARED_LIBRARY_EXTENSION)); // [状態] - IDENT manifest シンボル名を期待値へ追加する (Windows のみ実際にエクスポートされる)。
#endif                                                /* PLATFORM_WINDOWS */

    // Pre-Assert

    // Act
    std::set<std::string> actual =
        testing::getActualExportNames(dll_path); // [手順] - dumpbin/nm で libcplat の実際のエクスポート一覧を取得する。

    // Assert
    // [サブ手順参照 名前=testing.expectExportNamesMatch]
    testing::expectExportNamesMatch(
        expected, actual,
        kExpectedExportSignatures); // 期待シンボルとの不足/想定外がないこと (Windows / Linux とも完全一致)。
}

// 公開ヘッダーの変数宣言が dllexport マクロ (CPLAT_EXPORT) を
// 伴わずに追加されていないことの確認
// [サブ手順参照 名前=exportTest.SetUp]
TEST_F(exportTest, public_header_variables_declare_export_macro)
{
    // Arrange
    std::string include_dir =
        workspace_root +
        "/app/cplat/prod/include"; // [状態] - 公開ヘッダーのディレクトリを "/app/cplat/prod/include" に設定する。

    // Pre-Assert

    // Act
    std::vector<std::string> undecorated = testing::findUndecoratedExternVariables(
        include_dir,
        "CPLAT_EXPORT"); // [手順] - prod/include 配下を走査し、CPLAT_EXPORT を伴わない extern 変数宣言を集める。

    // Assert
    EXPECT_TRUE(undecorated.empty()) << "CPLAT_EXPORT を伴わない変数宣言: "
                                     << testing::joinNames(
                                            undecorated); // [確認_正常系] - 該当する宣言が 1 件もないこと。
}
