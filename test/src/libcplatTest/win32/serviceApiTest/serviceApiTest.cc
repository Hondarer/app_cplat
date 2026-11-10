#include <testfw.h>
#include <mock_cplat.h>
#include <cplat/win32/win32.h>
#include <cstddef>
#include <string>
#include <type_traits>

#if defined(PLATFORM_WINDOWS)
namespace
{
SC_HANDLE s_service;
DWORD s_level;
int s_config_calls;
int s_dispatch_calls;
bool s_null_description;
bool s_terminated;
std::wstring s_description;
std::wstring s_service_name;
LPSERVICE_MAIN_FUNCTIONW s_service_proc;
BOOL s_config_result;

void WINAPI service_main(DWORD, LPWSTR *) {}

class serviceApiTest : public testing::Test
{
  protected:
    // [サブ手順 名前=serviceApiTest.SetUp]
    void SetUp() override
    {
        s_config_calls = 0;          // [状態] - 構成変更の W 版呼び出し回数を 0 にする。
        s_dispatch_calls = 0;        // [状態] - ディスパッチャーの W 版呼び出し回数を 0 にする。
        s_null_description = false;  // [状態] - NULL の説明を観測した状態を解除する。
        s_terminated = false;        // [状態] - テーブルの終端を観測した状態を解除する。
        s_description.clear();       // [状態] - W 版へ渡された説明文の記録を空にする。
        s_service_name.clear();      // [状態] - W 版へ渡されたサービス名の記録を空にする。
        s_config_result = TRUE;      // [状態] - 構成変更の W 版の戻り値を TRUE にする。
        SetLastError(ERROR_SUCCESS); // [状態] - Windows の直前エラーをクリアする。
    }
    // [サブ手順終了]
};
} // namespace

extern "C" BOOL fake_ChangeServiceConfig2W(SC_HANDLE service, DWORD level, LPVOID info)
{
    const auto *description = static_cast<const SERVICE_DESCRIPTIONW *>(info);
    s_service = service;
    s_level = level;
    ++s_config_calls;
    s_null_description = description->lpDescription == nullptr;
    if (!s_null_description)
    {
        s_description = description->lpDescription;
    }
    return s_config_result;
}

extern "C" BOOL fake_StartServiceCtrlDispatcherW(const SERVICE_TABLE_ENTRYW *table)
{
    ++s_dispatch_calls;
    s_service_name = table[0].lpServiceName;
    s_service_proc = table[0].lpServiceProc;
    s_terminated = table[1].lpServiceName == nullptr && table[1].lpServiceProc == nullptr;
    return TRUE;
}

static_assert(sizeof(SERVICE_TABLE_ENTRYU) == sizeof(SERVICE_TABLE_ENTRYW));
static_assert(offsetof(SERVICE_TABLE_ENTRYU, lpServiceName) == offsetof(SERVICE_TABLE_ENTRYW, lpServiceName));
static_assert(offsetof(SERVICE_TABLE_ENTRYU, lpServiceProc) == offsetof(SERVICE_TABLE_ENTRYW, lpServiceProc));
static_assert(std::is_same_v<decltype(SERVICE_TABLE_ENTRYU::lpServiceProc), LPSERVICE_MAIN_FUNCTIONW>);

// UTF-8 の説明を W 版の構造体へ変換することの確認
// [サブ手順参照 名前=serviceApiTest.SetUp]
TEST_F(serviceApiTest, ConvertsDescription)
{
    // Arrange
    char text[] = "説明"; // [状態] - UTF-8 の説明文を用意する。
    SERVICE_DESCRIPTIONA description = {text};
    SC_HANDLE service = reinterpret_cast<SC_HANDLE>(1);
    // Pre-Assert
    // Act
    BOOL actual_ret =
        ChangeServiceConfig2U(service, SERVICE_CONFIG_DESCRIPTION, &description); // [手順] - UTF-8 の説明文を指定する。
    // Assert
    EXPECT_TRUE(actual_ret);                         // [確認_正常系] - 戻り値が TRUE であること。
    EXPECT_EQ(1, s_config_calls);                    // [確認_正常系] - W 版が 1 回呼び出されること。
    EXPECT_EQ(service, s_service);                   // [確認_正常系] - サービス ハンドルが保持されること。
    EXPECT_EQ(SERVICE_CONFIG_DESCRIPTION, s_level);  // [確認_正常系] - 情報レベルが保持されること。
    EXPECT_EQ(L"説明", s_description);               // [確認_正常系] - 説明が UTF-16 に変換されること。
    EXPECT_STREQ("説明", description.lpDescription); // [確認_正常系] - 入力の説明が変更されないこと。
}

// NULL の情報ポインターを W 版へ渡さず拒否することの確認
// [サブ手順参照 名前=serviceApiTest.SetUp]
TEST_F(serviceApiTest, RejectsNullInfo)
{
    // Arrange
    // Pre-Assert
    // Act
    BOOL actual_ret =
        ChangeServiceConfig2U(nullptr, SERVICE_CONFIG_DESCRIPTION, nullptr); // [手順] - lpInfo に nullptr を指定する。
    DWORD error = GetLastError();
    // Assert
    EXPECT_FALSE(actual_ret);                  // [確認_異常系] - 戻り値が FALSE であること。
    EXPECT_EQ(ERROR_INVALID_PARAMETER, error); // [確認_異常系] - 不正な引数のエラーが設定されること。
    EXPECT_EQ(0, s_config_calls);              // [確認_異常系] - W 版が呼び出されないこと。
}

// NULL の説明を変換せず W 版のメンバーへ渡すことの確認
// [サブ手順参照 名前=serviceApiTest.SetUp]
TEST_F(serviceApiTest, PassesNullDescription)
{
    // Arrange
    SERVICE_DESCRIPTIONA description = {}; // [状態] - lpDescription を nullptr にする。
    // Pre-Assert
    // Act
    BOOL actual_ret =
        ChangeServiceConfig2U(nullptr, SERVICE_CONFIG_DESCRIPTION, &description); // [手順] - NULL の説明を指定する。
    // Assert
    EXPECT_TRUE(actual_ret);         // [確認_正常系] - 戻り値が TRUE であること。
    EXPECT_EQ(1, s_config_calls);    // [確認_正常系] - W 版が 1 回呼び出されること。
    EXPECT_TRUE(s_null_description); // [確認_正常系] - W 版へ渡す lpDescription が nullptr であること。
}

// 未対応の情報レベルを拒否することの確認
// [サブ手順参照 名前=serviceApiTest.SetUp]
TEST_F(serviceApiTest, RejectsUnsupportedLevel)
{
    // Arrange
    SERVICE_DESCRIPTIONA description = {};
    // Pre-Assert
    // Act
    BOOL actual_ret = ChangeServiceConfig2U(nullptr, SERVICE_CONFIG_FAILURE_ACTIONS,
                                            &description); // [手順] - 未対応の情報レベルを指定する。
    DWORD error = GetLastError();
    // Assert
    EXPECT_FALSE(actual_ret);                  // [確認_異常系] - 戻り値が FALSE であること。
    EXPECT_EQ(ERROR_INVALID_PARAMETER, error); // [確認_異常系] - 不正な引数のエラーが設定されること。
    EXPECT_EQ(0, s_config_calls);              // [確認_異常系] - W 版が呼び出されないこと。
}

// UTF-16 への変換失敗を拒否することの確認
// [サブ手順参照 名前=serviceApiTest.SetUp]
TEST_F(serviceApiTest, RejectsConversionFailure)
{
    // Arrange
    testing::NiceMock<Mock_cplat> mock_cplat;
    char text[] = "description";
    SERVICE_DESCRIPTIONA description = {text};
    ON_CALL(mock_cplat, cplat_utf8_to_wstr_alloc(testing::_))
        .WillByDefault(testing::Return(nullptr)); // [状態] - UTF-16 への変換を失敗させる。
    // Pre-Assert
    // Act
    BOOL actual_ret = ChangeServiceConfig2U(nullptr, SERVICE_CONFIG_DESCRIPTION,
                                            &description); // [手順] - 変換が失敗する説明文を指定する。
    DWORD error = GetLastError();
    // Assert
    EXPECT_FALSE(actual_ret);                  // [確認_異常系] - 戻り値が FALSE であること。
    EXPECT_EQ(ERROR_INVALID_PARAMETER, error); // [確認_異常系] - 不正な引数のエラーが設定されること。
    EXPECT_EQ(0, s_config_calls);              // [確認_異常系] - W 版が呼び出されないこと。
}

// W 版の失敗をそのまま返すことの確認
// [サブ手順参照 名前=serviceApiTest.SetUp]
TEST_F(serviceApiTest, PreservesNativeFailure)
{
    // Arrange
    SERVICE_DESCRIPTIONA description = {};
    s_config_result = FALSE; // [状態] - W 版が FALSE を返すようにする。
    // Pre-Assert
    // Act
    BOOL actual_ret = ChangeServiceConfig2U(nullptr, SERVICE_CONFIG_DESCRIPTION,
                                            &description); // [手順] - W 版が失敗する条件で呼び出す。
    // Assert
    EXPECT_FALSE(actual_ret);     // [確認_異常系] - W 版の FALSE が返ること。
    EXPECT_EQ(1, s_config_calls); // [確認_異常系] - W 版が 1 回呼び出されること。
}

// サービス名を変換し関数ポインターと終端を維持することの確認
// [サブ手順参照 名前=serviceApiTest.SetUp]
TEST_F(serviceApiTest, ConvertsServiceTable)
{
    // Arrange
    const SERVICE_TABLE_ENTRYU table[] = {{"サービス", service_main},
                                          {nullptr, nullptr}}; // [状態] - UTF-8 のサービス名と終端を用意する。
    // Pre-Assert
    // Act
    BOOL actual_ret = StartServiceCtrlDispatcherU(table); // [手順] - UTF-8 サービス テーブルを指定する。
    // Assert
    EXPECT_TRUE(actual_ret);                 // [確認_正常系] - 戻り値が TRUE であること。
    EXPECT_EQ(1, s_dispatch_calls);          // [確認_正常系] - W 版が 1 回呼び出されること。
    EXPECT_EQ(L"サービス", s_service_name);  // [確認_正常系] - サービス名が UTF-16 に変換されること。
    EXPECT_EQ(service_main, s_service_proc); // [確認_正常系] - サービス メイン関数が保持されること。
    EXPECT_TRUE(s_terminated);               // [確認_正常系] - 終端の両メンバーが nullptr であること。
}

// NULL のテーブルを拒否することの確認
// [サブ手順参照 名前=serviceApiTest.SetUp]
TEST_F(serviceApiTest, RejectsNullServiceTable)
{
    // Arrange
    // Pre-Assert
    // Act
    BOOL actual_ret = StartServiceCtrlDispatcherU(nullptr); // [手順] - NULL のテーブルを指定する。
    DWORD error = GetLastError();
    // Assert
    EXPECT_FALSE(actual_ret);                  // [確認_異常系] - 戻り値が FALSE であること。
    EXPECT_EQ(ERROR_INVALID_PARAMETER, error); // [確認_異常系] - 不正な引数のエラーが設定されること。
    EXPECT_EQ(0, s_dispatch_calls);            // [確認_異常系] - W 版が呼び出されないこと。
}
// 新しい情報ポインター型で実ライブラリへ委譲できることの確認
// [サブ手順参照 名前=serviceApiTest.SetUp]
TEST_F(serviceApiTest, RealDelegateRejectsNullInfo)
{
    // Arrange
    // Pre-Assert
    // Act
    BOOL actual_ret = delegate_real_ChangeServiceConfig2U(
        nullptr, SERVICE_CONFIG_DESCRIPTION, nullptr); // [手順] - real delegate へ NULL の情報ポインターを渡す。
    DWORD error = GetLastError();
    // Assert
    EXPECT_FALSE(actual_ret);                  // [確認_異常系] - 実ライブラリの FALSE が返ること。
    EXPECT_EQ(ERROR_INVALID_PARAMETER, error); // [確認_異常系] - 実ライブラリが不正な引数のエラーを設定すること。
    EXPECT_EQ(0, s_config_calls);              // [確認_異常系] - テスト側の W 版 fake を呼び出さないこと。
}

// 新しいサービス テーブル型で実ライブラリへ委譲できることの確認
// [サブ手順参照 名前=serviceApiTest.SetUp]
TEST_F(serviceApiTest, RealDelegateRejectsNullTable)
{
    // Arrange
    // Pre-Assert
    // Act
    BOOL actual_ret =
        delegate_real_StartServiceCtrlDispatcherU(nullptr); // [手順] - real delegate へ NULL のテーブルを渡す。
    DWORD error = GetLastError();
    // Assert
    EXPECT_FALSE(actual_ret);                  // [確認_異常系] - 実ライブラリの FALSE が返ること。
    EXPECT_EQ(ERROR_INVALID_PARAMETER, error); // [確認_異常系] - 実ライブラリが不正な引数のエラーを設定すること。
    EXPECT_EQ(0, s_dispatch_calls);            // [確認_異常系] - テスト側の W 版 fake を呼び出さないこと。
}
#endif /* PLATFORM_WINDOWS */
