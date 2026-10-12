#include <testfw.h>
#include <mock_cplat.h>
#include <cplat/runtime/assertion.h>

#include <cstdio>

namespace
{

int g_evaluation_count = 0;

int count_evaluation(int value)
{
    g_evaluation_count++;
    return value;
}

void print_id_callback(const cplat_assert_event *, void *context)
{
    fprintf(stderr, "[callback %d]", *static_cast<int *>(context));
}

void print_event_callback(const cplat_assert_event *event, void *)
{
    fprintf(stderr, "[event expression=%s line=%d function=%s]", event->expression, event->line, event->function);
}

void recursive_assert_callback(const cplat_assert_event *, void *)
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

    (void)cplat_assert_register(print_id_callback, &first);
    (void)cplat_assert_register(print_id_callback, &second);
    CPLAT_ASSERT(count_evaluation(0) == 1);
}

void fail_with_event_callback()
{
    (void)cplat_assert_register(print_event_callback, NULL);
    cplat_assert_fail_at("state == ready", "sample.c", 42, "sample_function");
}

void fail_with_null_location()
{
    (void)cplat_assert_register(print_event_callback, NULL);
    cplat_assert_fail_at(NULL, NULL, 0, NULL);
}

void fail_inside_callback()
{
    (void)cplat_assert_register(recursive_assert_callback, NULL);
    CPLAT_ASSERT(count_evaluation(0) == 1);
}

} // namespace

class assertionTest : public Test
{
  protected:
    // [サブ手順 名前=assertionTest.SetUp]
    void SetUp() override
    {
        g_evaluation_count = 0; // [状態] - 条件式の評価回数を 0 にする。
    }
    // [サブ手順終了]
};

// 条件が真の場合は終了せず、条件式を 1 回だけ評価することの確認
// [サブ手順参照 名前=assertionTest.SetUp]
TEST_F(assertionTest, test_true_condition_continues_and_evaluates_once)
{
    // Arrange

    // Pre-Assert

    // Act
    CPLAT_ASSERT(count_evaluation(1) == 1); // [手順] - 真となる条件式で CPLAT_ASSERT を実行する。

    // Assert
    EXPECT_EQ(1, g_evaluation_count); // [確認_正常系] - 条件式が 1 回だけ評価されること。
}

// 条件が偽の場合に失敗位置を出力して異常終了することの確認
TEST_F(assertionTest, test_false_condition_aborts_with_location)
{
    // Arrange

    // Pre-Assert

    // Act
    EXPECT_DEATH(fail_assertion(),
                 "CPLAT_ASSERT failed: value == 1, file .*assertionTest\\.cc, line .*, function "
                 "fail_assertion"); // [手順] - 偽となる条件式で CPLAT_ASSERT を実行する。
    // [確認_異常系] - 条件式、ファイル名、行番号、関数名を標準エラー出力へ書き出して異常終了すること。

    // Assert
}

// 登録したコールバックが LIFO 順で呼び出されてから異常終了することの確認
TEST_F(assertionTest, test_callbacks_are_invoked_in_lifo_order)
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
TEST_F(assertionTest, test_callback_receives_event)
{
    // Arrange

    // Pre-Assert

    // Act
    EXPECT_DEATH(
        fail_with_event_callback(),
        "\\[event expression=state == ready line=42 function=sample_function\\]"); // [手順] - 失敗位置を指定して異常終了の処理を実行する。
    // [確認_異常系] - 指定した条件式、行番号、関数名がコールバックへ渡されること。

    // Assert
}

// 失敗位置の NULL を空文字列として扱うことの確認
TEST_F(assertionTest, test_null_location_is_treated_as_empty)
{
    // Arrange

    // Pre-Assert

    // Act
    EXPECT_DEATH(
        fail_with_null_location(),
        "\\[event expression= line=0 function=\\]"); // [手順] - 失敗位置をすべて NULL として異常終了の処理を実行する。
    // [確認_異常系] - NULL の失敗位置が空文字列としてコールバックへ渡されること。

    // Assert
}

// コールバック内で条件が偽になった場合も異常終了することの確認
TEST_F(assertionTest, test_failure_inside_callback_aborts)
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
TEST_F(assertionTest, test_register_rejects_invalid_inputs)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_malloc(_))
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_malloc が 1 回呼び出されること。
                                    // [Pre-Assert手順] - cplat_malloc から NULL を返却する。

    // Act
    int null_result = cplat_assert_register(NULL, NULL); // [手順] - NULL コールバックを登録する。
    int oom_result =
        cplat_assert_register(print_id_callback, NULL); // [手順] - メモリ確保の失敗時にコールバックを登録する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              null_result); // [確認_異常系] - cplat_assert_register が NULL コールバックを拒否すること。
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY,
              oom_result); // [確認_異常系] - cplat_assert_register がメモリ不足を通知すること。
}
