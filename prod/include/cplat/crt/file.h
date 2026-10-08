/**
 *******************************************************************************
 *  @file           file.h
 *  @brief          低レベルのファイル I/O を抽象化する API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/04/24
 *
 *  Windows の HANDLE ベース I/O と Linux の fd ベース I/O を共通化し、
 *  UTF-8 パスを受け取る低レベル書き込み用 API を提供します。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

/* NOTE: このヘッダーは多数のソース ファイルから参照されるため、            */
/*       @hideincludedbygraph によって "Included by" グラフを無効にします。 */

#ifndef CPLAT_CRT_FILE_H
#define CPLAT_CRT_FILE_H

#include <stddef.h>
#include <stdint.h>
#include <cplat/base/platform.h>
#include <cplat/base/error.h>
#include <cplat/base/result.h>
#include <cplat/clock/timespec.h>
#include <cplat/cplat_export.h>

#if defined(PLATFORM_WINDOWS)
    #include <cplat/base/windows_sdk.h>
#endif /* PLATFORM_WINDOWS */

/**
 *  @ingroup        CPLAT_CRT
 *  @{
 */

#define CPLAT_FILE_OPEN_CREATE        (1 << 0) /**< ファイルが存在しない場合に新規作成する。 */
#define CPLAT_FILE_OPEN_TRUNCATE      (1 << 1) /**< 既存ファイルを開く際に内容を切り詰める。 */
#define CPLAT_FILE_OPEN_APPEND        (1 << 2) /**< 書き込みをファイル末尾に追記する。 */
#define CPLAT_FILE_OPEN_WRITE_THROUGH (1 << 3) /**< 書き込みをバッファリングせずディスクに直接書き出す。 */
#define CPLAT_FILE_OPEN_READ          (1 << 4) /**< 読み取りアクセスを要求する。 */
#define CPLAT_FILE_OPEN_WRITE \
    (1 << 5) /**< 書き込みアクセスを要求する。@ref CPLAT_FILE_OPEN_READ と \
                  @ref CPLAT_FILE_OPEN_WRITE のいずれも指定しない場合は、\
                  既定で書き込み専用アクセスとして扱い、\
                  @ref CPLAT_FILE_OPEN_WRITE が指定されているものとして扱う。 */
#define CPLAT_FILE_OPEN_CREATE_NEW \
    (1 << 6) /**< 新規作成のみ許可する。既存ファイルがある場合は失敗する。\
                  @ref CPLAT_FILE_OPEN_CREATE の指定が必須であり、指定しない場合 \
                  cplat_file_open() は呼び出し自体を @ref CPLAT_ERR_INVALID_ARGUMENT で失敗させる。 */

/**
 *  @brief  ファイル ハンドルの抽象化構造体 (Linux の fd、Windows の HANDLE を保持) です。
 */
typedef struct cplat_file
{
#if defined(PLATFORM_LINUX)
    int handle;
#elif defined(PLATFORM_WINDOWS)
    HANDLE handle;
#endif            /* PLATFORM_ */
    int writable; /**< cplat_file_open() で書き込みアクセスを要求した場合に 0 以外。 */
} cplat_file;

/**
 *  @brief  ファイル実体の同一性を表す構造体です。
 *
 *  同じボリューム上の同じファイル実体であれば、開き直しても同じ値になります。\n
 *  Linux では stat の st_dev と st_ino、Windows では GetFileInformationByHandle の
 *  dwVolumeSerialNumber と nFileIndexHigh / nFileIndexLow に対応します。\n
 *  リネームではファイル実体が変わらないため値は変化せず、
 *  同一パスにファイルが作り直されたことの検出に使用できます。
 */
typedef struct cplat_file_id
{
    /* OS のファイル識別子の幅に合わせるため、コーディング規範の例外として uint64_t を使用する。 */
    uint64_t volume; /**< ボリューム識別子 (Linux は st_dev、Windows は dwVolumeSerialNumber)。 */
    uint64_t index;  /**< ファイル識別子 (Linux は st_ino、Windows は nFileIndexHigh / nFileIndexLow の連結)。 */
} cplat_file_id;

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          `cplat_file` 構造体を無効状態に初期化します。
     *  @param[out]     file  初期化対象の構造体。NULL を渡してはなりません。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p file に対する呼び出しは同時に実行できます。\n
     *  同一 @p file に対する並行操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT void CPLAT_API cplat_file_init(cplat_file *file);

    /**
     *  @brief          UTF-8 パスでファイルを開きます。
     *  @param[in,out]  file   オープン結果の格納先。NULL を渡してはなりません。
     *                         すでにオープン済みの場合は先にクローズしてから開き直します。
     *  @param[in]      path   開くファイルのパス (UTF-8)。NULL を渡してはなりません。
     *  @param[in]      flags  オープン フラグ (@ref CPLAT_FILE_OPEN_CREATE 等の OR 結合)。
     *                         負値を渡した場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。\n
     *                         @ref CPLAT_FILE_OPEN_READ / @ref CPLAT_FILE_OPEN_WRITE を
     *                         いずれも指定しない場合は、既定で書き込み専用アクセスとして開き、
     *                         @ref CPLAT_FILE_OPEN_WRITE が指定されているものとして扱います。\n
     *                         @ref CPLAT_FILE_OPEN_CREATE_NEW を @ref CPLAT_FILE_OPEN_CREATE
     *                         なしで指定した場合も @ref CPLAT_ERR_INVALID_ARGUMENT を返します。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK または @ref CPLAT_ERR_UNKNOWN を返します。
     *
     *  @par            ファイル共有と排他制御
     *  @ref CPLAT_FILE_OPEN_READ と @ref CPLAT_FILE_OPEN_WRITE は、本関数が作成するハンドルの
     *  アクセス権を指定するフラグです。他プロセスによるファイル アクセスの可否は指定しません。\n
     *  Windows では `FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE` を指定し、本関数が
     *  作成するハンドルによって他プロセスの読み取り、書き込み、削除を妨げないようにします。
     *  Linux の `open()` には、これらの操作を拒否する共有モードがありません。\n
     *  本関数は排他的オープンを提供しません。複数プロセスによるアクセスを直列化する場合は、
     *  `cplat_interprocess_lock` または `cplat_interprocess_rwlock` (`sync.h`) を使用してください。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p file に対する呼び出しは同時に実行できます。\n
     *  同一 @p file に対する並行操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_file_open(cplat_file *file, const char *path, int flags, cplat_error *detail_out);

    /**
     *  @brief          ファイルにバイト列を書き込みます。
     *  @param[in]      file  書き込み対象のファイル。NULL を渡してはなりません。\n
     *                        @ref CPLAT_FILE_OPEN_WRITE を指定して開いている必要があります。
     *  @param[in]      buf   書き込むデータ。NULL を渡してはなりません。
     *  @param[in]      len   書き込むバイト数。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_PERMISSION_DENIED 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。\n
     *                  読み取り専用でオープンしたハンドルに対しては、OS の書き込み呼び出しを
     *                  行わずに @ref CPLAT_ERR_PERMISSION_DENIED を返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p file に対する呼び出しは同時に実行できます。\n
     *  同一 @p file への並行書き込みは、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_file_write(cplat_file *file, const void *buf, size_t len, cplat_error *detail_out);

    /**
     *  @brief          ファイルからバイト列を読み取ります。
     *  @param[in]      file      読み取り対象のファイル。NULL を渡してはなりません。\n
     *                            @ref CPLAT_FILE_OPEN_READ を指定して開いている必要があります。
     *  @param[out]     buf       読み取り先。NULL を渡してはなりません。
     *  @param[in]      len       読み取る最大バイト数。
     *  @param[out]     read_out  実際に読み取ったバイト数の格納先。NULL を渡してはなりません。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  ファイル終端に達した場合は @p read_out に 0 を格納して @ref CPLAT_OK を返します。\n
     *  @p read_out が @p len より小さい値になることは終端以外でも起こりうるため、
     *  必要なバイト数が揃うまで呼び出し側で繰り返してください。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p file に対する呼び出しは同時に実行できます。\n
     *  同一 @p file への並行読み取りは、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_file_read(cplat_file *file, void *buf, size_t len, size_t *read_out,
                                               cplat_error *detail_out);

    /**
     *  @brief          ファイル サイズを取得します。
     *  @param[in]      file      対象のファイル。NULL を渡してはなりません。
     *  @param[out]     size_out  サイズ (バイト) の格納先。NULL を渡してはなりません。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK または @ref CPLAT_ERR_UNKNOWN を返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p file に対する呼び出しは同時に実行できます。\n
     *  同一 @p file に対するクローズや書き込みと並行して呼び出さないでください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_file_get_size(const cplat_file *file, size_t *size_out, cplat_error *detail_out);

    /**
     *  @brief          ファイル サイズを指定値に設定します (拡張または切り詰め)。
     *  @param[in]      file  対象のファイル。書き込みアクセスでオープン済みでなければなりません。
     *                        NULL を渡してはなりません。
     *  @param[in]      size  設定後のファイル サイズ (バイト)。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK または @ref CPLAT_ERR_UNKNOWN を返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p file に対する呼び出しは同時に実行できます。\n
     *  同一 @p file への並行操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_file_set_size(cplat_file *file, size_t size, cplat_error *detail_out);

    /**
     *  @brief          開いているファイルの同一性情報を取得します。
     *  @param[in]      file    対象のファイル。NULL を渡してはなりません。
     *  @param[out]     id_out  同一性情報の格納先。NULL を渡してはなりません。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK または @ref CPLAT_ERR_UNKNOWN を返します。
     *
     *  Linux では fstat、Windows では GetFileInformationByHandle で取得します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p file に対する呼び出しは同時に実行できます。\n
     *  同一 @p file に対するクローズ操作と並行して呼び出さないでください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_file_get_id(const cplat_file *file, cplat_file_id *id_out,
                                                 cplat_error *detail_out);

    /**
     *  @brief          UTF-8 パスが現在指しているファイルの同一性情報を取得します。
     *  @param[in]      path    対象ファイルのパス (UTF-8)。NULL を渡してはなりません。
     *  @param[out]     id_out  同一性情報の格納先。NULL を渡してはなりません。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK または @ref CPLAT_ERR_UNKNOWN を返します (パスが存在しない場合を含む)。
     *
     *  `cplat_file_get_id()` の結果と比較することで、開いているファイルが
     *  そのパスの最新の実体を指しているかを判定できます。\n
     *  Windows では属性読み取りアクセス (FILE_READ_ATTRIBUTES) で一時的に開いて取得するため、
     *  他プロセスの共有モードによらず取得できます。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_file_get_path_id(const char *path, cplat_file_id *id_out, cplat_error *detail_out);

    /**
     *  @brief          開いているファイルの最終更新日時を取得します。
     *  @param[in]      file           対象のファイル。NULL または無効なハンドルを渡してはなりません。
     *  @param[out]     timestamp_out  最終更新日時の格納先。NULL を渡してはなりません。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  Linux では `fstat` の `st_mtim`、Windows では `GetFileTime` の最終書き込み日時を使用します。\n
     *  `cplat_stat()` の `st_mtime` は秒精度ですが、本関数はサブ秒の値も返します。
     *
     *  @note           取得できる分解能はファイル システムに依存します。
     *                  Windows の NTFS は 100 ナノ秒、FAT は 2 秒です。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p file に対する呼び出しは同時に実行できます。\n
     *  同一 @p file に対するクローズ操作と並行して呼び出さないでください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_file_get_modified_timestamp(const cplat_file *file, cplat_timespec *timestamp_out,
                                                                 cplat_error *detail_out);

    /**
     *  @brief          開いているファイルの最終更新日時を設定します。
     *  @param[in]      file       対象のファイル。NULL または無効なハンドルを渡してはなりません。\n
     *                             @ref CPLAT_FILE_OPEN_WRITE を指定して開いている必要があります。
     *  @param[in]      timestamp  設定する最終更新日時。NULL を渡してはなりません。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_PERMISSION_DENIED 、@ref CPLAT_ERR_UNKNOWN の
     *                  いずれかを返します。\n
     *                  読み取り専用でオープンしたハンドルに対しては、OS の呼び出しを
     *                  行わずに @ref CPLAT_ERR_PERMISSION_DENIED を返します。
     *
     *  最終アクセス日時は変更しません。\n
     *  Linux では `futimens`、Windows では `SetFileTime` を使用します。
     *
     *  @note           書き込みアクセスを要求するのは、両プラットフォームで挙動をそろえるためです。
     *                  Linux の `futimens` は読み取り専用で開いた記述子でも、
     *                  呼び出し元がファイルの所有者であれば成功します。
     *                  厳しい側の Windows に合わせ、本関数は事前に検査して失敗させます。
     *
     *  @note           設定できる分解能はファイル システムに依存します。
     *                  Windows では 100 ナノ秒単位へ切り捨てられます。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p file に対する呼び出しは同時に実行できます。\n
     *  同一 @p file への並行操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_file_set_modified_timestamp(cplat_file *file, const cplat_timespec *timestamp,
                                                                 cplat_error *detail_out);

    /**
     *  @brief          UTF-8 パスが指すファイルの最終更新日時を取得します。
     *  @param[in]      path           対象ファイルのパス (UTF-8)。NULL を渡してはなりません。
     *  @param[out]     timestamp_out  最終更新日時の格納先。NULL を渡してはなりません。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します
     *                  (パスが存在しない場合を含みます)。
     *
     *  Windows では属性読み取りアクセス (FILE_READ_ATTRIBUTES) で一時的に開いて取得するため、
     *  他プロセスの共有モードによらず取得できます。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_file_get_path_modified_timestamp(const char *path, cplat_timespec *timestamp_out,
                                                                      cplat_error *detail_out);

    /**
     *  @brief          UTF-8 パスが指すファイルの最終更新日時を設定します。
     *  @param[in]      path       対象ファイルのパス (UTF-8)。NULL を渡してはなりません。
     *  @param[in]      timestamp  設定する最終更新日時。NULL を渡してはなりません。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_PERMISSION_DENIED 、@ref CPLAT_ERR_UNKNOWN の
     *                  いずれかを返します (パスが存在しない場合を含みます)。
     *
     *  最終アクセス日時は変更しません。\n
     *  Linux では `utimensat`、Windows では属性書き込みアクセス (FILE_WRITE_ATTRIBUTES) で
     *  一時的に開いて `SetFileTime` を使用します。\n
     *  ファイルの書き込み権限が必要です。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるパスに対する呼び出しは同時に実行できます。\n
     *  同一パスに対する並行操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_file_set_path_modified_timestamp(const char *path, const cplat_timespec *timestamp,
                                                                      cplat_error *detail_out);

    /**
     *  @brief          ファイルへ書き込んだ内容を永続記憶装置へ反映します。
     *  @param[in]      file       対象のファイル。NULL または無効なハンドルを渡してはなりません。
     *  @param[out]     detail_out エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         成功時は @ref CPLAT_OK 、失敗時は共通結果コードを返します。
     *
     *  Linux では `fsync`、Windows では `FlushFileBuffers` を使用します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p file に対する呼び出しは同時に実行できます。\n
     *  同一 @p file に対する並行操作は、呼び出し側で同期してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_file_flush(cplat_file *file, cplat_error *detail_out);

    /**
     *  @brief          ファイルを閉じます。
     *                  `cplat_file_init()` で初期化済みの無効ハンドルには何もしません。
     *  @param[in]      file  閉じるファイル。NULL を渡してはなりません。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         成功時は @ref CPLAT_OK 、失敗時は共通結果コードを返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なる @p file に対する呼び出しは同時に実行できます。\n
     *  同一 @p file に対する並行操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_file_close(cplat_file *file, cplat_error *detail_out);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_CRT_FILE_H */
