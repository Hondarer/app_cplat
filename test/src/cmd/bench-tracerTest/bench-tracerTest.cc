#include <testfw.h>
#include <mock_cplat.h>

using namespace testing;

// 数値引数の変換失敗時に測定や worker の処理へ進まないことの確認
TEST(bench_tracerTest, rejects_invalid_numeric_arguments)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    const char *invalid[] = {"", "abc", "999999999999999999999999"};
    int results[12] = {0};
    size_t index = 0;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_tracer_create(_)).Times(0);
    // [Pre-Assert確認_異常系] - 測定用 tracer の生成が呼び出されないこと。
    EXPECT_CALL(mock_cplat, cplat_trace_file_sink_create(_, _, _, _)).Times(0);
    // [Pre-Assert確認_異常系] - worker のファイル シンク生成が呼び出されないこと。

    // Act
    for (const char *text : invalid)
    {
        const char *process_args[] = {"bench-tracer", "unused.log", text, "1"};
        const char *message_args[] = {"bench-tracer", "unused.log", "1", text};
        const char *worker_message_args[] = {"bench-tracer", "--worker", "unused.log", text, "0"};
        const char *worker_buffer_args[] = {"bench-tracer", "--worker", "unused.log", "1", text};
        results[index++] = __real_main(4, const_cast<char **>(process_args));
        results[index++] = __real_main(4, const_cast<char **>(message_args));
        results[index++] = __real_main(5, const_cast<char **>(worker_message_args));
        results[index++] = __real_main(5, const_cast<char **>(worker_buffer_args));
        // [手順] - 空入力、不正文字、範囲超過の各入力を、通常モードと worker の各数値引数へ指定して実行する。
    }

    // Assert
    for (int result : results)
    {
        EXPECT_EQ(1, result); // [確認_異常系 回数=12] - すべての変換失敗が終了値 1 へ反映されること。
    }
}
