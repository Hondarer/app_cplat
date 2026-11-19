#include <testfw.h>
#include <mock_cplat.h>
#include <mock_stdio.h>

#include <cstdint>
#include <string>

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

// プロセス数とメッセージ数の積が int に収まらないとき、測定へ進まないことの確認
TEST(bench_tracerTest, rejects_product_that_does_not_fit_in_int)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_tracer_create(_)).Times(0);
    // [Pre-Assert確認_異常系] - 測定用 tracer の生成が呼び出されないこと。
    EXPECT_CALL(mock_cplat, cplat_trace_file_sink_create(_, _, _, _)).Times(0);
    // [Pre-Assert確認_異常系] - worker のファイル シンク生成が呼び出されないこと。

    // Act
    const char *args[] = {"bench-tracer", "unused.log", "65536", "65536"};
    int result = __real_main(4, const_cast<char **>(args));
    // [手順] - 各引数は int に収まり、積だけが int の最大値を超える組を実行する。

    // Assert
    EXPECT_EQ(1, result); // [確認_異常系] - 積が int に収まらない入力の終了値が 1 であること。
}

// 測定中の書き込みが失敗した場合に、測定値を出力せずに 1 を返すことの確認
TEST(bench_tracerTest, returns_failure_when_filtered_write_fails)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_stdio> mock_stdio;
    const char *args[] = {"bench-tracer", "unused.log", "1", "2"};

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_tracer_writef_at(_, _, _, _))
        .WillRepeatedly(Return(CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_異常系] - cplat_tracer_writef_at() が呼び出されること。
                                                    // [Pre-Assert手順] - cplat_tracer_writef_at() から CPLAT_ERR_UNKNOWN を返す。
    EXPECT_CALL(mock_stdio, printf(_, _, _, _)).Times(0); // [Pre-Assert確認_異常系] - printf() が呼び出されないこと。(測定値を出力しないことの確認)
    EXPECT_CALL(mock_cplat, cplat_process_start(_, _))
        .Times(0); // [Pre-Assert確認_異常系] - cplat_process_start() が呼び出されないこと。(後続の測定へ進まないことの確認)

    // Act
    int result = __real_main(4, const_cast<char **>(args)); // [手順] - 測定中の書き込みが失敗する状態で __real_main() を呼び出す。

    // Assert
    EXPECT_EQ(1, result); // [確認_異常系] - __real_main() の戻り値が 1 であること。
}
// 測定結果の出力の後の fflush(stdout) が失敗した場合に失敗の終了コードを返し、診断を出すことの確認
TEST(bench_tracerTest, main_returns_failure_when_final_flush_fails)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    NiceMock<Mock_cplat> mock_cplat;
    cplat_process *child = reinterpret_cast<cplat_process *>(static_cast<uintptr_t>(0x1234));
    ON_CALL(mock_cplat, cplat_process_start(_, _))
        .WillByDefault(DoAll(SetArgPointee<1>(child), Return(CPLAT_OK))); // [状態] - cplat_process_start() から偽の子プロセスを格納して CPLAT_OK を返す。
    ON_CALL(mock_cplat, cplat_process_wait(child)).WillByDefault(Return(CPLAT_OK)); // [状態] - cplat_process_wait() から CPLAT_OK を返す。
    ON_CALL(mock_cplat, cplat_process_get_exit_code(child, _))
        .WillByDefault(DoAll(SetArgPointee<1>(0), Return(CPLAT_OK))); // [状態] - cplat_process_get_exit_code() から終了コード 0 を格納して CPLAT_OK を返す。
    ON_CALL(mock_cplat, cplat_process_dispose(child)).WillByDefault(Return()); // [状態] - cplat_process_dispose() から戻る。
    ON_CALL(mock_stdio, printf(_, _, _, _)).WillByDefault(Return(0)); // [状態] - printf() から 0 を返す。
    const char *argv[] = {"bench-tracer", "unused.log", "1", "1"};
    const int argc = (int)(sizeof(argv) / sizeof(argv[0]));

    // Pre-Assert
    EXPECT_CALL(mock_stdio, fflush(_, _, _, stdout))
        .WillOnce(Return(EOF)); // [Pre-Assert確認_異常系] - fflush() が stdout を指定して 1 回呼び出されること。
                                // [Pre-Assert手順] - fflush() から EOF を返す。

    // Act
    testing::internal::CaptureStderr();
    int rc = __real_main(argc, (char **)&argv); // [手順] - フラッシュが失敗する状態で、成功の経路の引数を指定して __real_main() を呼び出す。
    std::string errors = testing::internal::GetCapturedStderr();

    // Assert
    EXPECT_EQ(1, rc); // [確認_異常系] - __real_main() の戻り値が 1 であること。
    EXPECT_THAT(errors, HasSubstr("bench-tracer: failed to write stdout")); // [確認_異常系] - 標準エラー出力に書き込みの失敗の診断が出ること。
}

// 測定結果の出力の後の ferror(stdout) がエラーを示した場合に失敗の終了コードを返し、診断を出すことの確認
TEST(bench_tracerTest, main_returns_failure_when_final_stream_has_error)
{
    // Arrange
    NiceMock<Mock_stdio> mock_stdio;
    NiceMock<Mock_cplat> mock_cplat;
    cplat_process *child = reinterpret_cast<cplat_process *>(static_cast<uintptr_t>(0x1234));
    ON_CALL(mock_cplat, cplat_process_start(_, _))
        .WillByDefault(DoAll(SetArgPointee<1>(child), Return(CPLAT_OK))); // [状態] - cplat_process_start() から偽の子プロセスを格納して CPLAT_OK を返す。
    ON_CALL(mock_cplat, cplat_process_wait(child)).WillByDefault(Return(CPLAT_OK)); // [状態] - cplat_process_wait() から CPLAT_OK を返す。
    ON_CALL(mock_cplat, cplat_process_get_exit_code(child, _))
        .WillByDefault(DoAll(SetArgPointee<1>(0), Return(CPLAT_OK))); // [状態] - cplat_process_get_exit_code() から終了コード 0 を格納して CPLAT_OK を返す。
    ON_CALL(mock_cplat, cplat_process_dispose(child)).WillByDefault(Return()); // [状態] - cplat_process_dispose() から戻る。
    ON_CALL(mock_stdio, printf(_, _, _, _)).WillByDefault(Return(0)); // [状態] - printf() から 0 を返す。
    const char *argv[] = {"bench-tracer", "unused.log", "1", "1"};
    const int argc = (int)(sizeof(argv) / sizeof(argv[0]));

    // Pre-Assert
    EXPECT_CALL(mock_stdio, ferror(_, _, _, stdout))
        .WillOnce(Return(1)); // [Pre-Assert確認_異常系] - ferror() が stdout を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - ferror() から 1 を返す。

    // Act
    testing::internal::CaptureStderr();
    int rc = __real_main(argc, (char **)&argv); // [手順] - ストリームのエラーがある状態で、成功の経路の引数を指定して __real_main() を呼び出す。
    std::string errors = testing::internal::GetCapturedStderr();

    // Assert
    EXPECT_EQ(1, rc); // [確認_異常系] - __real_main() の戻り値が 1 であること。
    EXPECT_THAT(errors, HasSubstr("bench-tracer: failed to write stdout")); // [確認_異常系] - 標準エラー出力に書き込みの失敗の診断が出ること。
}
