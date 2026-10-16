#include <testfw.h>
#include <mock_cplat.h>
#include <cplat/console/console.h>
#include <cplat/console/console_internal.h>
#include <stdio.h>

/* ===== テスト クラス ===== */

class consoleTest : public Test
{
  protected:
    NiceMock<Mock_cplat> mock_cplat;

    // [サブ手順 名前=consoleTest.SetUp]
    void SetUp() override
    {
        // console.c の単体テストでは shutdown.c を対象外とし、登録 API の呼び出しをフェイクする。
        ON_CALL(mock_cplat, cplat_shutdown_register(_, _)).WillByDefault(Return(CPLAT_OK));
        // [状態] - cplat_shutdown_register() の既定動作を設定する。
    }
    // [サブ手順終了]
};

/* ===== 共通テスト (Windows / Linux 両方) ===== */

// cplat_console_init がクラッシュしないことの確認
// [サブ手順参照 名前=consoleTest.SetUp]
TEST_F(consoleTest, init_succeeds)
{
    // Arrange

    // Pre-Assert

    // Act
    cplat_console_init(); // [手順] - コンソール ヘルパーを初期化する。

    // Assert
    SUCCEED(); // [確認_正常系] - SUCCEED() に到達すること。(cplat_console_init() がクラッシュしないことの確認)
}

// init 後に dispose_on_shutdown() がクラッシュしないことの確認
// [サブ手順参照 名前=consoleTest.SetUp]
TEST_F(consoleTest, dispose_on_shutdown_after_init)
{
    // Arrange
    cplat_console_init(); // [状態] - 初期化済みのコンソール ヘルパーとする。
    cplat_shutdown_event event = {CPLAT_SHUTDOWN_REASON_NORMAL_EXIT, CPLAT_SHUTDOWN_CODE_KIND_NONE,
                                  0}; // [状態] - 通常終了イベントを用意する。

    // Pre-Assert

    // Act
    cplat_internal_console_dispose_on_shutdown(
        &event,
        nullptr); // [手順] - 正常終了イベントで dispose_on_shutdown() を呼び出す。

    // Assert
    SUCCEED(); // [確認_正常系] - SUCCEED() に到達すること。(初期化後の cplat_internal_console_dispose_on_shutdown() がクラッシュしないことの確認)
}

// init なしで dispose_on_shutdown() を呼んでも安全なことの確認
// [サブ手順参照 名前=consoleTest.SetUp]
TEST_F(consoleTest, dispose_on_shutdown_without_init)
{
    // Arrange
    cplat_shutdown_event event = {CPLAT_SHUTDOWN_REASON_NORMAL_EXIT, CPLAT_SHUTDOWN_CODE_KIND_NONE,
                                  0}; // [状態] - 通常終了イベントを用意する (init は呼ばない)。

    // Pre-Assert

    // Act
    cplat_internal_console_dispose_on_shutdown(&event,
                                               nullptr); // [手順] - init を呼ばずに dispose_on_shutdown() を呼び出す。

    // Assert
    SUCCEED(); // [確認_正常系] - SUCCEED() に到達すること。(初期化せずに呼び出した cplat_internal_console_dispose_on_shutdown() がクラッシュしないことの確認)
}

// dispose_on_shutdown() を 2 回呼んでも安全なことの確認
// [サブ手順参照 名前=consoleTest.SetUp]
TEST_F(consoleTest, double_dispose_on_shutdown)
{
    // Arrange
    cplat_console_init(); // [状態] - 初期化済みのコンソール ヘルパーとする。
    cplat_shutdown_event event = {CPLAT_SHUTDOWN_REASON_NORMAL_EXIT, CPLAT_SHUTDOWN_CODE_KIND_NONE,
                                  0}; // [状態] - 通常終了イベントを用意する。

    // Pre-Assert

    // Act
    cplat_internal_console_dispose_on_shutdown(&event, nullptr); // [手順] - 1 回目の dispose_on_shutdown() を呼び出す。
    cplat_internal_console_dispose_on_shutdown(&event,
                                               nullptr); // [手順] - 続けて 2 回目の dispose_on_shutdown() を呼び出す。

    // Assert
    SUCCEED(); // [確認_正常系] - SUCCEED() に到達すること。(2 回目の cplat_internal_console_dispose_on_shutdown() がクラッシュしないことの確認)
}

// init 後に終了中イベントの dispose_on_shutdown() が安全に何もしないことの確認
// [サブ手順参照 名前=consoleTest.SetUp]
TEST_F(consoleTest, dispose_on_shutdown_process_terminating)
{
    // Arrange
    cplat_console_init(); // [状態] - 初期化済みのコンソール ヘルパーとする。
    cplat_shutdown_event terminating_event = {CPLAT_SHUTDOWN_REASON_PROCESS_TERMINATING, CPLAT_SHUTDOWN_CODE_KIND_NONE,
                                              0}; // [状態] - プロセス終了中イベントを用意する。
    cplat_shutdown_event normal_event = {CPLAT_SHUTDOWN_REASON_NORMAL_EXIT, CPLAT_SHUTDOWN_CODE_KIND_NONE, 0};

    // Pre-Assert

    // Act
    cplat_internal_console_dispose_on_shutdown(&terminating_event,
                                               nullptr); // [手順] - 終了中イベントで dispose_on_shutdown() を呼び出す。

    // Assert
    SUCCEED(); // [確認_正常系] - SUCCEED() に到達すること。(終了中イベントを渡した cplat_internal_console_dispose_on_shutdown() がクラッシュしないことの確認)

    // Cleanup
    // init 状態を通常終了イベントで解放する。
    cplat_internal_console_dispose_on_shutdown(&normal_event, nullptr);
}

// init 後に printf / fprintf を呼んでもクラッシュしないことの確認
// [サブ手順参照 名前=consoleTest.SetUp]
TEST_F(consoleTest, write_after_init)
{
    // Arrange
    cplat_console_init(); // [状態] - 初期化済みのコンソール ヘルパーとする。

    // Pre-Assert

    // Act
    printf("consoleTest: stdout\n");          // [手順] - stdout に書き込む。
    fprintf(stderr, "consoleTest: stderr\n"); // [手順] - stderr に書き込む。

    // Assert
    SUCCEED(); // [確認_正常系] - SUCCEED() に到達すること。(初期化後の printf() と fprintf() がクラッシュしないことの確認)
}

/* ===== Linux NOP テスト ===== */
/*
 * Linux では cplat_console_init / cplat_console_dispose は no-op である。
 * 次のテストは、no-op 実装が stdout / stderr に一切影響を与えないことを確認する。
 */

#if defined(PLATFORM_LINUX)

// Linux: init 前後で stdout の FD が変わらないことの確認
// [サブ手順参照 名前=consoleTest.SetUp]
TEST_F(consoleTest, nop_stdout_fd_unchanged)
{
    // Arrange
    int fd_before = fileno(stdout); // [状態] - init 前の stdout FD を記録する。

    // Pre-Assert

    // Act
    cplat_console_init(); // [手順] - コンソール ヘルパーを初期化する。
    int fd_after_init = fileno(stdout);
    cplat_console_dispose(); // [手順] - no-op の dispose を呼び出す。
    int fd_after_dispose = fileno(stdout);

    // Assert
    EXPECT_EQ(
        fd_before,
        fd_after_init); // [確認_正常系] - 初期化後の stdout に対する fileno() の戻り値が初期化前の fileno() の戻り値と一致すること。
    EXPECT_EQ(
        fd_before,
        fd_after_dispose); // [確認_正常系] - 解放後の stdout に対する fileno() の戻り値が初期化前の fileno() の戻り値と一致すること。
}

// Linux: init 前後で stderr の FD が変わらないことの確認
// [サブ手順参照 名前=consoleTest.SetUp]
TEST_F(consoleTest, nop_stderr_fd_unchanged)
{
    // Arrange
    int fd_before = fileno(stderr); // [状態] - init 前の stderr FD を記録する。

    // Pre-Assert

    // Act
    cplat_console_init(); // [手順] - コンソール ヘルパーを初期化する。
    int fd_after_init = fileno(stderr);
    cplat_console_dispose(); // [手順] - no-op の dispose を呼び出す。
    int fd_after_dispose = fileno(stderr);

    // Assert
    EXPECT_EQ(
        fd_before,
        fd_after_init); // [確認_正常系] - 初期化後の stderr に対する fileno() の戻り値が初期化前の fileno() の戻り値と一致すること。
    EXPECT_EQ(
        fd_before,
        fd_after_dispose); // [確認_正常系] - 解放後の stderr に対する fileno() の戻り値が初期化前の fileno() の戻り値と一致すること。
}

// Linux: dispose を呼んでも stdout の FD が変わらないことの確認
// [サブ手順参照 名前=consoleTest.SetUp]
TEST_F(consoleTest, nop_dispose_stdout_fd_unchanged)
{
    // Arrange
    int fd_before = fileno(stdout); // [状態] - init 前の stdout FD を記録する。
    cplat_console_init();           // [状態] - 初期化済みのコンソール ヘルパーとする。

    // Pre-Assert

    // Act
    cplat_console_dispose(); // [手順] - no-op の dispose を呼び出す。

    // Assert
    EXPECT_EQ(
        fd_before,
        fileno(
            stdout)); // [確認_正常系] - 解放後の stdout に対する fileno() の戻り値が初期化前の fileno() の戻り値と一致すること。
}

#endif /* PLATFORM_LINUX */
