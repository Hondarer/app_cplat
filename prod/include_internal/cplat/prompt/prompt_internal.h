/**
 *  @file           prompt_internal.h
 *  @brief          プロンプトの状態とプラットフォーム処理を内部実装間で共有します。
 *
 *  入力履歴、UTF-8 編集バッファー、端末の raw モードを管理する内部状態を定義します。
 *
 *  @hideincludedbygraph
 *
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CPLAT_PROMPT_INTERNAL_H
#define CPLAT_PROMPT_INTERNAL_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <cplat/prompt/prompt.h>
#include <cplat/base/platform.h>

#if defined(PLATFORM_LINUX)
    #include <termios.h>
    #include <unistd.h>
    #include <cplat/crt/select.h>
#elif defined(PLATFORM_WINDOWS)
    #include <cplat/base/windows_sdk.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /* ---- 1 つの呼び出し元に対応する履歴コンテキスト ---- */
    typedef struct cplat_internal_prompt_ctx
    {
        /* ポインター類を先に並べてパディングを排除 */
        const char *file; /* __FILE__ の文字列ポインター (コピー不要) */
        char **entries;   /* リング バッファー (history_max 個の char*) */
        char *saved_line; /* ブラウズ前の編集内容退避 (LINE_MAX バイト) */
        size_t count;     /* 有効エントリ数 */
        size_t head;      /* リング先頭インデックス (最古) */
        int line;         /* __LINE__ */
        int browse_idx;   /* ブラウズ中インデックス (-1 = 現在行) */
    } cplat_internal_prompt_ctx;

    /* ---- メイン ハンドル (不透明型の実体) ---- */
    struct cplat_prompt
    {
        /* 編集バッファー (readline_at 呼び出しごとに初期化) */
        char *edit_buf;         /* 編集中の入力バッファー */
        size_t edit_len;        /* 現在の文字数 (NUL 除く) */
        size_t edit_cap;        /* edit_buf の容量 */
        size_t input_max_bytes; /* edit_buf の最大容量 */
        size_t cursor;          /* カーソル位置 (バイト オフセット、0〜edit_len) */

        /* 履歴コンテキスト管理 */
        cplat_internal_prompt_ctx *contexts; /* コンテキスト配列 (動的拡張) */
        size_t ctx_count;                    /* 現在のコンテキスト数 */
        size_t ctx_cap;                      /* contexts 配列の容量 */
        size_t history_max;                  /* 各コンテキストの履歴最大数 */

        /* _readline_fmt 用プロンプト文字列バッファー (遅延 malloc、自動拡張) */
        char *prompt_fmt_buf;
        size_t prompt_fmt_cap;

        /* TTY 状態 */
        int is_tty;

#if defined(PLATFORM_LINUX)
        /* struct termios は 4 バイト アライン・60 バイト。raw_active 後に 4 バイトの
         * 末尾パディングが必要なため、明示メンバーで定義する。 */
        struct termios orig_term;
        int raw_active;
        char _pad[4]; /* 構造体末尾 8 バイト アライン用パディング */
#elif defined(PLATFORM_WINDOWS)
    /* HANDLE は 8 バイト アライン。is_tty (int) との間に 4 バイトのパディングが必要なため明示メンバーで定義する。 */
    char _pad[4]; /* HANDLE 前の 8 バイト アライン用パディング */
    HANDLE stdin_handle;
    DWORD orig_in_mode;
    int raw_active;
#endif
    };

    /* ---- プラットフォーム抽象インターフェース (各 _platform.c で実装) ---- */

    /**
     *  @brief          端末を 1 バイト単位で入力できる raw モードへ移行します。
     *  @param[in]      p  プロンプト ハンドルです。
     *
     *  移行に成功した場合は @p p の raw モード状態を有効にし、復元用の端末設定を保存します。
     */
    void cplat_internal_prompt_platform_enter_raw(cplat_prompt *p);

    /**
     *  @brief          raw モードを解除して保存済みの端末設定を復元します。
     *  @param[in]      p  プロンプト ハンドルです。
     */
    void cplat_internal_prompt_platform_leave_raw(cplat_prompt *p);

    /**
     *  @brief          標準入力から 1 バイトを待機して読み取ります。
     *  @param[in]      p  プロンプト ハンドルです。
     *  @return         読み取った 0 以上のバイト値を返します。EOF または読み取り失敗の場合は -1 を返します。
     */
    int cplat_internal_prompt_platform_read_char(cplat_prompt *p);

    /**
     *  @brief          標準入力から 1 バイトを最大 50 ミリ秒待って読み取ります。
     *  @param[in]      p  プロンプト ハンドルです。
     *  @return         読み取った 0 以上のバイト値を返します。タイムアウト、EOF、または読み取り失敗の場合は
     *                  -1 を返します。
     */
    int cplat_internal_prompt_platform_read_char_nb(cplat_prompt *p);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CPLAT_PROMPT_INTERNAL_H */
