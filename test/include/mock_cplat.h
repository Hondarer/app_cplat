#ifndef MOCK_CPLAT_H
#define MOCK_CPLAT_H

#include <cplat/base/platform.h>
#include <testfw.h>
#include <stdint.h>
#include <time.h>
#include <stdarg.h>
#include <vector>

#if defined(COMPILER_MSVC)
    #define MOCK_CPLAT_LINK_IMPL(func) __pragma(comment(linker, "/INCLUDE:_mock_impl_" #func))

// mock_cplat の弱リンク実装 (MOCK_WEAK_IMPL) は、参照側の翻訳単位が存在しない場合に
// MSVC がオブジェクトをリンクへ取り込まず、cplat_* が未解決になることがある。
// MOCK_CPLAT_LINK_IMPL(func) は /INCLUDE:_mock_impl_<func> をリンカーへ渡し、
// 実体を明示的に含める。列挙順は公開 API 表 (exportTest.cc の CPLAT_EXPORT_TABLE)
// に合わせる。cplat_internal_console_dispose_on_shutdown は内部関数のため、console 公開 API
// の直後に置く。cplat_syslog_sink_* は Linux 専用のため、この Windows 専用ブロック
// には含めない。

// cplat/argparser/argparser.h
MOCK_CPLAT_LINK_IMPL(cplat_argparser_handle_create)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_init)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_handle_dispose)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_handle_register_flag)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_register_flag)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_handle_register_option_int)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_register_option_int)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_handle_register_option_string)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_register_option_string)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_handle_register_option_int_array)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_register_option_int_array)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_handle_register_option_string_array)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_register_option_string_array)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_handle_register_positional_int)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_register_positional_int)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_handle_register_positional_string)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_register_positional_string)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_handle_register_positional_int_array)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_register_positional_int_array)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_handle_register_positional_string_array)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_register_positional_string_array)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_handle_parse)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_parse)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_handle_get_error)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_get_error)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_handle_get_error_target)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_get_error_target)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_handle_get_error_index)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_get_error_index)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_handle_get_error_message)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_get_error_message)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_handle_get_usage)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_get_usage)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_handle_print_usage)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_print_usage)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_handle_print_error_messages)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_print_error_messages)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_handle_get_register_error_count)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_get_register_error_count)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_handle_get_register_error)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_get_register_error)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_handle_get_register_error_target)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_get_register_error_target)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_handle_get_register_error_message)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_get_register_error_message)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_handle_print_register_error_messages)
MOCK_CPLAT_LINK_IMPL(cplat_argparser_print_register_error_messages)

// cplat/hashtable/hashtable.h
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_required_size)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_create)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_create_growable)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_attach)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_validate)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_get_config_ref)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_get_config_val)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_buffer_size)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_buffer_ref)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_add)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_upsert)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_insert_direct)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_update)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_update_rec)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_find_value_ref)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_find_value_copy)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_find_recno)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_get_key_ref)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_get_key_copy)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_get_value_ref)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_get_value_copy)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_get_status)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_next_record)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_get_timestamp_ref)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_get_timestamp_val)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_get_generation)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_get_table_timestamp_ref)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_get_table_timestamp_val)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_get_table_generation)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_find_timestamp_ref)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_find_timestamp_val)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_find_generation)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_count_status)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_count)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_deleted_count)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_empty_count)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_delete)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_delete_rec)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_push_deleted)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_purge_deleted)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_compact)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_resize)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_rebuild_into)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_clear)
MOCK_CPLAT_LINK_IMPL(cplat_hashtable_dispose)

// cplat/clock/clock.h
MOCK_CPLAT_LINK_IMPL(cplat_clock_get_monotonic_ms)
MOCK_CPLAT_LINK_IMPL(cplat_clock_get_monotonic)
MOCK_CPLAT_LINK_IMPL(cplat_clock_get_realtime)
MOCK_CPLAT_LINK_IMPL(cplat_clock_format_realtime_iso8601_local)
MOCK_CPLAT_LINK_IMPL(cplat_clock_format_realtime_iso8601_utc)
MOCK_CPLAT_LINK_IMPL(cplat_clock_get_realtime_utc)
MOCK_CPLAT_LINK_IMPL(cplat_clock_get_realtime_deadline_ms)

// cplat/clock/timespec.h
MOCK_CPLAT_LINK_IMPL(cplat_timespec_normalize)
MOCK_CPLAT_LINK_IMPL(cplat_timespec_add)
MOCK_CPLAT_LINK_IMPL(cplat_timespec_sub)
MOCK_CPLAT_LINK_IMPL(cplat_timespec_cmp)
MOCK_CPLAT_LINK_IMPL(cplat_timespec_add_ms)
MOCK_CPLAT_LINK_IMPL(cplat_timespec_diff_ms)
MOCK_CPLAT_LINK_IMPL(cplat_timespec_to_native)
MOCK_CPLAT_LINK_IMPL(cplat_timespec_from_native)

// cplat/compress/compress.h
MOCK_CPLAT_LINK_IMPL(cplat_compress)
MOCK_CPLAT_LINK_IMPL(cplat_decompress)

// cplat/console/console.h
MOCK_CPLAT_LINK_IMPL(cplat_console_init)
MOCK_CPLAT_LINK_IMPL(cplat_console_dispose)
MOCK_CPLAT_LINK_IMPL(cplat_console_attach_parent)
MOCK_CPLAT_LINK_IMPL(cplat_console_write)

// cplat/console/console.h (internal)
MOCK_CPLAT_LINK_IMPL(cplat_internal_console_dispose_on_shutdown)

// cplat/crt/fcntl.h
MOCK_CPLAT_LINK_IMPL(cplat_open)
MOCK_CPLAT_LINK_IMPL(cplat_open_fmt)
MOCK_CPLAT_LINK_IMPL(cplat_vopen_fmt)

// cplat/crt/file.h
MOCK_CPLAT_LINK_IMPL(cplat_file_init)
MOCK_CPLAT_LINK_IMPL(cplat_file_open)
MOCK_CPLAT_LINK_IMPL(cplat_file_write)
MOCK_CPLAT_LINK_IMPL(cplat_file_read)
MOCK_CPLAT_LINK_IMPL(cplat_file_get_size)
MOCK_CPLAT_LINK_IMPL(cplat_file_set_size)
MOCK_CPLAT_LINK_IMPL(cplat_file_get_id)
MOCK_CPLAT_LINK_IMPL(cplat_file_get_path_id)
MOCK_CPLAT_LINK_IMPL(cplat_file_get_modified_timestamp)
MOCK_CPLAT_LINK_IMPL(cplat_file_set_modified_timestamp)
MOCK_CPLAT_LINK_IMPL(cplat_file_get_path_modified_timestamp)
MOCK_CPLAT_LINK_IMPL(cplat_file_set_path_modified_timestamp)
MOCK_CPLAT_LINK_IMPL(cplat_file_flush)
MOCK_CPLAT_LINK_IMPL(cplat_file_close)

// cplat/crt/path.h
MOCK_CPLAT_LINK_IMPL(cplat_path_normalize_sep)
MOCK_CPLAT_LINK_IMPL(cplat_path_get_full)
MOCK_CPLAT_LINK_IMPL(cplat_path_equal)
MOCK_CPLAT_LINK_IMPL(cplat_path_get_temp_dir)
MOCK_CPLAT_LINK_IMPL(cplat_path_concat_n)
MOCK_CPLAT_LINK_IMPL(cplat_vpath_concat_n)
MOCK_CPLAT_LINK_IMPL(cplat_path_basename)
MOCK_CPLAT_LINK_IMPL(cplat_path_dirname)
MOCK_CPLAT_LINK_IMPL(cplat_path_extension)
MOCK_CPLAT_LINK_IMPL(cplat_path_strip_extension)
MOCK_CPLAT_LINK_IMPL(cplat_path_join_n)
MOCK_CPLAT_LINK_IMPL(cplat_vpath_join_n)

// cplat/crt/stdio.h
MOCK_CPLAT_LINK_IMPL(cplat_fopen)
MOCK_CPLAT_LINK_IMPL(cplat_freopen)
MOCK_CPLAT_LINK_IMPL(cplat_fclose)
MOCK_CPLAT_LINK_IMPL(cplat_fflush)
MOCK_CPLAT_LINK_IMPL(cplat_fread)
MOCK_CPLAT_LINK_IMPL(cplat_fwrite)
MOCK_CPLAT_LINK_IMPL(cplat_remove)
MOCK_CPLAT_LINK_IMPL(cplat_rename)
MOCK_CPLAT_LINK_IMPL(cplat_scanf)
MOCK_CPLAT_LINK_IMPL(cplat_vscanf)
MOCK_CPLAT_LINK_IMPL(cplat_fscanf)
MOCK_CPLAT_LINK_IMPL(cplat_vfscanf)
MOCK_CPLAT_LINK_IMPL(cplat_snprintf)
MOCK_CPLAT_LINK_IMPL(cplat_vsnprintf)
MOCK_CPLAT_LINK_IMPL(cplat_fgets)
MOCK_CPLAT_LINK_IMPL(cplat_fprintf)
MOCK_CPLAT_LINK_IMPL(cplat_vfprintf)
MOCK_CPLAT_LINK_IMPL(cplat_fseek)
MOCK_CPLAT_LINK_IMPL(cplat_ftell)
MOCK_CPLAT_LINK_IMPL(cplat_fopen_fmt)
MOCK_CPLAT_LINK_IMPL(cplat_vfopen_fmt)
MOCK_CPLAT_LINK_IMPL(cplat_remove_fmt)
MOCK_CPLAT_LINK_IMPL(cplat_vremove_fmt)
MOCK_CPLAT_LINK_IMPL(cplat_fopen_temp)

// cplat/crt/stdlib.h
MOCK_CPLAT_LINK_IMPL(cplat_getenv)
MOCK_CPLAT_LINK_IMPL(cplat_setenv)
MOCK_CPLAT_LINK_IMPL(cplat_unsetenv)
MOCK_CPLAT_LINK_IMPL(cplat_parse_int64)
MOCK_CPLAT_LINK_IMPL(cplat_parse_uint64)
MOCK_CPLAT_LINK_IMPL(cplat_parse_int)
MOCK_CPLAT_LINK_IMPL(cplat_parse_double)
MOCK_CPLAT_LINK_IMPL(cplat_malloc)
MOCK_CPLAT_LINK_IMPL(cplat_malloc_zerofill)
MOCK_CPLAT_LINK_IMPL(cplat_calloc)
MOCK_CPLAT_LINK_IMPL(cplat_realloc)
MOCK_CPLAT_LINK_IMPL(cplat_realloc_zerofill)
MOCK_CPLAT_LINK_IMPL(cplat_free)

// cplat/crt/string.h
MOCK_CPLAT_LINK_IMPL(cplat_strcpy)
MOCK_CPLAT_LINK_IMPL(cplat_strncpy)
MOCK_CPLAT_LINK_IMPL(cplat_strcat)
MOCK_CPLAT_LINK_IMPL(cplat_strncat)
MOCK_CPLAT_LINK_IMPL(cplat_strtok_r)
MOCK_CPLAT_LINK_IMPL(cplat_strdup)
MOCK_CPLAT_LINK_IMPL(cplat_strcasecmp)
MOCK_CPLAT_LINK_IMPL(cplat_strncasecmp)
MOCK_CPLAT_LINK_IMPL(cplat_wcscpy)
MOCK_CPLAT_LINK_IMPL(cplat_sscanf)
MOCK_CPLAT_LINK_IMPL(cplat_vsscanf)

// cplat/crt/sys/stat.h
MOCK_CPLAT_LINK_IMPL(cplat_stat)
MOCK_CPLAT_LINK_IMPL(cplat_mkdir)
MOCK_CPLAT_LINK_IMPL(cplat_makedirs)
MOCK_CPLAT_LINK_IMPL(cplat_rmdir)
MOCK_CPLAT_LINK_IMPL(cplat_stat_fmt)
MOCK_CPLAT_LINK_IMPL(cplat_vstat_fmt)
MOCK_CPLAT_LINK_IMPL(cplat_mkdir_fmt)
MOCK_CPLAT_LINK_IMPL(cplat_vmkdir_fmt)
MOCK_CPLAT_LINK_IMPL(cplat_file_stat_is_regular)

// cplat/crt/time.h
MOCK_CPLAT_LINK_IMPL(cplat_gmtime)
MOCK_CPLAT_LINK_IMPL(cplat_localtime)
MOCK_CPLAT_LINK_IMPL(cplat_ctime)

// cplat/crt/unistd.h
MOCK_CPLAT_LINK_IMPL(cplat_isatty)
MOCK_CPLAT_LINK_IMPL(cplat_lseek)
MOCK_CPLAT_LINK_IMPL(cplat_close)
MOCK_CPLAT_LINK_IMPL(cplat_dup)
MOCK_CPLAT_LINK_IMPL(cplat_dup2)
MOCK_CPLAT_LINK_IMPL(cplat_read)
MOCK_CPLAT_LINK_IMPL(cplat_write)
MOCK_CPLAT_LINK_IMPL(cplat_access)
MOCK_CPLAT_LINK_IMPL(cplat_access_fmt)
MOCK_CPLAT_LINK_IMPL(cplat_vaccess_fmt)

// cplat/crypto/crypto.h
MOCK_CPLAT_LINK_IMPL(cplat_crypto_encrypt)
MOCK_CPLAT_LINK_IMPL(cplat_crypto_decrypt)
MOCK_CPLAT_LINK_IMPL(cplat_passphrase_to_key)

// cplat/base/error.h
MOCK_CPLAT_LINK_IMPL(cplat_error_clear)
MOCK_CPLAT_LINK_IMPL(cplat_error_capture_errno)
MOCK_CPLAT_LINK_IMPL(cplat_error_capture_current_errno)
MOCK_CPLAT_LINK_IMPL(cplat_error_get_last)
MOCK_CPLAT_LINK_IMPL(cplat_error_set_last)
MOCK_CPLAT_LINK_IMPL(cplat_error_clear_last)
MOCK_CPLAT_LINK_IMPL(cplat_error_is_set)
MOCK_CPLAT_LINK_IMPL(cplat_error_get_domain)
MOCK_CPLAT_LINK_IMPL(cplat_error_get_errno)
MOCK_CPLAT_LINK_IMPL(cplat_error_to_result)
MOCK_CPLAT_LINK_IMPL(cplat_error_get_cause)
MOCK_CPLAT_LINK_IMPL(cplat_error_is)

// cplat/base/error_message.h
MOCK_CPLAT_LINK_IMPL(cplat_result_to_string)
MOCK_CPLAT_LINK_IMPL(cplat_error_message)

// cplat/crypto/random.h
MOCK_CPLAT_LINK_IMPL(cplat_random_bytes)

// cplat/locale/ui_language.h
MOCK_CPLAT_LINK_IMPL(cplat_ui_language_get_tag)

// cplat/mmap/mmap.h
MOCK_CPLAT_LINK_IMPL(cplat_mmap_attach)
MOCK_CPLAT_LINK_IMPL(cplat_mmap_get_address)
MOCK_CPLAT_LINK_IMPL(cplat_mmap_get_size)
MOCK_CPLAT_LINK_IMPL(cplat_mmap_flush)
MOCK_CPLAT_LINK_IMPL(cplat_mmap_detach)

// cplat/net/byteorder.h
MOCK_CPLAT_LINK_IMPL(cplat_hton16)
MOCK_CPLAT_LINK_IMPL(cplat_ntoh16)
MOCK_CPLAT_LINK_IMPL(cplat_hton32)
MOCK_CPLAT_LINK_IMPL(cplat_ntoh32)
MOCK_CPLAT_LINK_IMPL(cplat_hton64)
MOCK_CPLAT_LINK_IMPL(cplat_ntoh64)

// cplat/net/endpoint.h
MOCK_CPLAT_LINK_IMPL(cplat_ipv4_parse)
MOCK_CPLAT_LINK_IMPL(cplat_ipv4_resolve)
MOCK_CPLAT_LINK_IMPL(cplat_ipv4_to_string)

// cplat/net/socket.h
MOCK_CPLAT_LINK_IMPL(cplat_socket_open)
MOCK_CPLAT_LINK_IMPL(cplat_socket_close)
MOCK_CPLAT_LINK_IMPL(cplat_socket_shutdown)
MOCK_CPLAT_LINK_IMPL(cplat_socket_bind)
MOCK_CPLAT_LINK_IMPL(cplat_socket_listen)
MOCK_CPLAT_LINK_IMPL(cplat_socket_accept)
MOCK_CPLAT_LINK_IMPL(cplat_socket_connect)
MOCK_CPLAT_LINK_IMPL(cplat_socket_get_pending_error)
MOCK_CPLAT_LINK_IMPL(cplat_socket_set_nonblocking)
MOCK_CPLAT_LINK_IMPL(cplat_socket_set_reuse_address)
MOCK_CPLAT_LINK_IMPL(cplat_socket_set_broadcast)
MOCK_CPLAT_LINK_IMPL(cplat_socket_set_multicast_interface)
MOCK_CPLAT_LINK_IMPL(cplat_socket_join_multicast_group)
MOCK_CPLAT_LINK_IMPL(cplat_socket_leave_multicast_group)
MOCK_CPLAT_LINK_IMPL(cplat_socket_send)
MOCK_CPLAT_LINK_IMPL(cplat_socket_recv)
MOCK_CPLAT_LINK_IMPL(cplat_socket_sendto)
MOCK_CPLAT_LINK_IMPL(cplat_socket_recvfrom)
MOCK_CPLAT_LINK_IMPL(cplat_socket_send_all)
MOCK_CPLAT_LINK_IMPL(cplat_socket_recv_all)
MOCK_CPLAT_LINK_IMPL(cplat_socket_wait_readable)
MOCK_CPLAT_LINK_IMPL(cplat_socket_wait_writable)
MOCK_CPLAT_LINK_IMPL(cplat_socket_wait_readable_multi)
MOCK_CPLAT_LINK_IMPL(cplat_socket_shutdown_receive)

// cplat/prompt/pinned_prompt.h
MOCK_CPLAT_LINK_IMPL(cplat_pinned_prompt_create)
MOCK_CPLAT_LINK_IMPL(cplat_pinned_prompt_dispose)
MOCK_CPLAT_LINK_IMPL(cplat_pinned_prompt_readline_at)
MOCK_CPLAT_LINK_IMPL(cplat_pinned_prompt_readline_with_initial_at)
MOCK_CPLAT_LINK_IMPL(cplat_pinned_prompt_readline_fmt_at)
MOCK_CPLAT_LINK_IMPL(cplat_pinned_prompt_write)
MOCK_CPLAT_LINK_IMPL(cplat_pinned_prompt_printf)
MOCK_CPLAT_LINK_IMPL(cplat_pinned_prompt_status_enable)
MOCK_CPLAT_LINK_IMPL(cplat_pinned_prompt_status_set)

// cplat/prompt/prompt.h
MOCK_CPLAT_LINK_IMPL(cplat_prompt_create)
MOCK_CPLAT_LINK_IMPL(cplat_prompt_dispose)
MOCK_CPLAT_LINK_IMPL(cplat_prompt_readline_at)
MOCK_CPLAT_LINK_IMPL(cplat_prompt_readline_with_initial_at)
MOCK_CPLAT_LINK_IMPL(cplat_prompt_readline_fmt_at)

// cplat/runtime/elevated_process.h
MOCK_CPLAT_LINK_IMPL(cplat_elevated_process_is_elevated)
MOCK_CPLAT_LINK_IMPL(cplat_elevated_process_run_if_needed)
MOCK_CPLAT_LINK_IMPL(cplat_elevated_process_run_with_result)
MOCK_CPLAT_LINK_IMPL(cplat_elevated_process_extract_result_target)
MOCK_CPLAT_LINK_IMPL(cplat_elevated_process_report_result)
MOCK_CPLAT_LINK_IMPL(cplat_elevated_process_run_piped)
MOCK_CPLAT_LINK_IMPL(cplat_elevated_process_attach_output_pipes)

// cplat/regex/regex.h
MOCK_CPLAT_LINK_IMPL(cplat_regex_create)
MOCK_CPLAT_LINK_IMPL(cplat_regex_dispose)
MOCK_CPLAT_LINK_IMPL(cplat_regex_get_group_count)
MOCK_CPLAT_LINK_IMPL(cplat_regex_search)
MOCK_CPLAT_LINK_IMPL(cplat_regex_matches)
MOCK_CPLAT_LINK_IMPL(cplat_regex_replace)
MOCK_CPLAT_LINK_IMPL(cplat_regex_iter_create)
MOCK_CPLAT_LINK_IMPL(cplat_regex_iter_next)
MOCK_CPLAT_LINK_IMPL(cplat_regex_iter_dispose)
MOCK_CPLAT_LINK_IMPL(cplat_regex_split)

// cplat/runtime/memory_lock.h
MOCK_CPLAT_LINK_IMPL(cplat_memory_lock_range)
MOCK_CPLAT_LINK_IMPL(cplat_memory_unlock_range)
MOCK_CPLAT_LINK_IMPL(cplat_memory_lock_self)
MOCK_CPLAT_LINK_IMPL(cplat_memory_lock_scope_release)
MOCK_CPLAT_LINK_IMPL(cplat_secure_zero)

// cplat/runtime/host.h
MOCK_CPLAT_LINK_IMPL(cplat_host_get_name)

// cplat/runtime/module.h
MOCK_CPLAT_LINK_IMPL(cplat_module_get_path)
MOCK_CPLAT_LINK_IMPL(cplat_module_get_basename)

// cplat/runtime/process.h
MOCK_CPLAT_LINK_IMPL(cplat_process_get_executable_path)
MOCK_CPLAT_LINK_IMPL(cplat_process_get_pid)
MOCK_CPLAT_LINK_IMPL(cplat_process_get_tid)
MOCK_CPLAT_LINK_IMPL(cplat_process_start)
MOCK_CPLAT_LINK_IMPL(cplat_process_wait)
MOCK_CPLAT_LINK_IMPL(cplat_process_get_exit_code)
MOCK_CPLAT_LINK_IMPL(cplat_process_terminate)
MOCK_CPLAT_LINK_IMPL(cplat_process_dispose)
MOCK_CPLAT_LINK_IMPL(cplat_process_run_sync)

// cplat/runtime/shutdown.h
MOCK_CPLAT_LINK_IMPL(cplat_shutdown_register)
MOCK_CPLAT_LINK_IMPL(cplat_shutdown_request_register)
MOCK_CPLAT_LINK_IMPL(cplat_exit)
MOCK_CPLAT_LINK_IMPL(cplat_shutdown_invoke_for_test)
MOCK_CPLAT_LINK_IMPL(cplat_shutdown_request_invoke_for_test)
MOCK_CPLAT_LINK_IMPL(cplat_shutdown_reset_for_test)

// cplat/runtime/sym_loader.h
MOCK_CPLAT_LINK_IMPL(cplat_sym_loader_resolve)
MOCK_CPLAT_LINK_IMPL(cplat_sym_loader_is_default)
MOCK_CPLAT_LINK_IMPL(cplat_sym_loader_init)
MOCK_CPLAT_LINK_IMPL(cplat_sym_loader_dispose)
MOCK_CPLAT_LINK_IMPL(cplat_sym_loader_info)

// cplat/string_catalog/string_catalog.h
MOCK_CPLAT_LINK_IMPL(cplat_string_catalog_set_language)
MOCK_CPLAT_LINK_IMPL(cplat_string_catalog_get_language)
MOCK_CPLAT_LINK_IMPL(cplat_string_catalog_language_from_tag)
MOCK_CPLAT_LINK_IMPL(cplat_string_catalog_format)
MOCK_CPLAT_LINK_IMPL(cplat_string_catalog_vformat)
MOCK_CPLAT_LINK_IMPL(cplat_string_catalog_verify)
MOCK_CPLAT_LINK_IMPL(cplat_string_catalog_get_entry)
MOCK_CPLAT_LINK_IMPL(cplat_string_catalog_get_category)
MOCK_CPLAT_LINK_IMPL(cplat_string_catalog_get_id)
MOCK_CPLAT_LINK_IMPL(cplat_string_catalog_get_note)

// cplat/string_catalog/filter.h
MOCK_CPLAT_LINK_IMPL(cplat_string_catalog_filter_compile)
MOCK_CPLAT_LINK_IMPL(cplat_string_catalog_filter_compile_line)
MOCK_CPLAT_LINK_IMPL(cplat_string_catalog_filter_insert_line)
MOCK_CPLAT_LINK_IMPL(cplat_string_catalog_filter_remove_line)
MOCK_CPLAT_LINK_IMPL(cplat_string_catalog_filter_validate)
MOCK_CPLAT_LINK_IMPL(cplat_string_catalog_filter_get_info)
MOCK_CPLAT_LINK_IMPL(cplat_string_catalog_filter_decompile_line)
MOCK_CPLAT_LINK_IMPL(cplat_string_catalog_filter_slot_create)
MOCK_CPLAT_LINK_IMPL(cplat_string_catalog_filter_slot_dispose)
MOCK_CPLAT_LINK_IMPL(cplat_string_catalog_filter_slot_get_catalog)
MOCK_CPLAT_LINK_IMPL(cplat_string_catalog_filter_source_publish)
MOCK_CPLAT_LINK_IMPL(cplat_string_catalog_filter_get_catalog_id)
MOCK_CPLAT_LINK_IMPL(cplat_string_catalog_filter_source_get_info)
MOCK_CPLAT_LINK_IMPL(cplat_string_catalog_filter_slot_attach_source)
MOCK_CPLAT_LINK_IMPL(cplat_string_catalog_filter_slot_get_source_status)
MOCK_CPLAT_LINK_IMPL(cplat_string_catalog_filter_slot_apply)
MOCK_CPLAT_LINK_IMPL(cplat_string_catalog_filter_slot_check)
MOCK_CPLAT_LINK_IMPL(cplat_string_catalog_filter_slot_snapshot)
MOCK_CPLAT_LINK_IMPL(cplat_string_catalog_filter_slot_get_line_error)
MOCK_CPLAT_LINK_IMPL(cplat_string_catalog_filter_slot_describe_line)
MOCK_CPLAT_LINK_IMPL(cplat_string_catalog_filter_slot_test)
MOCK_CPLAT_LINK_IMPL(cplat_string_catalog_filter_slot_vformat)
MOCK_CPLAT_LINK_IMPL(cplat_string_catalog_filter_slot_format)

// cplat/sync/sync.h
MOCK_CPLAT_LINK_IMPL(cplat_local_lock_create)
MOCK_CPLAT_LINK_IMPL(cplat_local_lock_lock)
MOCK_CPLAT_LINK_IMPL(cplat_local_lock_try_lock)
MOCK_CPLAT_LINK_IMPL(cplat_local_lock_unlock)
MOCK_CPLAT_LINK_IMPL(cplat_local_lock_dispose)
MOCK_CPLAT_LINK_IMPL(cplat_condvar_create)
MOCK_CPLAT_LINK_IMPL(cplat_condvar_wait)
MOCK_CPLAT_LINK_IMPL(cplat_condvar_signal)
MOCK_CPLAT_LINK_IMPL(cplat_condvar_broadcast)
MOCK_CPLAT_LINK_IMPL(cplat_condvar_dispose)
MOCK_CPLAT_LINK_IMPL(cplat_local_rwlock_create)
MOCK_CPLAT_LINK_IMPL(cplat_local_rwlock_lock_shared)
MOCK_CPLAT_LINK_IMPL(cplat_local_rwlock_try_lock_shared)
MOCK_CPLAT_LINK_IMPL(cplat_local_rwlock_lock_exclusive)
MOCK_CPLAT_LINK_IMPL(cplat_local_rwlock_try_lock_exclusive)
MOCK_CPLAT_LINK_IMPL(cplat_local_rwlock_unlock_shared)
MOCK_CPLAT_LINK_IMPL(cplat_local_rwlock_unlock_exclusive)
MOCK_CPLAT_LINK_IMPL(cplat_local_rwlock_dispose)
MOCK_CPLAT_LINK_IMPL(cplat_thread_create)
MOCK_CPLAT_LINK_IMPL(cplat_thread_join)
MOCK_CPLAT_LINK_IMPL(cplat_thread_detach)
MOCK_CPLAT_LINK_IMPL(cplat_interprocess_lock_open)
MOCK_CPLAT_LINK_IMPL(cplat_interprocess_lock_import_descriptor)
MOCK_CPLAT_LINK_IMPL(cplat_interprocess_lock_export_descriptor)
MOCK_CPLAT_LINK_IMPL(cplat_interprocess_lock_lock)
MOCK_CPLAT_LINK_IMPL(cplat_interprocess_lock_try_lock)
MOCK_CPLAT_LINK_IMPL(cplat_interprocess_lock_unlock)
MOCK_CPLAT_LINK_IMPL(cplat_interprocess_lock_dispose)
MOCK_CPLAT_LINK_IMPL(cplat_interprocess_rwlock_open)
MOCK_CPLAT_LINK_IMPL(cplat_interprocess_rwlock_import_descriptor)
MOCK_CPLAT_LINK_IMPL(cplat_interprocess_rwlock_export_descriptor)
MOCK_CPLAT_LINK_IMPL(cplat_interprocess_rwlock_lock_shared)
MOCK_CPLAT_LINK_IMPL(cplat_interprocess_rwlock_try_lock_shared)
MOCK_CPLAT_LINK_IMPL(cplat_interprocess_rwlock_lock_exclusive)
MOCK_CPLAT_LINK_IMPL(cplat_interprocess_rwlock_try_lock_exclusive)
MOCK_CPLAT_LINK_IMPL(cplat_interprocess_rwlock_unlock)
MOCK_CPLAT_LINK_IMPL(cplat_interprocess_rwlock_dispose)
MOCK_CPLAT_LINK_IMPL(cplat_call_once)
MOCK_CPLAT_LINK_IMPL(cplat_sleep_ms)

// cplat/trace/trace_file.h
MOCK_CPLAT_LINK_IMPL(cplat_trace_file_sink_create)
MOCK_CPLAT_LINK_IMPL(cplat_trace_file_sink_write)
MOCK_CPLAT_LINK_IMPL(cplat_trace_file_sink_dispose)

// cplat/trace/tracer.h
MOCK_CPLAT_LINK_IMPL(cplat_tracer_create)
MOCK_CPLAT_LINK_IMPL(cplat_tracer_start)
MOCK_CPLAT_LINK_IMPL(cplat_tracer_stop)
MOCK_CPLAT_LINK_IMPL(cplat_tracer_get_state)
MOCK_CPLAT_LINK_IMPL(cplat_tracer_write_at)
MOCK_CPLAT_LINK_IMPL(cplat_tracer_writef_at)
MOCK_CPLAT_LINK_IMPL(cplat_tracer_vwritef_at)
MOCK_CPLAT_LINK_IMPL(cplat_tracer_write_hex_at)
MOCK_CPLAT_LINK_IMPL(cplat_tracer_write_hexf_at)
MOCK_CPLAT_LINK_IMPL(cplat_tracer_vwrite_hexf_at)
MOCK_CPLAT_LINK_IMPL(cplat_tracer_hex_sep)
MOCK_CPLAT_LINK_IMPL(cplat_tracer_hex_msg)
MOCK_CPLAT_LINK_IMPL(cplat_tracer_write_with_source)
MOCK_CPLAT_LINK_IMPL(cplat_tracer_set_name)
MOCK_CPLAT_LINK_IMPL(cplat_tracer_get_name)
MOCK_CPLAT_LINK_IMPL(cplat_tracer_get_identifier)
MOCK_CPLAT_LINK_IMPL(cplat_tracer_set_file_name)
MOCK_CPLAT_LINK_IMPL(cplat_tracer_get_file_name)
MOCK_CPLAT_LINK_IMPL(cplat_tracer_get_file_identifier)
MOCK_CPLAT_LINK_IMPL(cplat_tracer_get_os_level)
MOCK_CPLAT_LINK_IMPL(cplat_tracer_set_os_level)
MOCK_CPLAT_LINK_IMPL(cplat_tracer_get_etw_level)
MOCK_CPLAT_LINK_IMPL(cplat_tracer_set_etw_level)
MOCK_CPLAT_LINK_IMPL(cplat_tracer_get_file_level)
MOCK_CPLAT_LINK_IMPL(cplat_tracer_set_file_level)
MOCK_CPLAT_LINK_IMPL(cplat_tracer_get_stderr_level)
MOCK_CPLAT_LINK_IMPL(cplat_tracer_set_stderr_level)
MOCK_CPLAT_LINK_IMPL(cplat_tracer_dispose)
MOCK_CPLAT_LINK_IMPL(cplat_tracer_set_hook)
MOCK_CPLAT_LINK_IMPL(cplat_tracer_remove_hook)
MOCK_CPLAT_LINK_IMPL(cplat_tracer_call_next_hook)

    #if defined(PLATFORM_WINDOWS)

// cplat/crt/wchar_conv.h
MOCK_CPLAT_LINK_IMPL(cplat_utf8_to_wpath)
MOCK_CPLAT_LINK_IMPL(cplat_utf8_to_wstr)
MOCK_CPLAT_LINK_IMPL(cplat_wpath_to_utf8)
MOCK_CPLAT_LINK_IMPL(cplat_wstr_to_utf8)
MOCK_CPLAT_LINK_IMPL(cplat_utf8_to_wstr_alloc)
MOCK_CPLAT_LINK_IMPL(cplat_wstr_to_utf8_alloc)

// cplat/base/error.h
MOCK_CPLAT_LINK_IMPL(cplat_error_capture_windows_error)
MOCK_CPLAT_LINK_IMPL(cplat_error_capture_current_windows_error)
MOCK_CPLAT_LINK_IMPL(cplat_error_get_windows_error)

// cplat/trace/etw.h
MOCK_CPLAT_LINK_IMPL(cplat_etw_provider_create)
MOCK_CPLAT_LINK_IMPL(cplat_etw_provider_write)
MOCK_CPLAT_LINK_IMPL(cplat_etw_provider_dispose)
MOCK_CPLAT_LINK_IMPL(cplat_etw_session_check_access)
MOCK_CPLAT_LINK_IMPL(cplat_etw_session_start)
MOCK_CPLAT_LINK_IMPL(cplat_etw_session_stop)

// cplat/trace/eventlog.h
MOCK_CPLAT_LINK_IMPL(cplat_eventlog_sink_create)
MOCK_CPLAT_LINK_IMPL(cplat_eventlog_sink_write)
MOCK_CPLAT_LINK_IMPL(cplat_eventlog_sink_dispose)
MOCK_CPLAT_LINK_IMPL(cplat_eventlog_register_source)
MOCK_CPLAT_LINK_IMPL(cplat_eventlog_unregister_source)

// cplat/win32/win32.h
MOCK_CPLAT_LINK_IMPL(CreateFileU)
MOCK_CPLAT_LINK_IMPL(CreateNamedPipeU)
MOCK_CPLAT_LINK_IMPL(GetModuleFileNameU)
MOCK_CPLAT_LINK_IMPL(WriteConsoleU)
MOCK_CPLAT_LINK_IMPL(GetVolumePathNameU)
MOCK_CPLAT_LINK_IMPL(GetVolumeInformationU)
MOCK_CPLAT_LINK_IMPL(LoadLibraryU)
MOCK_CPLAT_LINK_IMPL(CreateProcessU)
MOCK_CPLAT_LINK_IMPL(OpenSCManagerU)
MOCK_CPLAT_LINK_IMPL(CreateServiceU)
MOCK_CPLAT_LINK_IMPL(OpenServiceU)
MOCK_CPLAT_LINK_IMPL(ChangeServiceConfig2U)
MOCK_CPLAT_LINK_IMPL(RegisterServiceCtrlHandlerExU)
MOCK_CPLAT_LINK_IMPL(StartServiceCtrlDispatcherU)
    #endif /* PLATFORM_WINDOWS */
    #undef MOCK_CPLAT_LINK_IMPL
#endif /* COMPILER_MSVC */

#include <cplat/argparser/argparser.h>
#include <cplat/hashtable/hashtable.h>
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
#include <cplat/base/error.h>
#include <cplat/base/error_message.h>
#include <cplat/crypto/crypto.h>
#include <cplat/crypto/random.h>
#include <cplat/locale/ui_language.h>
#include <cplat/mmap/mmap.h>
#include <cplat/net/byteorder.h>
#include <cplat/net/endpoint.h>
#include <cplat/net/socket.h>
#include <cplat/regex/regex.h>
#include <cplat/runtime/elevated_process.h>
#include <cplat/runtime/host.h>
#include <cplat/runtime/memory_lock.h>
#include <cplat/runtime/module.h>
#include <cplat/runtime/process.h>
#include <cplat/runtime/shutdown.h>
#include <cplat/runtime/sym_loader.h>
#include <cplat/string_catalog/filter.h>
#include <cplat/string_catalog/string_catalog.h>
#include <cplat/sync/sync.h>
#include <cplat/trace/trace_file.h>
#include <cplat/trace/tracer.h>
#include <cplat/prompt/prompt.h>
#include <cplat/prompt/pinned_prompt.h>
#if defined(PLATFORM_WINDOWS)
    #include <cplat/crt/wchar_conv.h>
    #include <cplat/trace/etw.h>
    #include <cplat/trace/eventlog.h>
    #include <cplat/win32/win32.h>
#elif defined(PLATFORM_LINUX)
    #include <cplat/trace/syslog.h>
#endif /* PLATFORM_ */

inline constexpr char kLibCplatName[] = "libcplat" TESTFW_SHARED_LIBRARY_EXTENSION;

// 書式を展開した NUL 終端文字列を返す。
// 期待値の照合と実関数への委譲の双方で使用する。固定長バッファーで展開すると長い出力が
// 切り詰められ、被テスト側の切り詰め判定が実関数と一致しなくなるため、testfw の allocvprintf で
// 必要な長さを確保する。戻り値は解放不要で、.data() は常に有効な NUL 終端文字列を指す。
extern std::vector<char> mock_cplat_expand_format(const char *format, va_list args);

// cplat/argparser/argparser.h
extern cplat_argparser *delegate_real_cplat_argparser_handle_create(int argc, char *const *argv,
                                                                    const cplat_argparser_options *options);
extern void delegate_real_cplat_argparser_init(int argc, char *const *argv, const char *description);
extern void delegate_real_cplat_argparser_handle_dispose(cplat_argparser *parser);
extern int delegate_real_cplat_argparser_handle_register_flag(cplat_argparser *parser, const char *short_name,
                                                              const char *long_name, const char *description,
                                                              int *storage);
extern int delegate_real_cplat_argparser_register_flag(const char *short_name, const char *long_name,
                                                       const char *description, int *storage);
extern int delegate_real_cplat_argparser_handle_register_option_int(cplat_argparser *parser, const char *short_name,
                                                                    const char *long_name, const char *value_name,
                                                                    const char *description, unsigned int flags,
                                                                    int *storage);
extern int delegate_real_cplat_argparser_register_option_int(const char *short_name, const char *long_name,
                                                             const char *value_name, const char *description,
                                                             unsigned int flags, int *storage);
extern int delegate_real_cplat_argparser_handle_register_option_string(cplat_argparser *parser, const char *short_name,
                                                                       const char *long_name, const char *value_name,
                                                                       const char *description, unsigned int flags,
                                                                       const char **storage);
extern int delegate_real_cplat_argparser_register_option_string(const char *short_name, const char *long_name,
                                                                const char *value_name, const char *description,
                                                                unsigned int flags, const char **storage);
extern int delegate_real_cplat_argparser_handle_register_option_int_array(cplat_argparser *parser,
                                                                          const char *short_name, const char *long_name,
                                                                          const char *value_name,
                                                                          const char *description, unsigned int flags,
                                                                          int *storage, size_t capacity, size_t *count);
extern int delegate_real_cplat_argparser_register_option_int_array(const char *short_name, const char *long_name,
                                                                   const char *value_name, const char *description,
                                                                   unsigned int flags, int *storage, size_t capacity,
                                                                   size_t *count);
extern int delegate_real_cplat_argparser_handle_register_option_string_array(
    cplat_argparser *parser, const char *short_name, const char *long_name, const char *value_name,
    const char *description, unsigned int flags, const char **storage, size_t capacity, size_t *count);
extern int delegate_real_cplat_argparser_register_option_string_array(const char *short_name, const char *long_name,
                                                                      const char *value_name, const char *description,
                                                                      unsigned int flags, const char **storage,
                                                                      size_t capacity, size_t *count);
extern int delegate_real_cplat_argparser_handle_register_positional_int(cplat_argparser *parser, const char *name,
                                                                        const char *description, unsigned int flags,
                                                                        int *storage);
extern int delegate_real_cplat_argparser_register_positional_int(const char *name, const char *description,
                                                                 unsigned int flags, int *storage);
extern int delegate_real_cplat_argparser_handle_register_positional_string(cplat_argparser *parser, const char *name,
                                                                           const char *description, unsigned int flags,
                                                                           const char **storage);
extern int delegate_real_cplat_argparser_register_positional_string(const char *name, const char *description,
                                                                    unsigned int flags, const char **storage);
extern int delegate_real_cplat_argparser_handle_register_positional_int_array(cplat_argparser *parser, const char *name,
                                                                              const char *description,
                                                                              unsigned int flags, int *storage,
                                                                              size_t capacity, size_t *count);
extern int delegate_real_cplat_argparser_register_positional_int_array(const char *name, const char *description,
                                                                       unsigned int flags, int *storage,
                                                                       size_t capacity, size_t *count);
extern int delegate_real_cplat_argparser_handle_register_positional_string_array(
    cplat_argparser *parser, const char *name, const char *description, unsigned int flags, const char **storage,
    size_t capacity, size_t *count);
extern int delegate_real_cplat_argparser_register_positional_string_array(const char *name, const char *description,
                                                                          unsigned int flags, const char **storage,
                                                                          size_t capacity, size_t *count);
extern int delegate_real_cplat_argparser_handle_parse(cplat_argparser *parser);
extern int delegate_real_cplat_argparser_parse(void);
extern int delegate_real_cplat_argparser_handle_get_error(const cplat_argparser *parser);
extern int delegate_real_cplat_argparser_get_error(void);
extern const char *delegate_real_cplat_argparser_handle_get_error_target(const cplat_argparser *parser);
extern const char *delegate_real_cplat_argparser_get_error_target(void);
extern int delegate_real_cplat_argparser_handle_get_error_index(const cplat_argparser *parser);
extern int delegate_real_cplat_argparser_get_error_index(void);
extern int delegate_real_cplat_argparser_handle_get_error_message(const cplat_argparser *parser, char *buffer,
                                                                  size_t buffer_size);
extern int delegate_real_cplat_argparser_get_error_message(char *buffer, size_t buffer_size);
extern int delegate_real_cplat_argparser_handle_get_usage(const cplat_argparser *parser, char *buffer,
                                                          size_t buffer_size, size_t *required_size);
extern int delegate_real_cplat_argparser_get_usage(char *buffer, size_t buffer_size, size_t *required_size);
extern int delegate_real_cplat_argparser_handle_print_usage(const cplat_argparser *parser, FILE *stream);
extern int delegate_real_cplat_argparser_print_usage(FILE *stream);
extern int delegate_real_cplat_argparser_handle_print_error_messages(const cplat_argparser *parser, FILE *stream);
extern int delegate_real_cplat_argparser_print_error_messages(FILE *stream);
extern size_t delegate_real_cplat_argparser_handle_get_register_error_count(const cplat_argparser *parser);
extern size_t delegate_real_cplat_argparser_get_register_error_count(void);
extern int delegate_real_cplat_argparser_handle_get_register_error(const cplat_argparser *parser, size_t index);
extern int delegate_real_cplat_argparser_get_register_error(size_t index);
extern const char *delegate_real_cplat_argparser_handle_get_register_error_target(const cplat_argparser *parser,
                                                                                  size_t index);
extern const char *delegate_real_cplat_argparser_get_register_error_target(size_t index);
extern int delegate_real_cplat_argparser_handle_get_register_error_message(const cplat_argparser *parser, size_t index,
                                                                           char *buffer, size_t buffer_size);
extern int delegate_real_cplat_argparser_get_register_error_message(size_t index, char *buffer, size_t buffer_size);
extern int delegate_real_cplat_argparser_handle_print_register_error_messages(const cplat_argparser *parser,
                                                                              FILE *stream);
extern int delegate_real_cplat_argparser_print_register_error_messages(FILE *stream);

// cplat/hashtable/hashtable.h
extern int delegate_real_cplat_hashtable_required_size(const cplat_hashtable_config *config, size_t *mgmt_size_out,
                                                       size_t *data_size_out);
extern int delegate_real_cplat_hashtable_create(const cplat_hashtable_config *config, void *buf_mgmt,
                                                size_t buf_mgmt_size, void *buf_data, size_t buf_data_size,
                                                cplat_hashtable **ht_out);
extern int delegate_real_cplat_hashtable_create_growable(const cplat_hashtable_config *initial_config,
                                                         const cplat_hashtable_growth_config *growth_config,
                                                         cplat_hashtable **ht_out);
extern int delegate_real_cplat_hashtable_attach(void *buf_mgmt, size_t buf_mgmt_size, void *buf_data,
                                                size_t buf_data_size, cplat_hashtable **ht_out);
extern int delegate_real_cplat_hashtable_validate(const cplat_hashtable *ht);
extern int delegate_real_cplat_hashtable_get_config_ref(const cplat_hashtable *ht,
                                                        const cplat_hashtable_config **config_out);
extern int delegate_real_cplat_hashtable_get_config_val(const cplat_hashtable *ht, cplat_hashtable_config *config_out);
extern int delegate_real_cplat_hashtable_buffer_size(const cplat_hashtable *ht, size_t *mgmt_size_out,
                                                     size_t *data_size_out);
extern int delegate_real_cplat_hashtable_buffer_ref(const cplat_hashtable *ht, const void **mgmt_out,
                                                    const void **data_out);
extern int delegate_real_cplat_hashtable_add(cplat_hashtable *ht, const void *key, const void *value,
                                             cplat_hashtable_add_deleted_policy deleted_policy);
extern int delegate_real_cplat_hashtable_upsert(cplat_hashtable *ht, const void *key, const void *value,
                                                int *inserted_out);
extern int delegate_real_cplat_hashtable_insert_direct(cplat_hashtable *ht, uint64_t record, const void *key,
                                                       int status, const void *value, const cplat_timespec *timestamp,
                                                       uint64_t generation);
extern int delegate_real_cplat_hashtable_update(cplat_hashtable *ht, const void *key, const void *value);
extern int delegate_real_cplat_hashtable_update_rec(cplat_hashtable *ht, uint64_t record, const void *value);
extern int delegate_real_cplat_hashtable_find_value_ref(const cplat_hashtable *ht, const void *key,
                                                        const void **value_out);
extern int delegate_real_cplat_hashtable_find_value_copy(const cplat_hashtable *ht, const void *key, void *dest,
                                                         size_t dest_size, size_t *required_size_out);
extern int delegate_real_cplat_hashtable_find_recno(const cplat_hashtable *ht, const void *key, uint64_t *record_out);
extern int delegate_real_cplat_hashtable_get_key_ref(const cplat_hashtable *ht, uint64_t record, const void **key_out);
extern int delegate_real_cplat_hashtable_get_key_copy(const cplat_hashtable *ht, uint64_t record, void *dest,
                                                      size_t dest_size, size_t *required_size_out);
extern int delegate_real_cplat_hashtable_get_value_ref(const cplat_hashtable *ht, uint64_t record,
                                                       const void **value_out);
extern int delegate_real_cplat_hashtable_get_value_copy(const cplat_hashtable *ht, uint64_t record, void *dest,
                                                        size_t dest_size, size_t *required_size_out);
extern int delegate_real_cplat_hashtable_get_status(const cplat_hashtable *ht, uint64_t record, int *status_out);
extern int delegate_real_cplat_hashtable_next_record(const cplat_hashtable *ht, uint64_t from, unsigned int status_mask,
                                                     uint64_t *record_out, int *has_record_out);
extern int delegate_real_cplat_hashtable_get_timestamp_ref(const cplat_hashtable *ht, uint64_t record,
                                                           const cplat_timespec **timestamp_out);
extern int delegate_real_cplat_hashtable_get_timestamp_val(const cplat_hashtable *ht, uint64_t record,
                                                           cplat_timespec *timestamp_out);
extern int delegate_real_cplat_hashtable_get_generation(const cplat_hashtable *ht, uint64_t record,
                                                        uint64_t *generation_out);
extern int delegate_real_cplat_hashtable_get_table_timestamp_ref(const cplat_hashtable *ht,
                                                                 const cplat_timespec **timestamp_out);
extern int delegate_real_cplat_hashtable_get_table_timestamp_val(const cplat_hashtable *ht,
                                                                 cplat_timespec *timestamp_out);
extern int delegate_real_cplat_hashtable_get_table_generation(const cplat_hashtable *ht, uint64_t *generation_out);
extern int delegate_real_cplat_hashtable_find_timestamp_ref(const cplat_hashtable *ht, const void *key,
                                                            const cplat_timespec **timestamp_out);
extern int delegate_real_cplat_hashtable_find_timestamp_val(const cplat_hashtable *ht, const void *key,
                                                            cplat_timespec *timestamp_out);
extern int delegate_real_cplat_hashtable_find_generation(const cplat_hashtable *ht, const void *key,
                                                         uint64_t *generation_out);
extern int delegate_real_cplat_hashtable_count_status(const cplat_hashtable *ht, size_t *in_use_out,
                                                      size_t *deleted_out, size_t *empty_out);
extern int delegate_real_cplat_hashtable_count(const cplat_hashtable *ht, size_t *count_out);
extern int delegate_real_cplat_hashtable_deleted_count(const cplat_hashtable *ht, size_t *count_out);
extern int delegate_real_cplat_hashtable_empty_count(const cplat_hashtable *ht, size_t *count_out);
extern int delegate_real_cplat_hashtable_delete(cplat_hashtable *ht, const void *key);
extern int delegate_real_cplat_hashtable_delete_rec(cplat_hashtable *ht, uint64_t record);
extern int delegate_real_cplat_hashtable_push_deleted(cplat_hashtable *ht);
extern int delegate_real_cplat_hashtable_purge_deleted(cplat_hashtable *ht);
extern int delegate_real_cplat_hashtable_compact(cplat_hashtable *ht);
extern int delegate_real_cplat_hashtable_resize(cplat_hashtable *ht, const cplat_hashtable_config *new_config);
extern int delegate_real_cplat_hashtable_rebuild_into(const cplat_hashtable *src,
                                                      const cplat_hashtable_config *new_config, void *buf_mgmt,
                                                      size_t buf_mgmt_size, void *buf_data, size_t buf_data_size,
                                                      cplat_hashtable **ht_out);
extern int delegate_real_cplat_hashtable_clear(cplat_hashtable *ht);
extern void delegate_real_cplat_hashtable_dispose(cplat_hashtable *ht);

// cplat/clock/clock.h
extern uint64_t delegate_real_cplat_clock_get_monotonic_ms(void);
extern void delegate_real_cplat_clock_get_monotonic(cplat_timespec *ts);
extern void delegate_real_cplat_clock_get_realtime(cplat_timespec *ts);
extern int delegate_real_cplat_clock_format_realtime_iso8601_local(char *buf, size_t buf_size,
                                                                   const cplat_timespec *timestamp);
extern int delegate_real_cplat_clock_format_realtime_iso8601_utc(char *buf, size_t buf_size,
                                                                 const cplat_timespec *timestamp);
extern void delegate_real_cplat_clock_get_realtime_utc(struct tm *utc_tm, int32_t *tv_nsec);
extern void delegate_real_cplat_clock_get_realtime_deadline_ms(uint64_t timeout_ms, struct timespec *abs_timeout);

// cplat/clock/timespec.h
extern void delegate_real_cplat_timespec_normalize(cplat_timespec *ts);
extern void delegate_real_cplat_timespec_add(const cplat_timespec *a, const cplat_timespec *b, cplat_timespec *result);
extern void delegate_real_cplat_timespec_sub(const cplat_timespec *a, const cplat_timespec *b, cplat_timespec *result);
extern int delegate_real_cplat_timespec_cmp(const cplat_timespec *a, const cplat_timespec *b);
extern void delegate_real_cplat_timespec_add_ms(const cplat_timespec *ts, uint64_t timeout_ms, cplat_timespec *result);
extern int64_t delegate_real_cplat_timespec_diff_ms(const cplat_timespec *end, const cplat_timespec *start);
extern void delegate_real_cplat_timespec_to_native(const cplat_timespec *ts, struct timespec *native);
extern void delegate_real_cplat_timespec_from_native(const struct timespec *native, cplat_timespec *ts);

// cplat/compress/compress.h
extern int delegate_real_cplat_compress(uint8_t *dst, size_t *dst_len, const uint8_t *src, size_t src_len);
extern int delegate_real_cplat_decompress(uint8_t *dst, size_t *dst_len, const uint8_t *src, size_t src_len);

// cplat/console/console.h
extern void delegate_real_cplat_console_init(void);
extern void delegate_real_cplat_console_dispose(void);
extern int delegate_real_cplat_console_attach_parent(int *argc, char **argv, int *attached_out);
extern int delegate_real_cplat_console_write(cplat_stream stream, const char *text);

// cplat/console/console.h (internal)
extern void delegate_real_cplat_console_dispose_on_shutdown(const cplat_shutdown_event *event, void *context);

// cplat/crt/fcntl.h
extern int delegate_real_cplat_open(const char *path, int flags, int mode, cplat_error *detail_out);
extern int delegate_real_cplat_open_fmt(int flags, int mode, cplat_error *detail_out, const char *format, ...);
extern int delegate_real_cplat_vopen_fmt(int flags, int mode, cplat_error *detail_out, const char *format,
                                         va_list args);

// cplat/crt/file.h
extern void delegate_real_cplat_file_init(cplat_file *file);
extern int delegate_real_cplat_file_open(cplat_file *file, const char *path, int flags, cplat_error *detail_out);
extern int delegate_real_cplat_file_write(cplat_file *file, const void *buf, size_t len, cplat_error *detail_out);
extern int delegate_real_cplat_file_read(cplat_file *file, void *buf, size_t len, size_t *read_out,
                                         cplat_error *detail_out);
extern int delegate_real_cplat_file_get_size(const cplat_file *file, size_t *size_out, cplat_error *detail_out);
extern int delegate_real_cplat_file_set_size(cplat_file *file, size_t size, cplat_error *detail_out);
extern int delegate_real_cplat_file_get_id(const cplat_file *file, cplat_file_id *id_out, cplat_error *detail_out);
extern int delegate_real_cplat_file_get_path_id(const char *path, cplat_file_id *id_out, cplat_error *detail_out);
extern int delegate_real_cplat_file_get_modified_timestamp(const cplat_file *file, cplat_timespec *timestamp_out,
                                                           cplat_error *detail_out);
extern int delegate_real_cplat_file_set_modified_timestamp(cplat_file *file, const cplat_timespec *timestamp,
                                                           cplat_error *detail_out);
extern int delegate_real_cplat_file_get_path_modified_timestamp(const char *path, cplat_timespec *timestamp_out,
                                                                cplat_error *detail_out);
extern int delegate_real_cplat_file_set_path_modified_timestamp(const char *path, const cplat_timespec *timestamp,
                                                                cplat_error *detail_out);
extern int delegate_real_cplat_file_flush(cplat_file *file, cplat_error *detail_out);
extern int delegate_real_cplat_file_close(cplat_file *file, cplat_error *detail_out);

// cplat/crt/path.h
extern char *delegate_real_cplat_path_normalize_sep(char *path);
extern int delegate_real_cplat_path_get_full(char *path_out, size_t path_size, cplat_error *detail_out,
                                             const char *path);
extern int delegate_real_cplat_path_equal(const char *lhs, const char *rhs, int *equal_out, cplat_error *detail_out);
extern int delegate_real_cplat_path_get_temp_dir(char *path_out, size_t path_size, cplat_error *detail_out);
extern int delegate_real_cplat_path_concat_n(char *path_out, size_t path_size, cplat_error *detail_out,
                                             size_t part_count, ...);
extern int delegate_real_cplat_vpath_concat_n(char *path_out, size_t path_size, cplat_error *detail_out,
                                              size_t part_count, va_list args);
extern const char *delegate_real_cplat_path_basename(const char *path);
extern int delegate_real_cplat_path_dirname(char *path_out, size_t path_size, cplat_error *detail_out,
                                            const char *path);
extern const char *delegate_real_cplat_path_extension(const char *path);
extern int delegate_real_cplat_path_strip_extension(char *path_out, size_t path_size, cplat_error *detail_out,
                                                    const char *path);
extern int delegate_real_cplat_path_join_n(char *path_out, size_t path_size, cplat_error *detail_out, size_t part_count,
                                           ...);
extern int delegate_real_cplat_vpath_join_n(char *path_out, size_t path_size, cplat_error *detail_out,
                                            size_t part_count, va_list args);

// cplat/crt/stdio.h
extern FILE *delegate_real_cplat_fopen(const char *path, const char *modes, cplat_error *detail_out);
extern FILE *delegate_real_cplat_freopen(const char *path, const char *modes, FILE *stream, cplat_error *detail_out);
extern int delegate_real_cplat_fclose(FILE *stream, cplat_error *detail_out);
extern int delegate_real_cplat_fflush(FILE *stream, cplat_error *detail_out);
extern size_t delegate_real_cplat_fread(void *buffer, size_t size, size_t count, FILE *stream, cplat_error *detail_out);
extern size_t delegate_real_cplat_fwrite(const void *buffer, size_t size, size_t count, FILE *stream,
                                         cplat_error *detail_out);
extern int delegate_real_cplat_remove(const char *path, cplat_error *detail_out);
extern int delegate_real_cplat_rename(const char *oldpath, const char *newpath, cplat_error *detail_out);
extern int delegate_real_cplat_scanf(const char *format, va_list args);
extern int delegate_real_cplat_vscanf(const char *format, va_list args);
extern int delegate_real_cplat_fscanf(FILE *stream, const char *format, va_list args);
extern int delegate_real_cplat_vfscanf(FILE *stream, const char *format, va_list args);
extern int delegate_real_cplat_snprintf(char *dest, size_t dest_size, const char *format, ...);
extern int delegate_real_cplat_vsnprintf(char *dest, size_t dest_size, const char *format, va_list args);
extern int delegate_real_cplat_fgets(char *dest, size_t dest_size, FILE *stream, cplat_error *detail_out);
extern int delegate_real_cplat_fprintf(FILE *stream, const char *format, ...);
extern int delegate_real_cplat_vfprintf(FILE *stream, const char *format, va_list args);
extern int delegate_real_cplat_fseek(FILE *stream, int64_t offset, int whence);
extern int64_t delegate_real_cplat_ftell(FILE *stream);
extern FILE *delegate_real_cplat_fopen_fmt(const char *modes, cplat_error *detail_out, const char *format, ...);
extern FILE *delegate_real_cplat_vfopen_fmt(const char *modes, cplat_error *detail_out, const char *format,
                                            va_list args);
extern int delegate_real_cplat_remove_fmt(cplat_error *detail_out, const char *format, ...);
extern int delegate_real_cplat_vremove_fmt(cplat_error *detail_out, const char *format, va_list args);
extern FILE *delegate_real_cplat_fopen_temp(const char *prefix, const char *modes, char *path_out, size_t path_size,
                                            cplat_error *detail_out);

// cplat/crt/stdlib.h
extern int delegate_real_cplat_getenv(const char *name, char *buf, size_t buf_size, int *exists_out,
                                      cplat_error *detail_out);
extern int delegate_real_cplat_setenv(const char *name, const char *value, int overwrite, cplat_error *detail_out);
extern int delegate_real_cplat_unsetenv(const char *name, cplat_error *detail_out);
extern int delegate_real_cplat_parse_int64(int64_t *value_out, const char *text, int base);
extern int delegate_real_cplat_parse_uint64(uint64_t *value_out, const char *text, int base);
extern int delegate_real_cplat_parse_int(int *value_out, const char *text, int base);
extern int delegate_real_cplat_parse_double(double *value_out, const char *text);
extern void *delegate_real_cplat_malloc(size_t size);
extern void *delegate_real_cplat_malloc_zerofill(size_t size);
extern void *delegate_real_cplat_calloc(size_t count, size_t size);
extern void *delegate_real_cplat_realloc(void *ptr, size_t count, size_t size);
extern void *delegate_real_cplat_realloc_zerofill(void *ptr, size_t old_count, size_t count, size_t size);
extern void delegate_real_cplat_free(void *ptr);

// cplat/crt/string.h
extern int delegate_real_cplat_strcpy(char *dest, size_t dest_size, const char *src);
extern int delegate_real_cplat_strncpy(char *dest, size_t dest_size, const char *src, size_t count);
extern int delegate_real_cplat_strcat(char *dest, size_t dest_size, const char *src);
extern int delegate_real_cplat_strncat(char *dest, size_t dest_size, const char *src, size_t count);
extern char *delegate_real_cplat_strtok_r(char *str, const char *delim, char **saveptr);
extern char *delegate_real_cplat_strdup(const char *src);
extern int delegate_real_cplat_strcasecmp(const char *lhs, const char *rhs);
extern int delegate_real_cplat_strncasecmp(const char *lhs, const char *rhs, size_t count);
extern int delegate_real_cplat_wcscpy(wchar_t *dest, size_t dest_size, const wchar_t *src);
extern int delegate_real_cplat_sscanf(const char *buffer, const char *format, va_list args);
extern int delegate_real_cplat_vsscanf(const char *buffer, const char *format, va_list args);

// cplat/crt/sys/stat.h
extern int delegate_real_cplat_stat(cplat_file_stat_t *buf, cplat_error *detail_out, const char *path);
extern int delegate_real_cplat_mkdir(const char *path, cplat_error *detail_out);
extern int delegate_real_cplat_makedirs(const char *path, cplat_error *detail_out);
extern int delegate_real_cplat_rmdir(const char *path, cplat_error *detail_out);
extern int delegate_real_cplat_stat_fmt(cplat_file_stat_t *buf, cplat_error *detail_out, const char *format, ...);
extern int delegate_real_cplat_vstat_fmt(cplat_file_stat_t *buf, cplat_error *detail_out, const char *format,
                                         va_list args);
extern int delegate_real_cplat_mkdir_fmt(cplat_error *detail_out, const char *format, ...);
extern int delegate_real_cplat_vmkdir_fmt(cplat_error *detail_out, const char *format, va_list args);
extern int delegate_real_cplat_file_stat_is_regular(const cplat_file_stat_t *file_stat);

// cplat/crt/time.h
extern int delegate_real_cplat_gmtime(struct tm *utc_tm, const time_t *timep);
extern int delegate_real_cplat_localtime(struct tm *local_tm, const time_t *timep);
extern int delegate_real_cplat_ctime(char *buf, size_t buf_size, const time_t *timep);

// cplat/crt/unistd.h
extern int delegate_real_cplat_isatty(cplat_stream stream);
extern int64_t delegate_real_cplat_lseek(int fd, int64_t offset, int whence, cplat_error *detail_out);
extern int delegate_real_cplat_close(int fd, cplat_error *detail_out);
extern int delegate_real_cplat_dup(int fd, cplat_error *detail_out);
extern int delegate_real_cplat_dup2(int oldfd, int newfd, cplat_error *detail_out);
extern int64_t delegate_real_cplat_read(int fd, void *buf, size_t count, cplat_error *detail_out);
extern int64_t delegate_real_cplat_write(int fd, const void *buf, size_t count, cplat_error *detail_out);
extern int delegate_real_cplat_access(const char *path, int mode, cplat_error *detail_out);
extern int delegate_real_cplat_access_fmt(int mode, cplat_error *detail_out, const char *format, ...);
extern int delegate_real_cplat_vaccess_fmt(int mode, cplat_error *detail_out, const char *format, va_list args);

// cplat/crypto/crypto.h
extern int delegate_real_cplat_crypto_encrypt(uint8_t *dst, size_t *dst_len, const uint8_t *src, size_t src_len,
                                              const uint8_t *key, const uint8_t *nonce, const uint8_t *aad,
                                              size_t aad_len);
extern int delegate_real_cplat_crypto_decrypt(uint8_t *dst, size_t *dst_len, const uint8_t *src, size_t src_len,
                                              const uint8_t *key, const uint8_t *nonce, const uint8_t *aad,
                                              size_t aad_len);
extern int delegate_real_cplat_passphrase_to_key(uint8_t *key, const uint8_t *passphrase, size_t passphrase_len);

// cplat/base/error.h
extern void delegate_real_cplat_error_clear(cplat_error *error);
extern void delegate_real_cplat_error_capture_errno(cplat_error *error, int errno_value);
extern void delegate_real_cplat_error_capture_current_errno(cplat_error *error);
extern void delegate_real_cplat_error_get_last(cplat_error *error_out);
extern void delegate_real_cplat_error_set_last(const cplat_error *error);
extern void delegate_real_cplat_error_clear_last(void);
extern int delegate_real_cplat_error_is_set(const cplat_error *error);
extern cplat_error_domain delegate_real_cplat_error_get_domain(const cplat_error *error);
extern int delegate_real_cplat_error_get_errno(const cplat_error *error);
extern int delegate_real_cplat_error_to_result(const cplat_error *error);
extern cplat_error_cause delegate_real_cplat_error_get_cause(const cplat_error *error);
extern int delegate_real_cplat_error_is(const cplat_error *error, cplat_error_cause cause);

// cplat/base/error_message.h
extern const char *delegate_real_cplat_result_to_string(int result);
extern int delegate_real_cplat_error_message(char *buf, size_t buf_size, const cplat_error *error);

// cplat/crypto/random.h
extern int delegate_real_cplat_random_bytes(void *buf, size_t size);

// cplat/locale/ui_language.h
extern int delegate_real_cplat_ui_language_get_tag(char *tag_out, size_t tag_size);

// cplat/mmap/mmap.h
extern int delegate_real_cplat_mmap_attach(const char *path, cplat_mmap_access access, size_t create_size,
                                           cplat_mmap **map, cplat_error *detail_out);
extern void *delegate_real_cplat_mmap_get_address(const cplat_mmap *map);
extern size_t delegate_real_cplat_mmap_get_size(const cplat_mmap *map);
extern int delegate_real_cplat_mmap_flush(cplat_mmap *map, void *address, size_t length, cplat_error *detail_out);
extern int delegate_real_cplat_mmap_detach(cplat_mmap *map, cplat_error *detail_out);

// cplat/net/byteorder.h
extern uint16_t delegate_real_cplat_hton16(uint16_t value);
extern uint16_t delegate_real_cplat_ntoh16(uint16_t value);
extern uint32_t delegate_real_cplat_hton32(uint32_t value);
extern uint32_t delegate_real_cplat_ntoh32(uint32_t value);
extern uint64_t delegate_real_cplat_hton64(uint64_t value);
extern uint64_t delegate_real_cplat_ntoh64(uint64_t value);

// cplat/net/endpoint.h
extern int delegate_real_cplat_ipv4_parse(const char *text, uint32_t *address_out);
extern int delegate_real_cplat_ipv4_resolve(const char *text, uint32_t *address_out, cplat_error *detail_out);
extern int delegate_real_cplat_ipv4_to_string(uint32_t address, char *buffer, size_t buffer_size,
                                              cplat_error *detail_out);

// cplat/net/socket.h
extern int delegate_real_cplat_socket_open(cplat_socket_kind kind, cplat_socket *sock_out, cplat_error *detail_out);
extern void delegate_real_cplat_socket_close(cplat_socket sock);
extern void delegate_real_cplat_socket_shutdown(cplat_socket sock);
extern int delegate_real_cplat_socket_bind(cplat_socket sock, const cplat_ipv4_endpoint *endpoint,
                                           cplat_error *detail_out);
extern int delegate_real_cplat_socket_listen(cplat_socket sock, int backlog, cplat_error *detail_out);
extern int delegate_real_cplat_socket_accept(cplat_socket sock, cplat_ipv4_endpoint *peer_out, cplat_socket *sock_out,
                                             cplat_error *detail_out);
extern int delegate_real_cplat_socket_connect(cplat_socket sock, const cplat_ipv4_endpoint *endpoint,
                                              cplat_error *detail_out);
extern int delegate_real_cplat_socket_get_pending_error(cplat_socket sock, cplat_error *detail_out);
extern int delegate_real_cplat_socket_set_nonblocking(cplat_socket sock, int enable, cplat_error *detail_out);
extern int delegate_real_cplat_socket_set_reuse_address(cplat_socket sock, int enable, cplat_error *detail_out);
extern int delegate_real_cplat_socket_set_broadcast(cplat_socket sock, int enable, cplat_error *detail_out);
extern int delegate_real_cplat_socket_set_multicast_interface(cplat_socket sock, uint32_t interface_address,
                                                              cplat_error *detail_out);
extern int delegate_real_cplat_socket_join_multicast_group(cplat_socket sock, uint32_t group_address,
                                                           uint32_t interface_address, cplat_error *detail_out);
extern int delegate_real_cplat_socket_leave_multicast_group(cplat_socket sock, uint32_t group_address,
                                                            uint32_t interface_address, cplat_error *detail_out);
extern int delegate_real_cplat_socket_send(cplat_socket sock, const void *buf, size_t len, size_t *sent_out,
                                           cplat_error *detail_out);
extern int delegate_real_cplat_socket_recv(cplat_socket sock, void *buf, size_t len, size_t *received_out,
                                           cplat_error *detail_out);
extern int delegate_real_cplat_socket_sendto(cplat_socket sock, const void *buf, size_t len,
                                             const cplat_ipv4_endpoint *endpoint, size_t *sent_out,
                                             cplat_error *detail_out);
extern int delegate_real_cplat_socket_recvfrom(cplat_socket sock, void *buf, size_t len, cplat_ipv4_endpoint *peer_out,
                                               size_t *received_out, cplat_error *detail_out);
extern int delegate_real_cplat_socket_send_all(cplat_socket sock, const void *buf, size_t len, cplat_error *detail_out);
extern int delegate_real_cplat_socket_recv_all(cplat_socket sock, void *buf, size_t len, cplat_error *detail_out);
extern int delegate_real_cplat_socket_wait_readable(cplat_socket sock, int timeout_ms, int *ready_out,
                                                    cplat_error *detail_out);
extern int delegate_real_cplat_socket_wait_writable(cplat_socket sock, int timeout_ms, int *ready_out,
                                                    cplat_error *detail_out);
extern int delegate_real_cplat_socket_wait_readable_multi(const cplat_socket *socks, size_t count, int timeout_ms,
                                                          unsigned char *ready_out, cplat_error *detail_out);
extern int delegate_real_cplat_socket_shutdown_receive(cplat_socket *sock_inout, cplat_error *detail_out);

// cplat/prompt/pinned_prompt.h
extern cplat_pinned_prompt *delegate_real_cplat_pinned_prompt_create(const cplat_pinned_prompt_options *options);
extern void delegate_real_cplat_pinned_prompt_dispose(cplat_pinned_prompt *screen);
extern int delegate_real_cplat_pinned_prompt_readline_at(cplat_pinned_prompt *screen, char *buf, size_t buf_size,
                                                         const char *prompt_str, const char *file, int line);
extern int delegate_real_cplat_pinned_prompt_readline_with_initial_at(cplat_pinned_prompt *screen, char *buf,
                                                                      size_t buf_size, const char *prompt_str,
                                                                      const char *initial_text, const char *file,
                                                                      int line);
extern int delegate_real_cplat_pinned_prompt_readline_fmt_at(cplat_pinned_prompt *screen, char *buf, size_t buf_size,
                                                             const char *file, int line, const char *fmt, va_list args);
extern int delegate_real_cplat_pinned_prompt_write(cplat_pinned_prompt *screen, cplat_pinned_prompt_channel channel,
                                                   const void *data, size_t size, size_t *written_out);
extern int delegate_real_cplat_pinned_prompt_printf(cplat_pinned_prompt *screen, cplat_pinned_prompt_channel channel,
                                                    const char *fmt, ...);
extern int delegate_real_cplat_pinned_prompt_status_enable(cplat_pinned_prompt *screen,
                                                           cplat_pinned_prompt_status_position position, int enable);
extern int delegate_real_cplat_pinned_prompt_status_set(cplat_pinned_prompt *screen,
                                                        cplat_pinned_prompt_status_position position,
                                                        cplat_pinned_prompt_status_align align, const char *content);

// cplat/prompt/prompt.h
extern cplat_prompt *delegate_real_cplat_prompt_create(const cplat_prompt_options *options);
extern void delegate_real_cplat_prompt_dispose(cplat_prompt *prompt);
extern int delegate_real_cplat_prompt_readline_at(cplat_prompt *prompt, char *buf, size_t buf_size,
                                                  const char *prompt_str, const char *file, int line);
extern int delegate_real_cplat_prompt_readline_with_initial_at(cplat_prompt *prompt, char *buf, size_t buf_size,
                                                               const char *prompt_str, const char *initial_text,
                                                               const char *file, int line);
extern int delegate_real_cplat_prompt_readline_fmt_at(cplat_prompt *p, char *buf, size_t buf_size, const char *file,
                                                      int line, const char *fmt, va_list args);

// cplat/runtime/elevated_process.h
extern int delegate_real_cplat_elevated_process_is_elevated(int *elevated);
extern int delegate_real_cplat_elevated_process_run_if_needed(const char *arguments, int *exit_code, int *handled);
extern int delegate_real_cplat_elevated_process_run_with_result(const char *arguments, int *exit_code, int *handled,
                                                                char *result_message, size_t result_message_size);
extern int delegate_real_cplat_elevated_process_extract_result_target(int *argc, char **argv, int *detected_out);
extern int delegate_real_cplat_elevated_process_report_result(const char *message);
extern int delegate_real_cplat_elevated_process_run_piped(const char *arguments,
                                                          cplat_elevated_process_output_fn output_fn, void *context,
                                                          int *exit_code, int *handled);
extern int delegate_real_cplat_elevated_process_attach_output_pipes(int *argc, char **argv, int *attached_out);

// cplat/regex/regex.h
extern int delegate_real_cplat_regex_create(const char *pattern, unsigned int flags, cplat_regex **regex_out,
                                            cplat_error *detail_out);
extern void delegate_real_cplat_regex_dispose(cplat_regex *regex);
extern size_t delegate_real_cplat_regex_get_group_count(const cplat_regex *regex);
extern int delegate_real_cplat_regex_search(const cplat_regex *regex, const char *text, size_t text_len,
                                            size_t start_offset, unsigned int match_flags,
                                            cplat_regex_match *matches_out, size_t matches_capacity, int *matched_out,
                                            cplat_error *detail_out);
extern int delegate_real_cplat_regex_matches(const cplat_regex *regex, const char *text, size_t text_len,
                                             unsigned int match_flags, cplat_regex_match *matches_out,
                                             size_t matches_capacity, int *matched_out, cplat_error *detail_out);
extern int delegate_real_cplat_regex_replace(const cplat_regex *regex, const char *text, size_t text_len,
                                             const char *replacement, unsigned int flags, char *result_out,
                                             size_t result_size, size_t *required_size_out, cplat_error *detail_out);
extern int delegate_real_cplat_regex_iter_create(const cplat_regex *regex, const char *text, size_t text_len,
                                                 unsigned int match_flags, cplat_regex_iter **iter_out,
                                                 cplat_error *detail_out);
extern int delegate_real_cplat_regex_iter_next(cplat_regex_iter *iter, cplat_regex_match *matches_out,
                                               size_t matches_capacity, int *has_match_out, cplat_error *detail_out);
extern void delegate_real_cplat_regex_iter_dispose(cplat_regex_iter *iter);
extern int delegate_real_cplat_regex_split(const cplat_regex *regex, const char *text, size_t text_len,
                                           size_t max_parts, unsigned int match_flags, cplat_regex_match *parts_out,
                                           size_t parts_capacity, size_t *part_count_out, cplat_error *detail_out);

// cplat/runtime/memory_lock.h
extern int delegate_real_cplat_memory_lock_range(const void *address, size_t size);
extern int delegate_real_cplat_memory_unlock_range(const void *address, size_t size);
extern int delegate_real_cplat_memory_lock_self(const cplat_memory_lock_self_options *options,
                                                cplat_memory_lock_scope **scope);
extern int delegate_real_cplat_memory_lock_scope_release(cplat_memory_lock_scope *scope);
extern void delegate_real_cplat_secure_zero(void *buf, size_t size);

// cplat/runtime/host.h
extern int delegate_real_cplat_host_get_name(char *name_out, size_t name_size);

// cplat/runtime/module.h
extern int delegate_real_cplat_module_get_path(char *path_out, size_t path_size, const void *func_addr);
extern int delegate_real_cplat_module_get_basename(char *basename_out, size_t basename_size, const void *func_addr);

// cplat/runtime/process.h
extern int delegate_real_cplat_process_get_executable_path(char *path_out, size_t path_size);
extern uint32_t delegate_real_cplat_process_get_pid(void);
extern uint32_t delegate_real_cplat_process_get_tid(void);
extern int delegate_real_cplat_process_start(const cplat_process_options *options, cplat_process **process);
extern int delegate_real_cplat_process_wait(cplat_process *process, int timeout_ms);
extern int delegate_real_cplat_process_get_exit_code(cplat_process *process, int *exit_code);
extern int delegate_real_cplat_process_terminate(cplat_process *process);
extern void delegate_real_cplat_process_dispose(cplat_process *process);
extern int delegate_real_cplat_process_run_sync(const cplat_process_options *options, int timeout_ms, int *exit_code);

// cplat/runtime/shutdown.h
extern int delegate_real_cplat_shutdown_register(cplat_shutdown_fn callback, void *context);
extern int delegate_real_cplat_shutdown_request_register(cplat_shutdown_fn callback, void *context);
extern void delegate_real_cplat_exit(int code);
extern int delegate_real_cplat_shutdown_invoke_for_test(const cplat_shutdown_event *event, int *invoked_out);
extern int delegate_real_cplat_shutdown_request_invoke_for_test(const cplat_shutdown_event *event, int *invoked_out);
extern void delegate_real_cplat_shutdown_reset_for_test(void);

// cplat/runtime/sym_loader.h
extern void *delegate_real_cplat_sym_loader_resolve(cplat_sym_loader_entry *fobj);
extern int delegate_real_cplat_sym_loader_is_default(cplat_sym_loader_entry *fobj);
extern void delegate_real_cplat_sym_loader_init(cplat_sym_loader_entry *const *fobj_array, size_t fobj_length,
                                                const char *configpath);
extern void delegate_real_cplat_sym_loader_dispose(cplat_sym_loader_entry *const *fobj_array, size_t fobj_length);
extern int delegate_real_cplat_sym_loader_info(cplat_sym_loader_entry *const *fobj_array, size_t fobj_length);

// cplat/string_catalog/string_catalog.h
extern int delegate_real_cplat_string_catalog_set_language(cplat_string_catalog_language language);
extern cplat_string_catalog_language delegate_real_cplat_string_catalog_get_language(void);
extern int delegate_real_cplat_string_catalog_language_from_tag(const char *tag,
                                                                cplat_string_catalog_language *language_out);
extern int delegate_real_cplat_string_catalog_format(const cplat_string_catalog *catalog, char *dest, size_t dest_size,
                                                     int string_key, ...);
extern int delegate_real_cplat_string_catalog_vformat(const cplat_string_catalog *catalog, char *dest, size_t dest_size,
                                                      int string_key, va_list args);
extern int delegate_real_cplat_string_catalog_verify(const cplat_string_catalog *catalog, int *string_key_out,
                                                     cplat_string_catalog_language *language_out);
extern const cplat_string_catalog_entry *
delegate_real_cplat_string_catalog_get_entry(const cplat_string_catalog *catalog, int string_key);
extern int delegate_real_cplat_string_catalog_get_category(const cplat_string_catalog *catalog, int string_key);
extern const char *delegate_real_cplat_string_catalog_get_id(const cplat_string_catalog *catalog, int string_key);
extern const char *delegate_real_cplat_string_catalog_get_note(const cplat_string_catalog *catalog, int string_key);

// cplat/string_catalog/filter.h
extern int delegate_real_cplat_string_catalog_filter_compile(const char *lines, size_t line_count, size_t line_width,
                                                             size_t line_capacity, void *image_out, size_t image_size,
                                                             cplat_string_catalog_filter_diagnostic *diagnostics,
                                                             size_t diagnostic_capacity, size_t *invalid_count_out);
extern int
delegate_real_cplat_string_catalog_filter_compile_line(void *image, size_t image_size, size_t line_index,
                                                       const char *text,
                                                       cplat_string_catalog_filter_diagnostic *diagnostic_out);
extern int
delegate_real_cplat_string_catalog_filter_insert_line(void *image, size_t image_size, size_t line_index,
                                                      const char *text,
                                                      cplat_string_catalog_filter_diagnostic *diagnostic_out);
extern int delegate_real_cplat_string_catalog_filter_remove_line(void *image, size_t image_size, size_t line_index);
extern int delegate_real_cplat_string_catalog_filter_validate(const void *image, size_t image_size);
extern int delegate_real_cplat_string_catalog_filter_get_info(const void *image, size_t image_size,
                                                              cplat_string_catalog_filter_info *info_out);
extern int delegate_real_cplat_string_catalog_filter_decompile_line(const void *image, size_t image_size,
                                                                    size_t line_index, char *dest, size_t dest_size);
extern int delegate_real_cplat_string_catalog_filter_slot_create(
    const cplat_string_catalog *catalog, const cplat_string_catalog_filter_key_name *key_names, size_t key_name_count,
    const cplat_string_catalog_filter_category_names *category_names, size_t line_capacity, size_t line_width,
    cplat_string_catalog_filter_slot **slot_out);
extern void delegate_real_cplat_string_catalog_filter_slot_dispose(cplat_string_catalog_filter_slot **slot);
extern const cplat_string_catalog *
delegate_real_cplat_string_catalog_filter_slot_get_catalog(const cplat_string_catalog_filter_slot *slot);
extern int delegate_real_cplat_string_catalog_filter_source_publish(void *source, size_t source_size, const void *image,
                                                                    size_t image_size, uint64_t catalog_id,
                                                                    const cplat_string_catalog_filter_source_lock *lock,
                                                                    uint64_t *revision_out);
extern int delegate_real_cplat_string_catalog_filter_get_catalog_id(const cplat_string_catalog *catalog,
                                                                    uint64_t *catalog_id_out);
extern int delegate_real_cplat_string_catalog_filter_source_get_info(const void *source, size_t source_size,
                                                                     cplat_string_catalog_filter_source_info *info_out);
extern int
delegate_real_cplat_string_catalog_filter_slot_attach_source(cplat_string_catalog_filter_slot *slot, const void *source,
                                                             size_t source_size,
                                                             const cplat_string_catalog_filter_source_lock *lock);
extern int
delegate_real_cplat_string_catalog_filter_slot_get_source_status(cplat_string_catalog_filter_slot *slot,
                                                                 cplat_string_catalog_filter_source_status *status_out);
extern int delegate_real_cplat_string_catalog_filter_slot_apply(cplat_string_catalog_filter_slot *slot,
                                                                const void *image, size_t image_size,
                                                                cplat_string_catalog_filter_diagnostic *diagnostics,
                                                                size_t diagnostic_capacity, size_t *invalid_count_out);
extern int delegate_real_cplat_string_catalog_filter_slot_check(cplat_string_catalog_filter_slot *slot,
                                                                const void *image, size_t image_size,
                                                                cplat_string_catalog_filter_diagnostic *diagnostics,
                                                                size_t diagnostic_capacity, size_t *invalid_count_out,
                                                                cplat_string_catalog_filter_warning *warnings,
                                                                size_t warning_capacity, size_t *warning_count_out);
extern int delegate_real_cplat_string_catalog_filter_slot_snapshot(cplat_string_catalog_filter_slot *slot,
                                                                   void *image_out, size_t image_size);
extern int
delegate_real_cplat_string_catalog_filter_slot_get_line_error(cplat_string_catalog_filter_slot *slot, size_t line_index,
                                                              cplat_string_catalog_filter_line_error *error_out);
extern int delegate_real_cplat_string_catalog_filter_slot_describe_line(cplat_string_catalog_filter_slot *slot,
                                                                        size_t line_index, char *dest,
                                                                        size_t dest_size);
extern int delegate_real_cplat_string_catalog_filter_slot_test(cplat_string_catalog_filter_slot *slot, int string_key,
                                                               cplat_string_catalog_filter_state *state_out);
extern int delegate_real_cplat_string_catalog_filter_slot_vformat(cplat_string_catalog_filter_slot *slot, char *dest,
                                                                  size_t dest_size, int *matched_out, int string_key,
                                                                  va_list args);
extern int delegate_real_cplat_string_catalog_filter_slot_format(cplat_string_catalog_filter_slot *slot, char *dest,
                                                                 size_t dest_size, int *matched_out, int string_key,
                                                                 ...);

// cplat/sync/sync.h
extern int delegate_real_cplat_local_lock_create(cplat_local_lock **mtx);
extern int delegate_real_cplat_local_lock_lock(cplat_local_lock *mtx, int timeout_ms);
extern int delegate_real_cplat_local_lock_try_lock(cplat_local_lock *mtx);
extern int delegate_real_cplat_local_lock_unlock(cplat_local_lock *mtx);
extern void delegate_real_cplat_local_lock_dispose(cplat_local_lock *mtx);
extern int delegate_real_cplat_condvar_create(cplat_condvar **cv);
extern int delegate_real_cplat_condvar_wait(cplat_condvar *cv, cplat_local_lock *mtx, int timeout_ms);
extern int delegate_real_cplat_condvar_signal(cplat_condvar *cv);
extern int delegate_real_cplat_condvar_broadcast(cplat_condvar *cv);
extern void delegate_real_cplat_condvar_dispose(cplat_condvar *cv);
extern int delegate_real_cplat_local_rwlock_create(cplat_local_rwlock **rwlock);
extern int delegate_real_cplat_local_rwlock_lock_shared(cplat_local_rwlock *rwlock, int timeout_ms);
extern int delegate_real_cplat_local_rwlock_try_lock_shared(cplat_local_rwlock *rwlock);
extern int delegate_real_cplat_local_rwlock_lock_exclusive(cplat_local_rwlock *rwlock, int timeout_ms);
extern int delegate_real_cplat_local_rwlock_try_lock_exclusive(cplat_local_rwlock *rwlock);
extern int delegate_real_cplat_local_rwlock_unlock_shared(cplat_local_rwlock *rwlock);
extern int delegate_real_cplat_local_rwlock_unlock_exclusive(cplat_local_rwlock *rwlock);
extern void delegate_real_cplat_local_rwlock_dispose(cplat_local_rwlock *rwlock);
extern int delegate_real_cplat_thread_create(cplat_thread **thread, cplat_thread_fn func, void *arg);
extern int delegate_real_cplat_thread_join(cplat_thread *thread, int timeout_ms);
extern void delegate_real_cplat_thread_detach(cplat_thread *thread);
extern int delegate_real_cplat_interprocess_lock_open(const char *identity, cplat_interprocess_lock **lock);
extern int delegate_real_cplat_interprocess_lock_import_descriptor(const void *descriptor, size_t descriptor_size,
                                                                   cplat_interprocess_lock **lock);
extern int delegate_real_cplat_interprocess_lock_export_descriptor(const cplat_interprocess_lock *lock,
                                                                   void *descriptor, size_t *descriptor_size);
extern int delegate_real_cplat_interprocess_lock_lock(cplat_interprocess_lock *lock, int timeout_ms);
extern int delegate_real_cplat_interprocess_lock_try_lock(cplat_interprocess_lock *lock);
extern int delegate_real_cplat_interprocess_lock_unlock(cplat_interprocess_lock *lock);
extern void delegate_real_cplat_interprocess_lock_dispose(cplat_interprocess_lock *lock);
extern int delegate_real_cplat_interprocess_rwlock_open(const char *identity, cplat_interprocess_rwlock **lock);
extern int delegate_real_cplat_interprocess_rwlock_import_descriptor(const void *descriptor, size_t descriptor_size,
                                                                     cplat_interprocess_rwlock **lock);
extern int delegate_real_cplat_interprocess_rwlock_export_descriptor(const cplat_interprocess_rwlock *lock,
                                                                     void *descriptor, size_t *descriptor_size);
extern int delegate_real_cplat_interprocess_rwlock_lock_shared(cplat_interprocess_rwlock *lock, int timeout_ms);
extern int delegate_real_cplat_interprocess_rwlock_try_lock_shared(cplat_interprocess_rwlock *lock);
extern int delegate_real_cplat_interprocess_rwlock_lock_exclusive(cplat_interprocess_rwlock *lock, int timeout_ms);
extern int delegate_real_cplat_interprocess_rwlock_try_lock_exclusive(cplat_interprocess_rwlock *lock);
extern int delegate_real_cplat_interprocess_rwlock_unlock(cplat_interprocess_rwlock *lock);
extern void delegate_real_cplat_interprocess_rwlock_dispose(cplat_interprocess_rwlock *lock);
extern void delegate_real_cplat_call_once(cplat_once_flag *flag, cplat_once_fn func);
extern void delegate_real_cplat_sleep_ms(int ms);

// cplat/trace/trace_file.h
extern cplat_trace_file_sink *delegate_real_cplat_trace_file_sink_create(const char *path, size_t max_bytes,
                                                                         int generations, int flags);
extern int delegate_real_cplat_trace_file_sink_write(cplat_trace_file_sink *handle, int level,
                                                     const cplat_timespec *timestamp, const char *message);
extern void delegate_real_cplat_trace_file_sink_dispose(cplat_trace_file_sink *handle);

// cplat/trace/tracer.h
extern cplat_tracer *delegate_real_cplat_tracer_create(cplat_tracer_concurrency_mode);
extern int delegate_real_cplat_tracer_start(cplat_tracer *handle);
extern int delegate_real_cplat_tracer_stop(cplat_tracer *handle);
extern cplat_tracer_state delegate_real_cplat_tracer_get_state(cplat_tracer *handle);
extern int delegate_real_cplat_tracer_write_at(cplat_tracer *handle, cplat_trace_level level,
                                               const cplat_timespec *timestamp, const char *message);
extern int delegate_real_cplat_tracer_writef_at(cplat_tracer *handle, cplat_trace_level level,
                                                const cplat_timespec *timestamp, const char *format, ...);
extern int delegate_real_cplat_tracer_vwritef_at(cplat_tracer *handle, cplat_trace_level level,
                                                 const cplat_timespec *timestamp, const char *format, va_list args);
extern int delegate_real_cplat_tracer_write_hex_at(cplat_tracer *handle, cplat_trace_level level,
                                                   const cplat_timespec *timestamp, const void *data, size_t size,
                                                   const char *message);
extern int delegate_real_cplat_tracer_write_hexf_at(cplat_tracer *handle, cplat_trace_level level,
                                                    const cplat_timespec *timestamp, const void *data, size_t size,
                                                    const char *format, ...);
extern int delegate_real_cplat_tracer_vwrite_hexf_at(cplat_tracer *handle, cplat_trace_level level,
                                                     const cplat_timespec *timestamp, const void *data, size_t size,
                                                     const char *format, va_list args);
extern const char *delegate_real_cplat_tracer_hex_sep(const char *message);
extern const char *delegate_real_cplat_tracer_hex_msg(const char *message);
extern int delegate_real_cplat_tracer_write_with_source(cplat_tracer *handle, cplat_trace_level level,
                                                        const cplat_timespec *timestamp, const char *file, int line,
                                                        const char *message);
extern int delegate_real_cplat_tracer_set_name(cplat_tracer *handle, const char *name, int64_t identifier);
extern int delegate_real_cplat_tracer_get_name(cplat_tracer *handle, char *name_out, size_t name_size);
extern int64_t delegate_real_cplat_tracer_get_identifier(cplat_tracer *handle);
extern int delegate_real_cplat_tracer_set_file_name(cplat_tracer *handle, const char *name, int64_t identifier);
extern int delegate_real_cplat_tracer_get_file_name(cplat_tracer *handle, char *file_name_out, size_t file_name_size);
extern int64_t delegate_real_cplat_tracer_get_file_identifier(cplat_tracer *handle);
extern cplat_trace_level delegate_real_cplat_tracer_get_os_level(cplat_tracer *handle);
extern int delegate_real_cplat_tracer_set_os_level(cplat_tracer *handle, cplat_trace_level level);
extern cplat_trace_level delegate_real_cplat_tracer_get_etw_level(cplat_tracer *handle);
extern int delegate_real_cplat_tracer_set_etw_level(cplat_tracer *handle, cplat_trace_level level);
extern cplat_trace_level delegate_real_cplat_tracer_get_file_level(cplat_tracer *handle);
extern int delegate_real_cplat_tracer_set_file_level(cplat_tracer *handle, const char *path, cplat_trace_level level,
                                                     size_t max_bytes, int generations, int flags);
extern cplat_trace_level delegate_real_cplat_tracer_get_stderr_level(cplat_tracer *handle);
extern int delegate_real_cplat_tracer_set_stderr_level(cplat_tracer *handle, cplat_trace_level level);
extern void delegate_real_cplat_tracer_dispose(cplat_tracer **handle);
extern cplat_tracer_hook_entry *delegate_real_cplat_tracer_set_hook(cplat_tracer *handle, cplat_tracer_hook_fn fn,
                                                                    void *context);
extern void delegate_real_cplat_tracer_remove_hook(cplat_tracer *handle, cplat_tracer_hook_entry *hook_entry);
extern void delegate_real_cplat_tracer_call_next_hook(cplat_tracer_hook_entry *prev, cplat_tracer *handle,
                                                      cplat_trace_level level, const cplat_timespec *timestamp,
                                                      const char *message);

#if defined(PLATFORM_WINDOWS)

// cplat/crt/wchar_conv.h
extern int delegate_real_cplat_utf8_to_wpath(wchar_t *wbuf, size_t wbuf_count, const char *utf8_path);
extern int delegate_real_cplat_utf8_to_wstr(wchar_t *wbuf, size_t wbuf_count, const char *utf8_text);
extern int delegate_real_cplat_wpath_to_utf8(char *dest, size_t dest_size, const wchar_t *wpath);
extern int delegate_real_cplat_wstr_to_utf8(char *dest, size_t dest_size, const wchar_t *wtext);
extern wchar_t *delegate_real_cplat_utf8_to_wstr_alloc(const char *utf8_text);
extern char *delegate_real_cplat_wstr_to_utf8_alloc(const wchar_t *wtext);

// cplat/base/error.h
extern void delegate_real_cplat_error_capture_windows_error(cplat_error *error, unsigned long error_code);
extern void delegate_real_cplat_error_capture_current_windows_error(cplat_error *error);
extern unsigned long delegate_real_cplat_error_get_windows_error(const cplat_error *error);

// cplat/trace/etw.h
extern cplat_etw_provider *delegate_real_cplat_etw_provider_create(cplat_etw_provider_ref_t provider_ref);
extern int delegate_real_cplat_etw_provider_write(cplat_etw_provider *handle, int level, const char *service,
                                                  const char *message);
extern void delegate_real_cplat_etw_provider_dispose(cplat_etw_provider *handle);
extern int delegate_real_cplat_etw_session_check_access(void);
extern int delegate_real_cplat_etw_session_start(const char *session_name, const char *provider_guid_str,
                                                 cplat_etw_event_fn callback, void *context,
                                                 cplat_etw_session **session_out);
extern void delegate_real_cplat_etw_session_stop(cplat_etw_session *session);

// cplat/trace/eventlog.h
extern cplat_eventlog_sink *delegate_real_cplat_eventlog_sink_create(const char *source_name);
extern int delegate_real_cplat_eventlog_sink_write(cplat_eventlog_sink *handle, int level, int64_t file_identifier,
                                                   const char *instance_name, int64_t instance_identifier,
                                                   const char *message);
extern void delegate_real_cplat_eventlog_sink_dispose(cplat_eventlog_sink *handle);
extern int delegate_real_cplat_eventlog_register_source(const char *source_name, const char *message_file_path);
extern int delegate_real_cplat_eventlog_unregister_source(const char *source_name);

// cplat/win32/win32.h
extern HANDLE delegate_real_CreateFileU(const char *utf8_path, DWORD desired_access, DWORD share_mode,
                                        LPSECURITY_ATTRIBUTES security_attributes, DWORD creation_disposition,
                                        DWORD flags_and_attributes, HANDLE template_file);
extern HANDLE delegate_real_CreateNamedPipeU(const char *utf8_name, DWORD open_mode, DWORD pipe_mode,
                                             DWORD max_instances, DWORD out_buffer_size, DWORD in_buffer_size,
                                             DWORD default_timeout, LPSECURITY_ATTRIBUTES security_attributes);
extern DWORD delegate_real_GetModuleFileNameU(HMODULE module, char *utf8_buf, DWORD size);
extern BOOL delegate_real_WriteConsoleU(HANDLE console, const char *utf8_text, DWORD utf8_length, DWORD *written_length,
                                        void *reserved);
extern BOOL delegate_real_GetVolumePathNameU(const char *utf8_path, char *utf8_volume_root, DWORD size);
extern BOOL delegate_real_GetVolumeInformationU(const char *utf8_root_path, char *utf8_volume_name,
                                                DWORD volume_name_size, DWORD *serial_number,
                                                DWORD *max_component_length, DWORD *file_system_flags,
                                                char *utf8_file_system_name, DWORD file_system_name_size);
extern HMODULE delegate_real_LoadLibraryU(const char *utf8_file_name);
extern BOOL delegate_real_CreateProcessU(const char *utf8_application_name, const char *utf8_command_line,
                                         LPSECURITY_ATTRIBUTES process_attributes,
                                         LPSECURITY_ATTRIBUTES thread_attributes, BOOL inherit_handles,
                                         DWORD creation_flags, LPVOID environment, const char *utf8_current_directory,
                                         LPSTARTUPINFOW startup_info, LPPROCESS_INFORMATION process_information);
extern SC_HANDLE delegate_real_OpenSCManagerU(const char *utf8_machine_name, const char *utf8_database_name,
                                              DWORD desired_access);
extern SC_HANDLE delegate_real_CreateServiceU(SC_HANDLE scm, const char *utf8_service_name,
                                              const char *utf8_display_name, DWORD desired_access, DWORD service_type,
                                              DWORD start_type, DWORD error_control, const char *utf8_binary_path_name,
                                              const char *utf8_load_order_group, LPDWORD tag_id,
                                              const char *utf8_dependencies, const char *utf8_service_start_name,
                                              const char *utf8_password);
extern SC_HANDLE delegate_real_OpenServiceU(SC_HANDLE scm, const char *utf8_service_name, DWORD desired_access);
extern BOOL delegate_real_ChangeServiceConfig2U(SC_HANDLE service, DWORD info_level, const char *utf8_text);
extern SERVICE_STATUS_HANDLE delegate_real_RegisterServiceCtrlHandlerExU(const char *utf8_service_name,
                                                                         LPHANDLER_FUNCTION_EX handler_proc,
                                                                         LPVOID context);
extern BOOL delegate_real_StartServiceCtrlDispatcherU(const cplat_service_entry_u *service_table);
#endif /* PLATFORM_WINDOWS */

#if defined(PLATFORM_LINUX)

// cplat/trace/syslog.h
extern cplat_syslog_sink *delegate_real_cplat_syslog_sink_create(const char *ident, int facility);
extern int delegate_real_cplat_syslog_sink_write(cplat_syslog_sink *handle, int level, const cplat_timespec *timestamp,
                                                 const char *message);
extern int delegate_real_cplat_syslog_sink_rename(cplat_syslog_sink *handle, const char *new_ident);
extern void delegate_real_cplat_syslog_sink_dispose(cplat_syslog_sink *handle);
#endif /* PLATFORM_LINUX */

class Mock_cplat
{
  public:
    // cplat/argparser/argparser.h
    MOCK_METHOD(cplat_argparser *, cplat_argparser_handle_create,
                (int, char *const *, const cplat_argparser_options *));
    MOCK_METHOD(void, cplat_argparser_init, (int, char *const *, const char *));
    MOCK_METHOD(void, cplat_argparser_handle_dispose, (cplat_argparser *));
    MOCK_METHOD(int, cplat_argparser_handle_register_flag,
                (cplat_argparser *, const char *, const char *, const char *, int *));
    MOCK_METHOD(int, cplat_argparser_register_flag, (const char *, const char *, const char *, int *));
    MOCK_METHOD(int, cplat_argparser_handle_register_option_int,
                (cplat_argparser *, const char *, const char *, const char *, const char *, unsigned int, int *));
    MOCK_METHOD(int, cplat_argparser_register_option_int,
                (const char *, const char *, const char *, const char *, unsigned int, int *));
    MOCK_METHOD(int, cplat_argparser_handle_register_option_string,
                (cplat_argparser *, const char *, const char *, const char *, const char *, unsigned int,
                 const char **));
    MOCK_METHOD(int, cplat_argparser_register_option_string,
                (const char *, const char *, const char *, const char *, unsigned int, const char **));
    MOCK_METHOD(int, cplat_argparser_handle_register_option_int_array,
                (cplat_argparser *, const char *, const char *, const char *, const char *, unsigned int, int *, size_t,
                 size_t *));
    MOCK_METHOD(int, cplat_argparser_register_option_int_array,
                (const char *, const char *, const char *, const char *, unsigned int, int *, size_t, size_t *));
    MOCK_METHOD(int, cplat_argparser_handle_register_option_string_array,
                (cplat_argparser *, const char *, const char *, const char *, const char *, unsigned int, const char **,
                 size_t, size_t *));
    MOCK_METHOD(int, cplat_argparser_register_option_string_array,
                (const char *, const char *, const char *, const char *, unsigned int, const char **, size_t,
                 size_t *));
    MOCK_METHOD(int, cplat_argparser_handle_register_positional_int,
                (cplat_argparser *, const char *, const char *, unsigned int, int *));
    MOCK_METHOD(int, cplat_argparser_register_positional_int, (const char *, const char *, unsigned int, int *));
    MOCK_METHOD(int, cplat_argparser_handle_register_positional_string,
                (cplat_argparser *, const char *, const char *, unsigned int, const char **));
    MOCK_METHOD(int, cplat_argparser_register_positional_string,
                (const char *, const char *, unsigned int, const char **));
    MOCK_METHOD(int, cplat_argparser_handle_register_positional_int_array,
                (cplat_argparser *, const char *, const char *, unsigned int, int *, size_t, size_t *));
    MOCK_METHOD(int, cplat_argparser_register_positional_int_array,
                (const char *, const char *, unsigned int, int *, size_t, size_t *));
    MOCK_METHOD(int, cplat_argparser_handle_register_positional_string_array,
                (cplat_argparser *, const char *, const char *, unsigned int, const char **, size_t, size_t *));
    MOCK_METHOD(int, cplat_argparser_register_positional_string_array,
                (const char *, const char *, unsigned int, const char **, size_t, size_t *));
    MOCK_METHOD(int, cplat_argparser_handle_parse, (cplat_argparser *));
    MOCK_METHOD(int, cplat_argparser_parse, ());
    MOCK_METHOD(int, cplat_argparser_handle_get_error, (const cplat_argparser *));
    MOCK_METHOD(int, cplat_argparser_get_error, ());
    MOCK_METHOD(const char *, cplat_argparser_handle_get_error_target, (const cplat_argparser *));
    MOCK_METHOD(const char *, cplat_argparser_get_error_target, ());
    MOCK_METHOD(int, cplat_argparser_handle_get_error_index, (const cplat_argparser *));
    MOCK_METHOD(int, cplat_argparser_get_error_index, ());
    MOCK_METHOD(int, cplat_argparser_handle_get_error_message, (const cplat_argparser *, char *, size_t));
    MOCK_METHOD(int, cplat_argparser_get_error_message, (char *, size_t));
    MOCK_METHOD(int, cplat_argparser_handle_get_usage, (const cplat_argparser *, char *, size_t, size_t *));
    MOCK_METHOD(int, cplat_argparser_get_usage, (char *, size_t, size_t *));
    MOCK_METHOD(int, cplat_argparser_handle_print_usage, (const cplat_argparser *, FILE *));
    MOCK_METHOD(int, cplat_argparser_print_usage, (FILE *));
    MOCK_METHOD(int, cplat_argparser_handle_print_error_messages, (const cplat_argparser *, FILE *));
    MOCK_METHOD(int, cplat_argparser_print_error_messages, (FILE *));
    MOCK_METHOD(size_t, cplat_argparser_handle_get_register_error_count, (const cplat_argparser *));
    MOCK_METHOD(size_t, cplat_argparser_get_register_error_count, ());
    MOCK_METHOD(int, cplat_argparser_handle_get_register_error, (const cplat_argparser *, size_t));
    MOCK_METHOD(int, cplat_argparser_get_register_error, (size_t));
    MOCK_METHOD(const char *, cplat_argparser_handle_get_register_error_target, (const cplat_argparser *, size_t));
    MOCK_METHOD(const char *, cplat_argparser_get_register_error_target, (size_t));
    MOCK_METHOD(int, cplat_argparser_handle_get_register_error_message,
                (const cplat_argparser *, size_t, char *, size_t));
    MOCK_METHOD(int, cplat_argparser_get_register_error_message, (size_t, char *, size_t));
    MOCK_METHOD(int, cplat_argparser_handle_print_register_error_messages, (const cplat_argparser *, FILE *));
    MOCK_METHOD(int, cplat_argparser_print_register_error_messages, (FILE *));

    // cplat/hashtable/hashtable.h
    MOCK_METHOD(int, cplat_hashtable_required_size, (const cplat_hashtable_config *, size_t *, size_t *));
    MOCK_METHOD(int, cplat_hashtable_create,
                (const cplat_hashtable_config *, void *, size_t, void *, size_t, cplat_hashtable **));
    MOCK_METHOD(int, cplat_hashtable_create_growable,
                (const cplat_hashtable_config *, const cplat_hashtable_growth_config *, cplat_hashtable **));
    MOCK_METHOD(int, cplat_hashtable_attach, (void *, size_t, void *, size_t, cplat_hashtable **));
    MOCK_METHOD(int, cplat_hashtable_validate, (const cplat_hashtable *));
    MOCK_METHOD(int, cplat_hashtable_get_config_ref, (const cplat_hashtable *, const cplat_hashtable_config **));
    MOCK_METHOD(int, cplat_hashtable_get_config_val, (const cplat_hashtable *, cplat_hashtable_config *));
    MOCK_METHOD(int, cplat_hashtable_buffer_size, (const cplat_hashtable *, size_t *, size_t *));
    MOCK_METHOD(int, cplat_hashtable_buffer_ref, (const cplat_hashtable *, const void **, const void **));
    MOCK_METHOD(int, cplat_hashtable_add,
                (cplat_hashtable *, const void *, const void *, cplat_hashtable_add_deleted_policy));
    MOCK_METHOD(int, cplat_hashtable_upsert, (cplat_hashtable *, const void *, const void *, int *));
    MOCK_METHOD(int, cplat_hashtable_insert_direct,
                (cplat_hashtable *, uint64_t, const void *, int, const void *, const cplat_timespec *, uint64_t));
    MOCK_METHOD(int, cplat_hashtable_update, (cplat_hashtable *, const void *, const void *));
    MOCK_METHOD(int, cplat_hashtable_update_rec, (cplat_hashtable *, uint64_t, const void *));
    MOCK_METHOD(int, cplat_hashtable_find_value_ref, (const cplat_hashtable *, const void *, const void **));
    MOCK_METHOD(int, cplat_hashtable_find_value_copy,
                (const cplat_hashtable *, const void *, void *, size_t, size_t *));
    MOCK_METHOD(int, cplat_hashtable_find_recno, (const cplat_hashtable *, const void *, uint64_t *));
    MOCK_METHOD(int, cplat_hashtable_get_key_ref, (const cplat_hashtable *, uint64_t, const void **));
    MOCK_METHOD(int, cplat_hashtable_get_key_copy, (const cplat_hashtable *, uint64_t, void *, size_t, size_t *));
    MOCK_METHOD(int, cplat_hashtable_get_value_ref, (const cplat_hashtable *, uint64_t, const void **));
    MOCK_METHOD(int, cplat_hashtable_get_value_copy, (const cplat_hashtable *, uint64_t, void *, size_t, size_t *));
    MOCK_METHOD(int, cplat_hashtable_get_status, (const cplat_hashtable *, uint64_t, int *));
    MOCK_METHOD(int, cplat_hashtable_next_record, (const cplat_hashtable *, uint64_t, unsigned int, uint64_t *, int *));
    MOCK_METHOD(int, cplat_hashtable_get_timestamp_ref, (const cplat_hashtable *, uint64_t, const cplat_timespec **));
    MOCK_METHOD(int, cplat_hashtable_get_timestamp_val, (const cplat_hashtable *, uint64_t, cplat_timespec *));
    MOCK_METHOD(int, cplat_hashtable_get_generation, (const cplat_hashtable *, uint64_t, uint64_t *));
    MOCK_METHOD(int, cplat_hashtable_get_table_timestamp_ref, (const cplat_hashtable *, const cplat_timespec **));
    MOCK_METHOD(int, cplat_hashtable_get_table_timestamp_val, (const cplat_hashtable *, cplat_timespec *));
    MOCK_METHOD(int, cplat_hashtable_get_table_generation, (const cplat_hashtable *, uint64_t *));
    MOCK_METHOD(int, cplat_hashtable_find_timestamp_ref,
                (const cplat_hashtable *, const void *, const cplat_timespec **));
    MOCK_METHOD(int, cplat_hashtable_find_timestamp_val, (const cplat_hashtable *, const void *, cplat_timespec *));
    MOCK_METHOD(int, cplat_hashtable_find_generation, (const cplat_hashtable *, const void *, uint64_t *));
    MOCK_METHOD(int, cplat_hashtable_count_status, (const cplat_hashtable *, size_t *, size_t *, size_t *));
    MOCK_METHOD(int, cplat_hashtable_count, (const cplat_hashtable *, size_t *));
    MOCK_METHOD(int, cplat_hashtable_deleted_count, (const cplat_hashtable *, size_t *));
    MOCK_METHOD(int, cplat_hashtable_empty_count, (const cplat_hashtable *, size_t *));
    MOCK_METHOD(int, cplat_hashtable_delete, (cplat_hashtable *, const void *));
    MOCK_METHOD(int, cplat_hashtable_delete_rec, (cplat_hashtable *, uint64_t));
    MOCK_METHOD(int, cplat_hashtable_push_deleted, (cplat_hashtable *));
    MOCK_METHOD(int, cplat_hashtable_purge_deleted, (cplat_hashtable *));
    MOCK_METHOD(int, cplat_hashtable_compact, (cplat_hashtable *));
    MOCK_METHOD(int, cplat_hashtable_resize, (cplat_hashtable *, const cplat_hashtable_config *));
    MOCK_METHOD(int, cplat_hashtable_rebuild_into,
                (const cplat_hashtable *, const cplat_hashtable_config *, void *, size_t, void *, size_t,
                 cplat_hashtable **));
    MOCK_METHOD(int, cplat_hashtable_clear, (cplat_hashtable *));
    MOCK_METHOD(void, cplat_hashtable_dispose, (cplat_hashtable *));

    // cplat/clock/clock.h
    MOCK_METHOD(uint64_t, cplat_clock_get_monotonic_ms, ());
    MOCK_METHOD(void, cplat_clock_get_monotonic, (cplat_timespec *));
    MOCK_METHOD(void, cplat_clock_get_realtime, (cplat_timespec *));
    MOCK_METHOD(int, cplat_clock_format_realtime_iso8601_local, (char *, size_t, const cplat_timespec *));
    MOCK_METHOD(int, cplat_clock_format_realtime_iso8601_utc, (char *, size_t, const cplat_timespec *));
    MOCK_METHOD(void, cplat_clock_get_realtime_utc, (struct tm *, int32_t *));
    MOCK_METHOD(void, cplat_clock_get_realtime_deadline_ms, (uint64_t, struct timespec *));

    // cplat/clock/timespec.h
    MOCK_METHOD(void, cplat_timespec_normalize, (cplat_timespec *));
    MOCK_METHOD(void, cplat_timespec_add, (const cplat_timespec *, const cplat_timespec *, cplat_timespec *));
    MOCK_METHOD(void, cplat_timespec_sub, (const cplat_timespec *, const cplat_timespec *, cplat_timespec *));
    MOCK_METHOD(int, cplat_timespec_cmp, (const cplat_timespec *, const cplat_timespec *));
    MOCK_METHOD(void, cplat_timespec_add_ms, (const cplat_timespec *, uint64_t, cplat_timespec *));
    MOCK_METHOD(int64_t, cplat_timespec_diff_ms, (const cplat_timespec *, const cplat_timespec *));
    MOCK_METHOD(void, cplat_timespec_to_native, (const cplat_timespec *, struct timespec *));
    MOCK_METHOD(void, cplat_timespec_from_native, (const struct timespec *, cplat_timespec *));

    // cplat/compress/compress.h
    MOCK_METHOD(int, cplat_compress, (uint8_t *, size_t *, const uint8_t *, size_t));
    MOCK_METHOD(int, cplat_decompress, (uint8_t *, size_t *, const uint8_t *, size_t));

    // cplat/console/console.h
    MOCK_METHOD(void, cplat_console_init, ());
    MOCK_METHOD(void, cplat_console_dispose, ());
    MOCK_METHOD(int, cplat_console_attach_parent, (int *, char **, int *));
    MOCK_METHOD(int, cplat_console_write, (cplat_stream, const char *));

    // cplat/console/console.h (internal)
    MOCK_METHOD(void, cplat_internal_console_dispose_on_shutdown, (const cplat_shutdown_event *, void *));

    // cplat/crt/fcntl.h
    MOCK_METHOD(int, cplat_open, (const char *, int, int, cplat_error *));
    MOCK_METHOD(int, cplat_open_fmt, (int, int, cplat_error *, const char *));
    MOCK_METHOD(int, cplat_vopen_fmt, (int, int, cplat_error *, const char *));

    // cplat/crt/file.h
    MOCK_METHOD(void, cplat_file_init, (cplat_file *));
    MOCK_METHOD(int, cplat_file_open, (cplat_file *, const char *, int, cplat_error *));
    MOCK_METHOD(int, cplat_file_write, (cplat_file *, const void *, size_t, cplat_error *));
    MOCK_METHOD(int, cplat_file_read, (cplat_file *, void *, size_t, size_t *, cplat_error *));
    MOCK_METHOD(int, cplat_file_get_size, (const cplat_file *, size_t *, cplat_error *));
    MOCK_METHOD(int, cplat_file_set_size, (cplat_file *, size_t, cplat_error *));
    MOCK_METHOD(int, cplat_file_get_id, (const cplat_file *, cplat_file_id *, cplat_error *));
    MOCK_METHOD(int, cplat_file_get_path_id, (const char *, cplat_file_id *, cplat_error *));
    MOCK_METHOD(int, cplat_file_get_modified_timestamp, (const cplat_file *, cplat_timespec *, cplat_error *));
    MOCK_METHOD(int, cplat_file_set_modified_timestamp, (cplat_file *, const cplat_timespec *, cplat_error *));
    MOCK_METHOD(int, cplat_file_get_path_modified_timestamp, (const char *, cplat_timespec *, cplat_error *));
    MOCK_METHOD(int, cplat_file_set_path_modified_timestamp, (const char *, const cplat_timespec *, cplat_error *));
    MOCK_METHOD(int, cplat_file_flush, (cplat_file *, cplat_error *));
    MOCK_METHOD(int, cplat_file_close, (cplat_file *, cplat_error *));

    // cplat/crt/path.h
    MOCK_METHOD(char *, cplat_path_normalize_sep, (char *));
    MOCK_METHOD(int, cplat_path_get_full, (char *, size_t, cplat_error *, const char *));
    MOCK_METHOD(int, cplat_path_equal, (const char *, const char *, int *, cplat_error *));
    MOCK_METHOD(int, cplat_path_get_temp_dir, (char *, size_t, cplat_error *));
    MOCK_METHOD(int, cplat_path_concat_n, (char *, size_t, cplat_error *, size_t, va_list));
    MOCK_METHOD(int, cplat_vpath_concat_n, (char *, size_t, cplat_error *, size_t, va_list));
    MOCK_METHOD(const char *, cplat_path_basename, (const char *));
    MOCK_METHOD(int, cplat_path_dirname, (char *, size_t, cplat_error *, const char *));
    MOCK_METHOD(const char *, cplat_path_extension, (const char *));
    MOCK_METHOD(int, cplat_path_strip_extension, (char *, size_t, cplat_error *, const char *));
    MOCK_METHOD(int, cplat_path_join_n, (char *, size_t, cplat_error *, size_t, va_list));
    MOCK_METHOD(int, cplat_vpath_join_n, (char *, size_t, cplat_error *, size_t, va_list));

    // cplat/crt/stdio.h
    MOCK_METHOD(FILE *, cplat_fopen, (const char *, const char *, cplat_error *));
    MOCK_METHOD(FILE *, cplat_freopen, (const char *, const char *, FILE *, cplat_error *));
    MOCK_METHOD(int, cplat_fclose, (FILE *, cplat_error *));
    MOCK_METHOD(int, cplat_fflush, (FILE *, cplat_error *));
    MOCK_METHOD(size_t, cplat_fread, (void *, size_t, size_t, FILE *, cplat_error *));
    MOCK_METHOD(size_t, cplat_fwrite, (const void *, size_t, size_t, FILE *, cplat_error *));
    MOCK_METHOD(int, cplat_remove, (const char *, cplat_error *));
    MOCK_METHOD(int, cplat_rename, (const char *, const char *, cplat_error *));
    MOCK_METHOD(int, cplat_scanf, (const char *, va_list));
    MOCK_METHOD(int, cplat_vscanf, (const char *, va_list));
    MOCK_METHOD(int, cplat_fscanf, (FILE *, const char *, va_list));
    MOCK_METHOD(int, cplat_vfscanf, (FILE *, const char *, va_list));
    MOCK_METHOD(int, cplat_snprintf, (char *, size_t, const char *));
    MOCK_METHOD(int, cplat_vsnprintf, (char *, size_t, const char *));
    MOCK_METHOD(int, cplat_fgets, (char *, size_t, FILE *, cplat_error *));
    MOCK_METHOD(int, cplat_fprintf, (FILE *, const char *));
    MOCK_METHOD(int, cplat_vfprintf, (FILE *, const char *));
    MOCK_METHOD(int, cplat_fseek, (FILE *, int64_t, int));
    MOCK_METHOD(int64_t, cplat_ftell, (FILE *));
    MOCK_METHOD(FILE *, cplat_fopen_fmt, (const char *, cplat_error *, const char *));
    MOCK_METHOD(FILE *, cplat_vfopen_fmt, (const char *, cplat_error *, const char *));
    MOCK_METHOD(int, cplat_remove_fmt, (cplat_error *, const char *));
    MOCK_METHOD(int, cplat_vremove_fmt, (cplat_error *, const char *));
    MOCK_METHOD(FILE *, cplat_fopen_temp, (const char *, const char *, char *, size_t, cplat_error *));

    // cplat/crt/stdlib.h
    MOCK_METHOD(int, cplat_getenv, (const char *, char *, size_t, int *, cplat_error *));
    MOCK_METHOD(int, cplat_setenv, (const char *, const char *, int, cplat_error *));
    MOCK_METHOD(int, cplat_unsetenv, (const char *, cplat_error *));
    MOCK_METHOD(int, cplat_parse_int64, (int64_t *, const char *, int));
    MOCK_METHOD(int, cplat_parse_uint64, (uint64_t *, const char *, int));
    MOCK_METHOD(int, cplat_parse_int, (int *, const char *, int));
    MOCK_METHOD(int, cplat_parse_double, (double *, const char *));
    MOCK_METHOD(void *, cplat_malloc, (size_t));
    MOCK_METHOD(void *, cplat_malloc_zerofill, (size_t));
    MOCK_METHOD(void *, cplat_calloc, (size_t, size_t));
    MOCK_METHOD(void *, cplat_realloc, (void *, size_t, size_t));
    MOCK_METHOD(void *, cplat_realloc_zerofill, (void *, size_t, size_t, size_t));
    MOCK_METHOD(void, cplat_free, (void *));

    // cplat/crt/string.h
    MOCK_METHOD(int, cplat_strcpy, (char *, size_t, const char *));
    MOCK_METHOD(int, cplat_strncpy, (char *, size_t, const char *, size_t));
    MOCK_METHOD(int, cplat_strcat, (char *, size_t, const char *));
    MOCK_METHOD(int, cplat_strncat, (char *, size_t, const char *, size_t));
    MOCK_METHOD(char *, cplat_strtok_r, (char *, const char *, char **));
    MOCK_METHOD(char *, cplat_strdup, (const char *));
    MOCK_METHOD(int, cplat_strcasecmp, (const char *, const char *));
    MOCK_METHOD(int, cplat_strncasecmp, (const char *, const char *, size_t));
    MOCK_METHOD(int, cplat_wcscpy, (wchar_t *, size_t, const wchar_t *));
    MOCK_METHOD(int, cplat_sscanf, (const char *, const char *, va_list));
    MOCK_METHOD(int, cplat_vsscanf, (const char *, const char *, va_list));

    // cplat/crt/sys/stat.h
    MOCK_METHOD(int, cplat_stat, (cplat_file_stat_t *, cplat_error *, const char *));
    MOCK_METHOD(int, cplat_mkdir, (const char *, cplat_error *));
    MOCK_METHOD(int, cplat_makedirs, (const char *, cplat_error *));
    MOCK_METHOD(int, cplat_rmdir, (const char *, cplat_error *));
    MOCK_METHOD(int, cplat_stat_fmt, (cplat_file_stat_t *, cplat_error *, const char *));
    MOCK_METHOD(int, cplat_vstat_fmt, (cplat_file_stat_t *, cplat_error *, const char *));
    MOCK_METHOD(int, cplat_mkdir_fmt, (cplat_error *, const char *));
    MOCK_METHOD(int, cplat_vmkdir_fmt, (cplat_error *, const char *));
    MOCK_METHOD(int, cplat_file_stat_is_regular, (const cplat_file_stat_t *));

    // cplat/crt/time.h
    MOCK_METHOD(int, cplat_gmtime, (struct tm *, const time_t *));
    MOCK_METHOD(int, cplat_localtime, (struct tm *, const time_t *));
    MOCK_METHOD(int, cplat_ctime, (char *, size_t, const time_t *));

    // cplat/crt/unistd.h
    MOCK_METHOD(int, cplat_isatty, (cplat_stream));
    MOCK_METHOD(int64_t, cplat_lseek, (int, int64_t, int, cplat_error *));
    MOCK_METHOD(int, cplat_close, (int, cplat_error *));
    MOCK_METHOD(int, cplat_dup, (int, cplat_error *));
    MOCK_METHOD(int, cplat_dup2, (int, int, cplat_error *));
    MOCK_METHOD(int64_t, cplat_read, (int, void *, size_t, cplat_error *));
    MOCK_METHOD(int64_t, cplat_write, (int, const void *, size_t, cplat_error *));
    MOCK_METHOD(int, cplat_access, (const char *, int, cplat_error *));
    MOCK_METHOD(int, cplat_access_fmt, (int, cplat_error *, const char *));
    MOCK_METHOD(int, cplat_vaccess_fmt, (int, cplat_error *, const char *));

    // cplat/crypto/crypto.h
    MOCK_METHOD(int, cplat_crypto_encrypt,
                (uint8_t *, size_t *, const uint8_t *, size_t, const uint8_t *, const uint8_t *, const uint8_t *,
                 size_t));
    MOCK_METHOD(int, cplat_crypto_decrypt,
                (uint8_t *, size_t *, const uint8_t *, size_t, const uint8_t *, const uint8_t *, const uint8_t *,
                 size_t));
    MOCK_METHOD(int, cplat_passphrase_to_key, (uint8_t *, const uint8_t *, size_t));

    // cplat/base/error.h
    MOCK_METHOD(void, cplat_error_clear, (cplat_error *));
    MOCK_METHOD(void, cplat_error_capture_errno, (cplat_error *, int));
    MOCK_METHOD(void, cplat_error_capture_current_errno, (cplat_error *));
    MOCK_METHOD(void, cplat_error_get_last, (cplat_error *));
    MOCK_METHOD(void, cplat_error_set_last, (const cplat_error *));
    MOCK_METHOD(void, cplat_error_clear_last, ());
    MOCK_METHOD(int, cplat_error_is_set, (const cplat_error *));
    MOCK_METHOD(cplat_error_domain, cplat_error_get_domain, (const cplat_error *));
    MOCK_METHOD(int, cplat_error_get_errno, (const cplat_error *));
    MOCK_METHOD(int, cplat_error_to_result, (const cplat_error *));
    MOCK_METHOD(cplat_error_cause, cplat_error_get_cause, (const cplat_error *));
    MOCK_METHOD(int, cplat_error_is, (const cplat_error *, cplat_error_cause));

    // cplat/base/error_message.h
    MOCK_METHOD(const char *, cplat_result_to_string, (int));
    MOCK_METHOD(int, cplat_error_message, (char *, size_t, const cplat_error *));

    // cplat/crypto/random.h
    MOCK_METHOD(int, cplat_random_bytes, (void *, size_t));

    // cplat/locale/ui_language.h
    MOCK_METHOD(int, cplat_ui_language_get_tag, (char *, size_t));

    // cplat/mmap/mmap.h
    MOCK_METHOD(int, cplat_mmap_attach, (const char *, cplat_mmap_access, size_t, cplat_mmap **, cplat_error *));
    MOCK_METHOD(void *, cplat_mmap_get_address, (const cplat_mmap *));
    MOCK_METHOD(size_t, cplat_mmap_get_size, (const cplat_mmap *));
    MOCK_METHOD(int, cplat_mmap_flush, (cplat_mmap *, void *, size_t, cplat_error *));
    MOCK_METHOD(int, cplat_mmap_detach, (cplat_mmap *, cplat_error *));

    // cplat/net/byteorder.h
    MOCK_METHOD(uint16_t, cplat_hton16, (uint16_t));
    MOCK_METHOD(uint16_t, cplat_ntoh16, (uint16_t));
    MOCK_METHOD(uint32_t, cplat_hton32, (uint32_t));
    MOCK_METHOD(uint32_t, cplat_ntoh32, (uint32_t));
    MOCK_METHOD(uint64_t, cplat_hton64, (uint64_t));
    MOCK_METHOD(uint64_t, cplat_ntoh64, (uint64_t));

    // cplat/net/endpoint.h
    MOCK_METHOD(int, cplat_ipv4_parse, (const char *, uint32_t *));
    MOCK_METHOD(int, cplat_ipv4_resolve, (const char *, uint32_t *, cplat_error *));
    MOCK_METHOD(int, cplat_ipv4_to_string, (uint32_t, char *, size_t, cplat_error *));

    // cplat/net/socket.h
    MOCK_METHOD(int, cplat_socket_open, (cplat_socket_kind, cplat_socket *, cplat_error *));
    MOCK_METHOD(void, cplat_socket_close, (cplat_socket));
    MOCK_METHOD(void, cplat_socket_shutdown, (cplat_socket));
    MOCK_METHOD(int, cplat_socket_bind, (cplat_socket, const cplat_ipv4_endpoint *, cplat_error *));
    MOCK_METHOD(int, cplat_socket_listen, (cplat_socket, int, cplat_error *));
    MOCK_METHOD(int, cplat_socket_accept, (cplat_socket, cplat_ipv4_endpoint *, cplat_socket *, cplat_error *));
    MOCK_METHOD(int, cplat_socket_connect, (cplat_socket, const cplat_ipv4_endpoint *, cplat_error *));
    MOCK_METHOD(int, cplat_socket_get_pending_error, (cplat_socket, cplat_error *));
    MOCK_METHOD(int, cplat_socket_set_nonblocking, (cplat_socket, int, cplat_error *));
    MOCK_METHOD(int, cplat_socket_set_reuse_address, (cplat_socket, int, cplat_error *));
    MOCK_METHOD(int, cplat_socket_set_broadcast, (cplat_socket, int, cplat_error *));
    MOCK_METHOD(int, cplat_socket_set_multicast_interface, (cplat_socket, uint32_t, cplat_error *));
    MOCK_METHOD(int, cplat_socket_join_multicast_group, (cplat_socket, uint32_t, uint32_t, cplat_error *));
    MOCK_METHOD(int, cplat_socket_leave_multicast_group, (cplat_socket, uint32_t, uint32_t, cplat_error *));
    MOCK_METHOD(int, cplat_socket_send, (cplat_socket, const void *, size_t, size_t *, cplat_error *));
    MOCK_METHOD(int, cplat_socket_recv, (cplat_socket, void *, size_t, size_t *, cplat_error *));
    MOCK_METHOD(int, cplat_socket_sendto,
                (cplat_socket, const void *, size_t, const cplat_ipv4_endpoint *, size_t *, cplat_error *));
    MOCK_METHOD(int, cplat_socket_recvfrom,
                (cplat_socket, void *, size_t, cplat_ipv4_endpoint *, size_t *, cplat_error *));
    MOCK_METHOD(int, cplat_socket_send_all, (cplat_socket, const void *, size_t, cplat_error *));
    MOCK_METHOD(int, cplat_socket_recv_all, (cplat_socket, void *, size_t, cplat_error *));
    MOCK_METHOD(int, cplat_socket_wait_readable, (cplat_socket, int, int *, cplat_error *));
    MOCK_METHOD(int, cplat_socket_wait_writable, (cplat_socket, int, int *, cplat_error *));
    MOCK_METHOD(int, cplat_socket_wait_readable_multi,
                (const cplat_socket *, size_t, int, unsigned char *, cplat_error *));
    MOCK_METHOD(int, cplat_socket_shutdown_receive, (cplat_socket *, cplat_error *));

    // cplat/prompt/pinned_prompt.h
    MOCK_METHOD(cplat_pinned_prompt *, cplat_pinned_prompt_create, (const cplat_pinned_prompt_options *));
    MOCK_METHOD(void, cplat_pinned_prompt_dispose, (cplat_pinned_prompt *));
    MOCK_METHOD(int, cplat_pinned_prompt_readline_at,
                (cplat_pinned_prompt *, char *, size_t, const char *, const char *, int));
    MOCK_METHOD(int, cplat_pinned_prompt_readline_with_initial_at,
                (cplat_pinned_prompt *, char *, size_t, const char *, const char *, const char *, int));
    MOCK_METHOD(int, cplat_pinned_prompt_readline_fmt_at,
                (cplat_pinned_prompt *, char *, size_t, const char *, int, const char *, va_list));
    MOCK_METHOD(int, cplat_pinned_prompt_write,
                (cplat_pinned_prompt *, cplat_pinned_prompt_channel, const void *, size_t, size_t *));
    MOCK_METHOD(int, cplat_pinned_prompt_printf, (cplat_pinned_prompt *, cplat_pinned_prompt_channel, const char *));
    MOCK_METHOD(int, cplat_pinned_prompt_status_enable,
                (cplat_pinned_prompt *, cplat_pinned_prompt_status_position, int));
    MOCK_METHOD(int, cplat_pinned_prompt_status_set,
                (cplat_pinned_prompt *, cplat_pinned_prompt_status_position, cplat_pinned_prompt_status_align,
                 const char *));

    // cplat/prompt/prompt.h
    MOCK_METHOD(cplat_prompt *, cplat_prompt_create, (const cplat_prompt_options *));
    MOCK_METHOD(void, cplat_prompt_dispose, (cplat_prompt *));
    MOCK_METHOD(int, cplat_prompt_readline_at, (cplat_prompt *, char *, size_t, const char *, const char *, int));
    MOCK_METHOD(int, cplat_prompt_readline_with_initial_at,
                (cplat_prompt *, char *, size_t, const char *, const char *, const char *, int));
    MOCK_METHOD(int, cplat_prompt_readline_fmt_at,
                (cplat_prompt *, char *, size_t, const char *, int, const char *, va_list));

    // cplat/runtime/elevated_process.h
    MOCK_METHOD(int, cplat_elevated_process_is_elevated, (int *));
    MOCK_METHOD(int, cplat_elevated_process_run_if_needed, (const char *, int *, int *));
    MOCK_METHOD(int, cplat_elevated_process_run_with_result, (const char *, int *, int *, char *, size_t));
    MOCK_METHOD(int, cplat_elevated_process_extract_result_target, (int *, char **, int *));
    MOCK_METHOD(int, cplat_elevated_process_report_result, (const char *));
    MOCK_METHOD(int, cplat_elevated_process_run_piped,
                (const char *, cplat_elevated_process_output_fn, void *, int *, int *));
    MOCK_METHOD(int, cplat_elevated_process_attach_output_pipes, (int *, char **, int *));

    // cplat/regex/regex.h
    MOCK_METHOD(int, cplat_regex_create, (const char *, unsigned int, cplat_regex **, cplat_error *));
    MOCK_METHOD(void, cplat_regex_dispose, (cplat_regex *));
    MOCK_METHOD(size_t, cplat_regex_get_group_count, (const cplat_regex *));
    MOCK_METHOD(int, cplat_regex_search,
                (const cplat_regex *, const char *, size_t, size_t, unsigned int, cplat_regex_match *, size_t, int *,
                 cplat_error *));
    MOCK_METHOD(int, cplat_regex_matches,
                (const cplat_regex *, const char *, size_t, unsigned int, cplat_regex_match *, size_t, int *,
                 cplat_error *));
    MOCK_METHOD(int, cplat_regex_replace,
                (const cplat_regex *, const char *, size_t, const char *, unsigned int, char *, size_t, size_t *,
                 cplat_error *));
    MOCK_METHOD(int, cplat_regex_iter_create,
                (const cplat_regex *, const char *, size_t, unsigned int, cplat_regex_iter **, cplat_error *));
    MOCK_METHOD(int, cplat_regex_iter_next, (cplat_regex_iter *, cplat_regex_match *, size_t, int *, cplat_error *));
    MOCK_METHOD(void, cplat_regex_iter_dispose, (cplat_regex_iter *));
    MOCK_METHOD(int, cplat_regex_split,
                (const cplat_regex *, const char *, size_t, size_t, unsigned int, cplat_regex_match *, size_t, size_t *,
                 cplat_error *));

    // cplat/runtime/memory_lock.h
    MOCK_METHOD(int, cplat_memory_lock_range, (const void *, size_t));
    MOCK_METHOD(int, cplat_memory_unlock_range, (const void *, size_t));
    MOCK_METHOD(int, cplat_memory_lock_self, (const cplat_memory_lock_self_options *, cplat_memory_lock_scope **));
    MOCK_METHOD(int, cplat_memory_lock_scope_release, (cplat_memory_lock_scope *));
    MOCK_METHOD(void, cplat_secure_zero, (void *, size_t));

    // cplat/runtime/host.h
    MOCK_METHOD(int, cplat_host_get_name, (char *, size_t));

    // cplat/runtime/module.h
    MOCK_METHOD(int, cplat_module_get_path, (char *, size_t, const void *));
    MOCK_METHOD(int, cplat_module_get_basename, (char *, size_t, const void *));

    // cplat/runtime/process.h
    MOCK_METHOD(int, cplat_process_get_executable_path, (char *, size_t));
    MOCK_METHOD(uint32_t, cplat_process_get_pid, ());
    MOCK_METHOD(uint32_t, cplat_process_get_tid, ());
    MOCK_METHOD(int, cplat_process_start, (const cplat_process_options *, cplat_process **));
    MOCK_METHOD(int, cplat_process_wait, (cplat_process *, int));
    MOCK_METHOD(int, cplat_process_get_exit_code, (cplat_process *, int *));
    MOCK_METHOD(int, cplat_process_terminate, (cplat_process *));
    MOCK_METHOD(void, cplat_process_dispose, (cplat_process *));
    MOCK_METHOD(int, cplat_process_run_sync, (const cplat_process_options *, int, int *));

    // cplat/runtime/shutdown.h
    MOCK_METHOD(int, cplat_shutdown_register, (cplat_shutdown_fn, void *));
    MOCK_METHOD(int, cplat_shutdown_request_register, (cplat_shutdown_fn, void *));
    MOCK_METHOD(void, cplat_exit, (int));
    MOCK_METHOD(int, cplat_shutdown_invoke_for_test, (const cplat_shutdown_event *, int *));
    MOCK_METHOD(int, cplat_shutdown_request_invoke_for_test, (const cplat_shutdown_event *, int *));
    MOCK_METHOD(void, cplat_shutdown_reset_for_test, ());

    // cplat/runtime/sym_loader.h
    MOCK_METHOD(void *, cplat_sym_loader_resolve, (cplat_sym_loader_entry *));
    MOCK_METHOD(int, cplat_sym_loader_is_default, (cplat_sym_loader_entry *));
    MOCK_METHOD(void, cplat_sym_loader_init, (cplat_sym_loader_entry *const *, size_t, const char *));
    MOCK_METHOD(void, cplat_sym_loader_dispose, (cplat_sym_loader_entry *const *, size_t));
    MOCK_METHOD(int, cplat_sym_loader_info, (cplat_sym_loader_entry *const *, size_t));

    // cplat/string_catalog/string_catalog.h
    MOCK_METHOD(int, cplat_string_catalog_set_language, (cplat_string_catalog_language));
    MOCK_METHOD(cplat_string_catalog_language, cplat_string_catalog_get_language, ());
    MOCK_METHOD(int, cplat_string_catalog_language_from_tag, (const char *, cplat_string_catalog_language *));
    MOCK_METHOD(int, cplat_string_catalog_format, (const cplat_string_catalog *, char *, size_t, int, va_list));
    MOCK_METHOD(int, cplat_string_catalog_vformat, (const cplat_string_catalog *, char *, size_t, int, va_list));
    MOCK_METHOD(int, cplat_string_catalog_verify,
                (const cplat_string_catalog *, int *, cplat_string_catalog_language *));
    MOCK_METHOD(const cplat_string_catalog_entry *, cplat_string_catalog_get_entry,
                (const cplat_string_catalog *, int));
    MOCK_METHOD(int, cplat_string_catalog_get_category, (const cplat_string_catalog *, int));
    MOCK_METHOD(const char *, cplat_string_catalog_get_id, (const cplat_string_catalog *, int));
    MOCK_METHOD(const char *, cplat_string_catalog_get_note, (const cplat_string_catalog *, int));

    // cplat/string_catalog/filter.h
    MOCK_METHOD(int, cplat_string_catalog_filter_compile,
                (const char *, size_t, size_t, size_t, void *, size_t, cplat_string_catalog_filter_diagnostic *, size_t,
                 size_t *));
    MOCK_METHOD(int, cplat_string_catalog_filter_compile_line,
                (void *, size_t, size_t, const char *, cplat_string_catalog_filter_diagnostic *));
    MOCK_METHOD(int, cplat_string_catalog_filter_insert_line,
                (void *, size_t, size_t, const char *, cplat_string_catalog_filter_diagnostic *));
    MOCK_METHOD(int, cplat_string_catalog_filter_remove_line, (void *, size_t, size_t));
    MOCK_METHOD(int, cplat_string_catalog_filter_validate, (const void *, size_t));
    MOCK_METHOD(int, cplat_string_catalog_filter_get_info, (const void *, size_t, cplat_string_catalog_filter_info *));
    MOCK_METHOD(int, cplat_string_catalog_filter_decompile_line, (const void *, size_t, size_t, char *, size_t));
    MOCK_METHOD(int, cplat_string_catalog_filter_slot_create,
                (const cplat_string_catalog *, const cplat_string_catalog_filter_key_name *, size_t,
                 const cplat_string_catalog_filter_category_names *, size_t, size_t,
                 cplat_string_catalog_filter_slot **));
    MOCK_METHOD(void, cplat_string_catalog_filter_slot_dispose, (cplat_string_catalog_filter_slot **));
    MOCK_METHOD(const cplat_string_catalog *, cplat_string_catalog_filter_slot_get_catalog,
                (const cplat_string_catalog_filter_slot *));
    MOCK_METHOD(int, cplat_string_catalog_filter_source_publish,
                (void *, size_t, const void *, size_t, uint64_t, const cplat_string_catalog_filter_source_lock *,
                 uint64_t *));
    MOCK_METHOD(int, cplat_string_catalog_filter_get_catalog_id, (const cplat_string_catalog *, uint64_t *));
    MOCK_METHOD(int, cplat_string_catalog_filter_source_get_info,
                (const void *, size_t, cplat_string_catalog_filter_source_info *));
    MOCK_METHOD(int, cplat_string_catalog_filter_slot_attach_source,
                (cplat_string_catalog_filter_slot *, const void *, size_t,
                 const cplat_string_catalog_filter_source_lock *));
    MOCK_METHOD(int, cplat_string_catalog_filter_slot_get_source_status,
                (cplat_string_catalog_filter_slot *, cplat_string_catalog_filter_source_status *));
    MOCK_METHOD(int, cplat_string_catalog_filter_slot_apply,
                (cplat_string_catalog_filter_slot *, const void *, size_t, cplat_string_catalog_filter_diagnostic *,
                 size_t, size_t *));
    MOCK_METHOD(int, cplat_string_catalog_filter_slot_check,
                (cplat_string_catalog_filter_slot *, const void *, size_t, cplat_string_catalog_filter_diagnostic *,
                 size_t, size_t *, cplat_string_catalog_filter_warning *, size_t, size_t *));
    MOCK_METHOD(int, cplat_string_catalog_filter_slot_snapshot, (cplat_string_catalog_filter_slot *, void *, size_t));
    MOCK_METHOD(int, cplat_string_catalog_filter_slot_get_line_error,
                (cplat_string_catalog_filter_slot *, size_t, cplat_string_catalog_filter_line_error *));
    MOCK_METHOD(int, cplat_string_catalog_filter_slot_describe_line,
                (cplat_string_catalog_filter_slot *, size_t, char *, size_t));
    MOCK_METHOD(int, cplat_string_catalog_filter_slot_test,
                (cplat_string_catalog_filter_slot *, int, cplat_string_catalog_filter_state *));
    MOCK_METHOD(int, cplat_string_catalog_filter_slot_vformat,
                (cplat_string_catalog_filter_slot *, char *, size_t, int *, int, va_list));
    MOCK_METHOD(int, cplat_string_catalog_filter_slot_format,
                (cplat_string_catalog_filter_slot *, char *, size_t, int *, int, va_list));

    // cplat/sync/sync.h
    MOCK_METHOD(int, cplat_local_lock_create, (cplat_local_lock **));
    MOCK_METHOD(int, cplat_local_lock_lock, (cplat_local_lock *, int));
    MOCK_METHOD(int, cplat_local_lock_try_lock, (cplat_local_lock *));
    MOCK_METHOD(int, cplat_local_lock_unlock, (cplat_local_lock *));
    MOCK_METHOD(void, cplat_local_lock_dispose, (cplat_local_lock *));
    MOCK_METHOD(int, cplat_condvar_create, (cplat_condvar **));
    MOCK_METHOD(int, cplat_condvar_wait, (cplat_condvar *, cplat_local_lock *, int));
    MOCK_METHOD(int, cplat_condvar_signal, (cplat_condvar *));
    MOCK_METHOD(int, cplat_condvar_broadcast, (cplat_condvar *));
    MOCK_METHOD(void, cplat_condvar_dispose, (cplat_condvar *));
    MOCK_METHOD(int, cplat_local_rwlock_create, (cplat_local_rwlock **));
    MOCK_METHOD(int, cplat_local_rwlock_lock_shared, (cplat_local_rwlock *, int));
    MOCK_METHOD(int, cplat_local_rwlock_try_lock_shared, (cplat_local_rwlock *));
    MOCK_METHOD(int, cplat_local_rwlock_lock_exclusive, (cplat_local_rwlock *, int));
    MOCK_METHOD(int, cplat_local_rwlock_try_lock_exclusive, (cplat_local_rwlock *));
    MOCK_METHOD(int, cplat_local_rwlock_unlock_shared, (cplat_local_rwlock *));
    MOCK_METHOD(int, cplat_local_rwlock_unlock_exclusive, (cplat_local_rwlock *));
    MOCK_METHOD(void, cplat_local_rwlock_dispose, (cplat_local_rwlock *));
    MOCK_METHOD(int, cplat_thread_create, (cplat_thread **, cplat_thread_fn, void *));
    MOCK_METHOD(int, cplat_thread_join, (cplat_thread *, int));
    MOCK_METHOD(void, cplat_thread_detach, (cplat_thread *));
    MOCK_METHOD(int, cplat_interprocess_lock_open, (const char *, cplat_interprocess_lock **));
    MOCK_METHOD(int, cplat_interprocess_lock_import_descriptor, (const void *, size_t, cplat_interprocess_lock **));
    MOCK_METHOD(int, cplat_interprocess_lock_export_descriptor, (const cplat_interprocess_lock *, void *, size_t *));
    MOCK_METHOD(int, cplat_interprocess_lock_lock, (cplat_interprocess_lock *, int));
    MOCK_METHOD(int, cplat_interprocess_lock_try_lock, (cplat_interprocess_lock *));
    MOCK_METHOD(int, cplat_interprocess_lock_unlock, (cplat_interprocess_lock *));
    MOCK_METHOD(void, cplat_interprocess_lock_dispose, (cplat_interprocess_lock *));
    MOCK_METHOD(int, cplat_interprocess_rwlock_open, (const char *, cplat_interprocess_rwlock **));
    MOCK_METHOD(int, cplat_interprocess_rwlock_import_descriptor, (const void *, size_t, cplat_interprocess_rwlock **));
    MOCK_METHOD(int, cplat_interprocess_rwlock_export_descriptor,
                (const cplat_interprocess_rwlock *, void *, size_t *));
    MOCK_METHOD(int, cplat_interprocess_rwlock_lock_shared, (cplat_interprocess_rwlock *, int));
    MOCK_METHOD(int, cplat_interprocess_rwlock_try_lock_shared, (cplat_interprocess_rwlock *));
    MOCK_METHOD(int, cplat_interprocess_rwlock_lock_exclusive, (cplat_interprocess_rwlock *, int));
    MOCK_METHOD(int, cplat_interprocess_rwlock_try_lock_exclusive, (cplat_interprocess_rwlock *));
    MOCK_METHOD(int, cplat_interprocess_rwlock_unlock, (cplat_interprocess_rwlock *));
    MOCK_METHOD(void, cplat_interprocess_rwlock_dispose, (cplat_interprocess_rwlock *));
    MOCK_METHOD(void, cplat_call_once, (cplat_once_flag *, cplat_once_fn));
    MOCK_METHOD(void, cplat_sleep_ms, (int));

    // cplat/trace/trace_file.h
    MOCK_METHOD(cplat_trace_file_sink *, cplat_trace_file_sink_create, (const char *, size_t, int, int));
    MOCK_METHOD(int, cplat_trace_file_sink_write, (cplat_trace_file_sink *, int, const cplat_timespec *, const char *));
    MOCK_METHOD(void, cplat_trace_file_sink_dispose, (cplat_trace_file_sink *));

    // cplat/trace/tracer.h
    MOCK_METHOD(cplat_tracer *, cplat_tracer_create, (cplat_tracer_concurrency_mode));
    MOCK_METHOD(int, cplat_tracer_start, (cplat_tracer *));
    MOCK_METHOD(int, cplat_tracer_stop, (cplat_tracer *));
    MOCK_METHOD(cplat_tracer_state, cplat_tracer_get_state, (cplat_tracer *));
    MOCK_METHOD(int, cplat_tracer_write_at, (cplat_tracer *, cplat_trace_level, const cplat_timespec *, const char *));
    MOCK_METHOD(int, cplat_tracer_writef_at, (cplat_tracer *, cplat_trace_level, const cplat_timespec *, const char *));
    MOCK_METHOD(int, cplat_tracer_vwritef_at,
                (cplat_tracer *, cplat_trace_level, const cplat_timespec *, const char *, va_list));
    MOCK_METHOD(int, cplat_tracer_write_hex_at,
                (cplat_tracer *, cplat_trace_level, const cplat_timespec *, const void *, size_t, const char *));
    MOCK_METHOD(int, cplat_tracer_write_hexf_at,
                (cplat_tracer *, cplat_trace_level, const cplat_timespec *, const void *, size_t, const char *));
    MOCK_METHOD(int, cplat_tracer_vwrite_hexf_at,
                (cplat_tracer *, cplat_trace_level, const cplat_timespec *, const void *, size_t, const char *,
                 va_list));
    MOCK_METHOD(const char *, cplat_tracer_hex_sep, (const char *));
    MOCK_METHOD(const char *, cplat_tracer_hex_msg, (const char *));
    MOCK_METHOD(int, cplat_tracer_write_with_source,
                (cplat_tracer *, cplat_trace_level, const cplat_timespec *, const char *, int, const char *));
    MOCK_METHOD(int, cplat_tracer_set_name, (cplat_tracer *, const char *, int64_t));
    MOCK_METHOD(int, cplat_tracer_get_name, (cplat_tracer *, char *, size_t));
    MOCK_METHOD(int64_t, cplat_tracer_get_identifier, (cplat_tracer *));
    MOCK_METHOD(int, cplat_tracer_set_file_name, (cplat_tracer *, const char *, int64_t));
    MOCK_METHOD(int, cplat_tracer_get_file_name, (cplat_tracer *, char *, size_t));
    MOCK_METHOD(int64_t, cplat_tracer_get_file_identifier, (cplat_tracer *));
    MOCK_METHOD(cplat_trace_level, cplat_tracer_get_os_level, (cplat_tracer *));
    MOCK_METHOD(int, cplat_tracer_set_os_level, (cplat_tracer *, cplat_trace_level));
    MOCK_METHOD(cplat_trace_level, cplat_tracer_get_etw_level, (cplat_tracer *));
    MOCK_METHOD(int, cplat_tracer_set_etw_level, (cplat_tracer *, cplat_trace_level));
    MOCK_METHOD(cplat_trace_level, cplat_tracer_get_file_level, (cplat_tracer *));
    MOCK_METHOD(int, cplat_tracer_set_file_level, (cplat_tracer *, const char *, cplat_trace_level, size_t, int, int));
    MOCK_METHOD(cplat_trace_level, cplat_tracer_get_stderr_level, (cplat_tracer *));
    MOCK_METHOD(int, cplat_tracer_set_stderr_level, (cplat_tracer *, cplat_trace_level));
    MOCK_METHOD(void, cplat_tracer_dispose, (cplat_tracer **));
    MOCK_METHOD(cplat_tracer_hook_entry *, cplat_tracer_set_hook, (cplat_tracer *, cplat_tracer_hook_fn, void *));
    MOCK_METHOD(void, cplat_tracer_remove_hook, (cplat_tracer *, cplat_tracer_hook_entry *));
    MOCK_METHOD(void, cplat_tracer_call_next_hook,
                (cplat_tracer_hook_entry *, cplat_tracer *, cplat_trace_level, const cplat_timespec *, const char *));

#if defined(PLATFORM_WINDOWS)

    // cplat/crt/wchar_conv.h
    MOCK_METHOD(int, cplat_utf8_to_wpath, (wchar_t *, size_t, const char *));
    MOCK_METHOD(int, cplat_utf8_to_wstr, (wchar_t *, size_t, const char *));
    MOCK_METHOD(int, cplat_wpath_to_utf8, (char *, size_t, const wchar_t *));
    MOCK_METHOD(int, cplat_wstr_to_utf8, (char *, size_t, const wchar_t *));
    MOCK_METHOD(wchar_t *, cplat_utf8_to_wstr_alloc, (const char *));
    MOCK_METHOD(char *, cplat_wstr_to_utf8_alloc, (const wchar_t *));

    // cplat/base/error.h
    MOCK_METHOD(void, cplat_error_capture_windows_error, (cplat_error *, unsigned long));
    MOCK_METHOD(void, cplat_error_capture_current_windows_error, (cplat_error *));
    MOCK_METHOD(unsigned long, cplat_error_get_windows_error, (const cplat_error *));

    // cplat/trace/etw.h
    MOCK_METHOD(cplat_etw_provider *, cplat_etw_provider_create, (cplat_etw_provider_ref_t));
    MOCK_METHOD(int, cplat_etw_provider_write, (cplat_etw_provider *, int, const char *, const char *));
    MOCK_METHOD(void, cplat_etw_provider_dispose, (cplat_etw_provider *));
    MOCK_METHOD(int, cplat_etw_session_check_access, ());
    MOCK_METHOD(int, cplat_etw_session_start,
                (const char *, const char *, cplat_etw_event_fn, void *, cplat_etw_session **));
    MOCK_METHOD(void, cplat_etw_session_stop, (cplat_etw_session *));

    // cplat/trace/eventlog.h
    MOCK_METHOD(cplat_eventlog_sink *, cplat_eventlog_sink_create, (const char *));
    MOCK_METHOD(int, cplat_eventlog_sink_write,
                (cplat_eventlog_sink *, int, int64_t, const char *, int64_t, const char *));
    MOCK_METHOD(void, cplat_eventlog_sink_dispose, (cplat_eventlog_sink *));
    MOCK_METHOD(int, cplat_eventlog_register_source, (const char *, const char *));
    MOCK_METHOD(int, cplat_eventlog_unregister_source, (const char *));

    // cplat/win32/win32.h
    MOCK_METHOD(HANDLE, CreateFileU, (const char *, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE));
    MOCK_METHOD(HANDLE, CreateNamedPipeU,
                (const char *, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPSECURITY_ATTRIBUTES));
    MOCK_METHOD(DWORD, GetModuleFileNameU, (HMODULE, char *, DWORD));
    MOCK_METHOD(BOOL, WriteConsoleU, (HANDLE, const char *, DWORD, DWORD *, void *));
    MOCK_METHOD(BOOL, GetVolumePathNameU, (const char *, char *, DWORD));
    MOCK_METHOD(BOOL, GetVolumeInformationU, (const char *, char *, DWORD, DWORD *, DWORD *, DWORD *, char *, DWORD));
    MOCK_METHOD(HMODULE, LoadLibraryU, (const char *));
    MOCK_METHOD(BOOL, CreateProcessU,
                (const char *, const char *, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD, LPVOID,
                 const char *, LPSTARTUPINFOW, LPPROCESS_INFORMATION));
    MOCK_METHOD(SC_HANDLE, OpenSCManagerU, (const char *, const char *, DWORD));
    MOCK_METHOD(SC_HANDLE, CreateServiceU,
                (SC_HANDLE, const char *, const char *, DWORD, DWORD, DWORD, DWORD, const char *, const char *, LPDWORD,
                 const char *, const char *, const char *));
    MOCK_METHOD(SC_HANDLE, OpenServiceU, (SC_HANDLE, const char *, DWORD));
    MOCK_METHOD(BOOL, ChangeServiceConfig2U, (SC_HANDLE, DWORD, const char *));
    MOCK_METHOD(SERVICE_STATUS_HANDLE, RegisterServiceCtrlHandlerExU, (const char *, LPHANDLER_FUNCTION_EX, LPVOID));
    MOCK_METHOD(BOOL, StartServiceCtrlDispatcherU, (const cplat_service_entry_u *));
#endif /* PLATFORM_WINDOWS */

#if defined(PLATFORM_LINUX)

    // cplat/trace/syslog.h
    MOCK_METHOD(cplat_syslog_sink *, cplat_syslog_sink_create, (const char *, int));
    MOCK_METHOD(int, cplat_syslog_sink_write, (cplat_syslog_sink *, int, const cplat_timespec *, const char *));
    MOCK_METHOD(int, cplat_syslog_sink_rename, (cplat_syslog_sink *, const char *));
    MOCK_METHOD(void, cplat_syslog_sink_dispose, (cplat_syslog_sink *));
#endif /* PLATFORM_LINUX */

    Mock_cplat();
    ~Mock_cplat();
};

extern Mock_cplat *_mock_cplat;

#endif /* MOCK_CPLAT_H */
