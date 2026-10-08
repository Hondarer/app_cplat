#ifndef CPLAT_TRACE_FILE_H
#define CPLAT_TRACE_FILE_H

/**
 *  @file           trace_file.h
 *  @brief          ファイルへトレースを出力するプロバイダー API を提供します。
 *
 *  トレース メッセージをローテーション付きテキスト ファイルへ同期書き込みする
 *  クロスプラットフォーム プロバイダーです。\n
 *  cplat_etw_provider (Windows ETW) および cplat_syslog_sink (Linux syslog) と
 *  同じ init / write / dispose インターフェースを提供します。
 *
 *  @par            出力フォーマット
    @code
   2026-03-31 12:34:56.789 I メッセージテキスト
    @endcode
 *  レベル文字: C=CRITICAL / E=ERROR / W=WARNING / I=INFO / V=VERBOSE
 *
 *  @hideincludedbygraph
 *
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#include <stddef.h>
#include <cplat/base/platform.h>
#include <cplat/base/result.h>
#include <cplat/clock/clock.h>
#include <cplat/cplat_export.h>
#include <cplat/trace/tracer.h>

/**
 *  @ingroup        CPLAT_TRACE
 *  @{
 */

/* ===== 設定定数 ===== */

/**
 *  @brief          トレース ファイル 1 世代あたりの既定最大サイズ (バイト) です。
 *
 *  この値を超えるとローテーションが実行されます。\n
 *  cplat_trace_file_sink_create の max_bytes に 0 を指定した場合に使用されます。
 */
#define CPLAT_TRACE_FILE_SINK_DEFAULT_MAX_BYTES ((size_t)(1 * 1024 * 1024))

/**
 *  @brief          保持するトレース ファイル世代数の既定値です。
 *
 *  ローテーション時に path.1 〜 path.N のファイルを保持します。\n
 *  cplat_trace_file_sink_create の generations に 0 以下を指定した場合に使用されます。
 */
#define CPLAT_TRACE_FILE_SINK_DEFAULT_GENERATIONS 3

/**
 *  @brief          複数プロセスからの同時書き込みを調停するフラグです。
 *
 *  cplat_trace_file_sink_create の flags に指定します。\n
 *  指定すると、ファイル実体の変更検出と、ロック ファイル `{path}.lock` による
 *  ローテーションのプロセス間調停を有効にします。\n
 *  指定しない場合は単一プロセス専用となり、ロック ファイルは作成されません。
 *  単一プロセス専用であることは OS の排他的オープンでは保証されないため、呼び出し側が
 *  他プロセスから同一パスへ書き込まないことを保証する必要があります。
 */
#define CPLAT_TRACE_FILE_SINK_SHARED (1 << 0)

/**
 *  @brief          OS の書き込みキャッシュを使用するフラグです。
 *
 *  cplat_trace_file_sink_create の flags に指定します。\n
 *  指定しない場合は、各書き込みの完了時にデータを永続媒体へ反映するよう OS へ要求します。\n
 *  指定すると、書き込み完了時点では OS キャッシュにだけ保持される場合があります。
 *  プロセスまたは OS の異常終了時に直近のトレースが失われる可能性と引き換えに、
 *  書き込みの待ち時間を短縮します。
 */
#define CPLAT_TRACE_FILE_SINK_OS_BUFFERED (1 << 1)

/* ===== 不透明ハンドル型 ===== */

/** ファイル トレース プロバイダー ハンドル (不透明型)。 */
typedef struct cplat_trace_file_sink cplat_trace_file_sink;

/* ===== API 関数 ===== */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          ファイル トレース プロバイダーを初期化します。
     *
     *  指定されたファイル パスへの書き込みを開始します。\n
     *  ファイルが存在する場合は追記します。存在しない場合は新規作成します。\n
     *  親ディレクトリが存在しない場合は再帰的に自動生成します (`mkdir -p` 相当)。\n
     *  max_bytes に 0 を指定した場合は CPLAT_TRACE_FILE_SINK_DEFAULT_MAX_BYTES を使用します。\n
     *  generations に 0 以下を指定した場合は CPLAT_TRACE_FILE_SINK_DEFAULT_GENERATIONS を使用します。
     *
     *  flags に 0 を指定した場合は単一プロセス専用です。このモードは OS の排他的オープンを
     *  使用しないため、呼び出し側が他プロセスから同一パスへ書き込まないことを保証してください。\n
     *  flags に @ref CPLAT_TRACE_FILE_SINK_SHARED を指定した場合は、
     *  複数プロセスから同一パスへ書き込むための調停を有効にします。このとき:
     *  - 各書き込みは OS のアトミック追記で行い、サイズ判定はファイルの実サイズで行います。
     *  - ローテーションはロック ファイル `{path}.lock` によるプロセス間排他のもとで実行します。
     *    ロック ファイルは常設で、削除されません。
     *  - ロック ファイルのオープンに失敗した場合は NULL を返します。
     *
     *  @par            プロセス内での同一パス共有
     *  同一プロセス内で同一パス (正規化して比較。Windows は大文字小文字を区別しない) を指定して
     *  本関数を複数回呼び出した場合、新しいハンドルは生成せず、既存のハンドルを参照カウントで
     *  共有して返します。書き込みはハンドル内部の mutex で排他されるため、単一プロセス モードでも
     *  同一プロセス内の複数の利用者が同一ファイルへ安全に出力できます。このとき:
     *  - max_bytes / generations は最初の生成時の値が有効です (2 回目以降の指定は無視されます)。
     *  - 既存ハンドルと flags の共有モード設定が一致しない場合は NULL を返します。
     *  - 解放には利用者ごとに cplat_trace_file_sink_dispose の呼び出しが必要です。
     *
     *  @param[in]      path         出力ファイル パス。NULL の場合は NULL を返します。
     *  @param[in]      max_bytes    1 ファイルあたりの最大バイト数。0 で既定値を使用。
     *  @param[in]      generations  保持する旧世代数。0 以下で既定値を使用。
     *  @param[in]      flags        @ref CPLAT_TRACE_FILE_SINK_SHARED と
     *                               @ref CPLAT_TRACE_FILE_SINK_OS_BUFFERED の OR 結合、または 0。
     *                               負値を渡した場合は NULL を返します。
     *  @return         成功時はハンドル、失敗時は NULL を返します。
     *
     *  @warning        追記のアトミック性が保証されないファイル システム (NFS など) では、
     *                  共有モードでも行が混入する場合があります。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT cplat_trace_file_sink *CPLAT_API cplat_trace_file_sink_create(const char *path, size_t max_bytes,
                                                                               int generations, int flags);

    /**
     *  @brief          ファイルへトレース メッセージを書き込みます。
     *
     *  @param[in]      handle     cplat_trace_file_sink_create の戻り値。NULL は無視。
     *  @param[in]      level      トレース レベル。
     *  @param[in]      timestamp  使用する実時刻。NULL の場合は API 内部で現在時刻を取得。
     *                             不正な明示タイムスタンプが渡された場合も現在時刻で出力を継続し、
     *                             戻り値は @ref CPLAT_ERR_UNKNOWN を返します。
     *  @param[in]      message    null 終端 UTF-8 文字列。NULL は無視。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_trace_file_sink_write(cplat_trace_file_sink *handle, int level,
                                                           const cplat_timespec *timestamp, const char *message);

    /**
     *  @brief          ファイル トレース プロバイダーを終了します。
     *
     *  ハンドルがプロセス内で共有されている場合は参照カウントを減らし、
     *  参照カウントが 0 になったときにファイルを閉じてリソースを解放します。
     *
     *  @param[in]      handle   cplat_trace_file_sink_create の戻り値。NULL は無視。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p handle に対する操作は同時に実行できます。\n
     *  同一 @p handle に対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT void CPLAT_API cplat_trace_file_sink_dispose(cplat_trace_file_sink *handle);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_TRACE_FILE_H */
