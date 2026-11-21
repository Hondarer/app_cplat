/**
 *******************************************************************************
 *  @file           eventlog-register.c
 *  @brief          cplat 共通イベント ソースを登録および削除するコマンドを実装します。
 *  @author         Tetsuo Honda
 *  @date           2026/06/14
 *  @version        1.0.0
 *
 *  Windows のアプリケーション イベント ログに cplat 共通イベント ソースを
 *  登録/削除します。HKLM への書き込みには管理者権限が必要なため、未昇格時は
 *  UAC 昇格を行います。\n
 *  登録先は `HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\EventLog\Application\cplat.tracer`
 *  キーです。ソース名 `cplat.tracer` は `CPLAT_TRACER_DEFAULT_PROVIDER_NAME` の値で、
 *  登録状態の確認はこのキーの有無で判断できます。\n
 *  Linux ではイベント ログを使用しないため、案内を表示して終了します。
 *
 *  @copyright      Copyright (C) Tetsuo Honda. 2026. All rights reserved.
 *
 *******************************************************************************
 */

#include "eventlog-register.h"

#include <cplat/argparser/argparser.h>
#include <cplat/base/platform.h>
#include <cplat/console/console.h>
#include <cplat/runtime/elevated_process.h>
#include <cplat/runtime/process.h>
#include <cplat/trace/tracer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(PLATFORM_WINDOWS)

    #include <cplat/base/windows_sdk.h>
    #include <cplat/crt/path.h>
    #include <cplat/crt/wchar_conv.h>
    #include <cplat/runtime/module.h>
    #include <cplat/trace/eventlog.h>

/** Win32 RT_MESSAGETABLE のリソース種別番号です。 */
/* see: https://learn.microsoft.com/en-us/windows/win32/menurc/resource-types */
    #define EVENTLOG_MESSAGETABLE_TYPE_ID 11

/**
 *  @brief          管理者権限を保証します。必要なら UAC 昇格して再実行します。
 *  @param[in]      command  昇格再実行するサブコマンド ("install" / "uninstall")。
 *  @param[out]     handled  昇格プロセスで処理済みの場合は 0 以外を格納します。
 *  @return         継続可能な場合は 0、異常時は 0 以外を返します。
 *
 *  昇格プロセスの標準出力と標準エラー出力は名前付きパイプで受け取り、本プロセスの
 *  標準出力と標準エラー出力へそのまま書き込みます。
 */
static int ensure_elevated(const char *command, int *handled)
{
    int exit_code;
    int ret;

    if (handled == NULL)
    {
        return -1;
    }

    exit_code = 1;
    ret = cplat_elevated_process_run_piped(command, NULL, NULL, &exit_code, handled);
    if (ret != 0)
    {
        /* 診断の出力先がほかにないため、戻り値は明示的に破棄する。 */
        (void)fprintf(stderr, "管理者権限での実行に失敗しました。\n");
        return -1;
    }

    if (*handled != 0 && exit_code != 0)
    {
        /*
         * 昇格プロセスの診断は、出力の接続に失敗すると届かない。
         * 利用者に失敗が必ず見えるよう、終了コードを呼び出し元の側で表示する。
         * 診断の出力先がほかにないため、戻り値は明示的に破棄する。
         */
        (void)fprintf(stderr, "昇格したプロセスが失敗しました (終了コード: %d)。\n", exit_code);
        return -1;
    }

    return 0;
}

/**
 *  @brief          イベント ソース API のステータスを表示します。
 *  @param[in]      ret     cplat_eventlog_register_source / unregister_source の戻り値。
 *  @param[in]      action  操作名 ("登録" / "削除")。
 *  @return         正常終了時は 0、異常終了時は 0 以外を返します。
 *
 *  本プロセスが昇格ワーカーの場合、標準出力と標準エラー出力は呼び出し元プロセスのパイプへ
 *  接続済みのため、呼び出し元のコンソールに表示されます。
 */
static int report_status(const int ret, const char *action, const char *message_file_path)
{
    if (ret == CPLAT_OK)
    {
        /*
         * 結果の出力の失敗は標準出力のストリームに残り、main が終了の直前にまとめて検査するため、
         * この 2 つの printf の戻り値は明示的に破棄する。
         */
        (void)printf("イベント ソース '%s' を%sしました。\n", CPLAT_TRACER_DEFAULT_PROVIDER_NAME, action);

        if (message_file_path != NULL)
        {
            (void)printf("メッセージ DLL: %s\n"
                         "注: メッセージ DLL は、イベント ソースの登録が有効な期間を通じて必要です。\n",
                         message_file_path);
        }

        return 0;
    }

    /* 診断の出力先がほかにないため、この関数の fprintf の戻り値は明示的に破棄する。 */
    if (ret == CPLAT_ERR_PERMISSION_DENIED)
    {
        (void)fprintf(stderr, "アクセスが拒否されました。管理者として実行してください。\n");
    }
    else if (ret == CPLAT_ERR_NOT_FOUND || ret == CPLAT_ERR_CORRUPT_DESCRIPTOR)
    {
        (void)fprintf(stderr, "メッセージ リソース DLL を確認できません。libcplat.dll と同じディレクトリに "
                              "libcplat_eventlog_messages.dll を配置してください。\n");
    }
    else if (ret == CPLAT_ERR_INVALID_ARGUMENT)
    {
        (void)fprintf(stderr, "パラメーターが不正です。\n");
    }
    else
    {
        (void)fprintf(stderr, "システム エラーにより%sに失敗しました。\n", action);
    }

    return -1;
}

/**
 *  @brief          共通イベント ソースを登録します。
 *  @return         正常終了時は 0、異常終了時は 0 以外を返します。
 */
static int do_install(void)
{
    int handled = 0;
    int rc;
    int ret;
    char module_path[PLATFORM_PATH_UTF8_MAX];
    char directory[PLATFORM_PATH_UTF8_MAX];
    char message_file[PLATFORM_PATH_UTF8_MAX];
    wchar_t wpath[PLATFORM_PATH_MAX];
    HMODULE resources;
    HRSRC table;

    /* 管理者権限がない場合は UAC 昇格を試行し、昇格側で処理済みなら終了する。 */
    rc = ensure_elevated("install", &handled);
    if (rc != 0 || handled != 0)
    {
        return rc;
    }

    /* 使用中の libcplat と同じディレクトリに配置したリソース DLL を登録する。 */
    ret = cplat_module_get_path(module_path, sizeof(module_path), (const void *)cplat_eventlog_register_source);
    if (ret != CPLAT_OK)
    {
        return report_status(ret, "登録", NULL);
    }

    ret = cplat_path_dirname(directory, sizeof(directory), NULL, module_path);
    if (ret != CPLAT_OK)
    {
        return report_status(ret, "登録", NULL);
    }

    ret = cplat_path_join(message_file, sizeof(message_file), NULL, directory, "libcplat_eventlog_messages.dll");
    if (ret != CPLAT_OK)
    {
        return report_status(ret, "登録", NULL);
    }

    if (cplat_utf8_to_wpath(wpath, sizeof(wpath) / sizeof(wpath[0]), message_file) < 0)
    {
        return report_status(CPLAT_ERR_UNKNOWN, "登録", NULL);
    }

    /*
     * 実行コードを読み込まず、登録前にメッセージ テーブルの存在を確認する。
     * LoadLibraryExW が要求するパス区切りへ変換する。
     * see: https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryexw
     */
    for (wchar_t *separator = wpath; *separator != L'\0'; ++separator)
    {
        if (*separator == L'/')
        {
            *separator = L'\\';
        }
    }

    resources = LoadLibraryExW(wpath, NULL, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    if (resources == NULL)
    {
        return report_status(CPLAT_ERR_NOT_FOUND, "登録", NULL);
    }

    /* mc.exe が生成するテーブルは ID 1、リソース種別は RT_MESSAGETABLE (11)。 */
    table = FindResourceW(resources, MAKEINTRESOURCEW(1), MAKEINTRESOURCEW(EVENTLOG_MESSAGETABLE_TYPE_ID));
    /*
     * 存在の確認だけにデータ ファイルとして読み込んだ DLL の解放であり、失敗しても登録の判定は変わらないため、
     * 戻り値は明示的に破棄する。
     */
    (void)FreeLibrary(resources);

    if (table == NULL)
    {
        return report_status(CPLAT_ERR_CORRUPT_DESCRIPTOR, "登録", NULL);
    }

    ret = cplat_eventlog_register_source(CPLAT_TRACER_DEFAULT_PROVIDER_NAME, message_file);
    if (ret == CPLAT_OK)
    {
        return report_status(ret, "登録", message_file);
    }

    return report_status(ret, "登録", NULL);
}

/**
 *  @brief          共通イベント ソースの登録を削除します。
 *  @return         正常終了時は 0、異常終了時は 0 以外を返します。
 */
static int do_uninstall(void)
{
    int handled = 0;
    int rc;
    int ret;

    /* 管理者権限がない場合は UAC 昇格を試行し、昇格側で処理済みなら終了する。 */
    rc = ensure_elevated("uninstall", &handled);
    if (rc != 0 || handled != 0)
    {
        return rc;
    }

    ret = cplat_eventlog_unregister_source(CPLAT_TRACER_DEFAULT_PROVIDER_NAME);

    return report_status(ret, "削除", NULL);
}

/* Doxygen コメントは、ヘッダーに記載 */

int eventlog_register_run(const char *command)
{
    if (strcmp(command, "install") == 0)
    {
        return do_install();
    }

    if (strcmp(command, "uninstall") == 0)
    {
        return do_uninstall();
    }

    /* command は main() の argparser 検証により install/uninstall のいずれかへ確定するため到達しない。 */
    return -1;
}

#else /* PLATFORM_LINUX */

/* Doxygen コメントは、ヘッダーに記載 */

int eventlog_register_run(const char *command)
{
    (void)command;

    /* 診断の出力先がほかにないため、戻り値は明示的に破棄する。 */
    (void)fprintf(stderr, "eventlog-register は Windows 専用です。Linux ではイベント ログを使用しません。\n");

    return -1;
}

#endif /* PLATFORM_ */

/**
 *  @brief          メイン エントリ ポイントです。
 *  @param[in]      argc  コマンド ライン引数の数。
 *  @param[in]      argv  コマンド ライン引数の配列。
 *  @return         正常終了時は 0、異常終了時は 0 以外を返します。
 */
int main(int argc, char *argv[])
{
    int run_result;

    /*
     * 昇格ワーカーとして再起動された場合、出力パイプ フラグを argv から取り除き、
     * stdout / stderr を呼び出し元プロセスのパイプへ接続する。引数解析より前に呼び出す。
     */
    if (cplat_elevated_process_attach_output_pipes(&argc, argv, NULL) != CPLAT_OK)
    {
        return EXIT_FAILURE;
    }

    cplat_console_init();

    int need_help = 0;
    const char *command = NULL;

    cplat_argparser_init(argc, argv, "cplat 共通イベント ソースを登録または削除します。");
    /*
     * 登録の失敗はパーサーに記録され、直後の cplat_argparser_get_register_error_count() でまとめて検査するため、
     * 登録の関数の戻り値は明示的に破棄する。
     */
    (void)cplat_argparser_register_flag("-h", "--help", "ヘルプを表示します。", &need_help);
    (void)cplat_argparser_register_positional_string("command", "install または uninstall を指定します。",
                                                     CPLAT_ARGPARSER_REQUIRED, &command);

    if (cplat_argparser_get_register_error_count() > 0)
    {
        /* 終了コードはすでに失敗に決まっているため、戻り値は明示的に破棄する。 */
        (void)cplat_argparser_print_register_error_messages(stderr);
        return EXIT_FAILURE;
    }

    int parse_result = cplat_argparser_parse();

    if (need_help != 0)
    {
        if (cplat_argparser_print_usage(stdout) != CPLAT_OK || fflush(stdout) != 0 || ferror(stdout) != 0)
        {
            (void)fprintf(stderr, "標準出力へ書き込めませんでした。\n"); /* 診断の出力先がほかにないため、戻り値は明示的に破棄する */
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }

    if (parse_result != CPLAT_OK)
    {
        /* 終了コードはすでに失敗に決まっているため、この 2 つの表示の関数の戻り値は明示的に破棄する。 */
        (void)cplat_argparser_print_error_messages(stderr);
        (void)cplat_argparser_print_usage(stderr);
        return EXIT_FAILURE;
    }

    if (strcmp(command, "install") != 0 && strcmp(command, "uninstall") != 0)
    {
        /*
         * 終了コードはすでに失敗に決まっており、診断の出力先もほかにないため、
         * この fprintf と表示の関数の戻り値は明示的に破棄する。
         */
        (void)fprintf(stderr, "不明なコマンド '%s'\n\n", command);
        (void)cplat_argparser_print_usage(stderr);
        return EXIT_FAILURE;
    }

    run_result = eventlog_register_run(command);

    if (run_result != 0)
    {
        return EXIT_FAILURE;
    }

    if (fflush(stdout) != 0 || ferror(stdout) != 0)
    {
        (void)fprintf(stderr, "標準出力へ書き込めませんでした。\n"); /* 診断の出力先がほかにないため、戻り値は明示的に破棄する */
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
