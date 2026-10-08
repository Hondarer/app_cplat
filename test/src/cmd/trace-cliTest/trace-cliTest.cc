#include <testfw.h>
#include <mock_stdio.h>
#include <mock_cplat.h>
#include "trace-cli.h"

#include <vector>
#include <string>
#include <cerrno>
#include <cstring>
#include <cstdint>
#include <cstdlib>

using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::HasSubstr;
using testing::NiceMock;
using testing::Pointee;
using testing::Return;
using testing::StrEq;

namespace
{

static const char *const kRcSuccessTty = "\033[32mrc=0\033[0m\n";
static const char *const kRcErrorTty = "\033[31mrc=-1\033[0m\n";

static char *copy_line(char *dst, int size, const char *src)
{
    size_t len = strlen(src);

    if (size <= 0)
    {
        return NULL;
    }

    if (len >= (size_t)size)
    {
        len = (size_t)size - 1U;
    }
    memcpy(dst, src, len);
    dst[len] = '\0';
    return dst;
}

static int emulate_cplat_strncpy(char *dest, size_t dest_size, const char *src, size_t count)
{
    size_t len;

    if (dest == NULL || dest_size == 0U || src == NULL)
    {
        return EINVAL;
    }

    len = strlen(src);
    if (len > count)
    {
        len = count;
    }
    if (len >= dest_size)
    {
        len = dest_size - 1U;
    }

    memcpy(dest, src, len);
    dest[len] = '\0';
    return 0;
}

} // namespace

class trace_cliTest : public Test
{
  protected:
    NiceMock<Mock_stdio> mock_stdio_;
    NiceMock<Mock_cplat> mock_cplat_;
    trace_cli_session session_{};
    cplat_tracer *handle_ = reinterpret_cast<cplat_tracer *>(static_cast<uintptr_t>(0x1234));

    // [サブ手順 名前=trace_cliTest.SetUp]
    void SetUp() override
    {
        trace_cli_session_init(&session_);
        ON_CALL(mock_cplat_, cplat_strncpy(_, _, _, _)).WillByDefault(emulate_cplat_strncpy);
        // [状態] - `cplat_strncpy` の既定動作を設定する。
        ON_CALL(mock_cplat_, cplat_isatty(CPLAT_STREAM_STDOUT)).WillByDefault(Return(1));
        // [状態] - `cplat_isatty` の既定動作を設定する。
        ON_CALL(mock_cplat_, cplat_tracer_dispose(_)).WillByDefault(Return());
        // [状態] - `cplat_tracer_dispose` の既定動作を設定する。
        ON_CALL(mock_stdio_, printf(_, _, _, _)).WillByDefault(Return(0));
        // [状態] - `printf` の既定動作を設定する。
        ON_CALL(mock_stdio_, fprintf(_, _, _, _, _)).WillByDefault(Return(0));
        // [状態] - `fprintf` の既定動作を設定する。
    }
    // [サブ手順終了]

    // [サブ手順 名前=trace_cliTest.TearDown]
    void TearDown() override
    {
        trace_cli_session_dispose(&session_);
    }
    // [サブ手順終了]
};

// create で handle が生成され、2 回目の create が拒否されることの確認
// [サブ手順参照 名前=trace_cliTest.SetUp]
TEST_F(trace_cliTest, process_line_create_and_reject_second_create)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_tracer_create(CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED))
        .WillOnce(Return(handle_)); // [Pre-Assert確認_正常系] - cplat_tracer_create が 1 回呼び出されること。
                                    // [Pre-Assert手順] - cplat_tracer_create から handle_ を返却する。
    EXPECT_CALL(mock_stdio_, printf(_, _, _, StrEq("handle=created\n")))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - create 成功時に "handle=created" が出力されること。
    EXPECT_CALL(mock_stdio_, fprintf(_, _, _, _, HasSubstr("既存の handle")))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_異常系] - 2 回目の create で "既存の handle" を含む操作エラーが出力されること。

    // Act
    int first = trace_cli_process_line(&session_, "create");  // [手順] - 1 回目の "create" を処理する。
    int second = trace_cli_process_line(&session_, "create"); // [手順] - 続けて 2 回目の "create" を処理する。

    // Assert
    EXPECT_EQ(0, first); // [確認_正常系] - 1 回目に呼び出した trace_cli_process_line の戻り値が 0 (継続) であること。
    EXPECT_LT(second,
              0); // [確認_異常系] - 2 回目に呼び出した trace_cli_process_line の戻り値が負 (エラー) であること。
    EXPECT_EQ(handle_, session_.handle); // [確認_正常系] - session に生成済み handle が保持されること。
}
// [サブ手順参照 名前=trace_cliTest.TearDown]

// set-file-level の null キーワードが NULL パスとして tracer API へ渡されることの確認
// [サブ手順参照 名前=trace_cliTest.SetUp]
TEST_F(trace_cliTest, process_line_set_file_level_accepts_null_keyword)
{
    // Arrange
    session_.handle = handle_; // [状態] - 既存 handle を持つ session とする。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_tracer_set_file_level(handle_, nullptr, CPLAT_TRACE_LEVEL_INFO, 0U, 0, 0))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - cplat_tracer_set_file_level が path=NULL、level=INFO で 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_tracer_set_file_level から 0 を返却する。
    EXPECT_CALL(mock_stdio_, printf(_, _, _, StrEq(kRcSuccessTty)))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 正常戻り値が緑の "rc=0" として表示されること。

    // Act
    int rc = trace_cli_process_line(&session_,
                                    "set-file-level null INFO"); // [手順] - "set-file-level null INFO" を処理する。

    // Assert
    EXPECT_EQ(0, rc); // [確認_正常系] - trace_cli_process_line の戻り値が 0 (継続) であること。
}
// [サブ手順参照 名前=trace_cliTest.TearDown]

// set-os-level で tracer API がエラーの場合に赤の rc 表示となることの確認
// [サブ手順参照 名前=trace_cliTest.SetUp]
TEST_F(trace_cliTest, process_line_set_os_level_colors_error_rc)
{
    // Arrange
    session_.handle = handle_; // [状態] - 既存 handle を持つ session とする。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_tracer_set_os_level(handle_, CPLAT_TRACE_LEVEL_INFO))
        .WillOnce(
            Return(-1)); // [Pre-Assert確認_異常系] - cplat_tracer_set_os_level が level=INFO で 1 回呼び出されること。
                         // [Pre-Assert手順] - cplat_tracer_set_os_level から -1 を返却する。
    EXPECT_CALL(mock_stdio_, printf(_, _, _, StrEq(kRcErrorTty)))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - エラー戻り値が赤の "rc=-1" として表示されること。

    // Act
    int rc = trace_cli_process_line(&session_, "set-os-level INFO"); // [手順] - "set-os-level INFO" を処理する。

    // Assert
    EXPECT_EQ(0, rc); // [確認_正常系] - trace_cli_process_line の戻り値が 0 (継続) であること。
}
// [サブ手順参照 名前=trace_cliTest.TearDown]

// stdout が TTY でない場合に rc 表示へ ANSI 色を付けないことの確認
// [サブ手順参照 名前=trace_cliTest.SetUp]
TEST_F(trace_cliTest, process_line_set_os_level_keeps_plain_rc_when_stdout_is_not_tty)
{
    // Arrange
    session_.handle = handle_; // [状態] - 既存 handle を持つ session とする。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_isatty(CPLAT_STREAM_STDOUT))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - cplat_isatty(CPLAT_STREAM_STDOUT) が 1 回呼び出されること。
                              // [Pre-Assert手順] - cplat_isatty から 0 (非 TTY) を返却する。
    EXPECT_CALL(mock_cplat_, cplat_tracer_set_os_level(handle_, CPLAT_TRACE_LEVEL_INFO))
        .WillOnce(
            Return(0)); // [Pre-Assert確認_正常系] - cplat_tracer_set_os_level が level=INFO で 1 回呼び出されること。
                        // [Pre-Assert手順] - cplat_tracer_set_os_level から 0 を返却する。
    EXPECT_CALL(mock_stdio_, printf(_, _, _, StrEq("rc=0\n")))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - ANSI 色なしの "rc=0" が表示されること。

    // Act
    int rc = trace_cli_process_line(&session_, "set-os-level INFO"); // [手順] - "set-os-level INFO" を処理する。

    // Assert
    EXPECT_EQ(0, rc); // [確認_正常系] - trace_cli_process_line の戻り値が 0 (継続) であること。
}
// [サブ手順参照 名前=trace_cliTest.TearDown]

// dispose で保持中の handle が解放され session から外れることの確認
// [サブ手順参照 名前=trace_cliTest.SetUp]
TEST_F(trace_cliTest, process_line_dispose_releases_handle)
{
    // Arrange
    session_.handle = handle_; // [状態] - 既存 handle を持つ session とする。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_tracer_dispose(_))
        .Times(AnyNumber())
        .WillRepeatedly(Return()); // [Pre-Assert手順] - 後処理で発生する NULL ハンドルの dispose を許容する。
    // [Pre-Assert確認_正常系] - mock_cplat_ の cplat_tracer_dispose(_) が登録した呼び出し期待を満たすこと。
    EXPECT_CALL(mock_cplat_, cplat_tracer_dispose(Pointee(handle_)))
        .WillOnce(
            Return()); // [Pre-Assert確認_正常系] - handle_ を保持するポインターで cplat_tracer_dispose が 1 回呼び出されること。
    EXPECT_CALL(mock_stdio_, printf(_, _, _, StrEq("handle=disposed\n")))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - dispose 結果として "handle=disposed" が出力されること。

    // Act
    int rc = trace_cli_process_line(&session_, "dispose"); // [手順] - "dispose" を処理する。

    // Assert
    EXPECT_EQ(0, rc);                    // [確認_正常系] - trace_cli_process_line の戻り値が 0 (継続) であること。
    EXPECT_EQ(nullptr, session_.handle); // [確認_正常系] - session の handle が NULL に戻ること。
}
// [サブ手順参照 名前=trace_cliTest.TearDown]

// write-hex の引用付き 16 進文字列とラベルが解析されて API へ渡されることの確認
// [サブ手順参照 名前=trace_cliTest.SetUp]
TEST_F(trace_cliTest, process_line_write_hex_parses_quoted_hex_and_label)
{
    // Arrange
    session_.handle = handle_; // [状態] - 既存 handle を持つ session とする。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_,
                cplat_tracer_write_hex_at(handle_, CPLAT_TRACE_LEVEL_INFO, nullptr, _, 3U, StrEq("payload bytes")))
        .WillOnce(
            [](cplat_tracer *, cplat_trace_level, const cplat_timespec *, const void *data, size_t size, const char *)
            {
                const unsigned char *bytes = static_cast<const unsigned char *>(data);
                EXPECT_EQ((size_t)3, size); // [Pre-Assert確認_正常系] - 変換後データ長が 3 byte であること。
                EXPECT_EQ((unsigned char)0x01, bytes[0]); // [Pre-Assert確認_正常系] - 先頭 byte が 0x01 であること。
                EXPECT_EQ((unsigned char)0xAB, bytes[1]); // [Pre-Assert確認_正常系] - 2 byte 目が 0xAB であること。
                EXPECT_EQ((unsigned char)0xFF, bytes[2]); // [Pre-Assert確認_正常系] - 3 byte 目が 0xFF であること。
                return 0;
            }); // [Pre-Assert確認_正常系] - cplat_tracer_write_hex_at がラベル "payload bytes" で 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_tracer_write_hex_at から 0 を返却する。
    EXPECT_CALL(mock_stdio_, printf(_, _, _, StrEq(kRcSuccessTty)))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 正常戻り値が緑の "rc=0" として表示されること。

    // Act
    int rc = trace_cli_process_line(
        &session_,
        "write-hex INFO \"01 AB FF\" payload bytes"); // [手順] - "write-hex INFO \"01 AB FF\" payload bytes" を処理する。

    // Assert
    EXPECT_EQ(0, rc); // [確認_正常系] - trace_cli_process_line の戻り値が 0 (継続) であること。
}
// [サブ手順参照 名前=trace_cliTest.TearDown]

// writef が行末までを 1 つの message 文字列として API へ渡すことの確認
// [サブ手順参照 名前=trace_cliTest.SetUp]
TEST_F(trace_cliTest, process_line_writef_uses_message_as_single_string)
{
    // Arrange
    session_.handle = handle_; // [状態] - 既存 handle を持つ session とする。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_tracer_writef_at(handle_, CPLAT_TRACE_LEVEL_DEBUG, nullptr,
                                                    StrEq("message with spaces")))
        .WillOnce(Return(
            0)); // [Pre-Assert確認_正常系] - cplat_tracer_writef_at が空白を含む "message with spaces" 全体で 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_tracer_writef_at から 0 を返却する。
    EXPECT_CALL(mock_stdio_, printf(_, _, _, StrEq(kRcSuccessTty)))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 正常戻り値が緑の "rc=0" として表示されること。

    // Act
    int rc = trace_cli_process_line(
        &session_, "writef DEBUG message with spaces"); // [手順] - "writef DEBUG message with spaces" を処理する。

    // Assert
    EXPECT_EQ(0, rc); // [確認_正常系] - trace_cli_process_line の戻り値が 0 (継続) であること。
}
// [サブ手順参照 名前=trace_cliTest.TearDown]

// handle 未生成でも get-os-level が NULL handle のまま API を呼び出すことの確認
// [サブ手順参照 名前=trace_cliTest.SetUp]
TEST_F(trace_cliTest, process_line_get_os_level_calls_api_with_null_handle)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_tracer_get_os_level(nullptr))
        .WillOnce(Return(
            CPLAT_TRACE_LEVEL_WARNING)); // [Pre-Assert確認_正常系] - cplat_tracer_get_os_level が NULL handle のまま 1 回呼び出されること。
                                         // [Pre-Assert手順] - cplat_tracer_get_os_level から WARNING を返却する。
    EXPECT_CALL(mock_stdio_, printf(_, _, _, StrEq("level=WARNING(2)\n")))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 取得結果が "level=WARNING(2)" として表示されること。

    // Act
    int rc = trace_cli_process_line(&session_, "get-os-level"); // [手順] - "get-os-level" を処理する。

    // Assert
    EXPECT_EQ(0, rc); // [確認_正常系] - trace_cli_process_line の戻り値が 0 (継続) であること。
}
// [サブ手順参照 名前=trace_cliTest.TearDown]

// help でコマンド一覧が stdout に出力されることの確認
// [サブ手順参照 名前=trace_cliTest.SetUp]
TEST_F(trace_cliTest, process_line_help_prints_command_list)
{
    // Arrange

    // Pre-Assert
    EXPECT_CALL(mock_stdio_, printf(_, _, _, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(0)); // [Pre-Assert手順] - help の複数行 stdout 出力を許容する。
    // [Pre-Assert確認_正常系] - mock_stdio_ の printf(_, _, _, _) が登録した呼び出し期待を満たすこと。
    EXPECT_CALL(mock_stdio_, printf(_, _, _, HasSubstr("trace-cli")))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - "trace-cli" を含む help 見出しが出力されること。
    EXPECT_CALL(mock_stdio_, printf(_, _, _, HasSubstr("write-hexf")))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - "write-hexf" を含むコマンド一覧が出力されること。

    // Act
    int rc = trace_cli_process_line(&session_, "help"); // [手順] - "help" を処理する。

    // Assert
    EXPECT_EQ(0, rc); // [確認_正常系] - trace_cli_process_line の戻り値が 0 (継続) であること。
}
// [サブ手順参照 名前=trace_cliTest.TearDown]

// quit が終了要求として処理されることの確認
// [サブ手順参照 名前=trace_cliTest.SetUp]
TEST_F(trace_cliTest, process_line_quit_requests_exit)
{
    // Arrange

    // Pre-Assert

    // Act
    int rc = trace_cli_process_line(&session_, "quit"); // [手順] - "quit" を処理する。

    // Assert
    EXPECT_EQ(1, rc); // [確認_正常系] - trace_cli_process_line の戻り値が 1 (終了要求) であること。
    EXPECT_EQ(1, session_.exit_requested); // [確認_正常系] - session の exit_requested が 1 になること。
}
// [サブ手順参照 名前=trace_cliTest.TearDown]

// --help 指定時に usage を表示して正常終了することの確認
// [サブ手順参照 名前=trace_cliTest.SetUp]
TEST_F(trace_cliTest, main_prints_usage_on_help)
{
    // Arrange
    int argc = 2;
    const char *argv[] = {"trace-cli", "--help"}; // [状態] - main() に与える引数を "trace-cli", "--help" とする。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_console_init())
        .WillOnce(
            Return()); // [Pre-Assert確認_正常系] - main() 呼び出し時に cplat_console_init が 1 回呼び出されること。

    // Act
    int rc = __real_main(argc, (char **)&argv); // [手順] - main() に引数を与えて呼び出す。

    // Assert
    EXPECT_EQ(EXIT_SUCCESS, rc); // [確認_正常系] - main() の戻り値が EXIT_SUCCESS であること。
}
// [サブ手順参照 名前=trace_cliTest.TearDown]

// 対話モードで create から exit までの一連のコマンドが処理され handle が解放されることの確認
// [サブ手順参照 名前=trace_cliTest.SetUp]
TEST_F(trace_cliTest, main_runs_interactive_sequence_and_disposes_handle)
{
    // Arrange
    int argc = 1;
    const char *argv[] = {
        "trace-cli"}; // [状態] - main() に与える引数をプログラム名 "trace-cli" のみとし、対話モードで起動する。
    std::vector<std::string> lines = {
        "create", "start", "write INFO hello world", "stop", "dispose", "exit",
    }; // [状態] - 対話入力として "create", "start", "write INFO hello world", "stop", "dispose", "exit" を順に供給する入力列を用意する。
    size_t index = 0U;
    cplat_prompt *prompt_handle = reinterpret_cast<cplat_prompt *>(static_cast<uintptr_t>(0x5678));

    testing::Sequence io_seq;

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_console_init())
        .WillOnce(
            Return()); // [Pre-Assert確認_正常系] - main() 呼び出し時に cplat_console_init が 1 回呼び出されること。
    EXPECT_CALL(mock_cplat_, cplat_prompt_create(_))
        .WillOnce(Return(
            prompt_handle)); // [Pre-Assert確認_正常系] - main() 呼び出し時に cplat_prompt_create が 1 回呼び出されること。
                             // [Pre-Assert手順] - cplat_prompt_create から prompt_handle を返却する。
    EXPECT_CALL(mock_cplat_, cplat_prompt_dispose(prompt_handle))
        .WillOnce(
            Return()); // [Pre-Assert確認_正常系] - main() 終了時に cplat_prompt_dispose(prompt_handle) が 1 回呼び出されること。
    EXPECT_CALL(mock_cplat_, cplat_tracer_dispose(_))
        .Times(AnyNumber())
        .WillRepeatedly(Return()); // [Pre-Assert手順] - 後処理で発生する NULL ハンドルの dispose を許容する。
    // [Pre-Assert確認_正常系] - mock_cplat_ の cplat_tracer_dispose(_) が登録した呼び出し期待を満たすこと。
    EXPECT_CALL(mock_cplat_, cplat_tracer_create(CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED))
        .WillOnce(Return(
            handle_)); // [Pre-Assert確認_正常系] - create コマンドで cplat_tracer_create が 1 回呼び出されること。
                       // [Pre-Assert手順] - cplat_tracer_create から handle_ を返却する。
    EXPECT_CALL(mock_cplat_, cplat_tracer_get_state(handle_))
        .WillOnce(Return(CPLAT_TRACER_STATE_STOPPED))
        .WillOnce(Return(CPLAT_TRACER_STATE_STARTED))
        .WillOnce(Return(CPLAT_TRACER_STATE_STARTED))
        .WillOnce(Return(
            CPLAT_TRACER_STATE_STOPPED)); // [Pre-Assert確認_正常系] - cplat_tracer_get_state が 4 回参照されること。
    // [Pre-Assert手順] - 状態を STOPPED、STARTED、STARTED、STOPPED の順に返却する。
    EXPECT_CALL(mock_cplat_, cplat_tracer_start(handle_))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - start コマンドで cplat_tracer_start が 1 回呼び出されること。
                              // [Pre-Assert手順] - cplat_tracer_start から 0 を返却する。
    EXPECT_CALL(mock_cplat_, cplat_tracer_write_at(handle_, CPLAT_TRACE_LEVEL_INFO, nullptr, StrEq("hello world")))
        .WillOnce(
            Return(0)); // [Pre-Assert確認_正常系] - write コマンドで message "hello world" がそのまま渡されること。
                        // [Pre-Assert手順] - cplat_tracer_write_at から 0 を返却する。
    EXPECT_CALL(mock_cplat_, cplat_tracer_stop(handle_))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - stop コマンドで cplat_tracer_stop が 1 回呼び出されること。
                              // [Pre-Assert手順] - cplat_tracer_stop から 0 を返却する。
    EXPECT_CALL(mock_cplat_, cplat_tracer_dispose(Pointee(handle_)))
        .WillOnce(
            Return()); // [Pre-Assert確認_正常系] - dispose コマンドで handle_ を保持するポインターが 1 回渡されること。
    EXPECT_CALL(mock_cplat_, cplat_prompt_readline_fmt_at(prompt_handle, _, _, _, _, _, _))
        .WillRepeatedly(
            [&](cplat_prompt *, char *buf, size_t buf_size, const char *, int, const char *, va_list) -> int
            {
                if (index >= lines.size())
                {
                    return CPLAT_ERR_EOF;
                }
                copy_line(buf, (int)buf_size, lines[index++].c_str());
                return CPLAT_OK;
            }); // [Pre-Assert手順] - cplat_prompt_readline_fmt_at にて入力列を 1 行ずつ返却し、尽きたら CPLAT_ERR_EOF を返却する。
    // [Pre-Assert確認_正常系] - mock_cplat_ の cplat_prompt_readline_fmt_at(prompt_handle, _, _, _, _, _, _) が登録した呼び出し期待を満たすこと。
    EXPECT_CALL(mock_stdio_, printf(_, _, _, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(0)); // [Pre-Assert手順] - help を含むその他の stdout 出力を許容する。
    // [Pre-Assert確認_正常系] - mock_stdio_ の printf(_, _, _, _) が登録した呼び出し期待を満たすこと。
    EXPECT_CALL(mock_stdio_, printf(_, _, _, StrEq("handle=created\n")))
        .InSequence(io_seq)
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - create 結果として "handle=created" が出力されること。
    EXPECT_CALL(mock_stdio_, printf(_, _, _, StrEq(kRcSuccessTty)))
        .InSequence(io_seq)
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - start の戻り値が緑の "rc=0" として表示されること。
    EXPECT_CALL(mock_stdio_, printf(_, _, _, StrEq(kRcSuccessTty)))
        .InSequence(io_seq)
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - write の戻り値が緑の "rc=0" として表示されること。
    EXPECT_CALL(mock_stdio_, printf(_, _, _, StrEq(kRcSuccessTty)))
        .InSequence(io_seq)
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - stop の戻り値が緑の "rc=0" として表示されること。
    EXPECT_CALL(mock_stdio_, printf(_, _, _, StrEq("handle=disposed\n")))
        .InSequence(io_seq)
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - dispose 結果として "handle=disposed" が出力されること。

    // Act
    int rc = __real_main(argc, (char **)&argv); // [手順] - main() を引数なしで呼び出し、対話シーケンスを実行する。

    // Assert
    EXPECT_EQ(EXIT_SUCCESS, rc); // [確認_正常系] - main() の戻り値が EXIT_SUCCESS であること。
}
// [サブ手順参照 名前=trace_cliTest.TearDown]
