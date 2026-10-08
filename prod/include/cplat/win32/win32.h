/**
 *******************************************************************************
 *  @file           win32.h
 *  @brief          Win32 API を UTF-8 で呼び出す U サフィックスのラッパーを提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/06/09
 *  @version        1.0.0
 *
 *  文字列引数を UTF-8 (`const char *`) で受け取り、内部で UTF-16LE に変換して
 *  対応する W 版 Win32 API を呼び出すラッパー関数を提供します。\n
 *  A 版 API の代わりに U 版を使うことで、日本語環境 (cp932) など非 ASCII 文字
 *  セットに起因する文字化けを防ぎます。\n
 *  文字列を扱わない API (CloseHandle / ReadFile / SetEvent 等) はラップしません。\n
 *  Linux では本ヘッダーは何も宣言しません。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#ifndef CPLAT_WIN32_WIN32_H
#define CPLAT_WIN32_WIN32_H

/**
 *  @ingroup        CPLAT_WIN32
 *  @{
 */

#include <cplat/cplat_export.h>

#if defined(PLATFORM_WINDOWS)

    #include <cplat/base/windows_sdk.h>

/* SCM 関連型 (SC_HANDLE / SERVICE_TABLE_ENTRYW 等) は winsvc.h が提供する。    */
/* windows_sdk.h が WIN32_LEAN_AND_MEAN 付きで windows.h を取り込むが、         */
/* winsvc.h は WIN32_LEAN_AND_MEAN の対象外のため windows.h 経由で取り込まれる。 */

/**
 *  @brief          StartServiceCtrlDispatcherU 用の UTF-8 サービス エントリです。
 *
 *  終端要素は service_name を NULL にします。\n
 *                  service_proc は W 版シグネチャ (LPSERVICE_MAIN_FUNCTIONW) を使用します。
 */
typedef struct cplat_service_entry_u
{
    const char *service_name;              /**< サービス名 (UTF-8)。終端要素は NULL。 */
    LPSERVICE_MAIN_FUNCTIONW service_proc; /**< サービス メイン関数。 */
} cplat_service_entry_u;

/* ------------------------------------------------------------------ */
/*  ファイル / パイプ / ライブラリ                                       */
/* ------------------------------------------------------------------ */

/**
 *  @brief          ファイルまたは I/O デバイスを作成または開きます (UTF-8 パス版)。
 *
 *  utf8_path を UTF-16 に変換して CreateFileW を呼び出します。\n
 *                  変換失敗時は SetLastError(ERROR_INVALID_PARAMETER) を設定して
 *                  INVALID_HANDLE_VALUE を返します。\n
 *                  それ以外の挙動は CreateFileW と同一です。
 *  @param[in]      utf8_path               ファイルまたはデバイスの名前 (UTF-8)。
 *  @param[in]      desired_access          アクセスの要求 (GENERIC_READ / GENERIC_WRITE 等)。
 *  @param[in]      share_mode              共有モード。
 *  @param[in]      security_attributes     セキュリティ属性へのポインター。NULL 可。
 *  @param[in]      creation_disposition    ファイルが存在する場合と存在しない場合の動作。
 *  @param[in]      flags_and_attributes    ファイル属性とフラグ。
 *  @param[in]      template_file           テンプレート ファイルのハンドル。NULL 可。
 *  @return         成功時は開いたファイルのハンドル。失敗時は INVALID_HANDLE_VALUE。
 *  @see            CreateFileW
 *                  https://learn.microsoft.com/windows/win32/api/fileapi/nf-fileapi-createfilew
 *
 *  @par            スレッド セーフ
 *  本関数はスレッド セーフです。
 */
CPLAT_EXPORT HANDLE CPLAT_API CreateFileU(const char *utf8_path, DWORD desired_access, DWORD share_mode,
                                          LPSECURITY_ATTRIBUTES security_attributes, DWORD creation_disposition,
                                          DWORD flags_and_attributes, HANDLE template_file);

/**
 *  @brief          名前付きパイプのインスタンスを作成します (UTF-8 名前版)。
 *
 *  utf8_name を UTF-16 に変換して CreateNamedPipeW を呼び出します。\n
 *                  変換失敗時は SetLastError(ERROR_INVALID_PARAMETER) を設定して
 *                  INVALID_HANDLE_VALUE を返します。\n
 *                  それ以外の挙動は CreateNamedPipeW と同一です。
 *  @param[in]      utf8_name               パイプ名 (UTF-8) です。"\\\\.\\pipe\\{name}" 形式です。
 *  @param[in]      open_mode               パイプのアクセス モード。
 *  @param[in]      pipe_mode               パイプの種類、読み取りモード、待機モード。
 *  @param[in]      max_instances           最大インスタンス数。
 *  @param[in]      out_buffer_size         出力バッファーのサイズ (バイト)。
 *  @param[in]      in_buffer_size          入力バッファーのサイズ (バイト)。
 *  @param[in]      default_timeout         クライアントの待機タイムアウト (ミリ秒)。
 *  @param[in]      security_attributes     セキュリティ属性へのポインター。NULL 可。
 *  @return         成功時はパイプのサーバー エンド ハンドル。失敗時は INVALID_HANDLE_VALUE。
 *  @see            CreateNamedPipeW
 *                  https://learn.microsoft.com/windows/win32/api/namedpipeapi/nf-namedpipeapi-createnamedpipew
 *
 *  @par            スレッド セーフ
 *  本関数はスレッド セーフです。
 */
CPLAT_EXPORT HANDLE CPLAT_API CreateNamedPipeU(const char *utf8_name, DWORD open_mode, DWORD pipe_mode,
                                               DWORD max_instances, DWORD out_buffer_size, DWORD in_buffer_size,
                                               DWORD default_timeout, LPSECURITY_ATTRIBUTES security_attributes);

/**
 *  @brief          指定したモジュールのファイルの完全修飾パスを取得します (UTF-8 出力版)。
 *
 *  GetModuleFileNameW でワイド パスを取得し、UTF-8 に変換して utf8_buf に
 *                  書き込みます。\n
 *                  パス区切り文字の正規化 (\\\\→/) は行いません。\n
 *                  utf8_buf のサイズが不足する場合は切り詰め、SetLastError(ERROR_INSUFFICIENT_BUFFER)
 *                  を設定して実際に書き込んだバイト数 (NUL 終端を除く) を返します。\n
 *                  変換失敗時は 0 を返します。
 *  @param[in]      module      モジュールのハンドル。NULL のとき呼び出し元プロセスの実行ファイル。
 *  @param[out]     utf8_buf    パスの書き込み先バッファー (UTF-8)。
 *  @param[in]      size        utf8_buf のサイズ (バイト)。
 *  @return         書き込んだバイト数 (NUL 終端を除く)。失敗時は 0。
 *  @see            GetModuleFileNameW
 *                  https://learn.microsoft.com/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulefilenamew
 *
 *  @par            スレッド セーフ
 *  本関数はスレッド セーフです。
 */
CPLAT_EXPORT DWORD CPLAT_API GetModuleFileNameU(HMODULE module, char *utf8_buf, DWORD size);

/**
 *  @brief          コンソールへ文字列を書き込みます (UTF-8 入力版)。
 *
 *  utf8_text を UTF-16 に変換して WriteConsoleW を呼び出します。\n
 *                  WriteConsoleA はコンソールのコード ページ (日本語環境では cp932) で
 *                  解釈するため、UTF-8 文字列を渡すと文字化けします。\n
 *                  変換失敗時は SetLastError(ERROR_INVALID_PARAMETER) を設定して FALSE を返します。\n
 *                  ハンドルがコンソールでない場合 (リダイレクト時) は WriteConsoleW と同様に失敗します。
 *  @param[in]      console         コンソール スクリーン バッファーのハンドル。
 *  @param[in]      utf8_text       書き込む文字列 (UTF-8)。NUL 終端されている必要があります。
 *  @param[in]      utf8_length     utf8_text のバイト数 (NUL 終端を除く)。
 *  @param[out]     written_length  書き込めた utf8_text のバイト数の格納先。NULL 可。\n
 *                                  全体を書き込めた場合は utf8_length と同じ値を格納します。
 *  @param[in]      reserved        予約。NULL を指定してください。
 *  @return         成功時は非 0。失敗時は 0。
 *
 *  @attention      WriteConsoleW は書き込めた UTF-16 単位数を返すため、
 *                  部分書き込みが発生した場合の @p written_length は
 *                  書き込めた UTF-16 単位数に対応する UTF-8 バイト数ではなく 0 になります。
 *                  部分書き込みを検出する用途には使用しないでください。
 *  @see            WriteConsoleW
 *                  https://learn.microsoft.com/windows/console/writeconsole
 *
 *  @par            スレッド セーフ
 *  本関数はスレッド セーフです。
 */
CPLAT_EXPORT BOOL CPLAT_API WriteConsoleU(HANDLE console, const char *utf8_text, DWORD utf8_length,
                                          DWORD *written_length, void *reserved);

/**
 *  @brief          パスを含むボリュームのマウント ポイントを取得します (UTF-8 版)。
 *
 *  utf8_path を UTF-16 に変換して GetVolumePathNameW を呼び出し、結果を UTF-8 に
 *                  変換して utf8_volume_root へ書き込みます。\n
 *                  A 版はコンソールのコード ページで解釈するため、非 ASCII の
 *                  マウント パスを扱えません。\n
 *                  変換失敗またはバッファー不足時は SetLastError を設定して FALSE を返します。
 *  @param[in]      utf8_path           対象のパス (UTF-8)。
 *  @param[out]     utf8_volume_root    マウント ポイントの書き込み先 (UTF-8)。
 *  @param[in]      size                utf8_volume_root のサイズ (バイト)。
 *  @return         成功時は非 0。失敗時は 0。
 *  @see            GetVolumePathNameW
 *                  https://learn.microsoft.com/windows/win32/api/fileapi/nf-fileapi-getvolumepathnamew
 *
 *  @par            スレッド セーフ
 *  本関数はスレッド セーフです。
 */
CPLAT_EXPORT BOOL CPLAT_API GetVolumePathNameU(const char *utf8_path, char *utf8_volume_root, DWORD size);

/**
 *  @brief          ボリュームの情報を取得します (UTF-8 版)。
 *
 *  utf8_root_path を UTF-16 に変換して GetVolumeInformationW を呼び出し、
 *                  文字列の出力を UTF-8 に変換して書き込みます。\n
 *                  A 版はコンソールのコード ページで解釈するため、非 ASCII の
 *                  ボリューム名やマウント パスを扱えません。\n
 *                  変換失敗またはバッファー不足時は SetLastError を設定して FALSE を返します。
 *  @param[in]      utf8_root_path          ボリュームのルート パス (UTF-8)。NULL のとき現在のディレクトリのボリューム。
 *  @param[out]     utf8_volume_name        ボリューム名の書き込み先 (UTF-8)。NULL 可。
 *  @param[in]      volume_name_size        utf8_volume_name のサイズ (バイト)。
 *  @param[out]     serial_number           シリアル番号の格納先。NULL 可。
 *  @param[out]     max_component_length    ファイル名の最大長の格納先。NULL 可。
 *  @param[out]     file_system_flags       ファイル システムのフラグの格納先。NULL 可。
 *  @param[out]     utf8_file_system_name   ファイル システム名の書き込み先 (UTF-8)。NULL 可。
 *  @param[in]      file_system_name_size   utf8_file_system_name のサイズ (バイト)。
 *  @return         成功時は非 0。失敗時は 0。
 *  @see            GetVolumeInformationW
 *                  https://learn.microsoft.com/windows/win32/api/fileapi/nf-fileapi-getvolumeinformationw
 *
 *  @par            スレッド セーフ
 *  本関数はスレッド セーフです。
 */
CPLAT_EXPORT BOOL CPLAT_API GetVolumeInformationU(const char *utf8_root_path, char *utf8_volume_name,
                                                  DWORD volume_name_size, DWORD *serial_number,
                                                  DWORD *max_component_length, DWORD *file_system_flags,
                                                  char *utf8_file_system_name, DWORD file_system_name_size);

/**
 *  @brief          指定したモジュールをプロセスのアドレス空間にロードします (UTF-8 名前版)。
 *
 *  utf8_file_name を UTF-16 に変換して LoadLibraryW を呼び出します。\n
 *                  変換失敗時は SetLastError(ERROR_INVALID_PARAMETER) を設定して NULL を返します。\n
 *                  それ以外の挙動は LoadLibraryW と同一です。
 *  @param[in]      utf8_file_name  モジュール ファイルの名前 (UTF-8)。
 *  @return         成功時はモジュールのハンドル。失敗時は NULL。
 *  @see            LoadLibraryW
 *                  https://learn.microsoft.com/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryw
 *
 *  @par            スレッド セーフ
 *  本関数はスレッド セーフです。
 */
CPLAT_EXPORT HMODULE CPLAT_API LoadLibraryU(const char *utf8_file_name);

/* ------------------------------------------------------------------ */
/*  プロセス                                                             */
/* ------------------------------------------------------------------ */

/**
 *  @brief          新しいプロセスとそのプライマリ スレッドを作成します (UTF-8 文字列版)。
 *
 *  utf8_application_name / utf8_command_line / utf8_current_directory を
 *                  UTF-16 に変換して CreateProcessW を呼び出します。NULL 引数は変換せず
 *                  そのまま NULL として渡します。\n
 *                  utf8_command_line は CreateProcessW が内部で変更するため、
 *                  確保した可変バッファーを渡します。\n
 *                  environment が NULL 以外の場合は、CREATE_UNICODE_ENVIRONMENT フラグを
 *                  設定した UTF-16 環境ブロックを渡してください (本関数では変換しません)。\n
 *                  変換失敗時は SetLastError(ERROR_INVALID_PARAMETER) を設定して FALSE を返します。\n
 *                  それ以外の挙動は CreateProcessW と同一です。
 *  @param[in]      utf8_application_name   実行するモジュール名 (UTF-8)。NULL 可。
 *  @param[in]      utf8_command_line        コマンド ライン (UTF-8)。NULL 可。
 *  @param[in]      process_attributes      プロセスのセキュリティ属性。NULL 可。
 *  @param[in]      thread_attributes       スレッドのセキュリティ属性。NULL 可。
 *  @param[in]      inherit_handles         ハンドルの継承を許可するか。
 *  @param[in]      creation_flags          プロセスの作成フラグ。
 *  @param[in]      environment             環境ブロックへのポインター。NULL 可。
 *  @param[in]      utf8_current_directory  カレント ディレクトリ (UTF-8)。NULL 可。
 *  @param[in]      startup_info            スタートアップ情報 (W 版構造体)。
 *  @param[out]     process_information     プロセスとスレッドのハンドルと ID。
 *  @return         成功時は TRUE。失敗時は FALSE。
 *  @see            CreateProcessW
 *                  https://learn.microsoft.com/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw
 *
 *  @par            スレッド セーフ
 *  本関数はスレッド セーフです。
 */
CPLAT_EXPORT BOOL CPLAT_API CreateProcessU(const char *utf8_application_name, const char *utf8_command_line,
                                           LPSECURITY_ATTRIBUTES process_attributes,
                                           LPSECURITY_ATTRIBUTES thread_attributes, BOOL inherit_handles,
                                           DWORD creation_flags, LPVOID environment, const char *utf8_current_directory,
                                           LPSTARTUPINFOW startup_info, LPPROCESS_INFORMATION process_information);

/* ------------------------------------------------------------------ */
/*  サービス コントロール マネージャー (SCM)                              */
/* ------------------------------------------------------------------ */

/**
 *  @brief          サービス コントロール マネージャーへの接続を確立します (UTF-8 文字列版)。
 *
 *  utf8_machine_name / utf8_database_name を UTF-16 に変換して
 *                  OpenSCManagerW を呼び出します。NULL 引数は変換せずそのまま渡します。\n
 *                  変換失敗時は SetLastError(ERROR_INVALID_PARAMETER) を設定して NULL を返します。\n
 *                  それ以外の挙動は OpenSCManagerW と同一です。
 *  @param[in]      utf8_machine_name   コンピューター名 (UTF-8)。NULL でローカル コンピューター。
 *  @param[in]      utf8_database_name  SCM データベース名 (UTF-8)。NULL で既定のデータベース。
 *  @param[in]      desired_access      アクセス権。
 *  @return         成功時は SCM へのハンドル。失敗時は NULL。
 *  @see            OpenSCManagerW
 *                  https://learn.microsoft.com/windows/win32/api/winsvc/nf-winsvc-openscmanagerw
 *
 *  @par            スレッド セーフ
 *  本関数はスレッド セーフです。
 */
CPLAT_EXPORT SC_HANDLE CPLAT_API OpenSCManagerU(const char *utf8_machine_name, const char *utf8_database_name,
                                                DWORD desired_access);

/**
 *  @brief          サービス オブジェクトを作成して SCM データベースに追加します (UTF-8 文字列版)。
 *
 *  文字列引数を UTF-16 に変換して CreateServiceW を呼び出します。
 *                  NULL の引数は変換せずそのまま渡します。\n
 *                  utf8_dependencies は NULL または単一の依存サービス名のみ対応します。
 *                  二重 NULL 終端のマルチ文字列には対応していません。\n
 *                  変換失敗時は SetLastError(ERROR_INVALID_PARAMETER) を設定して NULL を返します。\n
 *                  それ以外の挙動は CreateServiceW と同一です。
 *  @param[in]      scm                     SCM ハンドル (OpenSCManagerU の戻り値)。
 *  @param[in]      utf8_service_name       サービス名 (UTF-8)。
 *  @param[in]      utf8_display_name       表示名 (UTF-8)。NULL 可。
 *  @param[in]      desired_access          アクセス権。
 *  @param[in]      service_type            サービスの種類。
 *  @param[in]      start_type              起動の種類。
 *  @param[in]      error_control           エラー コントロール。
 *  @param[in]      utf8_binary_path_name   実行ファイルのパス (UTF-8)。NULL 可。
 *  @param[in]      utf8_load_order_group   ロード オーダー グループ (UTF-8)。NULL 可。
 *  @param[out]     tag_id                  タグ ID の受け取りポインター。NULL 可。
 *  @param[in]      utf8_dependencies       依存サービス名 (UTF-8)。NULL または単一名。
 *  @param[in]      utf8_service_start_name 起動アカウント名 (UTF-8)。NULL 可。
 *  @param[in]      utf8_password           パスワード (UTF-8)。NULL 可。
 *  @return         成功時はサービス オブジェクトのハンドル。失敗時は NULL。
 *  @see            CreateServiceW
 *                  https://learn.microsoft.com/windows/win32/api/winsvc/nf-winsvc-createservicew
 *
 *  @par            スレッド セーフ
 *  本関数はスレッド セーフです。
 */
CPLAT_EXPORT SC_HANDLE CPLAT_API CreateServiceU(SC_HANDLE scm, const char *utf8_service_name,
                                                const char *utf8_display_name, DWORD desired_access, DWORD service_type,
                                                DWORD start_type, DWORD error_control,
                                                const char *utf8_binary_path_name, const char *utf8_load_order_group,
                                                LPDWORD tag_id, const char *utf8_dependencies,
                                                const char *utf8_service_start_name, const char *utf8_password);

/**
 *  @brief          既存のサービスのハンドルを開きます (UTF-8 名前版)。
 *
 *  utf8_service_name を UTF-16 に変換して OpenServiceW を呼び出します。\n
 *                  変換失敗時は SetLastError(ERROR_INVALID_PARAMETER) を設定して NULL を返します。\n
 *                  それ以外の挙動は OpenServiceW と同一です。
 *  @param[in]      scm                 SCM ハンドル (OpenSCManagerU の戻り値)。
 *  @param[in]      utf8_service_name   サービス名 (UTF-8)。
 *  @param[in]      desired_access      アクセス権。
 *  @return         成功時はサービス オブジェクトのハンドル。失敗時は NULL。
 *  @see            OpenServiceW
 *                  https://learn.microsoft.com/windows/win32/api/winsvc/nf-winsvc-openservicew
 *
 *  @par            スレッド セーフ
 *  本関数はスレッド セーフです。
 */
CPLAT_EXPORT SC_HANDLE CPLAT_API OpenServiceU(SC_HANDLE scm, const char *utf8_service_name, DWORD desired_access);

/**
 *  @brief          サービスの説明文を変更します (UTF-8 文字列版)。
 *
 *  SERVICE_CONFIG_DESCRIPTION のみ対応します。\n
 *                  utf8_text を UTF-16 に変換して SERVICE_DESCRIPTIONW を組み立て、
 *                  ChangeServiceConfig2W を呼び出します。\n
 *                  info_level が SERVICE_CONFIG_DESCRIPTION 以外の場合は
 *                  SetLastError(ERROR_INVALID_PARAMETER) を設定して FALSE を返します。\n
 *                  変換失敗時も SetLastError(ERROR_INVALID_PARAMETER) を設定して FALSE を返します。
 *  @param[in]      service     サービス ハンドル (OpenServiceU / CreateServiceU の戻り値)。
 *  @param[in]      info_level  変更する構成情報の種類。SERVICE_CONFIG_DESCRIPTION のみ対応。
 *  @param[in]      utf8_text   説明文 (UTF-8)。NULL を渡すと説明文を削除します。
 *  @return         成功時は TRUE。失敗時は FALSE。
 *  @see            ChangeServiceConfig2W
 *                  https://learn.microsoft.com/windows/win32/api/winsvc/nf-winsvc-changeserviceconfig2w
 *
 *  @par            スレッド セーフ
 *  本関数はスレッド セーフです。
 */
CPLAT_EXPORT BOOL CPLAT_API ChangeServiceConfig2U(SC_HANDLE service, DWORD info_level, const char *utf8_text);

/**
 *  @brief          拡張サービス コントロール要求を処理する関数を登録します (UTF-8 名前版)。
 *
 *  utf8_service_name を UTF-16 に変換して RegisterServiceCtrlHandlerExW を
 *                  呼び出します。\n
 *                  変換失敗時は SetLastError(ERROR_INVALID_PARAMETER) を設定して NULL を返します。\n
 *                  それ以外の挙動は RegisterServiceCtrlHandlerExW と同一です。
 *  @param[in]      utf8_service_name   サービス名 (UTF-8)。
 *  @param[in]      handler_proc        拡張ハンドラー関数へのポインター。
 *  @param[in]      context             ハンドラーへ渡すコンテキスト。NULL 可。
 *  @return         成功時はサービス ステータス ハンドル。失敗時は NULL。
 *  @see            RegisterServiceCtrlHandlerExW
 *                  https://learn.microsoft.com/windows/win32/api/winsvc/nf-winsvc-registerservicectrlhandlerexw
 *
 *  @par            スレッド セーフ
 *  本関数はスレッド セーフです。
 */
CPLAT_EXPORT SERVICE_STATUS_HANDLE CPLAT_API RegisterServiceCtrlHandlerExU(const char *utf8_service_name,
                                                                           LPHANDLER_FUNCTION_EX handler_proc,
                                                                           LPVOID context);

/**
 *  @brief          サービス プロセスのメイン スレッドを SCM に接続します (UTF-8 テーブル版)。
 *
 *  cplat_service_entry_u テーブルの各 service_name を UTF-16 に変換して
 *                  SERVICE_TABLE_ENTRYW 配列を組み立て、StartServiceCtrlDispatcherW を
 *                  呼び出します。テーブルの終端要素は service_name を NULL にしてください。\n
 *                  変換失敗時は SetLastError(ERROR_OUTOFMEMORY) を設定して FALSE を返します。\n
 *                  それ以外の挙動は StartServiceCtrlDispatcherW と同一です。
 *  @param[in]      service_table   サービス エントリの配列。終端の service_name は NULL。
 *  @return         成功時は TRUE。失敗時は FALSE。
 *  @see            StartServiceCtrlDispatcherW
 *                  https://learn.microsoft.com/windows/win32/api/winsvc/nf-winsvc-startservicectrldispatcherw
 *
 *  @par            スレッド セーフ
 *  本関数はスレッド セーフです。
 */
CPLAT_EXPORT BOOL CPLAT_API StartServiceCtrlDispatcherU(const cplat_service_entry_u *service_table);

#endif /* PLATFORM_WINDOWS */

/** @} */

#endif /* CPLAT_WIN32_WIN32_H */
