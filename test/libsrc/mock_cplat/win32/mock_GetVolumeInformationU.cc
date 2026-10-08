#include <testfw.h>
#include <mock_cplat.h>

#if defined(PLATFORM_WINDOWS)

BOOL delegate_real_GetVolumeInformationU(const char *utf8_root_path, char *utf8_volume_name, DWORD volume_name_size,
                                         DWORD *serial_number, DWORD *max_component_length, DWORD *file_system_flags,
                                         char *utf8_file_system_name, DWORD file_system_name_size)
{
    static auto real_fn = reinterpret_cast<decltype(&GetVolumeInformationU)>(
        resolveSharedSymbolOrExit(kLibCplatName, "GetVolumeInformationU"));

    return real_fn(utf8_root_path, utf8_volume_name, volume_name_size, serial_number, max_component_length,
                   file_system_flags, utf8_file_system_name, file_system_name_size);
}

MOCK_WEAK_IMPL(BOOL, GetVolumeInformationU, const char *utf8_root_path, char *utf8_volume_name, DWORD volume_name_size,
               DWORD *serial_number, DWORD *max_component_length, DWORD *file_system_flags, char *utf8_file_system_name,
               DWORD file_system_name_size)
{
    BOOL mock_ret;

    if (_mock_cplat != nullptr)
    {
        mock_ret = _mock_cplat->GetVolumeInformationU(utf8_root_path, utf8_volume_name, volume_name_size, serial_number,
                                                      max_component_length, file_system_flags, utf8_file_system_name,
                                                      file_system_name_size);
    }
    else
    {
        mock_ret = delegate_real_GetVolumeInformationU(utf8_root_path, utf8_volume_name, volume_name_size,
                                                       serial_number, max_component_length, file_system_flags,
                                                       utf8_file_system_name, file_system_name_size);
    }

    if (getTraceLevel() > TRACE_NONE)
    {
        printf("  > %s %s, 0x%p, %lu, 0x%p, 0x%p, 0x%p, 0x%p, %lu", __func__,
               (utf8_root_path != nullptr) ? utf8_root_path : "(null)", (void *)utf8_volume_name, volume_name_size,
               (void *)serial_number, (void *)max_component_length, (void *)file_system_flags,
               (void *)utf8_file_system_name, file_system_name_size);
        if (getTraceLevel() >= TRACE_DETAIL)
        {
            printf(" -> %d\n", mock_ret);
        }
        else
        {
            printf("\n");
        }
    }

    return mock_ret;
}

#endif /* PLATFORM_WINDOWS */
