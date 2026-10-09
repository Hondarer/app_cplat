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
    cplat_prompt *prompt_ = NULL;

    // [サブ手順 名前=promptAllocFailureTest.SetUp]
    void SetUp() override
    {
        promptFakeReset();
        prompt_ = cplat_prompt_create(NULL);
        ASSERT_NE((cplat_prompt *)NULL, prompt_);
        // [状態確認] - `(cplat_prompt *)NULL` と `prompt_` が異なること。
        /* 端末に接続していない実行環境でも対話パスを通すため、TTY 状態を直接立てる */
        prompt_->is_tty = 1;
    }
    // [サブ手順終了]

    // [サブ手順 名前=promptAllocFailureTest.TearDown]
    void TearDown() override
    {
        cplat_prompt_dispose(prompt_);
        prompt_ = NULL;
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
            Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_calloc が要素数 1 を指定してハンドル確保のために 1 回呼び出されること。
                              // [Pre-Assert手順] - cplat_calloc から NULL を返却する。

    // Act
    cplat_prompt *handle = cplat_prompt_create(NULL); // [手順] - cplat_prompt_create を呼び出す。

    // Assert
    EXPECT_EQ((cplat_prompt *)NULL,
              handle); // [確認_異常系] - cplat_prompt_create の戻り値が NULL であること。
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
        .WillOnce(
            Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_malloc が編集バッファー確保のために 1 回呼び出されること。
                              // [Pre-Assert手順] - cplat_malloc から NULL を返却する。

    // Act
    cplat_prompt *handle = cplat_prompt_create(NULL); // [手順] - cplat_prompt_create を呼び出す。

    // Assert
    EXPECT_EQ((cplat_prompt *)NULL,
              handle); // [確認_異常系] - cplat_prompt_create の戻り値が NULL であること。
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
        .WillOnce(
            Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_realloc がコンテキスト配列の拡張のために 1 回呼び出されること。
                              // [Pre-Assert手順] - cplat_realloc から NULL を返却する。

    // Act
    int actual_ret = cplat_prompt_readline_at(prompt_, buf, sizeof(buf), ">> ", "promptAllocFailureTest.cc",
                                          1); // [手順] - 1 行読み取る。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_EOF,
        actual_ret); // [確認_異常系] - コンテキストを取得できず cplat_fgets へフォールバックし、標準入力が EOF のため CPLAT_ERR_EOF が返ること。
    EXPECT_EQ(0, promptFakeEnterRawCount()); // [確認_異常系] - raw モードへ移行しないこと。
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
        .WillOnce(
            Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_malloc が退避バッファー確保のために 1 回呼び出されること。
                              // [Pre-Assert手順] - cplat_malloc から NULL を返却する。

    // Act
    int actual_ret = cplat_prompt_readline_at(prompt_, buf, sizeof(buf), ">> ", "promptAllocFailureTest.cc",
                                          2); // [手順] - 1 行読み取る。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_EOF,
        actual_ret); // [確認_異常系] - コンテキストを取得できず cplat_fgets へフォールバックし、標準入力が EOF のため CPLAT_ERR_EOF が返ること。
    EXPECT_EQ(0, promptFakeEnterRawCount()); // [確認_異常系] - raw モードへ移行しないこと。
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
        .WillOnce(
            Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_malloc が 2 回呼び出されること。
                              // [Pre-Assert手順] - 1 回目は本物へ委譲し、2 回目は NULL を返却する。

    // Act
    int actual_ret = cplat_prompt_readline_at(prompt_, buf, sizeof(buf), ">> ", "promptAllocFailureTest.cc",
                                          3); // [手順] - "abc" と Enter を入力して 1 行読み取る。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 履歴へ残せなくても cplat_prompt_readline_at は CPLAT_OK を返すこと。
    EXPECT_STREQ("abc", buf);    // [確認_正常系] - 入力した "abc" が返ること。
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
    /* 書式バッファー以外の確保 (コンテキストの退避バッファーなど) は本物へ委譲する。
       gMock は後から宣言した期待値を優先するため、汎用の期待値を先に宣言する */
    EXPECT_CALL(mock_cplat, cplat_malloc(_))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_正常系] - 書式バッファー以外の cplat_malloc が任意の回数呼び出されること。
                                      // [Pre-Assert手順] - 本物の cplat_malloc へ委譲する。
    EXPECT_CALL(mock_cplat, cplat_malloc(256u))
        .WillOnce(
            Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_malloc が書式バッファーの初期容量 256 を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - cplat_malloc から NULL を返却する。

    // Act
    int actual_ret = cplat_prompt_readline_fmt_at(prompt_, buf, sizeof(buf), "promptAllocFailureTest.cc", 4, "[%d] ",
                                              7); // [手順] - 書式付きプロンプトで 1 行読み取る。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 空のプロンプトで継続し CPLAT_OK が返ること。
    EXPECT_STREQ("abc", buf);    // [確認_正常系] - 入力した "abc" が返ること。
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
    ASSERT_EQ(CPLAT_OK, cplat_prompt_readline_fmt_at(prompt_, buf, sizeof(buf), "promptAllocFailureTest.cc", 5,
                                                           "%s", "short")); // [状態] - 同じ呼び出し位置でコンテキストと書式バッファーを確保する。
                                                                            // [状態確認] - cplat_prompt_readline_fmt_at の戻り値が CPLAT_OK であること。

    NiceMock<Mock_cplat> mock_cplat;

    promptFakeSetInput("abc\r");

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_realloc(_, _, _))
        .WillOnce(
            Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_realloc が書式バッファーの拡張のために 1 回呼び出されること。
                              // [Pre-Assert手順] - cplat_realloc から NULL を返却する。

    // Act
    int actual_ret = cplat_prompt_readline_fmt_at(prompt_, buf, sizeof(buf), "promptAllocFailureTest.cc", 5, "%s",
                                              long_prompt.c_str()); // [手順] - 長いプロンプトで 1 行読み取る。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - プロンプトを切り捨てて継続し CPLAT_OK が返ること。
    EXPECT_STREQ("abc", buf);    // [確認_正常系] - 入力した "abc" が返ること。
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
                                                      // [状態確認] - cplat_prompt_readline_at の戻り値が CPLAT_OK であること。

    NiceMock<Mock_cplat> mock_cplat;
    int leave_raw_count_before = promptFakeLeaveRawCount();

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_realloc(_, _, _))
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_realloc が編集バッファーの拡張のために 1 回呼び出されること。
                                    // [Pre-Assert手順] - cplat_realloc から NULL を返却する。

    // Act
    int actual_ret = cplat_prompt_readline_with_initial_at(prompt_, buf, sizeof(buf), ">> ", long_initial.c_str(),
                                                           "promptAllocFailureTest.cc", 6); // [手順] - 長い初期値で 1 行読み取る。

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY, actual_ret); // [確認_異常系] - CPLAT_ERR_OUT_OF_MEMORY が返ること。
    EXPECT_STREQ("", buf);                          // [確認_異常系] - 出力先が空文字列であること。
    EXPECT_EQ(leave_raw_count_before + 1, promptFakeLeaveRawCount()); // [確認_異常系] - raw モードを解除して戻ること。
}
// [サブ手順参照 名前=promptAllocFailureTest.TearDown]
