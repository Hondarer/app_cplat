#include <testfw.h>
#include <mock_cplat.h>

#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include <cplat/service/service.h>
#include <cplat/service/service_internal.h>

using testing::_;
using testing::AnyNumber;
using testing::HasSubstr;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;

namespace
{

std::vector<std::string> g_calls;
std::vector<std::string> g_status_texts;
std::vector<cplat_service_event_info> g_events;
std::vector<std::string> g_event_session_ids;
int g_os_prepare_ret = CPLAT_OK;
int g_os_install_ret = CPLAT_OK;
int g_os_uninstall_ret = CPLAT_OK;
int g_os_run_ret = CPLAT_OK;
int g_os_run_exit_code = 0;
int g_on_start_rc = 0;
int g_on_run_rc = 0;
int g_on_stop_rc = 0;
std::function<void()> g_on_run_action;

void reset_state()
{
    g_calls.clear();
    g_status_texts.clear();
    g_events.clear();
    g_event_session_ids.clear();
    g_os_prepare_ret = CPLAT_OK;
    g_os_install_ret = CPLAT_OK;
    g_os_uninstall_ret = CPLAT_OK;
    g_os_run_ret = CPLAT_OK;
    g_os_run_exit_code = 0;
    g_on_start_rc = 0;
    g_on_run_rc = 0;
    g_on_stop_rc = 0;
    g_on_run_action = nullptr;
}

extern "C"
{
    static int test_on_start(void *user_data)
    {
        (void)user_data;
        g_calls.push_back("on_start");
        return g_on_start_rc;
    }

    static int test_on_run(void *user_data)
    {
        (void)user_data;
        g_calls.push_back("on_run");
        if (g_on_run_action)
        {
            g_on_run_action();
        }
        return g_on_run_rc;
    }

    static int test_on_stop(void *user_data)
    {
        (void)user_data;
        g_calls.push_back("on_stop");
        return g_on_stop_rc;
    }

    static void test_on_event(const cplat_service_event_info *info, void *user_data)
    {
        (void)user_data;
        g_calls.push_back("on_event");
        g_events.push_back(*info);
        if (info->session_id != nullptr)
        {
            g_event_session_ids.push_back(info->session_id);
        }
        else
        {
            g_event_session_ids.push_back("");
        }
    }

    static void test_on_reload(void *user_data)
    {
        (void)user_data;
        g_calls.push_back("on_reload");
    }

    int cplat_internal_service_os_prepare_run(void)
    {
        g_calls.push_back("os_prepare_run");
        return g_os_prepare_ret;
    }

    int cplat_internal_service_os_install(const cplat_service_definition *def,
                                          const cplat_service_install_options *options)
    {
        (void)def;
        (void)options;
        g_calls.push_back("os_install");
        return g_os_install_ret;
    }

    int cplat_internal_service_os_uninstall(const cplat_service_definition *def)
    {
        (void)def;
        g_calls.push_back("os_uninstall");
        return g_os_uninstall_ret;
    }

    int cplat_internal_service_os_run(const cplat_service_definition *def, int *exit_code_out)
    {
        (void)def;
        g_calls.push_back("os_run");
        if (exit_code_out != nullptr)
        {
            *exit_code_out = g_os_run_exit_code;
        }
        return g_os_run_ret;
    }

    void cplat_internal_service_os_notify_ready(void)
    {
        g_calls.push_back("notify_ready");
    }

    void cplat_internal_service_os_notify_stopping(void)
    {
        g_calls.push_back("notify_stopping");
    }

    void cplat_internal_service_os_notify_reloading(void)
    {
        g_calls.push_back("notify_reloading");
    }

    void cplat_internal_service_os_notify_status(const char *text)
    {
        g_calls.push_back("notify_status");
        if (text != nullptr)
        {
            g_status_texts.push_back(text);
        }
    }
}

cplat_service_definition make_def()
{
    cplat_service_definition def = {};
    def.name = "test-svc";
    def.display_name = "Test Service";
    def.description = "Test Description";
    def.on_start = test_on_start;
    def.on_run = test_on_run;
    def.on_stop = test_on_stop;
    def.on_event = test_on_event;
    def.on_reload = test_on_reload;
    def.tracer = nullptr;
    return def;
}

class serviceCommonTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        reset_state();
    }

    void TearDown() override
    {
        cplat_internal_service_stop_dispose();
    }
};

} // namespace

// cplat_service_prepare_run が OS フックへ委譲することの確認
TEST_F(serviceCommonTest, prepare_run_delegates_to_os_hook)
{
    // Arrange
    g_os_prepare_ret = CPLAT_OK;

    // Pre-Assert

    // Act
    int actual_ret = cplat_service_prepare_run(); // [手順] - cplat_service_prepare_run() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_service_prepare_run() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(1U, g_calls.size());   // [確認_正常系] - g_calls.size() の戻り値が 1U であること。
    EXPECT_EQ("os_prepare_run", g_calls[0]); // [確認_正常系] - g_calls[0] が "os_prepare_run" であること。
}

// cplat_service_install の引数検証 (NULL・不正文字・改行入り run_args) の確認
TEST_F(serviceCommonTest, install_validates_arguments)
{
    // Arrange
    cplat_service_definition def = make_def();
    cplat_service_install_options opts = {};
    opts.run_args = "run\n--bad";

    // Pre-Assert

    // Act
    int actual_ret_null = cplat_service_install(
        nullptr, nullptr); // [手順] - def に nullptr を指定して cplat_service_install() を呼び出す。
    def.name = "bad/name";
    int actual_ret_slash = cplat_service_install(
        &def, nullptr); // [手順] - スラッシュを含む名前を指定して cplat_service_install() を呼び出す。
    def.name = "good-name";
    int actual_ret_newline = cplat_service_install(
        &def, &opts); // [手順] - 改行を含む run_args を指定して cplat_service_install() を呼び出す。
    opts.run_args = "--foreground";
    int actual_ret_ok =
        cplat_service_install(&def, &opts); // [手順] - 妥当な引数を指定して cplat_service_install() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null); // [確認_異常系] - def が nullptr の場合の cplat_service_install() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_slash); // [確認_異常系] - スラッシュ入り名前の cplat_service_install() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_newline); // [確認_異常系] - 改行入り run_args の cplat_service_install() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_OK,
              actual_ret_ok); // [確認_正常系] - 妥当な引数の cplat_service_install() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        1U,
        g_calls
            .size()); // [確認_正常系] - g_calls.size() の戻り値が 1U であること。(妥当な 1 回のみ OS フックが呼ばれることの確認)
}

// cplat_service_uninstall の引数検証と委譲の確認
TEST_F(serviceCommonTest, uninstall_validates_and_delegates)
{
    // Arrange
    cplat_service_definition def = make_def();

    // Pre-Assert

    // Act
    int actual_ret_null =
        cplat_service_uninstall(nullptr); // [手順] - def に nullptr を指定して cplat_service_uninstall() を呼び出す。
    def.name = "";
    int actual_ret_empty =
        cplat_service_uninstall(&def); // [手順] - 空文字名を指定して cplat_service_uninstall() を呼び出す。
    def.name = "test-svc";
    int actual_ret_ok =
        cplat_service_uninstall(&def); // [手順] - 妥当な定義を指定して cplat_service_uninstall() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_null); // [確認_異常系] - def が nullptr の場合の cplat_service_uninstall() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_empty); // [確認_異常系] - 空文字名の cplat_service_uninstall() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(CPLAT_OK,
              actual_ret_ok); // [確認_正常系] - 妥当な定義の cplat_service_uninstall() の戻り値が CPLAT_OK であること。
}

// cplat_service_run_console の正常ライフサイクルと exit_code_out の確認
TEST_F(serviceCommonTest, run_console_executes_full_lifecycle)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_service_definition def = make_def();
    int exit_code = -1;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_console_init())
        .Times(1); // [Pre-Assert確認_正常系] - cplat_console_init() が 1 回呼び出されること。
    EXPECT_CALL(mock_cplat, cplat_shutdown_request_register(_, _))
        .WillOnce(
            Return(CPLAT_OK)); // [Pre-Assert確認_正常系] - cplat_shutdown_request_register() が 1 回呼び出されること。
                               // [Pre-Assert手順] - cplat_shutdown_request_register() から CPLAT_OK を返す。

    // Act
    int actual_ret = cplat_service_run_console(
        &def, &exit_code); // [手順] - コンソール モードで cplat_service_run_console() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_ret);   // [確認_正常系] - cplat_service_run_console() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, exit_code); // [確認_正常系] - exit_code が 0 であること。
    std::vector<std::string> expected = {"on_start", "notify_ready", "on_run", "notify_stopping", "on_stop"};
    EXPECT_EQ(expected, g_calls); // [確認_正常系] - g_calls が expected と一致すること。(規定順序で呼ばれることの確認)
}

// cplat_service_run_console で on_start 失敗時に後続処理を行わず exit_code_out へ失敗値を返すことの確認
TEST_F(serviceCommonTest, run_console_on_start_failure)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_service_definition def = make_def();
    int exit_code = -1;
    g_on_start_rc = 7; // [状態] - on_start が失敗 (7) を返すように設定する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_console_init())
        .Times(1); // [Pre-Assert確認_異常系] - cplat_console_init() が 1 回呼び出されること。
    EXPECT_CALL(mock_cplat, cplat_shutdown_request_register(_, _))
        .WillOnce(
            Return(CPLAT_OK)); // [Pre-Assert確認_異常系] - cplat_shutdown_request_register() が 1 回呼び出されること。
                               // [Pre-Assert手順] - cplat_shutdown_request_register() から CPLAT_OK を返す。

    // Act
    int actual_ret = cplat_service_run_console(
        &def, &exit_code); // [手順] - on_start が失敗する状態で cplat_service_run_console() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_異常系] - cplat_service_run_console() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(7, exit_code); // [確認_異常系] - exit_code が 7 であること。(on_start の戻り値が格納されることの確認)
    ASSERT_EQ(1U, g_calls.size()); // [確認_異常系] - g_calls.size() の戻り値が 1U であること。
    EXPECT_EQ("on_start",
              g_calls[0]); // [確認_異常系] - g_calls[0] が "on_start" であること。(後続が呼ばれないことの確認)
}

// cplat_service_run_console で on_run 失敗時も停止処理を行い exit_code_out へ失敗値を返すことの確認
TEST_F(serviceCommonTest, run_console_on_run_failure)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_service_definition def = make_def();
    int exit_code = -1;
    g_on_run_rc = 2; // [状態] - on_run が失敗 (2) を返すように設定する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_console_init())
        .Times(1); // [Pre-Assert確認_異常系] - cplat_console_init() が 1 回呼び出されること。
    EXPECT_CALL(mock_cplat, cplat_shutdown_request_register(_, _))
        .WillOnce(
            Return(CPLAT_OK)); // [Pre-Assert確認_異常系] - cplat_shutdown_request_register() が 1 回呼び出されること。
                               // [Pre-Assert手順] - cplat_shutdown_request_register() から CPLAT_OK を返す。

    // Act
    int actual_ret = cplat_service_run_console(
        &def, &exit_code); // [手順] - on_run が失敗する状態で cplat_service_run_console() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_異常系] - cplat_service_run_console() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(2, exit_code);         // [確認_異常系] - exit_code が 2 であること。
    ASSERT_EQ(5U, g_calls.size());   // [確認_異常系] - g_calls.size() の戻り値が 5U であること。
    EXPECT_EQ(
        "notify_stopping",
        g_calls
            [3]); // [確認_異常系] - g_calls[3] が "notify_stopping" であること。(on_run 失敗後も停止開始が通知されることの確認)
    EXPECT_EQ(
        "on_stop",
        g_calls
            [4]); // [確認_異常系] - g_calls[4] が "on_stop" であること。(on_run 失敗後も on_stop が呼ばれることの確認)
}

// cplat_service_run_console で on_stop 失敗時に exit_code_out へ失敗値を返すことの確認
TEST_F(serviceCommonTest, run_console_on_stop_failure)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_service_definition def = make_def();
    int exit_code = -1;
    g_on_stop_rc = 3; // [状態] - on_stop が失敗 (3) を返すように設定する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_console_init())
        .Times(1); // [Pre-Assert確認_異常系] - cplat_console_init() が 1 回呼び出されること。
    EXPECT_CALL(mock_cplat, cplat_shutdown_request_register(_, _))
        .WillOnce(
            Return(CPLAT_OK)); // [Pre-Assert確認_異常系] - cplat_shutdown_request_register() が 1 回呼び出されること。
                               // [Pre-Assert手順] - cplat_shutdown_request_register() から CPLAT_OK を返す。

    // Act
    int actual_ret = cplat_service_run_console(
        &def, &exit_code); // [手順] - on_stop が失敗する状態で cplat_service_run_console() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_異常系] - cplat_service_run_console() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(3, exit_code);         // [確認_異常系] - exit_code が 3 であること。
    ASSERT_EQ(
        5U,
        g_calls
            .size()); // [確認_異常系] - g_calls.size() の戻り値が 5U であること。(ライフサイクル全体が実行されることの確認)
}

// 停止要求の登録に失敗した場合に、ERROR トレースを出しコールバックを呼ばずエラーを返すことの確認
TEST_F(serviceCommonTest, run_console_fails_when_shutdown_request_register_fails)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_service_definition def = make_def();
    int exit_code = -1;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_shutdown_request_register(_, _))
        .WillOnce(Return(
            CPLAT_ERR_OUT_OF_MEMORY)); // [Pre-Assert確認_異常系] - cplat_shutdown_request_register() が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_shutdown_request_register() から CPLAT_ERR_OUT_OF_MEMORY を返す。
    EXPECT_CALL(mock_cplat, cplat_tracer_writef_at(_, _, _, _)).Times(AnyNumber());
    EXPECT_CALL(mock_cplat,
                cplat_tracer_writef_at(_, CPLAT_TRACE_LEVEL_ERROR, _, HasSubstr("停止要求の登録に失敗しました")))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - 登録の失敗を示す ERROR トレースが 1 回出力されること。
                              // [Pre-Assert手順] - cplat_tracer_writef_at() から 0 を返す。

    // Act
    int actual_ret = cplat_service_run_console(
        &def, &exit_code); // [手順] - 停止要求登録が失敗する状態で cplat_service_run_console() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_OUT_OF_MEMORY,
        actual_ret); // [確認_異常系] - cplat_service_run_console() の戻り値が CPLAT_ERR_OUT_OF_MEMORY であること。
    EXPECT_TRUE(
        g_calls.empty()); // [確認_異常系] - g_calls.empty() の戻り値が true であること。(on_start を呼ばないことの確認)
}

// on_start 失敗時に on_run と on_stop が呼ばれないことの確認
TEST_F(serviceCommonTest, run_callbacks_aborts_when_on_start_fails)
{
    // Arrange
    cplat_service_definition def = make_def();
    g_on_start_rc = 7;

    // Pre-Assert

    // Act
    int actual_rc = cplat_internal_service_run_callbacks(
        &def); // [手順] - on_start が 7 を返す状態で cplat_internal_service_run_callbacks() を呼び出す。

    // Assert
    EXPECT_EQ(7, actual_rc); // [確認_異常系] - cplat_internal_service_run_callbacks() の戻り値が 7 であること。
    std::vector<std::string> expected = {"on_start"};
    EXPECT_EQ(
        expected,
        g_calls); // [確認_異常系] - g_calls が expected と一致すること。(on_run と on_stop が呼ばれないことの確認)
}

// on_start が nullptr のとき、起動完了通知の後の最初のコールバックが on_run であることの確認
TEST_F(serviceCommonTest, run_callbacks_starts_at_on_run_when_on_start_is_null)
{
    // Arrange
    cplat_service_definition def = make_def();
    def.on_start = nullptr; // [状態] - on_start が nullptr の定義とする。

    // Pre-Assert

    // Act
    int actual_rc = cplat_internal_service_run_callbacks(
        &def); // [手順] - on_start が nullptr の定義で cplat_internal_service_run_callbacks() を呼び出す。

    // Assert
    EXPECT_EQ(0, actual_rc); // [確認_正常系] - cplat_internal_service_run_callbacks() の戻り値が 0 であること。
    std::vector<std::string> expected = {"notify_ready", "on_run", "notify_stopping", "on_stop"};
    EXPECT_EQ(expected, g_calls); // [確認_正常系] - g_calls が expected と一致すること。(on_start を呼ばないことの確認)
}

// on_stop が nullptr のとき、on_run の後に停止開始通知だけが呼ばれることの確認
TEST_F(serviceCommonTest, run_callbacks_notifies_stopping_when_on_stop_is_null)
{
    // Arrange
    cplat_service_definition def = make_def();
    def.on_stop = nullptr; // [状態] - on_stop が nullptr の定義とする。

    // Pre-Assert

    // Act
    int actual_rc = cplat_internal_service_run_callbacks(
        &def); // [手順] - on_stop が nullptr の定義で cplat_internal_service_run_callbacks() を呼び出す。

    // Assert
    EXPECT_EQ(0, actual_rc); // [確認_正常系] - cplat_internal_service_run_callbacks() の戻り値が 0 であること。
    std::vector<std::string> expected = {"on_start", "notify_ready", "on_run", "notify_stopping"};
    EXPECT_EQ(expected,
              g_calls); // [確認_正常系] - g_calls が expected と一致すること。(on_stop が呼ばれないことの確認)
}

// on_run と on_stop の双方が失敗したときに on_stop が実行され on_run の戻り値が採用されることの確認
TEST_F(serviceCommonTest, run_callbacks_calls_on_stop_and_keeps_on_run_error)
{
    // Arrange
    cplat_service_definition def = make_def();
    g_on_run_rc = 11;
    g_on_stop_rc = 22;

    // Pre-Assert

    // Act
    int actual_rc = cplat_internal_service_run_callbacks(
        &def); // [手順] - on_run=11、on_stop=22 で cplat_internal_service_run_callbacks() を呼び出す。

    // Assert
    EXPECT_EQ(
        11,
        actual_rc); // [確認_異常系] - cplat_internal_service_run_callbacks() の戻り値が 11 であること。(on_run の戻り値が採用されることの確認)
    std::vector<std::string> expected = {"on_start", "notify_ready", "on_run", "notify_stopping", "on_stop"};
    EXPECT_EQ(expected,
              g_calls); // [確認_正常系] - g_calls が expected と一致すること。(on_stop まで実行されることの確認)
}

// 未初期化時の停止 API が要求を記録せず待機で CPLAT_ERR_INVALID_ARGUMENT を返すことの確認
TEST_F(serviceCommonTest, stop_api_before_initialization)
{
    // Arrange
    // [状態] - 停止抽象が未初期化の状態とする。

    // Pre-Assert

    // Act
    cplat_service_request_stop(); // [手順] - 未初期化の状態で cplat_service_request_stop() を呼び出す。
    int actual_requested =
        cplat_service_stop_requested(); // [手順] - 未初期化の状態で cplat_service_stop_requested() を呼び出す。
    int actual_wait = cplat_service_wait_for_stop(
        10); // [手順] - 10 を指定して未初期化の状態で cplat_service_wait_for_stop() を呼び出す。

    // Assert
    EXPECT_EQ(
        0,
        actual_requested); // [確認_異常系] - cplat_service_stop_requested() の戻り値が 0 であること。(未初期化のため停止要求が記録されないことの確認)
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_wait); // [確認_異常系] - cplat_service_wait_for_stop() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// 停止抽象 API (wait_for_stop / stop_requested / request_stop / progress) の契約確認
TEST_F(serviceCommonTest, stop_abstraction_handles_timeout_request_and_progress)
{
    // Arrange
    uint32_t seen = 0U;
    EXPECT_EQ(
        0,
        cplat_service_stop_requested()); // [状態確認] - 未初期化時の cplat_service_stop_requested() の戻り値が 0 であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        cplat_service_wait_for_stop(
            0)); // [状態確認] - 未初期化時の cplat_service_wait_for_stop() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。

    // Pre-Assert

    // Act
    int actual_init = cplat_internal_service_stop_init(); // [手順] - cplat_internal_service_stop_init() を呼び出す。
    int actual_neg =
        cplat_service_wait_for_stop(-1); // [手順] - -1 を指定して cplat_service_wait_for_stop() を呼び出す。
    int actual_timeout = cplat_service_wait_for_stop(
        CPLAT_NO_WAIT); // [手順] - CPLAT_NO_WAIT を指定して cplat_service_wait_for_stop() を呼び出す。
    int actual_prog1 = cplat_internal_service_on_run_made_progress(
        &seen); // [手順] - 待機後に 1 回目の cplat_internal_service_on_run_made_progress() を呼び出す。
    int actual_prog2 = cplat_internal_service_on_run_made_progress(
        &seen); // [手順] - 続けて 2 回目の cplat_internal_service_on_run_made_progress() を呼び出す。

    std::thread stopper(
        []()
        {
            cplat_sleep_ms(20);
            cplat_service_request_stop();
        });
    int actual_forever = cplat_service_wait_for_stop(
        CPLAT_WAIT_FOREVER); // [手順] - CPLAT_WAIT_FOREVER を指定して cplat_service_wait_for_stop() を呼び出す。
    stopper.join();
    int actual_req = cplat_service_stop_requested(); // [手順] - cplat_service_stop_requested() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              actual_init); // [確認_正常系] - cplat_internal_service_stop_init() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_neg); // [確認_異常系] - -1 を指定した cplat_service_wait_for_stop() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_TIMEOUT,
        actual_timeout); // [確認_正常系] - CPLAT_NO_WAIT を指定した cplat_service_wait_for_stop() の戻り値が CPLAT_ERR_TIMEOUT であること。
    EXPECT_EQ(
        1,
        actual_prog1); // [確認_正常系] - 1 回目の cplat_internal_service_on_run_made_progress() の戻り値が 1 であること。
    EXPECT_EQ(
        0,
        actual_prog2); // [確認_正常系] - 2 回目の cplat_internal_service_on_run_made_progress() の戻り値が 0 であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_forever); // [確認_正常系] - CPLAT_WAIT_FOREVER を指定した cplat_service_wait_for_stop() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(1, actual_req); // [確認_正常系] - cplat_service_stop_requested() の戻り値が 1 であること。
}

// 停止要求の待機に失敗した場合に、ERROR トレースを出してエラー コードを返し停止要求状態を変えないことの確認
TEST_F(serviceCommonTest, wait_for_stop_returns_error_when_wait_fails)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_service_definition def = make_def();
    int exit_code = -1;
    int wait_result = CPLAT_OK;
    int requested = -1;
    g_on_run_action = [&wait_result, &requested]()
    {
        wait_result = cplat_service_wait_for_stop(1000);
        requested = cplat_service_stop_requested();
    }; // [状態] - on_run の中で cplat_service_wait_for_stop(1000) と cplat_service_stop_requested() を呼ぶ。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_condvar_timed_wait(_, _, 1000))
        .WillOnce(Return(
            CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_異常系] - cplat_condvar_timed_wait() が 1000 を指定して 1 回呼び出されること。
                                 // [Pre-Assert手順] - cplat_condvar_timed_wait() から CPLAT_ERR_UNKNOWN を返す。
    EXPECT_CALL(mock_cplat, cplat_tracer_writef_at(_, _, _, _)).Times(AnyNumber());
    EXPECT_CALL(mock_cplat,
                cplat_tracer_writef_at(_, CPLAT_TRACE_LEVEL_ERROR, _, HasSubstr("停止要求の待機に失敗しました")))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - 待機の失敗を示す ERROR トレースが 1 回出力されること。
                              // [Pre-Assert手順] - cplat_tracer_writef_at() から 0 を返す。

    // Act
    int actual_ret = cplat_service_run_console(&def, &exit_code); // [手順] - cplat_service_run_console() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              wait_result); // [確認_異常系] - cplat_service_wait_for_stop() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        0,
        requested); // [確認_異常系] - cplat_service_stop_requested() の戻り値が 0 であること。(停止要求の状態を変えないことの確認)
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_service_run_console() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(0, exit_code);         // [確認_正常系] - exit_code が 0 であること。
}

// cplat_service_wait_for_stop() で待機している間は on_run() が進行しているものとして扱うことの確認
TEST_F(serviceCommonTest, on_run_progress_is_alive_while_waiting)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_service_definition def = make_def();
    int exit_code = -1;
    int progress_while_waiting = -1;
    g_on_run_action = []()
    { (void)cplat_service_wait_for_stop(1000); }; // [状態] - on_run の中で cplat_service_wait_for_stop(1000) を呼ぶ。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_condvar_timed_wait(_, _, 1000))
        .WillOnce(Invoke(
            [&progress_while_waiting](cplat_condvar *, cplat_local_lock *, int) -> int
            {
                uint32_t seen_count = 0U;
                (void)cplat_internal_service_on_run_made_progress(&seen_count);
                progress_while_waiting = cplat_internal_service_on_run_made_progress(&seen_count);
                return CPLAT_ERR_TIMEOUT;
            })); // [Pre-Assert確認_正常系] - cplat_condvar_timed_wait() が 1000 を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - 待機中に計数を記録した後で cplat_internal_service_on_run_made_progress() を呼び、CPLAT_ERR_TIMEOUT を返す。

    // Act
    int actual_ret = cplat_service_run_console(&def, &exit_code); // [手順] - cplat_service_run_console() を呼び出す。

    // Assert
    EXPECT_EQ(
        1,
        progress_while_waiting); // [確認_正常系] - 待機中の cplat_internal_service_on_run_made_progress() の戻り値が 1 であること。(計数が変わらなくても待機中は進行として扱うことの確認)
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_service_run_console() の戻り値が CPLAT_OK であること。
}

// NULL や on_event 未設定ではイベントを配送しないことの確認
TEST_F(serviceCommonTest, dispatch_event_null_safety)
{
    // Arrange
    cplat_service_event_info info = {CPLAT_SERVICE_EVENT_POWER_SUSPEND, 0U, nullptr};
    cplat_service_definition def = make_def();
    cplat_service_definition def_without_event = def;
    def_without_event.on_event = nullptr; // [状態] - on_event が nullptr のサービス定義を用意する。

    // Pre-Assert

    // Act
    cplat_internal_service_dispatch_event(
        nullptr, &info); // [手順] - def に nullptr を指定して cplat_internal_service_dispatch_event() を呼び出す。

    // Assert
    EXPECT_TRUE(
        g_calls.empty()); // [確認_異常系] - def が nullptr の場合の g_calls.empty() の戻り値が true であること。

    // Act_2
    cplat_internal_service_dispatch_event(
        &def, nullptr); // [手順] - info に nullptr を指定して cplat_internal_service_dispatch_event() を呼び出す。

    // Assert_2
    EXPECT_TRUE(
        g_calls.empty()); // [確認_異常系] - info が nullptr の場合の g_calls.empty() の戻り値が true であること。

    // Act_3
    cplat_internal_service_dispatch_event(
        &def_without_event,
        &info); // [手順] - on_event が nullptr の定義を指定して cplat_internal_service_dispatch_event() を呼び出す。

    // Assert_3
    EXPECT_TRUE(
        g_calls.empty()); // [確認_異常系] - on_event が nullptr の場合の g_calls.empty() の戻り値が true であること。
}

// イベント情報 (種別とセッション ID) が on_event へ渡ることの確認
TEST_F(serviceCommonTest, dispatch_event_passes_info)
{
    // Arrange
    cplat_service_definition def = make_def();
    cplat_service_event_info info = {CPLAT_SERVICE_EVENT_SESSION_LOGON, 0U,
                                     "42"}; // [状態] - セッション ログオン イベント (ID: "42") を用意する。

    // Pre-Assert

    // Act
    cplat_internal_service_dispatch_event(
        &def,
        &info); // [手順] - セッション ログオン イベントを指定して cplat_internal_service_dispatch_event() を呼び出す。

    // Assert
    ASSERT_EQ(1U, g_events.size()); // [確認_正常系] - g_events.size() の戻り値が 1U であること。
    EXPECT_EQ(CPLAT_SERVICE_EVENT_SESSION_LOGON,
              g_events[0].type); // [確認_正常系] - g_events[0].type が CPLAT_SERVICE_EVENT_SESSION_LOGON であること。
    EXPECT_EQ("42", g_event_session_ids[0]); // [確認_正常系] - g_event_session_ids[0] が "42" であること。
}

// セッション ID なしのイベントが nullptr のまま on_event へ渡ることの確認
TEST_F(serviceCommonTest, dispatch_event_without_session_id)
{
    // Arrange
    cplat_service_definition def = make_def();
    cplat_service_event_info info = {CPLAT_SERVICE_EVENT_PRESHUTDOWN, 0U,
                                     nullptr}; // [状態] - session_id が nullptr のイベントを用意する。

    // Pre-Assert

    // Act
    cplat_internal_service_dispatch_event(
        &def,
        &info); // [手順] - session_id が nullptr のイベントを指定して cplat_internal_service_dispatch_event() を呼び出す。

    // Assert
    ASSERT_EQ(1U, g_events.size()); // [確認_正常系] - g_events.size() の戻り値が 1U であること。
    EXPECT_EQ(CPLAT_SERVICE_EVENT_PRESHUTDOWN,
              g_events[0].type); // [確認_正常系] - g_events[0].type が CPLAT_SERVICE_EVENT_PRESHUTDOWN であること。
    EXPECT_EQ(nullptr, g_events[0].session_id); // [確認_正常系] - g_events[0].session_id が nullptr であること。
}

// NULL や on_reload 未設定では再読込を配送しないことの確認
TEST_F(serviceCommonTest, dispatch_reload_null_safety)
{
    // Arrange
    cplat_service_definition def_without_reload = make_def();
    def_without_reload.on_reload = nullptr; // [状態] - on_reload が nullptr のサービス定義を用意する。

    // Pre-Assert

    // Act
    cplat_internal_service_dispatch_reload(
        nullptr); // [手順] - def に nullptr を指定して cplat_internal_service_dispatch_reload() を呼び出す。

    // Assert
    EXPECT_TRUE(
        g_calls.empty()); // [確認_異常系] - def が nullptr の場合の g_calls.empty() の戻り値が true であること。

    // Act_2
    cplat_internal_service_dispatch_reload(
        &def_without_reload); // [手順] - on_reload が nullptr の定義を指定して cplat_internal_service_dispatch_reload() を呼び出す。

    // Assert_2
    EXPECT_TRUE(
        g_calls.empty()); // [確認_異常系] - on_reload が nullptr の場合の g_calls.empty() の戻り値が true であること。
}

// イベント・再読込・状態テキスト配送の確認
TEST_F(serviceCommonTest, dispatch_event_reload_and_status_text)
{
    // Arrange
    cplat_service_definition def = make_def();
    cplat_service_event_info info = {CPLAT_SERVICE_EVENT_SESSION_LOGON, 0U, "42"};

    // Pre-Assert

    // Act
    cplat_service_set_status_text(nullptr); // [手順] - nullptr を指定して cplat_service_set_status_text() を呼び出す。
    cplat_service_set_status_text(
        "running cycle 1"); // [手順] - "running cycle 1" を指定して cplat_service_set_status_text() を呼び出す。
    cplat_internal_service_dispatch_event(
        &def, &info); // [手順] - OS イベントを指定して cplat_internal_service_dispatch_event() を呼び出す。
    cplat_internal_service_dispatch_reload(&def); // [手順] - cplat_internal_service_dispatch_reload() を呼び出す。

    // Assert
    ASSERT_EQ(
        1U,
        g_status_texts
            .size()); // [確認_正常系] - g_status_texts.size() の戻り値が 1U であること。(非 nullptr の 1 回のみ通知されることの確認)
    EXPECT_EQ("running cycle 1",
              g_status_texts[0]);   // [確認_正常系] - g_status_texts[0] が "running cycle 1" であること。
    ASSERT_EQ(1U, g_events.size()); // [確認_正常系] - g_events.size() の戻り値が 1U であること。
    EXPECT_EQ(CPLAT_SERVICE_EVENT_SESSION_LOGON,
              g_events[0].type); // [確認_正常系] - g_events[0].type が CPLAT_SERVICE_EVENT_SESSION_LOGON であること。
    std::vector<std::string> expected = {"notify_status", "on_event", "notify_reloading", "on_reload", "notify_ready"};
    EXPECT_EQ(
        expected,
        g_calls); // [確認_正常系] - g_calls が expected と一致すること。(reloading -> on_reload -> ready の順であることの確認)
}
