#ifndef TRACE_FILE_REGISTRY_TEST_H
#define TRACE_FILE_REGISTRY_TEST_H

#include <testfw.h>
#include <mock_cplat.h>

#include "trace_file.inject.h"

#include <filesystem>
#include <random>
#include <string>
#include <system_error>

class traceFileRegistryTest : public Test
{
  protected:
    testing::NiceMock<Mock_cplat> mock_cplat;
    std::filesystem::path temporary_directory;

    bool CreateTemporaryDirectory()
    {
        std::error_code error;
        const auto root = std::filesystem::temp_directory_path(error);
        if (error)
        {
            return false;
        }

        std::random_device random;
        const auto candidate = root / ("traceFileRegistryTest-" + std::to_string(random()) + "-" +
                                       std::to_string(random()));
        if (!std::filesystem::create_directory(candidate, error))
        {
            return false;
        }

        temporary_directory = candidate;
        return true;
    }

    // [サブ手順 名前=traceFileRegistryTest.SetUp]
    void SetUp() override
    {
        test_trace_file_reset_registry(); // [状態] - 登録表、ロック、初回初期化の結果を未初期化状態に戻す。
    }
    // [サブ手順終了]

    // [サブ手順 名前=traceFileRegistryTest.TearDown]
    void TearDown() override
    {
        testing::Mock::VerifyAndClearExpectations(&mock_cplat);
        test_trace_file_reset_registry(); // [手順] - 残った sink と登録表を解放し、ロックと初回初期化の結果を戻す。
        if (!temporary_directory.empty())
        {
            std::error_code error;
            (void)std::filesystem::remove_all(temporary_directory, error);
            EXPECT_FALSE(error) << error.message();
            // [手順] - 使用した一時ディレクトリを削除し、削除エラーを検出する。
        }
    }
    // [サブ手順終了]
};

#endif
