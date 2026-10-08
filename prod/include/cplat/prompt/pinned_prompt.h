/**
 *******************************************************************************
 *  @file           pinned_prompt.h
 *  @brief          コマンド操作向けの固定プロンプト API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/05/08
 *  @version        0.1.0
 *
 *  端末の最下部に 1 行の入力プロンプトを固定し、アプリケーションの出力をその上へ表示します。
 *  本 API は実験段階であり、コマンド ライン操作モデルの改良に伴って変更される場合があります。
 *  TTY でない場合は @ref cplat_fgets にフォールバックします。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CPLAT_PINNED_PROMPT_H
#define CPLAT_PINNED_PROMPT_H

#include <stddef.h>

#include <cplat/base/compiler.h>
#include <cplat/base/platform.h>
#include <cplat/base/result.h>
#include <cplat/prompt/prompt.h>
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
     *  @brief  固定プロンプトを操作する不透明ハンドルです。
     */
    typedef struct cplat_pinned_prompt cplat_pinned_prompt;

    /**
     *  @brief  固定プロンプトの上へ出力するときに使用する出力先です。
     */
    typedef enum cplat_pinned_prompt_channel
    {
        CPLAT_PINNED_PROMPT_CHANNEL_STDOUT = 0,
        CPLAT_PINNED_PROMPT_CHANNEL_STDERR = 1
    } cplat_pinned_prompt_channel;

    /**
     *  @brief  ステータス領域の表示位置です。
     */
    typedef enum cplat_pinned_prompt_status_position
    {
        CPLAT_PINNED_PROMPT_STATUS_POSITION_TOP = 0,
        CPLAT_PINNED_PROMPT_STATUS_POSITION_BOTTOM = 1
    } cplat_pinned_prompt_status_position;

    /**
     *  @brief  ステータス領域内の文字列配置です。
     */
    typedef enum cplat_pinned_prompt_status_align
    {
        CPLAT_PINNED_PROMPT_STATUS_ALIGN_LEFT = 0,
        CPLAT_PINNED_PROMPT_STATUS_ALIGN_RIGHT = 1
    } cplat_pinned_prompt_status_align;

    /**
     *  @brief  固定プロンプトの生成オプションです。
     */
    typedef struct cplat_pinned_prompt_options
    {
        /**
         *  @brief  将来拡張用のフラグです。0 を指定してください。
         */
        unsigned int flags;

        /**
         *  @brief  構造体配置用の予約領域です。0 を指定してください。
         */
        unsigned int reserved;

        /**
         *  @brief  入力編集と履歴に関するオプションです。
         */
        cplat_prompt_options input;
    } cplat_pinned_prompt_options;

    /**
     *  @brief          固定プロンプト ハンドルを生成します。
     *  @param[in]      options  生成オプションです。NULL の場合は既定値を使用します。
     *  @return         成功時は生成したハンドルを返します。メモリまたは同期オブジェクトを確保できない場合は
     *                  NULL を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT cplat_pinned_prompt *CPLAT_API cplat_pinned_prompt_create(const cplat_pinned_prompt_options *options);

    /**
     *  @brief          固定プロンプト ハンドルを解放します。
     *  @param[in]      screen  cplat_pinned_prompt_create() が返したハンドルです。NULL も指定できます。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  解放対象の @p screen を他スレッドが使用していないことを呼び出し側で保証してください。
     */
    CPLAT_EXPORT void CPLAT_API cplat_pinned_prompt_dispose(cplat_pinned_prompt *screen);

/**
 *  @brief          端末下部に固定したプロンプトで 1 行のコマンド入力を受け取ります。
 *  @param[in]      screen      固定プロンプト ハンドルです。
 *  @param[out]     buf         入力結果を格納するバッファーです。終端の改行は格納しません。
 *  @param[in]      buf_size    @p buf のバイト数です。
 *  @param[in]      prompt_str  表示するプロンプト文字列です。NULL の場合は空文字列として扱います。
 *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_EOF 、@ref CPLAT_ERR_CANCELED 、
 *                  @ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_BUFFER_TOO_SMALL 、
 *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
 *
 *  フォールバック時に入力行が @p buf に収まらない場合は @ref CPLAT_ERR_BUFFER_TOO_SMALL を返し、
 *  @p buf は空文字列になります。行の残りは次の呼び出しで取得できます。
 *
 *  @par            スレッド セーフ
 *  本マクロはスレッド セーフではありません。\n
 *  同一プロンプト ハンドルに対する呼び出しを、呼び出し側で直列化してください。
 */
#define cplat_pinned_prompt_readline(screen, buf, buf_size, prompt_str) \
    cplat_pinned_prompt_readline_at((screen), (buf), (buf_size), (prompt_str), __FILE__, __LINE__)

/**
 *  @brief          書式指定した固定プロンプトで 1 行のコマンド入力を受け取ります。
 *  @param[in]      screen    固定プロンプト ハンドルです。
 *  @param[out]     buf       入力結果を格納するバッファーです。終端の改行は格納しません。
 *  @param[in]      buf_size  @p buf のバイト数です。
 *  @param[in]      fmt       printf 形式の書式文字列です。NULL の場合は空文字列として扱います。
 *  @param[in]      ...       @p fmt に対応する書式引数です。
 *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_EOF 、@ref CPLAT_ERR_CANCELED 、
 *                  @ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_BUFFER_TOO_SMALL 、
 *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
 *
 *  フォールバック時に入力行が @p buf に収まらない場合は @ref CPLAT_ERR_BUFFER_TOO_SMALL を返し、
 *  @p buf は空文字列になります。行の残りは次の呼び出しで取得できます。
 *
 *  @par            スレッド セーフ
 *  本マクロはスレッド セーフではありません。\n
 *  同一プロンプト ハンドルに対する呼び出しを、呼び出し側で直列化してください。
 */
#define cplat_pinned_prompt_readline_fmt(screen, buf, buf_size, fmt, ...) \
    cplat_pinned_prompt_readline_fmt_at((screen), (buf), (buf_size), __FILE__, __LINE__, (fmt), ##__VA_ARGS__)

/**
 *  @brief          入力欄に初期値を入れた状態で 1 行入力を受け取ります。
 *  @param[in]      screen        固定プロンプト ハンドルです。
 *  @param[out]     buf           入力結果を格納するバッファーです。終端の改行は格納しません。
 *  @param[in]      buf_size      @p buf のバイト数です。
 *  @param[in]      prompt_str    表示するプロンプト文字列です。NULL の場合は空文字列として扱います。
 *  @param[in]      initial_text  入力欄の初期値です。NULL と空文字列は、初期値なしとして扱います。
 *  @return         cplat_pinned_prompt_readline() と同じ結果コードに加え、
 *                  初期値が不正な場合は @ref CPLAT_ERR_INVALID_ARGUMENT 、
 *                  初期値が入力欄の上限を超える場合は @ref CPLAT_ERR_BUFFER_TOO_SMALL 、
 *                  初期値のためのメモリを確保できない場合は @ref CPLAT_ERR_OUT_OF_MEMORY を返します。
 *
 *  詳細は cplat_pinned_prompt_readline_with_initial_at() を参照してください。
 *
 *  @par            スレッド セーフ
 *  本マクロはスレッド セーフではありません。\n
 *  同一プロンプト ハンドルに対する呼び出しを、呼び出し側で直列化してください。
 */
#define cplat_pinned_prompt_readline_with_initial(screen, buf, buf_size, prompt_str, initial_text) \
    cplat_pinned_prompt_readline_with_initial_at((screen), (buf), (buf_size), (prompt_str), (initial_text), __FILE__, \
                                                 __LINE__)

    /**
     *  @brief          呼び出し元の位置を明示して 1 行のコマンド入力を受け取ります。
     *
     *  通常は cplat_pinned_prompt_readline() を使用してください。
     *
     *  @param[in]      screen      固定プロンプト ハンドルです。
     *  @param[out]     buf         入力結果を格納するバッファーです。
     *  @param[in]      buf_size    @p buf のバイト数です。
     *  @param[in]      prompt_str  表示するプロンプト文字列です。NULL の場合は空文字列として扱います。
     *  @param[in]      file        履歴を識別する呼び出し元ファイル名です。
     *  @param[in]      line        履歴を識別する呼び出し元行番号です。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_EOF 、@ref CPLAT_ERR_CANCELED 、
     *                  @ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_BUFFER_TOO_SMALL 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  フォールバック時に入力行が @p buf に収まらない場合は @ref CPLAT_ERR_BUFFER_TOO_SMALL を返し、
     *  @p buf は空文字列になります。行の残りは次の呼び出しで取得できます。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  同一プロンプト ハンドルに対する呼び出しを、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_pinned_prompt_readline_at(cplat_pinned_prompt *screen, char *buf, size_t buf_size,
                                                               const char *prompt_str, const char *file, int line);

    /**
     *  @brief          呼び出し元を明示し、入力欄に初期値を入れた状態で 1 行入力を受け取ります。
     *
     *  通常は cplat_pinned_prompt_readline_with_initial() を使用してください。
     *
     *  @param[in]      screen        固定プロンプト ハンドルです。
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
     *  標準入力が端末でない場合は、cplat_pinned_prompt_readline_at() と同じく @ref cplat_fgets へフォールバックし、初期値を使用しません。\n
     *  入力側が行全体を与えるためです。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  同一プロンプト ハンドルに対する呼び出しを、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_pinned_prompt_readline_with_initial_at(cplat_pinned_prompt *screen, char *buf,
                                                                            size_t buf_size, const char *prompt_str,
                                                                            const char *initial_text, const char *file,
                                                                            int line);

    /**
     *  @brief          呼び出し元の位置とプロンプト書式を明示してコマンド入力を受け取ります。
     *
     *  通常は cplat_pinned_prompt_readline_fmt() を使用してください。
     *
     *  @param[in]      screen    固定プロンプト ハンドルです。
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
     *  フォールバック時に入力行が @p buf に収まらない場合は @ref CPLAT_ERR_BUFFER_TOO_SMALL を返し、
     *  @p buf は空文字列になります。行の残りは次の呼び出しで取得できます。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  同一プロンプト ハンドルに対する呼び出しを、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_pinned_prompt_readline_fmt_at(cplat_pinned_prompt *screen, char *buf,
                                                                   size_t buf_size, const char *file, int line,
                                                                   const char *fmt, ...)
#if defined(COMPILER_GCC)
        __attribute__((format(printf, 6, 7)))
#endif /* COMPILER_GCC */
        ;

    /**
     *  @brief          端末下部の固定プロンプトより上へデータを書き込みます。
     *  @param[in]      screen   固定プロンプト ハンドルです。
     *  @param[in]      channel  書き込み先の標準ストリームです。
     *  @param[in]      data         書き込むデータです。@p size が 0 の場合に限り NULL も指定できます。
     *  @param[in]      size         @p data から書き込むバイト数です。
     *  @param[out]     written_out  対象ストリームへ書き込んだバイト数を格納します。NULL も指定できます。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT のいずれかを返します。\n
     *                  書き込みが @p size バイトに満たない場合は @ref CPLAT_ERR_UNKNOWN を返します。
     *
     *  指定されたデータだけを書き込み、改行は付加しません。
     *  ANSI CSI SGR エスケープ シーケンスは、色指定としてそのまま出力します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_pinned_prompt_write(cplat_pinned_prompt *screen,
                                                         cplat_pinned_prompt_channel channel, const void *data,
                                                         size_t size, size_t *written_out);

    /**
     *  @brief          端末下部の固定プロンプトより上へ書式付き文字列を書き込みます。
     *  @param[in]      screen   固定プロンプト ハンドルです。
     *  @param[in]      channel  書き込み先の標準ストリームです。
     *  @param[in]      fmt      printf 形式の書式文字列です。NULL の場合は空文字列として扱います。
     *  @param[in]      ...      @p fmt に対応する書式引数です。
     *  @return         成功時は対象ストリームへ書き込んだバイト数を返します。引数不正、書式処理失敗、
     *                  またはメモリ確保失敗の場合は -1 を返します。
     *  @note           ANSI CSI SGR エスケープ シーケンスは、色指定としてそのまま出力します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_pinned_prompt_printf(cplat_pinned_prompt *screen,
                                                          cplat_pinned_prompt_channel channel, const char *fmt, ...)
#if defined(COMPILER_GCC)
        __attribute__((format(printf, 3, 4)))
#endif /* COMPILER_GCC */
        ;

    /**
     *  @brief          指定位置のステータス領域を有効または無効にします。
     *  @param[in]      screen    固定プロンプト ハンドルです。
     *  @param[in]      position  上部または下部のステータス領域を指定します。
     *  @param[in]      enable    0 以外の場合は有効にし、0 の場合は無効にします。
     *  @retval         CPLAT_OK                    ステータス領域の有効状態を変更しました。
     *  @retval         CPLAT_ERR_INVALID_ARGUMENT  @p screen が NULL、または @p position が不正です。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_pinned_prompt_status_enable(cplat_pinned_prompt *screen,
                                                                 cplat_pinned_prompt_status_position position,
                                                                 int enable);

    /**
     *  @brief          指定位置のステータス領域へ表示内容を設定します。
     *  @param[in]      screen    固定プロンプト ハンドルです。
     *  @param[in]      position  上部または下部のステータス領域を指定します。
     *  @param[in]      align     左寄せまたは右寄せを指定します。
     *  @param[in]      content   表示する文字列です。NULL の場合は指定位置の内容を消去します。
     *  @retval         CPLAT_OK                    表示内容を設定しました。
     *  @retval         CPLAT_ERR_INVALID_ARGUMENT  @p screen が NULL、または位置や配置の指定が不正です。
     *  @retval         CPLAT_ERR_OUT_OF_MEMORY     表示内容を保持するメモリを確保できません。
     *  @note           ANSI CSI SGR エスケープ シーケンスは色指定としてそのまま出力し、表示幅を
     *                  0 として配置を計算します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_pinned_prompt_status_set(cplat_pinned_prompt *screen,
                                                              cplat_pinned_prompt_status_position position,
                                                              cplat_pinned_prompt_status_align align,
                                                              const char *content);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_PINNED_PROMPT_H */
