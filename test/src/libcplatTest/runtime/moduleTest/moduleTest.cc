#include <testfw.h>
#include <mock_cplat.h>
#if defined(PLATFORM_LINUX)
    #include <mock_dlfcn.h>
#elif defined(PLATFORM_WINDOWS)
    #include <mock_windows.h>
#endif
#include <cplat/base/result.h>
#include <cplat/crt/path.h>
#include <cplat/runtime/module.h>

#include <cerrno>
#include <cstring>
#include <string>

using testing::_;
using testing::NiceMock;
using testing::StrEq;

#include "module.inject.h"

class moduleTest : public Test
{
  protected:
    /* テスト バイナリ内の関数アドレス。所属モジュールの特定に使用する。
       共有ライブラリ側の関数は ADD_SRCS でテスト バイナリへ取り込まれるため、
       所属モジュールとしては常にテスト バイナリが解決される。 */
    static const void *self_func_addr()
    {
        return reinterpret_cast<const void *>(&self_func_addr);
    }
};

/*
 * cplat_module_get_path
 */

#if defined(PLATFORM_LINUX)
// 関数アドレスから所属モジュールの絶対パスが取得できることの確認 (Linux)
TEST_F(moduleTest, get_path_returns_absolute_path_of_owning_module_linux)
{
    // Arrange
    NiceMock<Mock_dlfcn> mock_dlfcn;
    NiceMock<Mock_cplat> mock_cplat;
    char path[PLATFORM_PATH_MAX] = {0}; // [状態] - 出力バッファーを用意する。
    const char kModulePath[] = "/opt/cplat/moduleTest";

    // Pre-Assert
    EXPECT_CALL(mock_dlfcn, dladdr(_, _, _, _, _))
        .WillOnce(
            [](const char *, int, const char *, const void *, void *raw_info)
            {
                Dl_info *info = static_cast<Dl_info *>(raw_info);
                info->dli_fname = "/opt/cplat/moduleTest";
                return 1;
            }); // [Pre-Assert確認_正常系] - dladdr() が 1 回呼び出されること。
                // [Pre-Assert手順] - dladdr() から、所属モジュール パスを持つ情報を設定して 1 を返す。
    EXPECT_CALL(mock_cplat, cplat_path_get_full(_, _, _, StrEq(kModulePath)))
        .WillOnce(
            [](char *path_out, size_t path_size, cplat_error *detail_out, const char *)
            {
                const char resolved[] = "/opt/cplat/moduleTest";
                (void)path_size;
                if (detail_out != nullptr)
                {
                    *detail_out = {};
                }
                std::memcpy(path_out, resolved, sizeof(resolved));
                return CPLAT_OK;
            }); // [Pre-Assert確認_正常系] - cplat_path_get_full() が kModulePath と等しい文字列を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_path_get_full() から、正規化済みパスを書き込んで CPLAT_OK を返す。

    // Act
    int actual_ret = cplat_module_get_path(
        path, sizeof(path),
        self_func_addr()); // [手順] - テスト バイナリ内の関数アドレスを指定して cplat_module_get_path() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_module_get_path() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(kModulePath, path); // [確認_正常系] - path が "/opt/cplat/moduleTest" であること。
}

// dladdr が失敗した場合に UNKNOWN を返すことの確認
TEST_F(moduleTest, get_path_returns_unknown_when_dladdr_fails)
{
    // Arrange
    NiceMock<Mock_dlfcn> mock_dlfcn;
    char path[PLATFORM_PATH_MAX] = {};

    // Pre-Assert
    EXPECT_CALL(mock_dlfcn, dladdr(_, _, _, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - dladdr() が 1 回呼び出されること。
                              // [Pre-Assert手順] - dladdr() から 0 を返す。

    // Act
    int actual_ret = cplat_module_get_path(
        path, sizeof(path),
        self_func_addr()); // [手順] - dladdr() が失敗する状態で cplat_module_get_path() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_module_get_path() の戻り値が CPLAT_ERR_UNKNOWN であること。
}

// dladdr がモジュール名を返さない場合に UNKNOWN を返すことの確認
TEST_F(moduleTest, get_path_returns_unknown_when_dladdr_has_no_filename)
{
    // Arrange
    NiceMock<Mock_dlfcn> mock_dlfcn;
    char path[PLATFORM_PATH_MAX] = {};

    // Pre-Assert
    EXPECT_CALL(mock_dlfcn, dladdr(_, _, _, _, _))
        .WillOnce(
            [](const char *, int, const char *, const void *, void *raw_info)
            {
                Dl_info *info = static_cast<Dl_info *>(raw_info);
                info->dli_fname = nullptr;
                return 1;
            }); // [Pre-Assert確認_異常系] - dladdr() が 1 回呼び出されること。
                // [Pre-Assert手順] - dladdr() から、dli_fname が nullptr の情報を設定して 1 を返す。

    // Act
    int actual_ret = cplat_module_get_path(
        path, sizeof(path),
        self_func_addr()); // [手順] - ファイル名のない dladdr() の情報が返る状態で cplat_module_get_path() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_module_get_path() の戻り値が CPLAT_ERR_UNKNOWN であること。
}

// dladdr が空のモジュール名を返す場合に UNKNOWN を返すことの確認
TEST_F(moduleTest, get_path_returns_unknown_when_dladdr_has_empty_filename)
{
    // Arrange
    NiceMock<Mock_dlfcn> mock_dlfcn;
    char path[PLATFORM_PATH_MAX] = {};

    // Pre-Assert
    EXPECT_CALL(mock_dlfcn, dladdr(_, _, _, _, _))
        .WillOnce(
            [](const char *, int, const char *, const void *, void *raw_info)
            {
                Dl_info *info = static_cast<Dl_info *>(raw_info);
                info->dli_fname = "";
                return 1;
            }); // [Pre-Assert確認_異常系] - dladdr() が 1 回呼び出されること。
                // [Pre-Assert手順] - dladdr() から、空の dli_fname を持つ情報を設定して 1 を返す。

    // Act
    int actual_ret = cplat_module_get_path(
        path, sizeof(path),
        self_func_addr()); // [手順] - 空のファイル名を持つ dladdr() の情報が返る状態で cplat_module_get_path() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_module_get_path() の戻り値が CPLAT_ERR_UNKNOWN であること。
}

// 所属モジュールのパス正規化がメモリ不足で失敗した場合に OUT_OF_MEMORY を返すことの確認
TEST_F(moduleTest, get_path_returns_out_of_memory_when_normalization_reports_out_of_memory)
{
    // Arrange
    NiceMock<Mock_dlfcn> mock_dlfcn;
    NiceMock<Mock_cplat> mock_cplat;
    char path[PLATFORM_PATH_MAX] = {};

    // Pre-Assert
    EXPECT_CALL(mock_dlfcn, dladdr(_, _, _, _, _))
        .WillOnce(
            [](const char *, int, const char *, const void *, void *raw_info)
            {
                Dl_info *info = static_cast<Dl_info *>(raw_info);
                info->dli_fname = "/opt/cplat/moduleTest";
                return 1;
            }); // [Pre-Assert確認_正常系] - dladdr() が 1 回呼び出されること。
                // [Pre-Assert手順] - dladdr() から、所属モジュール パスを持つ情報を設定して 1 を返す。
    EXPECT_CALL(mock_cplat, cplat_path_get_full(_, _, _, _))
        .WillOnce(
            [](char *, size_t, cplat_error *detail_out, const char *)
            {
                *detail_out = {CPLAT_ERROR_DOMAIN_ERRNO, CPLAT_ERR_OUT_OF_MEMORY, ENOMEM};
                return CPLAT_ERR_OUT_OF_MEMORY;
            }); // [Pre-Assert確認_異常系] - cplat_path_get_full() が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_path_get_full() から、詳細に ENOMEM を設定して CPLAT_ERR_OUT_OF_MEMORY を返す。

    // Act
    int actual_ret = cplat_module_get_path(
        path, sizeof(path),
        self_func_addr()); // [手順] - パス正規化がメモリ不足で失敗する状態で cplat_module_get_path() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY,
              actual_ret); // [確認_異常系] - cplat_module_get_path() の戻り値が CPLAT_ERR_OUT_OF_MEMORY であること。
}
#elif defined(PLATFORM_WINDOWS)
// 関数アドレスから所属モジュールの絶対パスが取得できることの確認 (Windows)
TEST_F(moduleTest, get_path_returns_absolute_path_of_owning_module_windows)
{
    // Arrange
    char path[PLATFORM_PATH_MAX] = {0}; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_module_get_path(
        path, sizeof(path),
        self_func_addr()); // [手順] - テスト バイナリ内の関数アドレスを指定して cplat_module_get_path() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_module_get_path() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(':',
              path[1]); // [確認_正常系] - path[1] が ':' であること。(絶対パスがドライブ レター形式であることの確認)
    EXPECT_EQ(
        '/', path[2]); // [確認_正常系] - path[2] が '/' であること。(絶対パスが '/' 区切りへ正規化されていることの確認)
    EXPECT_NE(
        nullptr,
        std::strstr(
            path,
            "moduleTest")); // [確認_正常系] - "moduleTest" を探した std::strstr() の戻り値が nullptr 以外であること。(path にモジュール名が含まれることの確認)
}

// 260 バイト超の非 ASCII モジュール パスが取得できることの確認 (Windows)
TEST_F(moduleTest, get_path_returns_non_ascii_path_over_260_bytes_windows)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    std::wstring wpath = L"C:\\";
    std::string expected = "C:/";
    for (int i = 0; i < 100; ++i)
    {
        wpath += L"\u3042";
        expected += "\xE3\x81\x82";
    }
    wpath += L"\\mod.dll";
    expected += "/mod.dll";
    char path[PLATFORM_PATH_UTF8_MAX] = {}; // [状態] - 300 バイト超の非 ASCII パス用の出力バッファーを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetModuleFileNameW(_, _, _, _, _, _))
        .WillOnce(
            [wpath](const char *, int, const char *, HMODULE, LPWSTR filename, DWORD size) -> DWORD
            {
                (void)size;
                std::memcpy(filename, wpath.c_str(), (wpath.size() + 1U) * sizeof(wchar_t));
                return static_cast<DWORD>(wpath.size());
            }); // [Pre-Assert確認_正常系] - GetModuleFileNameW() が 1 回呼び出されること。
                // [Pre-Assert手順] - GetModuleFileNameW() から 260 バイト超の非 ASCII パスを返す。

    // Act
    int actual_ret = cplat_module_get_path(
        path, sizeof(path),
        self_func_addr()); // [手順] - 260 バイト超の非 ASCII パスが返る状態で cplat_module_get_path() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret);      // [確認_正常系] - cplat_module_get_path() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(expected.c_str(), path); // [確認_正常系] - path が正規化済み非 ASCII パスと一致すること。
}

// MAX_PATH を超える長いモジュール パスが取得できることの確認 (Windows)
TEST_F(moduleTest, get_path_returns_path_over_max_path_windows)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    std::wstring wpath = L"\\\\?\\C:\\" + std::wstring(300u, L'a') + L"\\mod.dll";
    std::string expected = "//?/C:/" + std::string(300u, 'a') + "/mod.dll";
    char path[1024] = {}; // [状態] - 300 文字超の長いパスを収容できる出力バッファーを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetModuleFileNameW(_, _, _, _, _, _))
        .WillOnce(
            [wpath](const char *, int, const char *, HMODULE, LPWSTR filename, DWORD size) -> DWORD
            {
                (void)size;
                std::memcpy(filename, wpath.c_str(), (wpath.size() + 1U) * sizeof(wchar_t));
                return static_cast<DWORD>(wpath.size());
            }); // [Pre-Assert確認_正常系] - GetModuleFileNameW() が 1 回呼び出されること。
                // [Pre-Assert手順] - GetModuleFileNameW() から MAX_PATH を超える長いパスを返す。

    // Act
    int actual_ret = cplat_module_get_path(
        path, sizeof(path),
        self_func_addr()); // [手順] - MAX_PATH を超えるパスが返る状態で cplat_module_get_path() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret);      // [確認_正常系] - cplat_module_get_path() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(expected.c_str(), path); // [確認_正常系] - path が正規化済みの長いパスと一致すること。
}

// 呼び出し元の出力バッファー不足時に BUFFER_TOO_SMALL を返すことの確認 (Windows)
TEST_F(moduleTest, get_path_returns_buffer_too_small_for_caller_buffer_windows)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    std::wstring wpath = L"\\\\?\\C:\\" + std::wstring(300u, L'a') + L"\\mod.dll";
    char path[64] = {'x'}; // [状態] - 長いパスが収まらない 64 バイトの出力バッファーを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetModuleFileNameW(_, _, _, _, _, _))
        .WillOnce(
            [wpath](const char *, int, const char *, HMODULE, LPWSTR filename, DWORD size) -> DWORD
            {
                (void)size;
                std::memcpy(filename, wpath.c_str(), (wpath.size() + 1U) * sizeof(wchar_t));
                return static_cast<DWORD>(wpath.size());
            }); // [Pre-Assert確認_異常系] - GetModuleFileNameW() が 1 回呼び出されること。
                // [Pre-Assert手順] - GetModuleFileNameW() から 300 文字超の長いパスを返す。

    // Act
    int actual_ret = cplat_module_get_path(
        path, sizeof(path),
        self_func_addr()); // [手順] - 呼び出し元のバッファーが不足する状態で cplat_module_get_path() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              actual_ret); // [確認_異常系] - cplat_module_get_path() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_EQ('\0', path[0]); // [確認_異常系] - path[0] が '\0' であること。
}

// GetModuleFileNameW が失敗した場合に UNKNOWN を返すことの確認 (Windows)
TEST_F(moduleTest, get_path_returns_unknown_when_module_file_name_fails_windows)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    char path[PLATFORM_PATH_UTF8_MAX] = {'x'};

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetModuleFileNameW(_, _, _, _, _, _))
        .WillOnce(Return(0U)); // [Pre-Assert確認_異常系] - GetModuleFileNameW() が 1 回呼び出されること。
                               // [Pre-Assert手順] - GetModuleFileNameW() から 0 を返す。

    // Act
    int actual_ret = cplat_module_get_path(
        path, sizeof(path),
        self_func_addr()); // [手順] - GetModuleFileNameW() が失敗する状態で cplat_module_get_path() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret);    // [確認_異常系] - cplat_module_get_path() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ('\0', path[0]); // [確認_異常系] - path[0] が '\0' であること。
}

// 中間バッファー確保失敗時に OUT_OF_MEMORY を返すことの確認 (Windows)
TEST_F(moduleTest, get_path_returns_out_of_memory_when_allocation_fails_windows)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    char path[PLATFORM_PATH_UTF8_MAX] = {'x'};

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_malloc(_))
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_malloc() が 1 回呼び出されること。
                                    // [Pre-Assert手順] - cplat_malloc() から nullptr を返す。

    // Act
    int actual_ret = cplat_module_get_path(
        path, sizeof(path),
        self_func_addr()); // [手順] - cplat_malloc() が失敗する状態で cplat_module_get_path() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY,
              actual_ret);    // [確認_異常系] - cplat_module_get_path() の戻り値が CPLAT_ERR_OUT_OF_MEMORY であること。
    EXPECT_EQ('\0', path[0]); // [確認_異常系] - path[0] が '\0' であること。
}

// 内部初期バッファーを超える長いパスからベース名を取得できることの確認 (Windows)
TEST_F(moduleTest, get_basename_handles_path_over_internal_buffer_windows)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    std::wstring wpath = L"\\\\?\\C:\\" + std::wstring(900u, L'a') + L"\\sample_mod.dll";
    char basename[64] = {}; // [状態] - 900 文字超のパスとベース名出力バッファーを用意する。

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetModuleFileNameW(_, _, _, _, _, _))
        .WillRepeatedly(
            [wpath](const char *, int, const char *, HMODULE, LPWSTR filename, DWORD size) -> DWORD
            {
                (void)size;
                std::memcpy(filename, wpath.c_str(), (wpath.size() + 1U) * sizeof(wchar_t));
                return static_cast<DWORD>(wpath.size());
            }); // [Pre-Assert確認_正常系] - GetModuleFileNameW() が呼び出されること。
                // [Pre-Assert手順] - GetModuleFileNameW() から 900 文字超の長いパスを返す。

    // Act
    int actual_ret = cplat_module_get_basename(
        basename, sizeof(basename),
        self_func_addr()); // [手順] - 初期バッファーを超える長いパスからベース名を取得するため cplat_module_get_basename() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_module_get_basename() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("sample_mod", basename); // [確認_正常系] - basename が "sample_mod" であること。
}
#endif /* PLATFORM_ */

// path_out に NULL を渡した場合に拒否されることの確認
TEST_F(moduleTest, get_path_rejects_null_out_path)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret = cplat_module_get_path(
        nullptr, 16u,
        self_func_addr()); // [手順] - path_out に nullptr を指定して cplat_module_get_path() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_module_get_path() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// path_size に 0 を渡した場合に拒否されることの確認
TEST_F(moduleTest, get_path_rejects_zero_size)
{
    // Arrange
    char path[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_module_get_path(
        path, 0u,
        self_func_addr()); // [手順] - path_size に 0 を指定して cplat_module_get_path() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_module_get_path() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// func_addr に NULL を渡した場合に拒否されることの確認
TEST_F(moduleTest, get_path_rejects_null_func_addr)
{
    // Arrange
    char path[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_module_get_path(
        path, sizeof(path), nullptr); // [手順] - func_addr に nullptr を指定して cplat_module_get_path() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_module_get_path() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// 出力バッファーが不足する場合に拒否されることの確認
TEST_F(moduleTest, get_path_returns_buffer_too_small)
{
    // Arrange
    char path[4]; // [状態] - モジュール パスが収まらない 4 byte の出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_module_get_path(
        path, sizeof(path),
        self_func_addr()); // [手順] - 不足するバッファーを指定して cplat_module_get_path() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              actual_ret); // [確認_異常系] - cplat_module_get_path() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
}

#if defined(PLATFORM_LINUX)
// 所属モジュールのパス正規化が失敗した場合に UNKNOWN を返すことの確認
TEST_F(moduleTest, get_path_returns_unknown_when_normalization_fails)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    char path[PLATFORM_PATH_MAX] = {};

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_path_get_full(_, _, _, _))
        .WillOnce(
            [](char *, size_t, cplat_error *detail_out, const char *)
            {
                *detail_out = {CPLAT_ERROR_DOMAIN_ERRNO, CPLAT_ERR_UNKNOWN, EIO};
                return CPLAT_ERR_UNKNOWN;
            }); // [Pre-Assert確認_異常系] - cplat_path_get_full() が 1 回呼び出されること。
                // [Pre-Assert手順] - cplat_path_get_full() から、詳細に EIO を設定して CPLAT_ERR_UNKNOWN を返す。

    // Act
    const int result = cplat_module_get_path(
        path, sizeof(path),
        self_func_addr()); // [手順] - パス正規化が失敗する状態で cplat_module_get_path() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - cplat_module_get_path() の戻り値が CPLAT_ERR_UNKNOWN であること。
}

// 所属モジュールのパスが長過ぎる場合に BUFFER_TOO_SMALL を返すことの確認
TEST_F(moduleTest, get_path_returns_buffer_too_small_when_normalization_reports_long_name)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    char path[PLATFORM_PATH_MAX] = {};

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_path_get_full(_, _, _, _))
        .WillOnce(
            [](char *, size_t, cplat_error *detail_out, const char *)
            {
                *detail_out = {CPLAT_ERROR_DOMAIN_ERRNO, CPLAT_ERR_BUFFER_TOO_SMALL, ENAMETOOLONG};
                return CPLAT_ERR_BUFFER_TOO_SMALL;
            }); // [Pre-Assert確認_異常系] - cplat_path_get_full() が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_path_get_full() から、詳細に ENAMETOOLONG を設定して CPLAT_ERR_BUFFER_TOO_SMALL を返す。

    // Act
    const int result = cplat_module_get_path(
        path, sizeof(path),
        self_func_addr()); // [手順] - 長過ぎるパスのエラーが返る状態で cplat_module_get_path() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              result); // [確認_異常系] - cplat_module_get_path() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
}
#endif /* PLATFORM_LINUX */

// 不正引数時に出力バッファーが空文字列へ初期化されることの確認
TEST_F(moduleTest, get_path_clears_output_for_null_function_address)
{
    // Arrange
    char path[PLATFORM_PATH_MAX];
    std::memset(path, 'X', sizeof(path));

    // Pre-Assert

    // Act
    const int result = cplat_module_get_path(
        path, sizeof(path), nullptr); // [手順] - func_addr に nullptr を指定して cplat_module_get_path() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              result); // [確認_異常系] - cplat_module_get_path() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ('\0',
              path[0]); // [確認_異常系] - path[0] が '\0' であること。(不正引数時に出力先が空文字列であることの確認)
}

/*
 * cplat_module_get_basename
 */

// 所属モジュールの拡張子を除いた名前が取得できることの確認
TEST_F(moduleTest, get_basename_returns_module_name_without_extension)
{
    // Arrange
    char basename[PLATFORM_PATH_MAX] = {0}; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_module_get_basename(
        basename, sizeof(basename),
        self_func_addr()); // [手順] - テスト バイナリ内の関数アドレスを指定して cplat_module_get_basename() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_module_get_basename() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("moduleTest", basename); // [確認_正常系] - basename が "moduleTest" であること。
}

// basename_out に NULL を渡した場合に拒否されることの確認
TEST_F(moduleTest, get_basename_rejects_null_out_basename)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret = cplat_module_get_basename(
        nullptr, 16u,
        self_func_addr()); // [手順] - basename_out に nullptr を指定して cplat_module_get_basename() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret); // [確認_異常系] - cplat_module_get_basename() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// basename_size に 0 を渡した場合に拒否されることの確認
TEST_F(moduleTest, get_basename_rejects_zero_size)
{
    // Arrange
    char basename[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_module_get_basename(
        basename, 0u,
        self_func_addr()); // [手順] - basename_size に 0 を指定して cplat_module_get_basename() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret); // [確認_異常系] - cplat_module_get_basename() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// パス取得に失敗した場合にその結果がそのまま返ることの確認
TEST_F(moduleTest, get_basename_propagates_get_path_failure)
{
    // Arrange
    char basename[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。

    std::memset(basename, 'X', sizeof(basename));

    // Pre-Assert

    // Act
    int actual_ret = cplat_module_get_basename(
        basename, sizeof(basename),
        nullptr); // [手順] - func_addr に nullptr を指定して cplat_module_get_basename() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret); // [確認_異常系] - cplat_module_get_basename() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。(cplat_module_get_path() の結果がそのまま返ることの確認)
    EXPECT_STREQ(
        "",
        basename); // [確認_異常系] - basename が "" であること。(パス取得失敗時に出力先が空文字列へ初期化されることの確認)
}

// 拡張子を除いた名前が収まらない場合に拒否されることの確認
TEST_F(moduleTest, get_basename_returns_buffer_too_small)
{
    // Arrange
    char basename[4]; // [状態] - "moduleTest" が収まらない 4 byte の出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_module_get_basename(
        basename, sizeof(basename),
        self_func_addr()); // [手順] - 不足するバッファーを指定して cplat_module_get_basename() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_BUFFER_TOO_SMALL,
        actual_ret); // [確認_異常系] - cplat_module_get_basename() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_STREQ(
        "",
        basename); // [確認_異常系] - basename が "" であること。(容量不足時に出力先が空文字列へ初期化されることの確認)
}

/*
 * find_shared_lib_extension_cut (inject 経由)
 */

#if defined(PLATFORM_LINUX)

// バージョン付き .so の切り出し位置が返ることの確認
TEST_F(moduleTest, extension_cut_finds_versioned_so)
{
    // Arrange
    const char name[] = "libsample.so.1.2.3"; // [状態] - バージョン付き共有ライブラリ名を用意する。

    // Pre-Assert

    // Act
    const char *cut = test_find_shared_lib_extension_cut(
        name); // [手順] - 切り出し元の名前を指定して test_find_shared_lib_extension_cut() を呼び出す。

    // Assert
    EXPECT_EQ(
        name + 9,
        cut); // [確認_正常系] - test_find_shared_lib_extension_cut() の戻り値が name + 9 と一致すること。(".so.1.2.3" の先頭を指すことの確認)
}

// 末尾が .so の場合の切り出し位置が返ることの確認
TEST_F(moduleTest, extension_cut_finds_plain_so)
{
    // Arrange
    const char name[] = "libsample.so"; // [状態] - 末尾が .so の共有ライブラリ名を用意する。

    // Pre-Assert

    // Act
    const char *cut = test_find_shared_lib_extension_cut(
        name); // [手順] - 切り出し元の名前を指定して test_find_shared_lib_extension_cut() を呼び出す。

    // Assert
    EXPECT_EQ(
        name + 9,
        cut); // [確認_正常系] - test_find_shared_lib_extension_cut() の戻り値が name + 9 と一致すること。(".so" の先頭を指すことの確認)
}

// 末尾が .dylib の場合の切り出し位置が返ることの確認
TEST_F(moduleTest, extension_cut_finds_dylib)
{
    // Arrange
    const char name[] = "libsample.dylib"; // [状態] - 末尾が .dylib の共有ライブラリ名を用意する。

    // Pre-Assert

    // Act
    const char *cut = test_find_shared_lib_extension_cut(
        name); // [手順] - 切り出し元の名前を指定して test_find_shared_lib_extension_cut() を呼び出す。

    // Assert
    EXPECT_EQ(
        name + 9,
        cut); // [確認_正常系] - test_find_shared_lib_extension_cut() の戻り値が name + 9 と一致すること。(".dylib" の先頭を指すことの確認)
}

// 共有ライブラリの拡張子でない場合に NULL が返ることの確認
TEST_F(moduleTest, extension_cut_returns_null_for_other_extension)
{
    // Arrange
    const char name[] = "sample.txt"; // [状態] - 共有ライブラリでない拡張子の名前を用意する。

    // Pre-Assert

    // Act
    const char *cut = test_find_shared_lib_extension_cut(
        name); // [手順] - 切り出し元の名前を指定して test_find_shared_lib_extension_cut() を呼び出す。

    // Assert
    EXPECT_EQ(
        nullptr,
        cut); // [確認_正常系] - test_find_shared_lib_extension_cut() の戻り値が nullptr であること。(汎用の拡張子除去へ委譲することの確認)
}

// 拡張子より短い名前で NULL が返ることの確認
TEST_F(moduleTest, extension_cut_returns_null_for_short_name)
{
    // Arrange
    const char name[] = "ab"; // [状態] - ".so" より短い名前を用意する。

    // Pre-Assert

    // Act
    const char *cut = test_find_shared_lib_extension_cut(
        name); // [手順] - 切り出し元の名前を指定して test_find_shared_lib_extension_cut() を呼び出す。

    // Assert
    EXPECT_EQ(
        nullptr,
        cut); // [確認_正常系] - test_find_shared_lib_extension_cut() の戻り値が nullptr であること。(名前が拡張子より短いことの確認)
}

// 任意パスの共有ライブラリ拡張子と通常拡張子を除去することの確認
TEST_F(moduleTest, basename_core_handles_shared_and_regular_extensions)
{
    // Arrange
    char shared_basename[32] = {};
    char regular_basename[32] = {};

    // Pre-Assert

    // Act
    int shared_result = test_get_basename_from_path(
        shared_basename, sizeof(shared_basename),
        "/usr/lib/libsample.so.1"); // [手順] - バージョン付き共有ライブラリのパスを指定して test_get_basename_from_path() を呼び出す。
    int regular_result = test_get_basename_from_path(
        regular_basename, sizeof(regular_basename),
        "/usr/bin/sample.txt"); // [手順] - 通常拡張子を持つパスを指定して test_get_basename_from_path() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        shared_result); // [確認_正常系] - バージョン付き共有ライブラリ名に対する test_get_basename_from_path() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "libsample",
        shared_basename); // [確認_正常系] - shared_basename が "libsample" であること。(バージョン付き .so が除去されることの確認)
    EXPECT_EQ(
        CPLAT_OK,
        regular_result); // [確認_正常系] - 通常拡張子を持つ名前に対する test_get_basename_from_path() の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(
        "sample",
        regular_basename); // [確認_正常系] - regular_basename が "sample" であること。(通常拡張子が除去されることの確認)
}

// basename 内部処理が空文字列と容量不足を分類することの確認
TEST_F(moduleTest, basename_core_rejects_empty_and_small_outputs)
{
    // Arrange
    char empty_basename[8] = {'x'};
    char shared_basename[4] = {'x'};
    char regular_basename[4] = {'x'};

    // Pre-Assert

    // Act
    int empty_result =
        test_get_basename_from_path(empty_basename, sizeof(empty_basename),
                                    ""); // [手順] - 空のパスを指定して test_get_basename_from_path() を呼び出す。
    int shared_result = test_get_basename_from_path(
        shared_basename, sizeof(shared_basename),
        "libsample.so"); // [手順] - 容量不足の出力先と共有ライブラリ名のパスを指定して test_get_basename_from_path() を呼び出す。
    int regular_result = test_get_basename_from_path(
        regular_basename, sizeof(regular_basename),
        "sample.txt"); // [手順] - 容量不足の出力先と通常拡張子のパスを指定して test_get_basename_from_path() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        empty_result); // [確認_異常系] - 空のパスに対する test_get_basename_from_path() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_STREQ(
        "",
        empty_basename); // [確認_異常系] - empty_basename が "" であること。(空パス時の出力が空文字列であることの確認)
    EXPECT_EQ(
        CPLAT_ERR_BUFFER_TOO_SMALL,
        shared_result); // [確認_異常系] - 容量不足の共有ライブラリ名に対する test_get_basename_from_path() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_STREQ(
        "",
        shared_basename); // [確認_異常系] - shared_basename が "" であること。(共有ライブラリ名の容量不足時に出力が空文字列であることの確認)
    EXPECT_EQ(
        CPLAT_ERR_BUFFER_TOO_SMALL,
        regular_result); // [確認_異常系] - 容量不足の通常拡張子名に対する test_get_basename_from_path() の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_STREQ(
        "",
        regular_basename); // [確認_異常系] - regular_basename が "" であること。(通常拡張子名の容量不足時に出力が空文字列であることの確認)
}

#endif /* PLATFORM_LINUX */
