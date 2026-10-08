/**
 *******************************************************************************
 *  @file           language_internal.h
 *  @brief          出力言語の決定状態を初期化する内部 API を宣言します。
 *
 *  出力言語は、利用側が設定しない場合、最初の参照時に実行環境の表示言語から決定します。\n
 *  決定はプロセスで 1 回だけ行うため、決定前の状態を再現する手段をテストへ提供します。\n
 *  本 API は公開しません。決定のやり直しは、プロセスの実行中に想定する操作ではありません。
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは複数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CPLAT_STRING_CATALOG_LANGUAGE_INTERNAL_H
#define CPLAT_STRING_CATALOG_LANGUAGE_INTERNAL_H

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          出力言語を、決定前の状態へ戻します。
     *
     *  次回の出力言語の参照で、実行環境の表示言語から決定し直します。\n
     *  実行環境ごとの決定を検証するテストから呼び出します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  文字列を組み立てているスレッドが存在しない状態で呼び出してください。
     */
    void cplat_internal_string_catalog_language_reset_for_test(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CPLAT_STRING_CATALOG_LANGUAGE_INTERNAL_H */
