#include <testfw.h>
#include <mock_stdio.h>
#include <mock_cplat.h>
#include "etw-viewer.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>

using testing::_;
using testing::HasSubstr;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::StrEq;

namespace
{

static int emulate_cplat_strncpy(char *dest, size_t dest_size, const char *src, size_t count)
{
    size_t i;

    if (dest == nullptr || dest_size == 0U || src == nullptr)
    {
        return -1;
    }

    for (i = 0; i < count && i + 1U < dest_size && src[i] != '\0'; i++)
    {
        dest[i] = src[i];
    }
    dest[i] = '\0';
    return 0;
}

static int emulate_cplat_clock_format_realtime_iso8601_local(char *dest, size_t dest_size,
                                                             const cplat_timespec *timestamp)
{
    const char *text;

    if (dest == nullptr || dest_size == 0U)
    {
        return -1;
    }
    if (timestamp != nullptr && timestamp->tv_sec == 0 && timestamp->tv_nsec == 0)
    {
        text = "1970-01-01T09:00:00.000+09:00";
    }
    else
    {
        text = "2000-01-01T00:00:00.000+09:00";
    }

    return emulate_cplat_strncpy(dest, dest_size, text, strlen(text));
}

} // namespace

class etw_viewerTest : public Test
{
  protected:
    NiceMock<Mock_stdio> mock_stdio_;
    NiceMock<Mock_cplat> mock_cplat_;

    // [サブ手順 名前=etw_viewerTest.SetUp]
    void SetUp() override
    {
        ON_CALL(mock_cplat_, cplat_strncpy(_, _, _, _)).WillByDefault(emulate_cplat_strncpy);
        // [状態] - `cplat_strncpy` の既定動作を設定する。
        ON_CALL(mock_cplat_, cplat_clock_format_realtime_iso8601_local(_, _, _))
            .WillByDefault(emulate_cplat_clock_format_realtime_iso8601_local);
        // [状態] - `cplat_clock_format_realtime_iso8601_local` の既定動作を設定する。
    }
    // [サブ手順終了]
};

#if defined(PLATFORM_WINDOWS)

// pid フィルター引数が session 開始の context へ伝わることの確認
// [サブ手順参照 名前=etw_viewerTest.SetUp]
TEST_F(etw_viewerTest, main_accepts_pid_filter)
{
    // Arrange
    int argc = 3;
    const char *argv[] = {"etw-viewer", "--pid", "4321"}; // [状態] - main() に与える引数を "--pid 4321" とする。
    etw_viewer_context captured_context{};

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_console_init())
        .WillOnce(
            Return()); // [Pre-Assert確認_正常系] - main() 呼び出し時に cplat_console_init が 1 回呼び出されること。
    EXPECT_CALL(mock_cplat_, cplat_etw_session_check_access())
        .WillOnce(Return(CPLAT_OK)); // [Pre-Assert手順] - 権限確認から OK を返却して通過させる。
    // [Pre-Assert確認_正常系] - mock_cplat_ の cplat_etw_session_check_access() が登録した呼び出し期待を満たすこと。
    EXPECT_CALL(mock_cplat_, cplat_etw_session_start(_, _, _, _, _))
        .WillOnce(Invoke(
            [&captured_context](const char *, const char *, cplat_etw_event_fn, void *context,
                                cplat_etw_session **session_out)
            {
                captured_context = *static_cast<const etw_viewer_context *>(context);
                *session_out = nullptr;
                return CPLAT_ERR_INVALID_ARGUMENT;
            })); // [Pre-Assert確認_正常系] - cplat_etw_session_start が 1 回呼び出されること。
                 // [Pre-Assert手順] - session 開始に渡された context を捕捉し、失敗を返却して打ち切る。

    // Act
    int rc = __real_main(argc, (char **)&argv); // [手順] - pid フィルター付きで main を呼び出す。

    // Assert
    EXPECT_NE(EXIT_SUCCESS, rc); // [確認_正常系] - session 開始を失敗させたため EXIT_SUCCESS 以外で終了すること。
    EXPECT_EQ(1, captured_context.has_process_id_filter); // [確認_正常系] - pid フィルター有効が context へ伝わること。
    EXPECT_EQ((uint32_t)4321,
              captured_context.process_id_filter); // [確認_正常系] - pid フィルター値 4321 が context へ伝わること。
}

// 数値でない pid 指定が拒否されることの確認
// [サブ手順参照 名前=etw_viewerTest.SetUp]
TEST_F(etw_viewerTest, main_rejects_invalid_pid)
{
    // Arrange
    int argc = 3;
    const char *argv[] = {"etw-viewer", "--pid",
                          "abc"}; // [状態] - main() に与える引数を数値でない "--pid abc" とする。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_console_init())
        .WillOnce(
            Return()); // [Pre-Assert確認_正常系] - main() 呼び出し時に cplat_console_init が 1 回呼び出されること。

    // Act
    int rc = __real_main(argc, (char **)&argv); // [手順] - 不正な pid で main を呼び出す。

    // Assert
    EXPECT_NE(EXIT_SUCCESS, rc); // [確認_異常系] - 数値でない pid は拒否されること。
}

// 既定 session 名に process id が埋め込まれることの確認
// [サブ手順参照 名前=etw_viewerTest.SetUp]
TEST_F(etw_viewerTest, build_default_session_name_formats_process_id)
{
    // Arrange
    char buffer[ETW_VIEWER_SESSION_NAME_MAX];

    // Pre-Assert

    // Act
    int rc =
        etw_viewer_build_default_session_name(4321UL, buffer, sizeof(buffer)); // [手順] - 既定 session 名を生成する。

    // Assert
    EXPECT_EQ(
        0,
        rc); // [確認_正常系] - etw_viewer_build_default_session_name の戻り値から、session 名生成が成功したと判断できること。
    EXPECT_STREQ("etw-viewer_4321", buffer); // [確認_正常系] - process id を含む名前になること。
}

// FILETIME がローカル ISO 8601 文字列へ変換されることの確認
// [サブ手順参照 名前=etw_viewerTest.SetUp]
TEST_F(etw_viewerTest, format_timestamp_utc_formats_filetime)
{
    // Arrange
    char buffer[64];

    // Pre-Assert

    // Act
    int rc = etw_viewer_format_timestamp_utc(116444736000000000LL, buffer,
                                             sizeof(buffer)); // [手順] - FILETIME epoch を UTC 文字列へ変換する。

    // Assert
    EXPECT_EQ(
        0,
        rc); // [確認_正常系] - etw_viewer_format_timestamp_utc の戻り値から、タイムスタンプ変換が成功したと判断できること。
    EXPECT_STREQ("1970-01-01T09:00:00.000+09:00", buffer); // [確認_正常系] - ローカル ISO8601 形式へ変換されること。
}

// Service ありイベントが syslog 互換形式で表示されることの確認
// [サブ手順参照 名前=etw_viewerTest.SetUp]
TEST_F(etw_viewerTest, handle_event_prints_service_and_message)
{
    // Arrange
    cplat_etw_event event = {4, 1234U, "Trace", "worker-7", "started", 116444736000000000LL};

    // Pre-Assert
    EXPECT_CALL(mock_stdio_, printf(_, _, _, StrEq("1970-01-01T09:00:00.000+09:00 <I>worker-7[1234]: started\n")))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - syslog 互換形式で出力されること。
    EXPECT_CALL(mock_stdio_, fflush(_, _, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 出力後に flush されること。

    // Act
    etw_viewer_handle_event(&event, NULL); // [手順] - Service ありイベントを表示する。

    // Assert
}

// Service なしイベントが既定 tag で表示されることの確認
// [サブ手順参照 名前=etw_viewerTest.SetUp]
TEST_F(etw_viewerTest, handle_event_prints_message_without_service)
{
    // Arrange
    cplat_etw_event event = {3, 5678U, "Trace", NULL, "degraded", 116444736000000000LL};

    // Pre-Assert
    EXPECT_CALL(mock_stdio_, printf(_, _, _, StrEq("1970-01-01T09:00:00.000+09:00 <W>cplat.tracer[5678]: degraded\n")))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - Service なしでは既定 tag を使って出力されること。
    EXPECT_CALL(mock_stdio_, fflush(_, _, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - 出力後に flush されること。

    // Act
    etw_viewer_handle_event(&event, NULL); // [手順] - Service なしイベントを表示する。

    // Assert
}

// Trace 以外のイベントが表示されないことの確認
// [サブ手順参照 名前=etw_viewerTest.SetUp]
TEST_F(etw_viewerTest, handle_event_skips_non_trace_event)
{
    // Arrange
    cplat_etw_event event = {5, 9012U, "EventMetadata", NULL, NULL, 116444736000000000LL};

    // Pre-Assert
    EXPECT_CALL(mock_stdio_, printf(_, _, _, _)).Times(0); // [Pre-Assert確認_正常系] - Trace 以外は表示しないこと。
    EXPECT_CALL(mock_stdio_, fflush(_, _, _, _)).Times(0); // [Pre-Assert確認_正常系] - Trace 以外は flush しないこと。

    // Act
    etw_viewer_handle_event(&event, NULL); // [手順] - Trace 以外のイベントを表示処理へ渡す。

    // Assert
}

// pid フィルター不一致のイベントが表示されないことの確認
// [サブ手順参照 名前=etw_viewerTest.SetUp]
TEST_F(etw_viewerTest, handle_event_skips_non_matching_pid_filter)
{
    // Arrange
    cplat_etw_event event = {4, 2222U, "Trace", NULL, "filtered", 116444736000000000LL};
    etw_viewer_context context = {1111U, 1};

    // Pre-Assert
    EXPECT_CALL(mock_stdio_, printf(_, _, _, _)).Times(0); // [Pre-Assert確認_正常系] - 不一致 pid は出力されないこと。
    EXPECT_CALL(mock_stdio_, fflush(_, _, _, _))
        .Times(0); // [Pre-Assert確認_正常系] - 不一致 pid は flush もしないこと。

    // Act
    etw_viewer_handle_event(&event, &context); // [手順] - pid フィルター不一致イベントを表示処理へ渡す。

    // Assert
}

// 未対応オプション指定時に main() が失敗終了することの確認
// [サブ手順参照 名前=etw_viewerTest.SetUp]
TEST_F(etw_viewerTest, main_rejects_invalid_arguments)
{
    // Arrange
    int argc = 2;
    const char *argv[] = {"etw-viewer",
                          "--unknown"}; // [状態] - main() に与える引数を未対応オプション "--unknown" とする。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_console_init())
        .WillOnce(
            Return()); // [Pre-Assert確認_正常系] - main() 呼び出し時に cplat_console_init が 1 回呼び出されること。

    // Act
    int rc = __real_main(argc, (char **)&argv); // [手順] - 不正引数で main を呼び出す。

    // Assert
    EXPECT_NE(EXIT_SUCCESS, rc); // [確認_異常系] - 不正引数で失敗終了すること。
}

// help オプション指定時に usage を表示して正常終了することの確認
// [サブ手順参照 名前=etw_viewerTest.SetUp]
TEST_F(etw_viewerTest, main_prints_usage_on_help)
{
    // Arrange
    const char *argv[] = {"etw-viewer", "-h"}; // [状態] - main() に与える引数を "-h" のみとする。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_console_init())
        .WillOnce(
            Return()); // [Pre-Assert確認_正常系] - main() 呼び出し時に cplat_console_init が 1 回呼び出されること。

    // Act
    int rc = __real_main(2, (char **)&argv); // [手順] - help オプションで main() を呼び出す。

    // Assert
    EXPECT_EQ(EXIT_SUCCESS, rc); // [確認_正常系] - help の表示後に正常終了すること。
}

// 権限不足の場合に案内メッセージを表示して失敗終了することの確認
// [サブ手順参照 名前=etw_viewerTest.SetUp]
TEST_F(etw_viewerTest, main_stops_when_access_is_denied)
{
    // Arrange
    int argc = 1;
    const char *argv[] = {"etw-viewer"}; // [状態] - main() に与える引数をプログラム名のみとする。

    // Pre-Assert
    EXPECT_CALL(mock_cplat_, cplat_console_init())
        .WillOnce(
            Return()); // [Pre-Assert確認_正常系] - main() 呼び出し時に cplat_console_init が 1 回呼び出されること。
    EXPECT_CALL(mock_cplat_, cplat_etw_session_check_access())
        .WillOnce(Return(
            CPLAT_ERR_PERMISSION_DENIED)); // [Pre-Assert確認_異常系] - cplat_etw_session_check_access が 1 回呼び出されること。
                                           // [Pre-Assert手順] - 権限確認から ERR_ACCESS を返却する。
    EXPECT_CALL(mock_stdio_, fprintf(_, _, _, _, HasSubstr("Performance Log Users")))
        .WillOnce(
            Return(0)); // [Pre-Assert確認_異常系] - "Performance Log Users" を含む案内メッセージが表示されること。

    // Act
    int rc = __real_main(argc, (char **)&argv); // [手順] - 権限不足の状態で main を呼び出す。

    // Assert
    EXPECT_NE(EXIT_SUCCESS, rc); // [確認_異常系] - 権限不足で失敗終了すること。
}

#endif
