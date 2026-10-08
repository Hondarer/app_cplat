/* テスト対象ソース ファイルの注入用追加ソース
 * このソースはテスト対象ソースの末尾に結合されます
 * この static 関数へのアクセサーによって
 * テスト プログラムからテスト対象ソースの static 関数にアクセスできます
 */
#ifndef _IN_TEST_SRC
    #include "argparser.c"
#endif /* _IN_TEST_SRC */

#include "argparser.inject.h"

int test_argparser_is_valid_short_name(const char *name)
{
    return argparser_is_valid_short_name(name);
}

int test_argparser_is_valid_long_name(const char *name)
{
    return argparser_is_valid_long_name(name);
}

void test_argparser_default_dispose_on_shutdown(const cplat_shutdown_event *event, void *context)
{
    argparser_default_dispose_on_shutdown(event, context);
}

cplat_argparser *test_argparser_default_acquire(int argc, char *const *argv, const cplat_argparser_options *options,
                                                int reset_existing)
{
    return argparser_default_acquire(argc, argv, options, reset_existing);
}

void test_argparser_apply_args(cplat_argparser *parser, int argc, char *const *argv)
{
    argparser_apply_args(parser, argc, argv);
}

void test_argparser_reset_default(void)
{
    if (s_default_parser != NULL)
    {
        argparser_dispose_core(s_default_parser);
        s_default_parser = NULL;
    }
    if (s_default_lock != NULL)
    {
        cplat_local_lock_dispose(s_default_lock);
        s_default_lock = NULL;
    }
    cplat_atomic_store_i32(&s_default_initialize_once.state, 0, CPLAT_MEMORY_ORDER_RELAXED);
}

char *test_argparser_replace_program_description(cplat_argparser *parser, char *description)
{
    char *previous = parser->program_description;
    parser->program_description = description;
    return previous;
}

void test_argparser_set_register_error_result(cplat_argparser *parser, size_t index, int result)
{
    parser->register_errors[index].result = result;
}

void test_argparser_set_last_error(cplat_argparser *parser, int result)
{
    parser->last_error = result;
    parser->last_error_target = NULL;
}

void test_argparser_clear_register_error_target(cplat_argparser *parser, size_t index)
{
    parser->register_errors[index].target = NULL;
}

void test_argparser_record_register_result(cplat_argparser *parser, int result)
{
    argparser_record_register_result(parser, result, NULL, NULL);
}

void test_argparser_replace_short_name(cplat_argparser *parser, size_t index, const char *name)
{
    free(parser->specs[index].short_name);
    parser->specs[index].short_name = argparser_strdup(name);
}
