#include <testfw.h>
#include <mock_cplat.h>
#include <cplat/base/result.h>
#include <cplat/prompt/prompt.h>
#include <cplat/prompt/prompt_internal.h>

#include <cstring>
#include <string>

#include "promptPlatformFake.h"

class promptAllocFailureTest : public Test
{
  protected:
    cplat_prompt *prompt_ = nullptr;

    // [サブ手順 名前=promptAllocFailureTest.SetUp]
    void SetUp() override
    {
        promptFakeReset();
        prompt_ = cplat_prompt_create(nullptr);
        ASSERT_NE(nullptr, prompt_);
        // [状態確認] - cplat_prompt_create() の戻り値が nullptr 以外であること。
        /* 端末に接続していない実行環境でも対話パスを通すため、TTY 状態を直接立てる */
        prompt_->is_tty = 1;
    }
    // [サブ手順終了]

    // [サブ手順 名前=promptAllocFailureTest.TearDown]
    void TearDown() override
    {
        cplat_prompt_dispose(prompt_);
        prompt_ = nullptr;
    }
    // [サブ手順終了]
};

// ハンドルの確保に失敗した場合に生成が失敗することの確認
// [サブ手順参照 名前=promptAllocFailureTest.SetUp]
TEST_F(promptAllocFailureTest, create_returns_null_when_handle_allocation_fails)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_calloc(1u, _))
        .WillOnce(
            Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_calloc() が要素数 1 を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - cplat_calloc() から nullptr を返す。

    // Act
    cplat_prompt *handle = cplat_prompt_create(nullptr); // [手順] - cplat_prompt_create() を呼び出す。

    // Assert
    EXPECT_EQ(nullptr,
              handle); // [確認_異常系] - cplat_prompt_create() の戻り値が nullptr であること。
}
// [サブ手順参照 名前=promptAllocFailureTest.TearDown]

// 編集バッファーの確保に失敗した場合に生成が失敗することの確認
// [サブ手順参照 名前=promptAllocFailureTest.SetUp]
TEST_F(promptAllocFailureTest, create_returns_null_when_edit_buffer_allocation_fails)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_malloc(_))
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_malloc() が 1 回呼び出されること。
                                    // [Pre-Assert手順] - cplat_malloc() から nullptr を返す。

    // Act
    cplat_prompt *handle = cplat_prompt_create(nullptr); // [手順] - cplat_prompt_create() を呼び出す。

    // Assert
    EXPECT_EQ(nullptr,
              handle); // [確認_異常系] - cplat_prompt_create() の戻り値が nullptr であること。
}
// [サブ手順参照 名前=promptAllocFailureTest.TearDown]

// コンテキスト配列の拡張に失敗した場合に cplat_fgets へフォールバックすることの確認
// [サブ手順参照 名前=promptAllocFailureTest.SetUp]
TEST_F(promptAllocFailureTest, readline_falls_back_when_context_expansion_fails)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    char buf[32]; // [状態] - 出力バッファーを用意する。

    promptFakeSetInput("abc\r");

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_realloc(_, _, _))
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_realloc() が 1 回呼び出されること。
                                    // [Pre-Assert手順] - cplat_realloc() から nullptr を返す。

    // Act
    int actual_ret = cplat_prompt_readline_at(prompt_, buf, sizeof(buf), ">> ", "promptAllocFailureTest.cc",
                                              1); // [手順] - cplat_prompt_readline_at() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_EOF,
        actual_ret); // [確認_異常系] - cplat_prompt_readline_at() の戻り値が CPLAT_ERR_EOF であること。(コンテキストを取得できず cplat_fgets へフォールバックし、標準入力が EOF であることの確認)
    EXPECT_EQ(
        0,
        promptFakeEnterRawCount()); // [確認_異常系] - promptFakeEnterRawCount() の戻り値が 0 であること。(raw モードへ移行しないことの確認)
}
// [サブ手順参照 名前=promptAllocFailureTest.TearDown]

// 履歴の退避バッファーの確保に失敗した場合に cplat_fgets へフォールバックすることの確認
// [サブ手順参照 名前=promptAllocFailureTest.SetUp]
TEST_F(promptAllocFailureTest, readline_falls_back_when_saved_line_allocation_fails)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    char buf[32]; // [状態] - 出力バッファーを用意する。

    promptFakeSetInput("abc\r");

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_malloc(_))
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_malloc() が 1 回呼び出されること。
                                    // [Pre-Assert手順] - cplat_malloc() から nullptr を返す。

    // Act
    int actual_ret = cplat_prompt_readline_at(prompt_, buf, sizeof(buf), ">> ", "promptAllocFailureTest.cc",
                                              2); // [手順] - cplat_prompt_readline_at() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_EOF,
        actual_ret); // [確認_異常系] - cplat_prompt_readline_at() の戻り値が CPLAT_ERR_EOF であること。(コンテキストを取得できず cplat_fgets へフォールバックし、標準入力が EOF であることの確認)
    EXPECT_EQ(
        0,
        promptFakeEnterRawCount()); // [確認_異常系] - promptFakeEnterRawCount() の戻り値が 0 であること。(raw モードへ移行しないことの確認)
}
// [サブ手順参照 名前=promptAllocFailureTest.TearDown]

// 履歴エントリの確保に失敗しても行の確定が成功することの確認
// [サブ手順参照 名前=promptAllocFailureTest.SetUp]
TEST_F(promptAllocFailureTest, readline_succeeds_when_history_entry_allocation_fails)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    char buf[32]; // [状態] - 出力バッファーを用意する。

    promptFakeSetInput("abc\r");
    /* 1 回目はコンテキスト生成時の退避バッファー、2 回目が履歴エントリの確保になる */

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_malloc(_))
        .WillOnce(DoDefault())
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_malloc() が 2 回呼び出されること。
    // [Pre-Assert手順] - cplat_malloc() から 1 回目は製品 (prod) の処理へ委譲し、2 回目は nullptr を返す。

    // Act
    int actual_ret =
        cplat_prompt_readline_at(prompt_, buf, sizeof(buf), ">> ", "promptAllocFailureTest.cc",
                                 3); // [手順] - "abc" と Enter を入力した状態で cplat_prompt_readline_at() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret); // [確認_正常系] - cplat_prompt_readline_at() の戻り値が CPLAT_OK であること。(履歴へ残せなくても行の確定が成功することの確認)
    EXPECT_STREQ("abc", buf); // [確認_正常系] - buf が "abc" であること。
}
// [サブ手順参照 名前=promptAllocFailureTest.TearDown]

// 書式バッファーの確保に失敗した場合に空のプロンプトで継続することの確認
// [サブ手順参照 名前=promptAllocFailureTest.SetUp]
TEST_F(promptAllocFailureTest, readline_fmt_continues_with_empty_prompt_when_allocation_fails)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    char buf[32]; // [状態] - 出力バッファーを用意する。

    promptFakeSetInput("abc\r");

    // Pre-Assert
    /* 書式バッファー以外の確保 (コンテキストの退避バッファーなど) は製品 (prod) の処理へ委譲する。
       gMock は後から宣言した期待値を優先するため、汎用の期待値を先に宣言する */
    EXPECT_CALL(mock_cplat, cplat_malloc(_))
        .Times(
            AnyNumber()); // [Pre-Assert確認_正常系] - 書式バッファー以外の cplat_malloc() が任意の回数呼び出されること。
    EXPECT_CALL(mock_cplat, cplat_malloc(256u))
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_malloc() が 256 を指定して 1 回呼び出されること。
                                    // [Pre-Assert手順] - cplat_malloc() から nullptr を返す。

    // Act
    int actual_ret = cplat_prompt_readline_fmt_at(
        prompt_, buf, sizeof(buf), "promptAllocFailureTest.cc", 4, "[%d] ",
        7); // [手順] - 書式付きプロンプトを指定して cplat_prompt_readline_fmt_at() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_prompt_readline_fmt_at() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("abc", buf);        // [確認_正常系] - buf が "abc" であること。
}
// [サブ手順参照 名前=promptAllocFailureTest.TearDown]

// 書式バッファーの再確保に失敗した場合に切り捨てて継続することの確認
// [サブ手順参照 名前=promptAllocFailureTest.SetUp]
TEST_F(promptAllocFailureTest, readline_fmt_truncates_prompt_when_reallocation_fails)
{
    // Arrange
    char buf[32];
    std::string long_prompt(512u, 'p'); // [状態] - 初期容量 256 を超える長さのプロンプト文字列を用意する。

    /* 同じ呼び出し位置で 1 度読み取り、コンテキストと書式バッファーを確保済みにする。
       これにより Act 中の realloc は書式バッファーの拡張だけになる */
    promptFakeSetInput("x\r");
    ASSERT_EQ(CPLAT_OK, cplat_prompt_readline_fmt_at(
                            prompt_, buf, sizeof(buf), "promptAllocFailureTest.cc", 5, "%s",
                            "short")); // [状態] - 同じ呼び出し位置でコンテキストと書式バッファーを確保する。
                                       // [状態確認] - cplat_prompt_readline_fmt_at() の戻り値が CPLAT_OK であること。

    NiceMock<Mock_cplat> mock_cplat;

    promptFakeSetInput("abc\r");

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_realloc(_, _, _))
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_realloc() が 1 回呼び出されること。
                                    // [Pre-Assert手順] - cplat_realloc() から nullptr を返す。

    // Act
    int actual_ret = cplat_prompt_readline_fmt_at(
        prompt_, buf, sizeof(buf), "promptAllocFailureTest.cc", 5, "%s",
        long_prompt.c_str()); // [手順] - 長いプロンプトを指定して cplat_prompt_readline_fmt_at() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_prompt_readline_fmt_at() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("abc", buf);        // [確認_正常系] - buf が "abc" であること。
}
// [サブ手順参照 名前=promptAllocFailureTest.TearDown]

// 初期値のための編集バッファーの拡張に失敗した場合に、メモリ不足を返すことの確認
// [サブ手順参照 名前=promptAllocFailureTest.SetUp]
TEST_F(promptAllocFailureTest, readline_with_initial_reports_out_of_memory_when_edit_buffer_expansion_fails)
{
    // Arrange
    char buf[32] = "stale";
    std::string long_initial(300u, 'i'); // [状態] - 編集バッファーの初期容量 256 を超える長さの初期値を用意する。

    /* 同じ呼び出し位置で 1 度読み取り、コンテキストを確保済みにする。
       これにより Act 中の realloc は初期値のための編集バッファーの拡張だけになる */
    promptFakeSetInput("x\r");
    ASSERT_EQ(CPLAT_OK, cplat_prompt_readline_at(prompt_, buf, sizeof(buf), ">> ", "promptAllocFailureTest.cc",
                                                 6)); // [状態] - 同じ呼び出し位置でコンテキストを確保する。
    // [状態確認] - cplat_prompt_readline_at() の戻り値が CPLAT_OK であること。

    NiceMock<Mock_cplat> mock_cplat;
    int leave_raw_count_before = promptFakeLeaveRawCount();

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_realloc(_, _, _))
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_realloc() が 1 回呼び出されること。
                                    // [Pre-Assert手順] - cplat_realloc() から nullptr を返す。

    // Act
    int actual_ret = cplat_prompt_readline_with_initial_at(
        prompt_, buf, sizeof(buf), ">> ", long_initial.c_str(), "promptAllocFailureTest.cc",
        6); // [手順] - 長い初期値を指定して cplat_prompt_readline_with_initial_at() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_OUT_OF_MEMORY,
        actual_ret); // [確認_異常系] - cplat_prompt_readline_with_initial_at() の戻り値が CPLAT_ERR_OUT_OF_MEMORY であること。
    EXPECT_STREQ("", buf); // [確認_異常系] - buf が "" であること。
    EXPECT_EQ(
        leave_raw_count_before + 1,
        promptFakeLeaveRawCount()); // [確認_異常系] - promptFakeLeaveRawCount() の戻り値が leave_raw_count_before + 1 と一致すること。(raw モードを解除して戻ることの確認)
}
// [サブ手順参照 名前=promptAllocFailureTest.TearDown]

namespace
{
void allocation_completion(const char *, size_t, cplat_prompt_completions *candidates, void *user_data)
{
    *static_cast<int *>(user_data) = cplat_prompt_completions_add(candidates, 0U, "start ");
}
} // namespace

// 候補を保持するメモリの確保に失敗しても入力済みの内容を維持することの確認
// [サブ手順参照 名前=promptAllocFailureTest.SetUp]
TEST_F(promptAllocFailureTest, completion_allocation_failure_keeps_input)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    char output[16] = {};
    int add_result = CPLAT_OK;
    cplat_prompt_completion_set(prompt_, allocation_completion, &add_result);
    promptFakeSetInput("s\t\r");
    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_realloc(_, 4U, sizeof(cplat_internal_prompt_ctx)))
        .WillOnce(Invoke(delegate_real_cplat_realloc));
    // [Pre-Assert確認_正常系] - 履歴コンテキストの領域を 1 回確保すること。
    // [Pre-Assert手順] - 実際のメモリ確保へ委譲する。
    EXPECT_CALL(mock_cplat, cplat_realloc(_, _, 1U)).WillOnce(Return(nullptr));
    // [Pre-Assert確認_異常系] - 候補の領域を 1 回拡張すること。
    // [Pre-Assert手順] - メモリ不足を返す。
    // Act
    int result = cplat_prompt_readline_at(prompt_, output, sizeof(output), "", "allocation", 1);
    // [手順] - 候補の確保に失敗する入力を読み取る。
    // Assert
    EXPECT_EQ(CPLAT_OK, result);                    // [確認_正常系] - 入力を継続できること。
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY, add_result); // [確認_異常系] - 候補の追加がメモリ不足を返すこと。
    EXPECT_STREQ("s", output);                      // [確認_異常系] - 入力済みの内容を維持すること。
}
// [サブ手順参照 名前=promptAllocFailureTest.TearDown]
