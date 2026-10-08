#include <testfw.h>
#include <mock_cplat.h>
#include <cplat/trace/trace_file.h>

#include <cstdio>

using testing::_;
using testing::AtLeast;
using testing::DoDefault;
using testing::NiceMock;
using testing::Return;

class traceFileAllocFailureTest : public Test
{
  protected:
    NiceMock<Mock_cplat> mock_cplat;

    // [サブ手順 名前=traceFileAllocFailureTest.SetUp]
    void SetUp() override
    {
        ON_CALL(mock_cplat, cplat_file_open(_, _, _, _)).WillByDefault(Return(CPLAT_OK));
        // [状態] - `cplat_file_open` の既定動作を設定する。
        ON_CALL(mock_cplat, cplat_file_get_size(_, _, _))
            .WillByDefault(
                [](const cplat_file *, size_t *size_out, cplat_error *)
                {
                    *size_out = 0;
                    return 0;
                });
        // [状態] - `cplat_file_get_size` の既定動作を設定する。
        ON_CALL(mock_cplat, cplat_file_close(_, _)).WillByDefault(Return(CPLAT_OK));
        // [状態] - `cplat_file_close` の既定動作を設定する。
    }
    // [サブ手順終了]
};

// レジストリ キーの確保に失敗した場合に生成が失敗することの確認
// [サブ手順参照 名前=traceFileAllocFailureTest.SetUp]
TEST_F(traceFileAllocFailureTest, create_returns_null_when_registry_key_allocation_fails)
{
    // Arrange
    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_malloc(_))
        .WillOnce(Return(nullptr))
        .WillRepeatedly(
            DoDefault()); // [Pre-Assert確認_異常系] - cplat_malloc がレジストリ キーの確保のために 1 回目に呼び出されること。
                          // [Pre-Assert手順] - 1 回目は NULL を返却し、以降は本物へ委譲する。

    // Act
    cplat_trace_file_sink *handle =
        cplat_trace_file_sink_create("traceFileAllocFailureTest_key.log", 0, 0,
                                     0); // [手順] - cplat_trace_file_sink_create を呼び出す。

    // Assert
    EXPECT_EQ((cplat_trace_file_sink *)NULL,
              handle); // [確認_異常系] - cplat_trace_file_sink_create の戻り値が NULL であること。
}

// ハンドルの確保に失敗した場合に生成が失敗することの確認
// [サブ手順参照 名前=traceFileAllocFailureTest.SetUp]
TEST_F(traceFileAllocFailureTest, create_returns_null_when_handle_allocation_fails)
{
    // Arrange
    // Pre-Assert
    /* 1 回目はレジストリ キー、2 回目がハンドルの確保になる */
    EXPECT_CALL(mock_cplat, cplat_malloc(_))
        .WillOnce(DoDefault())
        .WillOnce(Return(nullptr))
        .WillRepeatedly(
            DoDefault()); // [Pre-Assert確認_異常系] - cplat_malloc がハンドルの確保のために 2 回目に呼び出されること。
                          // [Pre-Assert手順] - 2 回目は NULL を返却し、他は本物へ委譲する。

    // Act
    cplat_trace_file_sink *handle =
        cplat_trace_file_sink_create("traceFileAllocFailureTest_handle.log", 0, 0,
                                     0); // [手順] - cplat_trace_file_sink_create を呼び出す。

    // Assert
    EXPECT_EQ((cplat_trace_file_sink *)NULL,
              handle); // [確認_異常系] - cplat_trace_file_sink_create の戻り値が NULL であること。
}

// パス文字列の複製に失敗した場合に生成が失敗することの確認
// [サブ手順参照 名前=traceFileAllocFailureTest.SetUp]
TEST_F(traceFileAllocFailureTest, create_returns_null_when_path_duplication_fails)
{
    // Arrange
    // Pre-Assert
    /* 1 回目はレジストリ キー、2 回目はハンドル、3 回目がパス文字列の複製になる */
    EXPECT_CALL(mock_cplat, cplat_malloc(_))
        .WillOnce(DoDefault())
        .WillOnce(DoDefault())
        .WillOnce(Return(nullptr))
        .WillRepeatedly(
            DoDefault()); // [Pre-Assert確認_異常系] - cplat_malloc がパス文字列の複製のために 3 回目に呼び出されること。
                          // [Pre-Assert手順] - 3 回目は NULL を返却し、他は本物へ委譲する。

    // Act
    cplat_trace_file_sink *handle =
        cplat_trace_file_sink_create("traceFileAllocFailureTest_path.log", 0, 0,
                                     0); // [手順] - cplat_trace_file_sink_create を呼び出す。

    // Assert
    EXPECT_EQ((cplat_trace_file_sink *)NULL,
              handle); // [確認_異常系] - cplat_trace_file_sink_create の戻り値が NULL であること。
}

// レジストリ配列の拡張に失敗した場合に生成が失敗することの確認
// [サブ手順参照 名前=traceFileAllocFailureTest.SetUp]
TEST_F(traceFileAllocFailureTest, create_returns_null_when_registry_expansion_fails)
{
    // Arrange
    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_realloc(_, _, _))
        .WillOnce(Return(nullptr))
        .WillRepeatedly(
            DoDefault()); // [Pre-Assert確認_異常系] - cplat_realloc がレジストリ配列の拡張のために 1 回目に呼び出されること。
                          // [Pre-Assert手順] - 1 回目は NULL を返却し、以降は本物へ委譲する。

    // Act
    cplat_trace_file_sink *handle =
        cplat_trace_file_sink_create("traceFileAllocFailureTest_registry.log", 0, 0,
                                     0); // [手順] - cplat_trace_file_sink_create を呼び出す。

    // Assert
    EXPECT_EQ((cplat_trace_file_sink *)NULL,
              handle); // [確認_異常系] - cplat_trace_file_sink_create の戻り値が NULL であること。
}
