/* テスト対象ソース ファイルの注入用追加ヘッダー
 * このヘッダーをテスト プログラムが参照することで
 * テスト プログラムからテスト対象ソースの static 関数にアクセスできます
 */
#ifndef TRACE_FILE_INJECT_H
#define TRACE_FILE_INJECT_H

#include <stddef.h>

#include <cplat/trace/trace_file.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    extern cplat_trace_file_sink *test_trace_file_sink_create_unregistered(const char *path, size_t max_bytes,
                                                                           int generations, int flags);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* TRACE_FILE_INJECT_H */
