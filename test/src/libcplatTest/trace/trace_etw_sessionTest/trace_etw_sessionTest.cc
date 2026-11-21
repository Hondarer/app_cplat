#include <cplat/base/platform.h>

#if defined(PLATFORM_WINDOWS)

    #include <cplat/base/windows_sdk.h>
    #include <testfw.h>
    #include <mock_cplat.h>

    #include <cstdint>
    #include <cstring>
    #include <vector>

    #include "trace_etw_session.inject.h"

namespace
{
struct property_spec
{
    const wchar_t *name;
    USHORT flags;
    USHORT in_type;
    USHORT length;
    USHORT count;
    USHORT count_property_index;
};

std::vector<unsigned char> build_trace_event_info(const std::vector<property_spec> &specs)
{
    const std::size_t prop_count = specs.size();
    const std::size_t header_size =
        sizeof(TRACE_EVENT_INFO) + (prop_count > 1U ? (prop_count - 1U) * sizeof(EVENT_PROPERTY_INFO) : 0U);
    std::size_t total_size = header_size;

    for (const property_spec &spec : specs)
    {
        if (spec.name != nullptr)
        {
            total_size += (std::wcslen(spec.name) + 1U) * sizeof(wchar_t);
        }
    }

    std::vector<unsigned char> buffer(total_size, 0U);
    TRACE_EVENT_INFO *info = reinterpret_cast<TRACE_EVENT_INFO *>(buffer.data());
    std::size_t name_cursor = header_size;

    info->TopLevelPropertyCount = static_cast<ULONG>(prop_count);
    for (std::size_t index = 0U; index < prop_count; index++)
    {
        EVENT_PROPERTY_INFO *prop = &info->EventPropertyInfoArray[index];
        const property_spec &spec = specs[index];

        prop->Flags = static_cast<PROPERTY_FLAGS>(spec.flags);
        if ((spec.flags & PropertyStruct) == 0U)
        {
            prop->nonStructType.InType = spec.in_type;
        }
        prop->length = spec.length;
        prop->count = spec.count;
        if ((spec.flags & PropertyParamCount) != 0U)
        {
            prop->countPropertyIndex = spec.count_property_index;
        }
        if (spec.name != nullptr)
        {
            const std::size_t name_bytes = (std::wcslen(spec.name) + 1U) * sizeof(wchar_t);

            std::memcpy(buffer.data() + name_cursor, spec.name, name_bytes);
            prop->NameOffset = static_cast<ULONG>(name_cursor);
            name_cursor += name_bytes;
        }
    }

    return buffer;
}
} // namespace

class trace_etw_sessionTest : public Test
{
};

// 文字列 2 個と整数 1 個のイベントで、Service、Message、ProcessId が得られることの確認
TEST_F(trace_etw_sessionTest, extract_reads_service_message_and_process_id)
{
    // Arrange
    const std::vector<property_spec> specs = {
        {L"Service", 0U, TDH_INTYPE_ANSISTRING, 0U, 1U, 0U},
        {L"Message", 0U, TDH_INTYPE_ANSISTRING, 0U, 1U, 0U},
        {L"ProcessId", 0U, TDH_INTYPE_UINT32, 0U, 1U, 0U},
    };
    const std::vector<unsigned char> info_buf = build_trace_event_info(specs);
    const TRACE_EVENT_INFO *info = reinterpret_cast<const TRACE_EVENT_INFO *>(info_buf.data());
    const unsigned char user_data[] = {
        's', 'v', 'c', '\0',
        'h', 'e', 'l', 'l', 'o', '\0',
        0xD2U, 0x04U, 0x00U, 0x00U,
    };
    EVENT_RECORD event_record = {};
    const char *service = nullptr;
    const char *message = nullptr;
    uint32_t process_id = 0U;

    event_record.UserData = const_cast<unsigned char *>(user_data);
    event_record.UserDataLength = static_cast<USHORT>(sizeof(user_data));

    // Pre-Assert

    // Act
    test_trace_etw_session_extract_event_fields(
        &event_record, info, &service, &message,
        &process_id); // [手順] - 文字列 2 個と整数 1 個のイベントを指定して test_trace_etw_session_extract_event_fields() を呼び出す。

    // Assert
    ASSERT_NE(nullptr, service); // [確認_正常系] - service が nullptr 以外であること。
    EXPECT_STREQ("svc", service); // [確認_正常系] - service が "svc" と一致すること。
    ASSERT_NE(nullptr, message); // [確認_正常系] - message が nullptr 以外であること。
    EXPECT_STREQ("hello", message); // [確認_正常系] - message が "hello" と一致すること。
    EXPECT_EQ(1234U, process_id); // [確認_正常系] - process_id が 1234U と一致すること。
}

// 構造体のプロパティに出会ったときに読み取りを打ち切り、後続の Message を読まないことの確認
TEST_F(trace_etw_sessionTest, extract_stops_at_struct_property)
{
    // Arrange
    const std::vector<property_spec> specs = {
        {L"Service", 0U, TDH_INTYPE_ANSISTRING, 0U, 1U, 0U},
        {L"Nested", PropertyStruct, 0U, 0U, 1U, 0U},
        {L"Message", 0U, TDH_INTYPE_ANSISTRING, 0U, 1U, 0U},
    };
    const std::vector<unsigned char> info_buf = build_trace_event_info(specs);
    const TRACE_EVENT_INFO *info = reinterpret_cast<const TRACE_EVENT_INFO *>(info_buf.data());
    const unsigned char user_data[] = {'s', 'v', 'c', '\0', 'x', '\0', 'm', 's', 'g', '\0'};
    EVENT_RECORD event_record = {};
    const char *service = nullptr;
    const char *message = nullptr;
    uint32_t process_id = 0U;

    event_record.UserData = const_cast<unsigned char *>(user_data);
    event_record.UserDataLength = static_cast<USHORT>(sizeof(user_data));

    // Pre-Assert

    // Act
    test_trace_etw_session_extract_event_fields(
        &event_record, info, &service, &message,
        &process_id); // [手順] - 構造体のプロパティを含むイベントを指定して test_trace_etw_session_extract_event_fields() を呼び出す。

    // Assert
    ASSERT_NE(nullptr, service); // [確認_正常系] - service が nullptr 以外であること。
    EXPECT_STREQ("svc", service); // [確認_正常系] - service が "svc" と一致すること。
    EXPECT_EQ(nullptr, message); // [確認_正常系] - 構造体の後ろの message が nullptr であること。
}

// 固定の要素数の配列プロパティに出会ったときに読み取りを打ち切り、後続を読まないことの確認
TEST_F(trace_etw_sessionTest, extract_stops_at_fixed_count_array_property)
{
    // Arrange
    const std::vector<property_spec> specs = {
        {L"Service", 0U, TDH_INTYPE_ANSISTRING, 0U, 1U, 0U},
        {L"Items", PropertyParamFixedCount, TDH_INTYPE_ANSISTRING, 0U, 2U, 0U},
        {L"Message", 0U, TDH_INTYPE_ANSISTRING, 0U, 1U, 0U},
    };
    const std::vector<unsigned char> info_buf = build_trace_event_info(specs);
    const TRACE_EVENT_INFO *info = reinterpret_cast<const TRACE_EVENT_INFO *>(info_buf.data());
    const unsigned char user_data[] = {'s', 'v', 'c', '\0', 'a', '\0', 'b', '\0', 'm', 's', 'g', '\0'};
    EVENT_RECORD event_record = {};
    const char *service = nullptr;
    const char *message = nullptr;
    uint32_t process_id = 0U;

    event_record.UserData = const_cast<unsigned char *>(user_data);
    event_record.UserDataLength = static_cast<USHORT>(sizeof(user_data));

    // Pre-Assert

    // Act
    test_trace_etw_session_extract_event_fields(
        &event_record, info, &service, &message,
        &process_id); // [手順] - 固定の要素数の配列プロパティを含むイベントを指定して test_trace_etw_session_extract_event_fields() を呼び出す。

    // Assert
    ASSERT_NE(nullptr, service); // [確認_正常系] - service が nullptr 以外であること。
    EXPECT_STREQ("svc", service); // [確認_正常系] - service が "svc" と一致すること。
    EXPECT_EQ(nullptr, message); // [確認_正常系] - 固定長配列の後ろの message が nullptr であること。
}

// 要素数を別のプロパティに持つ配列プロパティに出会ったときに読み取りを打ち切り、後続を読まないことの確認
TEST_F(trace_etw_sessionTest, extract_stops_at_variable_count_array_property)
{
    // Arrange
    const std::vector<property_spec> specs = {
        {L"Service", 0U, TDH_INTYPE_ANSISTRING, 0U, 1U, 0U},
        {L"Count", 0U, TDH_INTYPE_UINT32, 0U, 1U, 0U},
        {L"Items", PropertyParamCount, TDH_INTYPE_ANSISTRING, 0U, 1U, 1U},
        {L"Message", 0U, TDH_INTYPE_ANSISTRING, 0U, 1U, 0U},
    };
    const std::vector<unsigned char> info_buf = build_trace_event_info(specs);
    const TRACE_EVENT_INFO *info = reinterpret_cast<const TRACE_EVENT_INFO *>(info_buf.data());
    const unsigned char user_data[] = {
        's', 'v', 'c', '\0',
        0x02U, 0x00U, 0x00U, 0x00U,
        'a', '\0', 'b', '\0',
        'm', 's', 'g', '\0',
    };
    EVENT_RECORD event_record = {};
    const char *service = nullptr;
    const char *message = nullptr;
    uint32_t process_id = 0U;

    event_record.UserData = const_cast<unsigned char *>(user_data);
    event_record.UserDataLength = static_cast<USHORT>(sizeof(user_data));

    // Pre-Assert

    // Act
    test_trace_etw_session_extract_event_fields(
        &event_record, info, &service, &message,
        &process_id); // [手順] - 可変長配列プロパティを含むイベントを指定して test_trace_etw_session_extract_event_fields() を呼び出す。

    // Assert
    ASSERT_NE(nullptr, service); // [確認_正常系] - service が nullptr 以外であること。
    EXPECT_STREQ("svc", service); // [確認_正常系] - service が "svc" と一致すること。
    EXPECT_EQ(nullptr, message); // [確認_正常系] - 可変長配列の後ろの message が nullptr であること。
}

// 名前なしプロパティに出会ったときに読み取りを打ち切り、後続の Message を読まないことの確認
TEST_F(trace_etw_sessionTest, extract_stops_at_unnamed_property)
{
    // Arrange
    const std::vector<property_spec> specs = {
        {L"Service", 0U, TDH_INTYPE_ANSISTRING, 0U, 1U, 0U},
        {nullptr, 0U, TDH_INTYPE_ANSISTRING, 0U, 1U, 0U},
        {L"Message", 0U, TDH_INTYPE_ANSISTRING, 0U, 1U, 0U},
    };
    const std::vector<unsigned char> info_buf = build_trace_event_info(specs);
    const TRACE_EVENT_INFO *info = reinterpret_cast<const TRACE_EVENT_INFO *>(info_buf.data());
    const unsigned char user_data[] = {'s', 'v', 'c', '\0', 's', 'k', 'i', 'p', '\0', 'm', 's', 'g', '\0'};
    EVENT_RECORD event_record = {};
    const char *service = nullptr;
    const char *message = nullptr;
    uint32_t process_id = 0U;

    event_record.UserData = const_cast<unsigned char *>(user_data);
    event_record.UserDataLength = static_cast<USHORT>(sizeof(user_data));

    // Pre-Assert

    // Act
    test_trace_etw_session_extract_event_fields(
        &event_record, info, &service, &message,
        &process_id); // [手順] - 名前なしプロパティを含むイベントを指定して test_trace_etw_session_extract_event_fields() を呼び出す。

    // Assert
    ASSERT_NE(nullptr, service); // [確認_正常系] - service が nullptr 以外であること。
    EXPECT_STREQ("svc", service); // [確認_正常系] - service が "svc" と一致すること。
    EXPECT_EQ(nullptr, message); // [確認_正常系] - 名前なしプロパティの後ろの message が nullptr であること。
}

// 長さ指定プロパティを持つ文字列に出会ったときに読み取りを打ち切り、Service を読まないことの確認
TEST_F(trace_etw_sessionTest, extract_stops_at_string_with_length_property)
{
    // Arrange
    const std::vector<property_spec> specs = {
        {L"Len", 0U, TDH_INTYPE_UINT32, 0U, 1U, 0U},
        {L"Service", PropertyParamLength, TDH_INTYPE_ANSISTRING, 0U, 1U, 0U},
    };
    const std::vector<unsigned char> info_buf = build_trace_event_info(specs);
    const TRACE_EVENT_INFO *info = reinterpret_cast<const TRACE_EVENT_INFO *>(info_buf.data());
    const unsigned char user_data[] = {
        0x03U, 0x00U, 0x00U, 0x00U,
        'a', 'b', 'c',
        'm', 's', 'g', '\0',
    };
    EVENT_RECORD event_record = {};
    const char *service = nullptr;
    const char *message = nullptr;
    uint32_t process_id = 0U;

    event_record.UserData = const_cast<unsigned char *>(user_data);
    event_record.UserDataLength = static_cast<USHORT>(sizeof(user_data));

    // Pre-Assert

    // Act
    test_trace_etw_session_extract_event_fields(
        &event_record, info, &service, &message,
        &process_id); // [手順] - PropertyParamLength が立つ Service を含むイベントを指定して test_trace_etw_session_extract_event_fields() を呼び出す。

    // Assert
    EXPECT_EQ(nullptr, service); // [確認_正常系] - service が nullptr であること。
}

// 固定長の文字列プロパティに出会ったときに読み取りを打ち切り、Service を読まないことの確認
TEST_F(trace_etw_sessionTest, extract_stops_at_fixed_length_string)
{
    // Arrange
    const std::vector<property_spec> specs = {
        {L"Service", PropertyParamFixedLength, TDH_INTYPE_ANSISTRING, 3U, 1U, 0U},
    };
    const std::vector<unsigned char> info_buf = build_trace_event_info(specs);
    const TRACE_EVENT_INFO *info = reinterpret_cast<const TRACE_EVENT_INFO *>(info_buf.data());
    const unsigned char user_data[] = {'a', 'b', 'c'};
    EVENT_RECORD event_record = {};
    const char *service = nullptr;
    const char *message = nullptr;
    uint32_t process_id = 0U;

    event_record.UserData = const_cast<unsigned char *>(user_data);
    event_record.UserDataLength = static_cast<USHORT>(sizeof(user_data));

    // Pre-Assert

    // Act
    test_trace_etw_session_extract_event_fields(
        &event_record, info, &service, &message,
        &process_id); // [手順] - PropertyParamFixedLength が立ち length が 3 の Service を指定して test_trace_etw_session_extract_event_fields() を呼び出す。

    // Assert
    EXPECT_EQ(nullptr, service); // [確認_正常系] - service が nullptr であること。
}

#endif /* PLATFORM_WINDOWS */
