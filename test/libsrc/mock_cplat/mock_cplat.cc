#include <cplat/base/platform.h>
#include <testfw.h>
#include <mock_cplat.h>

Mock_cplat *_mock_cplat = nullptr;

Mock_cplat::Mock_cplat()
{
    // cplat/argparser/argparser.h
    ON_CALL(*this, cplat_argparser_handle_create(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_handle_create));
    ON_CALL(*this, cplat_argparser_init(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_argparser_init));
    ON_CALL(*this, cplat_argparser_handle_dispose(_))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_handle_dispose));
    ON_CALL(*this, cplat_argparser_handle_register_flag(_, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_handle_register_flag));
    ON_CALL(*this, cplat_argparser_register_flag(_, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_register_flag));
    ON_CALL(*this, cplat_argparser_handle_register_option_int(_, _, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_handle_register_option_int));
    ON_CALL(*this, cplat_argparser_register_option_int(_, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_register_option_int));
    ON_CALL(*this, cplat_argparser_handle_register_option_string(_, _, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_handle_register_option_string));
    ON_CALL(*this, cplat_argparser_register_option_string(_, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_register_option_string));
    ON_CALL(*this, cplat_argparser_handle_register_option_int_array(_, _, _, _, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_handle_register_option_int_array));
    ON_CALL(*this, cplat_argparser_register_option_int_array(_, _, _, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_register_option_int_array));
    ON_CALL(*this, cplat_argparser_handle_register_option_string_array(_, _, _, _, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_handle_register_option_string_array));
    ON_CALL(*this, cplat_argparser_register_option_string_array(_, _, _, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_register_option_string_array));
    ON_CALL(*this, cplat_argparser_handle_register_positional_int(_, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_handle_register_positional_int));
    ON_CALL(*this, cplat_argparser_register_positional_int(_, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_register_positional_int));
    ON_CALL(*this, cplat_argparser_handle_register_positional_string(_, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_handle_register_positional_string));
    ON_CALL(*this, cplat_argparser_register_positional_string(_, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_register_positional_string));
    ON_CALL(*this, cplat_argparser_handle_register_positional_int_array(_, _, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_handle_register_positional_int_array));
    ON_CALL(*this, cplat_argparser_register_positional_int_array(_, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_register_positional_int_array));
    ON_CALL(*this, cplat_argparser_handle_register_positional_string_array(_, _, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_handle_register_positional_string_array));
    ON_CALL(*this, cplat_argparser_register_positional_string_array(_, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_register_positional_string_array));
    ON_CALL(*this, cplat_argparser_handle_parse(_)).WillByDefault(Invoke(delegate_real_cplat_argparser_handle_parse));
    ON_CALL(*this, cplat_argparser_parse()).WillByDefault(Invoke(delegate_real_cplat_argparser_parse));
    ON_CALL(*this, cplat_argparser_handle_get_error(_))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_handle_get_error));
    ON_CALL(*this, cplat_argparser_get_error()).WillByDefault(Invoke(delegate_real_cplat_argparser_get_error));
    ON_CALL(*this, cplat_argparser_handle_get_error_target(_))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_handle_get_error_target));
    ON_CALL(*this, cplat_argparser_get_error_target())
        .WillByDefault(Invoke(delegate_real_cplat_argparser_get_error_target));
    ON_CALL(*this, cplat_argparser_handle_get_error_index(_))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_handle_get_error_index));
    ON_CALL(*this, cplat_argparser_get_error_index())
        .WillByDefault(Invoke(delegate_real_cplat_argparser_get_error_index));
    ON_CALL(*this, cplat_argparser_handle_get_error_message(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_handle_get_error_message));
    ON_CALL(*this, cplat_argparser_get_error_message(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_get_error_message));
    ON_CALL(*this, cplat_argparser_handle_get_usage(_, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_handle_get_usage));
    ON_CALL(*this, cplat_argparser_get_usage(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_argparser_get_usage));
    ON_CALL(*this, cplat_argparser_handle_print_usage(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_handle_print_usage));
    ON_CALL(*this, cplat_argparser_print_usage(_)).WillByDefault(Invoke(delegate_real_cplat_argparser_print_usage));
    ON_CALL(*this, cplat_argparser_handle_print_error_messages(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_handle_print_error_messages));
    ON_CALL(*this, cplat_argparser_print_error_messages(_))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_print_error_messages));
    ON_CALL(*this, cplat_argparser_handle_get_register_error_count(_))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_handle_get_register_error_count));
    ON_CALL(*this, cplat_argparser_get_register_error_count())
        .WillByDefault(Invoke(delegate_real_cplat_argparser_get_register_error_count));
    ON_CALL(*this, cplat_argparser_handle_get_register_error(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_handle_get_register_error));
    ON_CALL(*this, cplat_argparser_get_register_error(_))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_get_register_error));
    ON_CALL(*this, cplat_argparser_handle_get_register_error_target(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_handle_get_register_error_target));
    ON_CALL(*this, cplat_argparser_get_register_error_target(_))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_get_register_error_target));
    ON_CALL(*this, cplat_argparser_handle_get_register_error_message(_, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_handle_get_register_error_message));
    ON_CALL(*this, cplat_argparser_get_register_error_message(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_get_register_error_message));
    ON_CALL(*this, cplat_argparser_handle_print_register_error_messages(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_handle_print_register_error_messages));
    ON_CALL(*this, cplat_argparser_print_register_error_messages(_))
        .WillByDefault(Invoke(delegate_real_cplat_argparser_print_register_error_messages));

    // cplat/hashtable/hashtable.h
    ON_CALL(*this, cplat_hashtable_required_size(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_hashtable_required_size));
    ON_CALL(*this, cplat_hashtable_create(_, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_hashtable_create));
    ON_CALL(*this, cplat_hashtable_create_growable(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_hashtable_create_growable));
    ON_CALL(*this, cplat_hashtable_attach(_, _, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_hashtable_attach));
    ON_CALL(*this, cplat_hashtable_validate(_)).WillByDefault(Invoke(delegate_real_cplat_hashtable_validate));
    ON_CALL(*this, cplat_hashtable_get_config_ref(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_hashtable_get_config_ref));
    ON_CALL(*this, cplat_hashtable_get_config_val(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_hashtable_get_config_val));
    ON_CALL(*this, cplat_hashtable_buffer_size(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_hashtable_buffer_size));
    ON_CALL(*this, cplat_hashtable_buffer_ref(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_hashtable_buffer_ref));
    ON_CALL(*this, cplat_hashtable_add(_, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_hashtable_add));
    ON_CALL(*this, cplat_hashtable_upsert(_, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_hashtable_upsert));
    ON_CALL(*this, cplat_hashtable_insert_direct(_, _, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_hashtable_insert_direct));
    ON_CALL(*this, cplat_hashtable_update(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_hashtable_update));
    ON_CALL(*this, cplat_hashtable_update_rec(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_hashtable_update_rec));
    ON_CALL(*this, cplat_hashtable_find_value_ref(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_hashtable_find_value_ref));
    ON_CALL(*this, cplat_hashtable_find_value_copy(_, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_hashtable_find_value_copy));
    ON_CALL(*this, cplat_hashtable_find_recno(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_hashtable_find_recno));
    ON_CALL(*this, cplat_hashtable_get_key_ref(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_hashtable_get_key_ref));
    ON_CALL(*this, cplat_hashtable_get_key_copy(_, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_hashtable_get_key_copy));
    ON_CALL(*this, cplat_hashtable_get_value_ref(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_hashtable_get_value_ref));
    ON_CALL(*this, cplat_hashtable_get_value_copy(_, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_hashtable_get_value_copy));
    ON_CALL(*this, cplat_hashtable_get_status(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_hashtable_get_status));
    ON_CALL(*this, cplat_hashtable_next_record(_, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_hashtable_next_record));
    ON_CALL(*this, cplat_hashtable_get_timestamp_ref(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_hashtable_get_timestamp_ref));
    ON_CALL(*this, cplat_hashtable_get_timestamp_val(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_hashtable_get_timestamp_val));
    ON_CALL(*this, cplat_hashtable_get_generation(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_hashtable_get_generation));
    ON_CALL(*this, cplat_hashtable_get_table_timestamp_ref(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_hashtable_get_table_timestamp_ref));
    ON_CALL(*this, cplat_hashtable_get_table_timestamp_val(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_hashtable_get_table_timestamp_val));
    ON_CALL(*this, cplat_hashtable_get_table_generation(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_hashtable_get_table_generation));
    ON_CALL(*this, cplat_hashtable_find_timestamp_ref(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_hashtable_find_timestamp_ref));
    ON_CALL(*this, cplat_hashtable_find_timestamp_val(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_hashtable_find_timestamp_val));
    ON_CALL(*this, cplat_hashtable_find_generation(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_hashtable_find_generation));
    ON_CALL(*this, cplat_hashtable_count_status(_, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_hashtable_count_status));
    ON_CALL(*this, cplat_hashtable_count(_, _)).WillByDefault(Invoke(delegate_real_cplat_hashtable_count));
    ON_CALL(*this, cplat_hashtable_deleted_count(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_hashtable_deleted_count));
    ON_CALL(*this, cplat_hashtable_empty_count(_, _)).WillByDefault(Invoke(delegate_real_cplat_hashtable_empty_count));
    ON_CALL(*this, cplat_hashtable_delete(_, _)).WillByDefault(Invoke(delegate_real_cplat_hashtable_delete));
    ON_CALL(*this, cplat_hashtable_delete_rec(_, _)).WillByDefault(Invoke(delegate_real_cplat_hashtable_delete_rec));
    ON_CALL(*this, cplat_hashtable_push_deleted(_)).WillByDefault(Invoke(delegate_real_cplat_hashtable_push_deleted));
    ON_CALL(*this, cplat_hashtable_purge_deleted(_)).WillByDefault(Invoke(delegate_real_cplat_hashtable_purge_deleted));
    ON_CALL(*this, cplat_hashtable_compact(_)).WillByDefault(Invoke(delegate_real_cplat_hashtable_compact));
    ON_CALL(*this, cplat_hashtable_resize(_, _)).WillByDefault(Invoke(delegate_real_cplat_hashtable_resize));
    ON_CALL(*this, cplat_hashtable_rebuild_into(_, _, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_hashtable_rebuild_into));
    ON_CALL(*this, cplat_hashtable_clear(_)).WillByDefault(Invoke(delegate_real_cplat_hashtable_clear));
    ON_CALL(*this, cplat_hashtable_dispose(_)).WillByDefault(Invoke(delegate_real_cplat_hashtable_dispose));

    // cplat/clock/clock.h
    ON_CALL(*this, cplat_clock_get_monotonic_ms()).WillByDefault(Invoke(delegate_real_cplat_clock_get_monotonic_ms));
    ON_CALL(*this, cplat_clock_get_monotonic(_)).WillByDefault(Invoke(delegate_real_cplat_clock_get_monotonic));
    ON_CALL(*this, cplat_clock_get_realtime(_)).WillByDefault(Invoke(delegate_real_cplat_clock_get_realtime));
    ON_CALL(*this, cplat_clock_format_realtime_iso8601_local(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_clock_format_realtime_iso8601_local));
    ON_CALL(*this, cplat_clock_format_realtime_iso8601_utc(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_clock_format_realtime_iso8601_utc));
    ON_CALL(*this, cplat_clock_get_realtime_utc(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_clock_get_realtime_utc));
    ON_CALL(*this, cplat_clock_get_realtime_deadline_ms(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_clock_get_realtime_deadline_ms));

    // cplat/clock/timespec.h
    ON_CALL(*this, cplat_timespec_normalize(_)).WillByDefault(Invoke(delegate_real_cplat_timespec_normalize));
    ON_CALL(*this, cplat_timespec_add(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_timespec_add));
    ON_CALL(*this, cplat_timespec_sub(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_timespec_sub));
    ON_CALL(*this, cplat_timespec_cmp(_, _)).WillByDefault(Invoke(delegate_real_cplat_timespec_cmp));
    ON_CALL(*this, cplat_timespec_add_ms(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_timespec_add_ms));
    ON_CALL(*this, cplat_timespec_diff_ms(_, _)).WillByDefault(Invoke(delegate_real_cplat_timespec_diff_ms));
    ON_CALL(*this, cplat_timespec_to_native(_, _)).WillByDefault(Invoke(delegate_real_cplat_timespec_to_native));
    ON_CALL(*this, cplat_timespec_from_native(_, _)).WillByDefault(Invoke(delegate_real_cplat_timespec_from_native));

    // cplat/compress/compress.h
    ON_CALL(*this, cplat_compress(_, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_compress));
    ON_CALL(*this, cplat_decompress(_, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_decompress));

    // cplat/console/console.h
    ON_CALL(*this, cplat_console_init()).WillByDefault(Invoke(delegate_real_cplat_console_init));
    ON_CALL(*this, cplat_console_dispose()).WillByDefault(Invoke(delegate_real_cplat_console_dispose));
    ON_CALL(*this, cplat_console_attach_parent(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_console_attach_parent));
    ON_CALL(*this, cplat_console_write(_, _)).WillByDefault(Invoke(delegate_real_cplat_console_write));

    // cplat/console/console.h (internal)
    ON_CALL(*this, cplat_internal_console_dispose_on_shutdown(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_console_dispose_on_shutdown));

    // cplat/crt/fcntl.h
    ON_CALL(*this, cplat_open(_, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_open));
    ON_CALL(*this, cplat_open_fmt(_, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_open_fmt));
    ON_CALL(*this, cplat_vopen_fmt(_, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_open_fmt));

    // cplat/crt/file.h
    ON_CALL(*this, cplat_file_init(_)).WillByDefault(Invoke(delegate_real_cplat_file_init));
    ON_CALL(*this, cplat_file_open(_, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_file_open));
    ON_CALL(*this, cplat_file_write(_, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_file_write));
    ON_CALL(*this, cplat_file_read(_, _, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_file_read));
    ON_CALL(*this, cplat_file_get_size(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_file_get_size));
    ON_CALL(*this, cplat_file_set_size(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_file_set_size));
    ON_CALL(*this, cplat_file_get_id(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_file_get_id));
    ON_CALL(*this, cplat_file_get_path_id(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_file_get_path_id));
    ON_CALL(*this, cplat_file_get_modified_timestamp(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_file_get_modified_timestamp));
    ON_CALL(*this, cplat_file_set_modified_timestamp(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_file_set_modified_timestamp));
    ON_CALL(*this, cplat_file_get_path_modified_timestamp(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_file_get_path_modified_timestamp));
    ON_CALL(*this, cplat_file_set_path_modified_timestamp(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_file_set_path_modified_timestamp));
    ON_CALL(*this, cplat_file_flush(_, _)).WillByDefault(Invoke(delegate_real_cplat_file_flush));
    ON_CALL(*this, cplat_file_close(_, _)).WillByDefault(Invoke(delegate_real_cplat_file_close));

    // cplat/crt/path.h
    ON_CALL(*this, cplat_path_normalize_sep(_)).WillByDefault(Invoke(delegate_real_cplat_path_normalize_sep));
    ON_CALL(*this, cplat_path_get_full(_, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_path_get_full));
    ON_CALL(*this, cplat_path_equal(_, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_path_equal));
    ON_CALL(*this, cplat_path_get_temp_dir(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_path_get_temp_dir));
    ON_CALL(*this, cplat_path_concat_n(_, _, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_vpath_concat_n));
    ON_CALL(*this, cplat_vpath_concat_n(_, _, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_vpath_concat_n));
    ON_CALL(*this, cplat_path_basename(_)).WillByDefault(Invoke(delegate_real_cplat_path_basename));
    ON_CALL(*this, cplat_path_dirname(_, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_path_dirname));
    ON_CALL(*this, cplat_path_extension(_)).WillByDefault(Invoke(delegate_real_cplat_path_extension));
    ON_CALL(*this, cplat_path_strip_extension(_, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_path_strip_extension));
    ON_CALL(*this, cplat_path_join_n(_, _, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_vpath_join_n));
    ON_CALL(*this, cplat_vpath_join_n(_, _, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_vpath_join_n));

    // cplat/crt/stdio.h
    ON_CALL(*this, cplat_fopen(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_fopen));
    ON_CALL(*this, cplat_freopen(_, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_freopen));
    ON_CALL(*this, cplat_fclose(_, _)).WillByDefault(Invoke(delegate_real_cplat_fclose));
    ON_CALL(*this, cplat_fflush(_, _)).WillByDefault(Invoke(delegate_real_cplat_fflush));
    ON_CALL(*this, cplat_fread(_, _, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_fread));
    ON_CALL(*this, cplat_fwrite(_, _, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_fwrite));
    ON_CALL(*this, cplat_remove(_, _)).WillByDefault(Invoke(delegate_real_cplat_remove));
    ON_CALL(*this, cplat_rename(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_rename));
    ON_CALL(*this, cplat_scanf(_, _)).WillByDefault(Invoke(delegate_real_cplat_scanf));
    ON_CALL(*this, cplat_vscanf(_, _)).WillByDefault(Invoke(delegate_real_cplat_vscanf));
    ON_CALL(*this, cplat_fscanf(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_fscanf));
    ON_CALL(*this, cplat_vfscanf(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_vfscanf));
    ON_CALL(*this, cplat_snprintf(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_snprintf));
    ON_CALL(*this, cplat_vsnprintf(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_snprintf));
    ON_CALL(*this, cplat_fgets(_, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_fgets));
    ON_CALL(*this, cplat_fprintf(_, _)).WillByDefault(Invoke(delegate_real_cplat_fprintf));
    ON_CALL(*this, cplat_vfprintf(_, _)).WillByDefault(Invoke(delegate_real_cplat_fprintf));
    ON_CALL(*this, cplat_fseek(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_fseek));
    ON_CALL(*this, cplat_ftell(_)).WillByDefault(Invoke(delegate_real_cplat_ftell));
    ON_CALL(*this, cplat_fopen_fmt(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_fopen_fmt));
    ON_CALL(*this, cplat_vfopen_fmt(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_fopen_fmt));
    ON_CALL(*this, cplat_remove_fmt(_, _)).WillByDefault(Invoke(delegate_real_cplat_remove_fmt));
    ON_CALL(*this, cplat_vremove_fmt(_, _)).WillByDefault(Invoke(delegate_real_cplat_remove_fmt));
    ON_CALL(*this, cplat_fopen_temp(_, _, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_fopen_temp));

    // cplat/crt/stdlib.h
    ON_CALL(*this, cplat_getenv(_, _, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_getenv));
    ON_CALL(*this, cplat_setenv(_, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_setenv));
    ON_CALL(*this, cplat_unsetenv(_, _)).WillByDefault(Invoke(delegate_real_cplat_unsetenv));
    ON_CALL(*this, cplat_parse_int64(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_parse_int64));
    ON_CALL(*this, cplat_parse_uint64(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_parse_uint64));
    ON_CALL(*this, cplat_parse_int(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_parse_int));
    ON_CALL(*this, cplat_parse_double(_, _)).WillByDefault(Invoke(delegate_real_cplat_parse_double));
    ON_CALL(*this, cplat_malloc(_)).WillByDefault(Invoke(delegate_real_cplat_malloc));
    ON_CALL(*this, cplat_malloc_zerofill(_)).WillByDefault(Invoke(delegate_real_cplat_malloc_zerofill));
    ON_CALL(*this, cplat_calloc(_, _)).WillByDefault(Invoke(delegate_real_cplat_calloc));
    ON_CALL(*this, cplat_realloc(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_realloc));
    ON_CALL(*this, cplat_realloc_zerofill(_, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_realloc_zerofill));
    ON_CALL(*this, cplat_free(_)).WillByDefault(Invoke(delegate_real_cplat_free));

    // cplat/crt/string.h
    ON_CALL(*this, cplat_strcpy(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_strcpy));
    ON_CALL(*this, cplat_strncpy(_, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_strncpy));
    ON_CALL(*this, cplat_strcat(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_strcat));
    ON_CALL(*this, cplat_strncat(_, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_strncat));
    ON_CALL(*this, cplat_strtok_r(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_strtok_r));
    ON_CALL(*this, cplat_strdup(_)).WillByDefault(Invoke(delegate_real_cplat_strdup));
    ON_CALL(*this, cplat_strcasecmp(_, _)).WillByDefault(Invoke(delegate_real_cplat_strcasecmp));
    ON_CALL(*this, cplat_strncasecmp(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_strncasecmp));
    ON_CALL(*this, cplat_wcscpy(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_wcscpy));
    ON_CALL(*this, cplat_sscanf(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_sscanf));
    ON_CALL(*this, cplat_vsscanf(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_vsscanf));

    // cplat/crt/sys/stat.h
    ON_CALL(*this, cplat_stat(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_stat));
    ON_CALL(*this, cplat_mkdir(_, _)).WillByDefault(Invoke(delegate_real_cplat_mkdir));
    ON_CALL(*this, cplat_makedirs(_, _)).WillByDefault(Invoke(delegate_real_cplat_makedirs));
    ON_CALL(*this, cplat_rmdir(_, _)).WillByDefault(Invoke(delegate_real_cplat_rmdir));
    ON_CALL(*this, cplat_stat_fmt(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_stat_fmt));
    ON_CALL(*this, cplat_vstat_fmt(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_stat_fmt));
    ON_CALL(*this, cplat_mkdir_fmt(_, _)).WillByDefault(Invoke(delegate_real_cplat_mkdir_fmt));
    ON_CALL(*this, cplat_vmkdir_fmt(_, _)).WillByDefault(Invoke(delegate_real_cplat_mkdir_fmt));
    ON_CALL(*this, cplat_file_stat_is_regular(_)).WillByDefault(Invoke(delegate_real_cplat_file_stat_is_regular));

    // cplat/crt/time.h
    ON_CALL(*this, cplat_gmtime(_, _)).WillByDefault(Invoke(delegate_real_cplat_gmtime));
    ON_CALL(*this, cplat_localtime(_, _)).WillByDefault(Invoke(delegate_real_cplat_localtime));
    ON_CALL(*this, cplat_ctime(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_ctime));

    // cplat/crt/unistd.h
    ON_CALL(*this, cplat_isatty(_)).WillByDefault(Invoke(delegate_real_cplat_isatty));
    ON_CALL(*this, cplat_lseek(_, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_lseek));
    ON_CALL(*this, cplat_close(_, _)).WillByDefault(Invoke(delegate_real_cplat_close));
    ON_CALL(*this, cplat_dup(_, _)).WillByDefault(Invoke(delegate_real_cplat_dup));
    ON_CALL(*this, cplat_dup2(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_dup2));
    ON_CALL(*this, cplat_read(_, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_read));
    ON_CALL(*this, cplat_write(_, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_write));
    ON_CALL(*this, cplat_access(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_access));
    ON_CALL(*this, cplat_access_fmt(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_access_fmt));
    ON_CALL(*this, cplat_vaccess_fmt(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_access_fmt));

    // cplat/crypto/crypto.h
    ON_CALL(*this, cplat_crypto_encrypt(_, _, _, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_crypto_encrypt));
    ON_CALL(*this, cplat_crypto_decrypt(_, _, _, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_crypto_decrypt));
    ON_CALL(*this, cplat_passphrase_to_key(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_passphrase_to_key));

    // cplat/base/error.h
    ON_CALL(*this, cplat_error_clear(_)).WillByDefault(Invoke(delegate_real_cplat_error_clear));
    ON_CALL(*this, cplat_error_capture_errno(_, _)).WillByDefault(Invoke(delegate_real_cplat_error_capture_errno));
    ON_CALL(*this, cplat_error_capture_current_errno(_))
        .WillByDefault(Invoke(delegate_real_cplat_error_capture_current_errno));
    ON_CALL(*this, cplat_error_get_last(_)).WillByDefault(Invoke(delegate_real_cplat_error_get_last));
    ON_CALL(*this, cplat_error_set_last(_)).WillByDefault(Invoke(delegate_real_cplat_error_set_last));
    ON_CALL(*this, cplat_error_clear_last()).WillByDefault(Invoke(delegate_real_cplat_error_clear_last));
    ON_CALL(*this, cplat_error_is_set(_)).WillByDefault(Invoke(delegate_real_cplat_error_is_set));
    ON_CALL(*this, cplat_error_get_domain(_)).WillByDefault(Invoke(delegate_real_cplat_error_get_domain));
    ON_CALL(*this, cplat_error_get_errno(_)).WillByDefault(Invoke(delegate_real_cplat_error_get_errno));
    ON_CALL(*this, cplat_error_to_result(_)).WillByDefault(Invoke(delegate_real_cplat_error_to_result));
    ON_CALL(*this, cplat_error_get_cause(_)).WillByDefault(Invoke(delegate_real_cplat_error_get_cause));
    ON_CALL(*this, cplat_error_is(_, _)).WillByDefault(Invoke(delegate_real_cplat_error_is));

    // cplat/base/error_message.h
    ON_CALL(*this, cplat_result_to_string(_)).WillByDefault(Invoke(delegate_real_cplat_result_to_string));
    ON_CALL(*this, cplat_error_message(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_error_message));

    // cplat/crypto/random.h
    ON_CALL(*this, cplat_random_bytes(_, _)).WillByDefault(Invoke(delegate_real_cplat_random_bytes));

    // cplat/locale/ui_language.h
    ON_CALL(*this, cplat_ui_language_get_tag(_, _)).WillByDefault(Invoke(delegate_real_cplat_ui_language_get_tag));

    // cplat/mmap/mmap.h
    ON_CALL(*this, cplat_mmap_attach(_, _, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_mmap_attach));
    ON_CALL(*this, cplat_mmap_get_address(_)).WillByDefault(Invoke(delegate_real_cplat_mmap_get_address));
    ON_CALL(*this, cplat_mmap_get_size(_)).WillByDefault(Invoke(delegate_real_cplat_mmap_get_size));
    ON_CALL(*this, cplat_mmap_flush(_, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_mmap_flush));
    ON_CALL(*this, cplat_mmap_detach(_, _)).WillByDefault(Invoke(delegate_real_cplat_mmap_detach));

    // cplat/net/byteorder.h
    ON_CALL(*this, cplat_hton16(_)).WillByDefault(Invoke(delegate_real_cplat_hton16));
    ON_CALL(*this, cplat_ntoh16(_)).WillByDefault(Invoke(delegate_real_cplat_ntoh16));
    ON_CALL(*this, cplat_hton32(_)).WillByDefault(Invoke(delegate_real_cplat_hton32));
    ON_CALL(*this, cplat_ntoh32(_)).WillByDefault(Invoke(delegate_real_cplat_ntoh32));
    ON_CALL(*this, cplat_hton64(_)).WillByDefault(Invoke(delegate_real_cplat_hton64));
    ON_CALL(*this, cplat_ntoh64(_)).WillByDefault(Invoke(delegate_real_cplat_ntoh64));

    // cplat/net/endpoint.h
    ON_CALL(*this, cplat_ipv4_parse(_, _)).WillByDefault(Invoke(delegate_real_cplat_ipv4_parse));
    ON_CALL(*this, cplat_ipv4_resolve(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_ipv4_resolve));
    ON_CALL(*this, cplat_ipv4_to_string(_, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_ipv4_to_string));

    // cplat/net/socket.h
    ON_CALL(*this, cplat_socket_open(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_socket_open));
    ON_CALL(*this, cplat_socket_close(_)).WillByDefault(Invoke(delegate_real_cplat_socket_close));
    ON_CALL(*this, cplat_socket_shutdown(_)).WillByDefault(Invoke(delegate_real_cplat_socket_shutdown));
    ON_CALL(*this, cplat_socket_bind(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_socket_bind));
    ON_CALL(*this, cplat_socket_listen(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_socket_listen));
    ON_CALL(*this, cplat_socket_accept(_, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_socket_accept));
    ON_CALL(*this, cplat_socket_connect(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_socket_connect));
    ON_CALL(*this, cplat_socket_get_pending_error(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_socket_get_pending_error));
    ON_CALL(*this, cplat_socket_set_nonblocking(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_socket_set_nonblocking));
    ON_CALL(*this, cplat_socket_set_reuse_address(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_socket_set_reuse_address));
    ON_CALL(*this, cplat_socket_set_broadcast(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_socket_set_broadcast));
    ON_CALL(*this, cplat_socket_set_multicast_interface(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_socket_set_multicast_interface));
    ON_CALL(*this, cplat_socket_join_multicast_group(_, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_socket_join_multicast_group));
    ON_CALL(*this, cplat_socket_leave_multicast_group(_, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_socket_leave_multicast_group));
    ON_CALL(*this, cplat_socket_send(_, _, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_socket_send));
    ON_CALL(*this, cplat_socket_recv(_, _, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_socket_recv));
    ON_CALL(*this, cplat_socket_sendto(_, _, _, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_socket_sendto));
    ON_CALL(*this, cplat_socket_recvfrom(_, _, _, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_socket_recvfrom));
    ON_CALL(*this, cplat_socket_send_all(_, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_socket_send_all));
    ON_CALL(*this, cplat_socket_recv_all(_, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_socket_recv_all));
    ON_CALL(*this, cplat_socket_wait_readable(_, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_socket_wait_readable));
    ON_CALL(*this, cplat_socket_wait_writable(_, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_socket_wait_writable));
    ON_CALL(*this, cplat_socket_wait_readable_multi(_, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_socket_wait_readable_multi));
    ON_CALL(*this, cplat_socket_shutdown_receive(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_socket_shutdown_receive));

    // cplat/prompt/pinned_prompt.h
    ON_CALL(*this, cplat_pinned_prompt_create(_)).WillByDefault(Invoke(delegate_real_cplat_pinned_prompt_create));
    ON_CALL(*this, cplat_pinned_prompt_dispose(_)).WillByDefault(Invoke(delegate_real_cplat_pinned_prompt_dispose));
    ON_CALL(*this, cplat_pinned_prompt_readline_at(_, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_pinned_prompt_readline_at));
    ON_CALL(*this, cplat_pinned_prompt_readline_with_initial_at(_, _, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_pinned_prompt_readline_with_initial_at));
    ON_CALL(*this, cplat_pinned_prompt_readline_fmt_at(_, _, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_pinned_prompt_readline_fmt_at));
    ON_CALL(*this, cplat_pinned_prompt_write(_, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_pinned_prompt_write));
    ON_CALL(*this, cplat_pinned_prompt_printf(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_pinned_prompt_printf));
    ON_CALL(*this, cplat_pinned_prompt_status_enable(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_pinned_prompt_status_enable));
    ON_CALL(*this, cplat_pinned_prompt_status_set(_, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_pinned_prompt_status_set));

    // cplat/prompt/prompt.h
    ON_CALL(*this, cplat_prompt_create(_)).WillByDefault(Invoke(delegate_real_cplat_prompt_create));
    ON_CALL(*this, cplat_prompt_dispose(_)).WillByDefault(Invoke(delegate_real_cplat_prompt_dispose));
    ON_CALL(*this, cplat_prompt_readline_at(_, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_prompt_readline_at));
    ON_CALL(*this, cplat_prompt_readline_with_initial_at(_, _, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_prompt_readline_with_initial_at));
    ON_CALL(*this, cplat_prompt_readline_fmt_at(_, _, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_prompt_readline_fmt_at));

    // cplat/runtime/elevated_process.h
    ON_CALL(*this, cplat_elevated_process_is_elevated(_))
        .WillByDefault(Invoke(delegate_real_cplat_elevated_process_is_elevated));
    ON_CALL(*this, cplat_elevated_process_run_if_needed(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_elevated_process_run_if_needed));
    ON_CALL(*this, cplat_elevated_process_run_with_result(_, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_elevated_process_run_with_result));
    ON_CALL(*this, cplat_elevated_process_extract_result_target(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_elevated_process_extract_result_target));
    ON_CALL(*this, cplat_elevated_process_report_result(_))
        .WillByDefault(Invoke(delegate_real_cplat_elevated_process_report_result));
    ON_CALL(*this, cplat_elevated_process_run_piped(_, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_elevated_process_run_piped));
    ON_CALL(*this, cplat_elevated_process_attach_output_pipes(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_elevated_process_attach_output_pipes));

    // cplat/regex/regex.h
    ON_CALL(*this, cplat_regex_create(_, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_regex_create));
    ON_CALL(*this, cplat_regex_dispose(_)).WillByDefault(Invoke(delegate_real_cplat_regex_dispose));
    ON_CALL(*this, cplat_regex_get_group_count(_)).WillByDefault(Invoke(delegate_real_cplat_regex_get_group_count));
    ON_CALL(*this, cplat_regex_search(_, _, _, _, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_regex_search));
    ON_CALL(*this, cplat_regex_matches(_, _, _, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_regex_matches));
    ON_CALL(*this, cplat_regex_replace(_, _, _, _, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_regex_replace));
    ON_CALL(*this, cplat_regex_iter_create(_, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_regex_iter_create));
    ON_CALL(*this, cplat_regex_iter_next(_, _, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_regex_iter_next));
    ON_CALL(*this, cplat_regex_iter_dispose(_)).WillByDefault(Invoke(delegate_real_cplat_regex_iter_dispose));
    ON_CALL(*this, cplat_regex_split(_, _, _, _, _, _, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_regex_split));

    // cplat/runtime/memory_lock.h
    ON_CALL(*this, cplat_memory_lock_range(_, _)).WillByDefault(Invoke(delegate_real_cplat_memory_lock_range));
    ON_CALL(*this, cplat_memory_unlock_range(_, _)).WillByDefault(Invoke(delegate_real_cplat_memory_unlock_range));
    ON_CALL(*this, cplat_memory_lock_self(_, _)).WillByDefault(Invoke(delegate_real_cplat_memory_lock_self));
    ON_CALL(*this, cplat_memory_lock_scope_release(_))
        .WillByDefault(Invoke(delegate_real_cplat_memory_lock_scope_release));
    ON_CALL(*this, cplat_secure_zero(_, _)).WillByDefault(Invoke(delegate_real_cplat_secure_zero));

    // cplat/runtime/host.h
    ON_CALL(*this, cplat_host_get_name(_, _)).WillByDefault(Invoke(delegate_real_cplat_host_get_name));

    // cplat/runtime/module.h
    ON_CALL(*this, cplat_module_get_path(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_module_get_path));
    ON_CALL(*this, cplat_module_get_basename(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_module_get_basename));

    // cplat/runtime/process.h
    ON_CALL(*this, cplat_process_get_executable_path(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_process_get_executable_path));
    ON_CALL(*this, cplat_process_get_pid()).WillByDefault(Invoke(delegate_real_cplat_process_get_pid));
    ON_CALL(*this, cplat_process_get_tid()).WillByDefault(Invoke(delegate_real_cplat_process_get_tid));
    ON_CALL(*this, cplat_process_start(_, _)).WillByDefault(Invoke(delegate_real_cplat_process_start));
    ON_CALL(*this, cplat_process_wait(_, _)).WillByDefault(Invoke(delegate_real_cplat_process_wait));
    ON_CALL(*this, cplat_process_get_exit_code(_, _)).WillByDefault(Invoke(delegate_real_cplat_process_get_exit_code));
    ON_CALL(*this, cplat_process_terminate(_)).WillByDefault(Invoke(delegate_real_cplat_process_terminate));
    ON_CALL(*this, cplat_process_dispose(_)).WillByDefault(Invoke(delegate_real_cplat_process_dispose));
    ON_CALL(*this, cplat_process_run_sync(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_process_run_sync));

    // cplat/runtime/shutdown.h
    ON_CALL(*this, cplat_shutdown_register(_, _)).WillByDefault(Invoke(delegate_real_cplat_shutdown_register));
    ON_CALL(*this, cplat_shutdown_request_register(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_shutdown_request_register));
    ON_CALL(*this, cplat_exit(_)).WillByDefault(Invoke(delegate_real_cplat_exit));
    ON_CALL(*this, cplat_shutdown_invoke_for_test(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_shutdown_invoke_for_test));
    ON_CALL(*this, cplat_shutdown_request_invoke_for_test(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_shutdown_request_invoke_for_test));
    ON_CALL(*this, cplat_shutdown_reset_for_test()).WillByDefault(Invoke(delegate_real_cplat_shutdown_reset_for_test));

    // cplat/runtime/sym_loader.h
    ON_CALL(*this, cplat_sym_loader_resolve(_)).WillByDefault(Invoke(delegate_real_cplat_sym_loader_resolve));
    ON_CALL(*this, cplat_sym_loader_is_default(_)).WillByDefault(Invoke(delegate_real_cplat_sym_loader_is_default));
    ON_CALL(*this, cplat_sym_loader_init(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_sym_loader_init));
    ON_CALL(*this, cplat_sym_loader_dispose(_, _)).WillByDefault(Invoke(delegate_real_cplat_sym_loader_dispose));
    ON_CALL(*this, cplat_sym_loader_info(_, _)).WillByDefault(Invoke(delegate_real_cplat_sym_loader_info));

    // cplat/string_catalog/string_catalog.h
    ON_CALL(*this, cplat_string_catalog_set_language(_))
        .WillByDefault(Invoke(delegate_real_cplat_string_catalog_set_language));
    ON_CALL(*this, cplat_string_catalog_get_language())
        .WillByDefault(Invoke(delegate_real_cplat_string_catalog_get_language));
    ON_CALL(*this, cplat_string_catalog_language_from_tag(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_string_catalog_language_from_tag));
    ON_CALL(*this, cplat_string_catalog_format(_, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_string_catalog_vformat));
    ON_CALL(*this, cplat_string_catalog_vformat(_, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_string_catalog_vformat));
    ON_CALL(*this, cplat_string_catalog_verify(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_string_catalog_verify));
    ON_CALL(*this, cplat_string_catalog_get_entry(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_string_catalog_get_entry));
    ON_CALL(*this, cplat_string_catalog_get_category(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_string_catalog_get_category));
    ON_CALL(*this, cplat_string_catalog_get_id(_, _)).WillByDefault(Invoke(delegate_real_cplat_string_catalog_get_id));
    ON_CALL(*this, cplat_string_catalog_get_note(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_string_catalog_get_note));

    // cplat/string_catalog/filter.h
    ON_CALL(*this, cplat_string_catalog_filter_compile(_, _, _, _, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_string_catalog_filter_compile));
    ON_CALL(*this, cplat_string_catalog_filter_compile_line(_, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_string_catalog_filter_compile_line));
    ON_CALL(*this, cplat_string_catalog_filter_insert_line(_, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_string_catalog_filter_insert_line));
    ON_CALL(*this, cplat_string_catalog_filter_remove_line(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_string_catalog_filter_remove_line));
    ON_CALL(*this, cplat_string_catalog_filter_validate(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_string_catalog_filter_validate));
    ON_CALL(*this, cplat_string_catalog_filter_get_info(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_string_catalog_filter_get_info));
    ON_CALL(*this, cplat_string_catalog_filter_decompile_line(_, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_string_catalog_filter_decompile_line));
    ON_CALL(*this, cplat_string_catalog_filter_slot_create(_, _, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_string_catalog_filter_slot_create));
    ON_CALL(*this, cplat_string_catalog_filter_slot_dispose(_))
        .WillByDefault(Invoke(delegate_real_cplat_string_catalog_filter_slot_dispose));
    ON_CALL(*this, cplat_string_catalog_filter_slot_get_catalog(_))
        .WillByDefault(Invoke(delegate_real_cplat_string_catalog_filter_slot_get_catalog));
    ON_CALL(*this, cplat_string_catalog_filter_get_catalog_id(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_string_catalog_filter_get_catalog_id));
    ON_CALL(*this, cplat_string_catalog_filter_source_publish(_, _, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_string_catalog_filter_source_publish));
    ON_CALL(*this, cplat_string_catalog_filter_source_get_info(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_string_catalog_filter_source_get_info));
    ON_CALL(*this, cplat_string_catalog_filter_slot_attach_source(_, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_string_catalog_filter_slot_attach_source));
    ON_CALL(*this, cplat_string_catalog_filter_slot_get_source_status(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_string_catalog_filter_slot_get_source_status));
    ON_CALL(*this, cplat_string_catalog_filter_slot_apply(_, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_string_catalog_filter_slot_apply));
    ON_CALL(*this, cplat_string_catalog_filter_slot_check(_, _, _, _, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_string_catalog_filter_slot_check));
    ON_CALL(*this, cplat_string_catalog_filter_slot_snapshot(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_string_catalog_filter_slot_snapshot));
    ON_CALL(*this, cplat_string_catalog_filter_slot_get_line_error(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_string_catalog_filter_slot_get_line_error));
    ON_CALL(*this, cplat_string_catalog_filter_slot_describe_line(_, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_string_catalog_filter_slot_describe_line));
    ON_CALL(*this, cplat_string_catalog_filter_slot_test(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_string_catalog_filter_slot_test));
    ON_CALL(*this, cplat_string_catalog_filter_slot_vformat(_, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_string_catalog_filter_slot_vformat));
    ON_CALL(*this, cplat_string_catalog_filter_slot_format(_, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_string_catalog_filter_slot_vformat));

    // cplat/sync/sync.h
    ON_CALL(*this, cplat_local_lock_create(_)).WillByDefault(Invoke(delegate_real_cplat_local_lock_create));
    ON_CALL(*this, cplat_local_lock_lock(_, _)).WillByDefault(Invoke(delegate_real_cplat_local_lock_lock));
    ON_CALL(*this, cplat_local_lock_try_lock(_)).WillByDefault(Invoke(delegate_real_cplat_local_lock_try_lock));
    ON_CALL(*this, cplat_local_lock_unlock(_)).WillByDefault(Invoke(delegate_real_cplat_local_lock_unlock));
    ON_CALL(*this, cplat_local_lock_dispose(_)).WillByDefault(Invoke(delegate_real_cplat_local_lock_dispose));
    ON_CALL(*this, cplat_condvar_create(_)).WillByDefault(Invoke(delegate_real_cplat_condvar_create));
    ON_CALL(*this, cplat_condvar_wait(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_condvar_wait));
    ON_CALL(*this, cplat_condvar_signal(_)).WillByDefault(Invoke(delegate_real_cplat_condvar_signal));
    ON_CALL(*this, cplat_condvar_broadcast(_)).WillByDefault(Invoke(delegate_real_cplat_condvar_broadcast));
    ON_CALL(*this, cplat_condvar_dispose(_)).WillByDefault(Invoke(delegate_real_cplat_condvar_dispose));
    ON_CALL(*this, cplat_local_rwlock_create(_)).WillByDefault(Invoke(delegate_real_cplat_local_rwlock_create));
    ON_CALL(*this, cplat_local_rwlock_lock_shared(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_local_rwlock_lock_shared));
    ON_CALL(*this, cplat_local_rwlock_try_lock_shared(_))
        .WillByDefault(Invoke(delegate_real_cplat_local_rwlock_try_lock_shared));
    ON_CALL(*this, cplat_local_rwlock_lock_exclusive(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_local_rwlock_lock_exclusive));
    ON_CALL(*this, cplat_local_rwlock_try_lock_exclusive(_))
        .WillByDefault(Invoke(delegate_real_cplat_local_rwlock_try_lock_exclusive));
    ON_CALL(*this, cplat_local_rwlock_unlock_shared(_))
        .WillByDefault(Invoke(delegate_real_cplat_local_rwlock_unlock_shared));
    ON_CALL(*this, cplat_local_rwlock_unlock_exclusive(_))
        .WillByDefault(Invoke(delegate_real_cplat_local_rwlock_unlock_exclusive));
    ON_CALL(*this, cplat_local_rwlock_dispose(_)).WillByDefault(Invoke(delegate_real_cplat_local_rwlock_dispose));
    ON_CALL(*this, cplat_thread_create(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_thread_create));
    ON_CALL(*this, cplat_thread_join(_, _)).WillByDefault(Invoke(delegate_real_cplat_thread_join));
    ON_CALL(*this, cplat_thread_detach(_)).WillByDefault(Invoke(delegate_real_cplat_thread_detach));
    ON_CALL(*this, cplat_interprocess_lock_open(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_interprocess_lock_open));
    ON_CALL(*this, cplat_interprocess_lock_import_descriptor(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_interprocess_lock_import_descriptor));
    ON_CALL(*this, cplat_interprocess_lock_export_descriptor(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_interprocess_lock_export_descriptor));
    ON_CALL(*this, cplat_interprocess_lock_lock(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_interprocess_lock_lock));
    ON_CALL(*this, cplat_interprocess_lock_try_lock(_))
        .WillByDefault(Invoke(delegate_real_cplat_interprocess_lock_try_lock));
    ON_CALL(*this, cplat_interprocess_lock_unlock(_))
        .WillByDefault(Invoke(delegate_real_cplat_interprocess_lock_unlock));
    ON_CALL(*this, cplat_interprocess_lock_dispose(_))
        .WillByDefault(Invoke(delegate_real_cplat_interprocess_lock_dispose));
    ON_CALL(*this, cplat_interprocess_rwlock_open(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_interprocess_rwlock_open));
    ON_CALL(*this, cplat_interprocess_rwlock_import_descriptor(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_interprocess_rwlock_import_descriptor));
    ON_CALL(*this, cplat_interprocess_rwlock_export_descriptor(_, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_interprocess_rwlock_export_descriptor));
    ON_CALL(*this, cplat_interprocess_rwlock_lock_shared(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_interprocess_rwlock_lock_shared));
    ON_CALL(*this, cplat_interprocess_rwlock_try_lock_shared(_))
        .WillByDefault(Invoke(delegate_real_cplat_interprocess_rwlock_try_lock_shared));
    ON_CALL(*this, cplat_interprocess_rwlock_lock_exclusive(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_interprocess_rwlock_lock_exclusive));
    ON_CALL(*this, cplat_interprocess_rwlock_try_lock_exclusive(_))
        .WillByDefault(Invoke(delegate_real_cplat_interprocess_rwlock_try_lock_exclusive));
    ON_CALL(*this, cplat_interprocess_rwlock_unlock(_))
        .WillByDefault(Invoke(delegate_real_cplat_interprocess_rwlock_unlock));
    ON_CALL(*this, cplat_interprocess_rwlock_dispose(_))
        .WillByDefault(Invoke(delegate_real_cplat_interprocess_rwlock_dispose));
    ON_CALL(*this, cplat_call_once(_, _)).WillByDefault(Invoke(delegate_real_cplat_call_once));
    ON_CALL(*this, cplat_sleep_ms(_)).WillByDefault(Invoke(delegate_real_cplat_sleep_ms));

    // cplat/trace/trace_file.h
    ON_CALL(*this, cplat_trace_file_sink_create(_, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_trace_file_sink_create));
    ON_CALL(*this, cplat_trace_file_sink_write(_, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_trace_file_sink_write));
    ON_CALL(*this, cplat_trace_file_sink_dispose(_)).WillByDefault(Invoke(delegate_real_cplat_trace_file_sink_dispose));

    // cplat/trace/tracer.h
    ON_CALL(*this, cplat_tracer_create(_)).WillByDefault(Invoke(delegate_real_cplat_tracer_create));
    ON_CALL(*this, cplat_tracer_start(_)).WillByDefault(Invoke(delegate_real_cplat_tracer_start));
    ON_CALL(*this, cplat_tracer_stop(_)).WillByDefault(Invoke(delegate_real_cplat_tracer_stop));
    ON_CALL(*this, cplat_tracer_get_state(_)).WillByDefault(Invoke(delegate_real_cplat_tracer_get_state));
    ON_CALL(*this, cplat_tracer_write_at(_, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_tracer_write_at));
    ON_CALL(*this, cplat_tracer_writef_at(_, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_tracer_writef_at));
    ON_CALL(*this, cplat_tracer_vwritef_at(_, _, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_tracer_vwritef_at));
    ON_CALL(*this, cplat_tracer_write_hex_at(_, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_tracer_write_hex_at));
    ON_CALL(*this, cplat_tracer_write_hexf_at(_, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_tracer_write_hexf_at));
    ON_CALL(*this, cplat_tracer_vwrite_hexf_at(_, _, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_tracer_vwrite_hexf_at));
    ON_CALL(*this, cplat_tracer_hex_sep(_)).WillByDefault(Invoke(delegate_real_cplat_tracer_hex_sep));
    ON_CALL(*this, cplat_tracer_hex_msg(_)).WillByDefault(Invoke(delegate_real_cplat_tracer_hex_msg));
    ON_CALL(*this, cplat_tracer_write_with_source(_, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_tracer_write_with_source));
    ON_CALL(*this, cplat_tracer_set_name(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_tracer_set_name));
    ON_CALL(*this, cplat_tracer_get_name(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_tracer_get_name));
    ON_CALL(*this, cplat_tracer_get_identifier(_)).WillByDefault(Invoke(delegate_real_cplat_tracer_get_identifier));
    ON_CALL(*this, cplat_tracer_set_file_name(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_tracer_set_file_name));
    ON_CALL(*this, cplat_tracer_get_file_name(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_tracer_get_file_name));
    ON_CALL(*this, cplat_tracer_get_file_identifier(_))
        .WillByDefault(Invoke(delegate_real_cplat_tracer_get_file_identifier));
    ON_CALL(*this, cplat_tracer_get_os_level(_)).WillByDefault(Invoke(delegate_real_cplat_tracer_get_os_level));
    ON_CALL(*this, cplat_tracer_set_os_level(_, _)).WillByDefault(Invoke(delegate_real_cplat_tracer_set_os_level));
    ON_CALL(*this, cplat_tracer_get_etw_level(_)).WillByDefault(Invoke(delegate_real_cplat_tracer_get_etw_level));
    ON_CALL(*this, cplat_tracer_set_etw_level(_, _)).WillByDefault(Invoke(delegate_real_cplat_tracer_set_etw_level));
    ON_CALL(*this, cplat_tracer_get_file_level(_)).WillByDefault(Invoke(delegate_real_cplat_tracer_get_file_level));
    ON_CALL(*this, cplat_tracer_set_file_level(_, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_tracer_set_file_level));
    ON_CALL(*this, cplat_tracer_get_stderr_level(_)).WillByDefault(Invoke(delegate_real_cplat_tracer_get_stderr_level));
    ON_CALL(*this, cplat_tracer_set_stderr_level(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_tracer_set_stderr_level));
    ON_CALL(*this, cplat_tracer_dispose(_)).WillByDefault(Invoke(delegate_real_cplat_tracer_dispose));
    ON_CALL(*this, cplat_tracer_set_hook(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_tracer_set_hook));
    ON_CALL(*this, cplat_tracer_remove_hook(_, _)).WillByDefault(Invoke(delegate_real_cplat_tracer_remove_hook));
    ON_CALL(*this, cplat_tracer_call_next_hook(_, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_tracer_call_next_hook));

#if defined(PLATFORM_WINDOWS)

    // cplat/crt/wchar_conv.h
    ON_CALL(*this, cplat_utf8_to_wpath(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_utf8_to_wpath));
    ON_CALL(*this, cplat_utf8_to_wstr(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_utf8_to_wstr));
    ON_CALL(*this, cplat_wpath_to_utf8(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_wpath_to_utf8));
    ON_CALL(*this, cplat_wstr_to_utf8(_, _, _)).WillByDefault(Invoke(delegate_real_cplat_wstr_to_utf8));
    ON_CALL(*this, cplat_utf8_to_wstr_alloc(_)).WillByDefault(Invoke(delegate_real_cplat_utf8_to_wstr_alloc));
    ON_CALL(*this, cplat_wstr_to_utf8_alloc(_)).WillByDefault(Invoke(delegate_real_cplat_wstr_to_utf8_alloc));

    // cplat/base/error.h
    ON_CALL(*this, cplat_error_capture_windows_error(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_error_capture_windows_error));
    ON_CALL(*this, cplat_error_capture_current_windows_error(_))
        .WillByDefault(Invoke(delegate_real_cplat_error_capture_current_windows_error));
    ON_CALL(*this, cplat_error_get_windows_error(_)).WillByDefault(Invoke(delegate_real_cplat_error_get_windows_error));

    // cplat/trace/etw.h
    ON_CALL(*this, cplat_etw_provider_create(_)).WillByDefault(Invoke(delegate_real_cplat_etw_provider_create));
    ON_CALL(*this, cplat_etw_provider_write(_, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_etw_provider_write));
    ON_CALL(*this, cplat_etw_provider_dispose(_)).WillByDefault(Invoke(delegate_real_cplat_etw_provider_dispose));
    ON_CALL(*this, cplat_etw_session_check_access())
        .WillByDefault(Invoke(delegate_real_cplat_etw_session_check_access));
    ON_CALL(*this, cplat_etw_session_start(_, _, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_etw_session_start));
    ON_CALL(*this, cplat_etw_session_stop(_)).WillByDefault(Invoke(delegate_real_cplat_etw_session_stop));

    // cplat/trace/eventlog.h
    ON_CALL(*this, cplat_eventlog_sink_create(_)).WillByDefault(Invoke(delegate_real_cplat_eventlog_sink_create));
    ON_CALL(*this, cplat_eventlog_sink_write(_, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_cplat_eventlog_sink_write));
    ON_CALL(*this, cplat_eventlog_sink_dispose(_)).WillByDefault(Invoke(delegate_real_cplat_eventlog_sink_dispose));
    ON_CALL(*this, cplat_eventlog_register_source(_, _))
        .WillByDefault(Invoke(delegate_real_cplat_eventlog_register_source));
    ON_CALL(*this, cplat_eventlog_unregister_source(_))
        .WillByDefault(Invoke(delegate_real_cplat_eventlog_unregister_source));

    // cplat/win32/win32.h
    ON_CALL(*this, CreateFileU(_, _, _, _, _, _, _)).WillByDefault(Invoke(delegate_real_CreateFileU));
    ON_CALL(*this, CreateNamedPipeU(_, _, _, _, _, _, _, _)).WillByDefault(Invoke(delegate_real_CreateNamedPipeU));
    ON_CALL(*this, GetModuleFileNameU(_, _, _)).WillByDefault(Invoke(delegate_real_GetModuleFileNameU));
    ON_CALL(*this, WriteConsoleU(_, _, _, _, _)).WillByDefault(Invoke(delegate_real_WriteConsoleU));
    ON_CALL(*this, GetVolumePathNameU(_, _, _)).WillByDefault(Invoke(delegate_real_GetVolumePathNameU));
    ON_CALL(*this, GetVolumeInformationU(_, _, _, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_GetVolumeInformationU));
    ON_CALL(*this, LoadLibraryU(_)).WillByDefault(Invoke(delegate_real_LoadLibraryU));
    ON_CALL(*this, CreateProcessU(_, _, _, _, _, _, _, _, _, _)).WillByDefault(Invoke(delegate_real_CreateProcessU));
    ON_CALL(*this, OpenSCManagerU(_, _, _)).WillByDefault(Invoke(delegate_real_OpenSCManagerU));
    ON_CALL(*this, CreateServiceU(_, _, _, _, _, _, _, _, _, _, _, _, _))
        .WillByDefault(Invoke(delegate_real_CreateServiceU));
    ON_CALL(*this, OpenServiceU(_, _, _)).WillByDefault(Invoke(delegate_real_OpenServiceU));
    ON_CALL(*this, ChangeServiceConfig2U(_, _, _)).WillByDefault(Invoke(delegate_real_ChangeServiceConfig2U));
    ON_CALL(*this, RegisterServiceCtrlHandlerExU(_, _, _))
        .WillByDefault(Invoke(delegate_real_RegisterServiceCtrlHandlerExU));
    ON_CALL(*this, StartServiceCtrlDispatcherU(_)).WillByDefault(Invoke(delegate_real_StartServiceCtrlDispatcherU));
#endif /* PLATFORM_WINDOWS */

#if defined(PLATFORM_LINUX)

    // cplat/trace/syslog.h
    ON_CALL(*this, cplat_syslog_sink_create(_, _)).WillByDefault(Invoke(delegate_real_cplat_syslog_sink_create));
    ON_CALL(*this, cplat_syslog_sink_write(_, _, _, _)).WillByDefault(Invoke(delegate_real_cplat_syslog_sink_write));
    ON_CALL(*this, cplat_syslog_sink_rename(_, _)).WillByDefault(Invoke(delegate_real_cplat_syslog_sink_rename));
    ON_CALL(*this, cplat_syslog_sink_dispose(_)).WillByDefault(Invoke(delegate_real_cplat_syslog_sink_dispose));
#endif /* PLATFORM_LINUX */

    TESTFW_REGISTER_MOCK_INSTANCE(_mock_cplat);
}

Mock_cplat::~Mock_cplat()
{
    TESTFW_UNREGISTER_MOCK_INSTANCE(_mock_cplat);
}
