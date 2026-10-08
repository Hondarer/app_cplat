/**
 *******************************************************************************
 *  @file           hashtable-required-size.c
 *  @brief          ハッシュ テーブル構築に必要なバッファー サイズを表示します。
 *  @author         Tetsuo Honda
 *  @date           2026/08/18
 *  @version        1.0.0
 *
 *  capacity / key_size / value_size から @ref cplat_hashtable_required_size
 *  の結果(管理領域サイズとデータ領域サイズ)を、空白区切りで標準出力へ 1 行で出力します。\n
 *  既定はテーブル横断のタイムスタンプのみです。\n
 *  `--record-timestamp` を指定するとレコード単位のタイムスタンプと世代カウンターも持ち、
 *  管理領域が増加します。\n
 *  `--value-align` を指定すると固定長値をその境界へ整列させ、データ領域が増加します。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/argparser/argparser.h>
#include <cplat/base/result.h>
#include <cplat/console/console.h>
#include <cplat/hashtable/hashtable.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct hashtable_required_size_options
{
    int capacity;
    int key_size;
    int value_size;
    int key_storage_size;
    int value_storage_size;
    int value_align;
    int variable_key;
    int variable_value;
    int need_help;
    int record_timestamp;
} hashtable_required_size_options;

static int register_options(hashtable_required_size_options *options)
{
    (void)cplat_argparser_register_flag("-h", "--help", "show this help", &options->need_help);
    (void)cplat_argparser_register_option_int("-c", "--capacity", "N", "number of slots", CPLAT_ARGPARSER_REQUIRED,
                                              &options->capacity);
    (void)cplat_argparser_register_option_int("-k", "--key-size", "N", "bytes per fixed key", 0, &options->key_size);
    (void)cplat_argparser_register_option_int("-v", "--value-size", "N", "bytes per fixed value", 0,
                                              &options->value_size);
    (void)cplat_argparser_register_option_int(NULL, "--key-storage-size", "N", "bytes in variable key storage", 0,
                                              &options->key_storage_size);
    (void)cplat_argparser_register_option_int(NULL, "--value-storage-size", "N", "bytes in variable value storage", 0,
                                              &options->value_storage_size);
    (void)cplat_argparser_register_flag(NULL, "--variable-key", "store variable strings as keys",
                                        &options->variable_key);
    (void)cplat_argparser_register_flag(NULL, "--variable-value", "store variable strings as values",
                                        &options->variable_value);
    (void)cplat_argparser_register_option_int(
        NULL, "--value-align", "N", "alignment boundary for fixed values (0 packs them)", 0, &options->value_align);
    (void)cplat_argparser_register_flag(NULL, "--record-timestamp",
                                        "include per-record timestamps and generations in the management region",
                                        &options->record_timestamp);
    if (cplat_argparser_get_register_error_count() > 0)
    {
        (void)cplat_argparser_print_register_error_messages(stderr);
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    hashtable_required_size_options options = {0};
    cplat_hashtable_config config = {0};
    size_t mgmt_size = 0;
    size_t data_size = 0;
    int ret;

    cplat_console_init();

    cplat_argparser_init(argc, argv,
                         "Print the management-region and data-region buffer sizes required to construct a hash "
                         "table.");
    if (register_options(&options) != 0)
    {
        return EXIT_FAILURE;
    }

    ret = cplat_argparser_parse();
    if (options.need_help != 0)
    {
        (void)cplat_argparser_print_usage(stdout);
        return EXIT_SUCCESS;
    }
    if (ret != CPLAT_OK)
    {
        (void)cplat_argparser_print_error_messages(stderr);
        (void)cplat_argparser_print_usage(stderr);
        return EXIT_FAILURE;
    }

    if (options.value_align < 0)
    {
        (void)fprintf(stderr, "value alignment must not be negative.\n");
        return EXIT_FAILURE;
    }
    if ((options.capacity <= 0) ||
        (((options.variable_key == 0) && (options.key_size <= 0)) ||
         ((options.variable_key != 0) && (options.key_storage_size <= 0))) ||
        (((options.variable_value == 0) && (options.value_size <= 0)) ||
         ((options.variable_value != 0) && (options.value_storage_size <= 0))))
    {
        (void)fprintf(stderr, "capacity and the selected field sizes must be positive.\n");
        return EXIT_FAILURE;
    }

    config.capacity = (size_t)options.capacity;
    if (options.variable_key != 0)
    {
        config.key_type = CPLAT_HASHTABLE_FIELD_VARIABLE_STRING;
        config.key_storage_size = (size_t)options.key_storage_size;
    }
    else
    {
        config.key_type = CPLAT_HASHTABLE_FIELD_FIXED_STRING;
        config.key_size = (size_t)options.key_size;
    }
    if (options.variable_value != 0)
    {
        config.value_type = CPLAT_HASHTABLE_FIELD_VARIABLE_STRING;
        config.value_storage_size = (size_t)options.value_storage_size;
    }
    else
    {
        config.value_type = CPLAT_HASHTABLE_FIELD_FIXED_BINARY;
        config.value_size = (size_t)options.value_size;
        config.value_align = (size_t)options.value_align;
    }
    if (options.record_timestamp != 0)
    {
        config.timestamp_scope = CPLAT_HASHTABLE_TIMESTAMP_SCOPE_RECORD;
    }
    else
    {
        config.timestamp_scope = CPLAT_HASHTABLE_TIMESTAMP_SCOPE_TABLE;
    }
    config.lifetime = 2;

    ret = cplat_hashtable_required_size(&config, &mgmt_size, &data_size);
    if (ret != CPLAT_OK)
    {
        (void)fprintf(stderr, "failed to compute required size: %d\n", ret);
        return EXIT_FAILURE;
    }

    if (printf("%zu %zu\n", mgmt_size, data_size) < 0)
    {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
