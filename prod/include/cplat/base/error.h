/**
 *******************************************************************************
 *  @file           error.h
 *  @brief          OS 由来の詳細エラーをドメイン付きで保持する API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/07/31
 *  @version        1.0.0
 *
 *  errno と Win32 エラー コードを取り違えずに保持し、直前の cplat API
 *  が記録した詳細エラーの取得と、プラットフォーム共通の要因判定を提供します。
 *
 *  @section        error_last_contract 直前値の契約
 *
 *  `detail_out` を引数に持つ API は、失敗時に出力引数と現在のスレッドの直前値へ
 *  同じ詳細を記録し、成功時に両方をクリアします。\n
 *  出力引数へ NULL を指定した場合も、スレッドの直前値は更新されます。\n
 *  したがって cplat_error_get_last() は、直前に呼び出した対応 API の結果を
 *  成功と失敗のどちらであっても反映します。
 *
 *  直前値は次の対応 API の呼び出しで上書きされるため、以下は保証しません。
 *
 *  - スレッドをまたいだ参照 (直前値はスレッドごとに独立しています)
 *  - 対応 API 以外の関数を挟んだ後の参照
 *
 *  詳細を確実に保持する必要がある場合は `detail_out` を使用するか、
 *  cplat_error_get_last() の値を直ちにコピーしてください。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

#ifndef CPLAT_BASE_ERROR_H
#define CPLAT_BASE_ERROR_H

#include <cplat/base/platform.h>
#include <cplat/base/result.h>
#include <cplat/cplat_export.h>

/**
 *  @ingroup        CPLAT_BASE
 *  @{
 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          詳細エラー値の由来を表します。
     *
     *  @ref cplat_error::code の値をどの番号体系で解釈するかを示します。\n
     *  @ref cplat_error_get_cause() は、ドメインごとに専用の変換表で
     *  @ref cplat_error_cause へ変換します。
     *
     *  @note           `ERRNO` と `SOCKET_ERRNO` は Linux では同じ errno の番号体系ですが、
     *                  ドメインを分けています。\n
     *                  ソケット操作の `EAGAIN`/`EWOULDBLOCK` は非ブロッキング操作の待機を表し
     *                  @ref CPLAT_CAUSE_WOULD_BLOCK になりますが、
     *                  通常のファイル I/O やプロセス生成の `EAGAIN`
     *                  (`fork()`/`pthread_create()` の資源上限超過など) は
     *                  @ref CPLAT_CAUSE_BUSY のままです。\n
     *                  同じ生の値でも呼び出し文脈で意味が異なるため、ドメインで区別します。
     */
    typedef enum cplat_error_domain
    {
        /**
         *  @brief          詳細エラーが設定されていません。
         *
         *  @ref cplat_error::code は未定義であり、参照してはなりません。\n
         *  成功時、および詳細エラーの初期化直後 (@ref cplat_error_clear()) はこの値です。
         */
        CPLAT_ERROR_DOMAIN_NONE = 0,

        /**
         *  @brief          errno の値を保持しています。
         *
         *  @ref cplat_error::code は `<errno.h>` の errno 値です。\n
         *  @ref cplat_error_capture_errno()、@ref cplat_error_capture_current_errno()、
         *  および CRT/POSIX ラッパー (ファイル I/O、環境変数など) の失敗時に設定されます。\n
         *  ソケット操作の失敗は、このドメインではなく `SOCKET_ERRNO` になります。
         */
        CPLAT_ERROR_DOMAIN_ERRNO = 1,

        /**
         *  @brief          Win32 エラー コードを保持しています (Windows 専用)。
         *
         *  @ref cplat_error::code は `GetLastError()` が返す値です。\n
         *  `cplat_error_capture_windows_error()`、
         *  `cplat_error_capture_current_windows_error()` で設定されます。\n
         *  ソケット操作の失敗は、このドメインではなく `WINSOCK` になります。
         */
        CPLAT_ERROR_DOMAIN_WINDOWS = 2,

        /**
         *  @brief          ソケット操作が設定した errno の値を保持しています。
         *
         *  @ref cplat_error::code は `<errno.h>` の errno 値で、番号体系自体は
         *  `ERRNO` ドメインと同じです。\n
         *  `socket`/`connect`/`send`/`recv` など Linux のソケット API が失敗した際に設定され、
         *  `EAGAIN`/`EWOULDBLOCK` を @ref CPLAT_CAUSE_WOULD_BLOCK
         *  として解釈させるために `ERRNO` と区別しています。
         */
        CPLAT_ERROR_DOMAIN_SOCKET_ERRNO = 3,

        /**
         *  @brief          Winsock エラー コード (WSAGetLastError) を保持しています (Windows 専用)。
         *
         *  @ref cplat_error::code は `WSAGetLastError()` が返す値です。\n
         *  `WSAE*` の番号体系は Win32 の `GetLastError()` とは異なるため、
         *  `WINDOWS` ドメインとは別に扱います。
         */
        CPLAT_ERROR_DOMAIN_WINSOCK = 4,

        /**
         *  @brief          getaddrinfo のエラー コード (EAI_*) を保持しています。
         *
         *  @ref cplat_error::code は `getaddrinfo()` が返す `EAI_*` の値です。\n
         *  `EAI_*` は errno とも Winsock エラーとも異なる番号体系です。\n
         *
         *  @note           `EAI_SYSTEM` (Linux) の場合、実際の要因は `EAI_*` ではなく
         *                  呼び出し時点の `errno` 側にあります。\n
         *                  @ref cplat_error_get_cause() はこのケースを検出すると、
         *                  `errno` を素の errno として分類します
         *                  (ソケット操作ではないため `SOCKET_ERRNO` の特別扱いは適用しません)。
         */
        CPLAT_ERROR_DOMAIN_GAI = 5
    } cplat_error_domain;

    /**
     *  @brief          OS エラーをプラットフォーム共通で判定する要因を表します。
     *
     *  @ref cplat_error_get_cause() が @ref cplat_error::domain に応じて、
     *  errno、Win32 エラー コード、Winsock エラー コード、`getaddrinfo()` の `EAI_*` の
     *  いずれかから変換します。各値の Doxygen コメントに、変換元の生の定数を
     *  ドメインごとに記載します。
     *
     *  値は ABI として固定し、新しい要因は末尾へ追加します。
     */
    typedef enum cplat_error_cause
    {
        /**
         *  @brief          要因が設定されていません。
         *
         *  @ref cplat_error::domain が @ref CPLAT_ERROR_DOMAIN_NONE の場合、
         *  および @p error が NULL の場合に返ります。
         */
        CPLAT_CAUSE_NONE = 0,

        /**
         *  @brief          いずれの分類にも該当しない要因です。
         *
         *  各ドメインの変換表に定数が見つからない場合のフォールバック値です。\n
         *  新しい errno/Win32/Winsock/EAI_* 定数が追加され、まだ cplat 側の
         *  分類表に反映されていない場合もこの値になります。
         */
        CPLAT_CAUSE_OTHER = 1,

        /**
         *  @brief          対象が存在しません。
         *
         *  - errno: `ENOENT` / `ENODEV` / `ENXIO`
         *  - Win32: `ERROR_FILE_NOT_FOUND` / `ERROR_PATH_NOT_FOUND` / `ERROR_BAD_NETPATH` /
         *    `ERROR_INVALID_DRIVE` / `ERROR_SERVICE_DOES_NOT_EXIST`
         *  - GAI:   `EAI_NONAME` / `EAI_NODATA`
         */
        CPLAT_CAUSE_NOT_FOUND = 2,

        /**
         *  @brief          対象がすでに存在します。
         *
         *  - errno: `EEXIST`
         *  - Win32: `ERROR_FILE_EXISTS` / `ERROR_ALREADY_EXISTS` / `ERROR_SERVICE_EXISTS`
         */
        CPLAT_CAUSE_ALREADY_EXISTS = 3,

        /**
         *  @brief          アクセスが拒否されました。
         *
         *  - errno: `EACCES` / `EPERM`
         *  - Win32: `ERROR_ACCESS_DENIED` / `ERROR_PRIVILEGE_NOT_HELD`
         *  - Winsock: `WSAEACCES`
         */
        CPLAT_CAUSE_ACCESS_DENIED = 4,

        /**
         *  @brief          他プロセスが排他的に開いているため共有できません (Windows 専用)。
         *
         *  - Win32: `ERROR_SHARING_VIOLATION`
         *
         *  @note           errno には対応する概念がありません。\n
         *                  POSIX の `open`/`fopen` は既定で共有可のため、Linux ではこの要因は発生しません。
         */
        CPLAT_CAUSE_SHARING_VIOLATION = 5,

        /**
         *  @brief          ディレクトリを期待した対象がディレクトリではありません。
         *
         *  - errno: `ENOTDIR`
         *  - Win32: `ERROR_DIRECTORY`
         */
        CPLAT_CAUSE_NOT_A_DIRECTORY = 6,

        /**
         *  @brief          ファイルを期待した対象がディレクトリでした。
         *
         *  - errno: `EISDIR`
         *
         *  @note           Win32 の変換表には対応する定数がありません。\n
         *                  Windows でこの状況は別のエラー コードで表現され、
         *                  @ref CPLAT_CAUSE_OTHER になる場合があります。
         */
        CPLAT_CAUSE_IS_A_DIRECTORY = 7,

        /**
         *  @brief          ディレクトリが空ではないため削除できません。
         *
         *  - errno: `ENOTEMPTY`
         *  - Win32: `ERROR_DIR_NOT_EMPTY`
         */
        CPLAT_CAUSE_DIRECTORY_NOT_EMPTY = 8,

        /**
         *  @brief          パスまたはファイル名が長すぎます。
         *
         *  - errno: `ENAMETOOLONG`
         *  - Win32: `ERROR_FILENAME_EXCED_RANGE` / `ERROR_BUFFER_OVERFLOW`
         */
        CPLAT_CAUSE_NAME_TOO_LONG = 9,

        /**
         *  @brief          引数が不正です。
         *
         *  - errno: `EINVAL`
         *  - Win32: `ERROR_INVALID_PARAMETER`
         *  - Winsock: `WSAEINVAL`
         *  - GAI: `EAI_BADFLAGS`
         */
        CPLAT_CAUSE_INVALID_ARGUMENT = 10,

        /**
         *  @brief          メモリを確保できません。
         *
         *  - errno: `ENOMEM`
         *  - Win32: `ERROR_NOT_ENOUGH_MEMORY` / `ERROR_OUTOFMEMORY`
         *  - Winsock: `WSAENOBUFS`
         *  - GAI: `EAI_MEMORY`
         */
        CPLAT_CAUSE_OUT_OF_MEMORY = 11,

        /**
         *  @brief          ディスク容量が不足しています。
         *
         *  - errno: `ENOSPC` / `EDQUOT`
         *  - Win32: `ERROR_DISK_FULL` / `ERROR_HANDLE_DISK_FULL`
         */
        CPLAT_CAUSE_DISK_FULL = 12,

        /**
         *  @brief          リソースがビジー状態です。
         *
         *  - errno: `EBUSY` / `EAGAIN` / `ETXTBSY`
         *  - Win32: `ERROR_BUSY` / `ERROR_SERVICE_ALREADY_RUNNING` /
         *    `ERROR_SERVICE_MARKED_FOR_DELETE` / `ERROR_DEPENDENT_SERVICES_RUNNING` /
         *    `ERROR_SERVICE_CANNOT_ACCEPT_CTRL`
         *  - GAI: `EAI_AGAIN`
         *
         *  @note           `EAGAIN` がこの要因になるのは、@ref cplat_error::domain が
         *                  @ref CPLAT_ERROR_DOMAIN_ERRNO (通常のファイル I/O、
         *                  `fork()`/`pthread_create()` の資源上限超過など) の場合だけです。\n
         *                  @ref CPLAT_ERROR_DOMAIN_SOCKET_ERRNO の `EAGAIN`/`EWOULDBLOCK` は
         *                  @ref CPLAT_CAUSE_WOULD_BLOCK になります。
         */
        CPLAT_CAUSE_BUSY = 13,

        /**
         *  @brief          タイムアウトが発生しました。
         *
         *  - errno: `ETIMEDOUT`
         *  - Win32: `WAIT_TIMEOUT` / `ERROR_TIMEOUT` / `ERROR_SERVICE_REQUEST_TIMEOUT`
         *  - Winsock: `WSAETIMEDOUT`
         */
        CPLAT_CAUSE_TIMEOUT = 14,

        /**
         *  @brief          実行中の操作が中断されました。
         *
         *  - errno: `EINTR`
         *  - Win32: `ERROR_OPERATION_ABORTED` (I/O キャンセル)
         *  - Winsock: `WSAEINTR`
         *
         *  @note           シグナルによる中断は Linux の cplat 実装が内部でリトライして
         *                  吸収するため、cplat の API がこの要因を返すのは、
         *                  Windows の I/O キャンセル (`ERROR_OPERATION_ABORTED`) の場合です。\n
         *                  @ref cplat_error_capture_errno() へ利用者が明示的に `EINTR` を
         *                  渡した場合も、この要因になります。
         */
        CPLAT_CAUSE_INTERRUPTED = 15,

        /**
         *  @brief          パイプの読み取り側が閉じられています。
         *
         *  - errno: `EPIPE`
         *  - Win32: `ERROR_BROKEN_PIPE`
         */
        CPLAT_CAUSE_BROKEN_PIPE = 16,

        /**
         *  @brief          開いているファイル記述子/ハンドルが上限に達しました。
         *
         *  - errno: `EMFILE` / `ENFILE`
         *  - Win32: `ERROR_TOO_MANY_OPEN_FILES`
         *  - Winsock: `WSAEMFILE`
         */
        CPLAT_CAUSE_TOO_MANY_OPEN_FILES = 17,

        /**
         *  @brief          対象が読み取り専用です。
         *
         *  - errno: `EROFS`
         *  - Win32: `ERROR_WRITE_PROTECT`
         */
        CPLAT_CAUSE_READ_ONLY = 18,

        /**
         *  @brief          出力バッファーが不足しています。
         *
         *  - errno: `ERANGE`
         *  - Win32: `ERROR_INSUFFICIENT_BUFFER`
         *
         *  @note           ソケットの `EMSGSIZE`/`WSAEMSGSIZE` は、この要因ではなく
         *                  @ref CPLAT_CAUSE_MESSAGE_SIZE になります。
         */
        CPLAT_CAUSE_BUFFER_TOO_SMALL = 19,

        /**
         *  @brief          現在のプラットフォームまたは状態では操作がサポートされていません。
         *
         *  - errno: `ENOTSUP` / `EOPNOTSUPP` (Linux では `ENOTSUP` と同値の場合は重複を避けて省略) /
         *    `ENOSYS`
         *  - Win32: `ERROR_NOT_SUPPORTED` / `ERROR_CALL_NOT_IMPLEMENTED`
         *  - Winsock: `WSAEOPNOTSUPP` / `WSAEAFNOSUPPORT` / `WSAEPROTONOSUPPORT`
         *  - GAI: `EAI_FAMILY` / `EAI_SOCKTYPE` / `EAI_SERVICE`
         */
        CPLAT_CAUSE_UNSUPPORTED = 20,

        /**
         *  @brief          入出力エラーが発生しました。
         *
         *  - errno: `EIO`
         *  - Win32: `ERROR_IO_DEVICE`
         */
        CPLAT_CAUSE_IO_ERROR = 21,

        /**
         *  @brief          非ブロッキング操作を直ちに完了できず、再試行が必要です。
         *
         *  - errno (@ref CPLAT_ERROR_DOMAIN_SOCKET_ERRNO のみ): `EAGAIN` / `EWOULDBLOCK`
         *  - Winsock: `WSAEWOULDBLOCK`
         *
         *  @note           @ref CPLAT_ERROR_DOMAIN_ERRNO (通常のファイル I/O など) の
         *                  `EAGAIN` は、この要因ではなく @ref CPLAT_CAUSE_BUSY になります。
         */
        CPLAT_CAUSE_WOULD_BLOCK = 22,

        /**
         *  @brief          非同期操作が開始済みで、完了通知を待つ必要があります。
         *
         *  - errno: `EINPROGRESS`
         *  - Winsock: `WSAEINPROGRESS` / `WSAEALREADY`
         *
         *  @note           `cplat_socket_connect()` などは、この要因につながる
         *                  errno/Winsock 値を検出した場合、共通結果コードとして
         *                  `CPLAT_ERR_IN_PROGRESS` (result.h) を明示的に返します。
         */
        CPLAT_CAUSE_IN_PROGRESS = 23,

        /**
         *  @brief          接続が拒否されました。
         *
         *  - errno: `ECONNREFUSED`
         *  - Winsock: `WSAECONNREFUSED`
         */
        CPLAT_CAUSE_CONNECTION_REFUSED = 24,

        /**
         *  @brief          接続が相手側にリセットされました。
         *
         *  - errno: `ECONNRESET`
         *  - Winsock: `WSAECONNRESET`
         */
        CPLAT_CAUSE_CONNECTION_RESET = 25,

        /**
         *  @brief          接続が中断されました。
         *
         *  - errno: `ECONNABORTED`
         *  - Winsock: `WSAECONNABORTED`
         */
        CPLAT_CAUSE_CONNECTION_ABORTED = 26,

        /**
         *  @brief          ソケットが接続されていません。
         *
         *  - errno: `ENOTCONN`
         *  - Winsock: `WSAENOTCONN`
         */
        CPLAT_CAUSE_NOT_CONNECTED = 27,

        /**
         *  @brief          ソケットがすでに接続されています。
         *
         *  - errno: `EISCONN`
         *  - Winsock: `WSAEISCONN`
         */
        CPLAT_CAUSE_ALREADY_CONNECTED = 28,

        /**
         *  @brief          アドレスがすでに使用されています。
         *
         *  - errno: `EADDRINUSE`
         *  - Winsock: `WSAEADDRINUSE`
         */
        CPLAT_CAUSE_ADDRESS_IN_USE = 29,

        /**
         *  @brief          アドレスを割り当てられません。
         *
         *  - errno: `EADDRNOTAVAIL`
         *  - Winsock: `WSAEADDRNOTAVAIL`
         */
        CPLAT_CAUSE_ADDRESS_NOT_AVAILABLE = 30,

        /**
         *  @brief          ネットワークがダウンしています。
         *
         *  - errno: `ENETDOWN`
         *  - Winsock: `WSAENETDOWN`
         */
        CPLAT_CAUSE_NETWORK_DOWN = 31,

        /**
         *  @brief          ネットワークに到達できません。
         *
         *  - errno: `ENETUNREACH`
         *  - Winsock: `WSAENETUNREACH`
         */
        CPLAT_CAUSE_NETWORK_UNREACHABLE = 32,

        /**
         *  @brief          ホストに到達できません。
         *
         *  - errno: `EHOSTUNREACH`
         *  - Winsock: `WSAEHOSTUNREACH`
         */
        CPLAT_CAUSE_HOST_UNREACHABLE = 33,

        /**
         *  @brief          メッセージが送受信バッファーの上限を超えています。
         *
         *  - errno: `EMSGSIZE`
         *  - Winsock: `WSAEMSGSIZE`
         */
        CPLAT_CAUSE_MESSAGE_SIZE = 34,

        /**
         *  @brief          ソケットがすでにシャットダウンされています。
         *
         *  - errno: `ESHUTDOWN`
         *  - Winsock: `WSAESHUTDOWN`
         */
        CPLAT_CAUSE_SHUTDOWN = 35,

        /**
         *  @brief          Winsock が初期化されていません (Windows 専用)。
         *
         *  - Winsock: `WSANOTINITIALISED`
         *
         *  @note           errno/Win32 には対応する概念がありません。\n
         *                  `WSAStartup()` を呼び出さずに Winsock API を使用した場合に発生します。
         */
        CPLAT_CAUSE_NOT_INITIALIZED = 36
    } cplat_error_cause;

    /**
     *  @brief          OS 由来の詳細エラーをドメイン付きで保持します。
     */
    typedef struct cplat_error
    {
        cplat_error_domain domain; /**< code の由来。 */
        int result;                /**< 対応する共通結果コード (CPLAT_OK または CPLAT_ERR_*)。 */
        unsigned long code;        /**< ドメイン固有の生のエラー値。 */
    } cplat_error;

    /**
     *  @brief          詳細エラーを空の値へ初期化します。
     *  @param[out]     error 初期化する値。NULL 可。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void CPLAT_API cplat_error_clear(cplat_error *error);

    /**
     *  @brief          errno の値を詳細エラーへ取り込みます。
     *  @param[out]     error       格納先。NULL 可。
     *  @param[in]      errno_value errno の値。0 の場合は空の値を格納します。
     *
     *  自前で呼び出した OS API の結果を、cplat と同じ枠組みで扱う場合に使用します。
     *
     *  @attention      Windows の `GetLastError()` の値を渡してはなりません。
     *                  Win32 の値には cplat_error_capture_windows_error() を使用します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void CPLAT_API cplat_error_capture_errno(cplat_error *error, int errno_value);

    /**
     *  @brief          現在の errno を詳細エラーへ取り込みます。
     *  @param[out]     error 格納先。NULL 可。
     *
     *  OS API の失敗を検出した直後に使用します。本関数は errno を読み取る前に、
     *  errno を変更する可能性がある処理を行いません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void CPLAT_API cplat_error_capture_current_errno(cplat_error *error);

#if defined(PLATFORM_WINDOWS)
    /**
     *  @brief          Win32 エラー コードを詳細エラーへ取り込みます。
     *  @param[out]     error      格納先。NULL 可。
     *  @param[in]      error_code GetLastError() が返した値。ERROR_SUCCESS の場合は空の値を格納します。
     *
     *  @attention      errno の値を渡してはなりません。
     *                  errno には cplat_error_capture_errno() を使用します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void CPLAT_API cplat_error_capture_windows_error(cplat_error *error, unsigned long error_code);

    /**
     *  @brief          現在の Win32 エラー コードを詳細エラーへ取り込みます。
     *  @param[out]     error 格納先。NULL 可。
     *
     *  Win32 API の失敗を検出した直後に使用します。本関数は GetLastError() を呼び出す前に、
     *  Win32 エラー コードを変更する可能性がある処理を行いません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void CPLAT_API cplat_error_capture_current_windows_error(cplat_error *error);
#endif

    /**
     *  @brief          現在のスレッドで直前に記録された詳細エラーを取得します。
     *  @param[out]     error_out 格納先。NULL 可。
     *
     *  返された値は呼び出し側の記憶域へコピーされ、後続 API の呼び出し後も保持できます。\n
     *  記録の契約と保証しない範囲は @ref error_last_contract を参照してください。\n
     *  直前値が記録されていないスレッドでは、空の値 (@ref CPLAT_ERROR_DOMAIN_NONE) を格納します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void CPLAT_API cplat_error_get_last(cplat_error *error_out);

    /**
     *  @brief          現在のスレッドの直前値を、保存済みの詳細エラーで更新します。
     *  @param[in]      error 設定する値。NULL の場合は直前値をクリアします。
     *
     *  OS API の失敗後に保存した詳細を、後処理で直前値が変化した後に復元する場合に使用します。
     *  指定した値は現在のスレッドの記憶域へコピーされ、呼び出し元の値は変更しません。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void CPLAT_API cplat_error_set_last(const cplat_error *error);

    /**
     *  @brief          現在のスレッドに記録された詳細エラーをクリアします。
     *
     *  対応 API は成功時に直前値をクリアするため、通常は呼び出す必要がありません。\n
     *  対応 API 以外の処理を挟む前に、古い値が残らないようにする場合に使用します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void CPLAT_API cplat_error_clear_last(void);

    /**
     *  @brief          詳細エラーが設定されているかを返します。
     *  @param[in]      error 確認する値。NULL 可。
     *  @return         設定されている場合は 1、それ以外は 0 を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_error_is_set(const cplat_error *error);

    /**
     *  @brief          詳細エラーのドメインを返します。
     *  @param[in]      error 確認する値。NULL 可。
     *  @return         ドメインを返します。NULL または不正な値の場合は CPLAT_ERROR_DOMAIN_NONE を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT cplat_error_domain CPLAT_API cplat_error_get_domain(const cplat_error *error);

    /**
     *  @brief          errno の値を返します。
     *  @param[in]      error 確認する値。NULL 可。
     *  @return         errno ドメインの場合は保持値、それ以外は 0 を返します。
     *
     *  Win32 ドメインの値を errno として取り出すことはありません。\n
     *  ドメインが一致しない場合に 0 を返すことで、体系の取り違えを防ぎます。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_error_get_errno(const cplat_error *error);

#if defined(PLATFORM_WINDOWS)
    /**
     *  @brief          Win32 エラー コードを返します。
     *  @param[in]      error 確認する値。NULL 可。
     *  @return         Win32 ドメインの場合は保持値、それ以外は ERROR_SUCCESS を返します。
     *
     *  errno ドメインの値を Win32 エラー コードとして取り出すことはありません。\n
     *  ドメインが一致しない場合に ERROR_SUCCESS を返すことで、体系の取り違えを防ぎます。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT unsigned long CPLAT_API cplat_error_get_windows_error(const cplat_error *error);
#endif

    /**
     *  @brief          詳細エラーに対応する共通結果コードを返します。
     *  @param[in]      error 確認する値。NULL 可。
     *  @return         対応する共通結果コードを返します。空の値の場合は @ref CPLAT_OK 、
     *                  NULL または不正なドメインの場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_error_to_result(const cplat_error *error);

    /**
     *  @brief          詳細エラーに対応するプラットフォーム共通の要因を返します。
     *  @param[in]      error 確認する値。NULL 可。
     *  @return         対応する要因を返します。未設定または NULL の場合は CPLAT_CAUSE_NONE、
     *                  対応がない場合は CPLAT_CAUSE_OTHER を返します。
     *
     *  1 つのエラー値に対応する要因は高々 1 つです。\n
     *  分岐が複数に及ぶ場合は、本関数の戻り値に対する `switch` で記述できます。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT cplat_error_cause CPLAT_API cplat_error_get_cause(const cplat_error *error);

    /**
     *  @brief          詳細エラーが指定した要因に一致するかを返します。
     *  @param[in]      error 確認する値。NULL 可。
     *  @param[in]      cause 判定する要因。
     *  @return         一致する場合は 1、それ以外は 0 を返します。
     *
     *  単発の判定に使用します。cplat_error_get_cause() の結果との比較と同じ意味です。
     *
     *  @attention      @p error が NULL の場合は、@p cause の値にかかわらず常に 0 を返します。
     *                  cplat_error_get_cause() は NULL に対して @ref CPLAT_CAUSE_NONE を
     *                  返すため、`cplat_error_is(NULL, CPLAT_CAUSE_NONE)` は
     *                  両者を組み合わせた判定とは結果が異なります。
     *                  範囲外の @p cause を指定した場合も 0 を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_error_is(const cplat_error *error, cplat_error_cause cause);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_BASE_ERROR_H */
