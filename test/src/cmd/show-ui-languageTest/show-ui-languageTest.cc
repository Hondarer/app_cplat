#include <testfw.h>

#include <mock_cplat.h>
#include <mock_stdio.h>

#include <cplat/base/result.h>
#include <cplat/locale/ui_language.h>

#include <cstdlib>
#include <string>
#include <cstring>

namespace
{

/**
 *  @brief          表示言語の取得が、指定した言語タグを返すようにします。
 *  @param[in,out]  mock  設定する mock。
 *  @param[in]      tag   返す言語タグ。
 */
// [サブ手順 名前=show_ui_languageTest.expect_ui_language_tag]
void expect_ui_language_tag(NiceMock<Mock_cplat> &mock, const char *const tag)
{
    EXPECT_CALL(mock, cplat_ui_language_get_tag(_, _))
        .WillOnce(Invoke(
            [tag](char *tag_out, size_t tag_size)
            {
                const size_t length = strlen(tag);

                if (tag_out == nullptr || tag_size <= length)
                {
                    return CPLAT_ERR_BUFFER_TOO_SMALL;
                }
                memcpy(tag_out, tag, length + 1U);
                return CPLAT_OK;
            }));
    // [Pre-Assert確認_正常系] - cplat_ui_language_get_tag() が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_ui_language_get_tag() から、指定した表示言語タグを出力バッファーへコピーして CPLAT_OK を返す。
}
// [サブ手順終了]

} // namespace

class show_ui_languageTest : public Test
{
  protected:
    show_ui_languageTest()
    {
        /* main を複数回呼び出すため、コンソール初期化が登録する後始末を mock 側で受け止める */
        ON_CALL(mock_cplat_, cplat_shutdown_register(_, _)).WillByDefault(Return(CPLAT_OK));
    }

    NiceMock<Mock_stdio> mock_stdio_;
    NiceMock<Mock_cplat> mock_cplat_;
};

// 取得した言語タグを標準出力へ出力することの確認
TEST_F(show_ui_languageTest, main_prints_language_tag)
{
    // Arrange
    const char *argv[] = {"show-ui-language"};
    const int argc = (int)(sizeof(argv) / sizeof(argv[0]));

    // Pre-Assert
    // [サブ手順参照 名前=show_ui_languageTest.expect_ui_language_tag]
    expect_ui_language_tag(mock_cplat_, "ja-JP"); // 表示言語の取得が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_ui_language_get_tag() から、言語タグ "ja-JP" を出力バッファーに設定して CPLAT_OK を返す。
    EXPECT_CALL(mock_stdio_, printf(_, _, _, StrEq("ja-JP\n")))
        .Times(1); // [Pre-Assert確認_正常系] - printf() が "ja-JP\n" と等しい文字列を指定して 1 回呼び出されること。

    // Act
    int actual_ret =
        __real_main(argc, const_cast<char **>(argv)); // [手順] - 引数を指定しない状態で __real_main() を呼び出す。

    // Assert
    EXPECT_EQ(EXIT_SUCCESS, actual_ret); // [確認_正常系] - __real_main() の戻り値が EXIT_SUCCESS であること。
}

// ニュートラルの表示言語でも出力を行うことの確認
TEST_F(show_ui_languageTest, main_prints_empty_line_for_neutral)
{
    // Arrange
    const char *argv[] = {"show-ui-language"};
    const int argc = (int)(sizeof(argv) / sizeof(argv[0]));

    // Pre-Assert
    // [サブ手順参照 名前=show_ui_languageTest.expect_ui_language_tag]
    expect_ui_language_tag(mock_cplat_, ""); // 表示言語の取得が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_ui_language_get_tag() から、ニュートラルを表す空文字列を出力バッファーに設定して CPLAT_OK を返す。
    EXPECT_CALL(mock_stdio_, printf(_, _, _, StrEq("\n")))
        .Times(1); // [Pre-Assert確認_正常系] - printf() が "\n" と等しい文字列を指定して 1 回呼び出されること。

    // Act
    int actual_ret =
        __real_main(argc, const_cast<char **>(argv)); // [手順] - 引数を指定しない状態で __real_main() を呼び出す。

    // Assert
    EXPECT_EQ(EXIT_SUCCESS, actual_ret); // [確認_正常系] - __real_main() の戻り値が EXIT_SUCCESS であること。
}

// 表示言語の取得に失敗した場合に失敗終了することの確認
TEST_F(show_ui_languageTest, main_reports_failure)
{
    // Arrange
    const char *argv[] = {"show-ui-language"};
    const int argc = (int)(sizeof(argv) / sizeof(argv[0]));

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_ui_language_get_tag(_, _))
        .WillOnce(Return(
            CPLAT_ERR_INVALID_ARGUMENT)); // [Pre-Assert確認_異常系] - cplat_ui_language_get_tag() が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_ui_language_get_tag() から CPLAT_ERR_INVALID_ARGUMENT を返す。
    EXPECT_CALL(mock_stdio_, printf(_, _, _, _)).Times(0); // [Pre-Assert確認_異常系] - printf() が呼び出されないこと。

    // Act
    int actual_ret =
        __real_main(argc, const_cast<char **>(argv)); // [手順] - 取得が失敗する状態で __real_main() を呼び出す。

    // Assert
    EXPECT_EQ(EXIT_FAILURE, actual_ret); // [確認_異常系] - __real_main() の戻り値が EXIT_FAILURE であること。
}

// ヘルプの指定で使用方法を表示して正常終了することの確認
TEST_F(show_ui_languageTest, main_prints_usage_with_help)
{
    // Arrange
    const char *argv[] = {"show-ui-language", "--help"};
    const int argc = (int)(sizeof(argv) / sizeof(argv[0]));

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_ui_language_get_tag(_, _))
        .Times(0); // [Pre-Assert確認_正常系] - cplat_ui_language_get_tag() が呼び出されないこと。

    // Act
    int actual_ret =
        __real_main(argc, const_cast<char **>(argv)); // [手順] - --help を指定して __real_main() を呼び出す。

    // Assert
    EXPECT_EQ(EXIT_SUCCESS, actual_ret); // [確認_正常系] - __real_main() の戻り値が EXIT_SUCCESS であること。
}

// 未知のオプションで失敗終了することの確認
TEST_F(show_ui_languageTest, main_rejects_unknown_option)
{
    // Arrange
    const char *argv[] = {"show-ui-language", "--unknown"};
    const int argc = (int)(sizeof(argv) / sizeof(argv[0]));

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_ui_language_get_tag(_, _))
        .Times(0); // [Pre-Assert確認_異常系] - cplat_ui_language_get_tag() が呼び出されないこと。

    // Act
    int actual_ret =
        __real_main(argc, const_cast<char **>(argv)); // [手順] - 未知のオプションを指定して __real_main() を呼び出す。

    // Assert
    EXPECT_EQ(EXIT_FAILURE, actual_ret); // [確認_異常系] - __real_main() の戻り値が EXIT_FAILURE であること。
}

// --help の使い方の表示の関数が失敗した場合に失敗の終了コードを返し、診断を出すことの確認
TEST_F(show_ui_languageTest, main_returns_failure_when_help_usage_print_fails)
{
    // Arrange
    const char *argv[] = {"show-ui-language", "--help"};
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
TEST_F(show_ui_languageTest, main_returns_failure_when_help_flush_fails)
{
    // Arrange
    const char *argv[] = {"show-ui-language", "--help"};
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
TEST_F(show_ui_languageTest, main_returns_failure_when_help_stream_has_error)
{
    // Arrange
    const char *argv[] = {"show-ui-language", "--help"};
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

// 言語タグの出力に失敗した場合に EXIT_FAILURE を返すことの確認
TEST_F(show_ui_languageTest, main_returns_failure_when_tag_print_fails)
{
    // Arrange
    const char *argv[] = {"show-ui-language"};
    const int argc = (int)(sizeof(argv) / sizeof(argv[0]));

    // Pre-Assert
    // [サブ手順参照 名前=show_ui_languageTest.expect_ui_language_tag]
    expect_ui_language_tag(mock_cplat_, "ja-JP"); // 表示言語の取得が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_ui_language_get_tag() から、言語タグ "ja-JP" を出力バッファーに設定して CPLAT_OK を返す。
    EXPECT_CALL(mock_stdio_, printf(_, _, _, StrEq("ja-JP\n")))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - printf() が "ja-JP\n" と等しい文字列を指定して 1 回呼び出されること。
                               // [Pre-Assert手順] - printf() から -1 を返す。

    // Act
    int actual_ret = __real_main(
        argc, const_cast<char **>(argv)); // [手順] - 出力が失敗する状態で、引数を指定せずに __real_main() を呼び出す。

    // Assert
    EXPECT_EQ(EXIT_FAILURE, actual_ret); // [確認_異常系] - __real_main() の戻り値が EXIT_FAILURE であること。
}
