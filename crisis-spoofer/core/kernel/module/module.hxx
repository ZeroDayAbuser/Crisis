#pragma once

namespace module {
    constexpr auto max_loaded_module_walk = 1024u;

    struct info_t {
        std::uint64_t m_base;
        std::uint64_t m_size;
        kldr_data_table_entry_t* m_entry;
    };

    bool same_section_name( const section_header_t* section, const char* section_name ) {
        if ( !section || !section_name )
            return false;

        for ( auto idx = 0u; idx < sizeof( section->m_name ); idx++ ) {
            const auto lhs = section->m_name[ idx ];
            const auto rhs = section_name[ idx ];

            if ( !rhs )
                return !lhs;

            if ( lhs != rhs )
                return false;
        }

        return section_name[ sizeof( section->m_name ) ] == 0;
    }

    list_entry_t* loaded_module_list( ) {
        auto list = reinterpret_cast< list_entry_t* >(
            kernel::get_export( kernel::m_ntoskrnl_base, obf( "PsLoadedModuleList" ) ) );

        if ( !list || !list->m_flink || !list->m_blink )
            return nullptr;

        return list;
    }

    bool find( const wchar_t* module_name, info_t* out ) {
        if ( !module_name || !out )
            return false;

        out->m_base = 0;
        out->m_size = 0;
        out->m_entry = nullptr;

        auto list = loaded_module_list( );
        if ( !list )
            return false;

        unicode_string_t target_name{ };
        kernel::rtl_init_unicode_string( &target_name, module_name );

        auto link = list->m_flink;
        for ( auto walk = 0u; link && link != list && walk < max_loaded_module_walk; walk++ ) {
            auto entry = CONTAINING_RECORD( link, kldr_data_table_entry_t, m_in_load_order_links );
            if ( entry->m_dll_base && entry->m_size_of_image &&
                kernel::rtl_compare_unicode_string( &entry->m_base_dll_name, &target_name, true ) ) {
                out->m_base = reinterpret_cast< std::uint64_t >( entry->m_dll_base );
                out->m_size = entry->m_size_of_image;
                out->m_entry = entry;
                return true;
            }

            link = entry->m_in_load_order_links.m_flink;
        }

        return false;
    }

    std::uint64_t find_base( const wchar_t* module_name, std::uint64_t* module_size = nullptr ) {
        info_t module{ };
        if ( !find( module_name, &module ) )
            return 0;

        if ( module_size )
            *module_size = module.m_size;

        return module.m_base;
    }

    bool find_by_address( std::uint64_t address, info_t* out ) {
        if ( !address || !out )
            return false;

        out->m_base = 0;
        out->m_size = 0;
        out->m_entry = nullptr;

        auto list = loaded_module_list( );
        if ( !list )
            return false;

        auto link = list->m_flink;
        for ( auto walk = 0u; link && link != list && walk < max_loaded_module_walk; walk++ ) {
            auto entry = CONTAINING_RECORD( link, kldr_data_table_entry_t, m_in_load_order_links );
            const auto base = reinterpret_cast< std::uint64_t >( entry->m_dll_base );
            const auto size = static_cast< std::uint64_t >( entry->m_size_of_image );

            if ( base && size && address >= base && address < base + size ) {
                out->m_base = base;
                out->m_size = size;
                out->m_entry = entry;
                return true;
            }

            link = entry->m_in_load_order_links.m_flink;
        }

        return false;
    }

    section_header_t* find_section( std::uint64_t module_base, const char* section_name, std::uint64_t* section_base = nullptr, std::uint64_t* section_size = nullptr ) {
        if ( !module_base || !section_name )
            return nullptr;

        auto dos_header = reinterpret_cast< dos_header_t* >( module_base );
        if ( !dos_header->is_valid( ) )
            return nullptr;

        auto nt_headers = reinterpret_cast< nt_headers_t* >( module_base + dos_header->m_lfanew );
        if ( !nt_headers->is_valid( ) )
            return nullptr;

        auto section = reinterpret_cast< section_header_t* >(
            reinterpret_cast< std::uintptr_t >( nt_headers ) +
            nt_headers->m_size_of_optional_header + 0x18 );

        for ( auto idx = 0; idx < nt_headers->m_number_of_sections; idx++ ) {
            if ( !same_section_name( &section[ idx ], section_name ) )
                continue;

            if ( section_base )
                *section_base = module_base + section[ idx ].m_virtual_address;

            if ( section_size )
                *section_size = section[ idx ].m_virtual_size ?
                    section[ idx ].m_virtual_size : section[ idx ].m_size_of_raw_data;

            return &section[ idx ];
        }

        return nullptr;
    }

    bool matches( const std::uint8_t* address, const std::uint8_t* pattern, const char* mask ) {
        if ( !address || !pattern || !mask )
            return false;

        for ( auto idx = 0ull; mask[ idx ]; idx++ ) {
            if ( mask[ idx ] == '?' )
                continue;

            if ( address[ idx ] != pattern[ idx ] )
                return false;
        }

        return true;
    }

    std::uint64_t scan( std::uint64_t base, std::uint64_t size, const std::uint8_t* pattern, const char* mask ) {
        if ( !base || !size || !pattern || !mask )
            return 0;

        const auto pattern_size = std::strlen( mask );
        if ( !pattern_size || pattern_size > size )
            return 0;

        const auto scan_end = size - pattern_size;
        for ( auto offset = 0ull; offset <= scan_end; offset++ ) {
            const auto address = reinterpret_cast< const std::uint8_t* >( base + offset );
            if ( matches( address, pattern, mask ) )
                return reinterpret_cast< std::uint64_t >( address );
        }

        return 0;
    }

    std::uint64_t scan_module( const wchar_t* module_name, const std::uint8_t* pattern, const char* mask ) {
        info_t module{ };
        if ( !find( module_name, &module ) )
            return 0;

        return scan( module.m_base, module.m_size, pattern, mask );
    }

    std::uint64_t scan_section( const wchar_t* module_name, const char* section_name, const std::uint8_t* pattern, const char* mask ) {
        info_t module{ };
        if ( !find( module_name, &module ) )
            return 0;

        std::uint64_t section_base = 0;
        std::uint64_t section_size = 0;
        if ( !find_section( module.m_base, section_name, &section_base, &section_size ) )
            return 0;

        return scan( section_base, section_size, pattern, mask );
    }

    bool is_space( char value ) {
        return value == ' ' || value == '\t' || value == '\r' || value == '\n';
    }

    std::int32_t hex_value( char value ) {
        if ( value >= '0' && value <= '9' )
            return value - '0';

        if ( value >= 'a' && value <= 'f' )
            return value - 'a' + 10;

        if ( value >= 'A' && value <= 'F' )
            return value - 'A' + 10;

        return -1;
    }

    std::uint64_t scan_ida( std::uint64_t base, std::uint64_t size, const char* signature ) {
        if ( !base || !size || !signature )
            return 0;

        auto first = true;
        auto* current = reinterpret_cast< std::uint8_t* >( base );
        auto* match = current;
        auto* end = reinterpret_cast< std::uint8_t* >( base + size );
        auto* pattern = signature;

        while ( current < end ) {
            while ( is_space( *pattern ) )
                pattern++;

            if ( !*pattern )
                return reinterpret_cast< std::uint64_t >( match );

            if ( *pattern == '?' ) {
                if ( first ) {
                    match = current;
                    first = false;
                }

                current++;
                pattern++;
                if ( *pattern == '?' )
                    pattern++;

                continue;
            }

            const auto high = hex_value( pattern[ 0 ] );
            const auto low = hex_value( pattern[ 1 ] );
            if ( high < 0 || low < 0 )
                return 0;

            const auto value = static_cast< std::uint8_t >( ( high << 4 ) | low );
            if ( *current == value ) {
                if ( first ) {
                    match = current;
                    first = false;
                }

                current++;
                pattern += 2;
                continue;
            }

            current = first ? current + 1 : match + 1;
            pattern = signature;
            first = true;
        }

        return 0;
    }

    std::uint64_t scan_module_ida( const wchar_t* module_name, const char* signature ) {
        info_t module{ };
        if ( !find( module_name, &module ) )
            return 0;

        return scan_ida( module.m_base, module.m_size, signature );
    }

    std::uint64_t scan_section_ida( const wchar_t* module_name, const char* section_name, const char* signature ) {
        info_t module{ };
        if ( !find( module_name, &module ) )
            return 0;

        std::uint64_t section_base = 0;
        std::uint64_t section_size = 0;
        if ( !find_section( module.m_base, section_name, &section_base, &section_size ) )
            return 0;

        return scan_ida( section_base, section_size, signature );
    }
}
