#include <testfw.h>
#include <mock_cplat.h>
#if defined(PLATFORM_LINUX)
    #include <mock_dlfcn.h>
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
            }); // [Pre-Assert確認_正常系] - dladdr が 1 回呼び出されること。
                // [Pre-Assert手順] - 所属モジュール パスを持つ情報を返却する。
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
            }); // [Pre-Assert確認_正常系] - cplat_path_get_full がモジュール パスを指定して 1 回呼び出されること。
                // [Pre-Assert手順] - 正規化済みパスを書き込み、CPLAT_OK を返却する。

    // Act
    int actual_ret =
        cplat_module_get_path(path, sizeof(path),
                              self_func_addr()); // [手順] - テスト バイナリ内の関数アドレスを指定して呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_module_get_path の戻り値が CPLAT_OK であること。
    EXPECT_STREQ(kModulePath, path); // [確認_正常系] - 所属モジュールの絶対パスが返ること。
}

// dladdr が失敗した場合に UNKNOWN を返すことの確認
TEST_F(moduleTest, get_path_returns_unknown_when_dladdr_fails)
{
    // Arrange
    NiceMock<Mock_dlfcn> mock_dlfcn;
    char path[PLATFORM_PATH_MAX] = {};

    // Pre-Assert
    EXPECT_CALL(mock_dlfcn, dladdr(_, _, _, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - dladdr が 1 回呼び出されること。
                              // [Pre-Assert手順] - dladdr から失敗を返却する。

    // Act
    int actual_ret =
        cplat_module_get_path(path, sizeof(path),
                              self_func_addr()); // [手順] - dladdr の失敗を注入してモジュール パスを取得する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_module_get_path の戻り値が CPLAT_ERR_UNKNOWN であること。
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
                info->dli_fname = NULL;
                return 1;
            }); // [Pre-Assert確認_異常系] - dladdr が 1 回呼び出されること。
                // [Pre-Assert手順] - dli_fname が NULL の情報を返却する。

    // Act
    int actual_ret = cplat_module_get_path(
        path, sizeof(path), self_func_addr()); // [手順] - ファイル名のない dladdr 情報でモジュール パスを取得する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_module_get_path の戻り値が CPLAT_ERR_UNKNOWN であること。
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
            }); // [Pre-Assert確認_異常系] - dladdr が 1 回呼び出されること。
                // [Pre-Assert手順] - 空の dli_fname を返却する。

    // Act
    int actual_ret = cplat_module_get_path(
        path, sizeof(path), self_func_addr()); // [手順] - 空のファイル名を持つ dladdr 情報でモジュール パスを取得する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              actual_ret); // [確認_異常系] - cplat_module_get_path の戻り値が CPLAT_ERR_UNKNOWN であること。
}
#elif defined(PLATFORM_WINDOWS)
// 関数アドレスから所属モジュールの絶対パスが取得できることの確認 (Windows)
TEST_F(moduleTest, get_path_returns_absolute_path_of_owning_module_windows)
{
    // Arrange
    char path[PLATFORM_PATH_MAX] = {0}; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret =
        cplat_module_get_path(path, sizeof(path),
                              self_func_addr()); // [手順] - テスト バイナリ内の関数アドレスを指定して呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_module_get_path の戻り値が CPLAT_OK であること。
    EXPECT_EQ(':', path[1]);         // [確認_正常系] - 絶対パスがドライブ レター形式であること。
    EXPECT_EQ('/', path[2]);         // [確認_正常系] - 絶対パスが '/' 区切りへ正規化されていること。
    EXPECT_NE(nullptr,
              std::strstr(path, "moduleTest")); // [確認_正常系] - パスに所属モジュール名 moduleTest が含まれること。
}
#endif /* PLATFORM_ */

// path_out に NULL を渡した場合に拒否されることの確認
TEST_F(moduleTest, get_path_rejects_null_out_path)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret = cplat_module_get_path(NULL, 16u,
                                           self_func_addr()); // [手順] - path_out に NULL を指定して呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_module_get_path の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// path_size に 0 を渡した場合に拒否されることの確認
TEST_F(moduleTest, get_path_rejects_zero_size)
{
    // Arrange
    char path[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_module_get_path(path, 0u,
                                           self_func_addr()); // [手順] - path_size に 0 を指定して呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_module_get_path の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// func_addr に NULL を渡した場合に拒否されることの確認
TEST_F(moduleTest, get_path_rejects_null_func_addr)
{
    // Arrange
    char path[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_module_get_path(path, sizeof(path), NULL); // [手順] - func_addr に NULL を指定して呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              actual_ret); // [確認_異常系] - cplat_module_get_path の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// 出力バッファーが不足する場合に拒否されることの確認
TEST_F(moduleTest, get_path_returns_buffer_too_small)
{
    // Arrange
    char path[4]; // [状態] - モジュール パスが収まらない 4 byte の出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_module_get_path(path, sizeof(path),
                                           self_func_addr()); // [手順] - 不足するバッファーを指定して呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              actual_ret); // [確認_異常系] - cplat_module_get_path の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
}

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
            }); // [Pre-Assert確認_異常系] - cplat_path_get_full が 1 回呼び出されること。
                // [Pre-Assert手順] - 詳細に EIO を設定し、CPLAT_ERR_UNKNOWN を返却する。

    // Act
    const int result =
        cplat_module_get_path(path, sizeof(path), self_func_addr()); // [手順] - パス正規化失敗を注入して呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - cplat_module_get_path の戻り値が CPLAT_ERR_UNKNOWN であること。
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
            }); // [Pre-Assert確認_異常系] - cplat_path_get_full が 1 回呼び出されること。
                // [Pre-Assert手順] - 詳細に ENAMETOOLONG を設定し、CPLAT_ERR_BUFFER_TOO_SMALL を返却する。

    // Act
    const int result = cplat_module_get_path(path, sizeof(path),
                                             self_func_addr()); // [手順] - 長過ぎるパスのエラーを注入して呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              result); // [確認_異常系] - cplat_module_get_path の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
}

// 不正引数時に出力バッファーが空文字列へ初期化されることの確認
TEST_F(moduleTest, get_path_clears_output_for_null_function_address)
{
    // Arrange
    char path[PLATFORM_PATH_MAX];
    std::memset(path, 'X', sizeof(path));

    // Pre-Assert

    // Act
    const int result =
        cplat_module_get_path(path, sizeof(path), NULL); // [手順] - func_addr に NULL を指定して呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT,
              result); // [確認_異常系] - cplat_module_get_path の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ('\0', path[0]); // [確認_異常系] - 出力バッファーの先頭が空文字列になること。
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
        basename, sizeof(basename), self_func_addr()); // [手順] - テスト バイナリ内の関数アドレスを指定して呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret);      // [確認_正常系] - cplat_module_get_basename の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("moduleTest", basename); // [確認_正常系] - 拡張子を持たないモジュール名 "moduleTest" が返ること。
}

// basename_out に NULL を渡した場合に拒否されることの確認
TEST_F(moduleTest, get_basename_rejects_null_out_basename)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret = cplat_module_get_basename(NULL, 16u,
                                               self_func_addr()); // [手順] - basename_out に NULL を指定して呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret); // [確認_異常系] - cplat_module_get_basename の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// basename_size に 0 を渡した場合に拒否されることの確認
TEST_F(moduleTest, get_basename_rejects_zero_size)
{
    // Arrange
    char basename[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_module_get_basename(basename, 0u,
                                               self_func_addr()); // [手順] - basename_size に 0 を指定して呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret); // [確認_異常系] - cplat_module_get_basename の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// パス取得に失敗した場合にその結果がそのまま返ることの確認
TEST_F(moduleTest, get_basename_propagates_get_path_failure)
{
    // Arrange
    char basename[PLATFORM_PATH_MAX]; // [状態] - 出力バッファーを用意する。

    std::memset(basename, 'X', sizeof(basename));

    // Pre-Assert

    // Act
    int actual_ret = cplat_module_get_basename(basename, sizeof(basename),
                                               NULL); // [手順] - func_addr に NULL を指定して呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret); // [確認_異常系] - cplat_module_get_path が返した CPLAT_ERR_INVALID_ARGUMENT がそのまま返ること。
    EXPECT_STREQ("", basename); // [確認_異常系] - 出力バッファーが空文字列に初期化されること。
}

// 拡張子を除いた名前が収まらない場合に拒否されることの確認
TEST_F(moduleTest, get_basename_returns_buffer_too_small)
{
    // Arrange
    char basename[4]; // [状態] - "moduleTest" が収まらない 4 byte の出力バッファーを用意する。

    // Pre-Assert

    // Act
    int actual_ret = cplat_module_get_basename(basename, sizeof(basename),
                                               self_func_addr()); // [手順] - 不足するバッファーを指定して呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_BUFFER_TOO_SMALL,
        actual_ret); // [確認_異常系] - cplat_module_get_basename の戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_STREQ("", basename); // [確認_異常系] - 出力バッファーが空文字列に初期化されること。
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
    const char *cut = test_find_shared_lib_extension_cut(name); // [手順] - 切り出し位置を取得する。

    // Assert
    EXPECT_EQ(name + 9, cut); // [確認_正常系] - ".so.1.2.3" の先頭を指すポインターが返ること。
}

// 末尾が .so の場合の切り出し位置が返ることの確認
TEST_F(moduleTest, extension_cut_finds_plain_so)
{
    // Arrange
    const char name[] = "libsample.so"; // [状態] - 末尾が .so の共有ライブラリ名を用意する。

    // Pre-Assert

    // Act
    const char *cut = test_find_shared_lib_extension_cut(name); // [手順] - 切り出し位置を取得する。

    // Assert
    EXPECT_EQ(name + 9, cut); // [確認_正常系] - ".so" の先頭を指すポインターが返ること。
}

// 末尾が .dylib の場合の切り出し位置が返ることの確認
TEST_F(moduleTest, extension_cut_finds_dylib)
{
    // Arrange
    const char name[] = "libsample.dylib"; // [状態] - 末尾が .dylib の共有ライブラリ名を用意する。

    // Pre-Assert

    // Act
    const char *cut = test_find_shared_lib_extension_cut(name); // [手順] - 切り出し位置を取得する。

    // Assert
    EXPECT_EQ(name + 9, cut); // [確認_正常系] - ".dylib" の先頭を指すポインターが返ること。
}

// 共有ライブラリの拡張子でない場合に NULL が返ることの確認
TEST_F(moduleTest, extension_cut_returns_null_for_other_extension)
{
    // Arrange
    const char name[] = "sample.txt"; // [状態] - 共有ライブラリでない拡張子の名前を用意する。

    // Pre-Assert

    // Act
    const char *cut = test_find_shared_lib_extension_cut(name); // [手順] - 切り出し位置を取得する。

    // Assert
    EXPECT_EQ(nullptr, cut); // [確認_正常系] - 汎用の拡張子除去へ委譲するため NULL が返ること。
}

// 拡張子より短い名前で NULL が返ることの確認
TEST_F(moduleTest, extension_cut_returns_null_for_short_name)
{
    // Arrange
    const char name[] = "ab"; // [状態] - ".so" より短い名前を用意する。

    // Pre-Assert

    // Act
    const char *cut = test_find_shared_lib_extension_cut(name); // [手順] - 切り出し位置を取得する。

    // Assert
    EXPECT_EQ(nullptr, cut); // [確認_正常系] - 比較対象に満たないため NULL が返ること。
}

// 任意パスの共有ライブラリ拡張子と通常拡張子を除去することの確認
TEST_F(moduleTest, basename_core_handles_shared_and_regular_extensions)
{
    // Arrange
    char shared_basename[32] = {};
    char regular_basename[32] = {};

    // Pre-Assert

    // Act
    int shared_result =
        test_get_basename_from_path(shared_basename, sizeof(shared_basename),
                                    "/usr/lib/libsample.so.1"); // [手順] - バージョン付き共有ライブラリ名を取得する。
    int regular_result = test_get_basename_from_path(
        regular_basename, sizeof(regular_basename), "/usr/bin/sample.txt"); // [手順] - 通常拡張子を持つ名前を取得する。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              shared_result);                   // [確認_正常系] - 共有ライブラリ名取得の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("libsample", shared_basename); // [確認_正常系] - バージョン付き .so が除去されること。
    EXPECT_EQ(CPLAT_OK,
              regular_result);                // [確認_正常系] - 通常拡張子名取得の戻り値が CPLAT_OK であること。
    EXPECT_STREQ("sample", regular_basename); // [確認_正常系] - 通常拡張子が除去されること。
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
    int empty_result = test_get_basename_from_path(empty_basename, sizeof(empty_basename),
                                                   ""); // [手順] - 空のパスから basename を取得する。
    int shared_result =
        test_get_basename_from_path(shared_basename, sizeof(shared_basename),
                                    "libsample.so"); // [手順] - 容量不足の出力先へ共有ライブラリ名を取得する。
    int regular_result =
        test_get_basename_from_path(regular_basename, sizeof(regular_basename),
                                    "sample.txt"); // [手順] - 容量不足の出力先へ通常拡張子名を取得する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              empty_result);          // [確認_異常系] - 空パスの戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_STREQ("", empty_basename); // [確認_異常系] - 空パス時の出力が空文字列であること。
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              shared_result);          // [確認_異常系] - 共有ライブラリ名の容量不足が BUFFER_TOO_SMALL であること。
    EXPECT_STREQ("", shared_basename); // [確認_異常系] - 共有ライブラリ名の容量不足時に出力が空であること。
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL,
              regular_result);          // [確認_異常系] - 通常拡張子名の容量不足が BUFFER_TOO_SMALL であること。
    EXPECT_STREQ("", regular_basename); // [確認_異常系] - 通常拡張子名の容量不足時に出力が空であること。
}

#endif /* PLATFORM_LINUX */
