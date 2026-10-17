#include <testfw.h>
#include <mock_cplat.h>
#include <mock_unistd.h>
#include <cplat/base/result.h>
#include <cplat/runtime/elevated_process.h>

using testing::_;
using testing::NiceMock;
using testing::Return;

#if defined(PLATFORM_WINDOWS)
    #include <cplat/base/windows_sdk.h>
    #include <cplat/runtime/process_internal.h>
    #include <string>

// このテストではネイティブ プロセスの取り込み経路を実行しないため、リンク用の fake を定義する。
extern "C" cplat_process *cplat_internal_process_adopt_native(intptr_t native_handle)
{
    (void)native_handle;
    return nullptr;
}
#endif /* PLATFORM_WINDOWS */

// 昇格結果報告先が未検出の場合に出力フラグが 0 になることの確認
TEST(elevatedProcessTest, elevated_result_target_initializes_output)
{
    // Arrange
    int argc = 1;
    char program[] = "resultBehaviorTest";
    char *argv[] = {program, nullptr};
    int detected = 1;

    // Pre-Assert

    // Act
    int result = cplat_elevated_process_extract_result_target(
        &argc, argv,
        &detected); // [手順] - 結果報告先フラグのない引数を指定して cplat_elevated_process_extract_result_target() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        result); // [確認_正常系] - cplat_elevated_process_extract_result_target() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, detected); // [確認_正常系] - detected が 0 であること。(結果報告先が未検出であることの確認)
}

#if defined(PLATFORM_LINUX)
// Linux の root 判定が geteuid の結果を elevated へ反映することの確認
TEST(elevatedProcessTest, is_elevated_reflects_effective_user_id)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    int elevated = -1;

    // Pre-Assert
    EXPECT_CALL(mock_unistd, geteuid(_, _, _))
        .WillOnce(Return(static_cast<uid_t>(0))); // [Pre-Assert確認_正常系] - geteuid() が 1 回呼び出されること。
                                                  // [Pre-Assert手順] - geteuid() から実効ユーザー ID 0 を返す。

    // Act
    int root_result = cplat_elevated_process_is_elevated(
        &elevated); // [手順] - geteuid() が 0 を返す状態で cplat_elevated_process_is_elevated() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              root_result); // [確認_正常系] - cplat_elevated_process_is_elevated() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        1, elevated); // [確認_正常系] - elevated が 1 であること。(実効ユーザー ID 0 が昇格状態と判定されることの確認)
}

// Linux の非 root 判定が elevated を 0 にすることの確認
TEST(elevatedProcessTest, is_elevated_rejects_non_root_as_not_elevated)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    int elevated = -1;

    // Pre-Assert
    EXPECT_CALL(mock_unistd, geteuid(_, _, _))
        .WillOnce(Return(static_cast<uid_t>(1000))); // [Pre-Assert確認_正常系] - geteuid() が 1 回呼び出されること。
                                                     // [Pre-Assert手順] - geteuid() から実効ユーザー ID 1000 を返す。

    // Act
    int result = cplat_elevated_process_is_elevated(
        &elevated); // [手順] - geteuid() が 1000 を返す状態で cplat_elevated_process_is_elevated() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              result); // [確認_正常系] - cplat_elevated_process_is_elevated() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        0,
        elevated); // [確認_正常系] - elevated が 0 であること。(実効ユーザー ID 1000 が非昇格状態と判定されることの確認)
}
#endif /* PLATFORM_LINUX */

// 昇格判定 API が NULL 出力を拒否することの確認
TEST(elevatedProcessTest, elevated_apis_reject_null_outputs)
{
    // Arrange
    int exit_code = -1;
    int handled = -1;

    // Pre-Assert

    // Act
    int is_elevated_result = cplat_elevated_process_is_elevated(
        nullptr); // [手順] - 昇格状態の出力先に nullptr を指定して cplat_elevated_process_is_elevated() を呼び出す。
    int run_result = cplat_elevated_process_run_if_needed(
        nullptr, nullptr,
        &handled); // [手順] - exit_code に nullptr を指定して cplat_elevated_process_run_if_needed() を呼び出す。
    int run_handled_result = cplat_elevated_process_run_if_needed(
        nullptr, &exit_code,
        nullptr); // [手順] - handled に nullptr を指定して cplat_elevated_process_run_if_needed() を呼び出す。
    int result_run_with_result = cplat_elevated_process_run_with_result(
        nullptr, &exit_code, nullptr, nullptr,
        0U); // [手順] - handled に nullptr を指定して cplat_elevated_process_run_with_result() を呼び出す。
    int result_run_with_null_exit = cplat_elevated_process_run_with_result(
        nullptr, nullptr, &handled, nullptr,
        0U); // [手順] - exit_code に nullptr を指定して cplat_elevated_process_run_with_result() を呼び出す。
    int report_null_result = cplat_elevated_process_report_result(
        nullptr); // [手順] - message に nullptr を指定して cplat_elevated_process_report_result() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        is_elevated_result); // [確認_異常系] - cplat_elevated_process_is_elevated() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        run_result); // [確認_異常系] - exit_code が nullptr の cplat_elevated_process_run_if_needed() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        run_handled_result); // [確認_異常系] - handled が nullptr の cplat_elevated_process_run_if_needed() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        result_run_with_result); // [確認_異常系] - handled が nullptr の cplat_elevated_process_run_with_result() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        result_run_with_null_exit); // [確認_異常系] - exit_code が nullptr の cplat_elevated_process_run_with_result() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        report_null_result); // [確認_異常系] - cplat_elevated_process_report_result() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// run_piped が NULL 出力を拒否することの確認
TEST(elevatedProcessTest, run_piped_rejects_null_outputs)
{
    // Arrange
    int exit_code = -1;
    int handled = -1;

    // Pre-Assert

    // Act
    int null_exit_result = cplat_elevated_process_run_piped(
        nullptr, nullptr, nullptr, nullptr,
        &handled); // [手順] - exit_code に nullptr を指定して cplat_elevated_process_run_piped() を呼び出す。
    int null_handled_result = cplat_elevated_process_run_piped(
        nullptr, nullptr, nullptr, &exit_code,
        nullptr); // [手順] - handled に nullptr を指定して cplat_elevated_process_run_piped() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_exit_result); // [確認_異常系] - exit_code が nullptr の cplat_elevated_process_run_piped() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_handled_result); // [確認_異常系] - handled が nullptr の cplat_elevated_process_run_piped() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// 出力パイプ フラグがない場合に attach_output_pipes が何もしないことの確認
TEST(elevatedProcessTest, attach_output_pipes_without_flag_keeps_arguments)
{
    // Arrange
    char program[] = "pipeTest";
    char command[] = "install";
    char *argv[] = {program, command, nullptr};
    int argc = 2;
    int attached = -1;

    // Pre-Assert

    // Act
    int result = cplat_elevated_process_attach_output_pipes(
        &argc, argv,
        &attached); // [手順] - 出力パイプ フラグのない引数を指定して cplat_elevated_process_attach_output_pipes() を呼び出す。
    int null_result = cplat_elevated_process_attach_output_pipes(
        nullptr, nullptr,
        nullptr); // [手順] - すべての引数に nullptr を指定して cplat_elevated_process_attach_output_pipes() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        result); // [確認_正常系] - フラグがない場合の cplat_elevated_process_attach_output_pipes() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, attached);           // [確認_正常系] - attached が 0 であること。(パイプが接続されないことの確認)
    EXPECT_EQ(2, argc);               // [確認_正常系] - argc が 2 であること。(引数の個数が変更されないことの確認)
    EXPECT_STREQ("install", argv[1]); // [確認_正常系] - argv[1] が "install" であること。
    EXPECT_EQ(
        CPLAT_OK,
        null_result); // [確認_正常系] - nullptr 引数の cplat_elevated_process_attach_output_pipes() の戻り値が CPLAT_OK であること。
}

#if defined(PLATFORM_WINDOWS)
// 出力パイプ フラグの値が不正な場合にフラグを取り除いて失敗することの確認
TEST(elevatedProcessTest, attach_output_pipes_removes_malformed_flag_and_fails)
{
    // Arrange
    char program[] = "pipeTest";
    char flag[] = "--cplat-output-pipes=123:abc";
    char command[] = "install";
    char *argv[] = {program, flag, command, nullptr};
    int argc = 3;
    int attached = -1;

    // Pre-Assert

    // Act
    int result = cplat_elevated_process_attach_output_pipes(
        &argc, argv,
        &attached); // [手順] - ハンドル値が数値でないフラグを指定して cplat_elevated_process_attach_output_pipes() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        result); // [確認_異常系] - cplat_elevated_process_attach_output_pipes() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(0, attached); // [確認_異常系] - attached が 0 であること。(パイプが接続されないことの確認)
    EXPECT_EQ(2, argc); // [確認_異常系] - argc が 2 であること。(フラグの除去で引数の個数が 3 から 1 減ることの確認)
    EXPECT_STREQ("install",
                 argv[1]); // [確認_異常系] - argv[1] が "install" であること。(後続の引数が前へ詰められることの確認)
    EXPECT_EQ(nullptr, argv[2]); // [確認_異常系] - argv[2] が nullptr であること。
}

// 出力パイプ フラグのハンドルがパイプ以外を指す場合に付け替えないことの確認
TEST(elevatedProcessTest, attach_output_pipes_rejects_non_pipe_handles)
{
    // Arrange
    HANDLE event_handle = CreateEventW(
        nullptr, TRUE, FALSE,
        nullptr); // [手順] - パイプではないハンドルを作るため、手動リセットで非シグナル状態を指定して CreateEventW() を呼び出す。
    std::string flag_text = "--cplat-output-pipes=" + std::to_string(GetCurrentProcessId()) + ":" +
                            std::to_string(reinterpret_cast<uintptr_t>(event_handle)) + ":" +
                            std::to_string(reinterpret_cast<uintptr_t>(event_handle));
    char program[] = "pipeTest";
    char *argv[] = {program, &flag_text[0], nullptr};
    int argc = 2;
    int attached = -1;
    HANDLE stdout_before = GetStdHandle(STD_OUTPUT_HANDLE);

    ASSERT_NE(nullptr, event_handle); // [状態確認] - CreateEventW() の戻り値が nullptr 以外であること。

    // Pre-Assert

    // Act
    int result = cplat_elevated_process_attach_output_pipes(
        &argc, argv,
        &attached); // [手順] - 自プロセスのイベント ハンドルを指すフラグを指定して cplat_elevated_process_attach_output_pipes() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        result); // [確認_異常系] - cplat_elevated_process_attach_output_pipes() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(0, attached); // [確認_異常系] - attached が 0 であること。(パイプが接続されないことの確認)
    EXPECT_EQ(1, argc); // [確認_異常系] - argc が 1 であること。(フラグの除去で引数の個数が 2 から 1 減ることの確認)
    EXPECT_EQ(
        stdout_before,
        GetStdHandle(
            STD_OUTPUT_HANDLE)); // [確認_異常系] - 標準出力に対する GetStdHandle() の戻り値が stdout_before と一致すること。(標準出力ハンドルが変更されないことの確認)

    CloseHandle(event_handle);
}
#endif /* PLATFORM_WINDOWS */

#if defined(PLATFORM_LINUX)
// Linux では root の場合だけ run_piped が完了扱いにすることの確認
TEST(elevatedProcessTest, run_piped_reports_linux_elevation_state)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    int root_exit_code = -1;
    int root_handled = -1;
    int user_exit_code = 0;
    int user_handled = -1;

    // Pre-Assert
    EXPECT_CALL(mock_unistd, geteuid(_, _, _))
        .WillOnce(Return(static_cast<uid_t>(0)))
        .WillOnce(Return(static_cast<uid_t>(1000))); // [Pre-Assert確認_正常系] - geteuid() が 2 回呼び出されること。
    // [Pre-Assert手順] - geteuid() から 1 回目は 0、2 回目は 1000 を返す。

    // Act
    int root_result = cplat_elevated_process_run_piped(
        "--test", nullptr, nullptr, &root_exit_code,
        &root_handled); // [手順] - geteuid() が 0 を返す状態で cplat_elevated_process_run_piped() を呼び出す。
    int user_result = cplat_elevated_process_run_piped(
        "--test", nullptr, nullptr, &user_exit_code,
        &user_handled); // [手順] - geteuid() が 1000 を返す状態で cplat_elevated_process_run_piped() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        root_result); // [確認_正常系] - root 状態の cplat_elevated_process_run_piped() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, root_exit_code); // [確認_正常系] - root_exit_code が 0 であること。
    EXPECT_EQ(0,
              root_handled); // [確認_正常系] - root_handled が 0 であること。(Linux では昇格処理を実行しないことの確認)
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        user_result); // [確認_異常系] - 非 root 状態の cplat_elevated_process_run_piped() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(EXIT_FAILURE, user_exit_code); // [確認_異常系] - user_exit_code が EXIT_FAILURE であること。
    EXPECT_EQ(0,
              user_handled); // [確認_異常系] - user_handled が 0 であること。(Linux では昇格処理を実行しないことの確認)
}
#endif /* PLATFORM_LINUX */

// Linux 固有 API が省略可能な出力と結果報告を処理することの確認
TEST(elevatedProcessTest, linux_helpers_accept_optional_output_and_report_unavailable_target)
{
    // Arrange
    int argc = 1;
    char program[] = "resultBehaviorTest";
    char *argv[] = {program, nullptr};

    // Pre-Assert

    // Act
    int extract_result = cplat_elevated_process_extract_result_target(
        &argc, argv,
        nullptr); // [手順] - detected_out に nullptr を指定して cplat_elevated_process_extract_result_target() を呼び出す。
#if defined(PLATFORM_LINUX)
    int report_result = cplat_elevated_process_report_result(
        "result"); // [手順] - Linux で結果メッセージ "result" を指定して cplat_elevated_process_report_result() を呼び出す。
#elif defined(PLATFORM_WINDOWS)
    (void)cplat_elevated_process_report_result(
        "result"); // [手順] - Windows でも戻り値を使わずに結果メッセージ "result" を指定して cplat_elevated_process_report_result() を呼び出す。
#endif /* PLATFORM_ */

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        extract_result); // [確認_正常系] - detected_out を省略した cplat_elevated_process_extract_result_target() の戻り値が CPLAT_OK であること。
#if defined(PLATFORM_LINUX)
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        report_result); // [確認_異常系] - Linux の cplat_elevated_process_report_result() の戻り値が CPLAT_ERR_UNKNOWN であること。
#endif                        /* PLATFORM_LINUX */
}

#if defined(PLATFORM_LINUX)
// Linux では root の場合だけ昇格処理を完了扱いにすることの確認
TEST(elevatedProcessTest, run_apis_report_linux_elevation_state)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    int exit_code = -1;
    int handled = -1;
    char result_message[16] = "stale";

    // Pre-Assert
    EXPECT_CALL(mock_unistd, geteuid(_, _, _))
        .Times(2)
        .WillRepeatedly(Return(static_cast<uid_t>(0))); // [Pre-Assert確認_正常系] - geteuid() が 2 回呼び出されること。
                                                        // [Pre-Assert手順] - geteuid() から実効ユーザー ID 0 を返す。

    // Act
    int run_result = cplat_elevated_process_run_if_needed(
        "--test", &exit_code,
        &handled); // [手順] - root 状態で cplat_elevated_process_run_if_needed() を呼び出す。
    int result_run_with_result = cplat_elevated_process_run_with_result(
        "--test", &exit_code, &handled, result_message,
        sizeof(result_message)); // [手順] - root 状態で cplat_elevated_process_run_with_result() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        run_result); // [確認_正常系] - root 状態の cplat_elevated_process_run_if_needed() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, exit_code); // [確認_正常系] - exit_code が 0 であること。
    EXPECT_EQ(0, handled);   // [確認_正常系] - handled が 0 であること。(Linux では昇格処理を実行しないことの確認)
    EXPECT_EQ(
        CPLAT_OK,
        result_run_with_result); // [確認_正常系] - root 状態の cplat_elevated_process_run_with_result() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, handled); // [確認_正常系] - handled が 0 であること。(Linux では昇格処理を実行しないことの確認)
    EXPECT_STREQ("", result_message); // [確認_正常系] - result_message が "" であること。
}

// Linux の結果バッファー初期化が NULL とサイズ 0 を受理することの確認
TEST(elevatedProcessTest, run_with_result_accepts_absent_or_zero_sized_message_buffer)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    int exit_code = -1;
    int handled = -1;
    char result_message[2] = "x";

    // Pre-Assert
    EXPECT_CALL(mock_unistd, geteuid(_, _, _))
        .Times(2)
        .WillRepeatedly(Return(static_cast<uid_t>(0))); // [Pre-Assert確認_正常系] - geteuid() が 2 回呼び出されること。
                                                        // [Pre-Assert手順] - geteuid() から実効ユーザー ID 0 を返す。

    // Act
    int null_buffer_result = cplat_elevated_process_run_with_result(
        nullptr, &exit_code, &handled, nullptr,
        1U); // [手順] - result_message に nullptr を指定して cplat_elevated_process_run_with_result() を呼び出す。
    int zero_size_result = cplat_elevated_process_run_with_result(
        nullptr, &exit_code, &handled, result_message,
        0U); // [手順] - result_message_size に 0 を指定して cplat_elevated_process_run_with_result() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        null_buffer_result); // [確認_正常系] - nullptr バッファー指定時の cplat_elevated_process_run_with_result() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        zero_size_result); // [確認_正常系] - サイズ 0 指定時の cplat_elevated_process_run_with_result() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "x",
        result_message); // [確認_正常系] - result_message が "x" であること。(サイズ 0 のバッファー内容が変更されないことの確認)
}

// Linux の非 root 状態では昇格 API が失敗を報告することの確認
TEST(elevatedProcessTest, run_apis_reject_linux_non_root)
{
    // Arrange
    NiceMock<Mock_unistd> mock_unistd;
    int exit_code = 0;
    int handled = 0;

    // Pre-Assert
    EXPECT_CALL(mock_unistd, geteuid(_, _, _))
        .Times(2)
        .WillRepeatedly(
            Return(static_cast<uid_t>(1000))); // [Pre-Assert確認_異常系] - geteuid() が 2 回呼び出されること。
                                               // [Pre-Assert手順] - geteuid() から実効ユーザー ID 1000 を返す。

    // Act
    int run_result = cplat_elevated_process_run_if_needed(
        nullptr, &exit_code, &handled); // [手順] - 非 root 状態で cplat_elevated_process_run_if_needed() を呼び出す。
    int result_run_with_result = cplat_elevated_process_run_with_result(
        nullptr, &exit_code, &handled, nullptr,
        0U); // [手順] - 非 root 状態で cplat_elevated_process_run_with_result() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        run_result); // [確認_異常系] - 非 root 状態の cplat_elevated_process_run_if_needed() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(EXIT_FAILURE, exit_code); // [確認_異常系] - exit_code が EXIT_FAILURE であること。
    EXPECT_EQ(0, handled); // [確認_異常系] - handled が 0 であること。(Linux では昇格処理を実行しないことの確認)
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        result_run_with_result); // [確認_異常系] - 非 root 状態の cplat_elevated_process_run_with_result() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(EXIT_FAILURE, exit_code); // [確認_異常系] - exit_code が EXIT_FAILURE であること。
}
#endif /* PLATFORM_LINUX */
