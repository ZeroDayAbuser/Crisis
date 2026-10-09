#pragma once

namespace module {
    using f_load_library_a        = HINSTANCE( WINAPI* )( LPCSTR );
    using f_get_proc_address      = FARPROC( WINAPI* )( HMODULE, LPCSTR );
    using f_rtl_add_function_table = BOOLEAN( WINAPI* )( PRUNTIME_FUNCTION, DWORD, DWORD64 );
    using f_dll_entry_point       = BOOL( WINAPI* )( void*, DWORD, void* );

    struct manual_mapping_data_t {
        f_load_library_a         m_load_library_a;
        f_get_proc_address       m_get_proc_address;
        f_rtl_add_function_table m_rtl_add_function_table;
        BYTE*                    m_base;
        HINSTANCE                m_module;
        DWORD                    m_reason;
        LPVOID                   m_reserved;
        bool                     m_seh_support;
    };

#ifdef _WIN64
#define MODULE_RELOC_FLAG(RelInfo) ( ( ( RelInfo ) >> 0x0C ) == IMAGE_REL_BASED_DIR64 )
#else
#define MODULE_RELOC_FLAG(RelInfo) ( ( ( RelInfo ) >> 0x0C ) == IMAGE_REL_BASED_HIGHLOW )
#endif

#pragma runtime_checks( "", off )
#pragma optimize( "", off )
    void __stdcall manual_mapping_shellcode( manual_mapping_data_t* data ) {
        if ( !data )
            return;

        auto base = data->m_base;
        auto opt  = &reinterpret_cast< IMAGE_NT_HEADERS* >(
            base + reinterpret_cast< IMAGE_DOS_HEADER* >( base )->e_lfanew )->OptionalHeader;

        auto load_library_a   = data->m_load_library_a;
        auto get_proc_address = data->m_get_proc_address;
#ifdef _WIN64
        auto rtl_add_function_table = data->m_rtl_add_function_table;
#endif
        auto dll_main = reinterpret_cast< f_dll_entry_point >( base + opt->AddressOfEntryPoint );

        auto location_delta = reinterpret_cast< UINT_PTR >( base ) - opt->ImageBase;
        if ( location_delta && opt->DataDirectory[ IMAGE_DIRECTORY_ENTRY_BASERELOC ].Size ) {
            auto reloc_data = reinterpret_cast< IMAGE_BASE_RELOCATION* >(
                base + opt->DataDirectory[ IMAGE_DIRECTORY_ENTRY_BASERELOC ].VirtualAddress );
            const auto reloc_end = reinterpret_cast< IMAGE_BASE_RELOCATION* >(
                reinterpret_cast< std::uintptr_t >( reloc_data ) +
                opt->DataDirectory[ IMAGE_DIRECTORY_ENTRY_BASERELOC ].Size );

            while ( reloc_data < reloc_end && reloc_data->SizeOfBlock ) {
                auto entry_count  = ( reloc_data->SizeOfBlock - sizeof( IMAGE_BASE_RELOCATION ) ) / sizeof( WORD );
                auto relative_info = reinterpret_cast< WORD* >( reloc_data + 1 );

                for ( UINT idx = 0; idx != entry_count; ++idx, ++relative_info ) {
                    if ( MODULE_RELOC_FLAG( *relative_info ) ) {
                        auto patch = reinterpret_cast< UINT_PTR* >(
                            base + reloc_data->VirtualAddress + ( ( *relative_info ) & 0xFFF ) );
                        *patch += location_delta;
                    }
                }

                reloc_data = reinterpret_cast< IMAGE_BASE_RELOCATION* >(
                    reinterpret_cast< BYTE* >( reloc_data ) + reloc_data->SizeOfBlock );
            }
        }

        if ( opt->DataDirectory[ IMAGE_DIRECTORY_ENTRY_IMPORT ].Size ) {
            auto import_desc = reinterpret_cast< IMAGE_IMPORT_DESCRIPTOR* >(
                base + opt->DataDirectory[ IMAGE_DIRECTORY_ENTRY_IMPORT ].VirtualAddress );

            while ( import_desc->Name ) {
                auto module_name = reinterpret_cast< char* >( base + import_desc->Name );
                auto module      = load_library_a( module_name );

                auto thunk_ref = reinterpret_cast< ULONG_PTR* >( base + import_desc->OriginalFirstThunk );
                auto func_ref  = reinterpret_cast< ULONG_PTR* >( base + import_desc->FirstThunk );

                if ( !thunk_ref )
                    thunk_ref = func_ref;

                for ( ; *thunk_ref; ++thunk_ref, ++func_ref ) {
                    if ( IMAGE_SNAP_BY_ORDINAL( *thunk_ref ) ) {
                        *func_ref = reinterpret_cast< ULONG_PTR >(
                            get_proc_address( module, reinterpret_cast< char* >( *thunk_ref & 0xFFFF ) ) );
                    }
                    else {
                        auto import = reinterpret_cast< IMAGE_IMPORT_BY_NAME* >( base + ( *thunk_ref ) );
                        *func_ref   = reinterpret_cast< ULONG_PTR >( get_proc_address( module, import->Name ) );
                    }
                }

                ++import_desc;
            }
        }

        if ( opt->DataDirectory[ IMAGE_DIRECTORY_ENTRY_TLS ].Size ) {
            auto tls      = reinterpret_cast< IMAGE_TLS_DIRECTORY* >(
                base + opt->DataDirectory[ IMAGE_DIRECTORY_ENTRY_TLS ].VirtualAddress );
            auto callback = reinterpret_cast< PIMAGE_TLS_CALLBACK* >( tls->AddressOfCallBacks );

            for ( ; callback && *callback; ++callback )
                ( *callback )( base, DLL_PROCESS_ATTACH, nullptr );
        }

        bool exception_support_failed = false;

#ifdef _WIN64
        if ( data->m_seh_support ) {
            auto exception_dir = opt->DataDirectory[ IMAGE_DIRECTORY_ENTRY_EXCEPTION ];
            if ( exception_dir.Size ) {
                if ( !rtl_add_function_table(
                    reinterpret_cast< IMAGE_RUNTIME_FUNCTION_ENTRY* >( base + exception_dir.VirtualAddress ),
                    exception_dir.Size / sizeof( IMAGE_RUNTIME_FUNCTION_ENTRY ),
                    reinterpret_cast< DWORD64 >( base ) ) ) {
                    exception_support_failed = true;
                }
            }
        }
#endif

        dll_main( base, data->m_reason, data->m_reserved );
        data->m_module = exception_support_failed
            ? reinterpret_cast< HINSTANCE >( 0x505050 )
            : reinterpret_cast< HINSTANCE >( base );
    }
#pragma optimize( "", on )
#pragma runtime_checks( "", restore )
}
