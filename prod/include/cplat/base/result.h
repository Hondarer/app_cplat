/**
 *******************************************************************************
 *  @file           result.h
 *  @brief          cplat ライブラリ共通の結果コードを定義します。
 *  @author         Tetsuo Honda
 *  @date           2026/07/27
 *  @version        1.0.0
 *
 *  cplat の公開 API が戻り値として使用する共通の結果コードです。\n
 *  成功は @ref CPLAT_OK (0) のみとし、非 0 はすべて
 *  「要求した操作が完遂されなかった」ことを表します。エラーは負値です。\n
 *  エラーではない省略は正値の @ref CPLAT_SKIPPED で表します。
 *
 *  @ref CPLAT_ERR_UNKNOWN (-1) は、-2 以下の分類済みコードに該当しない
 *  その他のエラーを表します。OS 由来の詳細 (errno、GetLastError() の値) が
 *  必要な API は、結果コードとは別に cplat_error の出力引数または
 *  cplat_error_get_last() で伝達します。
 *
 *  判定は @c != @ref CPLAT_OK の名前比較を正とします。
 *  全エラーが負値のため @c < 0 判定も等価ですが、名前比較を推奨します。
 *
 *  なお、cplat_fopen など元 API の戻り値規約を保存する CRT ラッパーと
 *  Windows API の UTF-8 ラッパー (win32.h) は、本結果コードの適用対象外です。
 *  CRT の機能を使用する API でも、戻り値に共通結果コードを使用すると明記した
 *  関数には本結果コードを適用します。
 *
 *  本ヘッダーは、粗い分類 (@ref CPLAT_ERR_INVALID_ARGUMENT など) と
 *  細かい分類 (@ref CPLAT_ERR_UNKNOWN_OPTION など) の両方を含む単一の
 *  体系です。モジュール固有のコード体系を別に設けず、必要な分類は本ヘッダー
 *  へ追加します。
 *
 *  定義は課題別の帯 (省略、引数・状態・権限、リソース・バッファー、入力解析、制御)
 *  に分けて並べ、各帯に将来の追加用の余白を設けています。
 *
 *  @warning        帯は定義の整理と追加位置を示すためのものであり、範囲判定に
 *                  よる分類は本 API の契約に含めません。
 *                  @c rc @c <= @c -20 のような範囲比較で種別を判定せず、
 *                  個々のコード名との比較を使用してください。
 *
 *  @attention      各コードの値は ABI として凍結します。既存の値の変更は
 *                  禁止し、コードの追加は該当する帯の余白への追記のみと
 *                  します。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CPLAT_RESULT_H
#define CPLAT_RESULT_H

/**
 *  @defgroup       CPLAT_RESULT 共通結果コード
 *  @ingroup        CPLAT_BASE
 *
 *  cplat の公開 API が戻り値として使用する共通の結果コードです。\n
 *  成功は @ref CPLAT_OK (0) のみで、エラーは負値です。\n
 *  エラーではない省略は @ref CPLAT_SKIPPED (1) です。
 */

/**
 *  @ingroup        CPLAT_RESULT
 *  @{
 */
#define CPLAT_OK 0 /**< 処理に成功しました。 */

/* 省略 (エラーではない非完遂): +1 〜 */

/**
 *  @brief          要求は受け付けましたが、適用せずに省略しました。
 *
 *  失敗ではありません。先の設定では成立しない入力を破棄するときなど、
 *  呼び出し側が処理を続行してよい場合に返します。
 */
#define CPLAT_SKIPPED 1

/* 分類不能 */

/**
 *  @brief          -2 以下の分類済みコードに該当しない、その他のエラーです。
 *
 *  `cplat_internal_error_report_errno()`/`_windows_error()`/`_winsock_error()` (`base/error.c`) が
 *  内部で呼び出す `cplat_internal_result_from_errno()`/`_windows_error()`/`_winsock_error()`
 *  (`base/result.c`) は、以下で個別に文書化する OS エラー値のいずれにも
 *  一致しない場合、フォールバックとしてこの値を返します。
 */
#define CPLAT_ERR_UNKNOWN (-1)

/* 引数・状態・権限: -2 〜 -9 */

/**
 *  @brief          API 引数が不正です (NULL、負値など)。
 *
 *  - errno: `EINVAL`
 *  - Win32: `ERROR_INVALID_PARAMETER`
 *  - Winsock: `WSAEINVAL` / `WSAEFAULT`
 *
 *  @note           上記は OS API 呼び出しの失敗を `detail_out` 経由で本コードへ変換する
 *                  場合の対応です。cplat 自身の引数検査による
 *                  `CPLAT_ERR_INVALID_ARGUMENT` はこの変換を経由しません。
 */
#define CPLAT_ERR_INVALID_ARGUMENT (-2)

/**
 *  @brief          現在のプラットフォームまたは状態では操作がサポートされていません。
 *
 *  - Winsock: `WSAEOPNOTSUPP` / `WSAEAFNOSUPPORT` / `WSAEPROTONOSUPPORT`
 *
 *  @note           `cplat_internal_result_from_errno()`/`_windows_error()` (`base/result.c`) は
 *                  errno の `ENOTSUP`/`ENOSYS` や Win32 の `ERROR_NOT_SUPPORTED` を
 *                  この結果コードへ変換しません (`CPLAT_ERR_UNKNOWN` になります)。\n
 *                  要因レベルの分類 (`cplat_error_cause` の
 *                  @ref CPLAT_CAUSE_UNSUPPORTED) は 4 ドメインすべてに対応するため、
 *                  errno/Win32 由来の未サポートを判定したい場合は
 *                  `cplat_error_get_cause()` の戻り値を確認してください。
 */
#define CPLAT_ERR_UNSUPPORTED (-3)

/**
 *  @brief          権限が不足しています。
 *
 *  - errno: `EACCES` / `EPERM`
 *  - Win32: `ERROR_ACCESS_DENIED` / `ERROR_PRIVILEGE_NOT_HELD`
 *  - Winsock: `WSAEACCES`
 */
#define CPLAT_ERR_PERMISSION_DENIED (-4)

#define CPLAT_ERR_DUPLICATE_DEFINITION (-5) /**< 同名の定義が登録済みです。 */

/**
 *  @brief          対象が存在しません (ホスト名を解決できない場合など)。
 *
 *  - errno: `ENOENT`
 *  - Win32: `ERROR_FILE_NOT_FOUND` / `ERROR_PATH_NOT_FOUND`
 *  - Winsock: `WSAHOST_NOT_FOUND`
 */
#define CPLAT_ERR_NOT_FOUND (-6)

#define CPLAT_ERR_DUPLICATE_KEY (-7) /**< 同一キーが既に存在します。 */

/**
 *  @brief          期待する対象の識別値と一致しません。
 *
 *  形式は正しく読めたうえで、別の対象 (別のカタログなど) を指していることを表します。\n
 *  マジック値 (署名) の不一致は、別の形式と破損を区別できないため @ref CPLAT_ERR_CORRUPT_DESCRIPTOR とします。
 */
#define CPLAT_ERR_SIGNATURE_MISMATCH (-8)

/**
 *  @brief          対応する定義または形式の版と一致しません。
 *
 *  署名が一致し、同じ種類の対象であることを確かめたうえで、形式版が異なることを表します。\n
 *  長さ、範囲、内部整合性の不正は @ref CPLAT_ERR_CORRUPT_DESCRIPTOR とします。
 */
#define CPLAT_ERR_VERSION_MISMATCH (-9)

/* リソース・バッファー: -10 〜 -19 */

/**
 *  @brief          メモリを確保できません。
 *
 *  - errno: `ENOMEM`
 *  - Win32: `ERROR_NOT_ENOUGH_MEMORY` / `ERROR_OUTOFMEMORY`
 *  - Winsock: `WSAENOBUFS`
 */
#define CPLAT_ERR_OUT_OF_MEMORY (-10)

/**
 *  @brief          リソースがビジー状態です。
 *
 *  - errno: `EBUSY` / `EAGAIN`
 *  - Win32: `ERROR_BUSY`
 *  - Winsock: `WSAEWOULDBLOCK`
 *
 *  @note           ソケットの非ブロッキング操作を再試行させたい場合は、
 *                  この結果コードではなく `CPLAT_ERR_IN_PROGRESS` や、
 *                  要因レベルの `CPLAT_CAUSE_WOULD_BLOCK` を使用します。
 */
#define CPLAT_ERR_BUSY (-11)

/**
 *  @brief          タイムアウトが発生しました。
 *
 *  - errno: `ETIMEDOUT`
 *  - Win32: `WAIT_TIMEOUT` / `ERROR_TIMEOUT`
 *  - Winsock: `WSAETIMEDOUT`
 */
#define CPLAT_ERR_TIMEOUT (-12)

#define CPLAT_ERR_LIMIT_EXCEEDED (-13) /**< リソースの上限を超過しました。 */

/**
 *  @brief          出力バッファーが不足しています。
 *
 *  - errno: `ENAMETOOLONG` / `ERANGE`
 *  - Win32: `ERROR_INSUFFICIENT_BUFFER`
 *  - Winsock: `WSAEMSGSIZE`
 */
#define CPLAT_ERR_BUFFER_TOO_SMALL (-14)

#define CPLAT_ERR_CORRUPT_DESCRIPTOR (-15) /**< ディスクリプタが破損しています。 */
#define CPLAT_ERR_STORAGE_FULL       (-16) /**< 固定容量ストレージに空きがありません。 */

/* 入力解析: -20 〜 -39 */
#define CPLAT_ERR_UNKNOWN_OPTION       (-20) /**< 未登録のオプションが指定されました。 */
#define CPLAT_ERR_MISSING_VALUE        (-21) /**< 値を要する項目に値が指定されていません。 */
#define CPLAT_ERR_UNEXPECTED_VALUE     (-22) /**< 値を取らない項目に値が指定されました。 */
#define CPLAT_ERR_INVALID_INTEGER      (-23) /**< 整数値として解釈できません。 */
#define CPLAT_ERR_OUT_OF_RANGE         (-24) /**< 値が表現可能な範囲を超えています。 */
#define CPLAT_ERR_MISSING_REQUIRED     (-25) /**< 必須の項目が指定されていません。 */
#define CPLAT_ERR_DUPLICATE_OPTION     (-26) /**< 単数指定の項目が複数回指定されました。 */
#define CPLAT_ERR_TOO_MANY_ARGUMENTS   (-27) /**< 引数の個数が受入数を超えています。 */
#define CPLAT_ERR_TOO_MANY_OCCURRENCES (-28) /**< 同一項目の出現回数が容量を超えています。 */
#define CPLAT_ERR_INVALID_PATTERN      (-29) /**< 正規表現パターンの構文が不正です。 */
#define CPLAT_ERR_INVALID_ENCODING     (-30) /**< 文字列が UTF-8 として不正です。 */
#define CPLAT_ERR_MALFORMED_DEFINITION (-31) /**< 与えられた定義の内容が構文または整合の点で不正です。 */

/* 制御: -40 〜 */
#define CPLAT_ERR_EOF      (-40) /**< 入力が EOF に達しました。 */
#define CPLAT_ERR_CANCELED (-41) /**< ユーザー操作 (Ctrl+C など) により中断しました。 */

/**
 *  @brief          非同期処理が開始済みで、完了を待つ必要があります。
 *
 *  - errno: `EINPROGRESS`
 *  - Winsock: `WSAEWOULDBLOCK` / `WSAEINPROGRESS` / `WSAEALREADY`
 *
 *  @note           汎用の OS エラー変換表 (`cplat_internal_result_from_errno()` 等) には
 *                  含まれません。@ref cplat_socket_connect の非ブロッキング
 *                  connect が、上記の値を検出した場合に限り明示的にこの値を返します。
 *                  (`net/socket_linux.c`、`net/socket_windows.c`)
 */
#define CPLAT_ERR_IN_PROGRESS (-42)
/** @} */

#endif /* CPLAT_RESULT_H */
