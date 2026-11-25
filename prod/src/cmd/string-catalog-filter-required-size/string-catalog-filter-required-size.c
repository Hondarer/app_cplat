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

#include <cplat/crt/stdio.h>
#include <cplat/argparser/argparser.h>
#include <cplat/base/result.h>
#include <cplat/console/console.h>
#include <cplat/string_catalog/filter.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct filter_required_size_options
{
    int line_capacity; /* 条件行数の上限 */
    int line_width;    /* 条件行 1 行あたりのバイト数 [byte] */
    int need_help;
} filter_required_size_options;

/*
 * コマンドライン オプションの定義を登録する。
 * 成功時は 0、登録エラー発生時は -1 を返す。
 */
static int register_options(filter_required_size_options *options)
{
    char capacity_help[128];
    char width_help[128];

    if (cplat_snprintf(capacity_help, sizeof(capacity_help), "maximum number of condition lines (1-%u)",
                       CPLAT_STRING_CATALOG_FILTER_LINE_MAX) != CPLAT_OK ||
        cplat_snprintf(width_help, sizeof(width_help), "bytes per condition line (%u-%u)",
                       CPLAT_STRING_CATALOG_FILTER_LINE_WIDTH_MIN,
                       CPLAT_STRING_CATALOG_FILTER_LINE_WIDTH_MAX) != CPLAT_OK)
    {
        return -1;
    }

    /*
     * 登録の失敗はパーサーに記録され、直後の cplat_argparser_get_register_error_count() でまとめて検査するため、
     * この関数の登録の関数の戻り値は明示的に破棄する。
     */
    (void)cplat_argparser_register_flag("-h", "--help", "show this help", &options->need_help);
    (void)cplat_argparser_register_option_int("-l", "--line-capacity", "N", capacity_help, CPLAT_ARGPARSER_REQUIRED,
                                              &options->line_capacity);
    (void)cplat_argparser_register_option_int("-w", "--line-width", "N", width_help, CPLAT_ARGPARSER_REQUIRED,
                                              &options->line_width);

    if (cplat_argparser_get_register_error_count() > 0)
    {
        /* 登録の失敗は直後の -1 で伝え、診断の出力先もほかにないため、戻り値は明示的に破棄する。 */
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

    if (cplat_argparser_init(argc, argv,
                             "Print the source-region and filter-image sizes in bytes required for a string catalog "
                             "filter.") != CPLAT_OK)
    {
        /* 診断の出力先がほかにないため、戻り値は明示的に破棄する。 */
        (void)fprintf(stderr, "引数の解析の準備に失敗しました。\n");
        return EXIT_FAILURE;
    }

    if (register_options(&options) != 0)
    {
        return EXIT_FAILURE;
    }

    ret = cplat_argparser_parse();

    if (options.need_help != 0)
    {
        if (cplat_argparser_print_usage(stdout) != CPLAT_OK || fflush(stdout) != 0 || ferror(stdout) != 0)
        {
            (void)fprintf(stderr, "failed to write stdout.\n"); /* 診断の出力先がほかにないため、戻り値は明示的に破棄する */
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }

    if (ret != CPLAT_OK)
    {
        /* 終了コードはすでに失敗に決まっているため、この 2 つの表示の関数の戻り値は明示的に破棄する。 */
        (void)cplat_argparser_print_error_messages(stderr);
        (void)cplat_argparser_print_usage(stderr);
        return EXIT_FAILURE;
    }

    /* スロットの作成と同じ範囲に限る。範囲外の値で求めた大きさは、どのスロットにも結び付けられないためである。 */
    if ((options.line_capacity < 1) || (options.line_capacity > (int)CPLAT_STRING_CATALOG_FILTER_LINE_MAX))
    {
        /* 診断の出力先がほかにないため、戻り値は明示的に破棄する。 */
        (void)fprintf(stderr, "line capacity must be between 1 and %u.\n", CPLAT_STRING_CATALOG_FILTER_LINE_MAX);
        return EXIT_FAILURE;
    }

    if ((options.line_width < (int)CPLAT_STRING_CATALOG_FILTER_LINE_WIDTH_MIN) ||
        (options.line_width > (int)CPLAT_STRING_CATALOG_FILTER_LINE_WIDTH_MAX))
    {
        /* 診断の出力先がほかにないため、戻り値は明示的に破棄する。 */
        (void)fprintf(stderr, "line width must be between %u and %u.\n", CPLAT_STRING_CATALOG_FILTER_LINE_WIDTH_MIN,
                      CPLAT_STRING_CATALOG_FILTER_LINE_WIDTH_MAX);
        return EXIT_FAILURE;
    }

    /* ソース領域とフィルター オブジェクトに必要なバイト数を標準出力へ出力する。 */
    if (printf("%zu %zu\n", (size_t)CPLAT_STRING_CATALOG_FILTER_SOURCE_SIZE(options.line_capacity, options.line_width),
               (size_t)CPLAT_STRING_CATALOG_FILTER_IMAGE_SIZE(options.line_capacity, options.line_width)) < 0)
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
