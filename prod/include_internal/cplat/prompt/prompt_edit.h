/**
 *  @file           prompt_edit.h
 *  @brief          プロンプトの UTF-8 編集バッファーを管理する内部 API を提供します。
 *
 *  UTF-8 文字境界の移動、編集バッファーの拡張、生成オプションの既定値解決を提供します。
 *
 *  @hideincludedbygraph
 *
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CPLAT_PROMPT_EDIT_H
#define CPLAT_PROMPT_EDIT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     *  @brief          指定位置以前にある UTF-8 文字境界を取得します。
     *  @param[in]      buf  UTF-8 文字列です。
     *  @param[in]      pos  検索開始位置のバイト オフセットです。
     *  @return         @p pos の直前にある UTF-8 文字の先頭位置を返します。@p pos が 0 の場合は 0 を返します。
     */
    size_t cplat_internal_prompt_edit_utf8_prev_boundary(const char *buf, size_t pos);

    /**
     *  @brief          指定位置の次にある UTF-8 文字境界を取得します。
     *  @param[in]      buf  UTF-8 文字列です。
     *  @param[in]      len  @p buf の有効バイト数です。
     *  @param[in]      pos  検索開始位置のバイト オフセットです。
     *  @return         @p pos の次にある UTF-8 文字の先頭位置を返します。末尾に達した場合は @p len を返します。
     */
    size_t cplat_internal_prompt_edit_utf8_next_boundary(const char *buf, size_t len, size_t pos);

    /**
     *  @brief          指定位置を直前の UTF-8 文字境界へ補正します。
     *  @param[in]      buf  UTF-8 文字列です。
     *  @param[in]      len  @p buf の有効バイト数です。
     *  @param[in]      pos  補正するバイト オフセットです。
     *  @return         0 以上 @p len 以下の UTF-8 文字境界を返します。
     */
    size_t cplat_internal_prompt_edit_utf8_sanitize_boundary(const char *buf, size_t len, size_t pos);

    /**
     *  @brief          編集バッファーが必要な容量を持つように拡張します。
     *  @param[in,out]  buf        realloc() で拡張するバッファー ポインターの格納先です。
     *  @param[in,out]  cap        現在の容量を受け取り、成功時に拡張後の容量を格納します。
     *  @param[in]      max_bytes  バッファー容量の上限です。
     *  @param[in]      required   呼び出し側が必要とするバイト数です。
     *  @return         必要な容量を確保できた場合は 0 を返します。引数不正、上限超過、オーバーフロー、
     *                  またはメモリ確保失敗の場合は -1 を返します。
     */
    int cplat_internal_prompt_edit_ensure_capacity(char **buf, size_t *cap, size_t max_bytes, size_t required);

    /**
     *  @brief          プロンプトの入力編集と履歴に関するオプションを解決します。
     *
     *  要求値が 0 の項目へ既定値を適用し、初期容量を 2 以上かつ最大容量以下へ補正します。
     *
     *  @param[in]      requested_history_max       要求された履歴エントリ数の上限です。
     *  @param[in]      requested_initial_capacity  要求された入力バッファーの初期容量です。
     *  @param[in]      requested_max_bytes         要求された入力バッファーの最大容量です。
     *  @param[in]      initial_capacity_default    初期容量の既定値です。
     *  @param[out]     history_max                 解決後の履歴エントリ数を格納します。NULL も指定できます。
     *  @param[out]     initial_capacity            解決後の初期容量を格納します。NULL も指定できます。
     *  @param[out]     max_bytes                   解決後の最大容量を格納します。NULL も指定できます。
     */
    void cplat_internal_prompt_edit_resolve_options(size_t requested_history_max, size_t requested_initial_capacity,
                                                    size_t requested_max_bytes, size_t initial_capacity_default,
                                                    size_t *history_max, size_t *initial_capacity, size_t *max_bytes);

    /**
     *  @brief          入力欄の初期値が、1 行の編集内容として受け入れられるかを確認します。
     *  @param[in]      initial_text 初期値です。NULL は空文字列として扱います。
     *  @param[in]      max_bytes    NUL 終端を含む入力編集バッファーの最大バイト数です。
     *  @param[out]     length_out   初期値のバイト数 (NUL を除く) の格納先です。NULL を渡してはなりません。
     *  @return         受け入れられる場合は @ref CPLAT_OK を返します。
     *  @return         改行などの制御文字 (0x00 から 0x1F、および 0x7F) を含む場合は
     *                  @ref CPLAT_ERR_INVALID_ARGUMENT を返します。入力欄は 1 行のためです。
     *  @return         NUL 終端を含めて @p max_bytes を超える場合は @ref CPLAT_ERR_BUFFER_TOO_SMALL を返します。
     *
     *  prompt と pinned_prompt が、初期値付きの入力で同じ規則を使うための関数です。\n
     *  途中で切り詰めると UTF-8 の文字の途中で切れた値を編集させることになるため、切り詰めずに拒否します。
     */
    int cplat_internal_prompt_edit_validate_initial_text(const char *initial_text, size_t max_bytes,
                                                         size_t *length_out);

#ifdef __cplusplus
}
#endif

#endif /* CPLAT_PROMPT_EDIT_H */
