/**
 *******************************************************************************
 *  @file           prompt_state.h
 *  @brief          プロンプトの状態とプラットフォーム処理を内部実装間で共有します。
 *
 *  入力履歴、UTF-8 編集バッファー、端末の raw モードを管理する内部状態を定義します。
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef PROMPT_STATE_PRIVATE_H
#define PROMPT_STATE_PRIVATE_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <cplat/prompt/prompt.h>
#include <cplat/prompt/prompt_edit.h>
#include <cplat/base/platform.h>

#include "prompt_platform.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /* ---- 1 つの呼び出し元に対応する履歴コンテキスト ---- */
    typedef struct prompt_history_context
    {
        /* ポインター類を先に並べてパディングを排除 */
        const char *file; /* __FILE__ の文字列ポインター (コピー不要) */
        char **entries;   /* リング バッファー (history_max 個の char*) */
        char *saved_line; /* ブラウズ前の編集内容退避 (LINE_MAX バイト) */
        size_t count;     /* 有効エントリ数 */
        size_t head;      /* リング先頭インデックス (最古) */
        int line;         /* __LINE__ */
        int browse_idx;   /* ブラウズ中インデックス (-1 = 現在行) */
    } prompt_history_context;

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
        prompt_history_context *contexts; /* コンテキスト配列 (動的拡張) */
        size_t ctx_count;                 /* 現在のコンテキスト数 */
        size_t ctx_cap;                   /* contexts 配列の容量 */
        size_t history_max;               /* 各コンテキストの履歴最大数 */

        /* _readline_fmt 用プロンプト文字列バッファー (遅延 malloc、自動拡張) */
        char *prompt_fmt_buf;
        size_t prompt_fmt_cap;

        cplat_prompt_completion_fn completion_fn;
        void *completion_user_data;
        cplat_prompt_completions completions;
        size_t view_start;
        size_t cols;

        prompt_platform_context platform;
        int is_tty;
        char padding[4];
    };

#ifdef __cplusplus
}
#endif
#endif /* PROMPT_STATE_PRIVATE_H */
