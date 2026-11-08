/**
 *******************************************************************************
 *  @file           error_message_internal.h
 *  @brief          ドメイン固有の OS エラー値を文字列化する内部 API を提供します。
 *
 *  @hideincludedbygraph
 *
 *******************************************************************************
 */

#ifndef CPLAT_BASE_ERROR_MESSAGE_INTERNAL_H
#define CPLAT_BASE_ERROR_MESSAGE_INTERNAL_H

#include <cplat/base/error_message.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

    /**
     *  @brief          errno の値に対応するメッセージを取得します。
     *  @param[out]     buf         メッセージの格納先。
     *  @param[in]      buf_size    @p buf のバイト数。
     *  @param[in]      errno_value errno の値。
     *  @return         CPLAT_OK、CPLAT_ERR_INVALID_ARGUMENT、CPLAT_ERR_UNKNOWN、CPLAT_ERR_UNSUPPORTED のいずれかを返します。
     *  @note           対応していない OS では CPLAT_ERR_UNSUPPORTED を返します。
     */
    int cplat_internal_errno_message(char *buf, size_t buf_size, int errno_value);

#if defined(PLATFORM_WINDOWS)
    /**
     *  @brief          Win32 エラー コードに対応するメッセージを取得します。
     *  @param[out]     buf        メッセージの格納先。
     *  @param[in]      buf_size   @p buf のバイト数。
     *  @param[in]      error_code GetLastError() が返した値。
     *  @return         CPLAT_OK、CPLAT_ERR_INVALID_ARGUMENT、CPLAT_ERR_UNKNOWN のいずれかを返します。
     */
    int cplat_internal_win32_error_message(char *buf, size_t buf_size, unsigned long error_code);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CPLAT_BASE_ERROR_MESSAGE_INTERNAL_H */
