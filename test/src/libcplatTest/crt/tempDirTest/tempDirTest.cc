#include <cplat/base/platform.h>

#include <testfw.h>
#include <mock_cplat.h>
#include <cplat/base/error.h>
#include <cplat/base/result.h>
#include <cplat/crt/stdlib.h>
#include <cplat/crt/temp_dir_internal.h>

#if defined(PLATFORM_WINDOWS)
    #include <mock_windows.h>

    #include <cerrno>
    #include <cstring>
    #include <cwchar>

using testing::_;
using testing::DoDefault;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;

namespace
{
constexpr DWORD kInitialCapacity = MAX_PATH + 2U;
int g_fake_temp_path2_calls = 0;

DWORD WINAPI fake_get_temp_path2_success(DWORD buffer_length, LPWSTR buffer)
{
    static const wchar_t kSystemTemp[] = L"C:\\CustomSystemTemp\\";
    const DWORD needed = (DWORD)(sizeof(kSystemTemp) / sizeof(kSystemTemp[0]) - 1U);

    ++g_fake_temp_path2_calls;
    if (buffer != nullptr && buffer_length > needed)
    {
        std::memcpy(buffer, kSystemTemp, sizeof(kSystemTemp));
    }
    return needed;
}

DWORD WINAPI fake_get_temp_path2_default_system_temp(DWORD buffer_length, LPWSTR buffer)
{
    static const wchar_t kDefaultSystemTemp[] = L"C:\\Windows\\SystemTemp\\";
    const DWORD needed = (DWORD)(sizeof(kDefaultSystemTemp) / sizeof(kDefaultSystemTemp[0]) - 1U);

    if (buffer != nullptr && buffer_length > needed)
    {
        std::memcpy(buffer, kDefaultSystemTemp, sizeof(kDefaultSystemTemp));
    }
    return needed;
}
} // namespace

class tempDirTest : public testing::Test
{
  protected:
    NiceMock<Mock_cplat> mock_cplat_;
    NiceMock<Mock_windows> mock_windows_;

    // [サブ手順 名前=tempDirTest.SetUp]
    void SetUp() override
    {
        g_fake_temp_path2_calls = 0;
        cplat_internal_temp_dir_set_resolver_for_test(nullptr);
    }
    // [サブ手順終了]

    // [サブ手順 名前=tempDirTest.TearDown]
    void TearDown() override
    {
        cplat_internal_temp_dir_set_resolver_for_test(nullptr);
    }
    // [サブ手順終了]
};

// GetTempPath2W を解決できた場合に GetTempPath2W を呼び GetTempPathW を呼ばないことの確認
// [サブ手順参照 名前=tempDirTest.SetUp]
// [サブ手順参照 名前=tempDirTest.TearDown]
TEST_F(tempDirTest, uses_resolved_get_temp_path2w_without_calling_get_temp_pathw)
{
    // Arrange
    wchar_t *dir = nullptr;
    size_t length = 0U;
    cplat_error detail;
    const HMODULE fake_kernel32 = reinterpret_cast<HMODULE>(static_cast<uintptr_t>(0x1234));

    // Pre-Assert
    EXPECT_CALL(mock_windows_, GetModuleHandleW(_, _, _, _))
        .WillOnce(Return(fake_kernel32)); // [Pre-Assert確認_正常系] - GetModuleHandleW() が 1 回呼び出されること。
    EXPECT_CALL(mock_windows_, GetProcAddress(_, _, _, fake_kernel32, testing::StrEq("GetTempPath2W")))
        .WillOnce(Return(reinterpret_cast<FARPROC>(
            fake_get_temp_path2_success))); // [Pre-Assert確認_正常系] - GetProcAddress() が 1 回呼び出されること。
    EXPECT_CALL(mock_windows_, GetTempPathW(_, _, _, _, _))
        .Times(0); // [Pre-Assert確認_正常系] - GetTempPath2W 解決時は GetTempPathW() が呼び出されないこと。

    // Act
    int rc = cplat_internal_get_temp_dir_w(
        &dir, &length, &detail); // [手順] - GetTempPath2W を解決できる状態で cplat_internal_get_temp_dir_w() を呼び出す。

    // Assert
    ASSERT_EQ(CPLAT_OK, rc); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    EXPECT_EQ(1, g_fake_temp_path2_calls); // [確認_正常系] - GetTempPath2W が 1 回呼ばれること。
    ASSERT_NE(nullptr, dir); // [確認_正常系] - dir が nullptr 以外であること。
    EXPECT_STREQ(L"C:\\CustomSystemTemp\\", dir); // [確認_正常系] - SystemTemp のパスが返ること。
    EXPECT_EQ(std::wcslen(L"C:\\CustomSystemTemp\\"), length); // [確認_正常系] - length が一致すること。

    // Cleanup
    cplat_free(dir);
}

// GetTempPath2W が未解決の場合に GetTempPathW へフォールバックすることの確認
// [サブ手順参照 名前=tempDirTest.SetUp]
// [サブ手順参照 名前=tempDirTest.TearDown]
TEST_F(tempDirTest, falls_back_to_get_temp_pathw_when_get_proc_address_returns_null)
{
    // Arrange
    wchar_t *dir = nullptr;
    size_t length = 0U;
    cplat_error detail;
    const HMODULE fake_kernel32 = reinterpret_cast<HMODULE>(static_cast<uintptr_t>(0x1234));

    // Pre-Assert
    EXPECT_CALL(mock_windows_, GetModuleHandleW(_, _, _, _))
        .WillOnce(Return(fake_kernel32)); // [Pre-Assert確認_正常系] - GetModuleHandleW() が 1 回呼び出されること。
    EXPECT_CALL(mock_windows_, GetProcAddress(_, _, _, fake_kernel32, testing::StrEq("GetTempPath2W")))
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_正常系] - GetProcAddress() が nullptr を返すこと。
    EXPECT_CALL(mock_windows_, GetTempPathW(_, _, _, kInitialCapacity, _))
        .WillOnce(Invoke(
            [](const char *, int, const char *, DWORD buf_len, LPWSTR buf) -> DWORD
            {
                static const wchar_t kFallback[] = L"C:\\Users\\test\\AppData\\Local\\Temp\\";
                const DWORD needed = (DWORD)(sizeof(kFallback) / sizeof(kFallback[0]) - 1U);
                if (buf != nullptr && buf_len > needed)
                {
                    std::memcpy(buf, kFallback, sizeof(kFallback));
                }
                return needed;
            })); // [Pre-Assert確認_正常系] - フォールバック先の GetTempPathW() が 1 回呼び出されること。

    // Act
    int rc = cplat_internal_get_temp_dir_w(
        &dir, &length, &detail); // [手順] - GetProcAddress() が NULL を返す状態で呼び出す。

    // Assert
    ASSERT_EQ(CPLAT_OK, rc); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    ASSERT_NE(nullptr, dir); // [確認_正常系] - dir が nullptr 以外であること。
    EXPECT_STREQ(L"C:\\Users\\test\\AppData\\Local\\Temp\\", dir); // [確認_正常系] - フォールバック結果が返ること。
    EXPECT_EQ(std::wcslen(L"C:\\Users\\test\\AppData\\Local\\Temp\\"), length); // [確認_正常系] - 長さが一致すること。

    // Cleanup
    cplat_free(dir);
}

// テスト用の resolver で既定の SystemTemp (C:\Windows\SystemTemp) を返すことの確認
// [サブ手順参照 名前=tempDirTest.SetUp]
// [サブ手順参照 名前=tempDirTest.TearDown]
TEST_F(tempDirTest, returns_default_system_temp_via_resolver)
{
    // Arrange
    wchar_t *dir = reinterpret_cast<wchar_t *>(static_cast<uintptr_t>(0x1));
    size_t length = 0U;
    cplat_error detail;
    cplat_internal_temp_dir_set_resolver_for_test(fake_get_temp_path2_default_system_temp);

    // Pre-Assert

    // Act
    int rc = cplat_internal_get_temp_dir_w(&dir, &length, &detail); // [手順] - resolver 差し替えで呼び出す。

    // Assert
    ASSERT_EQ(CPLAT_OK, rc); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    ASSERT_NE(nullptr, dir); // [確認_正常系] - dir が nullptr 以外であること。
    EXPECT_STREQ(L"C:\\Windows\\SystemTemp\\", dir); // [確認_正常系] - 既定の SystemTemp がそのまま返ること。
    EXPECT_EQ(std::wcslen(L"C:\\Windows\\SystemTemp\\"), length); // [確認_正常系] - 長さが一致すること。

    // Cleanup
    cplat_free(dir);
}

// 戻り値が 0 の場合に GetLastError() を詳細エラーとして返すことの確認
// [サブ手順参照 名前=tempDirTest.SetUp]
// [サブ手順参照 名前=tempDirTest.TearDown]
TEST_F(tempDirTest, returns_windows_error_when_api_returns_zero)
{
    // Arrange
    wchar_t *dir = reinterpret_cast<wchar_t *>(static_cast<uintptr_t>(0x1));
    size_t length = 99U;
    cplat_error detail;
    const HMODULE fake_kernel32 = reinterpret_cast<HMODULE>(static_cast<uintptr_t>(0x1234));

    // Pre-Assert
    EXPECT_CALL(mock_windows_, GetModuleHandleW(_, _, _, _)).WillOnce(Return(fake_kernel32));
    EXPECT_CALL(mock_windows_, GetProcAddress(_, _, _, fake_kernel32, _)).WillOnce(Return(nullptr));
    EXPECT_CALL(mock_windows_, GetTempPathW(_, _, _, kInitialCapacity, _))
        .WillOnce(Return(0U)); // [Pre-Assert確認_異常系] - GetTempPathW() が 0 を返すこと。
    EXPECT_CALL(mock_windows_, GetLastError(_, _, _))
        .WillOnce(Return(ERROR_ACCESS_DENIED)); // [Pre-Assert確認_異常系] - GetLastError() が 1 回呼び出されること。

    // Act
    int rc = cplat_internal_get_temp_dir_w(&dir, &length, &detail); // [手順] - API が 0 を返す状態で呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_PERMISSION_DENIED, rc); // [確認_異常系] - 戻り値が CPLAT_ERR_PERMISSION_DENIED であること。
    EXPECT_EQ(nullptr, dir); // [確認_異常系] - 失敗時は dir_out に NULL が格納されること。
    EXPECT_EQ(0U, length); // [確認_異常系] - length_out が 0 であること。
    EXPECT_EQ(CPLAT_ERROR_DOMAIN_WINDOWS, cplat_error_get_domain(&detail)); // [確認_異常系] - domain が Win32 であること。
    EXPECT_EQ((unsigned long)ERROR_ACCESS_DENIED, cplat_error_get_windows_error(&detail)); // [確認_異常系] - 詳細が保持されること。
}

// 戻り値が容量 - 1 の場合に 1 回で成功することの確認
// [サブ手順参照 名前=tempDirTest.SetUp]
// [サブ手順参照 名前=tempDirTest.TearDown]
TEST_F(tempDirTest, succeeds_at_capacity_minus_one)
{
    // Arrange
    wchar_t *dir = nullptr;
    size_t length = 0U;
    cplat_error detail;
    const HMODULE fake_kernel32 = reinterpret_cast<HMODULE>(static_cast<uintptr_t>(0x1234));

    // Pre-Assert
    EXPECT_CALL(mock_windows_, GetModuleHandleW(_, _, _, _)).WillOnce(Return(fake_kernel32));
    EXPECT_CALL(mock_windows_, GetProcAddress(_, _, _, fake_kernel32, _)).WillOnce(Return(nullptr));
    EXPECT_CALL(mock_windows_, GetTempPathW(_, _, _, kInitialCapacity, _))
        .WillOnce(Invoke(
            [](const char *, int, const char *, DWORD buf_len, LPWSTR buf) -> DWORD
            {
                const DWORD target_len = buf_len - 1U;
                for (DWORD i = 0U; i < target_len - 1U; ++i)
                {
                    buf[i] = L'a';
                }
                buf[target_len - 1U] = L'\\';
                buf[target_len] = L'\0';
                return target_len;
            })); // [Pre-Assert確認_正常系] - GetTempPathW() が容量 - 1 を返して 1 回で完了すること。

    // Act
    int rc = cplat_internal_get_temp_dir_w(&dir, &length, &detail); // [手順] - 境界値 capacity - 1 で呼び出す。

    // Assert
    ASSERT_EQ(CPLAT_OK, rc); // [確認_正常系] - 戻り値が CPLAT_OK であること。
    ASSERT_NE(nullptr, dir); // [確認_正常系] - dir が nullptr 以外であること。
    EXPECT_EQ(kInitialCapacity - 1U, length); // [確認_正常系] - length が MAX_PATH + 1 であること。
    EXPECT_EQ(L'\\', dir[length - 1U]); // [確認_正常系] - 末尾の文字が保持されていること。
    EXPECT_EQ(L'\0', dir[length]); // [確認_正常系] - NUL 終端されていること。

    // Cleanup
    cplat_free(dir);
}

// 戻り値が容量と等しい場合、および容量 + 1 の場合に n + 1 で確保し直して成功することの確認
// [サブ手順参照 名前=tempDirTest.SetUp]
// [サブ手順参照 名前=tempDirTest.TearDown]
TEST_F(tempDirTest, reallocates_when_return_equals_or_exceeds_capacity)
{
    // Arrange
    wchar_t *dir_equal = nullptr;
    wchar_t *dir_plus_one = nullptr;
    size_t len_equal = 0U;
    size_t len_plus_one = 0U;
    cplat_error detail;
    const HMODULE fake_kernel32 = reinterpret_cast<HMODULE>(static_cast<uintptr_t>(0x1234));

    // Pre-Assert
    EXPECT_CALL(mock_windows_, GetModuleHandleW(_, _, _, _)).WillOnce(Return(fake_kernel32));
    EXPECT_CALL(mock_windows_, GetProcAddress(_, _, _, fake_kernel32, _)).WillOnce(Return(nullptr));
    {
        testing::InSequence seq;
        EXPECT_CALL(mock_windows_, GetTempPathW(_, _, _, kInitialCapacity, _))
            .WillOnce(Return(kInitialCapacity)); // [Pre-Assert確認_正常系] - 1 回目は容量と等しい値を返し呼び直すこと。
        EXPECT_CALL(mock_windows_, GetTempPathW(_, _, _, kInitialCapacity + 1U, _))
            .WillOnce(Invoke(
                [](const char *, int, const char *, DWORD buf_len, LPWSTR buf) -> DWORD
                {
                    (void)buf_len;
                    buf[0] = L'C';
                    buf[1] = L':';
                    buf[2] = L'\\';
                    buf[3] = L'\0';
                    return 3U;
                })); // [Pre-Assert確認_正常系] - n + 1 の領域で 2 回目が呼び出されること。
        EXPECT_CALL(mock_windows_, GetTempPathW(_, _, _, kInitialCapacity, _))
            .WillOnce(Return(kInitialCapacity + 1U)); // [Pre-Assert確認_正常系] - 容量 + 1 でも呼び直すこと。
        EXPECT_CALL(mock_windows_, GetTempPathW(_, _, _, kInitialCapacity + 2U, _))
            .WillOnce(Invoke(
                [](const char *, int, const char *, DWORD buf_len, LPWSTR buf) -> DWORD
                {
                    (void)buf_len;
                    buf[0] = L'D';
                    buf[1] = L':';
                    buf[2] = L'\\';
                    buf[3] = L'\0';
                    return 3U;
                })); // [Pre-Assert確認_正常系] - n + 1 の領域で成功すること。
    }

    // Act
    int rc_equal = cplat_internal_get_temp_dir_w(&dir_equal, &len_equal, &detail); // [手順] - n == 容量 の場合。
    int rc_plus_one =
        cplat_internal_get_temp_dir_w(&dir_plus_one, &len_plus_one, &detail); // [手順] - n == 容量 + 1 の場合。

    // Assert
    ASSERT_EQ(CPLAT_OK, rc_equal); // [確認_正常系] - n == 容量の再試行で CPLAT_OK が返ること。
    EXPECT_STREQ(L"C:\\", dir_equal); // [確認_正常系] - 再試行で得たパスが格納されること。
    EXPECT_EQ(3U, len_equal); // [確認_正常系] - 長さが一致すること。
    ASSERT_EQ(CPLAT_OK, rc_plus_one); // [確認_正常系] - n == 容量 + 1 の再試行で CPLAT_OK が返ること。
    EXPECT_STREQ(L"D:\\", dir_plus_one); // [確認_正常系] - 再試行で得たパスが格納されること。
    EXPECT_EQ(3U, len_plus_one); // [確認_正常系] - 長さが一致すること。

    // Cleanup
    cplat_free(dir_equal);
    cplat_free(dir_plus_one);
}

// 2 回の呼び直しでも収まらない場合に CPLAT_ERR_BUFFER_TOO_SMALL (ERROR_INSUFFICIENT_BUFFER) を返すことの確認
// [サブ手順参照 名前=tempDirTest.SetUp]
// [サブ手順参照 名前=tempDirTest.TearDown]
TEST_F(tempDirTest, returns_buffer_too_small_when_retries_exhausted)
{
    // Arrange
    wchar_t *dir = reinterpret_cast<wchar_t *>(static_cast<uintptr_t>(0x1));
    size_t length = 99U;
    cplat_error detail;
    const HMODULE fake_kernel32 = reinterpret_cast<HMODULE>(static_cast<uintptr_t>(0x1234));

    // Pre-Assert
    EXPECT_CALL(mock_windows_, GetModuleHandleW(_, _, _, _)).WillOnce(Return(fake_kernel32));
    EXPECT_CALL(mock_windows_, GetProcAddress(_, _, _, fake_kernel32, _)).WillOnce(Return(nullptr));
    {
        testing::InSequence seq;
        EXPECT_CALL(mock_windows_, GetTempPathW(_, _, _, kInitialCapacity, _))
            .WillOnce(Return(kInitialCapacity + 1U));
        EXPECT_CALL(mock_windows_, GetTempPathW(_, _, _, kInitialCapacity + 2U, _))
            .WillOnce(Return(kInitialCapacity + 5U));
        EXPECT_CALL(mock_windows_, GetTempPathW(_, _, _, kInitialCapacity + 6U, _))
            .WillOnce(Return(kInitialCapacity + 10U)); // [Pre-Assert確認_異常系] - 初回と 2 回の呼び直しがすべて不足すること。
    }

    // Act
    int rc = cplat_internal_get_temp_dir_w(&dir, &length, &detail); // [手順] - 3 回とも領域不足の値を返す状態で呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_BUFFER_TOO_SMALL, rc); // [確認_異常系] - 戻り値が CPLAT_ERR_BUFFER_TOO_SMALL であること。
    EXPECT_EQ(nullptr, dir); // [確認_異常系] - 失敗時は dir_out に NULL が格納されること。
    EXPECT_EQ(0U, length); // [確認_異常系] - length_out が 0 であること。
    EXPECT_EQ(CPLAT_ERROR_DOMAIN_WINDOWS, cplat_error_get_domain(&detail)); // [確認_異常系] - domain が Win32 であること。
    EXPECT_EQ((unsigned long)ERROR_INSUFFICIENT_BUFFER, cplat_error_get_windows_error(&detail)); // [確認_異常系] - 詳細が ERROR_INSUFFICIENT_BUFFER であること。
}

// 領域確保の失敗時に CPLAT_ERR_OUT_OF_MEMORY を返すことの確認 (初回および再確保時)
// [サブ手順参照 名前=tempDirTest.SetUp]
// [サブ手順参照 名前=tempDirTest.TearDown]
TEST_F(tempDirTest, returns_out_of_memory_when_malloc_fails)
{
    // Arrange
    wchar_t *dir = reinterpret_cast<wchar_t *>(static_cast<uintptr_t>(0x1));
    size_t length = 0U;
    cplat_error detail_first;
    cplat_error detail_retry;
    const HMODULE fake_kernel32 = reinterpret_cast<HMODULE>(static_cast<uintptr_t>(0x1234));

    // Pre-Assert
    EXPECT_CALL(mock_windows_, GetModuleHandleW(_, _, _, _)).WillOnce(Return(fake_kernel32));
    EXPECT_CALL(mock_windows_, GetProcAddress(_, _, _, fake_kernel32, _)).WillOnce(Return(nullptr));
    EXPECT_CALL(mock_cplat_, cplat_malloc(_))
        .WillOnce(Return(nullptr))
        .WillOnce(DoDefault())
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - 初回と再確保時の cplat_malloc() 失敗を順に注入する。
    EXPECT_CALL(mock_windows_, GetTempPathW(_, _, _, kInitialCapacity, _))
        .WillOnce(Return(kInitialCapacity)); // [Pre-Assert確認_異常系] - 2 回目の呼び出しでは再確保を要求すること。

    // Act
    int rc_first = cplat_internal_get_temp_dir_w(&dir, &length, &detail_first); // [手順] - 初回確保の失敗。
    int rc_retry = cplat_internal_get_temp_dir_w(&dir, &length, &detail_retry); // [手順] - 再確保時の失敗。

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY, rc_first); // [確認_異常系] - 初回失敗が CPLAT_ERR_OUT_OF_MEMORY であること。
    EXPECT_EQ(nullptr, dir); // [確認_異常系] - dir が NULL であること。
    EXPECT_EQ(ENOMEM, cplat_error_get_errno(&detail_first)); // [確認_異常系] - detail が ENOMEM であること。
    EXPECT_EQ(CPLAT_ERR_OUT_OF_MEMORY, rc_retry); // [確認_異常系] - 再確保失敗が CPLAT_ERR_OUT_OF_MEMORY であること。
    EXPECT_EQ(ENOMEM, cplat_error_get_errno(&detail_retry)); // [確認_異常系] - detail が ENOMEM であること。
}

// dir_out に NULL を渡した場合に CPLAT_ERR_INVALID_ARGUMENT を返すことの確認
// [サブ手順参照 名前=tempDirTest.SetUp]
// [サブ手順参照 名前=tempDirTest.TearDown]
TEST_F(tempDirTest, rejects_null_dir_out)
{
    // Arrange
    size_t length = 123U;
    cplat_error detail;

    // Pre-Assert

    // Act
    int rc = cplat_internal_get_temp_dir_w(nullptr, &length, &detail); // [手順] - dir_out = nullptr で呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, rc); // [確認_異常系] - CPLAT_ERR_INVALID_ARGUMENT を返すこと。
    EXPECT_EQ(EINVAL, cplat_error_get_errno(&detail)); // [確認_異常系] - EINVAL が記録されること。
}

#else /* PLATFORM_LINUX */

TEST(tempDirTest, dummy_on_linux)
{
    // Arrange

    // Pre-Assert

    // Act

    // Assert
    SUCCEED(); // [確認_正常系] - Linux 環境では Windows 専用テストを実行せず成功とすること。
}

#endif /* PLATFORM_ */
