/**
 *******************************************************************************
 *  @file           argparser-sample.c
 *  @brief          cplat_argparser の動作確認用サンプル コマンドを実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/07/12
 *  @version        1.0.0
 *
 *  フラグ、値付きオプション、複数値オプション、位置引数を登録し、
 *  解析結果を標準出力へ出力します。\n
 *  解析エラー時はエラー メッセージと usage を標準エラー出力へ出力して EXIT_FAILURE で終了します。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include <cplat/argparser/argparser.h>
#include <cplat/console/console.h>
#include <stdio.h>
#include <stdlib.h>

/* 複数値オプション (--include) の最大出現数 */
#define ARGPARSER_SAMPLE_INCLUDE_MAX (4)

/* 解析結果の格納先 */
typedef struct argparser_sample_options
{
    const char *name_value;
    const char *includes[ARGPARSER_SAMPLE_INCLUDE_MAX];
    const char *input_path;
    const char *output_path;
    size_t include_count;
    int need_help;
    int verbose_count;
    int count_value;
    int pad; /* 明示パディング (-Wpadded 対応) */
} argparser_sample_options;

/**
 *  @brief          オプションをパーサーへ登録します。
 *  @param[in,out]     options  解析結果の格納先として各メンバーのアドレスを登録するオプションです。
 *  @return         成功時は 0、登録に失敗した場合は -1 を返します。
 */
static int register_argparser(argparser_sample_options *options)
{
    /*
     * 登録の失敗はパーサーに記録され、直後の cplat_argparser_get_register_error_count() でまとめて検査するため、
     * この関数の登録の関数の戻り値は明示的に破棄する。
     */
    (void)cplat_argparser_register_flag("-h", "--help", "show this help", &options->need_help);
    (void)cplat_argparser_register_flag("-v", "--verbose", "increase verbosity", &options->verbose_count);
    (void)cplat_argparser_register_option_int("-c", "--count", "N", "repeat count", 0, &options->count_value);
    (void)cplat_argparser_register_option_string("-n", "--name", "NAME", "display name", 0, &options->name_value);
    (void)cplat_argparser_register_option_string_array("-i", "--include", "DIR", "include directory", 0,
                                                       options->includes, ARGPARSER_SAMPLE_INCLUDE_MAX,
                                                       &options->include_count);
    (void)cplat_argparser_register_positional_string("input", "input file", CPLAT_ARGPARSER_REQUIRED,
                                                     &options->input_path);
    (void)cplat_argparser_register_positional_string("output", "output file", 0, &options->output_path);

    /* オプションの登録に失敗した場合 (コーディング エラーの場合)。 */
    if (cplat_argparser_get_register_error_count() > 0)
    {
        /* 登録の失敗は直後の -1 で伝え、診断の出力先もほかにないため、戻り値は明示的に破棄する。 */
        (void)cplat_argparser_print_register_error_messages(stderr);
        return -1;
    }

    return 0;
}

/**
 *  @brief          解析結果を標準出力へ表示します。
 *  @param[in]      options  表示する解析結果。
 */
static void print_result(const argparser_sample_options *options)
{
    /*
     * 解析結果の出力の失敗は標準出力のストリームに残り、main が終了の直前にまとめて検査するため、
     * この関数の printf の戻り値は明示的に破棄する。
     */
    (void)printf("verbose: %d\n", options->verbose_count);
    (void)printf("count: %d\n", options->count_value);

    const char *name = options->name_value;
    if (name == NULL)
    {
        name = "(none)";
    }

    (void)printf("name: %s\n", name);

    (void)printf("includes: %zu\n", options->include_count);

    for (size_t i = 0; i < options->include_count; i++)
    {
        (void)printf("  include[%zu]: %s\n", i, options->includes[i]);
    }

    (void)printf("input: %s\n", options->input_path);

    const char *output = options->output_path;
    if (output == NULL)
    {
        output = "(none)";
    }

    (void)printf("output: %s\n", output);
}

int main(int argc, char *argv[])
{
    cplat_console_init();

    argparser_sample_options options = {0};
    options.count_value = 1; /* 0 以外の既定値は解析前に設定する */

    cplat_argparser_init(argc, argv, "cplat_argparser sample");

    if (register_argparser(&options) != 0)
    {
        return EXIT_FAILURE;
    }

    int parse_result = cplat_argparser_parse();

    if (options.need_help != 0)
    {
        /* 必須引数が省略されていても -h, --help を優先する。 */
        if (cplat_argparser_print_usage(stdout) != CPLAT_OK || fflush(stdout) != 0 || ferror(stdout) != 0)
        {
            (void)fprintf(stderr, "error: failed to write stdout\n"); /* 診断の出力先がほかにないため、戻り値は明示的に破棄する */
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }

    if (parse_result != CPLAT_OK)
    {
        /* 終了コードはすでに失敗に決まっているため、この 2 つの表示の関数の戻り値は明示的に破棄する。 */
        (void)cplat_argparser_print_error_messages(stderr);
        (void)cplat_argparser_print_usage(stderr);
        return EXIT_FAILURE;
    }

    print_result(&options);

    if (fflush(stdout) != 0 || ferror(stdout) != 0)
    {
        (void)fprintf(stderr, "error: failed to write stdout\n"); /* 診断の出力先がほかにないため、戻り値は明示的に破棄する */
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
