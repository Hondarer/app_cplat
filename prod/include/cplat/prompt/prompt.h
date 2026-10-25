/**
 *******************************************************************************
 *  @file           prompt.h
 *  @brief          対話的な 1 行入力を行う汎用プロンプト API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/04/30
 *  @version        1.0.0
 *
 *  対話的な 1 行入力を提供します。\n
 *  TTY (対話端末) では次のキー操作を使用できます。
 *  - 上/下矢印キー : 入力履歴を遡る/進む
 *  - 左/右矢印キー : カーソル移動
 *  - Home / End    : 行頭/行末へ移動
 *  - BackSpace     : カーソル前の文字を削除
 *  - Delete        : カーソル上の文字を削除
 *  - Ctrl+C        : 入力中断 (@ref CPLAT_ERR_CANCELED を返す)
 *  - Enter         : 確定
 *
 *  TTY でない場合 (パイプ・リダイレクト等)、または raw モードへの移行に失敗し変更済みの端末設定と
 *  シグナル ハンドラーを復元できた場合は @ref cplat_fgets にフォールバックします。\n
 *
 *  @par 使用例 (固定プロンプト)
    @code{.c}
    #include <cplat/prompt/prompt.h>

    int main(void) {
        char buf[256];
        cplat_prompt *prompt = cplat_prompt_create(NULL);
        while (cplat_prompt_readline(prompt, buf, sizeof(buf), ">> ") == CPLAT_OK) {
            printf("入力: %s\n", buf);
        }
        cplat_prompt_dispose(prompt);
        return 0;
    }
    @endcode
 *
 *  @par 使用例 (フォーマット プロンプト)
    @code{.c}
    while (cplat_prompt_readline_fmt(prompt, buf, sizeof(buf),
                                        "[%s]> ", state_name) == CPLAT_OK) {
        // ...
    }
    @endcode
 *
 *  @par 複数箇所からの使用
 *  同一ハンドルを複数箇所で呼び出すと、呼び出し元のファイル名・行番号ごとに
 *  独立した履歴が自動的に割り当てられます。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CPLAT_PROMPT_H
#define CPLAT_PROMPT_H

#include <stddef.h>
#include <cplat/base/platform.h>
#include <cplat/base/compiler.h>
#include <cplat/base/result.h>
#include <cplat/cplat_export.h>

/**
 *  @ingroup        CPLAT_PROMPT
 *  @{
 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/**
 *  @brief  各コンテキストで保持する履歴エントリ数の既定値です。
 */
#define CPLAT_PROMPT_HISTORY_DEFAULT 64

/**
 *  @brief  NUL 終端を含む入力バッファーの既定最大バイト数です。
 */
#define CPLAT_PROMPT_INPUT_BYTES_DEFAULT 4096

    /**
     *  @brief  プロンプトを操作する不透明ハンドルです。
     */
    typedef struct cplat_prompt cplat_prompt;

    /** @brief 補完関数の呼び出し中だけ有効な候補の受け取り先です。 */
    typedef struct cplat_prompt_completions cplat_prompt_completions;

    /**
     *  @brief          入力が空でない行末で、文字入力、削除、候補の確定後に候補を返します。
     *  @param[in]      line NUL 終端の入力です。末尾の UTF-8 文字が完成したときだけ呼ばれます。
     *  @param[in]      cursor 行末のバイト位置です。
     *  @param[in,out]  completions 候補の受け取り先です。呼び出しの間だけ有効です。
     *  @param[in,out]  user_data 登録時の値です。
     *
     *  入力と出力が端末の場合だけ呼ばれます。履歴、初期値、カーソル移動、Esc の直後は呼びません。
     *  pinned_prompt では mutex を保持したまま呼ばれます。補完関数から pinned_prompt の API を呼ばないでください。
     *  他スレッドの出力を待たせるため、短時間で処理を終えてください。
     */
    typedef void (*cplat_prompt_completion_fn)(const char *line, size_t cursor, cplat_prompt_completions *completions,
                                               void *user_data);

    /**
     *  @brief          入力済みの部分を含む候補を追加します。
     *  @param[in,out]  completions 補完関数へ渡された受け取り先です。NULL は指定できません。
     *  @param[in]      start 置換開始位置のバイト位置です。
     *  @param[in]      candidate NUL 終端の候補です。複製するため、戻った後に解放できます。
     *  @retval         CPLAT_OK 受け付けました。同じ開始位置と文字列は 1 件として扱います。
     *  @retval         CPLAT_ERR_INVALID_ARGUMENT NULL、行末より後または UTF-8 文字途中の開始位置、
     *                  制御文字 (0x01〜0x1F、0x7F)、入力済み部分と先頭が一致しない候補です。
     *  @retval         CPLAT_ERR_BUFFER_TOO_SMALL 置換後の入力が NUL 終端込みで上限を超えます。
     *  @retval         CPLAT_ERR_OUT_OF_MEMORY 1 件目の保持領域を確保できません。
     *
     *  NUL は候補の終端です。不正な候補は件数に数えません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_prompt_completions_add(cplat_prompt_completions *completions, size_t start,
                                                            const char *candidate);

    /**
     *  @brief          補完関数を登録します。唯一の候補の続きを灰色で表示し、Tab または行末の → で補います。
     *  @param[in,out]  prompt ハンドルです。
     *  @param[in]      fn 補完関数です。NULL で解除します。
     *  @param[in,out]  user_data 補完関数へ渡す値です。所有権は移りません。
     *  @retval         CPLAT_OK 登録または解除しました。
     *  @retval         CPLAT_ERR_INVALID_ARGUMENT ハンドルが NULL です。
     *  @par            スレッド セーフ
     *  同じハンドルへの同時呼び出しには対応しません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_prompt_completion_set(cplat_prompt *prompt, cplat_prompt_completion_fn fn,
                                                           void *user_data);

    /**
     *  @brief  プロンプトの生成オプションです。
     */
    typedef struct cplat_prompt_options
    {
        /**
         *  @brief  将来拡張用のフラグです。現時点では 0 を指定してください。
         */
        unsigned int flags;

        /**
         *  @brief  構造体配置用の予約領域です。現時点では 0 を指定してください。
         */
        unsigned int reserved;

        /**
         *  @brief  各コンテキストで保持する履歴エントリ数の上限です。
         *          0 の場合は `CPLAT_PROMPT_HISTORY_DEFAULT` を使用します。
         */
        size_t history_max;

        /**
         *  @brief  NUL 終端を含む入力編集バッファーの初期バイト数です。
         *          0 の場合は実装の既定値を使用します。
         */
        size_t input_initial_capacity;

        /**
         *  @brief  NUL 終端を含む入力編集バッファーの最大バイト数です。
         *          0 の場合は `CPLAT_PROMPT_INPUT_BYTES_DEFAULT` を使用します。
         */
        size_t input_max_bytes;
    } cplat_prompt_options;

    /**
     *  @brief          プロンプト ハンドルを生成します。
     *  @param[in]      options  生成オプションです。NULL の場合は既定設定を使用します。
     *  @return         成功時は生成したハンドルを返します。メモリを確保できない場合は NULL を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT cplat_prompt *CPLAT_API cplat_prompt_create(const cplat_prompt_options *options);

    /**
     *  @brief          プロンプト ハンドルを解放します。
     *  @param[in]      prompt  cplat_prompt_create() が返したハンドルです。NULL も指定できます。
     *
     *  未復元の端末設定または Linux の SIGWINCH ハンドラーが残っている場合は、
     *  最善努力で再度復元を試みてから資源を解放します。\n
     *  本関数の戻り値は void であり、OS が復元を拒み続けた場合の復元成功は保証せず、
     *  失敗も通知できません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  解放対象の @p prompt を他スレッドが使用していないことを呼び出し側で保証してください。
     */
    CPLAT_EXPORT void CPLAT_API cplat_prompt_dispose(cplat_prompt *prompt);

/**
 *  @brief          固定プロンプト文字列を表示して 1 行入力を受け取ります。
 *  @param[in]      p           プロンプト ハンドルです。
 *  @param[out]     buf         入力結果を格納するバッファーです。終端の改行は格納しません。
 *  @param[in]      buf_size    @p buf のバイト数です。
 *  @param[in]      prompt_str  表示するプロンプト文字列です。NULL の場合は空文字列として扱います。
 *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_EOF 、@ref CPLAT_ERR_CANCELED 、
 *                  @ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_BUFFER_TOO_SMALL 、
 *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
 *
 *  端末サイズの取得に成功した場合は幅と高さを独立に判定し、0 以下の幅は 80 列、
 *  0 以下の高さは 24 行を既定値とします。サイズ取得自体に失敗した場合は 80 列・24 行とします。\n
 *  非 TTY の場合、または raw モードへの移行に失敗し変更済みの端末設定と Linux の SIGWINCH
 *  ハンドラーを復元できた場合は、@ref cplat_fgets の行入力へフォールバックします。\n
 *  フォールバック時に入力行が @p buf に収まらない場合は @ref CPLAT_ERR_BUFFER_TOO_SMALL を返し、
 *  @p buf は空文字列になります。行の残りは次の呼び出しで取得できます。\n
 *  OS の入力エラーは @ref CPLAT_ERR_EOF と区別して @ref CPLAT_ERR_UNKNOWN を返します。\n
 *  端末設定または SIGWINCH ハンドラーの復元に失敗した場合は、行入力へフォールバックせず、
 *  入力結果より優先して @ref CPLAT_ERR_UNKNOWN を返します。\n
 *  復元できなかった項目は保持し、次回の入力開始前または cplat_prompt_dispose() 時に再試行します。
 *
 *  @par            スレッド セーフ
 *  本マクロはスレッド セーフではありません。\n
 *  標準入力の端末設定と、Linux の SIGWINCH のハンドラーは、プロセス全体で共有します。\n
 *  同一プロセス内で通常プロンプトと固定プロンプトは同時に有効になりません。\n
 *  そのため、異なるハンドルへの呼び出しも含め、
 *  入力を受け付ける関数 (cplat_pinned_prompt の入力関数を含む) の呼び出しを、プロセス全体で直列化してください。
 */
#define cplat_prompt_readline(p, buf, buf_size, prompt_str) \
    cplat_prompt_readline_at((p), (buf), (buf_size), (prompt_str), __FILE__, __LINE__)

/**
 *  @brief          printf 形式でプロンプトを生成して 1 行入力を受け取ります。
 *  @param[in]      p         プロンプト ハンドルです。
 *  @param[out]     buf       入力結果を格納するバッファーです。終端の改行は格納しません。
 *  @param[in]      buf_size  @p buf のバイト数です。
 *  @param[in]      fmt       printf 形式の書式文字列です。NULL の場合は空文字列として扱います。
 *  @param[in]      ...       @p fmt に対応する書式引数です。
 *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_EOF 、@ref CPLAT_ERR_CANCELED 、
 *                  @ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_BUFFER_TOO_SMALL 、
 *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
 *
 *  端末サイズの取得に成功した場合は幅と高さを独立に判定し、0 以下の幅は 80 列、
 *  0 以下の高さは 24 行を既定値とします。サイズ取得自体に失敗した場合は 80 列・24 行とします。\n
 *  非 TTY の場合、または raw モードへの移行に失敗し変更済みの端末設定と Linux の SIGWINCH
 *  ハンドラーを復元できた場合は、@ref cplat_fgets の行入力へフォールバックします。\n
 *  フォールバック時に入力行が @p buf に収まらない場合は @ref CPLAT_ERR_BUFFER_TOO_SMALL を返し、
 *  @p buf は空文字列になります。行の残りは次の呼び出しで取得できます。\n
 *  OS の入力エラーは @ref CPLAT_ERR_EOF と区別して @ref CPLAT_ERR_UNKNOWN を返します。\n
 *  端末設定または SIGWINCH ハンドラーの復元に失敗した場合は、行入力へフォールバックせず、
 *  入力結果より優先して @ref CPLAT_ERR_UNKNOWN を返します。\n
 *  復元できなかった項目は保持し、次回の入力開始前または cplat_prompt_dispose() 時に再試行します。
 *
 *  プロンプト文字列バッファーはハンドル内に保持し、必要に応じて自動拡張します。
 *
 *  @par            スレッド セーフ
 *  本マクロはスレッド セーフではありません。\n
 *  標準入力の端末設定と、Linux の SIGWINCH のハンドラーは、プロセス全体で共有します。\n
 *  同一プロセス内で通常プロンプトと固定プロンプトは同時に有効になりません。\n
 *  そのため、異なるハンドルへの呼び出しも含め、
 *  入力を受け付ける関数 (cplat_pinned_prompt の入力関数を含む) の呼び出しを、プロセス全体で直列化してください。
 */
#define cplat_prompt_readline_fmt(p, buf, buf_size, fmt, ...) \
    cplat_prompt_readline_fmt_at((p), (buf), (buf_size), __FILE__, __LINE__, (fmt), ##__VA_ARGS__)

/**
 *  @brief          入力欄に初期値を入れた状態で 1 行入力を受け取ります。
 *  @param[in]      p             プロンプト ハンドルです。
 *  @param[out]     buf           入力結果を格納するバッファーです。終端の改行は格納しません。
 *  @param[in]      buf_size      @p buf のバイト数です。
 *  @param[in]      prompt_str    表示するプロンプト文字列です。NULL の場合は空文字列として扱います。
 *  @param[in]      initial_text  入力欄の初期値です。NULL と空文字列は、初期値なしとして扱います。
 *  @return         cplat_prompt_readline() と同じ結果コードに加え、
 *                  初期値が不正な場合は @ref CPLAT_ERR_INVALID_ARGUMENT 、
 *                  初期値が入力欄の上限を超える場合は @ref CPLAT_ERR_BUFFER_TOO_SMALL 、
 *                  初期値のためのメモリを確保できない場合は @ref CPLAT_ERR_OUT_OF_MEMORY を返します。
 *
 *  非 TTY の場合、または raw モードへの移行に失敗し変更済みの端末設定と Linux の SIGWINCH
 *  ハンドラーを復元できた場合は、初期値を適用せず @ref cplat_fgets の行入力へフォールバックします。\n
 *  OS の入力エラーは @ref CPLAT_ERR_EOF と区別して @ref CPLAT_ERR_UNKNOWN を返します。\n
 *  端末設定または SIGWINCH ハンドラーの復元に失敗した場合は、行入力へフォールバックせず、
 *  入力結果より優先して @ref CPLAT_ERR_UNKNOWN を返します。\n
 *  復元できなかった項目は保持し、次回の入力開始前または cplat_prompt_dispose() 時に再試行します。\n
 *  詳細は cplat_prompt_readline_with_initial_at() を参照してください。
 *
 *  @par            スレッド セーフ
 *  本マクロはスレッド セーフではありません。\n
 *  標準入力の端末設定と、Linux の SIGWINCH のハンドラーは、プロセス全体で共有します。\n
 *  同一プロセス内で通常プロンプトと固定プロンプトは同時に有効になりません。\n
 *  そのため、異なるハンドルへの呼び出しも含め、
 *  入力を受け付ける関数 (cplat_pinned_prompt の入力関数を含む) の呼び出しを、プロセス全体で直列化してください。
 */
#define cplat_prompt_readline_with_initial(p, buf, buf_size, prompt_str, initial_text) \
    cplat_prompt_readline_with_initial_at((p), (buf), (buf_size), (prompt_str), (initial_text), __FILE__, __LINE__)

    /**
     *  @brief          呼び出し元を明示して 1 行入力を受け取ります。
     *
     *  通常は cplat_prompt_readline() を使用してください。
     *
     *  @param[in]      prompt      プロンプト ハンドルです。
     *  @param[out]     buf         入力結果を格納するバッファーです。
     *  @param[in]      buf_size    @p buf のバイト数です。
     *  @param[in]      prompt_str  表示するプロンプト文字列です。NULL の場合は空文字列として扱います。
     *  @param[in]      file        履歴を識別する呼び出し元ファイル名です。
     *  @param[in]      line        履歴を識別する呼び出し元行番号です。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_EOF 、@ref CPLAT_ERR_CANCELED 、
     *                  @ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_BUFFER_TOO_SMALL 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  端末サイズの取得に成功した場合は幅と高さを独立に判定し、0 以下の幅は 80 列、
     *  0 以下の高さは 24 行を既定値とします。サイズ取得自体に失敗した場合は 80 列・24 行とします。\n
     *  非 TTY の場合、または raw モードへの移行に失敗し変更済みの端末設定と Linux の SIGWINCH
     *  ハンドラーを復元できた場合は、@ref cplat_fgets の行入力へフォールバックします。\n
     *  フォールバック時に入力行が @p buf に収まらない場合は @ref CPLAT_ERR_BUFFER_TOO_SMALL を返し、
     *  @p buf は空文字列になります。行の残りは次の呼び出しで取得できます。\n
     *  OS の入力エラーは @ref CPLAT_ERR_EOF と区別して @ref CPLAT_ERR_UNKNOWN を返します。\n
     *  端末設定または SIGWINCH ハンドラーの復元に失敗した場合は、行入力へフォールバックせず、
     *  入力結果より優先して @ref CPLAT_ERR_UNKNOWN を返します。\n
     *  復元できなかった項目は保持し、次回の入力開始前または cplat_prompt_dispose() 時に再試行します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  標準入力の端末設定と、Linux の SIGWINCH のハンドラーは、プロセス全体で共有します。\n
     *  同一プロセス内で通常プロンプトと固定プロンプトは同時に有効になりません。\n
     *  そのため、異なるハンドルへの呼び出しも含め、
     *  入力を受け付ける関数 (cplat_pinned_prompt の入力関数を含む) の呼び出しを、プロセス全体で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_prompt_readline_at(cplat_prompt *prompt, char *buf, size_t buf_size,
                                                        const char *prompt_str, const char *file, int line);

    /**
     *  @brief          呼び出し元を明示し、入力欄に初期値を入れた状態で 1 行入力を受け取ります。
     *
     *  通常は cplat_prompt_readline_with_initial() を使用してください。
     *
     *  @param[in]      prompt        プロンプト ハンドルです。
     *  @param[out]     buf           入力結果を格納するバッファーです。
     *  @param[in]      buf_size      @p buf のバイト数です。
     *  @param[in]      prompt_str    表示するプロンプト文字列です。NULL の場合は空文字列として扱います。
     *  @param[in]      initial_text  入力欄の初期値です。NULL と空文字列は、初期値なしとして扱います。
     *  @param[in]      file          履歴を識別する呼び出し元ファイル名です。
     *  @param[in]      line          履歴を識別する呼び出し元行番号です。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_EOF 、@ref CPLAT_ERR_CANCELED 、
     *                  @ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_BUFFER_TOO_SMALL 、
     *                  @ref CPLAT_ERR_OUT_OF_MEMORY 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  入力欄に @p initial_text を入れ、カーソルを末尾に置いてから入力を受け付けます。\n
     *  利用者は初期値を編集して確定するか、そのまま確定できます。\n
     *  履歴をさかのぼったあとに最新の側へ戻ると、初期値 (編集した場合はその内容) へ戻ります。
     *
     *  初期値は、端末かどうかによらず次の規則で検証し、受け入れられない場合は入力を受け付けずに戻ります。
     *  - 改行などの制御文字 (0x00 から 0x1F、および 0x7F) を含む場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *    入力欄は 1 行のためです。
     *  - NUL 終端を含めて入力欄の上限 (@ref cplat_prompt_options::input_max_bytes) を超える場合は
     *    @ref CPLAT_ERR_BUFFER_TOO_SMALL を返します。
     *    途中で切り詰めると、UTF-8 の文字の途中で切れた値を編集させることになるためです。
     *
     *  端末サイズの取得に成功した場合は幅と高さを独立に判定し、0 以下の幅は 80 列、
     *  0 以下の高さは 24 行を既定値とします。サイズ取得自体に失敗した場合は 80 列・24 行とします。\n
     *  非 TTY の場合、または raw モードへの移行に失敗し変更済みの端末設定と Linux の SIGWINCH
     *  ハンドラーを復元できた場合は、cplat_prompt_readline_at() と同じく @ref cplat_fgets へ
     *  フォールバックし、初期値を使用しません。入力側が行全体を与えるためです。\n
     *  フォールバック時に入力行が @p buf に収まらない場合は @ref CPLAT_ERR_BUFFER_TOO_SMALL を返し、
     *  @p buf は空文字列になります。行の残りは次の呼び出しで取得できます。\n
     *  OS の入力エラーは @ref CPLAT_ERR_EOF と区別して @ref CPLAT_ERR_UNKNOWN を返します。\n
     *  端末設定または SIGWINCH ハンドラーの復元に失敗した場合は、行入力へフォールバックせず、
     *  入力結果より優先して @ref CPLAT_ERR_UNKNOWN を返します。\n
     *  復元できなかった項目は保持し、次回の入力開始前または cplat_prompt_dispose() 時に再試行します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  標準入力の端末設定と、Linux の SIGWINCH のハンドラーは、プロセス全体で共有します。\n
     *  同一プロセス内で通常プロンプトと固定プロンプトは同時に有効になりません。\n
     *  そのため、異なるハンドルへの呼び出しも含め、
     *  入力を受け付ける関数 (cplat_pinned_prompt の入力関数を含む) の呼び出しを、プロセス全体で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_prompt_readline_with_initial_at(cplat_prompt *prompt, char *buf, size_t buf_size,
                                                                     const char *prompt_str, const char *initial_text,
                                                                     const char *file, int line);

    /**
     *  @brief          呼び出し元を明示し、printf 形式のプロンプトで 1 行入力を受け取ります。
     *
     *  通常は cplat_prompt_readline_fmt() を使用してください。
     *
     *  @param[in]      p         プロンプト ハンドルです。
     *  @param[out]     buf       入力結果を格納するバッファーです。
     *  @param[in]      buf_size  @p buf のバイト数です。
     *  @param[in]      file      履歴を識別する呼び出し元ファイル名です。
     *  @param[in]      line      履歴を識別する呼び出し元行番号です。
     *  @param[in]      fmt       printf 形式の書式文字列です。NULL の場合は空文字列として扱います。
     *  @param[in]      ...       @p fmt に対応する書式引数です。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_EOF 、@ref CPLAT_ERR_CANCELED 、
     *                  @ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_BUFFER_TOO_SMALL 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  端末サイズの取得に成功した場合は幅と高さを独立に判定し、0 以下の幅は 80 列、
     *  0 以下の高さは 24 行を既定値とします。サイズ取得自体に失敗した場合は 80 列・24 行とします。\n
     *  非 TTY の場合、または raw モードへの移行に失敗し変更済みの端末設定と Linux の SIGWINCH
     *  ハンドラーを復元できた場合は、@ref cplat_fgets の行入力へフォールバックします。\n
     *  フォールバック時に入力行が @p buf に収まらない場合は @ref CPLAT_ERR_BUFFER_TOO_SMALL を返し、
     *  @p buf は空文字列になります。行の残りは次の呼び出しで取得できます。\n
     *  OS の入力エラーは @ref CPLAT_ERR_EOF と区別して @ref CPLAT_ERR_UNKNOWN を返します。\n
     *  端末設定または SIGWINCH ハンドラーの復元に失敗した場合は、行入力へフォールバックせず、
     *  入力結果より優先して @ref CPLAT_ERR_UNKNOWN を返します。\n
     *  復元できなかった項目は保持し、次回の入力開始前または cplat_prompt_dispose() 時に再試行します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  標準入力の端末設定と、Linux の SIGWINCH のハンドラーは、プロセス全体で共有します。\n
     *  同一プロセス内で通常プロンプトと固定プロンプトは同時に有効になりません。\n
     *  そのため、異なるハンドルへの呼び出しも含め、
     *  入力を受け付ける関数 (cplat_pinned_prompt の入力関数を含む) の呼び出しを、プロセス全体で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_prompt_readline_fmt_at(cplat_prompt *p, char *buf, size_t buf_size,
                                                            const char *file, int line, const char *fmt, ...)
#if defined(COMPILER_GCC)
        __attribute__((format(printf, 6, 7)))
#endif /* COMPILER_GCC */
        ;

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_PROMPT_H */
