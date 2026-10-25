/* テスト対象ソース ファイルの注入用追加ソース
 * このソースはテスト対象ソースの末尾に結合されます
 * この static 関数へのアクセサーによって
 * テスト プログラムからテスト対象ソースの static 関数にアクセスできます
 */
#ifndef _IN_TEST_SRC
    #include "pinned_prompt.c"
#endif /* _IN_TEST_SRC */

#include "pinned_prompt.inject.h"

void test_pinned_prompt_resize(cplat_pinned_prompt *screen)
{
    pinned_prompt_resize_locked(screen);
}

size_t test_pinned_prompt_cstr_len(const char *text)
{
    return cstr_len(text);
}

void test_pinned_prompt_set_tty(cplat_pinned_prompt *screen, int is_tty)
{
    screen->is_tty = is_tty;
}

int test_pinned_prompt_read_key(cplat_pinned_prompt *screen, int *ch_out)
{
    return (int)pinned_prompt_read_key(screen, ch_out);
}

void test_pinned_prompt_set_edit_line(cplat_pinned_prompt *screen, const char *line)
{
    pinned_prompt_set_edit_line(screen, line);
}

void test_pinned_prompt_set_cursor(cplat_pinned_prompt *screen, size_t cursor)
{
    screen->cursor = cursor;
}

void test_pinned_prompt_insert_byte(cplat_pinned_prompt *screen, int ch)
{
    pinned_prompt_insert_byte(screen, ch);
}

void test_pinned_prompt_backspace(cplat_pinned_prompt *screen)
{
    pinned_prompt_backspace(screen);
}

void test_pinned_prompt_delete(cplat_pinned_prompt *screen)
{
    pinned_prompt_delete(screen);
}

const char *test_pinned_prompt_edit_text(const cplat_pinned_prompt *screen)
{
    return screen->edit_buf;
}

size_t test_pinned_prompt_edit_length(const cplat_pinned_prompt *screen)
{
    return screen->edit_len;
}

void test_pinned_prompt_render_state(cplat_pinned_prompt *screen, int is_tty, int prompt_visible,
                                     int status_top_enabled, int status_bottom_enabled, const char *prompt,
                                     const char *edit_line, const char *top_left, const char *top_right,
                                     const char *bottom_left, const char *bottom_right)
{
    screen->is_tty = is_tty;
    screen->prompt_visible = prompt_visible;
    screen->status_top_enabled = status_top_enabled;
    screen->status_bottom_enabled = status_bottom_enabled;
    screen->status_dirty = 1;
    (void)pinned_prompt_set_prompt(screen, prompt);
    (void)pinned_prompt_set_status_content(&screen->status_top_left, &screen->status_top_left_cap, top_left);
    (void)pinned_prompt_set_status_content(&screen->status_top_right, &screen->status_top_right_cap, top_right);
    (void)pinned_prompt_set_status_content(&screen->status_bottom_left, &screen->status_bottom_left_cap, bottom_left);
    (void)pinned_prompt_set_status_content(&screen->status_bottom_right, &screen->status_bottom_right_cap,
                                           bottom_right);
    pinned_prompt_set_edit_line(screen, edit_line);
}

void test_pinned_prompt_render(cplat_pinned_prompt *screen)
{
    pinned_prompt_render_locked(screen);
}

void test_pinned_prompt_history_edge_cases(cplat_pinned_prompt *screen)
{
    pinned_prompt_history_ctx *ctx;

    ctx = pinned_prompt_find_or_create_history_ctx(screen, "edge.c", 1);
    pinned_prompt_history_prev(screen, NULL);
    pinned_prompt_history_next(screen, NULL);
    pinned_prompt_clear_edit_line(screen, NULL);
    pinned_prompt_history_prev(screen, ctx);
    pinned_prompt_history_add(screen, ctx, "same");
    pinned_prompt_history_add(screen, ctx, "same");
    pinned_prompt_history_prev(screen, ctx);
    pinned_prompt_history_prev(screen, ctx);
    pinned_prompt_history_next(screen, ctx);
    pinned_prompt_history_prev(screen, ctx);
}

void test_pinned_prompt_prepare_output(cplat_pinned_prompt *screen)
{
    pinned_prompt_prepare_output_locked(screen);
}

void test_pinned_prompt_set_internal_state(cplat_pinned_prompt *screen, int cols, int rows,
                                           int previous_main_bottom_row, int prompt_visible, int status_top_enabled,
                                           int status_bottom_enabled, size_t cursor, size_t view_start)
{
    screen->cols = cols;
    screen->rows = rows;
    screen->prev_main_bottom_row = previous_main_bottom_row;
    screen->prompt_visible = prompt_visible;
    screen->status_top_enabled = status_top_enabled;
    screen->status_bottom_enabled = status_bottom_enabled;
    screen->cursor = cursor;
    screen->view_start = view_start;
}

size_t test_pinned_prompt_view_start(const cplat_pinned_prompt *screen)
{
    return screen->view_start;
}

void test_pinned_prompt_set_mutex_active(cplat_pinned_prompt *screen, int mutex_active)
{
    screen->mutex_active = mutex_active;
}

void test_pinned_prompt_set_input_limits(cplat_pinned_prompt *screen, size_t history_max, size_t input_max_bytes)
{
    screen->history_max = history_max;
    screen->input_max_bytes = input_max_bytes;
}

void test_pinned_prompt_lock_and_unlock(cplat_pinned_prompt *screen)
{
    pinned_prompt_lock(screen);
    pinned_prompt_unlock(screen);
}

int test_pinned_prompt_set_prompt(cplat_pinned_prompt *screen, const char *prompt_string)
{
    return pinned_prompt_set_prompt(screen, prompt_string);
}

void test_pinned_prompt_adjust_view(cplat_pinned_prompt *screen)
{
    pinned_prompt_adjust_view(screen);
}

void test_pinned_prompt_layout(cplat_pinned_prompt *screen, int *prompt_row, int *prompt_separator_row,
                               int *main_bottom_row, int *show_top_status, int *show_bottom_status)
{
    pinned_prompt_layout layout;

    pinned_prompt_calc_layout(screen, &layout);
    *prompt_row = layout.prompt_row;
    *prompt_separator_row = layout.prompt_sep_row;
    *main_bottom_row = layout.main_bottom_row;
    *show_top_status = layout.show_top_status;
    *show_bottom_status = layout.show_bottom_status;
}

void test_pinned_prompt_clear_control_area(cplat_pinned_prompt *screen, int main_bottom_row)
{
    pinned_prompt_layout layout = {0};

    layout.main_bottom_row = main_bottom_row;
    pinned_prompt_clear_control_area(screen, &layout);
}

void test_pinned_prompt_hide(cplat_pinned_prompt *screen)
{
    pinned_prompt_hide_prompt_locked(screen);
}

void test_pinned_prompt_finish(cplat_pinned_prompt *screen)
{
    pinned_prompt_finish_prompt_locked(screen);
}

void test_pinned_prompt_cleanup_terminal(cplat_pinned_prompt *screen)
{
    pinned_prompt_cleanup_terminal_locked(screen);
}

int test_pinned_prompt_format(cplat_pinned_prompt *screen, const char *format, ...)
{
    va_list args;
    int result;

    va_start(args, format);
    result = pinned_prompt_format_prompt(screen, format, args);
    va_end(args);
    return result;
}

int test_pinned_prompt_set_status_content(cplat_pinned_prompt *screen, const char *content)
{
    return pinned_prompt_set_status_content(&screen->status_top_left, &screen->status_top_left_cap, content);
}

void test_pinned_prompt_history_null_inputs(cplat_pinned_prompt *screen)
{
    pinned_prompt_history_ctx context = {0};

    pinned_prompt_history_add(screen, NULL, "value");
    pinned_prompt_history_add(screen, &context, NULL);
    pinned_prompt_history_add(screen, &context, "");
    screen->history_max = 0U;
    pinned_prompt_history_add(screen, &context, "value");
    pinned_prompt_clear_edit_line(screen, &context);
    pinned_prompt_backspace(screen);
}

int test_pinned_prompt_history_fill(cplat_pinned_prompt *screen, size_t count)
{
    pinned_prompt_history_ctx *context;
    size_t i;

    context = pinned_prompt_find_or_create_history_ctx(screen, "fill.c", 1);
    if (context == NULL)
    {
        return -1;
    }
    for (i = 0U; i < count; i++)
    {
        char line[8];

        line[0] = (char)('a' + (int)(i % 26U));
        line[1] = '\0';
        pinned_prompt_history_add(screen, context, line);
    }
    pinned_prompt_history_prev(screen, context);
    pinned_prompt_history_prev(screen, context);
    pinned_prompt_history_next(screen, context);
    pinned_prompt_history_next(screen, context);
    return 0;
}

int test_pinned_prompt_history_count(const cplat_pinned_prompt *screen)
{
    return (int)screen->history_ctx_count;
}

int test_pinned_prompt_history_failure_state(cplat_pinned_prompt *screen, const char *file, int line)
{
    pinned_prompt_history_ctx *context;

    context = pinned_prompt_find_or_create_history_ctx(screen, file, line);
    if (context == NULL)
    {
        return -1;
    }
    return 0;
}

void test_pinned_prompt_history_null_entry_paths(cplat_pinned_prompt *screen)
{
    pinned_prompt_history_ctx *context;
    char *entry;
    size_t slot;

    context = pinned_prompt_find_or_create_history_ctx(screen, "null-entry.c", 1);
    if (context == NULL)
    {
        return;
    }
    pinned_prompt_history_add(screen, context, "entry");
    slot = PINNED_PROMPT_HIST_IDX(screen, context, context->count - 1U);
    entry = context->entries[slot];
    context->entries[slot] = NULL;
    pinned_prompt_history_prev(screen, context);
    context->browse_idx = 0;
    pinned_prompt_history_next(screen, context);
    context->entries[slot] = entry;
}

void test_pinned_prompt_render_status(cplat_pinned_prompt *screen, int row, const char *left, const char *right)
{
    pinned_prompt_render_status_line(screen, row, left, right);
}

void test_pinned_prompt_history_next_null(cplat_pinned_prompt *screen)
{
    pinned_prompt_history_next(screen, NULL);
}

void test_pinned_prompt_history_next_null_entry(cplat_pinned_prompt *screen)
{
    pinned_prompt_history_ctx *context;
    size_t slot;

    context = pinned_prompt_find_or_create_history_ctx(screen, "next-null.c", 1);
    if (context == NULL)
    {
        return;
    }
    pinned_prompt_history_add(screen, context, "a");
    pinned_prompt_history_add(screen, context, "b");
    slot = PINNED_PROMPT_HIST_IDX(screen, context, 1U);
    free(context->entries[slot]);
    context->entries[slot] = NULL;
    context->browse_idx = 0;
    pinned_prompt_history_next(screen, context);
}

void test_pinned_prompt_set_status_dirty(cplat_pinned_prompt *screen, int dirty)
{
    screen->status_dirty = dirty;
}

void test_pinned_prompt_history_add_after_null_last(cplat_pinned_prompt *screen)
{
    pinned_prompt_history_ctx *context;
    size_t slot;

    context = pinned_prompt_find_or_create_history_ctx(screen, "null-last.c", 1);
    if (context == NULL)
    {
        return;
    }
    pinned_prompt_history_add(screen, context, "first");
    slot = PINNED_PROMPT_HIST_IDX(screen, context, context->count - 1U);
    free(context->entries[slot]);
    context->entries[slot] = NULL;
    pinned_prompt_history_add(screen, context, "second");
}

void test_pinned_prompt_destroy_mutex(cplat_pinned_prompt *screen)
{
    if (screen->mutex_active)
    {
        (void)cplat_local_lock_dispose(screen->mutex);
        screen->mutex_active = 0;
    }
}

void test_pinned_prompt_history_release_entries(cplat_pinned_prompt *screen)
{
    pinned_prompt_history_ctx *context;
    size_t i;

    context = pinned_prompt_find_or_create_history_ctx(screen, "release.c", 1);
    if (context == NULL)
    {
        return;
    }
    for (i = 0U; i < screen->history_max; i++)
    {
        free(context->entries[i]);
    }
    free(context->entries);
    context->entries = NULL;
}

int test_pinned_prompt_raw_active(const cplat_pinned_prompt *screen)
{
    return screen->platform.raw_active;
}

void test_pinned_prompt_set_raw_active(cplat_pinned_prompt *screen, int active)
{
    screen->platform.raw_active = active;
}

void test_pinned_prompt_get_size(int *cols, int *rows)
{
    prompt_platform_get_size(cols, rows);
}

void test_pinned_prompt_enter_raw(cplat_pinned_prompt *screen)
{
    (void)prompt_platform_os_enter_raw(&screen->platform);
}

void test_pinned_prompt_leave_raw(cplat_pinned_prompt *screen)
{
    (void)prompt_platform_os_leave_raw(&screen->platform);
}

int test_pinned_prompt_read_char(cplat_pinned_prompt *screen)
{
    return prompt_platform_read_char(&screen->platform);
}

int test_pinned_prompt_read_char_nb(cplat_pinned_prompt *screen)
{
    return prompt_platform_read_char_nb(&screen->platform);
}

int test_pinned_prompt_platform_is_tty(void)
{
    return prompt_platform_is_tty();
}
