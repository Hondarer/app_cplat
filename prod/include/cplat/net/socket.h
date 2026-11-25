/**
 *******************************************************************************
 *  @file           socket.h
 *  @brief          IPv4 ソケットの生成、接続、送受信、待機を行う API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/08/11
 *  @version        1.0.0
 *
 *  プラットフォームごとに異なるソケット API を共通インターフェイスで抽象化します。
 *
 *  Table: プラットフォーム別のソケット API
 *
 *  | OS      | 使用ライブラリ                    | 待機 API   | 非ブロッキング設定       |
 *  | ------- | --------------------------------- | ---------- | ------------------------ |
 *  | Linux   | BSD ソケット (libc)               | `poll`     | `fcntl` (`O_NONBLOCK`)   |
 *  | Windows | Winsock2 (`ws2_32.lib`)           | `WSAPoll`  | `ioctlsocket` (`FIONBIO`)|
 *
 *  Winsock の初期化と終了は本モジュールの内部で行うため、利用側は初期化順序を
 *  意識せずに任意のタイミングで API を呼び出せます。\n
 *  待機を伴う API の設計原則は @ref sync.h を参照してください。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/*
 * このヘッダーは多数のソース ファイルから参照されるため、
 * @hideincludedbygraph によって "Included by" グラフを無効にする。
 */

#ifndef CPLAT_NET_SOCKET_H
#define CPLAT_NET_SOCKET_H

#include <cplat/base/error.h>
#include <cplat/base/result.h>
#include <cplat/base/wait.h>
#include <cplat/cplat_export.h>
#include <cplat/net/endpoint.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>

/** @ref cplat_socket_listen へ既定の待ち受けキュー長を指定する値。 */
#define CPLAT_SOCKET_BACKLOG_DEFAULT 0

/** @ref cplat_socket_set_multicast_ttl へ指定できる TTL の最大値。 */
#define CPLAT_SOCKET_MULTICAST_TTL_MAX 255U

/** @ref cplat_socket_wait_readable_multi および @ref cplat_socket_timed_wait_readable_multi へ一度に指定できるソケットの最大数。 */
#define CPLAT_SOCKET_WAIT_MAX 16U

/** @ref cplat_socket_watch_wait_readable_multi へ一度に指定できる登録の最大数。 */
#define CPLAT_SOCKET_WATCH_WAIT_MAX CPLAT_SOCKET_WAIT_MAX

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

    /** 待機の解除のオブジェクト (不透明型)。 */
    typedef struct cplat_socket_wait_cancel cplat_socket_wait_cancel;

    /** ソケットの登録のオブジェクト (不透明型)。 */
    typedef struct cplat_socket_watch cplat_socket_watch;

/** 無効なソケットを表す値。 */
#define CPLAT_INVALID_SOCKET ((cplat_socket) - 1)

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
     *  解放経路で呼び出しても、呼び出し前に記録された診断情報が失われません。\n
     *  閉じる処理が失敗しても、失敗は通知しません。呼び出し側に、失敗を受けて取れる手段がないためです。
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
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_WOULD_BLOCK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  失敗した場合、@p sock_out へ @ref CPLAT_INVALID_SOCKET を格納します。\n
     *  返すソケットは、リスナーの登録やモードにかかわらず、登録なしでブロッキング モードです。
     *  正規化に失敗した場合は返すソケットを閉じ、@p sock_out に @ref CPLAT_INVALID_SOCKET を格納します。
     *  非ブロッキング モードで接続待ちがない場合は @ref CPLAT_ERR_WOULD_BLOCK を返し、
     *  @p detail_out の要因は @ref CPLAT_CAUSE_WOULD_BLOCK になります。
     *  see: https://man7.org/linux/man-pages/man2/accept.2.html
     *  see: https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsaeventselect
     *  see: https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-ioctlsocket
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
     *  この場合は @ref cplat_socket_wait_writable または @ref cplat_socket_timed_wait_writable で完了を待ち、
     *  @ref cplat_socket_get_pending_error で結果を確認します。
     *  @ref cplat_socket_watch_attach 中のソケットには使用せず、@ref cplat_socket_watch_connect を使用します。
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
     *  @brief          ソケットの受信用に OS が保持できる領域の大きさを取得します。
     *  @param[in]      sock       対象のソケット。
     *  @param[out]     size_out   受信用の領域の大きさ [byte] の格納先。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  OS の `SO_RCVBUF` の取得値をそのまま @p size_out へ格納します。
     *  取得値が 0 の場合も、エラーとはせず 0 を格納して @ref CPLAT_OK を返します。
     *  OS が負の値を返した場合は、大きさとして表せないため @ref CPLAT_ERR_UNKNOWN を返します。\n
     *  失敗した場合、@p size_out は変更しません。
     *
     *  Linux の取得値は、設定した値の 2 倍になります (管理情報の分を含むため)。
     *  Windows の取得値は、OS が保持する値です。\n
     *  いずれのプラットフォームでも、アプリケーションが 1 回に受け取れるデータの大きさとは限りません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_get_receive_buffer_size(cplat_socket sock, size_t *size_out,
                                                                    cplat_error *detail_out);

    /**
     *  @brief          ソケットの受信用に OS が保持する領域の大きさを要求します。
     *  @param[in]      sock       対象のソケット。
     *  @param[in]      size       要求する受信用の領域の大きさ [byte]。1 以上 `INT_MAX` 以下を指定します。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  OS の `SO_RCVBUF` へ @p size をそのまま設定します。\n
     *  @p size に 0 または `INT_MAX` を超える値を指定した場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。\n
     *  設定後に実際に確保された大きさは、@ref cplat_socket_get_receive_buffer_size で取得します。
     *
     *  Linux は、設定値を 2 倍にして保持し (管理情報の分を含むため)、上限 (`net.core.rmem_max`) で
     *  切り詰めます (see: https://man7.org/linux/man-pages/man7/socket.7.html )。\n
     *  Windows は、指定した大きさを `SO_RCVBUF` として要求します
     *  (see: https://learn.microsoft.com/en-us/windows/win32/winsock/sol-socket-socket-options )。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_set_receive_buffer_size(cplat_socket sock, size_t size,
                                                                    cplat_error *detail_out);

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
     *  @brief          TCP の Nagle アルゴリズムを抑止するかどうかを設定します。
     *  @param[in]      sock       対象のソケット。
     *  @param[in]      enable     抑止する場合は 0 以外、抑止しない場合は 0。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         成功時は @ref CPLAT_OK 、無効な引数のときは
     *                  @ref CPLAT_ERR_INVALID_ARGUMENT 、その他の OS
     *                  エラーのときは、そのエラーに対応する共通結果コードを返します。
     *
     *  0 以外を指定すると `TCP_NODELAY` に 1 を設定し、Nagle アルゴリズムを抑止します。\n
     *  0 を指定すると `TCP_NODELAY` に 0 を設定し、Nagle アルゴリズムを有効にします。\n
     *  この関数は指定したソケットだけを変更し、ソケット生成時の既定は変えません。\n
     *  設定に失敗した OS エラーは、共通結果コードへ変換して返します。
     *  変換先を一つに決められないエラーの例は @ref CPLAT_ERR_UNKNOWN です。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_set_nodelay(cplat_socket sock, int enable, cplat_error *detail_out);

    /**
     *  @brief          マルチキャスト送信に使用するローカル インターフェイスを設定します。
     *  @param[in]      sock       対象のソケット。
     *  @param[in]      interface_address ローカル インターフェイスのアドレス
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
     *  @brief          マルチキャスト送信の TTL (経由できるルーターの数の上限) を設定します。
     *  @param[in]      sock       対象のソケット。
     *  @param[in]      ttl        設定する TTL。0 以上 @ref CPLAT_SOCKET_MULTICAST_TTL_MAX 以下を指定します。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  OS の失敗に対応する結果コードのいずれかを返します。
     *
     *  OS の `IP_MULTICAST_TTL` (`IPPROTO_IP`) へ @p ttl を設定します。このソケットから送る
     *  マルチキャストのパケットにだけ働き、ユニキャストとブロードキャストの TTL は変えません。\n
     *  @p ttl が @ref CPLAT_SOCKET_MULTICAST_TTL_MAX を超える場合は、OS を呼ばず
     *  @ref CPLAT_ERR_INVALID_ARGUMENT を返します (IPv4 の TTL は 8 ビットのため)。\n
     *  設定しない場合の値は OS の既定値です (Linux は 1)。\n
     *  Linux は `int`、Windows は `DWORD` の値として設定します。
     *  see: https://man7.org/linux/man-pages/man2/IP_MULTICAST_TTL.2const.html
     *  see: https://learn.microsoft.com/en-us/windows/win32/winsock/ipproto-ip-socket-options
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_set_multicast_ttl(cplat_socket sock, unsigned int ttl,
                                                              cplat_error *detail_out);

    /**
     *  @brief          マルチキャスト グループへ参加します。
     *  @param[in]      sock       対象のソケット。
     *  @param[in]      group_address マルチキャスト グループのアドレス
     *                             (ネットワーク バイト オーダー)。
     *  @param[in]      interface_address 受信に使用するローカル インターフェイスのアドレス
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
     *  @param[in]      interface_address 受信に使用していたローカル インターフェイスのアドレス
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
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_WOULD_BLOCK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  1 回の呼び出しで @p len 全体を送信するとは限りません。\n
     *  全体の送信を保証する場合は @ref cplat_socket_send_all を使用します。
     *  Linux では、切断済みの接続への送信による SIGPIPE を配信しません。\n
     *  送信エラーは @ref CPLAT_ERR_UNKNOWN で通知し、@p detail_out へ詳細を格納します。
     *  非ブロッキング ソケットで待つ必要がある場合は @ref CPLAT_ERR_WOULD_BLOCK を返します。
     *  @ref cplat_socket_watch_attach 中のソケットでは使用せず、@ref cplat_socket_watch_send を使用します。
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
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_WOULD_BLOCK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  非ブロッキング ソケットで待つ必要がある場合は @ref CPLAT_ERR_WOULD_BLOCK を返します。
     *  @ref cplat_socket_watch_attach 中のソケットでも使用できます。待つ必要がある場合は、
     *  @ref cplat_socket_watch_wait_readable_multi で待機します。
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
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_WOULD_BLOCK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  非ブロッキング ソケットで待つ必要がある場合は @ref CPLAT_ERR_WOULD_BLOCK を返します。
     *  @ref cplat_socket_watch_attach 中のソケットでは使用せず、@ref cplat_socket_watch_sendto を使用します。
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
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_WOULD_BLOCK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  非ブロッキング ソケットで待つ必要がある場合は @ref CPLAT_ERR_WOULD_BLOCK を返します。
     *  @ref cplat_socket_watch_attach 中のソケットでは使用できます。
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
     *  @ref cplat_socket_watch_attach 中のソケットでは使用せず、@ref cplat_socket_watch_send_all を使用します。
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
     *  @ref cplat_socket_watch_attach 中のソケットでは使用せず、@ref cplat_socket_watch_recv_all を使用します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるソケットに対する呼び出しは同時に実行できます。\n
     *  同一 @p sock に対する並行操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_recv_all(cplat_socket sock, void *buf, size_t len, cplat_error *detail_out);

    /**
     *  @brief          ソケットが受信可能になるまで無期限に待機します。
     *  @param[in]      sock       対象のソケット。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         受信可能になった場合は @ref CPLAT_OK 、
     *                  @ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  待機がシグナルで中断された場合は、待機を継続します。
     *  @ref cplat_socket_watch_attach 中のソケットでは使用しないでください。
     *  解除できる待機には @ref cplat_socket_watch_wait_readable_multi を使用します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_wait_readable(cplat_socket sock, cplat_error *detail_out);

    /**
     *  @brief          ソケットが受信可能になるまで指定期限内で待機します。
     *  @param[in]      sock       対象のソケット。
     *  @param[in]      timeout_ms タイムアウト時間 [ms]。
     *                             @ref CPLAT_WAIT_FOREVER または @ref CPLAT_NO_WAIT も指定できます。
     *                             負値を渡した場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         受信可能になった場合は @ref CPLAT_OK 、タイムアウトした場合は @ref CPLAT_ERR_TIMEOUT
     *                  (@p detail_out には Linux では @c ETIMEDOUT 、Windows では @c WSAETIMEDOUT を格納) 、
     *                  @ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  待機がシグナルで中断された場合は、残り時間を再計算して待機を継続します。\n
     *  @p timeout_ms より早く復帰しないため、シグナルの配信によってタイムアウトの
     *  判定が変化することはありません。
     *  @ref cplat_socket_watch_attach 中のソケットでは使用しないでください。
     *  解除できる待機には @ref cplat_socket_watch_wait_readable_multi を使用します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_timed_wait_readable(cplat_socket sock, int timeout_ms,
                                                                cplat_error *detail_out);

    /**
     *  @brief          ソケットが送信可能になるまで無期限に待機します。
     *  @param[in]      sock       対象のソケット。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         送信可能になった場合は @ref CPLAT_OK 、
     *                  @ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  待機がシグナルで中断された場合は、待機を継続します。
     *  @ref cplat_socket_watch_attach 中のソケットでは使用しないでください。
     *  解除できる待機には @ref cplat_socket_watch_wait_writable を使用します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_wait_writable(cplat_socket sock, cplat_error *detail_out);

    /**
     *  @brief          ソケットが送信可能になるまで指定期限内で待機します。
     *  @param[in]      sock       対象のソケット。
     *  @param[in]      timeout_ms タイムアウト時間 [ms]。
     *                             @ref CPLAT_WAIT_FOREVER または @ref CPLAT_NO_WAIT も指定できます。
     *                             負値を渡した場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         送信可能になった場合は @ref CPLAT_OK 、タイムアウトした場合は @ref CPLAT_ERR_TIMEOUT
     *                  (@p detail_out には Linux では @c ETIMEDOUT 、Windows では @c WSAETIMEDOUT を格納) 、
     *                  @ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  待機がシグナルで中断された場合は、残り時間を再計算して待機を継続します。\n
     *  @p timeout_ms より早く復帰しないため、シグナルの配信によってタイムアウトの
     *  判定が変化することはありません。
     *  @ref cplat_socket_watch_attach 中のソケットでは使用しないでください。
     *  解除できる待機には @ref cplat_socket_watch_wait_writable を使用します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_timed_wait_writable(cplat_socket sock, int timeout_ms,
                                                                cplat_error *detail_out);

    /**
     *  @brief          複数のソケットのいずれかが受信可能になるまで無期限に待機します。
     *  @param[in]      socks      対象のソケットの配列。@ref CPLAT_INVALID_SOCKET を含めても構いませんが、
     *                             有効なソケットが 1 つもない場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *  @param[in]      count      @p socks の要素数。1 以上 @ref CPLAT_SOCKET_WAIT_MAX 以下を指定します。
     *  @param[out]     ready_out  各ソケットが受信可能かどうかを格納する配列。
     *                             @p count 以上の要素数が必要です。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  待機がシグナルで中断された場合は、待機を継続します。
     *  @ref cplat_socket_watch_attach 中のソケットでは使用しないでください。
     *  解除できる待機には @ref cplat_socket_watch_wait_readable_multi を使用します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_wait_readable_multi(const cplat_socket *socks, size_t count,
                                                                unsigned char *ready_out, cplat_error *detail_out);

    /**
     *  @brief          複数のソケットのいずれかが受信可能になるまで指定期限内で待機します。
     *  @param[in]      socks      対象のソケットの配列。@ref CPLAT_INVALID_SOCKET を含めても構いません。
     *  @param[in]      count      @p socks の要素数。1 以上 @ref CPLAT_SOCKET_WAIT_MAX 以下を指定します。
     *  @param[in]      timeout_ms タイムアウト時間 [ms]。
     *                             @ref CPLAT_WAIT_FOREVER または @ref CPLAT_NO_WAIT も指定できます。
     *                             負値を渡した場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *  @param[out]     ready_out  各ソケットが受信可能かどうかを格納する配列。
     *                             @p count 以上の要素数が必要です。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_TIMEOUT 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @p socks の全要素が @ref CPLAT_INVALID_SOCKET の場合、@p timeout_ms が @ref CPLAT_WAIT_FOREVER
     *  であれば @ref CPLAT_ERR_INVALID_ARGUMENT を返し、有限の待ち時間であれば @p timeout_ms だけ
     *  待機してから @ref CPLAT_ERR_TIMEOUT を返します (@p ready_out はすべて 0、@p detail_out には
     *  Linux では @c ETIMEDOUT 、Windows では @c WSAETIMEDOUT を格納)。
     *  呼び出し側のポーリング ループが待機なしで回り続けることを避けるためです。\n
     *  期限内にいずれも受信可能にならなかった場合も、@p ready_out をすべて 0 にして
     *  @ref CPLAT_ERR_TIMEOUT を返します。\n
     *  待機がシグナルで中断された場合は、残り時間を再計算して待機を継続します。
     *  @ref cplat_socket_watch_attach 中のソケットでは使用しないでください。
     *  解除できる待機には @ref cplat_socket_watch_wait_readable_multi を使用します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_timed_wait_readable_multi(const cplat_socket *socks, size_t count,
                                                                      int timeout_ms, unsigned char *ready_out,
                                                                      cplat_error *detail_out);

    /**
     *  @brief          待機の解除のオブジェクトを生成します。
     *  @param[out]     cancel_out 生成したオブジェクトの格納先。失敗時は NULL を格納します。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_OUT_OF_MEMORY 、
     *                  OS の失敗に対応する結果コードのいずれかを返します。
     *
     *  生成の直後は未通知の状態です。Linux は `eventfd` (`EFD_NONBLOCK | EFD_CLOEXEC`)、
     *  Windows は手動リセットのイベント オブジェクトで作ります。
     *  see: https://man7.org/linux/man-pages/man2/eventfd.2.html
     *  see: https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsacreateevent
     *  see: https://learn.microsoft.com/en-us/windows/win32/sync/event-objects
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_wait_cancel_create(cplat_socket_wait_cancel **cancel_out,
                                                               cplat_error *detail_out);

    /**
     *  @brief          待機の解除を通知します。
     *  @param[in]      cancel     対象のオブジェクト。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         有効な @p cancel では必ず @ref CPLAT_OK 、NULL では @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *
     *  このオブジェクトを指定して待機しているすべての呼び出しを解除します。
     *  通知は @ref cplat_socket_wait_cancel_reset まで続き、その間に始めた待機もすぐに解除します。\n
     *  通知済みのオブジェクトへの再度の通知は、何もせず @ref CPLAT_OK を返します。\n
     *  通知状態を原子的に保持してから OS へ起床を通知します。OS の通知が失敗しても、待機側の
     *  最大 50 [ms] ごとの状態確認で解除を検出するため、本関数は OS の失敗を返しません。
     *  有効なオブジェクトとは、生成に成功し、破棄されず、リセットと破棄が通知や待機と競合しないものです。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。どのスレッドからも何度でも呼び出せます。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_wait_cancel_signal(cplat_socket_wait_cancel *cancel,
                                                               cplat_error *detail_out);

    /**
     *  @brief          待機の解除のオブジェクトを未通知の状態へ戻します。
     *  @param[in]      cancel     対象のオブジェクト。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、OS の失敗に対応する結果コードを返します。
     *
     *  未通知のオブジェクトへの呼び出しは、何もせず @ref CPLAT_OK を返します。\n
     *  このオブジェクトを指定して待機しているスレッドがなく、通知するスレッドもない状態で呼び出してください。
     *  リセットに失敗した場合はオブジェクトを再利用せず、破棄して生成し直します。
     *
     *  @par            スレッド セーフ
     *  同じオブジェクトへのほかの関数との同時呼び出しはできません。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_wait_cancel_reset(cplat_socket_wait_cancel *cancel,
                                                              cplat_error *detail_out);

    /**
     *  @brief          待機の解除のオブジェクトを破棄します。
     *  @param[in]      cancel 破棄するオブジェクト。NULL の場合は何もしません。
     *
     *  このオブジェクトを指定した待機がすべて戻った後に呼び出してください。
     *  直前エラー (@ref cplat_error_get_last) を保存し、復元してから返ります。
     *
     *  @par            スレッド セーフ
     *  同じオブジェクトへのほかの関数との同時呼び出しはできません。
     */
    CPLAT_EXPORT void CPLAT_API cplat_socket_wait_cancel_dispose(cplat_socket_wait_cancel *cancel);

    /**
     *  @brief          ソケットを解除できる待機の対象として登録します。
     *  @param[in]      sock       対象のソケット。
     *  @param[out]     watch_out  登録のオブジェクトの格納先。失敗時は NULL を格納します。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、@ref CPLAT_ERR_OUT_OF_MEMORY 、
     *                  OS の失敗に対応する結果コードのいずれかを返します。
     *
     *  登録に成功すると、両方の OS でソケットを非ブロッキング モードにします。
     *  失敗した場合、ソケットのモードと登録は呼び出し前のままです。\n
     *  登録中は次の規則に従ってください。
     *  - @ref cplat_socket_set_nonblocking でブロッキングへ戻さない (Windows では失敗します)。
     *  - 既存の待機関数 (@ref cplat_socket_wait_readable 、@ref cplat_socket_timed_wait_readable 、
     *    @ref cplat_socket_wait_writable 、@ref cplat_socket_timed_wait_writable 、
     *    @ref cplat_socket_wait_readable_multi 、@ref cplat_socket_timed_wait_readable_multi) を使わない。
     *  - 送信は @ref cplat_socket_watch_send 、@ref cplat_socket_watch_sendto 、
     *    @ref cplat_socket_watch_send_all だけで行う。@ref cplat_socket_send 、@ref cplat_socket_sendto 、
     *    @ref cplat_socket_send_all は使わない (Windows で送信可能の状態を管理できなくなるためです)。
     *  - 接続は @ref cplat_socket_watch_connect で行う。
     *  - 受信は @ref cplat_socket_recv 、@ref cplat_socket_recvfrom 、@ref cplat_socket_accept 、
     *    @ref cplat_socket_watch_recv_all で行う。@ref cplat_socket_recv_all は使わない。
     *    1 回の受信は待つ必要がある場合に
     *    @ref CPLAT_ERR_WOULD_BLOCK を返します。
     *  - 同じソケットを複数の登録へ重ねて登録しない。
     *
     *  登録前から受信可能なデータや保留中の接続がある場合も、最初の待機で検出します。
     *  ソケット オプションの設定 (@ref cplat_socket_set_nodelay など) は登録中も行えます。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_watch_attach(cplat_socket sock, cplat_socket_watch **watch_out,
                                                         cplat_error *detail_out);

    /**
     *  @brief          ソケットの登録を解き、登録のオブジェクトを破棄します。
     *  @param[in]      watch 対象の登録。NULL の場合は何もしません。
     *
     *  ソケットは閉じません。非ブロッキング モードのまま残ります。
     *  ブロッキングへ戻す場合は本関数の後に @ref cplat_socket_set_nonblocking を呼びます。\n
     *  登録を使う関数がすべて戻った後に呼び出してください。ソケットを閉じる場合は本関数の後に閉じます。
     *  失敗を通知しない解放関数であり、直前エラーを保存して復元してから返ります。
     *
     *  @par            スレッド セーフ
     *  同じ登録へのほかの関数との同時呼び出しはできません。
     */
    CPLAT_EXPORT void CPLAT_API cplat_socket_watch_detach(cplat_socket_watch *watch);

    /**
     *  @brief          登録したソケットのいずれかが受信可能になるか、解除が通知されるまで待機します。
     *  @param[in]      watches    登録の配列。NULL の要素を含めても構いません。
     *  @param[in]      count      @p watches の要素数。1 以上 @ref CPLAT_SOCKET_WATCH_WAIT_MAX 以下。
     *  @param[in]      cancel     解除オブジェクト。NULL の場合は解除できません。
     *  @param[in]      timeout_ms タイムアウト時間 [ms]。@ref CPLAT_WAIT_FOREVER または @ref CPLAT_NO_WAIT も指定できます。
     *  @param[out]     ready_out  各登録が受信可能かどうかを格納する配列。@p count 以上の要素数が必要です。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         受信可能になった場合は @ref CPLAT_OK 、解除された場合は @ref CPLAT_ERR_CANCELED 、
     *                  期限内に何も起きない場合は @ref CPLAT_ERR_TIMEOUT 、
     *                  @ref CPLAT_ERR_INVALID_ARGUMENT 、OS の失敗に対応する結果コードを返します。
     *
     *  受信可能はデータの到着、接続要求の到着 (リスナー)、相手の切断、ソケット エラーを含みます。
     *  切断とエラーは続く受信関数で EOF またはエラーとして得られます。
     *  受信可能な登録では受信関数 (@ref cplat_socket_recv 、@ref cplat_socket_recvfrom 、
     *  @ref cplat_socket_accept) を少なくとも 1 回呼びます。Windows は受信関数の呼び出しで次の通知を再開します。
     *  受信関数が @ref CPLAT_ERR_WOULD_BLOCK を返した場合は待機へ戻ります。\n
     *  解除と受信可能が同時の場合は @ref CPLAT_ERR_CANCELED を返し、@p ready_out をすべて 0 にします。
     *  全要素が NULL の場合、解除と期限だけを待ちます。解除が NULL で無期限の場合は不正な引数です。
     *  シグナルと無関係な通知では残り時間を再計算し、@p timeout_ms より早くタイムアウトを返しません。
     *  タイムアウトと解除の詳細値は Linux では `ETIMEDOUT` と `ECANCELED`、Windows では
     *  `WSAETIMEDOUT` と `WSA_E_CANCELLED` です。
     *
     *  @par            スレッド セーフ
     *  同じ登録の受信側関数を同時に呼べるのは 1 スレッドです。送信側関数 1 スレッドとは同時に呼べます。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_watch_wait_readable_multi(cplat_socket_watch *const *watches,
                                                                       size_t count,
                                                                       cplat_socket_wait_cancel *cancel,
                                                                       int timeout_ms, unsigned char *ready_out,
                                                                       cplat_error *detail_out);

    /**
     *  @brief          登録したソケットが送信可能になるか、解除が通知されるまで待機します。
     *  @param[in]      watch      対象の登録。
     *  @param[in]      cancel     解除オブジェクト。NULL の場合は解除できません。
     *  @param[in]      timeout_ms タイムアウト時間 [ms]。@ref CPLAT_WAIT_FOREVER または @ref CPLAT_NO_WAIT も指定できます。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         送信可能なら @ref CPLAT_OK 、解除なら @ref CPLAT_ERR_CANCELED 、
     *                  期限切れなら @ref CPLAT_ERR_TIMEOUT 、その他は引数や OS のエラーに対応する結果コードを返します。
     *
     *  @ref CPLAT_NO_WAIT は送信の空きがあるかどうかだけを確かめます。
     *  同じ登録で受信待機中に呼んでも受信側の通知を失わせません。
     *  相手の切断またはソケット エラーも送信可能として @ref CPLAT_OK を返します (両方の OS)。
     *  送信の成功は保証しないため、続く送信で結果を確認します。
     *  解除と送信可能 (切断とエラーを含む) が同時の場合は @ref CPLAT_ERR_CANCELED を返します。
     *
     *  @par            スレッド セーフ
     *  同じ登録の送信側関数を同時に呼べるのは 1 スレッドです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_watch_wait_writable(cplat_socket_watch *watch,
                                                                cplat_socket_wait_cancel *cancel, int timeout_ms,
                                                                cplat_error *detail_out);

    /**
     *  @brief          登録した接続済みソケットへ待たずに 1 回送信します。
     *  @param[in]      watch      対象の登録。
     *  @param[in]      buf        送信するデータ。
     *  @param[in]      len        @p buf のバイト数。@ref CPLAT_SOCKET_MAX_TRANSFER 以下。
     *  @param[out]     sent_out   送信できたバイト数の格納先。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_WOULD_BLOCK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  OS の失敗に対応する結果コードのいずれかを返します。
     *
     *  送信の空きがない場合は何も送らず @ref CPLAT_ERR_WOULD_BLOCK を返します。@p len の一部だけ送る場合があります。
     *  Linux では切断済みの接続への送信による SIGPIPE を配信しません。
     *
     *  @par            スレッド セーフ
     *  @ref cplat_socket_watch_wait_writable と同じ送信側の規則です。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_watch_send(cplat_socket_watch *watch, const void *buf, size_t len,
                                                       size_t *sent_out, cplat_error *detail_out);

    /**
     *  @brief          登録したソケットから指定エンドポイントへ待たずに 1 回送信します。
     *  @param[in]      watch      対象の登録。
     *  @param[in]      buf        送信するデータ。
     *  @param[in]      len        @p buf のバイト数。@ref CPLAT_SOCKET_MAX_TRANSFER 以下。
     *  @param[in]      endpoint   送信先のエンドポイント。
     *  @param[out]     sent_out   送信できたバイト数の格納先。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         @ref cplat_socket_watch_send と同じ結果を返します。
     *
     *  送信結果と登録の並行規則は @ref cplat_socket_watch_send に準じます。
     *
     *  @par            スレッド セーフ
     *  @ref cplat_socket_watch_wait_writable と同じ送信側の規則です。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_watch_sendto(cplat_socket_watch *watch, const void *buf, size_t len,
                                                         const cplat_ipv4_endpoint *endpoint, size_t *sent_out,
                                                         cplat_error *detail_out);

    /**
     *  @brief          登録した接続済みソケットへ指定バイト数をすべて送信します。
     *  @param[in]      watch      対象の登録。
     *  @param[in]      buf        送信するデータ。
     *  @param[in]      len        @p buf のバイト数。@ref CPLAT_SOCKET_MAX_TRANSFER 以下。
     *  @param[in]      cancel     解除オブジェクト。NULL 可。
     *  @param[in]      timeout_ms 全体の期限 [ms]。@ref CPLAT_WAIT_FOREVER も指定できます。
     *  @param[out]     sent_out   送信できたバイト数。NULL 可。失敗時も送信済みの数を格納します。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_CANCELED 、@ref CPLAT_ERR_TIMEOUT 、
     *                  @ref CPLAT_ERR_INVALID_ARGUMENT 、OS の失敗に対応する結果コードを返します。
     *
     *  送信の空きがない間は、送信可能になるか、解除、期限まで待ちます。各反復で解除、完了、期限を確認してから
     *  非ブロッキング送信を 1 回行います。終了による起床後の再送信も待つ必要がある場合は
     *  @ref CPLAT_ERR_UNKNOWN (詳細 EIO) を返します。
     *  開始時に単調時計から全体の絶対期限を一度だけ求めます。EINTR の後も確認へ戻り、少量ずつ送信できる間も
     *  確認を省きません。確認の優先順位は解除、完了、期限です。@ref CPLAT_NO_WAIT では、長さ 0 以外は
     *  最初の送信を行う前に @ref CPLAT_ERR_TIMEOUT を返します。
     *  一部送信後に解除または期限で戻った場合、ストリームは途中で切れています。接続を閉じてください。
     *  Linux では切断済みの接続への送信による SIGPIPE を配信しません。
     *  see: https://man7.org/linux/man-pages/man2/send.2.html
     *
     *  @par            スレッド セーフ
     *  @ref cplat_socket_watch_wait_writable と同じ送信側の規則です。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_watch_send_all(cplat_socket_watch *watch, const void *buf, size_t len,
                                                           cplat_socket_wait_cancel *cancel, int timeout_ms,
                                                           size_t *sent_out, cplat_error *detail_out);

    /**
     *  @brief          登録した接続済みソケットから指定バイト数をすべて受信します。
     *  @param[in]      watch        対象の登録。
     *  @param[out]     buf          受信データの格納先。
     *  @param[in]      len          受信するバイト数。@ref CPLAT_SOCKET_MAX_TRANSFER 以下。
     *  @param[in]      cancel       解除オブジェクト。NULL 可。
     *  @param[in]      timeout_ms   全体の期限 [ms]。@ref CPLAT_WAIT_FOREVER も指定できます。
     *  @param[out]     received_out 受信したバイト数。NULL 可。失敗時も受信済みの数を格納します。
     *  @param[out]     detail_out   エラー詳細の格納先。NULL 可。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_EOF 、@ref CPLAT_ERR_CANCELED 、@ref CPLAT_ERR_TIMEOUT 、
     *                  @ref CPLAT_ERR_INVALID_ARGUMENT 、OS の失敗に対応する結果コードを返します。
     *
     *  受信し終える前に相手が送信を終了した場合は @ref CPLAT_ERR_EOF を返します。
     *  開始時に単調時計から全体の絶対期限を一度だけ求めます。各反復で解除、完了、期限をこの順で確認してから
     *  非ブロッキング受信を 1 回行います。EINTR の後も確認へ戻り、少量ずつ受信できる間も確認を省きません。
     *  @ref CPLAT_NO_WAIT では、長さ 0 以外は最初の受信を行う前に @ref CPLAT_ERR_TIMEOUT を返します。
     *  終了による起床後の受信も待つ必要がある場合は @ref CPLAT_ERR_UNKNOWN (詳細 EIO) を返します。
     *  解除や期限で途中終了した場合、受信済みのデータは @p buf の先頭にあり、残りは同じ接続から続けて読めます。
     *
     *  @par            スレッド セーフ
     *  @ref cplat_socket_watch_wait_readable_multi と同じ受信側の規則です。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_watch_recv_all(cplat_socket_watch *watch, void *buf, size_t len,
                                                           cplat_socket_wait_cancel *cancel, int timeout_ms,
                                                           size_t *received_out, cplat_error *detail_out);

    /**
     *  @brief          登録した未接続ソケットで接続し、完了、解除、期限まで待ちます。
     *  @param[in]      watch      対象の登録。
     *  @param[in]      endpoint   接続先のエンドポイント。
     *  @param[in]      cancel     解除オブジェクト。NULL 可。
     *  @param[in]      timeout_ms 期限 [ms]。@ref CPLAT_WAIT_FOREVER も指定できます。
     *  @param[out]     detail_out エラー詳細の格納先。NULL 可。
     *  @return         接続成功は @ref CPLAT_OK 、解除は @ref CPLAT_ERR_CANCELED 、期限切れは @ref CPLAT_ERR_TIMEOUT 、
     *                  その他は引数、接続または OS のエラーに対応する結果コードを返します。
     *
     *  接続失敗の理由は @p detail_out に OS エラーを格納します (Linux は `SO_ERROR`、Windows は `FD_CONNECT`)。
     *  @ref CPLAT_OK 以外で戻った場合、接続状態は定まりません。登録を解いてソケットを閉じてください。
     *  同じソケットで接続をやり直すことはできません。
     *
     *  @par            スレッド セーフ
     *  @ref cplat_socket_watch_wait_writable と同じ送信側の規則です。
     */
    CPLAT_EXPORT int CPLAT_API cplat_socket_watch_connect(cplat_socket_watch *watch,
                                                          const cplat_ipv4_endpoint *endpoint,
                                                          cplat_socket_wait_cancel *cancel, int timeout_ms,
                                                          cplat_error *detail_out);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_NET_SOCKET_H */
