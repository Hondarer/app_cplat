/**
 *******************************************************************************
 *  @file           string-catalog-filter-required-size.c
 *  @brief          条件式フィルターのソース領域に必要なバイト数を表示します。
 *  @author         Tetsuo Honda
 *  @date           2026/10/03
 *  @version        1.0.0
 *
 *  行数の上限と行幅から、ソース領域 (@ref CPLAT_STRING_CATALOG_FILTER_SOURCE_SIZE) と
 *  フィルター オブジェクト (@ref CPLAT_STRING_CATALOG_FILTER_IMAGE_SIZE) のバイト数を求め、
 *  空白区切りで標準出力へ 1 行で出力します。\n
 *  共有メモリやファイルなど、ソース領域を利用側で確保する際の大きさの見積もりに使います。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/argparser/argparser.h>
#include <cplat/base/result.h>
#include <cplat/console/console.h>
#include <cplat/string_catalog/filter.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct filter_required_size_options
{
    int line_capacity;
    int line_width;
    int need_help;
} filter_required_size_options;

static int register_options(filter_required_size_options *options)
{
    (void)cplat_argparser_register_flag("-h", "--help", "show this help", &options->need_help);
    (void)cplat_argparser_register_option_int("-l", "--line-capacity", "N",
                                              "maximum number of condition lines (1-1024)", CPLAT_ARGPARSER_REQUIRED,
                                              &options->line_capacity);
    (void)cplat_argparser_register_option_int("-w", "--line-width", "N", "bytes per condition line (8-1024)",
                                              CPLAT_ARGPARSER_REQUIRED, &options->line_width);
    if (cplat_argparser_get_register_error_count() > 0)
    {
        (void)cplat_argparser_print_register_error_messages(stderr);
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    filter_required_size_options options = {0};
    int ret;

    cplat_console_init();

    cplat_argparser_init(argc, argv,
                         "Print the source-region and filter-image sizes in bytes required for a string catalog "
                         "filter.");
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

    /* スロットの作成と同じ範囲に限る。範囲外の値で求めた大きさは、どのスロットにも結び付けられないため */
    if ((options.line_capacity < 1) || (options.line_capacity > (int)CPLAT_STRING_CATALOG_FILTER_LINE_MAX))
    {
        (void)fprintf(stderr, "line capacity must be between 1 and %u.\n", CPLAT_STRING_CATALOG_FILTER_LINE_MAX);
        return EXIT_FAILURE;
    }
    if ((options.line_width < (int)CPLAT_STRING_CATALOG_FILTER_LINE_WIDTH_MIN) ||
        (options.line_width > (int)CPLAT_STRING_CATALOG_FILTER_LINE_WIDTH_MAX))
    {
        (void)fprintf(stderr, "line width must be between %u and %u.\n", CPLAT_STRING_CATALOG_FILTER_LINE_WIDTH_MIN,
                      CPLAT_STRING_CATALOG_FILTER_LINE_WIDTH_MAX);
        return EXIT_FAILURE;
    }

    if (printf("%zu %zu\n", (size_t)CPLAT_STRING_CATALOG_FILTER_SOURCE_SIZE(options.line_capacity, options.line_width),
               (size_t)CPLAT_STRING_CATALOG_FILTER_IMAGE_SIZE(options.line_capacity, options.line_width)) < 0)
    {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
