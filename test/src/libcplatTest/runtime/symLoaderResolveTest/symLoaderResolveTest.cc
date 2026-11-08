#include <testfw.h>
#include <mock_cplat.h>
#include <cplat/runtime/sym_loader.h>

#include <cstring>

#include "sym_loader_resolve.inject.h"

namespace
{
void complete_entry_lock_initialization(cplat_sym_loader_entry *entry)
{
    cplat_atomic_store_i32(&entry->lock_state, 2, CPLAT_MEMORY_ORDER_RELAXED);
}
} // namespace

class symLoaderResolveTest : public Test
{
  protected:
    cplat_sym_loader_entry entry_ = CPLAT_SYM_LOADER_ENTRY_INIT("test_key", void (*)(void));

    /* lib_name / func_name は固定長配列のため、テストからは直接書き込む */
    // [サブ手順 名前=symLoaderResolveTest.set_names]
    void set_names(const char *lib_name, const char *func_name)
    {
        ASSERT_EQ(CPLAT_OK, cplat_strcpy(entry_.lib_name, sizeof(entry_.lib_name), lib_name));
        // [状態確認] - lib_name を設定する cplat_strcpy() の戻り値が CPLAT_OK であること。
        ASSERT_EQ(CPLAT_OK, cplat_strcpy(entry_.func_name, sizeof(entry_.func_name), func_name));
        // [状態確認] - func_name を設定する cplat_strcpy() の戻り値が CPLAT_OK であること。
    }
    // [サブ手順終了]

    // [サブ手順 名前=symLoaderResolveTest.TearDown]
    void TearDown() override
    {
        cplat_sym_loader_entry *entries[] = {&entry_};

        cplat_sym_loader_dispose(entries, 1u);
    }
    // [サブ手順終了]
};

// 実在するライブラリとシンボルが解決されることの確認
TEST_F(symLoaderResolveTest, resolves_existing_symbol)
{
    // Arrange
    // [サブ手順参照 名前=symLoaderResolveTest.set_names]
    set_names("libcplat",
              "cplat_path_basename"); // [状態] - 実在するライブラリ名と関数名を設定したエントリを用意する。

    // Pre-Assert

    // Act
    void *func_ptr = nullptr;
    int status_0 = cplat_sym_loader_resolve(&entry_, &func_ptr); // [手順] - cplat_sym_loader_resolve() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, status_0); // [確認_正常系] - 結果コードを確認する。
    EXPECT_NE(nullptr, func_ptr);  // [確認_正常系] - cplat_sym_loader_resolve() の出力が nullptr 以外であること。
    EXPECT_EQ(
        1,
        cplat_atomic_load_i32(
            &entry_.resolved,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_正常系] - entry_.resolved を読み込んだ cplat_atomic_load_i32() の戻り値が 1 であること。(解決済みになることの確認)
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// 2 回目の呼び出しが解決済みの結果をそのまま返すことの確認
TEST_F(symLoaderResolveTest, second_call_returns_cached_result)
{
    // Arrange
    // [サブ手順参照 名前=symLoaderResolveTest.set_names]
    set_names("libcplat", "cplat_path_basename");
    void *first = nullptr;
    int status_0 = cplat_sym_loader_resolve(&entry_, &first); // [状態] - 1 回目の解決を済ませておく。

    // Pre-Assert

    // Act
    void *second = nullptr;
    int status_1 = cplat_sym_loader_resolve(
        &entry_, &second); // [手順] - 2 回目として同じエントリを指定して cplat_sym_loader_resolve() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, status_0); // [確認_正常系] - 結果コードを確認する。
    EXPECT_EQ(CPLAT_OK, status_1); // [確認_正常系] - 結果コードを確認する。
    EXPECT_EQ(
        first,
        second); // [確認_正常系] - 2 回目の cplat_sym_loader_resolve() の戻り値が 1 回目の cplat_sym_loader_resolve() の戻り値と一致すること。
    EXPECT_EQ(
        1,
        cplat_atomic_load_i32(
            &entry_.resolved,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_正常系] - entry_.resolved を読み込んだ cplat_atomic_load_i32() の戻り値が 1 であること。(解決状態が変化しないことの確認)
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// lib_name と func_name がともに "default" の場合に明示的既定値として扱われることの確認
TEST_F(symLoaderResolveTest, marks_explicit_default_when_both_names_are_default)
{
    // Arrange
    // [サブ手順参照 名前=symLoaderResolveTest.set_names]
    set_names("default", "default"); // [状態] - lib_name と func_name の双方に "default" を設定する。

    // Pre-Assert

    // Act
    void *func_ptr = nullptr;
    int status_0 = cplat_sym_loader_resolve(&entry_, &func_ptr); // [手順] - cplat_sym_loader_resolve() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, status_0); // [確認_正常系] - 結果コードを確認する。
    EXPECT_EQ(nullptr, func_ptr);  // [確認_正常系] - cplat_sym_loader_resolve() の出力が nullptr であること。
    EXPECT_EQ(
        2,
        cplat_atomic_load_i32(
            &entry_.resolved,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_正常系] - entry_.resolved を読み込んだ cplat_atomic_load_i32() の戻り値が 2 であること。(明示的既定値になることの確認)
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// lib_name が未設定の場合に定義なしとして扱われることの確認
TEST_F(symLoaderResolveTest, marks_undefined_when_lib_name_is_empty)
{
    // Arrange
    // [サブ手順参照 名前=symLoaderResolveTest.set_names]
    set_names("", "cplat_path_basename"); // [状態] - lib_name を空文字列にしたエントリを用意する。

    // Pre-Assert

    // Act
    void *func_ptr = nullptr;
    int status_0 = cplat_sym_loader_resolve(&entry_, &func_ptr); // [手順] - cplat_sym_loader_resolve() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_NOT_FOUND, status_0); // [確認_正常系] - 結果コードを確認する。
    EXPECT_EQ(nullptr, func_ptr); // [確認_異常系] - cplat_sym_loader_resolve() の出力が nullptr であること。
    EXPECT_EQ(
        CPLAT_ERR_NOT_FOUND,
        cplat_atomic_load_i32(
            &entry_.resolved,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_異常系] - entry_.resolved を読み込んだ cplat_atomic_load_i32() の戻り値が CPLAT_ERR_NOT_FOUND であること。(定義なしになることの確認)
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// func_name が未設定の場合に定義なしとして扱われることの確認
TEST_F(symLoaderResolveTest, marks_undefined_when_func_name_is_empty)
{
    // Arrange
    // [サブ手順参照 名前=symLoaderResolveTest.set_names]
    set_names("libcplat", ""); // [状態] - func_name を空文字列にしたエントリを用意する。

    // Pre-Assert

    // Act
    void *func_ptr = nullptr;
    int status_0 = cplat_sym_loader_resolve(&entry_, &func_ptr); // [手順] - cplat_sym_loader_resolve() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_NOT_FOUND, status_0); // [確認_正常系] - 結果コードを確認する。
    EXPECT_EQ(nullptr, func_ptr); // [確認_異常系] - cplat_sym_loader_resolve() の出力が nullptr であること。
    EXPECT_EQ(
        CPLAT_ERR_NOT_FOUND,
        cplat_atomic_load_i32(
            &entry_.resolved,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_異常系] - entry_.resolved を読み込んだ cplat_atomic_load_i32() の戻り値が CPLAT_ERR_NOT_FOUND であること。(定義なしになることの確認)
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// 拡張子を加えた名称が上限を超える場合に名称長超過として扱われることの確認
TEST_F(symLoaderResolveTest, marks_name_too_long_when_extension_does_not_fit)
{
    // Arrange
    char long_name[CPLAT_SYM_LOADER_NAME_MAX];

    std::memset(long_name, 'a', sizeof(long_name) - 1u);
    long_name[sizeof(long_name) - 1u] = '\0';
    // [サブ手順参照 名前=symLoaderResolveTest.set_names]
    set_names(long_name,
              "cplat_path_basename"); // [状態] - 拡張子を加えると上限を超える長さの lib_name を設定する。

    // Pre-Assert

    // Act
    void *func_ptr = nullptr;
    int status_0 = cplat_sym_loader_resolve(&entry_, &func_ptr); // [手順] - cplat_sym_loader_resolve() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_RANGE, status_0); // [確認_正常系] - 結果コードを確認する。
    EXPECT_EQ(nullptr, func_ptr); // [確認_異常系] - cplat_sym_loader_resolve() の出力が nullptr であること。
    EXPECT_EQ(
        CPLAT_ERR_OUT_OF_RANGE,
        cplat_atomic_load_i32(
            &entry_.resolved,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_異常系] - entry_.resolved を読み込んだ cplat_atomic_load_i32() の戻り値が CPLAT_ERR_OUT_OF_RANGE であること。(名称長超過になることの確認)
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// 実在しないライブラリのオープン失敗が記録されることの確認
TEST_F(symLoaderResolveTest, marks_open_error_when_library_is_missing)
{
    // Arrange
    // [サブ手順参照 名前=symLoaderResolveTest.set_names]
    set_names("libcplat_missing_for_test",
              "cplat_path_basename"); // [状態] - 実在しないライブラリ名を設定したエントリを用意する。

    // Pre-Assert

    // Act
    void *func_ptr = nullptr;
    int status_0 = cplat_sym_loader_resolve(&entry_, &func_ptr); // [手順] - cplat_sym_loader_resolve() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, status_0); // [確認_正常系] - 結果コードを確認する。
    EXPECT_EQ(nullptr, func_ptr);           // [確認_異常系] - cplat_sym_loader_resolve() の出力が nullptr であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        cplat_atomic_load_i32(
            &entry_.resolved,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_異常系] - entry_.resolved を読み込んだ cplat_atomic_load_i32() の戻り値が CPLAT_ERR_UNKNOWN であること。(ライブラリ オープン エラーになることの確認)
    EXPECT_EQ(nullptr, entry_.handle);    // [確認_異常系] - entry_.handle が nullptr であること。
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// 実在しないシンボル名でも解放処理までハンドルを保持することの確認
TEST_F(symLoaderResolveTest, retains_handle_for_disposal_when_symbol_is_missing)
{
    // Arrange
    // [サブ手順参照 名前=symLoaderResolveTest.set_names]
    set_names("libcplat",
              "cplat_symbol_that_does_not_exist"); // [状態] - 実在するライブラリと実在しない関数名を設定する。

    // Pre-Assert

    // Act
    void *func_ptr = nullptr;
    int status_0 = cplat_sym_loader_resolve(&entry_, &func_ptr); // [手順] - cplat_sym_loader_resolve() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_NOT_FOUND, status_0); // [確認_正常系] - 結果コードを確認する。
    EXPECT_EQ(nullptr, func_ptr); // [確認_異常系] - cplat_sym_loader_resolve() の出力が nullptr であること。
    EXPECT_EQ(
        CPLAT_ERR_NOT_FOUND,
        cplat_atomic_load_i32(
            &entry_.resolved,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_異常系] - entry_.resolved を読み込んだ cplat_atomic_load_i32() の戻り値が CPLAT_ERR_NOT_FOUND であること。(シンボル未検出を記録することの確認)
    EXPECT_NE(
        nullptr,
        entry_
            .handle); // [確認_異常系] - entry_.handle が nullptr 以外であること。(シンボルが見つからない場合もハンドルを保持することの確認)
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

#if defined(PLATFORM_LINUX)

// ロックの生成に失敗した場合に解決が失敗することの確認
// Windows は InterlockedCompareExchange を使うが、ロック生成の失敗経路は共通である
TEST_F(symLoaderResolveTest, returns_null_when_lock_creation_fails)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;

    // [サブ手順参照 名前=symLoaderResolveTest.set_names]
    set_names("libcplat", "cplat_path_basename"); // [状態] - 実在するライブラリ名と関数名を設定する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_local_lock_create(_))
        .WillOnce(Return(CPLAT_ERR_UNKNOWN))
        .WillRepeatedly(
            DoDefault()); // [Pre-Assert確認_異常系] - cplat_local_lock_create() が 1 回以上呼び出されること。
    // [Pre-Assert手順] - cplat_local_lock_create() から 1 回目は CPLAT_ERR_UNKNOWN を返し、以降は製品 (prod) の処理へ委譲する。

    // Act
    void *func_ptr = nullptr;
    int status_0 = cplat_sym_loader_resolve(&entry_, &func_ptr); // [手順] - cplat_sym_loader_resolve() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, status_0); // [確認_正常系] - 結果コードを確認する。
    EXPECT_EQ(nullptr, func_ptr);           // [確認_異常系] - cplat_sym_loader_resolve() の出力が nullptr であること。
    EXPECT_EQ(
        0,
        cplat_atomic_load_i32(
            &entry_.resolved,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_異常系] - entry_.resolved を読み込んだ cplat_atomic_load_i32() の戻り値が 0 であること。(未解決のままであることの確認)
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// ロックの取得に失敗した場合に解決が失敗することの確認
TEST_F(symLoaderResolveTest, returns_null_when_lock_acquisition_fails)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;

    // [サブ手順参照 名前=symLoaderResolveTest.set_names]
    set_names("libcplat", "cplat_path_basename"); // [状態] - 実在するライブラリ名と関数名を設定する。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_local_lock_lock(_, _))
        .WillOnce(Return(CPLAT_ERR_UNKNOWN))
        .WillRepeatedly(DoDefault()); // [Pre-Assert確認_異常系] - cplat_local_lock_lock() が 1 回以上呼び出されること。
    // [Pre-Assert手順] - cplat_local_lock_lock() から 1 回目は CPLAT_ERR_UNKNOWN を返し、以降は製品 (prod) の処理へ委譲する。

    // Act
    void *func_ptr = nullptr;
    int status_0 = cplat_sym_loader_resolve(&entry_, &func_ptr); // [手順] - cplat_sym_loader_resolve() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, status_0); // [確認_正常系] - 結果コードを確認する。
    EXPECT_EQ(nullptr, func_ptr);           // [確認_異常系] - cplat_sym_loader_resolve() の出力が nullptr であること。
    EXPECT_EQ(
        0,
        cplat_atomic_load_i32(
            &entry_.resolved,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_異常系] - entry_.resolved を読み込んだ cplat_atomic_load_i32() の戻り値が 0 であること。(未解決のままであることの確認)
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// ほかのスレッドがロック初期化を完了した状態を再利用することの確認
TEST_F(symLoaderResolveTest, reuses_lock_initialized_by_another_thread)
{
    // Arrange
    ASSERT_EQ(CPLAT_OK, cplat_local_lock_create(&entry_.lock)); // [状態] - エントリのロックを生成する。
    // [状態確認] - cplat_local_lock_create() の戻り値が CPLAT_OK であること。
    cplat_atomic_store_i32(&entry_.lock_state, 1, CPLAT_MEMORY_ORDER_RELAXED);
    test_sym_loader_set_entry_lock_wait_hook(complete_entry_lock_initialization);

    // Pre-Assert

    // Act
    int result = test_sym_loader_ensure_entry_lock_initialized(
        &entry_); // [手順] - 他スレッドの初期化中を表すエントリを指定して test_sym_loader_ensure_entry_lock_initialized() を呼び出す。

    // Assert
    EXPECT_EQ(0, result); // [確認_正常系] - test_sym_loader_ensure_entry_lock_initialized() の戻り値が 0 であること。
    EXPECT_EQ(
        2,
        cplat_atomic_load_i32(
            &entry_.lock_state,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_正常系] - entry_.lock_state を読み込んだ cplat_atomic_load_i32() の戻り値が 2 であること。(ロック初期化が完了することの確認)

    // Cleanup
    test_sym_loader_set_entry_lock_wait_hook(nullptr);
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// ロック初期化待機の既定処理が呼び出し元へ制御を戻すことの確認
TEST_F(symLoaderResolveTest, default_lock_wait_yields_execution)
{
    // Arrange

    // Pre-Assert

    // Act
    test_sym_loader_default_entry_lock_wait(
        &entry_); // [手順] - エントリを指定して test_sym_loader_default_entry_lock_wait() を呼び出す。

    // Assert
    SUCCEED(); // [確認_正常系] - SUCCEED() に到達すること。(test_sym_loader_default_entry_lock_wait() が呼び出し元へ制御を戻すことの確認)
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// ほかのスレッドによるロック初期化失敗を通知することの確認
TEST_F(symLoaderResolveTest, reports_lock_initialization_failure_from_another_thread)
{
    // Arrange
    cplat_atomic_store_i32(&entry_.lock_state, -1, CPLAT_MEMORY_ORDER_RELAXED);

    // Pre-Assert

    // Act
    int result = test_sym_loader_ensure_entry_lock_initialized(
        &entry_); // [手順] - 他スレッドの初期化失敗を表すエントリを指定して test_sym_loader_ensure_entry_lock_initialized() を呼び出す。

    // Assert
    EXPECT_EQ(-1, result); // [確認_異常系] - test_sym_loader_ensure_entry_lock_initialized() の戻り値が -1 であること。
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// ロック取得中に解決済みとなった結果を再利用することの確認
TEST_F(symLoaderResolveTest, returns_result_resolved_while_waiting_for_lock)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    // [サブ手順参照 名前=symLoaderResolveTest.set_names]
    set_names("default", "not_default");

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_local_lock_lock(_, _))
        .WillOnce(Invoke(
            [this](cplat_local_lock *lock, int timeout)
            {
                cplat_atomic_store_i32(&entry_.resolved, 1, CPLAT_MEMORY_ORDER_RELAXED);
                cplat_atomic_store_ptr(&entry_.func_ptr, reinterpret_cast<void *>(1), CPLAT_MEMORY_ORDER_RELAXED);
                return delegate_real_cplat_local_lock_lock(lock, timeout);
            })); // [Pre-Assert確認_正常系] - cplat_local_lock_lock() が 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_local_lock_lock() から、resolved と func_ptr を設定して CPLAT_OK を返す。

    // Act
    void *result = nullptr;
    int status_0 = cplat_sym_loader_resolve(
        &entry_,
        &result); // [手順] - ロック待機中に解決済みとなるエントリを指定して cplat_sym_loader_resolve() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, status_0); // [確認_正常系] - 結果コードを確認する。
    EXPECT_EQ(
        reinterpret_cast<void *>(1),
        result); // [確認_正常系] - cplat_sym_loader_resolve() の出力が reinterpret_cast<void *>(1) と一致すること。(別スレッドの解決結果を再利用することの確認)
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// ライブラリ名だけが default の場合に通常のライブラリ名として扱うことの確認
TEST_F(symLoaderResolveTest, does_not_mark_default_when_only_library_name_is_default)
{
    // Arrange
    // [サブ手順参照 名前=symLoaderResolveTest.set_names]
    set_names("default", "not_default");

    // Pre-Assert

    // Act
    void *result = nullptr;
    int status_0 = cplat_sym_loader_resolve(
        &entry_,
        &result); // [手順] - ライブラリ名だけが default のエントリを指定して cplat_sym_loader_resolve() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, status_0); // [確認_正常系] - 結果コードを確認する。
    EXPECT_EQ(nullptr, result);             // [確認_異常系] - cplat_sym_loader_resolve() の出力が nullptr であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        cplat_atomic_load_i32(
            &entry_.resolved,
            CPLAT_MEMORY_ORDER_RELAXED)); // [確認_異常系] - entry_.resolved を読み込んだ cplat_atomic_load_i32() の戻り値が CPLAT_ERR_UNKNOWN であること。(実在しない default ライブラリのオープン エラーになることの確認)
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

#endif /* PLATFORM_LINUX */

// エラーのキャッシュと必須引数の検査で出力を保持することの確認
TEST_F(symLoaderResolveTest, cached_failure_and_invalid_arguments_preserve_output)
{
    // Arrange
    void *sentinel = reinterpret_cast<void *>(static_cast<uintptr_t>(42));
    void *output = sentinel; // [状態] - 未設定エントリと出力保持を検出する値を用意する。

    // Pre-Assert

    // Act
    int first = cplat_sym_loader_resolve(&entry_, &output);
    int repeated = cplat_sym_loader_resolve(&entry_, &output);
    int null_entry = cplat_sym_loader_resolve(nullptr, &output);
    int null_output = cplat_sym_loader_resolve(&entry_, nullptr); // [手順] - 失敗の再参照と必須引数の検査を実行する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_NOT_FOUND, first); // [確認_異常系] - 未設定を通知すること。
    EXPECT_EQ(first, repeated);            // [確認_異常系] - キャッシュでも同じ結果になること。
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, null_entry);
    EXPECT_EQ(CPLAT_ERR_INVALID_ARGUMENT, null_output);
    EXPECT_EQ(sentinel, output); // [確認_異常系] - 失敗で出力を変更しないこと。
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]

// 固定長の関数名が終端されていない場合に配列外を参照しないことの確認
TEST_F(symLoaderResolveTest, rejects_unterminated_function_name)
{
    // Arrange
    memset(entry_.func_name, 'x', sizeof(entry_.func_name)); // [状態] - NUL 終端のない名前を用意する。
    void *output = nullptr;

    // Pre-Assert

    // Act
    int actual = cplat_sym_loader_resolve(&entry_, &output); // [手順] - 名前を検証する。

    // Assert
    EXPECT_EQ(CPLAT_ERR_OUT_OF_RANGE, actual); // [確認_異常系] - 終端の欠落を拒否すること。
}
// [サブ手順参照 名前=symLoaderResolveTest.TearDown]
