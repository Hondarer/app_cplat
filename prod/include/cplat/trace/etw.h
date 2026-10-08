#ifndef CPLAT_ETW_H
#define CPLAT_ETW_H

#include <cplat/base/platform.h>
#include <cplat/base/result.h>
#include <cplat/cplat_export.h>
#include <stdint.h>

/**
 *  @ingroup        CPLAT_TRACE
 *  @{
 */

/**
 *  @file           etw.h
 *  @brief          ETW を使用したトレースを補助する API を提供します。
 *
 *  TraceLogging ベースの ETW プロバイダーを簡易に操作するための
 *  ヘルパー関数群を提供します。\n
 *  Windows 専用ライブラリです。呼び出し元は `#if defined(PLATFORM_WINDOWS)` の
 *  中でのみ使用してください。
 *
 *  @hideincludedbygraph
 *
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

/**
 *  @struct         cplat_etw_event
 *  @brief          ETW consumer が受け取るイベント情報です。
 *
 *  event_name / service / message は callback 呼び出し中のみ有効なポインターです。
 */
typedef struct cplat_etw_event
{
    /** イベント レベル (1-5)。 */
    int level;
    /** イベント発行元のプロセス ID (Windows DWORD 互換のためコーディング規範の例外として uint32_t を維持)。 */
    uint32_t process_id;
    /** イベント名。取得できない場合は NULL。 */
    const char *event_name;
    /** Service フィールド。存在しない場合は NULL。 */
    const char *service;
    /** Message フィールド。存在しない場合は NULL。 */
    const char *message;
    /** ETW が付与したタイムスタンプ。EVENT_HEADER::TimeStamp の生値 (100ns 単位)。 */
    int64_t timestamp_100ns;
} cplat_etw_event;

/**
 *  @typedef        cplat_etw_event_fn
 *  @brief          ETW イベント受信コールバック型です。
 *
 *  @param[in]      event    受信イベント。callback 呼び出し中のみ参照可能。
 *  @param[in]      context  cplat_etw_session_start に渡したユーザー データ。
 *
 *  @par            スレッド セーフ
 *  コールバックは ETW ワーカー スレッドから呼び出されます。\n
 *  コールバックの実装者は再入性を確保してください。
 */
typedef void (*cplat_etw_event_fn)(const cplat_etw_event *event, void *context);

#if defined(PLATFORM_WINDOWS)

    #include <cplat/base/windows_sdk.h>

/* ===== プロバイダー参照型 ===== */

/**
 *  @typedef        cplat_etw_provider_ref_t
 *  @brief          プロバイダー参照型です。
 *
 *  TraceLoggingHProvider (TraceLoggingProvider.h が定義する型) と同等です。
 *  OS / SDK の `_tlgProvider_t` に対応する alias であるため、_t サフィックスを残す。
 *  see: コーディング規範「予約識別子の回避」の例外
 */
struct _tlgProvider_t;
typedef struct _tlgProvider_t const *cplat_etw_provider_ref_t;

    /* ===== プロバイダー定義マクロ ===== */

    /**
     *  @brief          ETW プロバイダーを定義するマクロです。
     *
     *  呼び出し元の .c ファイルのファイル スコープに 1 回だけ記述します。\n
     *  TRACELOGGING_DEFINE_PROVIDER(var, name, guid) に展開します。
     *
     *  @param          var   プロバイダー変数名 (cplat_etw_provider_ref_t 型)
     *  @param          name  プロバイダー名 (文字列リテラル)
     *  @param          guid  GUID (TraceLogging 形式の括弧付き定数タプル)
     */
    #define CPLAT_ETW_DEFINE_PROVIDER(var, name, guid) TRACELOGGING_DEFINE_PROVIDER(var, name, guid)

/* ===== 不透明ハンドル型 ===== */

/** ETW プロバイダー ハンドル (不透明型)。 */
typedef struct cplat_etw_provider cplat_etw_provider;

    /* ===== API 関数 ===== */

    #ifdef __cplusplus
extern "C"
{
    #endif /* __cplusplus */

    /**
     *  @brief          ETW プロバイダーを登録します。
     *
     *  @param[in]      provider_ref  CPLAT_ETW_DEFINE_PROVIDER で定義した変数。
     *  @return         成功時はハンドル、失敗時は NULL を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT cplat_etw_provider *CPLAT_API cplat_etw_provider_create(cplat_etw_provider_ref_t provider_ref);

    /**
     *  @brief          ETW プロバイダーへ UTF-8 メッセージを書き込みます。
     *
     *  @param[in]      handle   cplat_etw_provider_create の戻り値。NULL は無視。
     *  @param[in]      level    イベント レベル (1=CRITICAL / 2=ERROR / 3=WARNING / 4=INFO / 5=VERBOSE)。
     *  @param[in]      service  サービス名です。NULL の場合は Service フィールドなしで書き込みます。
     *  @param[in]      message  null 終端 UTF-8 文字列。NULL は無視。
     *  @return         常に @ref CPLAT_OK を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_etw_provider_write(cplat_etw_provider *handle, int level, const char *service,
                                                        const char *message);

    /**
     *  @brief          ETW プロバイダーの登録を解除します。
     *
     *  @param[in]      handle   cplat_etw_provider_create の戻り値。NULL は無視。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  解放対象の @p handle を他スレッドが使用していないことを呼び出し側で保証してください。
     */
    CPLAT_EXPORT void CPLAT_API cplat_etw_provider_dispose(cplat_etw_provider *handle);

    /* ===== セッション (Consumer) API ===== */

    /** ETW セッション ハンドル (不透明型)。 */
    typedef struct cplat_etw_session cplat_etw_session;

    /**
     *  @brief          ETW セッション開始に必要な権限があるか検査します。
     *
     *  @return         必要な権限がある場合は @ref CPLAT_OK 、権限不足の場合は
     *                  @ref CPLAT_ERR_PERMISSION_DENIED 、検査中にシステム エラーが発生した場合は
     *                  @ref CPLAT_ERR_UNKNOWN を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_etw_session_check_access(void);

    /**
     *  @brief          リアルタイム ETW セッションを開始し、指定プロバイダーを購読します。
     *
     *  @param[in]      session_name       セッション名 (システム全体で一意にしてください)。
     *  @param[in]      provider_guid_str  GUID 文字列 "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"。
     *  @param[in]      callback           イベント受信時に呼び出されるコールバック。
     *  @param[in]      context            コールバックに渡すユーザー データ。
     *  @param[out]     session_out        開始したセッションのハンドルの格納先。NULL を渡してはなりません。\n
     *                  失敗時は NULL を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_PERMISSION_DENIED 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_etw_session_start(const char *session_name, const char *provider_guid_str,
                                                       cplat_etw_event_fn callback, void *context,
                                                       cplat_etw_session **session_out);

    /**
     *  @brief          ETW セッションを停止し、リソースを解放します。
     *
     *  @param[in]      session  cplat_etw_session_start の戻り値。NULL は無視。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフではありません。\n
     *  同一 @p session への並行呼び出しは未定義動作です。
     */
    CPLAT_EXPORT void CPLAT_API cplat_etw_session_stop(cplat_etw_session *session);

    #ifdef __cplusplus
}
    #endif /* __cplusplus */

#endif /* PLATFORM_WINDOWS */

/** @} */

#endif /* CPLAT_ETW_H */
