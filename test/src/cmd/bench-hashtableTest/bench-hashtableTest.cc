#include <testfw.h>
#include <mock_stdio.h>
#include <mock_cplat.h>

#include <cstdio>
#include <cstdlib>
#include <string>

namespace
{

// CSV の出力先を置く一時ディレクトリを返す。
std::string temp_dir()
{
#if defined(_WIN32)
    char *dir = nullptr;
    size_t length = 0;
    std::string result = ".";

    if (_dupenv_s(&dir, &length, "TEMP") == 0 && dir != nullptr && dir[0] != '\0')
    {
        result = dir;
    }
    free(dir);
    return result;
#else
    return "/tmp";
#endif
}

// 実関数でストリームを閉じてから、閉じる処理の失敗 (EOF) を返す。
int close_then_fail(const char *file, const int line, const char *func, FILE *stream)
{
    (void)delegate_real_fclose(file, line, func, stream);
    return EOF;
}

} // namespace

class bench_hashtableTest : public Test
{
  protected:
    bench_hashtableTest()
    {
        // bench-hashtable.c の単体テストでは shutdown.c を対象外とし、登録 API をフェイクする。
        ON_CALL(mock_cplat_, cplat_shutdown_register(_, _)).WillByDefault(Return(CPLAT_OK));
    }

    NiceMock<Mock_stdio> mock_stdio_;
    NiceMock<Mock_cplat> mock_cplat_;
};
// --help の使い方の表示の関数が失敗した場合に失敗の終了コードを返し、診断を出すことの確認
TEST_F(bench_hashtableTest, main_returns_failure_when_help_usage_print_fails)
{
    // Arrange
    const char *argv[] = {"bench-hashtable", "--help"};
    const int argc = (int)(sizeof(argv) / sizeof(argv[0]));

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_argparser_print_usage(stdout))
        .WillOnce(Return(CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_異常系] - cplat_argparser_print_usage() が stdout を指定して 1 回呼び出されること。
                                              // [Pre-Assert手順] - cplat_argparser_print_usage() から CPLAT_ERR_UNKNOWN を返す。

    // Act
    testing::internal::CaptureStderr();
    int rc = __real_main(argc, (char **)&argv); // [手順] - 使い方の表示が失敗する状態で --help を指定して __real_main() を呼び出す。
    std::string errors = testing::internal::GetCapturedStderr();

    // Assert
    EXPECT_EQ(EXIT_FAILURE, rc); // [確認_異常系] - __real_main() の戻り値が EXIT_FAILURE であること。
    EXPECT_THAT(errors, HasSubstr("failed to write stdout.")); // [確認_異常系] - 標準エラー出力に書き込みの失敗の診断が出ること。
}

// --help の後の fflush(stdout) が失敗した場合に失敗の終了コードを返し、診断を出すことの確認
TEST_F(bench_hashtableTest, main_returns_failure_when_help_flush_fails)
{
    // Arrange
    const char *argv[] = {"bench-hashtable", "--help"};
    const int argc = (int)(sizeof(argv) / sizeof(argv[0]));

    // Pre-Assert
    EXPECT_CALL(mock_stdio_, fflush(_, _, _, stdout))
        .WillOnce(Return(EOF)); // [Pre-Assert確認_異常系] - fflush() が stdout を指定して 1 回呼び出されること。
                                // [Pre-Assert手順] - fflush() から EOF を返す。

    // Act
    testing::internal::CaptureStderr();
    int rc = __real_main(argc, (char **)&argv); // [手順] - フラッシュが失敗する状態で --help を指定して __real_main() を呼び出す。
    std::string errors = testing::internal::GetCapturedStderr();

    // Assert
    EXPECT_EQ(EXIT_FAILURE, rc); // [確認_異常系] - __real_main() の戻り値が EXIT_FAILURE であること。
    EXPECT_THAT(errors, HasSubstr("failed to write stdout.")); // [確認_異常系] - 標準エラー出力に書き込みの失敗の診断が出ること。
}

// --help の後の ferror(stdout) がエラーを示した場合に失敗の終了コードを返し、診断を出すことの確認
TEST_F(bench_hashtableTest, main_returns_failure_when_help_stream_has_error)
{
    // Arrange
    const char *argv[] = {"bench-hashtable", "--help"};
    const int argc = (int)(sizeof(argv) / sizeof(argv[0]));

    // Pre-Assert
    EXPECT_CALL(mock_stdio_, ferror(_, _, _, stdout))
        .WillOnce(Return(1)); // [Pre-Assert確認_異常系] - ferror() が stdout を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - ferror() から 1 を返す。

    // Act
    testing::internal::CaptureStderr();
    int rc = __real_main(argc, (char **)&argv); // [手順] - ストリームのエラーがある状態で --help を指定して __real_main() を呼び出す。
    std::string errors = testing::internal::GetCapturedStderr();

    // Assert
    EXPECT_EQ(EXIT_FAILURE, rc); // [確認_異常系] - __real_main() の戻り値が EXIT_FAILURE であること。
    EXPECT_THAT(errors, HasSubstr("failed to write stdout.")); // [確認_異常系] - 標準エラー出力に書き込みの失敗の診断が出ること。
}

// 表の出力の後の fflush(stdout) が失敗した場合に失敗の終了コードを返し、診断を出すことの確認
TEST_F(bench_hashtableTest, main_returns_failure_when_final_flush_fails)
{
    // Arrange
    const char *argv[] = {"bench-hashtable", "--max-capacity", "1"};
    const int argc = (int)(sizeof(argv) / sizeof(argv[0]));

    // Pre-Assert
    EXPECT_CALL(mock_stdio_, fflush(_, _, _, stdout))
        .WillOnce(Return(EOF)); // [Pre-Assert確認_異常系] - fflush() が stdout を指定して 1 回呼び出されること。
                                // [Pre-Assert手順] - fflush() から EOF を返す。

    // Act
    testing::internal::CaptureStderr();
    int rc = __real_main(argc, (char **)&argv); // [手順] - フラッシュが失敗する状態で、成功の経路の引数を指定して __real_main() を呼び出す。
    std::string errors = testing::internal::GetCapturedStderr();

    // Assert
    EXPECT_EQ(EXIT_FAILURE, rc); // [確認_異常系] - __real_main() の戻り値が EXIT_FAILURE であること。
    EXPECT_THAT(errors, HasSubstr("failed to write stdout.")); // [確認_異常系] - 標準エラー出力に書き込みの失敗の診断が出ること。
}

// 表の出力の後の ferror(stdout) がエラーを示した場合に失敗の終了コードを返し、診断を出すことの確認
TEST_F(bench_hashtableTest, main_returns_failure_when_final_stream_has_error)
{
    // Arrange
    const char *argv[] = {"bench-hashtable", "--max-capacity", "1"};
    const int argc = (int)(sizeof(argv) / sizeof(argv[0]));

    // Pre-Assert
    EXPECT_CALL(mock_stdio_, ferror(_, _, _, stdout))
        .WillOnce(Return(1)); // [Pre-Assert確認_異常系] - ferror() が stdout を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - ferror() から 1 を返す。

    // Act
    testing::internal::CaptureStderr();
    int rc = __real_main(argc, (char **)&argv); // [手順] - ストリームのエラーがある状態で、成功の経路の引数を指定して __real_main() を呼び出す。
    std::string errors = testing::internal::GetCapturedStderr();

    // Assert
    EXPECT_EQ(EXIT_FAILURE, rc); // [確認_異常系] - __real_main() の戻り値が EXIT_FAILURE であること。
    EXPECT_THAT(errors, HasSubstr("failed to write stdout.")); // [確認_異常系] - 標準エラー出力に書き込みの失敗の診断が出ること。
}

// CSV の書き込みの失敗を閉じる前の ferror() で検出し、EXIT_FAILURE と診断を返すことの確認
TEST_F(bench_hashtableTest, main_returns_failure_when_csv_stream_has_error)
{
    // Arrange
    std::string csv_path = temp_dir() + "/bench-hashtableTest.csv"; // [状態] - 一時ディレクトリに CSV の出力先を用意する。
    const char *argv[] = {"bench-hashtable", "--max-capacity", "1", "--csv", csv_path.c_str()};
    const int argc = (int)(sizeof(argv) / sizeof(argv[0]));

    // Pre-Assert
    EXPECT_CALL(mock_stdio_, ferror(_, _, _, _))
        .Times(AnyNumber()); // [Pre-Assert確認_異常系] - ferror() が任意の回数呼び出されること。(標準出力の検査は libc の処理へ委譲する)
    EXPECT_CALL(mock_stdio_, ferror(_, _, _, Ne(stdout)))
        .WillOnce(Return(1)); // [Pre-Assert確認_異常系] - ferror() が CSV のストリームを指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - ferror() から 1 を返す。

    // Act
    testing::internal::CaptureStderr();
    int rc = __real_main(argc, (char **)&argv); // [手順] - CSV のストリームにエラーがある状態で、測定のループを飛ばす引数を指定して __real_main() を呼び出す。
    std::string errors = testing::internal::GetCapturedStderr();

    // Assert
    EXPECT_EQ(EXIT_FAILURE, rc); // [確認_異常系] - __real_main() の戻り値が EXIT_FAILURE であること。
    EXPECT_THAT(errors, HasSubstr("failed to write " + csv_path)); // [確認_異常系] - 標準エラー出力に CSV の書き込みの失敗の診断が出ること。

    // Cleanup
    (void)std::remove(csv_path.c_str());
}

// CSV を閉じる処理の失敗を検出し、EXIT_FAILURE を返すことの確認
TEST_F(bench_hashtableTest, main_returns_failure_when_csv_close_fails)
{
    // Arrange
    std::string csv_path = temp_dir() + "/bench-hashtableTest.csv"; // [状態] - 一時ディレクトリに CSV の出力先を用意する。
    const char *argv[] = {"bench-hashtable", "--max-capacity", "1", "--csv", csv_path.c_str()};
    const int argc = (int)(sizeof(argv) / sizeof(argv[0]));

    // Pre-Assert
    EXPECT_CALL(mock_stdio_, fclose(_, _, _, _))
        .WillOnce(Invoke(close_then_fail)); // [Pre-Assert確認_異常系] - fclose() が 1 回呼び出されること。
                                            // [Pre-Assert手順] - fclose() から、実関数で閉じた後に EOF を返す。

    // Act
    int rc = __real_main(argc, (char **)&argv); // [手順] - CSV を閉じる処理が失敗する状態で、測定のループを飛ばす引数を指定して __real_main() を呼び出す。

    // Assert
    EXPECT_EQ(EXIT_FAILURE, rc); // [確認_異常系] - __real_main() の戻り値が EXIT_FAILURE であること。

    // Cleanup
    (void)std::remove(csv_path.c_str());
}
