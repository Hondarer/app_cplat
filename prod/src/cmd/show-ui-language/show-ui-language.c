/**
 *******************************************************************************
 *  @file           show-ui-language.c
 *  @brief          実行環境が示す表示言語を言語タグで表示します。
 *  @author         Tetsuo Honda
 *  @date           2026/09/21
 *  @version        1.0.0
 *
 *  @ref cplat_ui_language_get_tag が返す言語タグを、標準出力へ 1 行で出力します。\n
 *  表示言語がニュートラルの場合、言語タグは空文字列であるため、空行を出力します。
 *
 *  環境変数と OS の設定のどちらが採用されたかは出力しません。\n
 *  決定の順序は、ロケールの機能仕様を参照してください。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/argparser/argparser.h>
#include <cplat/base/error_message.h>
#include <cplat/base/result.h>
#include <cplat/console/console.h>
#include <cplat/locale/ui_language.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    char tag[CPLAT_UI_LANGUAGE_TAG_MAX];
    int need_help = 0;
    int ret;

    /* 後始末はシャットダウン コールバックが行うため、cplat_console_dispose は呼び出さない。 */
    cplat_console_init();

    cplat_argparser_init(argc, argv, "Print the display language of the current environment as a language tag.");
    (void)cplat_argparser_register_flag("-h", "--help", "show this help", &need_help);

    if (cplat_argparser_get_register_error_count() > 0)
    {
        (void)cplat_argparser_print_register_error_messages(stderr);
        return EXIT_FAILURE;
    }

    ret = cplat_argparser_parse();

    if (need_help != 0)
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

    ret = cplat_ui_language_get_tag(tag, sizeof(tag));
    if (ret != CPLAT_OK)
    {
        (void)fprintf(stderr, "failed to get the display language. (%s)\n", cplat_result_to_string(ret));
        return EXIT_FAILURE;
    }

    /* ニュートラルは空文字列で表すため、この場合は空行を出力する。 */
    (void)printf("%s\n", tag);

    return EXIT_SUCCESS;
}
