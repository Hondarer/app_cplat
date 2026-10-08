/**
 *******************************************************************************
 *  @file           mmap.h
 *  @brief          メモリ マップド ファイルを抽象化する API を提供します。
 *  @author         Tetsuo Honda
 *  @date           2026/07/26
 *  @version        1.0.0
 *
 *  Linux の `mmap` と Windows の `CreateFileMapping` + `MapViewOfFile` を
 *  共通のインターフェースで扱い、ファイルをプロセスのアドレス空間へマップした
 *  アドレスを公開します。\n
 *  マップ領域への並行アクセスに必要な排他制御は提供しません。\n
 *  排他制御が必要な場合は、呼び出し側で `sync.h` の同期機能を
 *  独立して生成し、寿命を管理してください。
 *
 *  @warning        本 API はローカル ファイル システム上のファイルを対象とする設計です。\n
 *                  NFS などのネットワーク ファイル システムでは、次の理由により
 *                  プロセス間 (特にホストをまたぐ場合) の一貫性が保証されません。
 *                  - @ref cplat_mmap_flush() が反映するのはサーバーへの書き込みまでであり、
 *                    他クライアントのキャッシュを無効化するものではない。
 *                  - 他クライアントがすでにマップ済みのページは、同期機能による排他制御だけでは
 *                    最新化されない (close-to-open セマンティクスに依存する)。\n
 *                  ネットワーク ファイル システム越しの共有が必要な場合は、本 API ではなく
 *                  別方式を検討してください。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#ifndef CPLAT_MMAP_MMAP_H
#define CPLAT_MMAP_MMAP_H

#include <stddef.h>

#include <cplat/base/result.h>
#include <cplat/base/error.h>
#include <cplat/cplat_export.h>

/**
 *  @ingroup        CPLAT_MMAP
 *  @{
 */

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /** @brief マップ時のアクセス モード。 */
    typedef enum cplat_mmap_access
    {
        CPLAT_MMAP_ACCESS_READ_ONLY = 0, /**< 読み取り専用でマップする。既存ファイルが必要。 */
        CPLAT_MMAP_ACCESS_READ_WRITE = 1 /**< 読み書き両用でマップする。存在しなければ新規作成する。 */
    } cplat_mmap_access;

    /** @brief メモリ マップド ファイルのハンドル (不透明構造体)。 */
    typedef struct cplat_mmap cplat_mmap;

    /**
     *  @brief          ファイルをアタッチし、プロセスのアドレス空間へマップします。
     *  @param[in]      path         マップするファイルのパス (UTF-8)。NULL を渡してはなりません。
     *  @param[in]      access       アクセス モード (@ref cplat_mmap_access)。
     *  @param[in]      create_size  新規作成時のファイル サイズ (バイト)。\n
     *                               @p access が @ref CPLAT_MMAP_ACCESS_READ_WRITE で、
     *                               @p path のファイルが存在しない場合にのみ使用します。
     *                               0 を渡した場合は @ref CPLAT_ERR_INVALID_ARGUMENT を返します。\n
     *                               既存ファイルを開く場合は無視され、現在のファイル サイズが
     *                               マップ サイズになります。
     *  @param[out]     map          生成したハンドルの格納先。NULL を渡してはなりません。
     *  @param[out]     detail_out   エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         @ref CPLAT_OK 、@ref CPLAT_ERR_INVALID_ARGUMENT 、
     *                  @ref CPLAT_ERR_UNKNOWN のいずれかを返します。
     *
     *  @p access が @ref CPLAT_MMAP_ACCESS_READ_ONLY のとき、@p path が存在しない場合は
     *  新規作成せず @ref CPLAT_ERR_UNKNOWN を返します。\n
     *  マップ結果のサイズが 0 になる場合 (空の既存ファイルを開いた場合など) は、
     *  `mmap`/`MapViewOfFile` がサイズ 0 を扱えないため @ref CPLAT_ERR_UNKNOWN を返します。\n
     *  @warning        @p path はローカル ファイル システム上のファイルを指定してください。
     *                  ネットワーク ファイル システム上のファイルを指定した場合、
     *                  プロセス間の一貫性は保証されません (本ファイル冒頭の @warning を参照)。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_mmap_attach(const char *path, cplat_mmap_access access, size_t create_size,
                                                 cplat_mmap **map, cplat_error *detail_out);

    /**
     *  @brief          マップ済みアドレスを取得します。
     *  @param[in]      map  対象のハンドル。NULL を渡してはなりません。
     *  @return         マップ済みアドレス。@p map が NULL の場合は NULL を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT void *CPLAT_API cplat_mmap_get_address(const cplat_mmap *map);

    /**
     *  @brief          マップ サイズを取得します。
     *  @param[in]      map  対象のハンドル。NULL を渡してはなりません。
     *  @return         マップ サイズ (バイト)。@p map が NULL の場合は 0 を返します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT size_t CPLAT_API cplat_mmap_get_size(const cplat_mmap *map);

    /**
     *  @brief          マップした内容をディスクへ反映します。
     *  @param[in]      map      対象のハンドル。NULL を渡してはなりません。
     *  @param[in]      address  反映対象の先頭アドレス。NULL の場合はマップ全体を対象にします。
     *  @param[in]      length   反映対象のサイズ (バイト)。@p address が NULL の場合は無視されます。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         成功時は @ref CPLAT_OK 、失敗時は共通結果コードを返します。
     *
     *  Linux では `msync(MS_SYNC)`、Windows では `FlushViewOfFile` に続けて
     *  `FlushFileBuffers` を呼び出します。
     *
     *  @par            スレッド セーフ
     *  本関数はスレッド セーフです。
     */
    CPLAT_EXPORT int CPLAT_API cplat_mmap_flush(cplat_mmap *map, void *address, size_t length, cplat_error *detail_out);

    /**
     *  @brief          マッピングを解除し、ハンドルを破棄します。
     *  @param[in]      map         破棄するハンドル。NULL の場合は何もしません。
     *  @param[out]     detail_out  エラー詳細の格納先。NULL を指定した場合、本引数へは
     *                  エラー詳細を設定せず、返却しません。
     *                  NULL 以外を指定した場合、成功時は空の値を格納します。
     *  @return         成功時は @ref CPLAT_OK 、失敗時は共通結果コードを返します。
     *
     *  @par            スレッド セーフ
     *  本関数は条件付きスレッド セーフです。\n
     *  異なるマップ ハンドルに対する呼び出しは同時に実行できます。\n
     *  同一マップ ハンドルに対する操作は、呼び出し側で直列化してください。
     */
    CPLAT_EXPORT int CPLAT_API cplat_mmap_detach(cplat_mmap *map, cplat_error *detail_out);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @} */

#endif /* CPLAT_MMAP_MMAP_H */
