#include <cplat/base/platform.h>
#include <mock_cplat.h>

#if defined(PLATFORM_LINUX)

    #include <testfw.h>
    #include <sys/mock_resource.h>
    #include <cplat/runtime/stack_limit_linux.h>

    #include <cerrno>
    #include <cstddef>
    #include <cstdint>
    #include <csignal>
    #include <sys/resource.h>
    #include <unistd.h>

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;

namespace
{

constexpr rlim_t kSoftLimitBytes = 8U * 1024U * 1024U;
constexpr rlim_t kHardLimitBytes = 64U * 1024U * 1024U;

// 子プロセスで引き下げるソフト リミットと、引き下げた値を超えて消費するスタック サイズ。
// 消費量は、起動時のソフト リミットから決まるスタック下の間隔 (最小 128 MiB) に収める。
constexpr rlim_t kLoweredSoftLimitBytes = 2U * 1024U * 1024U;
constexpr size_t kConsumedStackBytes = 8U * 1024U * 1024U;
constexpr size_t kStackChunkBytes = 64U * 1024U;
constexpr size_t kPageBytes = 4096U;

// getrlimit() の出力へ指定した値を設定する処理を返す。
auto fill_limit(rlim_t soft_limit, rlim_t hard_limit)
{
    return [soft_limit, hard_limit](const char *, const int, const char *, int, struct rlimit *rlim)
    {
        rlim->rlim_cur = soft_limit;
        rlim->rlim_max = hard_limit;
        return 0;
    };
}

// 再帰呼び出しで、メイン スレッドのスタックを remaining_bytes だけ消費する。
// 各フレームのすべてのページへ書き込み、スタックを実際に拡張させる。
__attribute__((noinline)) int consume_stack(size_t remaining_bytes)
{
    volatile char buffer[kStackChunkBytes];

    for (size_t offset = 0U; offset < sizeof(buffer); offset += kPageBytes)
    {
        buffer[offset] = 1;
    }
    if (remaining_bytes <= sizeof(buffer))
    {
        return buffer[0];
    }
    // 戻り値へ buffer を使い、末尾呼び出しの最適化でフレームが再利用されないようにする。
    return consume_stack(remaining_bytes - sizeof(buffer)) + buffer[sizeof(buffer) - 1U];
}

// 子プロセスで、ソフト リミットを引き下げてからスタックを消費し、終了する。
// raise が 1 の場合は、消費の前に cplat_stack_limit_linux_raise_to_hard_limit() を呼び出す。
[[noreturn]] void lower_limit_and_consume_stack(int raise)
{
    struct rlimit core_limit = {0U, 0U};
    struct rlimit stack_limit;

    // スタック溢れで強制終了した場合にコア ファイルを残さない。
    (void)setrlimit(RLIMIT_CORE, &core_limit);
    if (getrlimit(RLIMIT_STACK, &stack_limit) != 0)
    {
        _exit(2);
    }
    stack_limit.rlim_cur = kLoweredSoftLimitBytes;
    if (setrlimit(RLIMIT_STACK, &stack_limit) != 0)
    {
        _exit(3);
    }
    if (raise && (cplat_stack_limit_linux_raise_to_hard_limit(nullptr, nullptr) != CPLAT_OK))
    {
        _exit(4);
    }
    (void)consume_stack(kConsumedStackBytes);
    _exit(0);
}

// ソフト リミットを引き下げてから引き上げられるだけのハード リミットがあるかを判定する。
bool has_room_to_raise()
{
    struct rlimit stack_limit;

    if (getrlimit(RLIMIT_STACK, &stack_limit) != 0)
    {
        return false;
    }
    return (stack_limit.rlim_max == RLIM_INFINITY) || (stack_limit.rlim_max >= 2U * kConsumedStackBytes);
}

} // namespace

class stackLimitTest : public Test
{
};

// 取得関数が nullptr の出力引数を OS を呼び出さずに拒否することの確認
TEST_F(stackLimitTest, get_rejects_null_outputs)
{
    // Arrange
    NiceMock<Mock_sys_resource> mock_sys_resource;
    uint64_t limit_bytes = 0U; // [状態] - 出力引数の格納先を用意する。
    cplat_error detail;        // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_sys_resource, getrlimit(_, _, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - getrlimit() が呼び出されないこと。

    // Act
    int actual_ret_soft = cplat_stack_limit_linux_get(
        nullptr, &limit_bytes, &detail); // [手順] - soft_limit_bytes_out に nullptr を指定して cplat_stack_limit_linux_get() を呼び出す。
    int actual_ret_hard = cplat_stack_limit_linux_get(
        &limit_bytes, nullptr, nullptr); // [手順] - hard_limit_bytes_out に nullptr を指定して cplat_stack_limit_linux_get() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_soft); // [確認_異常系] - nullptr の soft_limit_bytes_out に対する cplat_stack_limit_linux_get() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
    EXPECT_EQ(EINVAL, cplat_error_get_errno(&detail)); // [確認_異常系] - cplat_error_get_errno() の戻り値が EINVAL であること。
    EXPECT_EQ(
        CPLAT_ERR_INVALID_ARGUMENT,
        actual_ret_hard); // [確認_異常系] - nullptr の hard_limit_bytes_out に対する cplat_stack_limit_linux_get() の戻り値が CPLAT_ERR_INVALID_ARGUMENT であること。
}

// 取得した資源上限がバイト数で格納され、RLIM_INFINITY が上限なしの値へ変換されることの確認
// cplat-req: id=CPLAT-RUNTIME-FUNC-017; uuid=510a4a3d-f78c-4733-bcb6-782d017559f3
TEST_F(stackLimitTest, get_converts_infinity_to_unlimited)
{
    // Arrange
    NiceMock<Mock_sys_resource> mock_sys_resource;
    uint64_t soft_limit_bytes = 0U;
    uint64_t hard_limit_bytes = 0U; // [状態] - 出力引数の格納先を用意する。
    cplat_error detail;             // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_sys_resource, getrlimit(_, _, _, RLIMIT_STACK, _))
        .WillOnce(Invoke(fill_limit(
            kSoftLimitBytes,
            RLIM_INFINITY))); // [Pre-Assert確認_正常系] - getrlimit() が RLIMIT_STACK を指定して 1 回呼び出されること。
                              // [Pre-Assert手順] - getrlimit() からソフト リミット 8 MiB、ハード リミット RLIM_INFINITY を返す。

    // Act
    int actual_ret = cplat_stack_limit_linux_get(&soft_limit_bytes, &hard_limit_bytes,
                                                 &detail); // [手順] - cplat_stack_limit_linux_get() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - cplat_stack_limit_linux_get() の戻り値が CPLAT_OK であること。
    EXPECT_EQ((uint64_t)kSoftLimitBytes, soft_limit_bytes); // [確認_正常系] - soft_limit_bytes が 8 MiB であること。
    EXPECT_EQ(CPLAT_STACK_LIMIT_LINUX_UNLIMITED,
              hard_limit_bytes); // [確認_正常系] - hard_limit_bytes が CPLAT_STACK_LIMIT_LINUX_UNLIMITED であること。
    EXPECT_EQ(0, cplat_error_is_set(&detail)); // [確認_正常系] - cplat_error_is_set() の戻り値が 0 であること。(成功時に詳細エラーがクリアされたことの確認)
}

// getrlimit() の失敗が詳細エラーで通知され、出力引数が変更されないことの確認
TEST_F(stackLimitTest, get_reports_os_failure_without_changing_outputs)
{
    // Arrange
    NiceMock<Mock_sys_resource> mock_sys_resource;
    uint64_t soft_limit_bytes = 1U;
    uint64_t hard_limit_bytes = 2U; // [状態] - 出力引数の格納先を初期値 1 と 2 で用意する。
    cplat_error detail;             // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_sys_resource, getrlimit(_, _, _, RLIMIT_STACK, _))
        .WillOnce(DoAll(testing::Assign(&errno, EFAULT),
                        Return(-1))); // [Pre-Assert確認_異常系] - getrlimit() が RLIMIT_STACK を指定して 1 回呼び出されること。
                                      // [Pre-Assert手順] - getrlimit() から errno に EFAULT を設定して -1 を返す。

    // Act
    int actual_ret = cplat_stack_limit_linux_get(&soft_limit_bytes, &hard_limit_bytes,
                                                 &detail); // [手順] - cplat_stack_limit_linux_get() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret); // [確認_異常系] - cplat_stack_limit_linux_get() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(EFAULT, cplat_error_get_errno(&detail)); // [確認_異常系] - cplat_error_get_errno() の戻り値が EFAULT であること。
    EXPECT_EQ(1U, soft_limit_bytes); // [確認_異常系] - soft_limit_bytes が 1 のままであること。
    EXPECT_EQ(2U, hard_limit_bytes); // [確認_異常系] - hard_limit_bytes が 2 のままであること。
}

// ソフト リミットがハード リミットと同じ値で setrlimit() へ渡されることの確認
// cplat-req: id=CPLAT-RUNTIME-FUNC-017; uuid=510a4a3d-f78c-4733-bcb6-782d017559f3
TEST_F(stackLimitTest, raise_sets_soft_limit_to_hard_limit)
{
    // Arrange
    NiceMock<Mock_sys_resource> mock_sys_resource;
    uint64_t soft_limit_bytes = 0U;      // [状態] - 出力引数の格納先を用意する。
    uint64_t unlimited_soft_bytes = 0U;  // [状態] - 上限なしの場合の出力引数の格納先を用意する。
    cplat_error detail;                  // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_sys_resource, getrlimit(_, _, _, RLIMIT_STACK, _))
        .WillOnce(Invoke(fill_limit(kSoftLimitBytes, kHardLimitBytes)))
        .WillOnce(Invoke(fill_limit(
            kSoftLimitBytes,
            RLIM_INFINITY))); // [Pre-Assert確認_正常系] - getrlimit() が RLIMIT_STACK を指定して 2 回呼び出されること。
                              // [Pre-Assert手順] - getrlimit() から 1 回目はハード リミット 64 MiB、2 回目は RLIM_INFINITY を返す。ソフト リミットはいずれも 8 MiB とする。
    EXPECT_CALL(mock_sys_resource, setrlimit(_, _, _, RLIMIT_STACK, _))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, int, const struct rlimit *rlim)
            {
                EXPECT_EQ(kHardLimitBytes, rlim->rlim_cur); // [Pre-Assert確認_正常系] - 1 回目の rlim->rlim_cur が 64 MiB であること。
                EXPECT_EQ(kHardLimitBytes, rlim->rlim_max); // [Pre-Assert確認_正常系] - 1 回目の rlim->rlim_max が 64 MiB であること。
                return 0;
            }))
        .WillOnce(Invoke(
            [](const char *, const int, const char *, int, const struct rlimit *rlim)
            {
                EXPECT_EQ(RLIM_INFINITY, rlim->rlim_cur); // [Pre-Assert確認_正常系] - 2 回目の rlim->rlim_cur が RLIM_INFINITY であること。
                EXPECT_EQ(RLIM_INFINITY, rlim->rlim_max); // [Pre-Assert確認_正常系] - 2 回目の rlim->rlim_max が RLIM_INFINITY であること。
                return 0;
            })); // [Pre-Assert確認_正常系] - setrlimit() が RLIMIT_STACK を指定して 2 回呼び出されること。
                 // [Pre-Assert手順] - setrlimit() から 0 を返す。

    // Act
    int actual_ret = cplat_stack_limit_linux_raise_to_hard_limit(
        &soft_limit_bytes, &detail); // [手順] - ハード リミット 64 MiB の状態で cplat_stack_limit_linux_raise_to_hard_limit() を呼び出す。
    int actual_ret_unlimited = cplat_stack_limit_linux_raise_to_hard_limit(
        &unlimited_soft_bytes, nullptr); // [手順] - ハード リミット RLIM_INFINITY の状態で cplat_stack_limit_linux_raise_to_hard_limit() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_OK, actual_ret); // [確認_正常系] - 64 MiB に対する cplat_stack_limit_linux_raise_to_hard_limit() の戻り値が CPLAT_OK であること。
    EXPECT_EQ((uint64_t)kHardLimitBytes, soft_limit_bytes); // [確認_正常系] - soft_limit_bytes が 64 MiB であること。
    EXPECT_EQ(0, cplat_error_is_set(&detail)); // [確認_正常系] - cplat_error_is_set() の戻り値が 0 であること。(成功時に詳細エラーがクリアされたことの確認)
    EXPECT_EQ(
        CPLAT_OK,
        actual_ret_unlimited); // [確認_正常系] - RLIM_INFINITY に対する cplat_stack_limit_linux_raise_to_hard_limit() の戻り値が CPLAT_OK であること。
    EXPECT_EQ(CPLAT_STACK_LIMIT_LINUX_UNLIMITED,
              unlimited_soft_bytes); // [確認_正常系] - unlimited_soft_bytes が CPLAT_STACK_LIMIT_LINUX_UNLIMITED であること。
}

// ソフト リミットがすでにハード リミットと等しい場合に、setrlimit() を呼び出さずに省略されることの確認
// cplat-req: id=CPLAT-RUNTIME-FUNC-017; uuid=510a4a3d-f78c-4733-bcb6-782d017559f3
TEST_F(stackLimitTest, raise_skips_when_soft_limit_equals_hard_limit)
{
    // Arrange
    NiceMock<Mock_sys_resource> mock_sys_resource;
    uint64_t soft_limit_bytes = 0U; // [状態] - 出力引数の格納先を用意する。
    cplat_error detail;             // [状態] - 詳細エラーの格納先を用意する。
    cplat_error_capture_errno(&detail, EPERM); // [状態] - 詳細エラーに EPERM を設定しておく。

    // Pre-Assert
    EXPECT_CALL(mock_sys_resource, getrlimit(_, _, _, RLIMIT_STACK, _))
        .WillOnce(Invoke(fill_limit(
            kHardLimitBytes,
            kHardLimitBytes))); // [Pre-Assert確認_正常系] - getrlimit() が RLIMIT_STACK を指定して 1 回呼び出されること。
                                // [Pre-Assert手順] - getrlimit() からソフト リミットとハード リミットに 64 MiB を返す。
    EXPECT_CALL(mock_sys_resource, setrlimit(_, _, _, _, _))
        .Times(0); // [Pre-Assert確認_正常系] - setrlimit() が呼び出されないこと。

    // Act
    int actual_ret = cplat_stack_limit_linux_raise_to_hard_limit(
        &soft_limit_bytes, &detail); // [手順] - cplat_stack_limit_linux_raise_to_hard_limit() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_SKIPPED, actual_ret); // [確認_正常系] - cplat_stack_limit_linux_raise_to_hard_limit() の戻り値が CPLAT_SKIPPED であること。
    EXPECT_EQ((uint64_t)kHardLimitBytes, soft_limit_bytes); // [確認_正常系] - soft_limit_bytes が 64 MiB であること。
    EXPECT_EQ(0, cplat_error_is_set(&detail)); // [確認_正常系] - cplat_error_is_set() の戻り値が 0 であること。(省略時に詳細エラーがクリアされたことの確認)
}

// setrlimit() の権限不足が結果コードと詳細エラーで通知され、出力引数が変更されないことの確認
TEST_F(stackLimitTest, raise_reports_permission_denied)
{
    // Arrange
    NiceMock<Mock_sys_resource> mock_sys_resource;
    uint64_t soft_limit_bytes = 1U; // [状態] - 出力引数の格納先を初期値 1 で用意する。
    cplat_error detail;
    cplat_error last; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_sys_resource, getrlimit(_, _, _, RLIMIT_STACK, _))
        .WillOnce(Invoke(fill_limit(
            kSoftLimitBytes,
            kHardLimitBytes))); // [Pre-Assert確認_異常系] - getrlimit() が RLIMIT_STACK を指定して 1 回呼び出されること。
                                // [Pre-Assert手順] - getrlimit() からソフト リミット 8 MiB、ハード リミット 64 MiB を返す。
    EXPECT_CALL(mock_sys_resource, setrlimit(_, _, _, RLIMIT_STACK, _))
        .WillOnce(DoAll(testing::Assign(&errno, EPERM),
                        Return(-1))); // [Pre-Assert確認_異常系] - setrlimit() が RLIMIT_STACK を指定して 1 回呼び出されること。
                                      // [Pre-Assert手順] - setrlimit() から errno に EPERM を設定して -1 を返す。

    // Act
    int actual_ret = cplat_stack_limit_linux_raise_to_hard_limit(
        &soft_limit_bytes, &detail); // [手順] - cplat_stack_limit_linux_raise_to_hard_limit() を呼び出す。
    cplat_error_get_last(&last);     // [手順] - cplat_error_get_last() を呼び出す。

    // Assert
    EXPECT_EQ(
        CPLAT_ERR_PERMISSION_DENIED,
        actual_ret); // [確認_異常系] - cplat_stack_limit_linux_raise_to_hard_limit() の戻り値が CPLAT_ERR_PERMISSION_DENIED であること。
    EXPECT_EQ(EPERM, cplat_error_get_errno(&detail)); // [確認_異常系] - detail に対する cplat_error_get_errno() の戻り値が EPERM であること。
    EXPECT_EQ(
        EPERM,
        cplat_error_get_errno(&last)); // [確認_異常系] - last に対する cplat_error_get_errno() の戻り値が EPERM であること。(スレッド ローカルの直前値にも記録されたことの確認)
    EXPECT_EQ(1U, soft_limit_bytes); // [確認_異常系] - soft_limit_bytes が 1 のままであること。
}

// getrlimit() が失敗した場合に、setrlimit() を呼び出さずに失敗が通知されることの確認
TEST_F(stackLimitTest, raise_reports_get_failure_without_setting)
{
    // Arrange
    NiceMock<Mock_sys_resource> mock_sys_resource;
    cplat_error detail; // [状態] - 詳細エラーの格納先を用意する。

    // Pre-Assert
    EXPECT_CALL(mock_sys_resource, getrlimit(_, _, _, RLIMIT_STACK, _))
        .WillOnce(DoAll(testing::Assign(&errno, EFAULT),
                        Return(-1))); // [Pre-Assert確認_異常系] - getrlimit() が RLIMIT_STACK を指定して 1 回呼び出されること。
                                      // [Pre-Assert手順] - getrlimit() から errno に EFAULT を設定して -1 を返す。
    EXPECT_CALL(mock_sys_resource, setrlimit(_, _, _, _, _))
        .Times(0); // [Pre-Assert確認_異常系] - setrlimit() が呼び出されないこと。

    // Act
    int actual_ret = cplat_stack_limit_linux_raise_to_hard_limit(
        nullptr, &detail); // [手順] - soft_limit_bytes_out に nullptr を指定して cplat_stack_limit_linux_raise_to_hard_limit() を呼び出す。

    // Assert
    EXPECT_EQ(CPLAT_ERR_UNKNOWN, actual_ret); // [確認_異常系] - cplat_stack_limit_linux_raise_to_hard_limit() の戻り値が CPLAT_ERR_UNKNOWN であること。
    EXPECT_EQ(EFAULT, cplat_error_get_errno(&detail)); // [確認_異常系] - cplat_error_get_errno() の戻り値が EFAULT であること。
}

// 引き下げたソフト リミットを超えるスタック消費が、引き上げ前は強制終了し、引き上げ後は成功することの確認
// cplat-req: id=CPLAT-RUNTIME-FUNC-017; uuid=510a4a3d-f78c-4733-bcb6-782d017559f3
TEST_F(stackLimitTest, raise_allows_main_thread_stack_beyond_lowered_limit)
{
    // Arrange
    if (!has_room_to_raise())
    {
        GTEST_SKIP() << "the hard limit of RLIMIT_STACK is too small to raise";
    }
    // [状態] - 子プロセスで、ソフト リミットを 2 MiB へ引き下げてから 8 MiB のスタックを消費する。

    // Pre-Assert

    // Act
    // [手順] - 子プロセスで、引き上げずにスタックを消費する。
    // Assert
    EXPECT_EXIT(lower_limit_and_consume_stack(0), ::testing::KilledBySignal(SIGSEGV), "");
    // [確認_異常系] - 子プロセスが SIGSEGV で強制終了すること。(引き下げたソフト リミットがメイン スレッドのスタックを制限することの確認)

    // Act_2
    // [手順] - 子プロセスで、cplat_stack_limit_linux_raise_to_hard_limit() を呼び出してからスタックを消費する。
    // Assert_2
    EXPECT_EXIT(lower_limit_and_consume_stack(1), ::testing::ExitedWithCode(0), "");
    // [確認_正常系] - 子プロセスが終了コード 0 で終了すること。(引き上げがメイン スレッドのスタックの拡張に適用されることの確認)
}

#endif /* PLATFORM_LINUX */
