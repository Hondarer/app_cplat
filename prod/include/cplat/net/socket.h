/**
 *******************************************************************************
 *  @file           socket.h
 *  @brief          IPv4 ソケットの生成、接続、送受信、待機を行う API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/08/11
 *  @version        1.0.0
 *
 *  プラットフォームごとに異なるソケット API を共通インターフェースで抽象化します。
 *
 *  | OS      | 使用ライブラリ                    | 待機 API   | 非ブロッキング設定       |
 *  | ------- | --------------------------------- | ---------- | ------------------------ |
 *  | Linux   | BSD ソケット (libc)               | `poll`     | `fcntl` (`O_NONBLOCK`)   |
 *  | Windows | Winsock2 (`ws2_32.lib`)           | `WSAPoll`  | `ioctlsocket` (`FIONBIO`)|
 *
 *  Table: プラットフォーム別のソケット API
 *
 *  Winsock の初期化と終了は本モジュールの内部で行うため、利用側は初期化順序を
 *  意識せずに任意のタイミングで API を呼び出せます。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CPLAT_NET_SOCKET_H
#define CPLAT_NET_SOCKET_H

#include <cplat/base/error.h>
#include <cplat/base/result.h>
#include <cplat/cplat_export.h>
#include <cplat/net/endpoint.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>

/** 無効なソケットを表す値。 */
#define CPLAT_INVALID_SOCKET ((cplat_socket) - 1)

/** タイムアウトなしで待機することを表すタイムアウト値。 */
#define CPLAT_SOCKET_WAIT_FOREVER (-1)

/** 即時リターンすることを表すタイムアウト値。 */
#define CPLAT_SOCKET_NO_WAIT 0

/** @ref cplat_socket_listen へ既定の待ち受けキュー長を指定する値。 */
#define CPLAT_SOCKET_BACKLOG_DEFAULT 0

/** @ref cplat_socket_wait_readable_multi へ一度に指定できるソケットの最大数。 */
#define CPLAT_SOCKET_WAIT_MAX 16U

/** 1 回の送受信で扱えるバイト数の上限。 */
#define CPLAT_SOCKET_MAX_TRANSFER ((size_t)INT_MAX)

/**
 *  @ingroup        CPLAT_NET
 *  @{
 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          ソケット ハンドルを表します。
     *
     *  Linux のファイル記述子 (`int`) と Windows の `SOCKET` (`UINT_PTR`) の双方を
     *  可逆に格納できる幅を持ちます。\n
     *  無効値の判定には @ref CPLAT_INVALID_SOCKET を使用し、数値リテラルと比較しません。
     */
    typedef intptr_t cplat_socket;

    /**
     *  @brief          生成するソケットの種別を表します。
     */
    typedef enum cplat_socket_kind
    {
        CPLAT_SOCKET_TCP = 0, /**< 接続指向のストリーム ソケット。 */
        CPLAT_SOCKET_UDP = 1  /**< データグラム ソケット。 */
    } cplat_socket_kind;

    /**
     *  @brief          IPv4 ソケットを生成します。
     *  @param[in]      kind     生成するソケットの種別。
     *  @param[out]     sock_out 生成したソケットの格納先。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  失敗した場合、@p sock_out へ @ref CPLAT_INVALID_SOCKET を格納します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_open(cplat_socket_kind kind, cplat_socket *sock_out,
                                                 cplat_error *detail_out);

    /**
     *  @brief          ソケットを閉じます。
     *  @param[in]      sock 閉じるソケット。@ref CPLAT_INVALID_SOCKET を指定した場合は何もしません。
     *
     *  本関数は直前エラー (@ref cplat_error_get_last) を保存し、復元してから返ります。\n
     *  解放経路で呼び出しても、呼び出し前に記録された診断情報が失われません。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるソケットに対する呼び出しは同時に実行できます。\n
     *  同一 @p sock を複数スレッドで同時に閉じる操作は二重クローズとなるため、同一 @p sock に対する操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT void CPLAT_API cplat_socket_close(cplat_socket sock);

    /**
     *  @brief          ソケットの送受信を両方向とも停止します。
     *  @param[in]      sock 対象のソケット。@ref CPLAT_INVALID_SOCKET を指定した場合は何もしません。
     *
     *  待機中のスレッドを解除する目的で使用します。ソケットは閉じません。\n
     *  失敗しても通知しません。停止できない場合でも呼び出し側に取るべき手段がないためです。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void CPLAT_API cplat_socket_shutdown(cplat_socket sock);

    /**
     *  @brief          ソケットへローカルのエンドポイントを割り当てます。
     *  @param[in]      sock       対象のソケット。
     *  @param[in]      endpoint   割り当てるエンドポイント。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_bind(cplat_socket sock, const cplat_ipv4_endpoint *endpoint,
                                                 cplat_error *detail_out);

    /**
     *  @brief          ソケットを接続待ち受け状態にします。
     *  @param[in]      sock       対象のソケット。
     *  @param[in]      backlog    待ち受けキューの長さ。@ref CPLAT_SOCKET_BACKLOG_DEFAULT を
     *                             指定した場合、OS の既定の最大値を使用します。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_listen(cplat_socket sock, int backlog, cplat_error *detail_out);

    /**
     *  @brief          待ち受け中のソケットで接続を受け付けます。
     *  @param[in]      sock       待ち受け中のソケット。
     *  @param[out]     peer_out   接続元のエンドポイントの格納先。NULL 可。
     *  @param[out]     sock_out   受け付けたソケットの格納先。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  失敗した場合、@p sock_out へ @ref CPLAT_INVALID_SOCKET を格納します。\n
     *  非ブロッキング モードで接続待ちがない場合は、@p detail_out の要因が
     *  @ref CPLAT_CAUSE_WOULD_BLOCK になります。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_accept(cplat_socket sock, cplat_ipv4_endpoint *peer_out,
                                                   cplat_socket *sock_out, cplat_error *detail_out);

    /**
     *  @brief          相手のエンドポイントへ接続します。
     *  @param[in]      sock       対象のソケット。
     *  @param[in]      endpoint   接続先のエンドポイント。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_IN_PROGRESS 、
     *                  @ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  非ブロッキング モードで接続が完了しなかった場合は、戻り値が
     *  @ref CPLAT_ERR_IN_PROGRESS になります。@p detail_out にはプラットフォームの
     *  詳細エラーを保持しますが、要因は Linux の `EINPROGRESS` と Windows の
     *  `WSAEWOULDBLOCK`、`WSAEINPROGRESS`、`WSAEALREADY` で異なる場合があります。\n
     *  この場合は @ref cplat_socket_wait_writable で完了を待ち、
     *  @ref cplat_socket_get_pending_error で結果を確認します。
     *
     *  ブロッキング モードの接続がシグナルで中断された場合は、本関数が完了を待って
     *  結果を確定します。呼び出し側が中断を意識する必要はありません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_connect(cplat_socket sock, const cplat_ipv4_endpoint *endpoint,
                                                    cplat_error *detail_out);

    /**
     *  @brief          ソケットに保留されているエラーを取得します。
     *  @param[in]      sock       対象のソケット。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         保留エラーがない場合は @ref CPLAT_OK 、
     *                  保留エラーがある場合はそれに対応する @ref CPLAT_ERR_UNKNOWN 等を返します。
     *
     *  非ブロッキングの接続が完了したかどうかの判定に使用します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_get_pending_error(cplat_socket sock, cplat_error *detail_out);

    /**
     *  @brief          ソケットの非ブロッキング モードを設定します。
     *  @param[in]      sock       対象のソケット。
     *  @param[in]      enable     非ブロッキングにする場合は 0 以外、ブロッキングにする場合は 0。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_set_nonblocking(cplat_socket sock, int enable, cplat_error *detail_out);

    /**
     *  @brief          アドレスの再利用を許可するかどうかを設定します。
     *  @param[in]      sock       対象のソケット。
     *  @param[in]      enable     許可する場合は 0 以外、許可しない場合は 0。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_set_reuse_address(cplat_socket sock, int enable, cplat_error *detail_out);

    /**
     *  @brief          ブロードキャスト送信を許可するかどうかを設定します。
     *  @param[in]      sock       対象のソケット。
     *  @param[in]      enable     許可する場合は 0 以外、許可しない場合は 0。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_set_broadcast(cplat_socket sock, int enable, cplat_error *detail_out);

    /**
     *  @brief          マルチキャスト送信に使用するローカル インターフェースを設定します。
     *  @param[in]      sock       対象のソケット。
     *  @param[in]      interface_address ローカル インターフェースのアドレス
     *                             (ネットワーク バイト オーダー)。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_set_multicast_interface(cplat_socket sock, uint32_t interface_address,
                                                                    cplat_error *detail_out);

    /**
     *  @brief          マルチキャスト グループへ参加します。
     *  @param[in]      sock       対象のソケット。
     *  @param[in]      group_address マルチキャスト グループのアドレス
     *                             (ネットワーク バイト オーダー)。
     *  @param[in]      interface_address 受信に使用するローカル インターフェースのアドレス
     *                             (ネットワーク バイト オーダー)。
     *                             @ref CPLAT_IPV4_ADDR_ANY を指定した場合は OS が選択します。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_join_multicast_group(cplat_socket sock, uint32_t group_address,
                                                                 uint32_t interface_address, cplat_error *detail_out);

    /**
     *  @brief          マルチキャスト グループから離脱します。
     *  @param[in]      sock       対象のソケット。
     *  @param[in]      group_address マルチキャスト グループのアドレス
     *                             (ネットワーク バイト オーダー)。
     *  @param[in]      interface_address 受信に使用していたローカル インターフェースのアドレス
     *                             (ネットワーク バイト オーダー)。
     *                             @ref cplat_socket_join_multicast_group へ指定した値と同じ値を指定します。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  ソケットを閉じると参加中のグループからは自動的に離脱するため、閉じる前に
     *  明示的な離脱を通知する場合に使用します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_leave_multicast_group(cplat_socket sock, uint32_t group_address,
                                                                  uint32_t interface_address, cplat_error *detail_out);

    /**
     *  @brief          接続済みソケットへ送信します。
     *  @param[in]      sock       対象のソケット。
     *  @param[in]      buf        送信するデータ。
     *  @param[in]      len        @p buf のバイト数。@ref CPLAT_SOCKET_MAX_TRANSFER 以下を指定します。
     *  @param[out]     sent_out   送信できたバイト数の格納先。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  1 回の呼び出しで @p len 全体を送信するとは限りません。\n
     *  全体の送信を保証する場合は @ref cplat_socket_send_all を使用します。
     *  Linux では、切断済みの接続への送信による SIGPIPE を配信しません。\n
     *  送信エラーは @ref CPLAT_ERR_UNKNOWN で通知し、@p detail_out へ詳細を格納します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるソケットに対する呼び出しは同時に実行できます。\n
     *  同一 @p sock に対する並行操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_send(cplat_socket sock, const void *buf, size_t len, size_t *sent_out,
                                                 cplat_error *detail_out);

    /**
     *  @brief          接続済みソケットから受信します。
     *  @param[in]      sock       対象のソケット。
     *  @param[out]     buf        受信データの格納先。
     *  @param[in]      len        @p buf のバイト数。@ref CPLAT_SOCKET_MAX_TRANSFER 以下を指定します。
     *  @param[out]     received_out 受信したバイト数の格納先。0 は相手が送信を終了したことを表します。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるソケットに対する呼び出しは同時に実行できます。\n
     *  同一 @p sock に対する並行操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_recv(cplat_socket sock, void *buf, size_t len, size_t *received_out,
                                                 cplat_error *detail_out);

    /**
     *  @brief          指定したエンドポイントへ送信します。
     *  @param[in]      sock       対象のソケット。
     *  @param[in]      buf        送信するデータ。
     *  @param[in]      len        @p buf のバイト数。@ref CPLAT_SOCKET_MAX_TRANSFER 以下を指定します。
     *  @param[in]      endpoint   送信先のエンドポイント。
     *  @param[out]     sent_out   送信できたバイト数の格納先。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_sendto(cplat_socket sock, const void *buf, size_t len,
                                                   const cplat_ipv4_endpoint *endpoint, size_t *sent_out,
                                                   cplat_error *detail_out);

    /**
     *  @brief          任意のエンドポイントから受信します。
     *  @param[in]      sock       対象のソケット。
     *  @param[out]     buf        受信データの格納先。
     *  @param[in]      len        @p buf のバイト数。@ref CPLAT_SOCKET_MAX_TRANSFER 以下を指定します。
     *  @param[out]     peer_out   送信元のエンドポイントの格納先。NULL 可。
     *  @param[out]     received_out 受信したバイト数の格納先。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_recvfrom(cplat_socket sock, void *buf, size_t len,
                                                     cplat_ipv4_endpoint *peer_out, size_t *received_out,
                                                     cplat_error *detail_out);

    /**
     *  @brief          指定したバイト数をすべて送信します。
     *  @param[in]      sock       対象のソケット。
     *  @param[in]      buf        送信するデータ。
     *  @param[in]      len        @p buf のバイト数。@ref CPLAT_SOCKET_MAX_TRANSFER 以下を指定します。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @p len 全体を送信するまで繰り返します。途中で送信できなくなった場合は
     *  @ref CPLAT_ERR_UNKNOWN を返します。\n
     *  ブロッキング モードのソケットで使用します。
     *  Linux では、切断済みの接続への送信による SIGPIPE を配信しません。\n
     *  送信エラーの詳細は @p detail_out へ格納します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるソケットに対する呼び出しは同時に実行できます。\n
     *  同一 @p sock に対する並行操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_send_all(cplat_socket sock, const void *buf, size_t len,
                                                     cplat_error *detail_out);

    /**
     *  @brief          指定したバイト数をすべて受信します。
     *  @param[in]      sock       対象のソケット。
     *  @param[out]     buf        受信データの格納先。
     *  @param[in]      len        受信するバイト数。@ref CPLAT_SOCKET_MAX_TRANSFER 以下を指定します。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_EOF 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @p len 全体を受信するまで繰り返します。\n
     *  受信し終える前に相手が送信を終了した場合は @ref CPLAT_ERR_EOF を返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるソケットに対する呼び出しは同時に実行できます。\n
     *  同一 @p sock に対する並行操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_recv_all(cplat_socket sock, void *buf, size_t len, cplat_error *detail_out);

    /**
     *  @brief          ソケットが受信可能になるまで待機します。
     *  @param[in]      sock       対象のソケット。
     *  @param[in]      timeout_ms タイムアウト時間 (ミリ秒)。
     *                             @ref CPLAT_SOCKET_WAIT_FOREVER または
     *                             @ref CPLAT_SOCKET_NO_WAIT も指定できます。
     *  @param[out]     ready_out  受信可能な場合に 1、タイムアウトした場合に 0 を格納します。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  待機がシグナルで中断された場合は、残り時間を再計算して待機を継続します。\n
     *  @p timeout_ms より早く復帰しないため、シグナルの配信によってタイムアウトの
     *  判定が変化することはありません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_wait_readable(cplat_socket sock, int timeout_ms, int *ready_out,
                                                          cplat_error *detail_out);

    /**
     *  @brief          ソケットが送信可能になるまで待機します。
     *  @param[in]      sock       対象のソケット。
     *  @param[in]      timeout_ms タイムアウト時間 (ミリ秒)。
     *                             @ref CPLAT_SOCKET_WAIT_FOREVER または
     *                             @ref CPLAT_SOCKET_NO_WAIT も指定できます。
     *  @param[out]     ready_out  送信可能な場合に 1、タイムアウトした場合に 0 を格納します。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  待機がシグナルで中断された場合は、残り時間を再計算して待機を継続します。\n
     *  @p timeout_ms より早く復帰しないため、シグナルの配信によってタイムアウトの
     *  判定が変化することはありません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_wait_writable(cplat_socket sock, int timeout_ms, int *ready_out,
                                                          cplat_error *detail_out);

    /**
     *  @brief          複数のソケットのいずれかが受信可能になるまで待機します。
     *  @param[in]      socks      対象のソケットの配列。@ref CPLAT_INVALID_SOCKET を含めても構いません。
     *  @param[in]      count      @p socks の要素数。1 以上 @ref CPLAT_SOCKET_WAIT_MAX 以下を指定します。
     *  @param[in]      timeout_ms タイムアウト時間 (ミリ秒)。
     *                             @ref CPLAT_SOCKET_WAIT_FOREVER または
     *                             @ref CPLAT_SOCKET_NO_WAIT も指定できます。
     *  @param[out]     ready_out  各ソケットが受信可能かどうかを格納する配列。
     *                             @p count 以上の要素数が必要です。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @p socks の全要素が @ref CPLAT_INVALID_SOCKET の場合、@p timeout_ms だけ待機してから
     *  @ref CPLAT_OK を返します。呼び出し側のポーリング ループが待機なしで回り続けることを
     *  避けるためです。\n
     *  待機がシグナルで中断された場合は、残り時間を再計算して待機を継続します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_wait_readable_multi(const cplat_socket *socks, size_t count, int timeout_ms,
                                                                unsigned char *ready_out, cplat_error *detail_out);

    /**
     *  @brief          ソケットの受信方向を停止します。
     *  @param[in,out]  sock_inout 対象のソケット。Windows ではソケットを閉じ、
     *                             @ref CPLAT_INVALID_SOCKET を書き戻します。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  受信で待機しているスレッドを解除する目的で使用します。
     *
     *  @warning        本関数はプラットフォームで意味論が異なります。\n
     *                  Linux は受信方向のみを停止してソケットを保持しますが、
     *                  Windows は受信の半クローズで待機を解除できないためソケットを閉じます。\n
     *                  呼び出し側がこの差を意識しなくて済むように、呼び出し後は
     *                  @p sock_inout の値を必ず参照し、@ref CPLAT_INVALID_SOCKET
     *                  でなくなっているかどうかで以後の利用可否を判断してください。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるソケットに対する呼び出しは同時に実行できます。\n
     *  同一 @p sock_inout に対する並行操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_shutdown_receive(cplat_socket *sock_inout, cplat_error *detail_out);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_NET_SOCKET_H */
