/* テスト対象ソース ファイルの注入用追加ソース
 * このソースはテスト対象ソースの末尾に結合されます
 * この static 関数へのアクセサーによって
 * テスト プログラムからテスト対象ソースの static 関数にアクセスできます
 */
#ifndef _IN_TEST_SRC
    #include "trace_file.c"
#endif /* _IN_TEST_SRC */

#include "trace_file.inject.h"

#include <string.h>

cplat_trace_file_sink *test_trace_file_sink_create_unregistered(const char *path, const size_t max_bytes,
                                                                const int generations, const int flags)
{
    return create_new_sink(path, strlen(path), max_bytes, generations, flags);
}
