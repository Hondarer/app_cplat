#include <cplat/base/platform.h>
#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_LINUX)

    #include <cstdio>
    #include <cstring>

extern "C"
{
    #include "bench_case.h"
}

// 環境収集だけを検証するため、表の行出力が参照する依存関数を fake として定義する。
extern "C" const char *bench_api_name(bench_api, int)
{
    return "fake-api";
}

extern "C" const char *bench_pattern_name(bench_pattern)
{
    return "fake-pattern";
}

namespace
{

// /proc/cpuinfo の 1 行分を保持する一時ストリームを作成する。
FILE *create_cpuinfo_stream(const char *content)
{
    FILE *fp = std::tmpfile();
    if (fp == nullptr)
    {
        return nullptr;
    }

    if (std::fputs(content, fp) == EOF)
    {
        (void)std::fclose(fp);
        return nullptr;
    }

    std::rewind(fp);
    return fp;
}

} // namespace

// /proc/cpuinfo の CPU 名の先頭の空白が除去されることの確認
TEST(benchReportTest, collect_environment_trims_leading_space_of_cpu_model)
{
    // Arrange
    NiceMock<Mock_cplat> mock;
    bench_environment env = {};
    FILE *cpuinfo = create_cpuinfo_stream(
        "model name\t: Test CPU @ 2.00GHz\n"); // [状態] - 先頭に半角空白を 1 つ含む model name 行の一時ファイルを用意する。
    ASSERT_NE(nullptr, cpuinfo);

    // Pre-Assert
    EXPECT_CALL(mock, cplat_fopen(StrEq("/proc/cpuinfo"), StrEq("r"), _))
        .WillOnce(Return(
            cpuinfo)); // [Pre-Assert確認_正常系] - cplat_fopen() が "/proc/cpuinfo" を読み取り専用で 1 回開くこと。
                       // [Pre-Assert手順] - 用意した一時ファイルのストリームを返す。

    // Act
    bench_report_collect_environment("/tmp", &env); // [手順] - bench_report_collect_environment() を呼び出す。

    // Assert
    EXPECT_STREQ("Test CPU @ 2.00GHz",
                 env.cpu_model); // [確認_正常系] - env.cpu_model の先頭の空白が除去されていること。
}

// /proc/cpuinfo の CPU 名の先頭のタブと複数の空白が除去されることの確認
TEST(benchReportTest, collect_environment_trims_leading_tabs_and_spaces_of_cpu_model)
{
    // Arrange
    NiceMock<Mock_cplat> mock;
    bench_environment env = {};
    FILE *cpuinfo = create_cpuinfo_stream(
        "model name\t:\t  Test CPU\n"); // [状態] - 先頭にタブと半角空白を含む model name 行の一時ファイルを用意する。
    ASSERT_NE(nullptr, cpuinfo);

    // Pre-Assert
    EXPECT_CALL(mock, cplat_fopen(StrEq("/proc/cpuinfo"), StrEq("r"), _))
        .WillOnce(Return(
            cpuinfo)); // [Pre-Assert確認_正常系] - cplat_fopen() が "/proc/cpuinfo" を読み取り専用で 1 回開くこと。
                       // [Pre-Assert手順] - 用意した一時ファイルのストリームを返す。

    // Act
    bench_report_collect_environment("/tmp", &env); // [手順] - bench_report_collect_environment() を呼び出す。

    // Assert
    EXPECT_STREQ("Test CPU",
                 env.cpu_model); // [確認_正常系] - env.cpu_model の先頭のタブと空白がすべて除去されていること。
}

// /proc/cpuinfo の CPU 名の先頭に空白がない場合に値がそのまま保持されることの確認
TEST(benchReportTest, collect_environment_keeps_cpu_model_without_leading_space)
{
    // Arrange
    NiceMock<Mock_cplat> mock;
    bench_environment env = {};
    FILE *cpuinfo = create_cpuinfo_stream(
        "model name:Test CPU\n"); // [状態] - コロンの直後に空白を含まない model name 行の一時ファイルを用意する。
    ASSERT_NE(nullptr, cpuinfo);

    // Pre-Assert
    EXPECT_CALL(mock, cplat_fopen(StrEq("/proc/cpuinfo"), StrEq("r"), _))
        .WillOnce(Return(
            cpuinfo)); // [Pre-Assert確認_正常系] - cplat_fopen() が "/proc/cpuinfo" を読み取り専用で 1 回開くこと。
                       // [Pre-Assert手順] - 用意した一時ファイルのストリームを返す。

    // Act
    bench_report_collect_environment("/tmp", &env); // [手順] - bench_report_collect_environment() を呼び出す。

    // Assert
    EXPECT_STREQ("Test CPU", env.cpu_model); // [確認_正常系] - env.cpu_model が "Test CPU" のまま保持されること。
}

#endif /* PLATFORM_LINUX */
