#include <testfw.h>
#include <mock_cplat.h>

#define DEFINE_ARGPARSER_RESULT(name, call_args, ...) \
    int delegate_real_##name(__VA_ARGS__) \
    { \
        static auto real_fn = reinterpret_cast<decltype(&name)>(resolveSharedSymbolOrExit(kLibCplatName, #name)); \
        return real_fn call_args; \
    } \
    MOCK_WEAK_IMPL(int, name, __VA_ARGS__) \
    { \
        int mock_ret; \
        if (_mock_cplat != nullptr) \
        { \
            mock_ret = _mock_cplat->name call_args; \
        } \
        else \
        { \
            mock_ret = delegate_real_##name call_args; \
        } \
        if (getTraceLevel() > TRACE_NONE) \
        { \
            printf("  > %s -> %d\n", __func__, (int)mock_ret); \
        } \
        return mock_ret; \
    }

#define DEFINE_ARGPARSER_VOID(name, call_args, ...) \
    void delegate_real_##name(__VA_ARGS__) \
    { \
        static auto real_fn = reinterpret_cast<decltype(&name)>(resolveSharedSymbolOrExit(kLibCplatName, #name)); \
        real_fn call_args; \
    } \
    MOCK_WEAK_IMPL(void, name, __VA_ARGS__) \
    { \
        if (_mock_cplat != nullptr) \
        { \
            _mock_cplat->name call_args; \
        } \
        else \
        { \
            delegate_real_##name call_args; \
        } \
        if (getTraceLevel() > TRACE_NONE) \
        { \
            printf("  > %s\n", __func__); \
        } \
    }

DEFINE_ARGPARSER_RESULT(cplat_argparser_handle_register_flag, (parser, short_name, long_name, description, storage),
                        cplat_argparser *parser, const char *short_name, const char *long_name, const char *description,
                        int *storage)
DEFINE_ARGPARSER_RESULT(cplat_argparser_handle_register_option_int,
                        (parser, short_name, long_name, value_name, description, flags, storage),
                        cplat_argparser *parser, const char *short_name, const char *long_name, const char *value_name,
                        const char *description, unsigned int flags, int *storage)
DEFINE_ARGPARSER_RESULT(cplat_argparser_handle_register_option_string,
                        (parser, short_name, long_name, value_name, description, flags, storage),
                        cplat_argparser *parser, const char *short_name, const char *long_name, const char *value_name,
                        const char *description, unsigned int flags, const char **storage)
DEFINE_ARGPARSER_RESULT(cplat_argparser_handle_register_option_int_array,
                        (parser, short_name, long_name, value_name, description, flags, storage, capacity, count),
                        cplat_argparser *parser, const char *short_name, const char *long_name, const char *value_name,
                        const char *description, unsigned int flags, int *storage, size_t capacity, size_t *count)
DEFINE_ARGPARSER_RESULT(cplat_argparser_handle_register_option_string_array,
                        (parser, short_name, long_name, value_name, description, flags, storage, capacity, count),
                        cplat_argparser *parser, const char *short_name, const char *long_name, const char *value_name,
                        const char *description, unsigned int flags, const char **storage, size_t capacity,
                        size_t *count)
DEFINE_ARGPARSER_RESULT(cplat_argparser_handle_register_positional_int, (parser, name, description, flags, storage),
                        cplat_argparser *parser, const char *name, const char *description, unsigned int flags,
                        int *storage)
DEFINE_ARGPARSER_RESULT(cplat_argparser_handle_register_positional_string, (parser, name, description, flags, storage),
                        cplat_argparser *parser, const char *name, const char *description, unsigned int flags,
                        const char **storage)
DEFINE_ARGPARSER_RESULT(cplat_argparser_handle_register_positional_int_array,
                        (parser, name, description, flags, storage, capacity, count), cplat_argparser *parser,
                        const char *name, const char *description, unsigned int flags, int *storage, size_t capacity,
                        size_t *count)
DEFINE_ARGPARSER_RESULT(cplat_argparser_handle_register_positional_string_array,
                        (parser, name, description, flags, storage, capacity, count), cplat_argparser *parser,
                        const char *name, const char *description, unsigned int flags, const char **storage,
                        size_t capacity, size_t *count)
DEFINE_ARGPARSER_RESULT(cplat_argparser_handle_parse, (parser), cplat_argparser *parser)
DEFINE_ARGPARSER_RESULT(cplat_argparser_handle_get_error_message, (parser, buffer, buffer_size),
                        const cplat_argparser *parser, char *buffer, size_t buffer_size)
DEFINE_ARGPARSER_RESULT(cplat_argparser_handle_get_usage, (parser, buffer, buffer_size, required_size),
                        const cplat_argparser *parser, char *buffer, size_t buffer_size, size_t *required_size)
DEFINE_ARGPARSER_RESULT(cplat_argparser_handle_print_usage, (parser, stream), const cplat_argparser *parser,
                        FILE *stream)
DEFINE_ARGPARSER_RESULT(cplat_argparser_handle_print_error_messages, (parser, stream), const cplat_argparser *parser,
                        FILE *stream)
DEFINE_ARGPARSER_RESULT(cplat_argparser_handle_get_register_error, (parser, index), const cplat_argparser *parser,
                        size_t index)
DEFINE_ARGPARSER_RESULT(cplat_argparser_handle_get_register_error_message, (parser, index, buffer, buffer_size),
                        const cplat_argparser *parser, size_t index, char *buffer, size_t buffer_size)
DEFINE_ARGPARSER_RESULT(cplat_argparser_handle_print_register_error_messages, (parser, stream),
                        const cplat_argparser *parser, FILE *stream)

DEFINE_ARGPARSER_VOID(cplat_argparser_handle_dispose, (parser), cplat_argparser *parser)

cplat_argparser *delegate_real_cplat_argparser_handle_create(int argc, char *const *argv,
                                                             const cplat_argparser_options *options)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_argparser_handle_create)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_argparser_handle_create"));

    return real_fn(argc, argv, options);
}

MOCK_WEAK_IMPL(cplat_argparser *, cplat_argparser_handle_create, int argc, char *const *argv,
               const cplat_argparser_options *options)
{
    cplat_argparser *mock_ret = nullptr;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_argparser_handle_create(argc, argv, options);
    }
    else
    {
        mock_ret = delegate_real_cplat_argparser_handle_create(argc, argv, options);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s 0x%p", __func__, (const void *)options);
        if (getTraceLevel() >= TRACE_DETAIL)
        {
            printf(" -> 0x%p\n", (void *)mock_ret);
        }
        else
        {
            printf("\n");
        }
    }

    return mock_ret;
}

int delegate_real_cplat_argparser_handle_get_error(const cplat_argparser *parser)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_argparser_handle_get_error)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_argparser_handle_get_error"));

    return real_fn(parser);
}

MOCK_WEAK_IMPL(int, cplat_argparser_handle_get_error, const cplat_argparser *parser)
{
    int mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_argparser_handle_get_error(parser);
    }
    else
    {
        mock_ret = delegate_real_cplat_argparser_handle_get_error(parser);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s -> %d\n", __func__, (int)mock_ret);
    }

    return mock_ret;
}

const char *delegate_real_cplat_argparser_handle_get_error_target(const cplat_argparser *parser)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_argparser_handle_get_error_target)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_argparser_handle_get_error_target"));

    return real_fn(parser);
}

MOCK_WEAK_IMPL(const char *, cplat_argparser_handle_get_error_target, const cplat_argparser *parser)
{
    const char *mock_ret = nullptr;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_argparser_handle_get_error_target(parser);
    }
    else
    {
        mock_ret = delegate_real_cplat_argparser_handle_get_error_target(parser);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        const char *trace_result = mock_ret;
        if (trace_result == nullptr)
        {
            trace_result = "(null)";
        }
        printf("  > %s -> %s\n", __func__, trace_result);
    }

    return mock_ret;
}

int delegate_real_cplat_argparser_handle_get_error_index(const cplat_argparser *parser)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_argparser_handle_get_error_index)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_argparser_handle_get_error_index"));

    return real_fn(parser);
}

MOCK_WEAK_IMPL(int, cplat_argparser_handle_get_error_index, const cplat_argparser *parser)
{
    int mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_argparser_handle_get_error_index(parser);
    }
    else
    {
        mock_ret = delegate_real_cplat_argparser_handle_get_error_index(parser);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s -> %d\n", __func__, mock_ret);
    }

    return mock_ret;
}

size_t delegate_real_cplat_argparser_handle_get_register_error_count(const cplat_argparser *parser)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_argparser_handle_get_register_error_count)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_argparser_handle_get_register_error_count"));

    return real_fn(parser);
}

MOCK_WEAK_IMPL(size_t, cplat_argparser_handle_get_register_error_count, const cplat_argparser *parser)
{
    size_t mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_argparser_handle_get_register_error_count(parser);
    }
    else
    {
        mock_ret = delegate_real_cplat_argparser_handle_get_register_error_count(parser);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s -> %zu\n", __func__, mock_ret);
    }

    return mock_ret;
}

const char *delegate_real_cplat_argparser_handle_get_register_error_target(const cplat_argparser *parser, size_t index)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_argparser_handle_get_register_error_target)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_argparser_handle_get_register_error_target"));

    return real_fn(parser, index);
}

MOCK_WEAK_IMPL(const char *, cplat_argparser_handle_get_register_error_target, const cplat_argparser *parser,
               size_t index)
{
    const char *mock_ret = nullptr;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_argparser_handle_get_register_error_target(parser, index);
    }
    else
    {
        mock_ret = delegate_real_cplat_argparser_handle_get_register_error_target(parser, index);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        const char *trace_result = mock_ret;
        if (trace_result == nullptr)
        {
            trace_result = "(null)";
        }
        printf("  > %s -> %s\n", __func__, trace_result);
    }

    return mock_ret;
}

/* ================================================================
 * 省略可能な単一インスタンス API (parser 引数なし) のモック
 * ================================================================ */

DEFINE_ARGPARSER_VOID(cplat_argparser_init, (argc, argv, description), int argc, char *const *argv,
                      const char *description)

DEFINE_ARGPARSER_RESULT(cplat_argparser_register_flag, (short_name, long_name, description, storage),
                        const char *short_name, const char *long_name, const char *description, int *storage)
DEFINE_ARGPARSER_RESULT(cplat_argparser_register_option_int,
                        (short_name, long_name, value_name, description, flags, storage), const char *short_name,
                        const char *long_name, const char *value_name, const char *description, unsigned int flags,
                        int *storage)
DEFINE_ARGPARSER_RESULT(cplat_argparser_register_option_string,
                        (short_name, long_name, value_name, description, flags, storage), const char *short_name,
                        const char *long_name, const char *value_name, const char *description, unsigned int flags,
                        const char **storage)
DEFINE_ARGPARSER_RESULT(cplat_argparser_register_option_int_array,
                        (short_name, long_name, value_name, description, flags, storage, capacity, count),
                        const char *short_name, const char *long_name, const char *value_name, const char *description,
                        unsigned int flags, int *storage, size_t capacity, size_t *count)
DEFINE_ARGPARSER_RESULT(cplat_argparser_register_option_string_array,
                        (short_name, long_name, value_name, description, flags, storage, capacity, count),
                        const char *short_name, const char *long_name, const char *value_name, const char *description,
                        unsigned int flags, const char **storage, size_t capacity, size_t *count)
DEFINE_ARGPARSER_RESULT(cplat_argparser_register_positional_int, (name, description, flags, storage), const char *name,
                        const char *description, unsigned int flags, int *storage)
DEFINE_ARGPARSER_RESULT(cplat_argparser_register_positional_string, (name, description, flags, storage),
                        const char *name, const char *description, unsigned int flags, const char **storage)
DEFINE_ARGPARSER_RESULT(cplat_argparser_register_positional_int_array,
                        (name, description, flags, storage, capacity, count), const char *name, const char *description,
                        unsigned int flags, int *storage, size_t capacity, size_t *count)
DEFINE_ARGPARSER_RESULT(cplat_argparser_register_positional_string_array,
                        (name, description, flags, storage, capacity, count), const char *name, const char *description,
                        unsigned int flags, const char **storage, size_t capacity, size_t *count)
DEFINE_ARGPARSER_RESULT(cplat_argparser_get_error_message, (buffer, buffer_size), char *buffer, size_t buffer_size)
DEFINE_ARGPARSER_RESULT(cplat_argparser_get_usage, (buffer, buffer_size, required_size), char *buffer,
                        size_t buffer_size, size_t *required_size)
DEFINE_ARGPARSER_RESULT(cplat_argparser_print_usage, (stream), FILE *stream)
DEFINE_ARGPARSER_RESULT(cplat_argparser_print_error_messages, (stream), FILE *stream)
DEFINE_ARGPARSER_RESULT(cplat_argparser_get_register_error, (index), size_t index)
DEFINE_ARGPARSER_RESULT(cplat_argparser_get_register_error_message, (index, buffer, buffer_size), size_t index,
                        char *buffer, size_t buffer_size)
DEFINE_ARGPARSER_RESULT(cplat_argparser_print_register_error_messages, (stream), FILE *stream)

int delegate_real_cplat_argparser_parse(void)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_argparser_parse)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_argparser_parse"));

    return real_fn();
}

MOCK_WEAK_IMPL(int, cplat_argparser_parse, void)
{
    int mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_argparser_parse();
    }
    else
    {
        mock_ret = delegate_real_cplat_argparser_parse();
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s -> %d\n", __func__, (int)mock_ret);
    }

    return mock_ret;
}

int delegate_real_cplat_argparser_get_error(void)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_argparser_get_error)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_argparser_get_error"));

    return real_fn();
}

MOCK_WEAK_IMPL(int, cplat_argparser_get_error, void)
{
    int mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_argparser_get_error();
    }
    else
    {
        mock_ret = delegate_real_cplat_argparser_get_error();
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s -> %d\n", __func__, (int)mock_ret);
    }

    return mock_ret;
}

const char *delegate_real_cplat_argparser_get_error_target(void)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_argparser_get_error_target)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_argparser_get_error_target"));

    return real_fn();
}

MOCK_WEAK_IMPL(const char *, cplat_argparser_get_error_target, void)
{
    const char *mock_ret = nullptr;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_argparser_get_error_target();
    }
    else
    {
        mock_ret = delegate_real_cplat_argparser_get_error_target();
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        const char *trace_result = mock_ret;
        if (trace_result == nullptr)
        {
            trace_result = "(null)";
        }
        printf("  > %s -> %s\n", __func__, trace_result);
    }

    return mock_ret;
}

int delegate_real_cplat_argparser_get_error_index(void)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_argparser_get_error_index)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_argparser_get_error_index"));

    return real_fn();
}

MOCK_WEAK_IMPL(int, cplat_argparser_get_error_index, void)
{
    int mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_argparser_get_error_index();
    }
    else
    {
        mock_ret = delegate_real_cplat_argparser_get_error_index();
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s -> %d\n", __func__, mock_ret);
    }

    return mock_ret;
}

size_t delegate_real_cplat_argparser_get_register_error_count(void)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_argparser_get_register_error_count)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_argparser_get_register_error_count"));

    return real_fn();
}

MOCK_WEAK_IMPL(size_t, cplat_argparser_get_register_error_count, void)
{
    size_t mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_argparser_get_register_error_count();
    }
    else
    {
        mock_ret = delegate_real_cplat_argparser_get_register_error_count();
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s -> %zu\n", __func__, mock_ret);
    }

    return mock_ret;
}

const char *delegate_real_cplat_argparser_get_register_error_target(size_t index)
{
    static auto real_fn = reinterpret_cast<decltype(&cplat_argparser_get_register_error_target)>(
        resolveSharedSymbolOrExit(kLibCplatName, "cplat_argparser_get_register_error_target"));

    return real_fn(index);
}

MOCK_WEAK_IMPL(const char *, cplat_argparser_get_register_error_target, size_t index)
{
    const char *mock_ret = nullptr;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->cplat_argparser_get_register_error_target(index);
    }
    else
    {
        mock_ret = delegate_real_cplat_argparser_get_register_error_target(index);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        const char *trace_result = mock_ret;
        if (trace_result == nullptr)
        {
            trace_result = "(null)";
        }
        printf("  > %s -> %s\n", __func__, trace_result);
    }

    return mock_ret;
}
