#include <testfw.h>
#include <mock_stdio.h>
#include <mock_cplat.h>

#include <cstdlib>
#include <string>

class eventlog_registerTest : public Test
{
  protected:
    eventlog_registerTest()
    {
        // eventlog-register.c の単体テストでは shutdown.c を対象外とし、登録 API をフェイクする。
        ON_CALL(mock_cplat_, cplat_shutdown_register(_, _)).WillByDefault(Return(CPLAT_OK));
        // 昇格ワーカーとしての出力パイプの付け替えは行わない。
        ON_CALL(mock_cplat_, cplat_elevated_process_attach_output_pipes(_, _, _)).WillByDefault(Return(CPLAT_OK));
    }

    NiceMock<Mock_stdio> mock_stdio_;
    NiceMock<Mock_cplat> mock_cplat_;
};
// --help の使い方の表示の関数が失敗した場合に失敗の終了コードを返し、診断を出すことの確認
TEST_F(eventlog_registerTest, main_returns_failure_when_help_usage_print_fails)
{
    // Arrange
    const char *argv[] = {"eventlog-register", "--help"};
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
    EXPECT_THAT(errors, HasSubstr("標準出力へ書き込めませんでした。")); // [確認_異常系] - 標準エラー出力に書き込みの失敗の診断が出ること。
}

// --help の後の fflush(stdout) が失敗した場合に失敗の終了コードを返し、診断を出すことの確認
TEST_F(eventlog_registerTest, main_returns_failure_when_help_flush_fails)
{
    // Arrange
    const char *argv[] = {"eventlog-register", "--help"};
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
    EXPECT_THAT(errors, HasSubstr("標準出力へ書き込めませんでした。")); // [確認_異常系] - 標準エラー出力に書き込みの失敗の診断が出ること。
}

// --help の後の ferror(stdout) がエラーを示した場合に失敗の終了コードを返し、診断を出すことの確認
TEST_F(eventlog_registerTest, main_returns_failure_when_help_stream_has_error)
{
    // Arrange
    const char *argv[] = {"eventlog-register", "--help"};
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
    EXPECT_THAT(errors, HasSubstr("標準出力へ書き込めませんでした。")); // [確認_異常系] - 標準エラー出力に書き込みの失敗の診断が出ること。
}

#if defined(PLATFORM_WINDOWS)
// 成功の経路 (uninstall) は Windows だけで到達する。Linux の eventlog_register_run() は常に -1 を返す。

// 削除の結果の出力の後の fflush(stdout) が失敗した場合に失敗の終了コードを返し、診断を出すことの確認
TEST_F(eventlog_registerTest, main_returns_failure_when_final_flush_fails)
{
    // Arrange
    ON_CALL(mock_cplat_, cplat_elevated_process_run_piped(_, _, _, _, _))
        .WillByDefault(DoAll(SetArgPointee<3>(0), SetArgPointee<4>(0),
                             Return(CPLAT_OK))); // [状態] - cplat_elevated_process_run_piped() から、昇格せずに続ける (handled = 0) として CPLAT_OK を返す。
    ON_CALL(mock_cplat_, cplat_eventlog_unregister_source(_))
        .WillByDefault(Return(CPLAT_OK)); // [状態] - cplat_eventlog_unregister_source() から CPLAT_OK を返す。
    const char *argv[] = {"eventlog-register", "uninstall"};
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
    EXPECT_THAT(errors, HasSubstr("標準出力へ書き込めませんでした。")); // [確認_異常系] - 標準エラー出力に書き込みの失敗の診断が出ること。
}

// 削除の結果の出力の後の ferror(stdout) がエラーを示した場合に失敗の終了コードを返し、診断を出すことの確認
TEST_F(eventlog_registerTest, main_returns_failure_when_final_stream_has_error)
{
    // Arrange
    ON_CALL(mock_cplat_, cplat_elevated_process_run_piped(_, _, _, _, _))
        .WillByDefault(DoAll(SetArgPointee<3>(0), SetArgPointee<4>(0),
                             Return(CPLAT_OK))); // [状態] - cplat_elevated_process_run_piped() から、昇格せずに続ける (handled = 0) として CPLAT_OK を返す。
    ON_CALL(mock_cplat_, cplat_eventlog_unregister_source(_))
        .WillByDefault(Return(CPLAT_OK)); // [状態] - cplat_eventlog_unregister_source() から CPLAT_OK を返す。
    const char *argv[] = {"eventlog-register", "uninstall"};
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
    EXPECT_THAT(errors, HasSubstr("標準出力へ書き込めませんでした。")); // [確認_異常系] - 標準エラー出力に書き込みの失敗の診断が出ること。
}
#endif /* PLATFORM_WINDOWS */
