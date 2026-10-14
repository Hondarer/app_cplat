#include <testfw.h>
#include <mock_cplat.h>
#include <cplat/runtime/abort.h>

#include <cstdio>

namespace
{

int g_evaluation_count = 0;

int count_evaluation(int value)
{
    g_evaluation_count++;
    return value;
}

void print_id_callback(const cplat_abort_event *, void *context)
{
    fprintf(stderr, "[callback %d]", *static_cast<int *>(context));
}

void print_event_callback(const cplat_abort_event *event, void *)
{
    fprintf(stderr, "[event message=%s line=%d function=%s kind=%d]", event->message, event->line,
            event->function, static_cast<int>(event->kind));
}

void recursive_assert_callback(const cplat_abort_event *, void *)
{
    CPLAT_ASSERT(count_evaluation(0) == 1);
}

void fail_assertion()
{
    int value = 0;

    CPLAT_ASSERT(value == 1);
}

void fail_with_callbacks_in_lifo_order()
{
    static int first = 1;
    static int second = 2;

    (void)cplat_abort_register(print_id_callback, &first);
    (void)cplat_abort_register(print_id_callback, &second);
    CPLAT_ASSERT(count_evaluation(0) == 1);
}

void fail_with_event_callback()
{
    (void)cplat_abort_register(print_event_callback, NULL);
    cplat_abort_at(CPLAT_ABORT_KIND_ASSERTION_FAILED, "state == ready", "sample.c", 42, "sample_function");
}

void fail_with_null_location()
{
    (void)cplat_abort_register(print_event_callback, NULL);
    cplat_abort_at(CPLAT_ABORT_KIND_ASSERTION_FAILED, NULL, NULL, 0, NULL);
}

void fail_inside_callback()
{
    (void)cplat_abort_register(recursive_assert_callback, NULL);
    CPLAT_ASSERT(count_evaluation(0) == 1);
}

void request_abort()
{
    CPLAT_ABORT("internal synchronization failed");
}

void request_abort_with_event_callback()
{
    (void)cplat_abort_register(print_event_callback, NULL);
    CPLAT_ABORT("internal synchronization failed");
}

void fail_assertion_with_event_callback()
{
    (void)cplat_abort_register(print_event_callback, NULL);
    CPLAT_ASSERT(count_evaluation(0) == 1);
}

void abort_with_unknown_kind()
{
    /* 列挙子 1 と 2 の値域 (0 から 3) に収まり、列挙子を持たない値 */
    cplat_abort_at(static_cast<cplat_abort_kind>(3), "unknown kind", "sample.c", 42, "sample_function");
}

} // namespace

class abortTest : public Test
{
  protected:
    // [サブ手順 名前=abortTest.SetUp]
    void SetUp() override
    {
        g_evaluation_count = 0; // [状態] - 条件式の評価回数を 0 にする。
    }
    // [サブ手順終了]
};

// 条件が真の場合は終了せず、条件式を 1 回だけ評価することの確認
// [サブ手順参照 名前=abortTest.SetUp]
TEST_F(abortTest, true_condition_continues_and_evaluates_once)
{
    // Arrange

    // Pre-Assert

    // Act
    CPLAT_ASSERT(count_evaluation(1) == 1); // [手順] - 真となる条件式で CPLAT_ASSERT を実行する。

    // Assert
    EXPECT_EQ(1, g_evaluation_count); // [確認_正常系] - 条件式が 1 回だけ評価されること。
}

// 条件が偽の場合に失敗位置を出力して異常終了することの確認
TEST_F(abortTest, false_condition_aborts_with_location)
{
    // Arrange

    // Pre-Assert

    // Act
    EXPECT_DEATH(fail_assertion(),
                 "CPLAT_ASSERT failed: value == 1, file .*abortTest\\.cc, line .*, function "
                 "fail_assertion"); // [手順] - 偽となる条件式で CPLAT_ASSERT を実行する。
    // [確認_異常系] - 条件式、ファイル名、行番号、関数名を標準エラー出力へ書き出して異常終了すること。

    // Assert
}

// 登録したコールバックが LIFO 順で呼び出されてから異常終了することの確認
TEST_F(abortTest, callbacks_are_invoked_in_lifo_order)
{
    // Arrange

    // Pre-Assert

    // Act
    EXPECT_DEATH(fail_with_callbacks_in_lifo_order(),
                 "\\[callback 2\\]\\[callback 1\\]"); // [手順] - コールバックを 2 件登録して条件を偽にする。
    // [確認_異常系] - 後に登録したコールバックから順に呼び出されて異常終了すること。

    // Assert
}

// コールバックへ失敗位置が渡されることの確認
TEST_F(abortTest, callback_receives_event)
{
    // Arrange

    // Pre-Assert

    // Act
    EXPECT_DEATH(
        fail_with_event_callback(),
        "\\[event message=state == ready line=42 function=sample_function kind=1\\]"); // [手順] - 失敗位置を指定して異常終了の処理を実行する。
    // [確認_異常系] - fail_with_event_callback() がプロセスを終了させ、出力が "\\[event message=state == ready line=42 function=sample_function kind=1\\]" に一致すること。(kind が CPLAT_ABORT_KIND_ASSERTION_FAILED であることの確認)

    // Assert
}

// 失敗位置の NULL を空文字列として扱うことの確認
TEST_F(abortTest, null_location_is_treated_as_empty)
{
    // Arrange

    // Pre-Assert

    // Act
    EXPECT_DEATH(
        fail_with_null_location(),
        "\\[event message= line=0 function= kind=1\\]"); // [手順] - 失敗位置をすべて NULL として異常終了の処理を実行する。
    // [確認_異常系] - fail_with_null_location() がプロセスを終了させ、出力が "\\[event message= line=0 function= kind=1\\]" に一致すること。

    // Assert
}

// コールバック内で条件が偽になった場合も異常終了することの確認
TEST_F(abortTest, failure_inside_callback_aborts)
{
    // Arrange

    // Pre-Assert

    // Act
    EXPECT_DEATH(fail_inside_callback(),
                 "function recursive_assert_callback"); // [手順] - コールバック内で偽となる条件式を実行する。
    // [確認_異常系] - コールバック内の失敗位置を書き出し、待機せずに異常終了すること。

    // Assert
}

// 登録時の不正引数とメモリ確保失敗を通知することの確認
TEST_F(abortTest, register_rejects_invalid_inputs)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_malloc(_))
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_malloc が 1 回呼び出されること。
                                    // [Pre-Assert手順] - cplat_malloc から NULL を返却する。

    // Act
    int null_result = cplat_abort_register(NULL, NULL); // [手順] - NULL コールバックを登録する。
    int oom_result =
        cplat_abort_register(print_id_callback, NULL); // [手順] - メモリ確保の失敗時にコールバックを登録する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              null_result); // [確認_異常系] - cplat_abort_register が NULL コールバックを拒否すること。
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY,
              oom_result); // [確認_異常系] - cplat_abort_register がメモリ不足を通知すること。
}

// 無条件の異常終了で理由と発生位置を出力することの確認
TEST_F(abortTest, requested_abort_outputs_reason_and_location)
{
    // Arrange

    // Pre-Assert

    // Act
    EXPECT_DEATH(request_abort(),
                 "CPLAT_ABORT: internal synchronization failed, file .*abortTest\\.cc, line .*, function request_abort"); // [手順] - 理由を指定して CPLAT_ABORT を実行する。
    // [確認_異常系] - request_abort() がプロセスを終了させ、出力が "CPLAT_ABORT: internal synchronization failed, file .*abortTest\\.cc, line .*, function request_abort" に一致すること。

    // Assert
}

// 無条件の異常終了でコールバックへ理由と REQUESTED の種別が渡されることの確認
TEST_F(abortTest, requested_abort_callback_receives_reason_and_kind)
{
    // Arrange

    // Pre-Assert

    // Act
    EXPECT_DEATH(request_abort_with_event_callback(),
                 "\\[event message=internal synchronization failed line=.* function=request_abort_with_event_callback kind=2\\]"); // [手順] - コールバックを登録して CPLAT_ABORT を実行する。
    // [確認_異常系] - request_abort_with_event_callback() がプロセスを終了させ、出力が "\\[event message=internal synchronization failed line=.* function=request_abort_with_event_callback kind=2\\]" に一致すること。(kind が CPLAT_ABORT_KIND_REQUESTED で、message が指定した理由であることの確認)

    // Assert
}

// 条件が偽の場合にコールバックへ ASSERTION_FAILED の種別が渡されることの確認
TEST_F(abortTest, assertion_callback_receives_failed_kind)
{
    // Arrange

    // Pre-Assert

    // Act
    EXPECT_DEATH(fail_assertion_with_event_callback(),
                 "\\[event message=count_evaluation\\(0\\) == 1 line=.* function=fail_assertion_with_event_callback kind=1\\]"); // [手順] - コールバックを登録して CPLAT_ASSERT の条件を偽にする。
    // [確認_異常系] - fail_assertion_with_event_callback() がプロセスを終了させ、出力が "\\[event message=count_evaluation\\(0\\) == 1 line=.* function=fail_assertion_with_event_callback kind=1\\]" に一致すること。(kind が CPLAT_ABORT_KIND_ASSERTION_FAILED であることの確認)

    // Assert
}

// 定義外の種別でも汎用の接頭辞で出力して異常終了することの確認
TEST_F(abortTest, unknown_kind_outputs_generic_prefix_and_aborts)
{
    // Arrange

    // Pre-Assert

    // Act
    EXPECT_DEATH(abort_with_unknown_kind(),
                 "cplat abort: unknown kind, file sample\\.c, line 42, function sample_function"); // [手順] - 定義外の種別 3 を指定して cplat_abort_at() を実行する。
    // [確認_異常系] - abort_with_unknown_kind() がプロセスを終了させ、出力が "cplat abort: unknown kind, file sample\\.c, line 42, function sample_function" に一致すること。

    // Assert
}
