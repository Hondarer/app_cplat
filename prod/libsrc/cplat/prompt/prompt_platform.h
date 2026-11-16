/**
 *******************************************************************************
 *  @file           prompt_platform.h
 *  @brief          プロンプトが共有する端末処理の契約を定義します。
 *******************************************************************************
 */
#ifndef PROMPT_PLATFORM_PRIVATE_H
#define PROMPT_PLATFORM_PRIVATE_H

#include <cplat/base/platform.h>
#include <cplat/prompt/prompt_edit.h>
#include <cplat/sync/atomic.h>
#if defined(PLATFORM_LINUX)
    #include <termios.h>
#elif defined(PLATFORM_WINDOWS)
    #include <cplat/base/windows_sdk.h>
    #include <cplat/sync/sync.h>
#endif

/** @brief 追加入力待ちと Windows 同期 I/O キャンセル再試行で共用する短い待機時間 [ms] です。 */
#define PROMPT_PLATFORM_SHORT_WAIT_MS 50

typedef enum prompt_platform_input
{
    PROMPT_PLATFORM_EOF = -1,
    PROMPT_PLATFORM_RESIZE = -2,
    PROMPT_PLATFORM_ERROR = -3,
    PROMPT_PLATFORM_TIMEOUT = -4,
    PROMPT_PLATFORM_CANCELED = -5
} prompt_platform_input;

/** 元の設定は復元成功まで保持します。同一プロセスで有効な入力は一つです。 */
typedef struct prompt_platform_context
{
#if defined(PLATFORM_LINUX)
    struct termios orig_term;
    int raw_active;
    int signal_active;
    int cancel_pipe[2];
    int cancel_pipe_active;
    cplat_atomic_i32 cancel_requested;
    char padding[4];
#elif defined(PLATFORM_WINDOWS)
    HANDLE stdin_handle;
    HANDLE input_thread;
    cplat_local_lock *input_lock;
    cplat_condvar *input_done_cv;
    cplat_internal_prompt_utf16_input windows_input;
    cplat_atomic_i32 cancel_requested;
    DWORD orig_in_mode;
    int raw_active;
    int input_active;
#else
    int raw_active;
#endif
} prompt_platform_context;

#ifdef __cplusplus
extern "C"
{
#endif
    /**
     *  @brief          標準入力と標準出力がともに端末なら 1 を返します。
     */
    int prompt_platform_is_tty(void);

    /**
     *  @brief          不明な寸法を 80 列・24 行で補います。
     *  @param[out]     cols_out 列数。NULL は指定できません。
     *  @param[out]     rows_out 行数。NULL は指定できません。
     */
    void prompt_platform_get_size(int *cols_out, int *rows_out);

    /**
     *  @brief          前回の復元未完項目を復元してから raw モードへ移行します。
     *  @param[in,out]  context ゼロ初期化済みの状態。NULL は指定できません。
     *  @return         成功は 0、復元済みの移行失敗は 1、復元未完は -1 です。
     */
    int prompt_platform_enter_raw(prompt_platform_context *context);

    /**
     *  @brief          全復元項目を試み、成功した項目だけ復元義務を解除します。
     *  @param[in,out]  context 保存状態。NULL は指定できません。
     *  @return         復元済みなら 0、復元未完なら -1 です。未完項目は次回または解放時に再試行します。
     */
    int prompt_platform_leave_raw(prompt_platform_context *context);

    /**
     *  @brief          入力を待ち、バイトまたは prompt_platform_input を返します。
     *  @param[in,out]  context 入力状態。NULL は指定できません。
     *  @return         0〜255、EOF、リサイズ確認契機、OS エラーのいずれかです。
     */
    int prompt_platform_read_char(prompt_platform_context *context);

    /**
     *  @brief          最大 50 [ms] の追加入力待ちを行います。
     *  @param[in,out]  context 入力状態。NULL は指定できません。
     *  @return         入力結果または PROMPT_PLATFORM_TIMEOUT です。OS エラーをタイムアウトと区別します。
     */
    int prompt_platform_read_char_nb(prompt_platform_context *context);

    /**
     *  @brief          入力待機の中止に対応したプラットフォーム状態を初期化します。
     *  @param[in,out]  context ゼロ初期化済みの状態です。NULL は指定できません。
     *  @return         成功は 0、失敗は -1 です。失敗時は確保した資源を解放します。
     */
    int prompt_platform_init_cancelable(prompt_platform_context *context);

    /**
     *  @brief          プラットフォーム固有の入力中止用資源を解放します。
     *  @param[in,out]  context 保存状態です。NULL も指定できます。
     */
    void prompt_platform_dispose(prompt_platform_context *context);

    /**
     *  @brief          進行中または後続の入力待機を中止し、破棄まで中止状態を維持します。
     *  @param[in,out]  context 入力状態です。
     *  @return         CPLAT_OK、CPLAT_ERR_INVALID_ARGUMENT、または CPLAT_ERR_UNKNOWN を返します。
     */
    int prompt_platform_stop_input(prompt_platform_context *context);

    /**
     *  @brief          入力中止状態が設定されているかを返します。
     *  @param[in]      context 入力状態です。
     *  @return         中止済みなら 1、それ以外は 0 を返します。
     */
    int prompt_platform_is_input_canceled(const prompt_platform_context *context);

    /**
     *  @brief          非 TTY 向けの共通 1 行入力を読み取ります。
     *  @param[in,out]  context  入力状態です。NULL は指定できません。
     *  @param[out]     buf      格納先バッファーです。NULL は指定できません。
     *  @param[in]      buf_size @p buf のバイト数です。
     *  @return         CPLAT_OK、CPLAT_ERR_EOF、CPLAT_ERR_CANCELED、CPLAT_ERR_BUFFER_TOO_SMALL、
     *                  CPLAT_ERR_INVALID_ARGUMENT、または CPLAT_ERR_UNKNOWN を返します。
     */
    int prompt_platform_read_line(prompt_platform_context *context, char *buf, size_t buf_size);

    /**
     *  @brief          OS の端末サイズを取得します。
     *  @param[out]     cols_out OS が返した列数です。
     *  @param[out]     rows_out OS が返した行数です。
     *  @return         成功は 0、失敗は -1 です。両引数は NULL 不可です。
     */
    int prompt_platform_os_get_size(int *cols_out, int *rows_out);

    /**
     *  @brief          OS の raw モード設定とリサイズ通知登録を行います。
     *  @param[in,out]  context 設定の保存先。NULL 不可です。
     *  @return         成功は 0、失敗は -1 です。変更済み項目には復元義務を残します。
     */
    int prompt_platform_os_enter_raw(prompt_platform_context *context);

    /**
     *  @brief          OS の保存設定とリサイズ通知を復元します。
     *  @param[in,out]  context 復元義務を保持する状態。NULL 不可です。
     *  @return         全項目の復元成功は 0、未完は -1 です。
     */
    int prompt_platform_os_leave_raw(prompt_platform_context *context);
#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* PROMPT_PLATFORM_PRIVATE_H */
