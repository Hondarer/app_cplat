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

/*
 * SCM 関連型 (SC_HANDLE / SERVICE_TABLE_ENTRYW 等) は winsvc.h が提供する。
 * windows_sdk.h が WIN32_LEAN_AND_MEAN 付きで windows.h を取り込むが、
 * winsvc.h は WIN32_LEAN_AND_MEAN の対象外のため windows.h 経由で取り込まれる。
 */

/**
 *  @brief          StartServiceCtrlDispatcherU 用の UTF-8 サービス エントリです。
 *
 *  終端要素は lpServiceName と lpServiceProc を NULL にします。\n
 *                  lpServiceProc は W 版シグネチャ (LPSERVICE_MAIN_FUNCTIONW) を使用し、引数の文字列は UTF-16 のままです。
 */
typedef struct SERVICE_TABLE_ENTRYU
{
    const char *lpServiceName;              /**< サービス名 (UTF-8)。終端要素は NULL。 */
    LPSERVICE_MAIN_FUNCTIONW lpServiceProc; /**< サービス メイン関数。 */
} SERVICE_TABLE_ENTRYU;

/* ------------------------------------------------------------------ */
/*  ファイル / パイプ / ライブラリ                                       */
/* ------------------------------------------------------------------ */

/**
 *  @brief          ファイルまたは I/O デバイスを作成または開きます (UTF-8 パス版)。
 *
 *  lpFileName を UTF-16 に変換して CreateFileW を呼び出します。\n
 *                  変換失敗時は SetLastError(ERROR_INVALID_PARAMETER) を設定して
 *                  INVALID_HANDLE_VALUE を返します。\n
 *                  それ以外の挙動は CreateFileW と同一です。
 *  @param[in]      lpFileName                ファイルまたはデバイスの名前 (UTF-8)。
 *  @param[in]      dwDesiredAccess           アクセスの要求 (GENERIC_READ / GENERIC_WRITE 等)。
 *  @param[in]      dwShareMode               共有モード。
 *  @param[in]      lpSecurityAttributes      セキュリティ属性へのポインター。NULL 可。
 *  @param[in]      dwCreationDisposition     ファイルが存在する場合と存在しない場合の動作。
 *  @param[in]      dwFlagsAndAttributes      ファイル属性とフラグ。
 *  @param[in]      hTemplateFile             テンプレート ファイルのハンドル。NULL 可。
 *  @return         成功時は開いたファイルのハンドル。失敗時は INVALID_HANDLE_VALUE。
 *  @see            CreateFileW
 *                  https://learn.microsoft.com/windows/win32/api/fileapi/nf-fileapi-createfilew
 *
 *  @par            スレッド セーフ
 *  本関数はスレッド セーフです。
 */
CPLAT_EXPORT HANDLE CPLAT_API CreateFileU(const char *lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                                          LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
                                          DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);

/**
 *  @brief          名前付きパイプのインスタンスを作成します (UTF-8 名前版)。
 *
 *  lpName を UTF-16 に変換して CreateNamedPipeW を呼び出します。\n
 *                  変換失敗時は SetLastError(ERROR_INVALID_PARAMETER) を設定して
 *                  INVALID_HANDLE_VALUE を返します。\n
 *                  それ以外の挙動は CreateNamedPipeW と同一です。
 *  @param[in]      lpName                    パイプ名 (UTF-8) です。"\\\\.\\pipe\\{name}" 形式です。
 *  @param[in]      dwOpenMode                パイプのアクセス モード。
 *  @param[in]      dwPipeMode                パイプの種類、読み取りモード、待機モード。
 *  @param[in]      nMaxInstances             最大インスタンス数。
 *  @param[in]      nOutBufferSize            出力バッファーのサイズ [byte]。
 *  @param[in]      nInBufferSize             入力バッファーのサイズ [byte]。
 *  @param[in]      nDefaultTimeOut           クライアントの待機タイムアウト [ms]。
 *  @param[in]      lpSecurityAttributes      セキュリティ属性へのポインター。NULL 可。
 *  @return         成功時はパイプのサーバー エンド ハンドル。失敗時は INVALID_HANDLE_VALUE。
 *  @see            CreateNamedPipeW
 *                  https://learn.microsoft.com/windows/win32/api/namedpipeapi/nf-namedpipeapi-createnamedpipew
 *
 *  @par            スレッド セーフ
 *  本関数はスレッド セーフです。
 */
CPLAT_EXPORT HANDLE CPLAT_API CreateNamedPipeU(const char *lpName, DWORD dwOpenMode, DWORD dwPipeMode,
                                               DWORD nMaxInstances, DWORD nOutBufferSize, DWORD nInBufferSize,
                                               DWORD nDefaultTimeOut, LPSECURITY_ATTRIBUTES lpSecurityAttributes);

/**
 *  @brief          指定したモジュールのファイルの完全修飾パスを取得します (UTF-8 出力版)。
 *
 *  GetModuleFileNameW でワイド パスを取得し、UTF-8 に変換して lpFilename に
 *                  書き込みます。\n
 *                  パス区切り文字の正規化 (\\\\→/) は行いません。\n
 *                  lpFilename のサイズが不足する場合は切り詰め、SetLastError(ERROR_INSUFFICIENT_BUFFER)
 *                  を設定して実際に書き込んだバイト数 (NUL 終端を除く) を返します。\n
 *                  変換失敗時は 0 を返します。
 *  @param[in]      hModule                   モジュールのハンドル。NULL のとき呼び出し元プロセスの実行ファイル。
 *  @param[out]     lpFilename                パスの書き込み先バッファー (UTF-8)。
 *  @param[in]      nSize                     lpFilename のサイズ [byte]。W 版の UTF-16 単位数とは異なります。
 *  @return         書き込んだバイト数 (NUL 終端を除く)。失敗時は 0。
 *  @see            GetModuleFileNameW
 *                  https://learn.microsoft.com/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulefilenamew
 *
 *  @par            スレッド セーフ
 *  本関数はスレッド セーフです。
 */
CPLAT_EXPORT DWORD CPLAT_API GetModuleFileNameU(HMODULE hModule, char *lpFilename, DWORD nSize);

/**
 *  @brief          コンソールへ文字列を書き込みます (UTF-8 入力版)。
 *
 *  lpBuffer を UTF-16 に変換して WriteConsoleW を呼び出します。\n
 *                  WriteConsoleA はコンソールのコード ページ (日本語環境では cp932) で
 *                  解釈するため、UTF-8 文字列を渡すと文字化けします。\n
 *                  変換失敗時は SetLastError(ERROR_INVALID_PARAMETER) を設定して FALSE を返します。\n
 *                  ハンドルがコンソールでない場合 (リダイレクト時) は WriteConsoleW と同様に失敗します。
 *  @param[in]      hConsoleOutput            コンソール スクリーン バッファーのハンドル。
 *  @param[in]      lpBuffer                  書き込む文字列 (UTF-8)。指定したバイト数を読み取れる必要があります。
 *  @param[in]      nNumberOfCharsToWrite     lpBuffer のバイト数 (NUL 終端を除く)。W 版の UTF-16 単位数とは異なります。
 *  @param[out]     lpNumberOfCharsWritten    書き込めた lpBuffer のバイト数の格納先。NULL 可。\n
 *                                            全体を書き込めた場合は nNumberOfCharsToWrite と同じ値を格納します。
 *  @param[in]      lpReserved                予約。NULL を指定してください。
 *  @return         成功時は非 0。失敗時は 0。
 *
 *  @attention      WriteConsoleW は書き込めた UTF-16 単位数を返すため、
 *                  部分書き込みが発生した場合の @p lpNumberOfCharsWritten は
 *                  書き込めた UTF-16 単位数に対応する UTF-8 バイト数ではなく 0 になります。
 *                  部分書き込みを検出する用途には使用しないでください。
 *  @see            WriteConsoleW
 *                  https://learn.microsoft.com/windows/console/writeconsole
 *
 *  @par            スレッド セーフ
 *  本関数はスレッド セーフです。
 */
CPLAT_EXPORT BOOL CPLAT_API WriteConsoleU(HANDLE hConsoleOutput, const char *lpBuffer, DWORD nNumberOfCharsToWrite,
                                          LPDWORD lpNumberOfCharsWritten, LPVOID lpReserved);

/**
 *  @brief          パスを含むボリュームのマウント ポイントを取得します (UTF-8 版)。
 *
 *  lpszFileName を UTF-16 に変換して GetVolumePathNameW を呼び出し、結果を UTF-8 に
 *                  変換して lpszVolumePathName へ書き込みます。\n
 *                  A 版はコンソールのコード ページで解釈するため、非 ASCII の
 *                  マウント パスを扱えません。\n
 *                  変換失敗またはバッファー不足時は SetLastError を設定して FALSE を返します。
 *  @param[in]      lpszFileName              対象のパス (UTF-8)。
 *  @param[out]     lpszVolumePathName        マウント ポイントの書き込み先 (UTF-8)。
 *  @param[in]      cchBufferLength           lpszVolumePathName のサイズ [byte]。W 版の UTF-16 単位数とは異なります。
 *  @return         成功時は非 0。失敗時は 0。
 *  @see            GetVolumePathNameW
 *                  https://learn.microsoft.com/windows/win32/api/fileapi/nf-fileapi-getvolumepathnamew
 *
 *  @par            スレッド セーフ
 *  本関数はスレッド セーフです。
 */
CPLAT_EXPORT BOOL CPLAT_API GetVolumePathNameU(const char *lpszFileName, char *lpszVolumePathName,
                                               DWORD cchBufferLength);

/**
 *  @brief          ボリュームの情報を取得します (UTF-8 版)。
 *
 *  lpRootPathName を UTF-16 に変換して GetVolumeInformationW を呼び出し、
 *                  文字列の出力を UTF-8 に変換して書き込みます。\n
 *                  A 版はコンソールのコード ページで解釈するため、非 ASCII の
 *                  ボリューム名やマウント パスを扱えません。\n
 *                  変換失敗またはバッファー不足時は SetLastError を設定して FALSE を返します。
 *  @param[in]      lpRootPathName            ボリュームのルート パス (UTF-8)。NULL のとき現在のディレクトリのボリューム。
 *  @param[out]     lpVolumeNameBuffer        ボリューム名の書き込み先 (UTF-8)。NULL 可。
 *  @param[in]      nVolumeNameSize           lpVolumeNameBuffer のサイズ [byte]。W 版の UTF-16 単位数とは異なります。
 *  @param[out]     lpVolumeSerialNumber      シリアル番号の格納先。NULL 可。
 *  @param[out]     lpMaximumComponentLength  ファイル名の最大長の格納先。NULL 可。
 *  @param[out]     lpFileSystemFlags         ファイル システムのフラグの格納先。NULL 可。
 *  @param[out]     lpFileSystemNameBuffer    ファイル システム名の書き込み先 (UTF-8)。NULL 可。
 *  @param[in]      nFileSystemNameSize       lpFileSystemNameBuffer のサイズ [byte]。W 版の UTF-16 単位数とは異なります。
 *  @return         成功時は非 0。失敗時は 0。
 *  @see            GetVolumeInformationW
 *                  https://learn.microsoft.com/windows/win32/api/fileapi/nf-fileapi-getvolumeinformationw
 *
 *  @par            スレッド セーフ
 *  本関数はスレッド セーフです。
 */
CPLAT_EXPORT BOOL CPLAT_API GetVolumeInformationU(const char *lpRootPathName, char *lpVolumeNameBuffer,
                                                  DWORD nVolumeNameSize, LPDWORD lpVolumeSerialNumber,
                                                  LPDWORD lpMaximumComponentLength, LPDWORD lpFileSystemFlags,
                                                  char *lpFileSystemNameBuffer, DWORD nFileSystemNameSize);

/**
 *  @brief          指定したモジュールをプロセスのアドレス空間にロードします (UTF-8 名前版)。
 *
 *  lpLibFileName を UTF-16 に変換して LoadLibraryW を呼び出します。\n
 *                  変換失敗時は SetLastError(ERROR_INVALID_PARAMETER) を設定して NULL を返します。\n
 *                  それ以外の挙動は LoadLibraryW と同一です。
 *  @param[in]      lpLibFileName             モジュール ファイルの名前 (UTF-8)。
 *  @return         成功時はモジュールのハンドル。失敗時は NULL。
 *  @see            LoadLibraryW
 *                  https://learn.microsoft.com/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryw
 *
 *  @par            スレッド セーフ
 *  本関数はスレッド セーフです。
 */
CPLAT_EXPORT HMODULE CPLAT_API LoadLibraryU(const char *lpLibFileName);

/* ------------------------------------------------------------------ */
/*  プロセス                                                             */
/* ------------------------------------------------------------------ */

/**
 *  @brief          新しいプロセスとそのプライマリ スレッドを作成します (UTF-8 文字列版)。
 *
 *  lpApplicationName / lpCommandLine / lpCurrentDirectory を
 *                  UTF-16 に変換して CreateProcessW を呼び出します。NULL 引数は変換せず
 *                  そのまま NULL として渡します。\n
 *                  lpCommandLine は CreateProcessW が内部で変更するため、
 *                  確保した可変バッファーを渡します。U 版は const char * を受け取り、入力を変更しません。\n
 *                  lpEnvironment が NULL 以外の場合は、CREATE_UNICODE_ENVIRONMENT フラグを
 *                  設定した UTF-16 環境ブロックを渡してください (本関数では変換しません)。\n
 *                  変換失敗時は SetLastError(ERROR_INVALID_PARAMETER) を設定して FALSE を返します。\n
 *                  それ以外の挙動は CreateProcessW と同一です。
 *  @param[in]      lpApplicationName         実行するモジュール名 (UTF-8)。NULL 可。
 *  @param[in]      lpCommandLine             コマンド ライン (UTF-8)。NULL 可。
 *  @param[in]      lpProcessAttributes       プロセスのセキュリティ属性。NULL 可。
 *  @param[in]      lpThreadAttributes        スレッドのセキュリティ属性。NULL 可。
 *  @param[in]      bInheritHandles           ハンドルの継承を許可するか。
 *  @param[in]      dwCreationFlags           プロセスの作成フラグ。
 *  @param[in]      lpEnvironment             環境ブロックへのポインター。NULL 可。
 *  @param[in]      lpCurrentDirectory        カレント ディレクトリ (UTF-8)。NULL 可。
 *  @param[in]      lpStartupInfo             スタートアップ情報 (W 版構造体)。構造体内の文字列は UTF-16 のままです。
 *  @param[out]     lpProcessInformation      プロセスとスレッドのハンドルと ID。
 *  @return         成功時は TRUE。失敗時は FALSE。
 *  @see            CreateProcessW
 *                  https://learn.microsoft.com/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw
 *
 *  @par            スレッド セーフ
 *  本関数はスレッド セーフです。
 */
CPLAT_EXPORT BOOL CPLAT_API CreateProcessU(const char *lpApplicationName, const char *lpCommandLine,
                                           LPSECURITY_ATTRIBUTES lpProcessAttributes,
                                           LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles,
                                           DWORD dwCreationFlags, LPVOID lpEnvironment, const char *lpCurrentDirectory,
                                           LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation);

/* ------------------------------------------------------------------ */
/*  サービス コントロール マネージャー (SCM)                              */
/* ------------------------------------------------------------------ */

/**
 *  @brief          サービス コントロール マネージャーへの接続を確立します (UTF-8 文字列版)。
 *
 *  lpMachineName / lpDatabaseName を UTF-16 に変換して
 *                  OpenSCManagerW を呼び出します。NULL 引数は変換せずそのまま渡します。\n
 *                  変換失敗時は SetLastError(ERROR_INVALID_PARAMETER) を設定して NULL を返します。\n
 *                  それ以外の挙動は OpenSCManagerW と同一です。
 *  @param[in]      lpMachineName             コンピューター名 (UTF-8)。NULL でローカル コンピューター。
 *  @param[in]      lpDatabaseName            SCM データベース名 (UTF-8)。NULL で既定のデータベース。
 *  @param[in]      dwDesiredAccess           アクセス権。
 *  @return         成功時は SCM へのハンドル。失敗時は NULL。
 *  @see            OpenSCManagerW
 *                  https://learn.microsoft.com/windows/win32/api/winsvc/nf-winsvc-openscmanagerw
 *
 *  @par            スレッド セーフ
 *  本関数はスレッド セーフです。
 */
CPLAT_EXPORT SC_HANDLE CPLAT_API OpenSCManagerU(const char *lpMachineName, const char *lpDatabaseName,
                                                DWORD dwDesiredAccess);

/**
 *  @brief          サービス オブジェクトを作成して SCM データベースに追加します (UTF-8 文字列版)。
 *
 *  文字列引数を UTF-16 に変換して CreateServiceW を呼び出します。
 *                  NULL の引数は変換せずそのまま渡します。\n
 *                  lpDependencies は NULL または単一の依存サービス名のみ対応します。
 *                  二重 NULL 終端のマルチ文字列には対応していません。\n
 *                  変換失敗時は SetLastError(ERROR_INVALID_PARAMETER) を設定して NULL を返します。\n
 *                  それ以外の挙動は CreateServiceW と同一です。
 *  @param[in]      hSCManager                SCM ハンドル (OpenSCManagerU の戻り値)。
 *  @param[in]      lpServiceName             サービス名 (UTF-8)。
 *  @param[in]      lpDisplayName             表示名 (UTF-8)。NULL 可。
 *  @param[in]      dwDesiredAccess           アクセス権。
 *  @param[in]      dwServiceType             サービスの種類。
 *  @param[in]      dwStartType               起動の種類。
 *  @param[in]      dwErrorControl            エラー コントロール。
 *  @param[in]      lpBinaryPathName          実行ファイルのパス (UTF-8)。NULL 可。
 *  @param[in]      lpLoadOrderGroup          ロード オーダー グループ (UTF-8)。NULL 可。
 *  @param[out]     lpdwTagId                 タグ ID の受け取りポインター。NULL 可。
 *  @param[in]      lpDependencies            依存サービス名 (UTF-8)。NULL または単一名。
 *  @param[in]      lpServiceStartName        起動アカウント名 (UTF-8)。NULL 可。
 *  @param[in]      lpPassword                パスワード (UTF-8)。NULL 可。
 *  @return         成功時はサービス オブジェクトのハンドル。失敗時は NULL。
 *  @see            CreateServiceW
 *                  https://learn.microsoft.com/windows/win32/api/winsvc/nf-winsvc-createservicew
 *
 *  @par            スレッド セーフ
 *  本関数はスレッド セーフです。
 */
CPLAT_EXPORT SC_HANDLE CPLAT_API CreateServiceU(SC_HANDLE hSCManager, const char *lpServiceName,
                                                const char *lpDisplayName, DWORD dwDesiredAccess, DWORD dwServiceType,
                                                DWORD dwStartType, DWORD dwErrorControl, const char *lpBinaryPathName,
                                                const char *lpLoadOrderGroup, LPDWORD lpdwTagId,
                                                const char *lpDependencies, const char *lpServiceStartName,
                                                const char *lpPassword);

/**
 *  @brief          既存のサービスのハンドルを開きます (UTF-8 名前版)。
 *
 *  lpServiceName を UTF-16 に変換して OpenServiceW を呼び出します。\n
 *                  変換失敗時は SetLastError(ERROR_INVALID_PARAMETER) を設定して NULL を返します。\n
 *                  それ以外の挙動は OpenServiceW と同一です。
 *  @param[in]      hSCManager                SCM ハンドル (OpenSCManagerU の戻り値)。
 *  @param[in]      lpServiceName             サービス名 (UTF-8)。
 *  @param[in]      dwDesiredAccess           アクセス権。
 *  @return         成功時はサービス オブジェクトのハンドル。失敗時は NULL。
 *  @see            OpenServiceW
 *                  https://learn.microsoft.com/windows/win32/api/winsvc/nf-winsvc-openservicew
 *
 *  @par            スレッド セーフ
 *  本関数はスレッド セーフです。
 */
CPLAT_EXPORT SC_HANDLE CPLAT_API OpenServiceU(SC_HANDLE hSCManager, const char *lpServiceName, DWORD dwDesiredAccess);

/**
 *  @brief          サービスの説明文を変更します (UTF-8 文字列版)。
 *
 *  SERVICE_CONFIG_DESCRIPTION のみ対応します。\n
 *                  lpInfo の SERVICE_DESCRIPTIONA.lpDescription を UTF-16 に変換し、SERVICE_DESCRIPTIONW を組み立てて
 *                  ChangeServiceConfig2W を呼び出します。\n
 *                  lpDescription が NULL の場合は、SERVICE_DESCRIPTIONW.lpDescription に NULL を渡します。\n
 *                  dwInfoLevel が SERVICE_CONFIG_DESCRIPTION 以外、または lpInfo が NULL の場合は
 *                  SetLastError(ERROR_INVALID_PARAMETER) を設定して FALSE を返します。\n
 *                  変換失敗時も SetLastError(ERROR_INVALID_PARAMETER) を設定して FALSE を返します。
 *  @param[in]      hService                  サービス ハンドル (OpenServiceU / CreateServiceU の戻り値)。
 *  @param[in]      dwInfoLevel               変更する構成情報の種類。SERVICE_CONFIG_DESCRIPTION のみ対応。
 *  @param[in]      lpInfo                    SERVICE_DESCRIPTIONA へのポインター。lpDescription は UTF-8。NULL は指定できません。
 *  @return         成功時は TRUE。失敗時は FALSE。
 *  @see            ChangeServiceConfig2W
 *                  https://learn.microsoft.com/windows/win32/api/winsvc/nf-winsvc-changeserviceconfig2w
 *
 *  @par            スレッド セーフ
 *  本関数はスレッド セーフです。
 */
CPLAT_EXPORT BOOL CPLAT_API ChangeServiceConfig2U(SC_HANDLE hService, DWORD dwInfoLevel, LPVOID lpInfo);

/**
 *  @brief          拡張サービス コントロール要求を処理する関数を登録します (UTF-8 名前版)。
 *
 *  lpServiceName を UTF-16 に変換して RegisterServiceCtrlHandlerExW を
 *                  呼び出します。\n
 *                  変換失敗時は SetLastError(ERROR_INVALID_PARAMETER) を設定して NULL を返します。\n
 *                  それ以外の挙動は RegisterServiceCtrlHandlerExW と同一です。
 *  @param[in]      lpServiceName             サービス名 (UTF-8)。
 *  @param[in]      lpHandlerProc             拡張ハンドラー関数へのポインター。
 *  @param[in]      lpContext                 ハンドラーへ渡すコンテキスト。NULL 可。
 *  @return         成功時はサービス ステータス ハンドル。失敗時は NULL。
 *  @see            RegisterServiceCtrlHandlerExW
 *                  https://learn.microsoft.com/windows/win32/api/winsvc/nf-winsvc-registerservicectrlhandlerexw
 *
 *  @par            スレッド セーフ
 *  本関数はスレッド セーフです。
 */
CPLAT_EXPORT SERVICE_STATUS_HANDLE CPLAT_API RegisterServiceCtrlHandlerExU(const char *lpServiceName,
                                                                           LPHANDLER_FUNCTION_EX lpHandlerProc,
                                                                           LPVOID lpContext);

/**
 *  @brief          サービス プロセスのメイン スレッドを SCM に接続します (UTF-8 テーブル版)。
 *
 *  SERVICE_TABLE_ENTRYU テーブルの各 lpServiceName を UTF-16 に変換して
 *                  SERVICE_TABLE_ENTRYW 配列を組み立て、StartServiceCtrlDispatcherW を
 *                  呼び出します。テーブルの終端要素は lpServiceName を NULL にしてください。\n
 *                  変換失敗時は SetLastError(ERROR_OUTOFMEMORY) を設定して FALSE を返します。\n
 *                  それ以外の挙動は StartServiceCtrlDispatcherW と同一です。
 *  @param[in]      lpServiceStartTable       サービス エントリの配列。終端の両メンバーは NULL。
 *  @return         成功時は TRUE。失敗時は FALSE。
 *  @see            StartServiceCtrlDispatcherW
 *                  https://learn.microsoft.com/windows/win32/api/winsvc/nf-winsvc-startservicectrldispatcherw
 *
 *  @par            スレッド セーフ
 *  本関数はスレッド セーフです。
 */
CPLAT_EXPORT BOOL CPLAT_API StartServiceCtrlDispatcherU(const SERVICE_TABLE_ENTRYU *lpServiceStartTable);

#endif /* PLATFORM_WINDOWS */

/** @} */

#endif /* CPLAT_WIN32_WIN32_H */
