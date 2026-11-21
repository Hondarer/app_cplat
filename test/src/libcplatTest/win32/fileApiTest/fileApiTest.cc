#include <testfw.h>
#include <cplat/base/platform.h>
#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */
#include <cplat/win32/win32.h>
#ifdef __cplusplus
}
#endif /* __cplusplus */
#include <mock_cplat.h>
#include <mock_windows.h>

#include <cstring>
#include <string>

#if defined(PLATFORM_WINDOWS)

using testing::_;
using testing::DoAll;
using testing::NiceMock;
using testing::Return;
using testing::SetArgPointee;

class fileApiTest : public Test
{
  protected:
    void SetUp() override
    {
        SetLastError(ERROR_SUCCESS);
    }
};

// 途中に NUL を含む UTF-8 バイト列で、NUL 以降の文字も WriteConsoleW へ渡ることの確認
TEST_F(fileApiTest, write_console_passes_characters_after_embedded_nul)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    const char input[] = "ab\0cd";
    DWORD written = 0U;
    std::wstring captured;
    HANDLE console_handle = reinterpret_cast<HANDLE>(1);

    // Pre-Assert
    EXPECT_CALL(mock_windows, WriteConsoleW(_, _, _, console_handle, _, 5U, _, nullptr))
        .WillOnce(
            [&captured](const char *, int, const char *, HANDLE, const VOID *buffer, DWORD chars_to_write,
                        LPDWORD chars_written, LPVOID) -> BOOL
            {
                const wchar_t *wbuf = static_cast<const wchar_t *>(buffer);
                captured.assign(wbuf, chars_to_write);
                if (chars_written != nullptr)
                {
                    *chars_written = chars_to_write;
                }
                return TRUE;
            }); // [Pre-Assert確認_正常系] - WriteConsoleW() が 5 文字を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - WriteConsoleW() へ渡されたワイド文字列を記録し、5 を返して TRUE を返す。

    // Act
    BOOL actual_ret =
        WriteConsoleU(console_handle, input, 5U, &written,
                      nullptr); // [手順] - 途中に NUL を含む 5 バイトの文字列を指定して WriteConsoleU() を呼び出す。

    // Assert
    EXPECT_TRUE(actual_ret); // [確認_正常系] - WriteConsoleU() の戻り値が TRUE であること。
    EXPECT_EQ(5U, written);  // [確認_正常系] - written が 5 であること。
    EXPECT_EQ(5U, captured.size());
    // [確認_正常系] - WriteConsoleW() が受け取った文字数が 5 であること。
    EXPECT_EQ(0, std::memcmp(captured.data(), L"ab\0cd", 5U * sizeof(wchar_t)));
    // [確認_正常系] - WriteConsoleW() が受け取った内容が L"ab\0cd" と一致すること。
}

// NUL 終端のないバッファーでも指定バイト数だけを変換して出力することの確認
TEST_F(fileApiTest, write_console_converts_buffer_without_terminator)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    const char input[6] = {'\xE3', '\x81', '\x82', '\xE3', '\x81', '\x84'}; // "あい" (NUL 終端なし)
    DWORD written = 0U;
    std::wstring captured;
    HANDLE console_handle = reinterpret_cast<HANDLE>(1);

    // Pre-Assert
    EXPECT_CALL(mock_windows, WriteConsoleW(_, _, _, console_handle, _, 2U, _, nullptr))
        .WillOnce(
            [&captured](const char *, int, const char *, HANDLE, const VOID *buffer, DWORD chars_to_write,
                        LPDWORD chars_written, LPVOID) -> BOOL
            {
                const wchar_t *wbuf = static_cast<const wchar_t *>(buffer);
                captured.assign(wbuf, chars_to_write);
                if (chars_written != nullptr)
                {
                    *chars_written = chars_to_write;
                }
                return TRUE;
            }); // [Pre-Assert確認_正常系] - WriteConsoleW() が 2 文字を指定して 1 回呼び出されること。
                // [Pre-Assert手順] - WriteConsoleW() へ渡されたワイド文字列を記録し、2 を返して TRUE を返す。

    // Act
    BOOL actual_ret =
        WriteConsoleU(console_handle, input, sizeof(input), &written,
                      nullptr); // [手順] - NUL 終端のない 6 バイト領域を指定して WriteConsoleU() を呼び出す。

    // Assert
    EXPECT_TRUE(actual_ret); // [確認_正常系] - WriteConsoleU() の戻り値が TRUE であること。
    EXPECT_EQ(6U, written);  // [確認_正常系] - written が 6 であること。
    EXPECT_EQ(std::wstring(L"\u3042\u3044"), captured);
    // [確認_正常系] - WriteConsoleW() が受け取った内容が L"あい" と一致すること。
}

// 部分書き込み時に書き込めたバイト数へ 0 を格納することの確認
TEST_F(fileApiTest, write_console_stores_zero_on_partial_write)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    const char input[] = "abc";
    DWORD written = 99U;
    HANDLE console_handle = reinterpret_cast<HANDLE>(1);

    // Pre-Assert
    EXPECT_CALL(mock_windows, WriteConsoleW(_, _, _, console_handle, _, 3U, _, nullptr))
        .WillOnce(DoAll(SetArgPointee<6>(1U), Return(TRUE)));
    // [Pre-Assert確認_異常系] - WriteConsoleW() が 3 文字を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - WriteConsoleW() から要求より少ない 1 を設定して TRUE を返す。

    // Act
    BOOL actual_ret = WriteConsoleU(console_handle, input, 3U, &written,
                                    nullptr); // [手順] - 部分書き込みが起きる状態で WriteConsoleU() を呼び出す。

    // Assert
    EXPECT_TRUE(actual_ret); // [確認_異常系] - WriteConsoleU() の戻り値が TRUE であること。
    EXPECT_EQ(0U, written);  // [確認_異常系] - 部分書き込み時に written が 0 であること。
}

// 長さ 0 の書き込みで WriteConsoleW が文字数 0 で呼ばれることの確認
TEST_F(fileApiTest, write_console_accepts_zero_length)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    DWORD written = 99U;
    HANDLE console_handle = reinterpret_cast<HANDLE>(1);

    // Pre-Assert
    EXPECT_CALL(mock_windows, WriteConsoleW(_, _, _, console_handle, _, 0U, _, nullptr))
        .WillOnce(DoAll(SetArgPointee<6>(0U), Return(TRUE)));
    // [Pre-Assert確認_正常系] - WriteConsoleW() が 0 文字を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - WriteConsoleW() から 0 を設定して TRUE を返す。

    // Act
    BOOL actual_ret = WriteConsoleU(console_handle, "", 0U, &written,
                                    nullptr); // [手順] - 長さ 0 を指定して WriteConsoleU() を呼び出す。

    // Assert
    EXPECT_TRUE(actual_ret); // [確認_正常系] - WriteConsoleU() の戻り値が TRUE であること。
    EXPECT_EQ(0U, written);  // [確認_正常系] - written が 0 であること。
}

// lpBuffer が NULL の場合に ERROR_INVALID_PARAMETER で失敗することの確認
TEST_F(fileApiTest, write_console_rejects_null_buffer)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    DWORD written = 99U;
    HANDLE console_handle = reinterpret_cast<HANDLE>(1);

    // Pre-Assert
    EXPECT_CALL(mock_windows, WriteConsoleW(_, _, _, _, _, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - WriteConsoleW() が呼び出されないこと。

    // Act
    BOOL actual_ret = WriteConsoleU(console_handle, nullptr, 3U, &written,
                                    nullptr); // [手順] - lpBuffer に nullptr を渡して WriteConsoleU() を呼び出す。
    DWORD err = GetLastError();

    // Assert
    EXPECT_FALSE(actual_ret);                // [確認_異常系] - WriteConsoleU() の戻り値が FALSE であること。
    EXPECT_EQ(ERROR_INVALID_PARAMETER, err); // [確認_異常系] - GetLastError() が ERROR_INVALID_PARAMETER であること。
    EXPECT_EQ(0U, written);                  // [確認_異常系] - written が 0 に初期化されること。
}

// バッファー不足時に UTF-8 の文字境界で切り詰め、nSize を返すことの確認
TEST_F(fileApiTest, get_module_file_name_truncates_at_utf8_character_boundary)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    const wchar_t kModulePath[] = L"C:\\\u3042\u3044\u3046.exe";
    // "C:\" (3 bytes) + "あ" (3 bytes) + "い" の途中 2 bytes + NUL (1 byte) = nSize 9
    char buf[9] = {};

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetModuleFileNameW(_, _, _, nullptr, _, _))
        .WillOnce(
            [kModulePath](const char *, int, const char *, HMODULE, LPWSTR filename, DWORD) -> DWORD
            {
                std::memcpy(filename, kModulePath, sizeof(kModulePath));
                return static_cast<DWORD>(wcslen(kModulePath));
            }); // [Pre-Assert確認_異常系] - GetModuleFileNameW() が 1 回呼び出されること。
                // [Pre-Assert手順] - GetModuleFileNameW() から L"C:\\あいう.exe" を返す。

    // Act
    DWORD actual_ret = GetModuleFileNameU(
        nullptr, buf,
        sizeof(buf)); // [手順] - "い" の途中で切れる nSize=9 を指定して GetModuleFileNameU() を呼び出す。
    DWORD err = GetLastError();

    // Assert
    EXPECT_EQ(sizeof(buf), actual_ret);
    // [確認_異常系] - GetModuleFileNameU() の戻り値が nSize と一致すること。
    EXPECT_EQ(ERROR_INSUFFICIENT_BUFFER, err);
    // [確認_異常系] - GetLastError() が ERROR_INSUFFICIENT_BUFFER であること。
    EXPECT_STREQ("C:\\\xE3\x81\x82", buf);
    // [確認_異常系] - buf が不完全な末尾文字を除いた "C:\\あ" であること。
}

// 文字の境界で切れる nSize でも切り詰め時は nSize を返すことの確認
TEST_F(fileApiTest, get_module_file_name_keeps_complete_character_at_buffer_end)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    const wchar_t kModulePath[] = L"C:\\\u3042\u3044.exe";
    // "C:\" (3 bytes) + "あ" (3 bytes) + NUL (1 byte) = nSize 7 (ちょうど "あ" で切れる)
    char buf[7] = {};

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetModuleFileNameW(_, _, _, nullptr, _, _))
        .WillOnce(
            [kModulePath](const char *, int, const char *, HMODULE, LPWSTR filename, DWORD) -> DWORD
            {
                std::memcpy(filename, kModulePath, sizeof(kModulePath));
                return static_cast<DWORD>(wcslen(kModulePath));
            }); // [Pre-Assert確認_異常系] - GetModuleFileNameW() が 1 回呼び出されること。
                // [Pre-Assert手順] - GetModuleFileNameW() から L"C:\\あい.exe" を返す。

    // Act
    DWORD actual_ret = GetModuleFileNameU(
        nullptr, buf, sizeof(buf)); // [手順] - 文字境界で切れる nSize=7 を指定して GetModuleFileNameU() を呼び出す。
    DWORD err = GetLastError();

    // Assert
    EXPECT_EQ(sizeof(buf), actual_ret);
    // [確認_異常系] - GetModuleFileNameU() の戻り値が nSize と一致すること。
    EXPECT_EQ(ERROR_INSUFFICIENT_BUFFER, err);
    // [確認_異常系] - GetLastError() が ERROR_INSUFFICIENT_BUFFER であること。
    EXPECT_EQ(sizeof(buf) - 1U, std::strlen(buf));
    // [確認_異常系] - 格納されたバイト数が nSize - 1 であること。
    EXPECT_STREQ("C:\\\xE3\x81\x82", buf);
    // [確認_異常系] - buf が "C:\\あ" であること。
}

// パスがちょうど nSize - 1 バイトで収まる場合に nSize - 1 を返すことの確認
TEST_F(fileApiTest, get_module_file_name_returns_length_when_path_fits_exactly)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    const wchar_t kModulePath[] = L"C:\\a.exe";
    char buf[9] = {}; // "C:\a.exe" (8 bytes) + NUL (1 byte) = 9 bytes

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetModuleFileNameW(_, _, _, nullptr, _, _))
        .WillOnce(
            [kModulePath](const char *, int, const char *, HMODULE, LPWSTR filename, DWORD) -> DWORD
            {
                std::memcpy(filename, kModulePath, sizeof(kModulePath));
                return static_cast<DWORD>(wcslen(kModulePath));
            }); // [Pre-Assert確認_正常系] - GetModuleFileNameW() が 1 回呼び出されること。
                // [Pre-Assert手順] - GetModuleFileNameW() から L"C:\\a.exe" を返す。

    // Act
    DWORD actual_ret =
        GetModuleFileNameU(nullptr, buf,
                           sizeof(buf)); // [手順] - ちょうど収まる nSize=9 を指定して GetModuleFileNameU() を呼び出す。

    // Assert
    EXPECT_EQ(sizeof(buf) - 1U, actual_ret);
    // [確認_正常系] - GetModuleFileNameU() の戻り値が nSize - 1 であること。
    EXPECT_STREQ("C:\\a.exe", buf); // [確認_正常系] - buf にパス全体が格納されること。
}

// 中間バッファーの確保失敗時に ERROR_OUTOFMEMORY を設定して 0 を返すことの確認
TEST_F(fileApiTest, get_module_file_name_reports_out_of_memory_when_allocation_fails)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    char buf[64] = {};

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_malloc(_))
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_malloc() が 1 回呼び出されること。
                                    // [Pre-Assert手順] - cplat_malloc() から nullptr を返す。

    // Act
    DWORD actual_ret = GetModuleFileNameU(
        nullptr, buf, sizeof(buf)); // [手順] - cplat_malloc() が失敗する状態で GetModuleFileNameU() を呼び出す。
    DWORD err = GetLastError();

    // Assert
    EXPECT_EQ(0U, actual_ret);         // [確認_異常系] - GetModuleFileNameU() の戻り値が 0 であること。
    EXPECT_EQ(ERROR_OUTOFMEMORY, err); // [確認_異常系] - GetLastError() が ERROR_OUTOFMEMORY であること。
}

// lpFilename が NULL または nSize が 0 の場合に ERROR_INVALID_PARAMETER で 0 を返すことの確認
TEST_F(fileApiTest, get_module_file_name_rejects_null_buffer_and_zero_size)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    char buf[16] = {};

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetModuleFileNameW(_, _, _, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - GetModuleFileNameW() が呼び出されないこと。

    // Act
    DWORD null_ret = GetModuleFileNameU(
        nullptr, nullptr, sizeof(buf)); // [手順] - lpFilename に nullptr を渡して GetModuleFileNameU() を呼び出す。
    DWORD null_err = GetLastError();
    SetLastError(ERROR_SUCCESS);
    DWORD zero_ret =
        GetModuleFileNameU(nullptr, buf, 0U); // [手順] - nSize に 0 を渡して GetModuleFileNameU() を呼び出す。
    DWORD zero_err = GetLastError();

    // Assert
    EXPECT_EQ(0U, null_ret); // [確認_異常系] - lpFilename が nullptr のとき戻り値が 0 であること。
    EXPECT_EQ(ERROR_INVALID_PARAMETER, null_err);
    // [確認_異常系] - lpFilename が nullptr のとき ERROR_INVALID_PARAMETER が設定されること。
    EXPECT_EQ(0U, zero_ret); // [確認_異常系] - nSize が 0 のとき戻り値が 0 であること。
    EXPECT_EQ(ERROR_INVALID_PARAMETER, zero_err);
    // [確認_異常系] - nSize が 0 のとき ERROR_INVALID_PARAMETER が設定されること。
}

// GetModuleFileNameW が 0 を返した場合に 0 を返すことの確認
TEST_F(fileApiTest, get_module_file_name_propagates_native_failure)
{
    // Arrange
    NiceMock<Mock_windows> mock_windows;
    char buf[64] = {};

    // Pre-Assert
    EXPECT_CALL(mock_windows, GetModuleFileNameW(_, _, _, nullptr, _, _))
        .WillOnce(Return(0U)); // [Pre-Assert確認_異常系] - GetModuleFileNameW() が 1 回呼び出されること。
                               // [Pre-Assert手順] - GetModuleFileNameW() から 0 を返す。

    // Act
    DWORD actual_ret = GetModuleFileNameU(
        nullptr, buf, sizeof(buf)); // [手順] - GetModuleFileNameW() が 0 を返す状態で GetModuleFileNameU() を呼び出す。

    // Assert
    EXPECT_EQ(0U, actual_ret); // [確認_異常系] - GetModuleFileNameU() の戻り値が 0 であること。
}

#endif /* PLATFORM_WINDOWS */
