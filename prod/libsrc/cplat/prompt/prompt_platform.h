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
#if defined(PLATFORM_LINUX)
    #include <termios.h>
#elif defined(PLATFORM_WINDOWS)
    #include <cplat/base/windows_sdk.h>
#endif

typedef enum prompt_platform_input
{
    PROMPT_PLATFORM_EOF = -1,
    PROMPT_PLATFORM_RESIZE = -2,
    PROMPT_PLATFORM_ERROR = -3,
    PROMPT_PLATFORM_TIMEOUT = -4
} prompt_platform_input;

/** 元の設定は復元成功まで保持します。同一プロセスで有効な入力は一つです。 */
typedef struct prompt_platform_context
{
#if defined(PLATFORM_LINUX)
    struct termios orig_term;
    int raw_active;
    int signal_active;
    char padding[4];
#elif defined(PLATFORM_WINDOWS)
    cplat_internal_prompt_utf16_input windows_input;
    HANDLE stdin_handle;
    DWORD orig_in_mode;
    int raw_active;
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
     *  @brief          最大 50 ms の追加入力待ちを行います。
     *  @param[in,out]  context 入力状態。NULL は指定できません。
     *  @return         入力結果または PROMPT_PLATFORM_TIMEOUT です。OS エラーをタイムアウトと区別します。
     */
    int prompt_platform_read_char_nb(prompt_platform_context *context);
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
#endif
#endif
