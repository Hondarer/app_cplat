#include <cplat/base/platform.h>

#include <testfw.h>
#include <mock_cplat.h>
#include <cplat/base/error.h>
#include <cplat/base/result.h>
#include <cplat/crt/stdlib.h>

#if defined(PLATFORM_WINDOWS)
    #include <cplat/crt/temp_dir_internal.h>
    #include <cplat/crt/temp_file_internal.h>
    #include <mock_windows.h>

    #include <cerrno>
    #include <cstdint>
    #include <cstring>
    #include <string>

using testing::_;
using testing::NiceMock;
using testing::Return;

namespace
{

const HANDLE kFakeHandle = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x1234));

// 一時ファイルの名前の長さ (接頭辞 3 文字、乱数の 16 進表記 16 文字、".tmp" の 4 文字) [wchar_t]。
const size_t kTempNameLength = 3U + 16U + 4U;

// 一時ディレクトリの取得の差し替えで返すパス。テストごとに設定する。
std::wstring g_temp_dir;

DWORD WINAPI temp_file_resolver_failing(DWORD, LPWSTR)
{
    SetLastError(ERROR_ACCESS_DENIED);
    return 0U;
}

DWORD WINAPI temp_file_resolver_from_global(DWORD buffer_length, LPWSTR buffer)
{
    const DWORD len = static_cast<DWORD>(g_temp_dir.size());
    if (buffer != nullptr && buffer_length > len)
    {
        std::memcpy(buffer, g_temp_dir.c_str(), (g_temp_dir.size() + 1U) * sizeof(wchar_t));
    }
    return len;
}

// 呼び出しの回数 n (0 から) に応じて、各バイトを n * 0x10 + (位置 + 1) とする乱数を書き込む。
// 1 回目は "0102030405060708"、2 回目は "1112131415161718" の 16 進表記になる。
int fill_sequential_random(void *dest, size_t dest_size, int *call_count)
{
    uint8_t *bytes = static_cast<uint8_t *>(dest);
    for (size_t i = 0; i < dest_size; ++i)
    {
        bytes[i] = static_cast<uint8_t>((*call_count * 0x10) + static_cast<int>(i) + 1);
    }
    ++*call_count;
    return CPLAT_OK;
}

// 区切りを含む、長さ total_length [wchar_t] のドライブの一時ディレクトリ ("C:\aaa...\") を返す。
std::wstring make_drive_dir(size_t total_length)
{
    return L"C:\\" + std::wstring(total_length - 4U, L'a') + L"\\";
}

// 作成したパスを取り出して解放する。path が NULL の場合は空を返す。
std::wstring take_path(wchar_t *path)
{
    std::wstring result = path != nullptr ? std::wstring(path) : std::wstring();
    cplat_free(path);
    return result;
}

// [サブ手順 名前=tempFileTest.setup_create_success]
// 乱数と一時ファイルの作成を、実ファイルを作らずに成功させる。
void setup_create_success(Mock_cplat &mock_cplat, Mock_windows &mock_windows, int *random_calls)
{
    ON_CALL(mock_cplat, cplat_random_bytes(_, _))
        .WillByDefault([random_calls](void *dest, size_t dest_size)
                       { return fill_sequential_random(dest, dest_size, random_calls); });
    // [状態] - cplat_random_bytes() から、呼び出しの回数に応じた既知のバイト列を書き込んで CPLAT_OK を返すように既定動作を設定する。
    ON_CALL(mock_windows, CreateFileW(_, _, _, _, _, _, _, _, _, _)).WillByDefault(Return(kFakeHandle));
    // [状態] - CreateFileW() から偽のハンドルを返すように既定動作を設定する。
    ON_CALL(mock_windows, CloseHandle(_, _, _, kFakeHandle)).WillByDefault(Return(TRUE));
    // [状態] - 偽のハンドルに対する CloseHandle() から TRUE を返すように既定動作を設定する。
}
// [サブ手順終了]

} // namespace

// path_out が NULL の場合に CPLAT_ERR_INVALID_ARGUMENT を返すことの確認
TEST(tempFileTest, null_path_out_returns_invalid_argument)
{
    // Arrange
    cplat_error detail;

    // Pre-Assert

    // Act
    int rc = cplat_internal_create_temp_file_w(
        L"ptr", nullptr,
        &detail); // [手順] - path_out に nullptr を渡して cplat_internal_create_temp_file_w() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, rc); // [確認_異常系] - 戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(EINVAL, cplat_error_get_errno(&detail)); // [確認_異常系] - detail の errno が EINVAL であること。
}

// 一時ディレクトリの取得が失敗した場合に、一時ファイルを作成せずにその失敗を返すことの確認
TEST(tempFileTest, temp_dir_failure_skips_create_file)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    wchar_t *path =
        reinterpret_cast<wchar_t *>(static_cast<uintptr_t>(0x1)); // [状態] - path を非 NULL の値で初期化する。
    cplat_error detail;
    cplat_internal_temp_dir_set_resolver_for_test(
        temp_file_resolver_failing); // [状態] - 一時ディレクトリの取得が ERROR_ACCESS_DENIED で失敗するように差し替える。

    // Pre-Assert
    EXPECT_CALL(mock_windows, CreateFileW(_, _, _, _, _, _, _, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - CreateFileW() が呼び出されないこと。

    // Act
    int rc = cplat_internal_create_temp_file_w(
        L"ptr", &path,
        &detail); // [手順] - 一時ディレクトリの取得が失敗する状態で cplat_internal_create_temp_file_w() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_PERMISSION_DENIED, rc); // [確認_異常系] - 戻り値が CPLAT_ERR_PERMISSION_DENIED であること。
    EXPECT_EQ(nullptr, path);                   // [確認_異常系] - path が nullptr であること。
    EXPECT_EQ((unsigned long)ERROR_ACCESS_DENIED,
              cplat_error_get_windows_error(
                  &detail)); // [確認_異常系] - detail の Win32 エラーが ERROR_ACCESS_DENIED であること。

    // Cleanup
    cplat_internal_temp_dir_set_resolver_for_test(nullptr);
}

// 一時ディレクトリの直下に、接頭辞と乱数の名前で CREATE_NEW により空のファイルを作成して閉じることの確認
// [サブ手順参照 名前=tempFileTest.setup_create_success]
TEST(tempFileTest, creates_file_with_create_new_under_temp_dir)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    wchar_t *path = nullptr;
    int random_calls = 0;
    setup_create_success(mock_cplat, mock_windows, &random_calls);
    g_temp_dir = L"C:\\Windows\\SystemTemp\\"; // [状態] - 一時ディレクトリを既定の SystemTemp とする。
    cplat_internal_temp_dir_set_resolver_for_test(temp_file_resolver_from_global);

    // Pre-Assert
    EXPECT_CALL(mock_windows, CreateFileW(_, _, _, testing::StrEq(L"C:\\Windows\\SystemTemp\\ptr0102030405060708.tmp"),
                                          GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                          nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr))
        .Times(
            1); // [Pre-Assert確認_正常系] - CreateFileW() が一時ディレクトリ直下のパス、GENERIC_WRITE、すべての共有、CREATE_NEW を指定して 1 回呼び出されること。
    EXPECT_CALL(mock_windows, CloseHandle(_, _, _, kFakeHandle))
        .Times(1); // [Pre-Assert確認_正常系] - CloseHandle() が作成したハンドルを指定して 1 回呼び出されること。

    // Act
    int rc = cplat_internal_create_temp_file_w(
        L"ptr", &path, nullptr); // [手順] - 接頭辞 "ptr" で cplat_internal_create_temp_file_w() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, rc); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_EQ(std::wstring(L"C:\\Windows\\SystemTemp\\ptr0102030405060708.tmp"),
              take_path(path)); // [確認_正常系] - path が "C:\Windows\SystemTemp\ptr0102030405060708.tmp" であること。

    // Cleanup
    cplat_internal_temp_dir_set_resolver_for_test(nullptr);
}

// 区切りの '/' を '\' へそろえ、末尾に区切りがない一時ディレクトリに区切りを補うことの確認
// [サブ手順参照 名前=tempFileTest.setup_create_success]
TEST(tempFileTest, normalizes_separators_of_temp_dir)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    wchar_t *path = nullptr;
    int random_calls = 0;
    setup_create_success(mock_cplat, mock_windows, &random_calls);
    g_temp_dir = L"C:/Short"; // [状態] - 一時ディレクトリを、'/' の区切りで末尾に区切りのない "C:/Short" とする。
    cplat_internal_temp_dir_set_resolver_for_test(temp_file_resolver_from_global);

    // Pre-Assert

    // Act
    int rc = cplat_internal_create_temp_file_w(
        L"ptr", &path, nullptr); // [手順] - 接頭辞 "ptr" で cplat_internal_create_temp_file_w() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, rc); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_EQ(std::wstring(L"C:\\Short\\ptr0102030405060708.tmp"),
              take_path(path)); // [確認_正常系] - path が "C:\Short\ptr0102030405060708.tmp" であること。

    // Cleanup
    cplat_internal_temp_dir_set_resolver_for_test(nullptr);
}

// 接頭辞が 4 文字以上の場合に、先頭 3 文字だけを名前に使うことの確認
// [サブ手順参照 名前=tempFileTest.setup_create_success]
TEST(tempFileTest, truncates_prefix_to_three_chars)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    wchar_t *path = nullptr;
    int random_calls = 0;
    setup_create_success(mock_cplat, mock_windows, &random_calls);
    g_temp_dir = L"C:\\Short\\"; // [状態] - 一時ディレクトリを "C:\Short\" とする。
    cplat_internal_temp_dir_set_resolver_for_test(temp_file_resolver_from_global);

    // Pre-Assert

    // Act
    int rc = cplat_internal_create_temp_file_w(
        L"abcdef", &path, nullptr); // [手順] - 接頭辞 "abcdef" で cplat_internal_create_temp_file_w() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, rc); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_EQ(std::wstring(L"C:\\Short\\abc0102030405060708.tmp"),
              take_path(path)); // [確認_正常系] - path が "C:\Short\abc0102030405060708.tmp" であること。

    // Cleanup
    cplat_internal_temp_dir_set_resolver_for_test(nullptr);
}

// 接頭辞が NULL の場合に、接頭辞のない名前を使うことの確認
// [サブ手順参照 名前=tempFileTest.setup_create_success]
TEST(tempFileTest, null_prefix_uses_name_without_prefix)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    wchar_t *path = nullptr;
    int random_calls = 0;
    setup_create_success(mock_cplat, mock_windows, &random_calls);
    g_temp_dir = L"C:\\Short\\"; // [状態] - 一時ディレクトリを "C:\Short\" とする。
    cplat_internal_temp_dir_set_resolver_for_test(temp_file_resolver_from_global);

    // Pre-Assert

    // Act
    int rc = cplat_internal_create_temp_file_w(
        nullptr, &path, nullptr); // [手順] - 接頭辞に nullptr を渡して cplat_internal_create_temp_file_w() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, rc); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_EQ(std::wstring(L"C:\\Short\\0102030405060708.tmp"),
              take_path(path)); // [確認_正常系] - path が "C:\Short\0102030405060708.tmp" であること。

    // Cleanup
    cplat_internal_temp_dir_set_resolver_for_test(nullptr);
}

// 完全なパスが MAX_PATH 未満の場合は、拡張パスの接頭辞を付けないことの確認
// [サブ手順参照 名前=tempFileTest.setup_create_success]
TEST(tempFileTest, path_shorter_than_max_path_has_no_extended_prefix)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    wchar_t *path = nullptr;
    int random_calls = 0;
    setup_create_success(mock_cplat, mock_windows, &random_calls);
    g_temp_dir = make_drive_dir(MAX_PATH - 1U - kTempNameLength);
    // [状態] - 完全なパスが MAX_PATH - 1 文字になるドライブの一時ディレクトリを用意する。
    cplat_internal_temp_dir_set_resolver_for_test(temp_file_resolver_from_global);
    const std::wstring expected = g_temp_dir + L"ptr0102030405060708.tmp";

    // Pre-Assert

    // Act
    int rc = cplat_internal_create_temp_file_w(
        L"ptr", &path, nullptr); // [手順] - 接頭辞 "ptr" で cplat_internal_create_temp_file_w() を呼び出す。
    const std::wstring created = take_path(path);

    // Assert
    EXPECT_EQ(CPLAT_OK, rc);      // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_EQ(expected, created); // [確認_正常系] - path が接頭辞のない一時ディレクトリ配下のパスであること。
    EXPECT_EQ(static_cast<size_t>(MAX_PATH - 1U),
              created.size()); // [確認_正常系] - path の長さが MAX_PATH - 1 であること。

    // Cleanup
    cplat_internal_temp_dir_set_resolver_for_test(nullptr);
}

// 完全なパスがちょうど MAX_PATH の場合に、ドライブのパスへ \\?\ を前置することの確認
// [サブ手順参照 名前=tempFileTest.setup_create_success]
TEST(tempFileTest, long_drive_path_has_extended_prefix)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    wchar_t *path = nullptr;
    int random_calls = 0;
    setup_create_success(mock_cplat, mock_windows, &random_calls);
    g_temp_dir = make_drive_dir(MAX_PATH - kTempNameLength);
    // [状態] - 完全なパスがちょうど MAX_PATH 文字になるドライブの一時ディレクトリを用意する。
    cplat_internal_temp_dir_set_resolver_for_test(temp_file_resolver_from_global);
    const std::wstring expected = L"\\\\?\\" + g_temp_dir + L"ptr0102030405060708.tmp";

    // Pre-Assert
    EXPECT_CALL(mock_windows, CreateFileW(_, _, _, testing::StrEq(expected.c_str()), _, _, _, CREATE_NEW, _, _))
        .Times(1); // [Pre-Assert確認_正常系] - CreateFileW() が \\?\ を前置したパスを指定して 1 回呼び出されること。

    // Act
    int rc = cplat_internal_create_temp_file_w(
        L"ptr", &path, nullptr); // [手順] - 接頭辞 "ptr" で cplat_internal_create_temp_file_w() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, rc);              // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_EQ(expected, take_path(path)); // [確認_正常系] - path が \\?\ を前置したパスであること。

    // Cleanup
    cplat_internal_temp_dir_set_resolver_for_test(nullptr);
}

// 完全なパスが MAX_PATH 以上の UNC のパスを \\?\UNC\ の形にすることの確認
// [サブ手順参照 名前=tempFileTest.setup_create_success]
TEST(tempFileTest, long_unc_path_has_extended_unc_prefix)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    wchar_t *path = nullptr;
    int random_calls = 0;
    setup_create_success(mock_cplat, mock_windows, &random_calls);
    const std::wstring share_part = L"server\\share\\" + std::wstring(240U, L'u') + L"\\";
    g_temp_dir =
        L"\\\\" + share_part; // [状態] - 完全なパスが MAX_PATH 文字以上になる UNC の一時ディレクトリを用意する。
    cplat_internal_temp_dir_set_resolver_for_test(temp_file_resolver_from_global);
    const std::wstring expected = L"\\\\?\\UNC\\" + share_part + L"ptr0102030405060708.tmp";

    // Pre-Assert

    // Act
    int rc = cplat_internal_create_temp_file_w(
        L"ptr", &path, nullptr); // [手順] - 接頭辞 "ptr" で cplat_internal_create_temp_file_w() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, rc);              // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_EQ(expected, take_path(path)); // [確認_正常系] - path が \\?\UNC\server\share\... の形であること。

    // Cleanup
    cplat_internal_temp_dir_set_resolver_for_test(nullptr);
}

// すでに \\?\ で始まる長い一時ディレクトリに、接頭辞を重ねないことの確認
// [サブ手順参照 名前=tempFileTest.setup_create_success]
TEST(tempFileTest, already_extended_temp_dir_is_not_prefixed_again)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    wchar_t *path = nullptr;
    int random_calls = 0;
    setup_create_success(mock_cplat, mock_windows, &random_calls);
    g_temp_dir = L"\\\\?\\" + make_drive_dir(MAX_PATH);
    // [状態] - \\?\ で始まり、完全なパスが MAX_PATH 文字以上になる一時ディレクトリを用意する。
    cplat_internal_temp_dir_set_resolver_for_test(temp_file_resolver_from_global);
    const std::wstring expected = g_temp_dir + L"ptr0102030405060708.tmp";

    // Pre-Assert

    // Act
    int rc = cplat_internal_create_temp_file_w(
        L"ptr", &path, nullptr); // [手順] - 接頭辞 "ptr" で cplat_internal_create_temp_file_w() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, rc);              // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_EQ(expected, take_path(path)); // [確認_正常系] - path が一時ディレクトリの接頭辞を 1 つだけ持つこと。

    // Cleanup
    cplat_internal_temp_dir_set_resolver_for_test(nullptr);
}

// 名前が衝突した場合に、別の名前で作成をやり直すことの確認
// [サブ手順参照 名前=tempFileTest.setup_create_success]
TEST(tempFileTest, retries_with_another_name_on_collision)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    wchar_t *path = nullptr;
    int random_calls = 0;
    setup_create_success(mock_cplat, mock_windows, &random_calls);
    g_temp_dir = L"C:\\Short\\"; // [状態] - 一時ディレクトリを "C:\Short\" とする。
    cplat_internal_temp_dir_set_resolver_for_test(temp_file_resolver_from_global);

    // Pre-Assert
    EXPECT_CALL(mock_windows,
                CreateFileW(_, _, _, testing::StrEq(L"C:\\Short\\ptr0102030405060708.tmp"), _, _, _, CREATE_NEW, _, _))
        .WillOnce(Return(
            INVALID_HANDLE_VALUE)); // [Pre-Assert確認_異常系] - 1 つ目の名前で CreateFileW() が 1 回呼び出されること。
                                    // [Pre-Assert手順] - CreateFileW() から INVALID_HANDLE_VALUE を返す。
    EXPECT_CALL(mock_windows, GetLastError(_, _, _))
        .WillOnce(Return(ERROR_FILE_EXISTS)); // [Pre-Assert確認_異常系] - GetLastError() が 1 回呼び出されること。
                                              // [Pre-Assert手順] - GetLastError() から ERROR_FILE_EXISTS を返す。
    EXPECT_CALL(mock_windows,
                CreateFileW(_, _, _, testing::StrEq(L"C:\\Short\\ptr1112131415161718.tmp"), _, _, _, CREATE_NEW, _, _))
        .WillOnce(
            Return(kFakeHandle)); // [Pre-Assert確認_正常系] - 2 つ目の名前で CreateFileW() が 1 回呼び出されること。
                                  // [Pre-Assert手順] - CreateFileW() から偽のハンドルを返す。

    // Act
    int rc = cplat_internal_create_temp_file_w(
        L"ptr", &path,
        nullptr); // [手順] - 1 つ目の名前が衝突する状態で cplat_internal_create_temp_file_w() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, rc);    // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_EQ(2, random_calls); // [確認_正常系] - 乱数が 2 回生成されたこと。
    EXPECT_EQ(std::wstring(L"C:\\Short\\ptr1112131415161718.tmp"),
              take_path(path)); // [確認_正常系] - path が 2 つ目の名前のパスであること。

    // Cleanup
    cplat_internal_temp_dir_set_resolver_for_test(nullptr);
}

// 名前の衝突が 16 回続いた場合に、作成を打ち切って失敗を返すことの確認
// [サブ手順参照 名前=tempFileTest.setup_create_success]
TEST(tempFileTest, gives_up_after_sixteen_collisions)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    wchar_t *path = nullptr;
    cplat_error detail;
    int random_calls = 0;
    setup_create_success(mock_cplat, mock_windows, &random_calls);
    g_temp_dir = L"C:\\Short\\"; // [状態] - 一時ディレクトリを "C:\Short\" とする。
    cplat_internal_temp_dir_set_resolver_for_test(temp_file_resolver_from_global);

    // Pre-Assert
    EXPECT_CALL(mock_windows, CreateFileW(_, _, _, _, _, _, _, CREATE_NEW, _, _))
        .Times(16)
        .WillRepeatedly(
            Return(INVALID_HANDLE_VALUE)); // [Pre-Assert確認_異常系] - CreateFileW() が 16 回呼び出されること。
                                           // [Pre-Assert手順] - CreateFileW() から INVALID_HANDLE_VALUE を返す。
    EXPECT_CALL(mock_windows, GetLastError(_, _, _))
        .Times(16)
        .WillRepeatedly(
            Return(ERROR_ALREADY_EXISTS)); // [Pre-Assert確認_異常系] - GetLastError() が 16 回呼び出されること。
                                           // [Pre-Assert手順] - GetLastError() から ERROR_ALREADY_EXISTS を返す。

    // Act
    int rc = cplat_internal_create_temp_file_w(
        L"ptr", &path, &detail); // [手順] - 名前の衝突が続く状態で cplat_internal_create_temp_file_w() を呼び出す。

    // Assert
    EXPECT_NE(CPLAT_OK, rc);     // [確認_異常系] - 戻り値が CPLAT_OK 以外であること。
    EXPECT_EQ(nullptr, path);    // [確認_異常系] - path が nullptr であること。
    EXPECT_EQ(16, random_calls); // [確認_異常系] - 乱数が 16 回生成されたこと。
    EXPECT_EQ((unsigned long)ERROR_ALREADY_EXISTS,
              cplat_error_get_windows_error(
                  &detail)); // [確認_異常系] - detail の Win32 エラーが ERROR_ALREADY_EXISTS であること。

    // Cleanup
    cplat_internal_temp_dir_set_resolver_for_test(nullptr);
}

// 衝突以外の理由で作成に失敗した場合は、やり直さずに失敗を返すことの確認
// [サブ手順参照 名前=tempFileTest.setup_create_success]
TEST(tempFileTest, does_not_retry_on_other_create_failure)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    wchar_t *path = nullptr;
    cplat_error detail;
    int random_calls = 0;
    setup_create_success(mock_cplat, mock_windows, &random_calls);
    g_temp_dir = L"C:\\Short\\"; // [状態] - 一時ディレクトリを "C:\Short\" とする。
    cplat_internal_temp_dir_set_resolver_for_test(temp_file_resolver_from_global);

    // Pre-Assert
    EXPECT_CALL(mock_windows, CreateFileW(_, _, _, _, _, _, _, CREATE_NEW, _, _))
        .WillOnce(Return(INVALID_HANDLE_VALUE)); // [Pre-Assert確認_異常系] - CreateFileW() が 1 回呼び出されること。
                                                 // [Pre-Assert手順] - CreateFileW() から INVALID_HANDLE_VALUE を返す。
    EXPECT_CALL(mock_windows, GetLastError(_, _, _))
        .WillOnce(Return(ERROR_ACCESS_DENIED)); // [Pre-Assert確認_異常系] - GetLastError() が 1 回呼び出されること。
                                                // [Pre-Assert手順] - GetLastError() から ERROR_ACCESS_DENIED を返す。

    // Act
    int rc = cplat_internal_create_temp_file_w(
        L"ptr", &path,
        &detail); // [手順] - 作成がアクセス拒否で失敗する状態で cplat_internal_create_temp_file_w() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_PERMISSION_DENIED, rc); // [確認_異常系] - 戻り値が CPLAT_ERR_PERMISSION_DENIED であること。
    EXPECT_EQ(nullptr, path);                   // [確認_異常系] - path が nullptr であること。
    EXPECT_EQ(1, random_calls);                 // [確認_異常系] - 乱数が 1 回だけ生成されたこと。
    EXPECT_EQ((unsigned long)ERROR_ACCESS_DENIED,
              cplat_error_get_windows_error(
                  &detail)); // [確認_異常系] - detail の Win32 エラーが ERROR_ACCESS_DENIED であること。

    // Cleanup
    cplat_internal_temp_dir_set_resolver_for_test(nullptr);
}

// 乱数を生成できない場合に、一時ファイルを作成せずにその結果コードを返すことの確認
// [サブ手順参照 名前=tempFileTest.setup_create_success]
TEST(tempFileTest, random_failure_skips_create_file)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_windows> mock_windows;
    wchar_t *path = nullptr;
    cplat_error detail;
    int random_calls = 0;
    setup_create_success(mock_cplat, mock_windows, &random_calls);
    g_temp_dir = L"C:\\Short\\"; // [状態] - 一時ディレクトリを "C:\Short\" とする。
    cplat_internal_temp_dir_set_resolver_for_test(temp_file_resolver_from_global);

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_random_bytes(_, _))
        .WillOnce(
            Return(CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_異常系] - cplat_random_bytes() が 1 回呼び出されること。
                                        // [Pre-Assert手順] - cplat_random_bytes() から CPLAT_ERR_UNKNOWN を返す。
    EXPECT_CALL(mock_windows, CreateFileW(_, _, _, _, _, _, _, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - CreateFileW() が呼び出されないこと。

    // Act
    int rc = cplat_internal_create_temp_file_w(
        L"ptr", &path, &detail); // [手順] - 乱数を生成できない状態で cplat_internal_create_temp_file_w() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, rc); // [確認_異常系] - 戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(nullptr, path);         // [確認_異常系] - path が nullptr であること。
    EXPECT_EQ(
        1,
        cplat_error_is_set(
            &detail)); // [確認_異常系] - cplat_error_is_set() の戻り値が 1 であること。(detail に詳細が記録されたことの確認)

    // Cleanup
    cplat_internal_temp_dir_set_resolver_for_test(nullptr);
}

#else /* PLATFORM_LINUX */

TEST(tempFileTest, dummy_on_linux)
{
    // Arrange

    // Pre-Assert

    // Act

    // Assert
    SUCCEED(); // [確認_正常系] - Linux 環境では Windows 専用テストを実行せず成功とすること。
}

#endif /* PLATFORM_ */
