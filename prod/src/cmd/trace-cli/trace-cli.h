#ifndef TRACE_CLI_PRIVATE_H
#define TRACE_CLI_PRIVATE_H

#include <cplat/trace/tracer.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct trace_cli_session
    {
        cplat_tracer *handle;
        int prompt_state;   /* プロンプト表示用のセッション状態 */
        int exit_requested; /* 終了要求フラグ (1 で終了) */
    } trace_cli_session;

    void trace_cli_session_init(trace_cli_session *session);
    void trace_cli_session_dispose(trace_cli_session *session);
    void trace_cli_print_help(void);
    int trace_cli_process_line(trace_cli_session *session, const char *line);

#ifdef __cplusplus
}
#endif

#endif /* TRACE_CLI_PRIVATE_H */
