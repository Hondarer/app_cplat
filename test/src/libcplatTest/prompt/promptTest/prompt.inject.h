/* テスト対象ソース ファイルの注入用追加ヘッダー
 * このヘッダーをテスト プログラムが参照することで
 * テスト プログラムからテスト対象ソースの static 関数にアクセスできます
 */
#ifndef PROMPT_INJECT_H
#define PROMPT_INJECT_H

#include <cplat/prompt/prompt_internal.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /* prompt.c の履歴操作へ直接アクセスする。 */
    extern void test_prompt_history_add(cplat_prompt *prompt, cplat_internal_prompt_ctx *context, const char *line);
    extern void test_prompt_history_prev(cplat_prompt *prompt, cplat_internal_prompt_ctx *context,
                                         const char *prompt_string);
    extern void test_prompt_history_next(cplat_prompt *prompt, cplat_internal_prompt_ctx *context,
                                         const char *prompt_string);

    /* prompt.c の呼び出し位置別コンテキスト検索へ直接アクセスする。 */
    extern cplat_internal_prompt_ctx *test_prompt_find_or_create_context(cplat_prompt *prompt, const char *file,
                                                                         int line);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* PROMPT_INJECT_H */
