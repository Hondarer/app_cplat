#include <testfw.h>
#include <cplat/crt/path.h>
#include <cplat/runtime/process.h>
#include <cplat/trace/tracer.h>
#include <string>
#include <thread>
#include <vector>

class traceFileIntegrationTest : public Test
{
};

// tracer-managed モードで実 rwlock を使用した並行参照が成功することの確認
TEST_F(traceFileIntegrationTest, tracer_managed_mode_supports_concurrent_reads)
{
    // Arrange
    cplat_tracer *handle = cplat_tracer_create(CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED);
    ASSERT_NE(nullptr, handle);
    // [状態確認] - cplat_tracer_create() の戻り値が nullptr 以外であること。
    ASSERT_EQ(CPLAT_OK, cplat_tracer_set_file_level(handle, nullptr, CPLAT_TRACE_LEVEL_NONE, 0, 0, 0));
    // [状態確認] - cplat_tracer_set_file_level() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_tracer_start(handle));
    // [状態確認] - cplat_tracer_start() の戻り値が CPLAT_OK であること。
    std::vector<std::thread> workers;
    std::vector<int> results(8, CPLAT_ERR_UNKNOWN);

    // Pre-Assert

    // Act
    for (size_t i = 0; i < results.size(); i++)
    {
        workers.emplace_back([handle, &results, i]() { results[i] = cplat_tracer_get_state(handle); });
    }
    for (std::thread &worker : workers)
    {
        worker.join();
    }
    cplat_tracer_dispose(&handle); // [手順] - 並行参照の完了後の tracer を指定して cplat_tracer_dispose() を呼び出す。

    // Assert
    for (int result : results)
    {
        EXPECT_EQ(
            CPLAT_TRACER_STATE_STARTED,
            result); // [確認_正常系 回数=8] - 各スレッドの cplat_tracer_get_state() の戻り値が CPLAT_TRACER_STATE_STARTED であること。
    }
    EXPECT_EQ(
        nullptr,
        handle); // [確認_正常系] - handle が nullptr であること。(cplat_tracer_dispose() がハンドルを nullptr にしたことの確認)
}

// 両方の並行処理管理モードで生成と破棄を繰り返せることの確認
TEST_F(traceFileIntegrationTest, both_concurrency_modes_repeat_lifecycle)
{
    // Arrange
    const cplat_tracer_concurrency_mode modes[] = {CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED,
                                                   CPLAT_TRACER_CONCURRENCY_TRACER_MANAGED};

    // Pre-Assert

    // Act
    std::vector<cplat_tracer *> disposed_handles;
    for (cplat_tracer_concurrency_mode mode : modes)
    {
        for (int i = 0; i < 16; i++)
        {
            cplat_tracer *handle =
                cplat_tracer_create(mode); // [手順] - 各並行処理管理モードを指定して cplat_tracer_create() を呼び出す。
            ASSERT_NE(nullptr, handle);
            // [確認_正常系 回数=2*16] - cplat_tracer_create() の戻り値が nullptr 以外であること。
            cplat_tracer_dispose(&handle); // [手順] - 生成した tracer を指定して cplat_tracer_dispose() を呼び出す。
            disposed_handles.push_back(handle);
        }
    }

    // Assert
    for (cplat_tracer *handle : disposed_handles)
    {
        EXPECT_EQ(
            nullptr,
            handle); // [確認_正常系 回数=2*16] - handle が nullptr であること。(cplat_tracer_dispose() がハンドルを nullptr にしたことの確認)
    }
}

// file trace を有効化すると実ファイルへメッセージが書き込まれることの確認
TEST_F(traceFileIntegrationTest, test_enable_file_trace_writes_messages)
{
    // Arrange
    std::string ws = findWorkspaceRoot();
    std::string path = ws + "/app/cplat/test/src/integration/trace/traceFileIntegrationTest/results/trace_test.log";
    remove(path.c_str());

    cplat_tracer *handle = cplat_tracer_create(
        CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED); // [状態] - 生成済みのトレース ハンドルを用意する。
    ASSERT_NE(nullptr, handle); // [状態確認] - cplat_tracer_create() の戻り値が nullptr 以外であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_tracer_set_os_level(handle, CPLAT_TRACE_LEVEL_NONE)); // [状態] - OS レベルを NONE とする。
    // [状態確認] - cplat_tracer_set_os_level() の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int actual_ret_tracer_set_file_level = cplat_tracer_set_file_level(
        handle, path.c_str(), CPLAT_TRACE_LEVEL_INFO, 0, 0,
        0); // [手順] - INFO レベルのファイル トレースを指定して cplat_tracer_set_file_level() を呼び出す。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_tracer_set_file_level); // [確認_正常系] - file trace を有効化した cplat_tracer_set_file_level() の戻り値が CPLAT_OK であること。
    int actual_ret_start = cplat_tracer_start(handle);
    ASSERT_EQ(CPLAT_OK, actual_ret_start);
    // [状態確認] - cplat_tracer_start() の戻り値が CPLAT_OK であること。
    int actual_ret_tracer_write = cplat_tracer_write_at(
        handle, CPLAT_TRACE_LEVEL_ERROR, nullptr,
        "file error message"); // [手順] - ERROR レベルと "file error message" を指定して cplat_tracer_write_at() を呼び出す。
    int actual_ret_tracer_write_2 = cplat_tracer_write_at(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "file info message"); // [手順] - INFO レベルと "file info message" を指定して cplat_tracer_write_at() を呼び出す。
    cplat_tracer_dispose(&handle);

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_tracer_write); // [確認_正常系] - ERROR 行を書き込んだ cplat_tracer_write_at() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_tracer_write_2); // [確認_正常系] - INFO 行を書き込んだ cplat_tracer_write_at() の戻り値が CPLAT_OK であること。
    EXPECT_FILE_EXISTS(path); // [確認_正常系] - path のファイルが存在すること。
    EXPECT_FILE_CONTAINS(path,
                         "file error message"); // [確認_正常系] - path のファイルが "file error message" を含むこと。
    EXPECT_FILE_CONTAINS(path,
                         "file info message"); // [確認_正常系] - path のファイルが "file info message" を含むこと。

    // Cleanup
    remove(path.c_str());
}

// file level が WARNING 以下の出力を抑止することの確認
TEST_F(traceFileIntegrationTest, test_file_level_filters_messages)
{
    // Arrange
    std::string ws = findWorkspaceRoot();
    std::string path = ws + "/app/cplat/test/src/integration/trace/traceFileIntegrationTest/results/trace_filter.log";
    remove(path.c_str());

    cplat_tracer *handle = cplat_tracer_create(
        CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED); // [状態] - 生成済みのトレース ハンドルを用意する。
    ASSERT_NE(nullptr, handle); // [状態確認] - cplat_tracer_create() の戻り値が nullptr 以外であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_tracer_set_os_level(handle, CPLAT_TRACE_LEVEL_NONE)); // [状態] - OS レベルを NONE とする。
    // [状態確認] - cplat_tracer_set_os_level() の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int actual_ret_tracer_set_file_level =
        cplat_tracer_set_file_level(handle, path.c_str(), CPLAT_TRACE_LEVEL_ERROR, 0, 0,
                                    0); // [手順] - ERROR レベルを指定して cplat_tracer_set_file_level() を呼び出す。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_tracer_set_file_level); // [確認_正常系] - file level を ERROR に設定した cplat_tracer_set_file_level() の戻り値が CPLAT_OK であること。
    int actual_ret_start = cplat_tracer_start(handle);
    ASSERT_EQ(CPLAT_OK, actual_ret_start);
    // [状態確認] - cplat_tracer_start() の戻り値が CPLAT_OK であること。
    int actual_ret_tracer_write = cplat_tracer_write_at(
        handle, CPLAT_TRACE_LEVEL_ERROR, nullptr,
        "should be in file"); // [手順] - ERROR レベルと "should be in file" を指定して cplat_tracer_write_at() を呼び出す。
    int actual_ret_tracer_write_2 = cplat_tracer_write_at(
        handle, CPLAT_TRACE_LEVEL_WARNING, nullptr,
        "should not be in file"); // [手順] - WARNING レベルと "should not be in file" を指定して cplat_tracer_write_at() を呼び出す。
    cplat_tracer_dispose(&handle);

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_tracer_write); // [確認_正常系] - ERROR 行を書き込んだ cplat_tracer_write_at() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_tracer_write_2); // [確認_正常系] - WARNING 行を書き込んだ cplat_tracer_write_at() の戻り値が CPLAT_OK であること。
    EXPECT_FILE_EXISTS(path); // [確認_正常系] - path のファイルが存在すること。
    EXPECT_FILE_CONTAINS(path,
                         "should be in file"); // [確認_正常系] - path のファイルが "should be in file" を含むこと。
    EXPECT_FALSE(testing::FileContains(
        path,
        "should not be in file")); // [確認_正常系] - "should not be in file" を探した testing::FileContains() の戻り値が false であること。(WARNING 行がファイルに含まれないことの確認)

    // Cleanup
    remove(path.c_str());
}

// DEBUG 設定時に V と D の marker が実ファイルへ出力されることの確認
TEST_F(traceFileIntegrationTest, test_debug_level_outputs_verbose_and_debug_markers)
{
    // Arrange
    std::string ws = findWorkspaceRoot();
    std::string path = ws + "/app/cplat/test/src/integration/trace/traceFileIntegrationTest/results/trace_debug.log";
    remove(path.c_str());

    cplat_tracer *handle = cplat_tracer_create(
        CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED); // [状態] - 生成済みのトレース ハンドルを用意する。
    ASSERT_NE(nullptr, handle); // [状態確認] - cplat_tracer_create() の戻り値が nullptr 以外であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_tracer_set_os_level(handle, CPLAT_TRACE_LEVEL_NONE)); // [状態] - OS レベルを NONE とする。
    // [状態確認] - cplat_tracer_set_os_level() の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int actual_ret_tracer_set_file_level =
        cplat_tracer_set_file_level(handle, path.c_str(), CPLAT_TRACE_LEVEL_DEBUG, 0, 0,
                                    0); // [手順] - DEBUG レベルを指定して cplat_tracer_set_file_level() を呼び出す。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_tracer_set_file_level); // [確認_正常系] - file level を DEBUG に設定した cplat_tracer_set_file_level() の戻り値が CPLAT_OK であること。
    int actual_ret_start = cplat_tracer_start(handle);
    ASSERT_EQ(CPLAT_OK, actual_ret_start);
    // [状態確認] - cplat_tracer_start() の戻り値が CPLAT_OK であること。
    int actual_ret_tracer_write = cplat_tracer_write_at(
        handle, CPLAT_TRACE_LEVEL_VERBOSE, nullptr,
        "verbose in debug file"); // [手順] - VERBOSE レベルと "verbose in debug file" を指定して cplat_tracer_write_at() を呼び出す。
    int actual_ret_tracer_write_2 = cplat_tracer_write_at(
        handle, CPLAT_TRACE_LEVEL_DEBUG, nullptr,
        "debug in debug file"); // [手順] - DEBUG レベルと "debug in debug file" を指定して cplat_tracer_write_at() を呼び出す。
    cplat_tracer_dispose(&handle);

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_tracer_write); // [確認_正常系] - VERBOSE 行を書き込んだ cplat_tracer_write_at() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_tracer_write_2); // [確認_正常系] - DEBUG 行を書き込んだ cplat_tracer_write_at() の戻り値が CPLAT_OK であること。
    EXPECT_FILE_EXISTS(path); // [確認_正常系] - path のファイルが存在すること。
    EXPECT_FILE_CONTAINS(
        path, " V verbose in debug file"); // [確認_正常系] - path のファイルが " V verbose in debug file" を含むこと。
    EXPECT_FILE_CONTAINS(
        path, " D debug in debug file"); // [確認_正常系] - path のファイルが " D debug in debug file" を含むこと。

    // Cleanup
    remove(path.c_str());
}

// level NONE 指定で file trace を無効化できることの確認
TEST_F(traceFileIntegrationTest, test_level_none_disables_file_trace)
{
    // Arrange
    std::string ws = findWorkspaceRoot();
    std::string path = ws + "/app/cplat/test/src/integration/trace/traceFileIntegrationTest/results/trace_disable.log";
    remove(path.c_str());

    cplat_tracer *handle = cplat_tracer_create(
        CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED); // [状態] - 生成済みのトレース ハンドルを用意する。
    ASSERT_NE(nullptr, handle); // [状態確認] - cplat_tracer_create() の戻り値が nullptr 以外であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_tracer_set_os_level(handle, CPLAT_TRACE_LEVEL_NONE)); // [状態] - OS レベルを NONE とする。
    // [状態確認] - cplat_tracer_set_os_level() の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int actual_ret_tracer_set_file_level = cplat_tracer_set_file_level(
        handle, path.c_str(), CPLAT_TRACE_LEVEL_INFO, 0, 0,
        0); // [手順] - INFO レベルのファイル トレースを指定して cplat_tracer_set_file_level() を呼び出す。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_tracer_set_file_level); // [確認_正常系] - file trace を有効化した cplat_tracer_set_file_level() の戻り値が CPLAT_OK であること。
    int actual_ret_start = cplat_tracer_start(handle);
    ASSERT_EQ(CPLAT_OK, actual_ret_start);
    // [状態確認] - 無効化前の cplat_tracer_start() の戻り値が CPLAT_OK であること。
    int actual_ret_tracer_write = cplat_tracer_write_at(
        handle, CPLAT_TRACE_LEVEL_ERROR, nullptr,
        "before disable"); // [手順] - 無効化前の状態で ERROR レベルと "before disable" を指定して cplat_tracer_write_at() を呼び出す。
    int actual_ret_stop = cplat_tracer_stop(handle);
    ASSERT_EQ(CPLAT_OK, actual_ret_stop);
    // [状態確認] - cplat_tracer_stop() の戻り値が CPLAT_OK であること。
    int actual_ret_tracer_set_file_level_2 =
        cplat_tracer_set_file_level(handle, path.c_str(), CPLAT_TRACE_LEVEL_NONE, 0, 0,
                                    0); // [手順] - NONE レベルを指定して cplat_tracer_set_file_level() を呼び出す。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_tracer_set_file_level_2); // [確認_正常系] - level NONE で file trace を無効化した cplat_tracer_set_file_level() の戻り値が CPLAT_OK であること。
    int actual_ret_start_2 = cplat_tracer_start(handle);
    ASSERT_EQ(CPLAT_OK, actual_ret_start_2);
    // [状態確認] - 無効化後の cplat_tracer_start() の戻り値が CPLAT_OK であること。
    int actual_ret_tracer_write_2 = cplat_tracer_write_at(
        handle, CPLAT_TRACE_LEVEL_ERROR, nullptr,
        "after disable"); // [手順] - 無効化後の状態で ERROR レベルと "after disable" を指定して cplat_tracer_write_at() を呼び出す。
    cplat_tracer_dispose(&handle);

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_tracer_write); // [確認_正常系] - 無効化前の cplat_tracer_write_at() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_tracer_write_2); // [確認_正常系] - 無効化後の cplat_tracer_write_at() の戻り値が CPLAT_OK であること。
    EXPECT_FILE_EXISTS(path);       // [確認_正常系] - path のファイルが存在すること。
    EXPECT_FILE_CONTAINS(path, "before disable"); // [確認_正常系] - path のファイルが "before disable" を含むこと。
    EXPECT_FALSE(testing::FileContains(
        path,
        "after disable")); // [確認_正常系] - "after disable" を探した testing::FileContains() の戻り値が false であること。(無効化後の行が追加されないことの確認)

    // Cleanup
    remove(path.c_str());
}

namespace
{

// 既定のトレース ファイルのパス (実行ファイルのディレクトリ + /log/<プロセス名>.log) を導出する
static std::string build_expected_default_path(void)
{
    char exe_path[PLATFORM_PATH_MAX];
    if (cplat_process_get_executable_path(exe_path, sizeof(exe_path)) != CPLAT_OK)
    {
        return std::string();
    }
    std::string full(exe_path);
    size_t sep_pos = full.rfind(PLATFORM_PATH_SEP_CHR);
    if (sep_pos == std::string::npos)
    {
        return std::string();
    }
    std::string dir = full.substr(0, sep_pos);
    std::string base = full.substr(sep_pos + 1);
    if (base.size() > 4 && base.compare(base.size() - 4, 4, ".exe") == 0)
    {
        base.resize(base.size() - 4); // Windows のプロセス名から .exe を除去する
    }
    return dir + PLATFORM_PATH_SEP "log" PLATFORM_PATH_SEP + base + ".log";
}

} // namespace

// set_file_level でパスを指定しない場合に既定パス (実行ファイルのディレクトリ + /log/<プロセス名>.log) へ
// 書き込まれることの確認。set_name はトレース ファイル名に影響しないことも実証する。
TEST_F(traceFileIntegrationTest, test_default_path_writes_to_log_directory_next_to_executable)
{
    // Arrange
    std::string path = build_expected_default_path(); // [状態] - 既定のトレース ファイル パスを用意する。
    ASSERT_FALSE(path.empty());                       // [状態確認] - path.empty() の戻り値が false であること。
    remove(path.c_str());

    cplat_tracer *handle = cplat_tracer_create(
        CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED); // [状態] - 生成済みのトレース ハンドルを用意する。
    ASSERT_NE(nullptr, handle); // [状態確認] - cplat_tracer_create() の戻り値が nullptr 以外であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_tracer_set_os_level(handle, CPLAT_TRACE_LEVEL_NONE)); // [状態] - OS レベルを NONE とする。
    // [状態確認] - cplat_tracer_set_os_level() の戻り値が CPLAT_OK であること。
    int actual_ret_tracer_set_name = cplat_tracer_set_name(
        handle, "default_path_it", 0); // [状態] - インスタンス名を設定する (ファイル名には影響しない)。
    ASSERT_EQ(CPLAT_OK,
              actual_ret_tracer_set_name); // [状態確認] - cplat_tracer_set_name() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_tracer_set_file_level(handle, nullptr, CPLAT_TRACE_LEVEL_INFO, 0, 0,
                                                    0)); // [状態] - パスを指定せずファイル レベルを INFO とする。
    // [状態確認] - cplat_tracer_set_file_level() の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int actual_ret_tracer_start =
        cplat_tracer_start(handle); // [手順] - パスを指定しない状態で cplat_tracer_start() を呼び出す。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_tracer_start); // [確認_正常系] - パスを指定せず start した cplat_tracer_start() の戻り値が CPLAT_OK であること。
    int actual_ret_tracer_write = cplat_tracer_write_at(
        handle, CPLAT_TRACE_LEVEL_INFO, nullptr,
        "default path message"); // [手順] - INFO レベルと "default path message" を指定して cplat_tracer_write_at() を呼び出す。
    cplat_tracer_dispose(&handle);

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_tracer_write); // [確認_正常系] - INFO 行を書き込んだ cplat_tracer_write_at() の戻り値が CPLAT_OK であること。
    EXPECT_FILE_EXISTS(
        path); // [確認_正常系] - path のファイルが存在すること。(実行ファイルの隣の log ディレクトリの既定パスにファイルがあることの確認)
    EXPECT_FILE_CONTAINS(
        path, "default path message"); // [確認_正常系] - path のファイルが "default path message" を含むこと。

    // Cleanup
    remove(path.c_str());
}

// 同一プロセス内の複数 tracer が占有モードのまま同一の既定パスへ出力できることの確認 (プロセス内調停)
TEST_F(traceFileIntegrationTest, test_two_tracers_share_default_path_in_single_process)
{
    // Arrange
    std::string path = build_expected_default_path(); // [状態] - 既定のトレース ファイル パスを用意する。
    ASSERT_FALSE(path.empty());                       // [状態確認] - path.empty() の戻り値が false であること。
    remove(path.c_str());

    cplat_tracer *first =
        cplat_tracer_create(CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED); // [状態] - 1 つ目のトレース ハンドルを用意する。
    cplat_tracer *second =
        cplat_tracer_create(CPLAT_TRACER_CONCURRENCY_CALLER_MANAGED); // [状態] - 2 つ目のトレース ハンドルを用意する。
    ASSERT_NE(nullptr,
              first); // [状態確認] - 1 つ目の cplat_tracer_create() の戻り値が nullptr 以外であること。
    ASSERT_NE(nullptr,
              second); // [状態確認] - 2 つ目の cplat_tracer_create() の戻り値が nullptr 以外であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_tracer_set_os_level(first, CPLAT_TRACE_LEVEL_NONE)); // [状態] - 1 つ目の OS レベルを NONE とする。
    // [状態確認] - 1 つ目の cplat_tracer_set_os_level() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK,
              cplat_tracer_set_os_level(second, CPLAT_TRACE_LEVEL_NONE)); // [状態] - 2 つ目の OS レベルを NONE とする。
    // [状態確認] - 2 つ目の cplat_tracer_set_os_level() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_tracer_set_file_level(first, nullptr, CPLAT_TRACE_LEVEL_INFO, 0, 0,
                                                    0)); // [状態] - 1 つ目のファイル レベルを INFO とする。
    // [状態確認] - 1 つ目の cplat_tracer_set_file_level() の戻り値が CPLAT_OK であること。
    ASSERT_EQ(CPLAT_OK, cplat_tracer_set_file_level(second, nullptr, CPLAT_TRACE_LEVEL_INFO, 0, 0,
                                                    0)); // [状態] - 2 つ目のファイル レベルを INFO とする。
    // [状態確認] - 2 つ目の cplat_tracer_set_file_level() の戻り値が CPLAT_OK であること。

    // Pre-Assert

    // Act
    int actual_ret_tracer_start =
        cplat_tracer_start(first); // [手順] - 1 つ目の tracer を指定して cplat_tracer_start() を呼び出す。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_tracer_start); // [確認_正常系] - 1 つ目の cplat_tracer_start() の戻り値が CPLAT_OK であること。
    int actual_ret_tracer_start_2 = cplat_tracer_start(
        second); // [手順] - 同一の既定パスを使う 2 つ目の tracer を指定して cplat_tracer_start() を呼び出す。
    ASSERT_EQ(
        CPLAT_OK,
        actual_ret_tracer_start_2); // [確認_正常系] - 2 つ目の cplat_tracer_start() の戻り値が CPLAT_OK であること。
    int actual_ret_tracer_write = cplat_tracer_write_at(
        first, CPLAT_TRACE_LEVEL_ERROR, nullptr,
        "from first tracer"); // [手順] - 1 つ目の tracer と "from first tracer" を指定して cplat_tracer_write_at() を呼び出す。
    int actual_ret_tracer_write_2 = cplat_tracer_write_at(
        second, CPLAT_TRACE_LEVEL_ERROR, nullptr,
        "from second tracer"); // [手順] - 2 つ目の tracer と "from second tracer" を指定して cplat_tracer_write_at() を呼び出す。
    cplat_tracer_dispose(&first); // [手順] - 1 つ目の tracer を指定して cplat_tracer_dispose() を呼び出す。
    int actual_ret_tracer_write_3 = cplat_tracer_write_at(
        second, CPLAT_TRACE_LEVEL_ERROR, nullptr,
        "after first dispose"); // [手順] - 1 つ目の解放後に 2 つ目の tracer と "after first dispose" を指定して cplat_tracer_write_at() を呼び出す。
    cplat_tracer_dispose(&second);

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_tracer_write); // [確認_正常系] - 1 つ目の cplat_tracer_write_at() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_tracer_write_2); // [確認_正常系] - 2 つ目の cplat_tracer_write_at() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_tracer_write_3); // [確認_正常系] - 1 つ目の解放後の cplat_tracer_write_at() の戻り値が CPLAT_OK であること。
    EXPECT_FILE_EXISTS(
        path); // [確認_正常系] - path のファイルが存在すること。(2 つの tracer が同一の既定パスのファイルを共有したことの確認)
    EXPECT_FILE_CONTAINS(path,
                         "from first tracer"); // [確認_正常系] - path のファイルが "from first tracer" を含むこと。
    EXPECT_FILE_CONTAINS(path,
                         "from second tracer"); // [確認_正常系] - path のファイルが "from second tracer" を含むこと。
    EXPECT_FILE_CONTAINS(path,
                         "after first dispose"); // [確認_正常系] - path のファイルが "after first dispose" を含むこと。

    // Cleanup
    remove(path.c_str());
}
