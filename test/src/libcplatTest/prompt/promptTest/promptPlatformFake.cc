// promptTest 専用のプラットフォーム層 fake
// prompt.c が呼ぶ prompt_platform_* を差し替え、実機の端末なしにキー入力を再現します
#include "../../../../../prod/libsrc/cplat/prompt/prompt_state.h"

#include <cplat/crt/unistd.h>
#include <cstddef>
#include <string>
#include <vector>

#include "promptPlatformFake.h"

namespace
{

std::vector<int> g_input;
size_t g_pos = 0u;
int g_enter_raw = 0;
int g_leave_raw = 0;
int g_enter_result = 0;
int g_leave_result = 0;
size_t g_columns = 80U;

int next_byte()
{
    if (g_pos >= g_input.size())
    {
        return -1;
    }

    return g_input[g_pos++];
}

} // namespace

void promptFakeSetInput(const std::string &bytes)
{
    g_input.clear();
    for (char byte : bytes)
    {
        g_input.push_back((int)(unsigned char)byte);
    }
    g_pos = 0u;
}

void promptFakeSetResults(const std::vector<int> &results)
{
    g_input = results;
    g_pos = 0u;
}

int promptFakeEnterRawCount(void)
{
    return g_enter_raw;
}

int promptFakeLeaveRawCount(void)
{
    return g_leave_raw;
}

void promptFakeReset(void)
{
    g_input.clear();
    g_pos = 0u;
    g_enter_raw = 0;
    g_leave_raw = 0;
    g_columns = 80U;
    g_enter_result = 0;
    g_leave_result = 0;
}

extern "C"
{
    int prompt_platform_is_tty(void)
    {
        return cplat_isatty(CPLAT_STREAM_STDIN) && cplat_isatty(CPLAT_STREAM_STDOUT);
    }
    void prompt_platform_get_size(int *cols, int *rows)
    {
        *cols = (int)g_columns;
        *rows = 24;
    }

    int prompt_platform_enter_raw(prompt_platform_context *p)
    {
        (void)p;
        g_enter_raw++;
        return g_enter_result;
    }

    int prompt_platform_leave_raw(prompt_platform_context *p)
    {
        (void)p;
        g_leave_raw++;
        return g_leave_result;
    }

    int prompt_platform_read_char(prompt_platform_context *p)
    {
        (void)p;
        return next_byte();
    }

    int prompt_platform_read_char_nb(prompt_platform_context *p)
    {
        (void)p;
        return next_byte();
    }
}

void promptFakeSetColumns(size_t columns)
{
    g_columns = columns;
}

void promptFakeSetFailures(int enter_result, int leave_result)
{
    g_enter_result = enter_result;
    g_leave_result = leave_result;
}
