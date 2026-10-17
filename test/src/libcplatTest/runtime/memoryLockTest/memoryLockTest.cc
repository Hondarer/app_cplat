#include <testfw.h>
#include <mock_pthread.h>
#include <sys/mock_mman.h>
#include <mock_cplat.h>
#include <cplat/runtime/memory_lock.h>

#include <cstdlib>
#include <cerrno>
#include <thread>

#include "memory_lock.inject.h"

using testing::_;
using testing::DoAll;
using testing::NiceMock;
using testing::Return;

class memoryLockTest : public Test
{
};

struct self_lock_thread_result
{
    cplat_memory_lock_scope *scope;
    int result;
    int pad;
};

static void lock_self_current_in_thread(self_lock_thread_result *thread_result)
{
    cplat_memory_lock_self_options options = {};

    thread_result->scope = nullptr;
    options.flags = CPLAT_MEMORY_LOCK_CURRENT;
    options.stack_prefault_bytes = 4096U;
    thread_result->result = cplat_memory_lock_self(&options, &thread_result->scope);
}

static void release_self_scope_in_thread(cplat_memory_lock_scope *scope, int *result)
{
    *result = cplat_memory_lock_scope_release(scope);
}

// range ロック API が不正引数を検出することの確認
TEST_F(memoryLockTest, test_range_rejects_invalid_arguments)
{
    // Arrange
    unsigned char buffer[16] = {}; // [状態] - 16 バイトのバッファーを用意する。
    int null_address_lock_result;
    int zero_size_lock_result;
    int null_address_unlock_result;
    int zero_size_unlock_result;

    // Pre-Assert

    // Act
    null_address_lock_result = cplat_memory_lock_range(
        nullptr, sizeof(buffer)); // [手順] - アドレスに nullptr を指定して cplat_memory_lock_range() を呼び出す。
    zero_size_lock_result =
        cplat_memory_lock_range(buffer, 0U); // [手順] - サイズに 0 を指定して cplat_memory_lock_range() を呼び出す。
    null_address_unlock_result = cplat_memory_unlock_range(
        nullptr, sizeof(buffer)); // [手順] - アドレスに nullptr を指定して cplat_memory_unlock_range() を呼び出す。
    zero_size_unlock_result = cplat_memory_unlock_range(
        buffer, 0U); // [手順] - サイズに 0 を指定して cplat_memory_unlock_range() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_address_lock_result); // [確認_異常系] - nullptr アドレスに対する cplat_memory_lock_range() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        zero_size_lock_result); // [確認_異常系] - サイズ 0 に対する cplat_memory_lock_range() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_address_unlock_result); // [確認_異常系] - nullptr アドレスに対する cplat_memory_unlock_range() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        zero_size_unlock_result); // [確認_異常系] - サイズ 0 に対する cplat_memory_unlock_range() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// ヒープ バッファーの range ロックと解除が成功することの確認
TEST_F(memoryLockTest, test_range_locks_and_unlocks_heap_buffer)
{
    // Arrange
#if defined(PLATFORM_LINUX)
    NiceMock<Mock_sys_mman> mock_mman;
#endif                            /* PLATFORM_LINUX */
    void *buffer = malloc(4096U); // [状態] - 4096 バイトのヒープ バッファーを確保する。
    ASSERT_NE(nullptr, buffer);   // [状態確認] - malloc() の戻り値が nullptr 以外であること。

    // Pre-Assert
#if defined(PLATFORM_LINUX)
    EXPECT_CALL(mock_mman, mlock(_, _, _, buffer, 4096U))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - mlock() が buffer、4096U を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - mlock() から 0 を返す。
    EXPECT_CALL(mock_mman, munlock(_, _, _, buffer, 4096U))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - munlock() が buffer、4096U を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - munlock() から 0 を返す。
#endif                        /* PLATFORM_LINUX */

    // Act
    int lock_result = cplat_memory_lock_range(
        buffer, 4096U); // [手順] - バッファーとサイズ 4096U を指定して cplat_memory_lock_range() を呼び出す。
    int unlock_result = cplat_memory_unlock_range(
        buffer, 4096U); // [手順] - バッファーとサイズ 4096U を指定して cplat_memory_unlock_range() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK,
              lock_result); // [確認_正常系] - cplat_memory_lock_range() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_OK,
              unlock_result); // [確認_正常系] - cplat_memory_unlock_range() の戻り値が CPLAT_OK であること。

    // Cleanup
    free(buffer);
}

#if defined(PLATFORM_LINUX)
// mlock と munlock の OS エラーがメモリ ロック結果へ変換されることの確認
TEST_F(memoryLockTest, test_range_maps_mlock_errors)
{
    // Arrange
    NiceMock<Mock_sys_mman> mock_mman;
    unsigned char buffer[32] = {};
    errno = ENOMEM;

    // Pre-Assert
    EXPECT_CALL(mock_mman, mlock(_, _, _, _, _))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - mlock() が 1 回呼び出されること。
                               // [Pre-Assert手順] - mlock() から -1 を返す。
    EXPECT_CALL(mock_mman, munlock(_, _, _, _, _))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - munlock() が 1 回呼び出されること。
                               // [Pre-Assert手順] - munlock() から -1 を返す。

    // Act
    int lock_result = cplat_memory_lock_range(
        buffer, sizeof(buffer)); // [手順] - mlock() が ENOMEM で失敗する状態で cplat_memory_lock_range() を呼び出す。
    errno = EACCES;
    int unlock_result = cplat_memory_unlock_range(
        buffer,
        sizeof(buffer)); // [手順] - munlock() が EACCES で失敗する状態で cplat_memory_unlock_range() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_LIMIT_EXCEEDED,
        lock_result); // [確認_異常系] - cplat_memory_lock_range() の戻り値が CPLAT_ERR_LIMIT_EXCEEDED であること。
    EXPECT_EQ(
        CPLAT_ERR_PERMISSION_DENIED,
        unlock_result); // [確認_異常系] - cplat_memory_unlock_range() の戻り値が CPLAT_ERR_PERMISSION_DENIED であること。
}

// mlockall の失敗が self lock の生成失敗として通知されることの確認
TEST_F(memoryLockTest, test_lock_self_reports_mlockall_failure)
{
    // Arrange
    NiceMock<Mock_sys_mman> mock_mman;
    NiceMock<Mock_cplat> mock_cplat;
    cplat_memory_lock_self_options options = {};
    cplat_memory_lock_scope *scope = nullptr;
    options.flags = CPLAT_MEMORY_LOCK_CURRENT;

    // Pre-Assert
    EXPECT_CALL(mock_mman, mlockall(_, _, _, _))
        .WillOnce(testing::Invoke(
            [](const char *, const int, const char *, const int)
            {
                errno = ENOMEM;
                return -1;
            })); // [Pre-Assert確認_異常系] - mlockall() が 1 回呼び出されること。
                 // [Pre-Assert手順] - mlockall() から、errno に ENOMEM を設定して -1 を返す。
    EXPECT_CALL(mock_cplat, cplat_call_once(_, _))
        .WillOnce(testing::Invoke(
            [](cplat_once_flag *flag, cplat_once_fn func)
            {
                func();
                cplat_atomic_store_i32(&flag->state, 2, CPLAT_MEMORY_ORDER_RELAXED);
            })); // [Pre-Assert確認_異常系] - cplat_call_once() が 1 回呼び出されること。
                 // [Pre-Assert手順] - cplat_call_once() から、渡された初期化関数を実行し、once 状態を完了にして戻る。
    EXPECT_CALL(mock_cplat, cplat_local_lock_create(_))
        .WillOnce(testing::Invoke(
            delegate_real_cplat_local_lock_create)); // [Pre-Assert確認_異常系] - cplat_local_lock_create() が 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_local_lock_create() から製品 (prod) の処理へ委譲する。
    EXPECT_CALL(mock_cplat, cplat_local_lock_lock(_, CPLAT_SYNC_WAIT_FOREVER))
        .WillOnce(Return(
            CPLAT_OK)); // [Pre-Assert確認_異常系] - cplat_local_lock_lock() が CPLAT_SYNC_WAIT_FOREVER を指定して 1 回呼び出されること。
                        // [Pre-Assert手順] - cplat_local_lock_lock() から CPLAT_OK を返す。
    EXPECT_CALL(mock_cplat, cplat_local_lock_unlock(_))
        .WillOnce(Return(CPLAT_OK)); // [Pre-Assert確認_異常系] - cplat_local_lock_unlock() が 1 回呼び出されること。
                                     // [Pre-Assert手順] - cplat_local_lock_unlock() から CPLAT_OK を返す。

    // Act
    int result = cplat_memory_lock_self(
        &options, &scope); // [手順] - mlockall() が ENOMEM で失敗する状態で cplat_memory_lock_self() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_LIMIT_EXCEEDED,
              result); // [確認_異常系] - cplat_memory_lock_self() の戻り値が CPLAT_ERR_LIMIT_EXCEEDED であること。
    EXPECT_EQ(nullptr,
              scope); // [確認_異常系] - scope が nullptr であること。(self lock の scope が生成されないことの確認)
}

// self lock の解除が munlockall のエラーを返すことの確認
TEST_F(memoryLockTest, test_scope_release_reports_munlockall_failure)
{
    // Arrange
    NiceMock<Mock_sys_mman> mock_mman;
    cplat_memory_lock_self_options options = {};
    cplat_memory_lock_scope *scope = nullptr;
    options.flags = CPLAT_MEMORY_LOCK_CURRENT;
    ON_CALL(mock_mman, mlockall(_, _, _, _))
        .WillByDefault(Return(0)); // [状態] - mlockall() から 0 を返すように既定動作を設定する。
    ASSERT_EQ(CPLAT_OK, cplat_memory_lock_self(&options, &scope)); // [状態] - self lock を生成する。
    // [状態確認] - cplat_memory_lock_self() の戻り値が CPLAT_OK であること。
    ASSERT_NE(nullptr, scope); // [状態確認] - scope が nullptr 以外であること。
    errno = EACCES;

    // Pre-Assert
    EXPECT_CALL(mock_mman, munlockall(_, _, _))
        .WillOnce(Return(-1)); // [Pre-Assert確認_異常系] - munlockall() が 1 回呼び出されること。
                               // [Pre-Assert手順] - munlockall() から -1 を返す。

    // Act
    int result = cplat_memory_lock_scope_release(
        scope); // [手順] - munlockall() が EACCES で失敗する状態で cplat_memory_lock_scope_release() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_PERMISSION_DENIED,
        result); // [確認_異常系] - cplat_memory_lock_scope_release() の戻り値が CPLAT_ERR_PERMISSION_DENIED であること。
}

// stack prefault の pthread_getattr_np 失敗が self lock の生成失敗になることの確認
TEST_F(memoryLockTest, test_lock_self_reports_stack_attribute_failure)
{
    // Arrange
    NiceMock<Mock_pthread> mock_pthread;
    cplat_memory_lock_self_options options = {};
    cplat_memory_lock_scope *scope = nullptr;
    options.flags = CPLAT_MEMORY_LOCK_CURRENT;
    options.stack_prefault_bytes = 1U;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_getattr_np(_, _, _, _, _))
        .WillOnce(Return(EINVAL)); // [Pre-Assert確認_異常系] - pthread_getattr_np() が 1 回呼び出されること。
                                   // [Pre-Assert手順] - pthread_getattr_np() から EINVAL を返す。

    // Act
    int result = cplat_memory_lock_self(
        &options, &scope); // [手順] - pthread_getattr_np() が失敗する状態で cplat_memory_lock_self() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - cplat_memory_lock_self() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        nullptr,
        scope); // [確認_異常系] - scope が nullptr であること。(stack prefault 失敗時に scope が生成されないことの確認)
}

// stack 情報の取得失敗と十分な stack 範囲を分類することの確認
TEST_F(memoryLockTest, test_prefault_stack_classifies_stack_ranges)
{
    // Arrange
    NiceMock<Mock_pthread> mock_pthread;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_getattr_np(_, _, _, _, _))
        .WillRepeatedly(Return(0)); // [Pre-Assert確認_正常系] - pthread_getattr_np() が任意の回数呼び出されること。
                                    // [Pre-Assert手順] - pthread_getattr_np() から 0 を返す。
    EXPECT_CALL(mock_pthread, pthread_attr_getstack(_, _, _, _, _, _))
        .WillOnce(Return(EINVAL))
        .WillOnce(DoAll(
            testing::Invoke(
                [](const char *, const int, const char *, const pthread_attr_t *, void **stack_addr, size_t *stack_size)
                {
                    *stack_addr = nullptr;
                    *stack_size = static_cast<size_t>(-1);
                }),
            Return(0))); // [Pre-Assert確認_正常系] - pthread_attr_getstack() が 2 回呼び出されること。
    // [Pre-Assert手順] - pthread_attr_getstack() から 1 回目は EINVAL、2 回目は十分な stack 範囲を設定して 0 を返す。
    EXPECT_CALL(mock_pthread, pthread_attr_destroy(_, _, _, _))
        .Times(2)
        .WillRepeatedly(Return(0)); // [Pre-Assert確認_正常系] - pthread_attr_destroy() が 2 回呼び出されること。
                                    // [Pre-Assert手順] - pthread_attr_destroy() から 0 を返す。

    // Act
    int failure_result = test_memory_lock_prefault_stack(
        1U); // [手順] - pthread_attr_getstack() が失敗する状態で test_memory_lock_prefault_stack() を呼び出す。
    int success_result = test_memory_lock_prefault_stack(
        1U); // [手順] - 十分な stack 範囲の状態で test_memory_lock_prefault_stack() を呼び出す。
    test_memory_lock_prefault_stack_recursive(
        4097U); // [手順] - 2 ページ分に相当する 4097U を指定して test_memory_lock_prefault_stack_recursive() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        failure_result); // [確認_異常系] - 属性取得失敗時の test_memory_lock_prefault_stack() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        CPLAT_OK,
        success_result); // [確認_正常系] - 十分な stack 範囲の test_memory_lock_prefault_stack() の戻り値が CPLAT_OK であること。
}

// 現在位置を含まない stack 範囲を制限超過として扱うことの確認
TEST_F(memoryLockTest, test_prefault_stack_rejects_range_above_current_position)
{
    // Arrange
    NiceMock<Mock_pthread> mock_pthread;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_getattr_np(_, _, _, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - pthread_getattr_np() が 1 回呼び出されること。
                              // [Pre-Assert手順] - pthread_getattr_np() から 0 を返す。
    EXPECT_CALL(mock_pthread, pthread_attr_getstack(_, _, _, _, _, _))
        .WillOnce(DoAll(
            testing::Invoke(
                [](const char *, const int, const char *, const pthread_attr_t *, void **stack_addr, size_t *stack_size)
                {
                    *stack_addr = reinterpret_cast<void *>(static_cast<uintptr_t>(-1));
                    *stack_size = 0U;
                }),
            Return(0))); // [Pre-Assert確認_異常系] - pthread_attr_getstack() が 1 回呼び出されること。
    // [Pre-Assert手順] - pthread_attr_getstack() から、現在位置より上の空 stack 範囲を設定して 0 を返す。
    EXPECT_CALL(mock_pthread, pthread_attr_destroy(_, _, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - pthread_attr_destroy() が 1 回呼び出されること。
                              // [Pre-Assert手順] - pthread_attr_destroy() から 0 を返す。

    // Act
    int result = test_memory_lock_prefault_stack(
        1U); // [手順] - 現在位置を含まない stack 範囲の状態で test_memory_lock_prefault_stack() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_LIMIT_EXCEEDED,
        result); // [確認_異常系] - test_memory_lock_prefault_stack() の戻り値が CPLAT_ERR_LIMIT_EXCEEDED であること。
}

// Linux の self lock flag を各 native flag へ変換することの確認
TEST_F(memoryLockTest, test_convert_flags_covers_each_flag_combination)
{
    // Arrange
    int native_flags = 0;

    // Pre-Assert

    // Act
    int zero_result = test_memory_lock_convert_flags(
        0, &native_flags); // [手順] - flag 0 を指定して test_memory_lock_convert_flags() を呼び出す。
    int unknown_result = test_memory_lock_convert_flags(
        0x100, &native_flags); // [手順] - 未定義 flag を指定して test_memory_lock_convert_flags() を呼び出す。
    int current_result = test_memory_lock_convert_flags(
        CPLAT_MEMORY_LOCK_CURRENT,
        &native_flags); // [手順] - CURRENT flag を指定して test_memory_lock_convert_flags() を呼び出す。
    int future_result = test_memory_lock_convert_flags(
        CPLAT_MEMORY_LOCK_FUTURE,
        &native_flags); // [手順] - FUTURE flag を指定して test_memory_lock_convert_flags() を呼び出す。
    int onfault_result = test_memory_lock_convert_flags(
        CPLAT_MEMORY_LOCK_ONFAULT,
        &native_flags); // [手順] - ONFAULT flag を指定して test_memory_lock_convert_flags() を呼び出す。
    int null_output_result = test_memory_lock_convert_flags(
        CPLAT_MEMORY_LOCK_CURRENT,
        nullptr); // [手順] - native flag の出力先に nullptr を指定して test_memory_lock_convert_flags() を呼び出す。

    // Assert
    EXPECT_EQ(
        -1,
        zero_result); // [確認_異常系] - flag 0 の test_memory_lock_convert_flags() の戻り値が -1 であること。(flag 0 を拒否することの確認)
    EXPECT_EQ(
        -1,
        unknown_result); // [確認_異常系] - 未定義 flag の test_memory_lock_convert_flags() の戻り値が -1 であること。(未定義 flag を拒否することの確認)
    EXPECT_EQ(
        0,
        current_result); // [確認_正常系] - CPLAT_MEMORY_LOCK_CURRENT の test_memory_lock_convert_flags() の戻り値が 0 であること。(CURRENT flag の変換が成功することの確認)
    EXPECT_EQ(
        0,
        future_result); // [確認_正常系] - CPLAT_MEMORY_LOCK_FUTURE の test_memory_lock_convert_flags() の戻り値が 0 であること。(FUTURE flag の変換が成功することの確認)
    EXPECT_EQ(
        0,
        onfault_result); // [確認_正常系] - CPLAT_MEMORY_LOCK_ONFAULT の test_memory_lock_convert_flags() の戻り値が 0 であること。(ONFAULT flag の変換が成功することの確認)
    EXPECT_EQ(
        0,
        null_output_result); // [確認_正常系] - 出力先に nullptr を指定した test_memory_lock_convert_flags() の戻り値が 0 であること。(出力先を省略した変換が成功することの確認)
}

// 内部 local lock の生成失敗と取得失敗を通知することの確認
TEST_F(memoryLockTest, test_internal_lock_classifies_initialization_and_lock_failures)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_local_lock *saved_lock = test_memory_lock_get_internal_lock();
    int saved_state = test_memory_lock_get_once_state();
    test_memory_lock_set_internal_lock(nullptr, 2);

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_local_lock_lock(_, CPLAT_SYNC_WAIT_FOREVER))
        .WillOnce(Return(
            CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_異常系] - cplat_local_lock_lock() が CPLAT_SYNC_WAIT_FOREVER を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_local_lock_lock() から CPLAT_ERR_UNKNOWN を返す。

    // Act
    int missing_result =
        test_memory_lock_internal_lock(); // [手順] - 内部 local lock が存在しない状態で test_memory_lock_internal_lock() を呼び出す。
    test_memory_lock_internal_unlock(); // [手順] - 内部 local lock が存在しない状態で test_memory_lock_internal_unlock() を呼び出す。
    test_memory_lock_set_internal_lock(reinterpret_cast<cplat_local_lock *>(1), 2);
    int lock_failure_result =
        test_memory_lock_internal_lock(); // [手順] - 内部 local lock の取得が失敗する状態で test_memory_lock_internal_lock() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        missing_result); // [確認_異常系] - 内部 local lock が存在しないときの test_memory_lock_internal_lock() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        lock_failure_result); // [確認_異常系] - 内部 local lock の取得失敗時の test_memory_lock_internal_lock() の戻り値が CPLAT_ERR_UNKNOWN であること。

    // Cleanup
    test_memory_lock_set_internal_lock(saved_lock, saved_state);
}

// self lock scope の内部状態不整合と参照数を分類することの確認
TEST_F(memoryLockTest, test_scope_release_classifies_internal_states)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_local_lock *saved_lock = test_memory_lock_get_internal_lock();
    int saved_state = test_memory_lock_get_once_state();
    test_memory_lock_set_internal_lock(reinterpret_cast<cplat_local_lock *>(1), 2);
    NiceMock<Mock_sys_mman> mock_mman;
    cplat_memory_lock_scope *unlocked_scope =
        test_memory_lock_create_scope(0); // [状態] - mlockall() 未実行の scope を用意する。
    cplat_memory_lock_scope *empty_count_scope =
        test_memory_lock_create_scope(1); // [状態] - 参照数確認用の mlockall() の scope を用意する。
    cplat_memory_lock_scope *shared_scope =
        test_memory_lock_create_scope(1); // [状態] - 複数参照確認用の mlockall() の scope を用意する。
    cplat_memory_lock_scope *lock_failure_scope =
        test_memory_lock_create_scope(1); // [状態] - 内部 lock 失敗確認用の scope を用意する。
    cplat_memory_lock_scope *last_scope =
        test_memory_lock_create_scope(1); // [状態] - 最後の参照確認用の mlockall() の scope を用意する。
    ASSERT_NE(
        nullptr,
        unlocked_scope); // [状態確認] - mlockall() 未実行の test_memory_lock_create_scope() の戻り値が nullptr 以外であること。
    ASSERT_NE(
        nullptr,
        empty_count_scope); // [状態確認] - 参照数確認用の test_memory_lock_create_scope() の戻り値が nullptr 以外であること。
    ASSERT_NE(
        nullptr,
        shared_scope); // [状態確認] - 複数参照確認用の test_memory_lock_create_scope() の戻り値が nullptr 以外であること。
    ASSERT_NE(
        nullptr,
        lock_failure_scope); // [状態確認] - 内部 lock 失敗確認用の test_memory_lock_create_scope() の戻り値が nullptr 以外であること。
    ASSERT_NE(
        nullptr,
        last_scope); // [状態確認] - 最後の参照確認用の test_memory_lock_create_scope() の戻り値が nullptr 以外であること。

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_local_lock_lock(_, CPLAT_SYNC_WAIT_FOREVER))
        .WillOnce(Return(CPLAT_OK))
        .WillOnce(Return(CPLAT_OK))
        .WillOnce(Return(CPLAT_OK))
        .WillOnce(Return(
            CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_異常系] - cplat_local_lock_lock() が CPLAT_SYNC_WAIT_FOREVER を指定して 4 回呼び出されること。
    // [Pre-Assert手順] - cplat_local_lock_lock() から 1 回目から 3 回目は CPLAT_OK、4 回目は CPLAT_ERR_UNKNOWN を返す。
    EXPECT_CALL(mock_cplat, cplat_local_lock_unlock(_))
        .Times(3)
        .WillRepeatedly(
            Return(CPLAT_OK)); // [Pre-Assert確認_正常系] - cplat_local_lock_unlock() が 3 回呼び出されること。
                               // [Pre-Assert手順] - cplat_local_lock_unlock() から CPLAT_OK を返す。
    EXPECT_CALL(mock_mman, munlockall(_, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_正常系] - munlockall() が 1 回呼び出されること。
                              // [Pre-Assert手順] - munlockall() から 0 を返す。

    // Act
    int unlocked_result = cplat_memory_lock_scope_release(
        unlocked_scope); // [手順] - mlockall() 未実行の scope を指定して cplat_memory_lock_scope_release() を呼び出す。
    test_memory_lock_set_scope_count(0U);
    int empty_count_result = cplat_memory_lock_scope_release(
        empty_count_scope); // [手順] - 参照数 0 の mlockall() の scope を指定して cplat_memory_lock_scope_release() を呼び出す。
    test_memory_lock_set_scope_count(2U);
    int shared_result = cplat_memory_lock_scope_release(
        shared_scope); // [手順] - 複数参照中の mlockall() の scope を指定して cplat_memory_lock_scope_release() を呼び出す。
    test_memory_lock_set_scope_count(1U);
    int last_result = cplat_memory_lock_scope_release(
        last_scope); // [手順] - 最後の参照である mlockall() の scope を指定して cplat_memory_lock_scope_release() を呼び出す。
    int lock_failure_result = cplat_memory_lock_scope_release(
        lock_failure_scope); // [手順] - 内部 local lock の取得が失敗する状態で scope を指定して cplat_memory_lock_scope_release() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        unlocked_result); // [確認_正常系] - mlockall() 未実行の scope に対する cplat_memory_lock_scope_release() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        empty_count_result); // [確認_異常系] - 参照数 0 の cplat_memory_lock_scope_release() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        CPLAT_OK,
        shared_result); // [確認_正常系] - 複数参照中の scope に対する cplat_memory_lock_scope_release() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        last_result); // [確認_正常系] - 最後の scope に対する cplat_memory_lock_scope_release() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_ERR_UNKNOWN,
        lock_failure_result); // [確認_異常系] - 内部 local lock の取得失敗中の cplat_memory_lock_scope_release() の戻り値が CPLAT_ERR_UNKNOWN であること。

    // Cleanup
    test_memory_lock_set_scope_count(0U);
    test_memory_lock_set_internal_lock(saved_lock, saved_state);
}

// self lock の内部 local lock 取得失敗を通知することの確認
TEST_F(memoryLockTest, test_lock_self_reports_internal_lock_failure)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    NiceMock<Mock_sys_mman> mock_mman;
    cplat_memory_lock_self_options options = {};
    cplat_memory_lock_scope *scope = nullptr;
    options.flags = CPLAT_MEMORY_LOCK_CURRENT;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_local_lock_lock(_, CPLAT_SYNC_WAIT_FOREVER))
        .WillOnce(Return(
            CPLAT_ERR_UNKNOWN)); // [Pre-Assert確認_異常系] - cplat_local_lock_lock() が CPLAT_SYNC_WAIT_FOREVER を指定して 1 回呼び出されること。
    // [Pre-Assert手順] - cplat_local_lock_lock() から、内部 lock の取得失敗として CPLAT_ERR_UNKNOWN を返す。
    EXPECT_CALL(mock_mman, mlockall(_, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - mlockall() が呼び出されないこと。

    // Act
    int result = cplat_memory_lock_self(
        &options, &scope); // [手順] - 内部 local lock の取得が失敗する状態で cplat_memory_lock_self() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - cplat_memory_lock_self() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        nullptr,
        scope); // [確認_異常系] - scope が nullptr であること。(内部 lock 取得失敗時に scope が nullptr のままであることの確認)
}

// self lock scope の確保失敗を通知することの確認
TEST_F(memoryLockTest, test_lock_self_reports_scope_allocation_failure)
{
    // Arrange
    NiceMock<Mock_cplat> mock_cplat;
    cplat_memory_lock_self_options options = {};
    cplat_memory_lock_scope *scope = nullptr;
    options.flags = CPLAT_MEMORY_LOCK_CURRENT;

    // Pre-Assert
    EXPECT_CALL(mock_cplat, cplat_calloc(_, _))
        .WillOnce(Return(nullptr)); // [Pre-Assert確認_異常系] - cplat_calloc() が 1 回呼び出されること。
                                    // [Pre-Assert手順] - cplat_calloc() から nullptr を返す。

    // Act
    int result = cplat_memory_lock_self(
        &options, &scope); // [手順] - scope のメモリ確保が失敗する状態で cplat_memory_lock_self() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN,
              result); // [確認_異常系] - cplat_memory_lock_self() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(
        nullptr,
        scope); // [確認_異常系] - scope が nullptr であること。(メモリ確保失敗時に scope が nullptr のままであることの確認)
}

// stack の取得範囲が安全余白を満たさない場合に制限超過を返すことの確認
TEST_F(memoryLockTest, test_lock_self_rejects_insufficient_stack_range)
{
    // Arrange
    NiceMock<Mock_pthread> mock_pthread;
    cplat_memory_lock_self_options options = {};
    cplat_memory_lock_scope *scope = nullptr;
    options.flags = CPLAT_MEMORY_LOCK_CURRENT;
    options.stack_prefault_bytes = 1U;

    // Pre-Assert
    EXPECT_CALL(mock_pthread, pthread_getattr_np(_, _, _, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - pthread_getattr_np() が 1 回呼び出されること。
                              // [Pre-Assert手順] - pthread_getattr_np() から 0 を返す。
    EXPECT_CALL(mock_pthread, pthread_attr_getstack(_, _, _, _, _, _))
        .WillOnce(DoAll(
            testing::Invoke(
                [](const char *, const int, const char *, const pthread_attr_t *, void **stack_addr, size_t *stack_size)
                {
                    *stack_addr = nullptr;
                    *stack_size = 1U;
                }),
            Return(0))); // [Pre-Assert確認_異常系] - pthread_attr_getstack() が 1 回呼び出されること。
    // [Pre-Assert手順] - pthread_attr_getstack() から、スタック先頭 nullptr、サイズ 1 を設定して 0 を返す。
    EXPECT_CALL(mock_pthread, pthread_attr_destroy(_, _, _, _))
        .WillOnce(Return(0)); // [Pre-Assert確認_異常系] - pthread_attr_destroy() が 1 回呼び出されること。
                              // [Pre-Assert手順] - pthread_attr_destroy() から 0 を返す。

    // Act
    int result = cplat_memory_lock_self(
        &options, &scope); // [手順] - スタック安全余白を満たさない状態で cplat_memory_lock_self() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_LIMIT_EXCEEDED,
              result); // [確認_異常系] - cplat_memory_lock_self() の戻り値が CPLAT_ERR_LIMIT_EXCEEDED であること。
    EXPECT_EQ(nullptr,
              scope); // [確認_異常系] - scope が nullptr であること。(スタック不足時に scope が生成されないことの確認)
}
#endif /* PLATFORM_LINUX */

// self ロック API が不正引数を検出することの確認
TEST_F(memoryLockTest, test_lock_self_rejects_invalid_arguments)
{
    // Arrange
    cplat_memory_lock_scope *scope = nullptr;
    cplat_memory_lock_self_options options = {};
    options.flags =
        CPLAT_MEMORY_LOCK_CURRENT; // [状態] - flags を CPLAT_MEMORY_LOCK_CURRENT としたオプションを用意する。
    int null_options_result;
    int null_output_result;
    int zero_flags_result;
    int unknown_flags_result;

    // Pre-Assert

    // Act
    null_options_result = cplat_memory_lock_self(
        nullptr, &scope); // [手順] - options に nullptr を指定して cplat_memory_lock_self() を呼び出す。
    null_output_result = cplat_memory_lock_self(
        &options, nullptr); // [手順] - 出力先に nullptr を指定して cplat_memory_lock_self() を呼び出す。
    options.flags = 0;
    zero_flags_result =
        cplat_memory_lock_self(&options, &scope); // [手順] - flags 0 を指定して cplat_memory_lock_self() を呼び出す。
    options.flags = CPLAT_MEMORY_LOCK_CURRENT | 0x100;
    unknown_flags_result = cplat_memory_lock_self(
        &options, &scope); // [手順] - 未定義フラグ 0x100 を指定して cplat_memory_lock_self() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_options_result); // [確認_異常系] - nullptr options に対する cplat_memory_lock_self() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        null_output_result); // [確認_異常系] - nullptr 出力先に対する cplat_memory_lock_self() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        zero_flags_result); // [確認_異常系] - flags 0 に対する cplat_memory_lock_self() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        unknown_flags_result); // [確認_異常系] - 未定義フラグ 0x100 に対する cplat_memory_lock_self() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// scope 解放が NULL を受理することの確認
TEST_F(memoryLockTest, test_scope_release_accepts_null)
{
    // Arrange

    // Pre-Assert

    // Act
    int actual_ret_memory_lock = cplat_memory_lock_scope_release(
        nullptr); // [手順] - scope に nullptr を指定して cplat_memory_lock_scope_release() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_memory_lock); // [確認_正常系] - cplat_memory_lock_scope_release() の戻り値が CPLAT_OK であること。
}

// 過大な stack_prefault_bytes が拒否されることの確認
TEST_F(memoryLockTest, test_lock_self_rejects_excessive_stack_prefault)
{
    // Arrange
    cplat_memory_lock_scope *scope = nullptr;
    cplat_memory_lock_self_options options = {};
    options.flags = CPLAT_MEMORY_LOCK_CURRENT;
    options.stack_prefault_bytes = static_cast<size_t>(-1); // [状態] - stack_prefault_bytes を過大な SIZE_MAX とする。

    // Pre-Assert

    // Act
    int actual_ret_memory_lock = cplat_memory_lock_self(
        &options, &scope); // [手順] - 過大な stack_prefault_bytes を指定して cplat_memory_lock_self() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_LIMIT_EXCEEDED,
        actual_ret_memory_lock); // [確認_異常系] - cplat_memory_lock_self() の戻り値が CPLAT_ERR_LIMIT_EXCEEDED であること。
    EXPECT_EQ(nullptr, scope); // [確認_異常系] - scope が nullptr であること。(scope が nullptr のままであることの確認)
}

// 複数スレッドから独立した self ロック scope を取得できることの確認
TEST_F(memoryLockTest, test_lock_self_allows_independent_scopes_from_multiple_threads)
{
    // Arrange
#if defined(PLATFORM_LINUX)
    NiceMock<Mock_sys_mman> mock_mman;
#endif /* PLATFORM_LINUX */
    self_lock_thread_result first = {};
    self_lock_thread_result second = {}; // [状態] - 2 スレッド分の結果格納先を用意する。

#if defined(PLATFORM_LINUX)
    ON_CALL(mock_mman, mlockall(_, _, _, _))
        .WillByDefault(Return(0)); // [状態] - mlockall() から 0 を返すように既定動作を設定する。
    ON_CALL(mock_mman, munlockall(_, _, _))
        .WillByDefault(Return(0)); // [状態] - munlockall() から 0 を返すように既定動作を設定する。
#endif                             /* PLATFORM_LINUX */

    // Pre-Assert

    // Act
    std::thread first_thread(lock_self_current_in_thread, &first);
    std::thread second_thread(
        lock_self_current_in_thread,
        &second); // [手順] - 各スレッドの結果格納先を指定して lock_self_current_in_thread() を呼び出す。
    first_thread.join();
    second_thread.join();

    // Assert
    if ((first.result != CPLAT_OK) || (second.result != CPLAT_OK))
    {

        // Cleanup
        (void)cplat_memory_lock_scope_release(first.scope);
        (void)cplat_memory_lock_scope_release(second.scope);
        GTEST_SKIP() << "self memory lock is not available in this environment";
    }

    // Assert
    ASSERT_NE(
        nullptr,
        first
            .scope); // [確認_正常系] - first.scope が nullptr 以外であること。(1 つ目のスレッドで scope が取得できることの確認)
    ASSERT_NE(
        nullptr,
        second
            .scope); // [確認_正常系] - second.scope が nullptr 以外であること。(2 つ目のスレッドで scope が取得できることの確認)

    // Act_2
    int first_release = CPLAT_ERR_UNKNOWN;
    int second_release = CPLAT_ERR_UNKNOWN;
    std::thread first_release_thread(release_self_scope_in_thread, first.scope, &first_release);
    std::thread second_release_thread(
        release_self_scope_in_thread, second.scope,
        &second_release); // [手順] - 各 scope と結果格納先を指定して release_self_scope_in_thread() を呼び出す。
    first_release_thread.join();
    second_release_thread.join();

    // Assert_2
    EXPECT_EQ(
        CPLAT_OK,
        first_release); // [確認_正常系] - 1 つ目の scope に対する cplat_memory_lock_scope_release() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(
        CPLAT_OK,
        second_release); // [確認_正常系] - 2 つ目の scope に対する cplat_memory_lock_scope_release() の戻り値が CPLAT_OK であること。
}

#if defined(PLATFORM_WINDOWS)
// Windows で未サポートのフラグが拒否されることの確認
TEST_F(memoryLockTest, test_lock_self_rejects_unsupported_flags_on_windows)
{
    // Arrange
    cplat_memory_lock_scope *scope = nullptr;
    cplat_memory_lock_self_options options = {}; // [状態] - 空のオプションを用意する。
    int future_result;
    int onfault_result;
    int combined_result;

    // Pre-Assert

    // Act
    options.flags = CPLAT_MEMORY_LOCK_FUTURE;
    future_result = cplat_memory_lock_self(
        &options, &scope); // [手順] - FUTURE フラグを指定して cplat_memory_lock_self() を呼び出す。
    options.flags = CPLAT_MEMORY_LOCK_ONFAULT;
    onfault_result = cplat_memory_lock_self(
        &options, &scope); // [手順] - ONFAULT フラグを指定して cplat_memory_lock_self() を呼び出す。
    options.flags = CPLAT_MEMORY_LOCK_CURRENT | CPLAT_MEMORY_LOCK_FUTURE;
    combined_result = cplat_memory_lock_self(
        &options, &scope); // [手順] - CURRENT と FUTURE の組み合わせを指定して cplat_memory_lock_self() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_UNSUPPORTED,
        future_result); // [確認_異常系] - FUTURE フラグに対する cplat_memory_lock_self() の戻り値が CPLAT_ERR_UNSUPPORTED であること。
    EXPECT_EQ(
        CPLAT_ERR_UNSUPPORTED,
        onfault_result); // [確認_異常系] - ONFAULT フラグに対する cplat_memory_lock_self() の戻り値が CPLAT_ERR_UNSUPPORTED であること。
    EXPECT_EQ(
        CPLAT_ERR_UNSUPPORTED,
        combined_result); // [確認_異常系] - CURRENT と FUTURE の組み合わせに対する cplat_memory_lock_self() の戻り値が CPLAT_ERR_UNSUPPORTED であること。
}
#endif
