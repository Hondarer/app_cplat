#ifndef CPLAT_SYSLOG_H
#define CPLAT_SYSLOG_H

#include <cplat/base/platform.h>
#include <cplat/base/result.h>
#include <cplat/clock/clock.h>
#include <cplat/cplat_export.h>

/**
 *  @ingroup        CPLAT_TRACE
 *  @{
 */

/**
 *  @file           syslog.h
 *  @brief          syslog への出力を補助する API を提供します。
 *
 *  Linux syslog (RFC5424 系実装) のラッパー関数群を提供します。\n
 *  Linux 専用ライブラリです。呼び出し元は `#if defined(PLATFORM_LINUX)` の
 *  中でのみ使用してください。
 *
 *  @hideincludedbygraph
 *
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#if defined(PLATFORM_LINUX)

/* ===== 不透明ハンドル型 ===== */

/** syslog プロバイダー ハンドル (不透明型)。 */
typedef struct cplat_syslog_sink cplat_syslog_sink;

    /* ===== API 関数 ===== */

    #ifdef __cplusplus
extern "C"
{
    #endif /* __cplusplus */

    /**
     *  @brief          syslog プロバイダーを初期化します。
     *
     *  @param[in]      ident     syslog メッセージに付与される識別子文字列。
     *  @param[in]      facility  syslog facility 値 (例: LOG_USER, LOG_LOCAL0〜LOG_LOCAL7)。
     *  @return         成功時はハンドル、失敗時は NULL を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT cplat_syslog_sink *CPLAT_API cplat_syslog_sink_create(const char *ident, int facility);

    /**
     *  @brief          syslog へ UTF-8 メッセージを書き込みます。
     *
     *  @param[in]      handle   cplat_syslog_sink_create の戻り値。NULL は無視。
     *  @param[in]      level    syslog severity 値。
     *  @param[in]      timestamp  デバッグ用 FD 出力に付与する実時刻です。NULL の場合は時刻を付与しません。
     *                             不正な明示タイムスタンプが渡された場合は現在時刻へ代替し、
     *                             出力は継続しつつ戻り値は @ref CPLAT_ERR_UNKNOWN を返します。
     *  @param[in]      message  null 終端 UTF-8 文字列。NULL は無視。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_syslog_sink_write(cplat_syslog_sink *handle, int level,
                                                       const cplat_timespec *timestamp, const char *message);

    /**
     *  @brief          syslog プロバイダーの識別子を変更します。
     *
     *  @param[in]      handle     cplat_syslog_sink_create の戻り値です。NULL を渡してはなりません。
     *  @param[in]      new_ident  新しい識別子文字列です。NULL を渡してはなりません。
     *  @retval         CPLAT_OK                    識別子を変更しました。
     *  @retval         CPLAT_ERR_INVALID_ARGUMENT  @p handle または @p new_ident が NULL です。
     *  @retval         CPLAT_ERR_OUT_OF_MEMORY     識別子を複製するメモリを確保できません。
     *  @return         上記以外の失敗時は、内部ロック API の共通結果コードを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_syslog_sink_rename(cplat_syslog_sink *handle, const char *new_ident);

    /**
     *  @brief          syslog プロバイダーを終了します。
     *
     *  @param[in]      handle   cplat_syslog_sink_create の戻り値。NULL は無視。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p handle に対する操作は同時に実行できます。\n
     *  同一 @p handle に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT void CPLAT_API cplat_syslog_sink_dispose(cplat_syslog_sink *handle);

    #ifdef __cplusplus
}
    #endif /* __cplusplus */

#endif /* PLATFORM_LINUX */

/** @} */

#endif /* CPLAT_SYSLOG_H */
