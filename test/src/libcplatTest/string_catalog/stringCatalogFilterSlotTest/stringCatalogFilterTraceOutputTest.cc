#include <testfw.h>
#include <mock_cplat.h>

#include "filterTestSupport.h"

#include "filter_test_catalog.h"
#include "gen/filter_test_trace.h"

#include <cplat/base/result.h>
#include <cplat/trace/tracer.h>

#include <cstdint>
#include <cstring>

using namespace filter_test;
using testing::_;
using testing::HasSubstr;
using testing::NiceMock;
using testing::Return;

/*
 *  生成器がトレース種別の生成物へ出力する、条件式フィルターとの接続を確認します。
 *  対象は gen/filter_test_trace.c の _create_filter、_set_filter、_write です。
 *  出力先のトレーサーは mock の cplat_tracer_write_at で受け、渡されたレベルを確認します。
 */
class stringCatalogFilterTraceOutputTest : public Test
{
  protected:
    NiceMock<Mock_cplat> mock_cplat;

    /** 出力先として渡すトレーサーです。cplat_tracer_write_at を mock で受けるため、中身は参照されません。 */
    uint64_t tracer_storage_ = 0U;

    unsigned char image_[kImageSize];

    cplat_string_catalog_filter_slot *slot_ = nullptr;

    /** テスト用カタログの識別値です。公開で指定します。 */
    static uint64_t trace_catalog_id()
    {
        uint64_t catalog_id = 0U;

        (void)cplat_string_catalog_filter_get_catalog_id(filter_test_trace_catalog(), &catalog_id);
        return catalog_id;
    }

    cplat_tracer *tracer()
    {
        return reinterpret_cast<cplat_tracer *>(&tracer_storage_);
    }

    void SetUp() override
    {
        memset(image_, 0, sizeof(image_));
        cplat_string_catalog_set_language(CPLAT_STRING_CATALOG_LANGUAGE_NEUTRAL);
        filter_test_trace_set_tracer(tracer());
        ON_CALL(mock_cplat, cplat_tracer_write_at(_, _, _, _)).WillByDefault(Return(CPLAT_OK));
    }

    void TearDown() override
    {
        (void)filter_test_trace_set_filter(nullptr);
        filter_test_trace_set_tracer(nullptr);
        cplat_string_catalog_filter_slot_dispose(&slot_);
    }

    /** 本カタログでスロットを作成し、条件式 1 行を適用して出力へ接続します。 */
    void connect_with_line(const char *text)
    {
        ASSERT_EQ(CPLAT_OK, filter_test_trace_create_filter(nullptr, kLineCapacity, kLineWidth, &slot_));
        // [状態確認] - `filter_test_trace_create_filter(nullptr, kLineCapacity, kLineWidth, &slot_)` の戻り値が `CPLAT_OK` であること。
        ASSERT_EQ(CPLAT_OK, compile_single_line(text, image_));
        // [状態確認] - `compile_single_line(text, image_)` の戻り値が `CPLAT_OK` であること。
        ASSERT_EQ(CPLAT_OK,
                  cplat_string_catalog_filter_slot_apply(slot_, image_, sizeof(image_), nullptr, 0U, nullptr));
        // [状態確認] - `cplat_string_catalog_filter_slot_apply(slot_, image_, sizeof(image_), nullptr, 0U, nullptr)` の戻り値が `CPLAT_OK` であること。
        ASSERT_EQ(CPLAT_OK, filter_test_trace_set_filter(slot_));
        // [状態確認] - `filter_test_trace_set_filter(slot_)` の戻り値が `CPLAT_OK` であること。
    }
};

// フィルターを接続しない場合は、定義のレベルで出力することの確認
TEST_F(stringCatalogFilterTraceOutputTest, unconnected_writes_with_defined_level)
{
    // Arrange
    int actual_ret;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_tracer_write_at(tracer(), CPLAT_TRACE_LEVEL_WARNING, nullptr,
                                                  HasSubstr("Job 5 failed. Error code=2")))
        .WillOnce(
            Return(CPLAT_OK)); // [Pre-Assert確認_正常系] - 定義のレベル WARNING で、組み立てた文字列を出力すること。

    // Act
    actual_ret = filter_test_trace_key_job_failed((uint64_t)5U, 2); // [手順] - JOB_FAILED を出力する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret);                    // [確認_正常系] - 出力に成功すること。
    EXPECT_EQ(nullptr, filter_test_trace_get_filter()); // [確認_正常系] - フィルターが未設定であること。
}

// 接続した条件式に一致した場合は、強制出力のレベルで出力することの確認
TEST_F(stringCatalogFilterTraceOutputTest, matched_trace_is_forced)
{
    // Arrange
    int actual_ret;
    connect_with_line("key == FILTER_TEST_TRACE_KEY_JOB_FAILED"); // [状態] - JOB_FAILED に一致する条件で接続する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_tracer_write_at(tracer(), CPLAT_TRACE_LEVEL_TO_FORCE(CPLAT_TRACE_LEVEL_WARNING),
                                                  nullptr, HasSubstr("Job 5 failed.")))
        .WillOnce(Return(CPLAT_OK)); // [Pre-Assert確認_正常系] - 強制出力のレベルで出力すること。

    // Act
    actual_ret = filter_test_trace_key_job_failed((uint64_t)5U, 2); // [手順] - JOB_FAILED を出力する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 出力に成功すること。
}

// 接続した条件式に一致しない場合は、定義のレベルのまま出力することの確認
TEST_F(stringCatalogFilterTraceOutputTest, unmatched_trace_keeps_defined_level)
{
    // Arrange
    int actual_ret;
    connect_with_line(
        "key == FILTER_TEST_TRACE_KEY_WORKER_STARTED"); // [状態] - JOB_FAILED に一致しない条件で接続する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_tracer_write_at(tracer(), CPLAT_TRACE_LEVEL_WARNING, nullptr, _))
        .WillOnce(Return(CPLAT_OK)); // [Pre-Assert確認_正常系] - 定義のレベルで出力すること。

    // Act
    actual_ret = filter_test_trace_key_job_failed((uint64_t)5U, 2); // [手順] - JOB_FAILED を出力する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 出力に成功すること。
}

// 別のカタログで作成したスロットは接続せず、それまでの接続を保つことの確認
TEST_F(stringCatalogFilterTraceOutputTest, set_filter_rejects_slot_of_another_catalog)
{
    // Arrange
    cplat_string_catalog_filter_slot *other_slot = nullptr;
    int actual_ret;
    connect_with_line("key == FILTER_TEST_TRACE_KEY_JOB_FAILED"); // [状態] - 本カタログのスロットで接続する。
    ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_create(
                            filter_test_catalog(), nullptr, 0U, nullptr, kLineCapacity, kLineWidth,
                            &other_slot)); // [状態] - 別のカタログでスロットを作成する。
    // [状態確認] - `cplat_string_catalog_filter_slot_create( filter_test_catalog(), nullptr, 0U, nullptr, kLineCapacity, kLineWidth, &other_slot)` の戻り値が `CPLAT_OK` であること。

    // Pre-Assert

    // Act
    actual_ret = filter_test_trace_set_filter(other_slot); // [手順] - 別のカタログのスロットを接続する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, actual_ret); // [確認_異常系] - CPLAT_ERR_INVALID_ARGUMENT を返すこと。
    EXPECT_EQ(slot_, filter_test_trace_get_filter());  // [確認_異常系] - それまでの接続を保つこと。

    cplat_string_catalog_filter_slot_dispose(&other_slot);
}

// 接続を解除すると、条件式に一致していたトレースも定義のレベルで出力することの確認
TEST_F(stringCatalogFilterTraceOutputTest, detached_filter_restores_defined_level)
{
    // Arrange
    int actual_detach_ret;
    connect_with_line("key == FILTER_TEST_TRACE_KEY_JOB_FAILED"); // [状態] - JOB_FAILED に一致する条件で接続する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_tracer_write_at(tracer(), CPLAT_TRACE_LEVEL_WARNING, nullptr, _))
        .WillOnce(Return(CPLAT_OK)); // [Pre-Assert確認_正常系] - 定義のレベルで出力すること。

    // Act
    actual_detach_ret = filter_test_trace_set_filter(nullptr); // [手順] - 接続を解除する。
    (void)filter_test_trace_key_job_failed((uint64_t)5U, 2);   // [手順] - JOB_FAILED を出力する。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_detach_ret);             // [確認_正常系] - 解除に成功すること。
    EXPECT_EQ(nullptr, filter_test_trace_get_filter()); // [確認_正常系] - フィルターが未設定になること。
}

// 作成したスロットが本カタログに結び付き、名前解決表で文字列キーの名前を解決することの確認
TEST_F(stringCatalogFilterTraceOutputTest, create_filter_binds_own_catalog_and_key_names)
{
    // Arrange
    size_t actual_invalid_count = 1U;
    int actual_apply_ret;
    ASSERT_EQ(CPLAT_OK, compile_single_line("key == FILTER_TEST_TRACE_KEY_WORKER_STOPPED",
                                            image_)); // [状態] - 文字列キーの名前を使う条件をコンパイルする。
    // [状態確認] - `compile_single_line("key == FILTER_TEST_TRACE_KEY_WORKER_STOPPED", image_)` の戻り値が `CPLAT_OK` であること。

    // Pre-Assert

    // Act
    ASSERT_EQ(CPLAT_OK, filter_test_trace_create_filter(nullptr, kLineCapacity, kLineWidth,
                                                        &slot_)); // [手順] - スロットを作成する。
    // [確認_正常系] - `filter_test_trace_create_filter(nullptr, kLineCapacity, kLineWidth, &slot_)` の戻り値が `CPLAT_OK` であること。
    actual_apply_ret = cplat_string_catalog_filter_slot_apply(slot_, image_, sizeof(image_), nullptr, 0U,
                                                              &actual_invalid_count); // [手順] - 条件を適用する。

    // Assert
    EXPECT_EQ(filter_test_trace_catalog(),
              cplat_string_catalog_filter_slot_get_catalog(slot_)); // [確認_正常系] - 本カタログに結び付くこと。
    EXPECT_EQ((size_t)filter_test_trace_entry_count(),
              filter_test_trace_key_name_count()); // [確認_正常系] - 名前解決表が全項目を持つこと。
    EXPECT_EQ(CPLAT_OK, actual_apply_ret);         // [確認_正常系] - 適用に成功すること。
    EXPECT_EQ(0U, actual_invalid_count);           // [確認_正常系] - 文字列キーの名前を解決し、無効な行がないこと。
}

// ソース領域を結び付けたスロットでは、出力のたびに公開された条件を取り込むことの確認
TEST_F(stringCatalogFilterTraceOutputTest, attached_source_is_taken_on_write)
{
    // Arrange
    alignas(8) static unsigned char source[CPLAT_STRING_CATALOG_FILTER_SOURCE_SIZE(kLineCapacity, kLineWidth)];
    memset(source, 0, sizeof(source));
    ASSERT_EQ(CPLAT_OK, filter_test_trace_create_filter(nullptr, kLineCapacity, kLineWidth,
                                                        &slot_)); // [状態] - スロットを作成する。
    // [状態確認] - `filter_test_trace_create_filter(nullptr, kLineCapacity, kLineWidth, &slot_)` の戻り値が `CPLAT_OK` であること。
    ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_slot_attach_source(slot_, source, sizeof(source),
                                                                       nullptr)); // [状態] - ソース領域を結び付ける。
    // [状態確認] - `cplat_string_catalog_filter_slot_attach_source(slot_, source, sizeof(source), nullptr)` の戻り値が `CPLAT_OK` であること。
    ASSERT_EQ(CPLAT_OK, filter_test_trace_set_filter(slot_));                     // [状態] - 出力へ接続する。
    // [状態確認] - `filter_test_trace_set_filter(slot_)` の戻り値が `CPLAT_OK` であること。
    ASSERT_EQ(CPLAT_OK, compile_single_line("category <= 2", image_));
    // [状態確認] - `compile_single_line("category <= 2", image_)` の戻り値が `CPLAT_OK` であること。
    ASSERT_EQ(CPLAT_OK, cplat_string_catalog_filter_source_publish(
                            source, sizeof(source), image_, sizeof(image_), trace_catalog_id(), nullptr,
                            nullptr)); // [状態] - WARNING 以上に一致する条件を公開する。
    // [状態確認] - `cplat_string_catalog_filter_source_publish( source, sizeof(source), image_, sizeof(image_), trace_catalog_id(), nullptr, nullptr)` の戻り値が `CPLAT_OK` であること。

    // Pre-Assert
    EXPECT_CALL(mock_cplat,
                cplat_tracer_write_at(tracer(), CPLAT_TRACE_LEVEL_TO_FORCE(CPLAT_TRACE_LEVEL_WARNING), nullptr, _))
        .WillOnce(
            Return(CPLAT_OK)); // [Pre-Assert確認_正常系] - 公開された条件で判定し、強制出力のレベルで出力すること。

    // Act
    (void)filter_test_trace_key_job_failed((uint64_t)5U, 2); // [手順] - JOB_FAILED を出力する。

    // Assert
    (void)cplat_string_catalog_filter_slot_attach_source(slot_, nullptr, 0U, nullptr);
}
