/* テスト対象ソース ファイルの注入用追加ソース
 * このソースはテスト対象ソースの末尾に結合されます
 * この static 関数へのアクセサーによって
 * テスト プログラムからテスト対象ソースの static 関数にアクセスできます
 */
#ifndef _IN_TEST_SRC
    #include "prompt.c"
#endif /* _IN_TEST_SRC */

#include "prompt.inject.h"

void test_prompt_history_add(cplat_prompt *prompt, prompt_history_context *context, const char *line)
{
    history_add(prompt, context, line);
}

void test_prompt_history_prev(cplat_prompt *prompt, prompt_history_context *context, const char *prompt_string)
{
    history_browse_prev(prompt, context, prompt_string);
}

void test_prompt_history_next(cplat_prompt *prompt, prompt_history_context *context, const char *prompt_string)
{
    history_browse_next(prompt, context, prompt_string);
}

prompt_history_context *test_prompt_find_or_create_context(cplat_prompt *prompt, const char *file, int line)
{
    return find_or_create_ctx(prompt, file, line);
}
