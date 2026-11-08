#include <testfw.h>
#include <mock_cplat.h>
extern "C"
{
#include "bench_case.h"
}

// 読み取り終了処理だけを検証するため、書き込み用の依存関数を fake として定義する。
extern "C" void bench_fill_record(size_t, bench_record *)
{
    ADD_FAILURE() << "unexpected record write";
}

// stdio の測定終了時のクローズ失敗を測定成功としないことの確認
TEST(benchIoFailureTest, stdio_close_failure_is_measurement_failure)
{
    // Arrange
    NiceMock<Mock_cplat> mock;
    bench_context context = {};
    context.path = "test.bin";
    bench_case job = {};
    job.api = BENCH_API_STDIO_REC;
    job.pattern = BENCH_PATTERN_SEQ_READ;
    FILE *stream = reinterpret_cast<FILE *>(uintptr_t{1});

    // Pre-Assert
    EXPECT_CALL(mock, cplat_fopen(_, _, _)).WillOnce(Return(stream));

    // [Pre-Assert確認_正常系] - ファイルを 1 回開くこと。
    // [Pre-Assert手順] - センチネルのストリームを返す。
    EXPECT_CALL(mock, cplat_fclose(stream, _)).WillOnce(Return(CPLAT_ERR_UNKNOWN));

    // [Pre-Assert確認_異常系] - センチネルのストリームを 1 回閉じること。
    // [Pre-Assert手順] - クローズ失敗を返す。

    // Act
    int result = bench_stdio_iterate(&context, &job); // [手順] - 読み取り後のクローズを失敗させる。

    // Assert
    EXPECT_EQ(-1, result); // [確認_異常系] - 測定が -1 を返すこと。
}

// mmap の反復中の解放失敗を測定成功としないことの確認
TEST(benchIoFailureTest, mmap_iteration_detach_failure_is_measurement_failure)
{
    // Arrange
    NiceMock<Mock_cplat> mock;
    bench_context context = {};
    context.path = "test.bin";
    bench_case job = {};
    job.api = BENCH_API_MMAP_EACH;
    job.pattern = BENCH_PATTERN_SEQ_READ;
    cplat_mmap *map = reinterpret_cast<cplat_mmap *>(uintptr_t{1});
    bench_record record = {};

    // Pre-Assert
    EXPECT_CALL(mock, cplat_mmap_attach(_, _, _, _, _))
        .WillOnce(testing::DoAll(testing::SetArgPointee<3>(map), Return(CPLAT_OK)));

    // [Pre-Assert確認_正常系] - 対応付けを 1 回作成すること。
    // [Pre-Assert手順] - センチネルの対応付けを返す。
    EXPECT_CALL(mock, cplat_mmap_get_address(map)).WillOnce(Return(&record));

    // [Pre-Assert確認_正常系] - 対応付けのアドレスを 1 回取得すること。
    // [Pre-Assert手順] - 有効なレコードのアドレスを返す。
    EXPECT_CALL(mock, cplat_mmap_detach(map, _)).WillOnce(Return(CPLAT_ERR_UNKNOWN));

    // [Pre-Assert確認_異常系] - 対応付けを 1 回解放すること。
    // [Pre-Assert手順] - 解放失敗を返す。

    // Act
    int result = bench_mmap_iterate(&context, &job); // [手順] - 反復中の解放を失敗させる。

    // Assert
    EXPECT_EQ(-1, result); // [確認_異常系] - 測定が -1 を返すこと。
}

// 保持した mmap の終了時の解放失敗を呼び出し元へ返すことの確認
TEST(benchIoFailureTest, mmap_teardown_failure_clears_owned_state)
{
    // Arrange
    NiceMock<Mock_cplat> mock;
    bench_context context = {};
    context.path = "test.bin";
    bench_case job = {};
    job.api = BENCH_API_MMAP_ONCE;
    cplat_mmap *map = reinterpret_cast<cplat_mmap *>(uintptr_t{1});

    // Pre-Assert
    EXPECT_CALL(mock, cplat_mmap_attach(_, _, _, _, _))
        .WillOnce(testing::DoAll(testing::SetArgPointee<3>(map), Return(CPLAT_OK)));

    // [Pre-Assert確認_正常系] - 対応付けを 1 回作成すること。
    // [Pre-Assert手順] - センチネルを返す。
    ASSERT_EQ(0, bench_mmap_setup(&context, &job));
    EXPECT_CALL(mock, cplat_mmap_detach(map, _)).WillOnce(Return(CPLAT_ERR_UNKNOWN));

    // [Pre-Assert確認_異常系] - 保持した対応付けを 1 回解放すること。
    // [Pre-Assert手順] - 解放失敗を返す。

    // Act
    int result = bench_mmap_teardown(&context, &job); // [手順] - 終了時の解放を失敗させる。

    // Assert
    EXPECT_EQ(-1, result);             // [確認_異常系] - 終了処理の失敗を返すこと。
    EXPECT_EQ(nullptr, context.state); // [確認_異常系] - 解放済みの状態を保持しないこと。
}
