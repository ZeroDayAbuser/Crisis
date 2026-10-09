#pragma once

namespace kernel {
	std::uint64_t m_ntoskrnl_base;
    std::uint64_t m_ntoskrnl_size;

    std::uint64_t m_image_base;
    std::uint64_t m_image_size;

    std::uint64_t get_export( std::uint64_t module_base, const char* export_name ) {
        auto dos_header{ reinterpret_cast< dos_header_t* > ( module_base ) };
        auto nt_headers{ reinterpret_cast< nt_headers_t* > ( module_base + dos_header->m_lfanew ) };
        if ( !dos_header->is_valid( ) || !nt_headers->is_valid( ) )
            return { };

        auto exp_dir{ nt_headers->m_export_table.as_rva< export_directory_t* >( module_base ) };
        if ( !exp_dir->m_address_of_functions
            || !exp_dir->m_address_of_names
            || !exp_dir->m_address_of_names_ordinals )
            return { };

        auto name{ reinterpret_cast< std::int32_t* > ( module_base + exp_dir->m_address_of_names ) };
        auto functions{ reinterpret_cast< std::int32_t* > ( module_base + exp_dir->m_address_of_functions ) };
        auto ordinals{ reinterpret_cast< std::int16_t* > ( module_base + exp_dir->m_address_of_names_ordinals ) };

        for ( auto idx = 0; idx < exp_dir->m_number_of_names; idx++ ) {
            auto cur_name{ module_base + name[ idx ] };
            auto cur_func{ module_base + functions[ ordinals[ idx ] ] };
            if ( !cur_name || !cur_func )
                continue;

            if ( std::strcmp( export_name, reinterpret_cast< char* >( cur_name ) ) )
                return cur_func;
        }

        return 0;
    }

    bool get_section( const char* section_name, std::uint64_t* exec_base, std::uint64_t* exec_size ) {
        auto dos_header{ reinterpret_cast< dos_header_t* > ( m_ntoskrnl_base ) };
        auto nt_headers{ reinterpret_cast< nt_headers_t* > ( m_ntoskrnl_base + dos_header->m_lfanew ) };
        if ( !dos_header->is_valid( )
            || !nt_headers->is_valid( ) )
            return false;

        auto section_header = reinterpret_cast< section_header_t* >(
            reinterpret_cast< std::uintptr_t >( nt_headers ) +
            nt_headers->m_size_of_optional_header + 0x18 );

        for ( int i = 0; i < nt_headers->m_number_of_sections; i++ ) {
            auto current_section_base = reinterpret_cast< std::uint64_t >( dos_header ) + section_header[ i ].m_virtual_address;
            if ( !std::strcmp( section_header[ i ].m_name, section_name ) ) {
                *exec_base = current_section_base;
                *exec_size = section_header[ i ].m_size_of_raw_data;
                break;
            }
        }

        return *exec_base && *exec_size;
    }

    std::uintptr_t find_pattern( std::uintptr_t base, std::size_t size, const char* pattern, const char* mask ) {
        const auto pattern_length = std::strlen( mask );
        if ( pattern_length == 0 || size < pattern_length )
            return 0;

        for ( std::size_t i = 0; i <= size - pattern_length; ++i ) {
            bool found = true;

            for ( std::size_t j = 0; j < pattern_length; ++j ) {
                if ( mask[ j ] == 'x' &&
                    *reinterpret_cast< const unsigned char* >( base + i + j ) != static_cast< unsigned char >( pattern[ j ] ) ) {
                    found = false;
                    break;
                }
            }

            if ( found )
                return base + i;
        }

        return 0;
    }

    std::uintptr_t find_signature( std::uintptr_t base, std::size_t size, const std::uint8_t* signature, const char* mask ) {
        const auto sig_length = std::strlen( mask );

        if ( sig_length == 0 || size < sig_length )
            return 0;

        for ( std::size_t i = 0; i <= size - sig_length; ++i ) {
            bool found = true;

            for ( std::size_t j = 0; j < sig_length; ++j ) {
                if ( mask[ j ] == 'x' &&
                    *reinterpret_cast< const std::uint8_t* >( base + i + j ) != signature[ j ] ) {
                    found = false;
                    break;
                }
            }

            if ( found )
                return base + i;
        }

        return 0;
    }

    std::uintptr_t find_ida_pattern( std::uintptr_t base, std::size_t size, const char* ida_pattern ) {
        std::uint8_t pattern[ 256 ];
        char mask[ 256 ];
        std::size_t pattern_size = 0;

        const char* ptr = ida_pattern;
        while ( *ptr ) {
            if ( *ptr == ' ' ) {
                ptr++;
                continue;
            }

            if ( *ptr == '?' ) {
                mask[ pattern_size ] = '?';
                pattern[ pattern_size++ ] = 0;
                ptr++;

                if ( *ptr == '?' ) ptr++;
            }
            else {
                char byte_str[ 3 ] = { ptr[ 0 ], ptr[ 1 ], 0 };
                pattern[ pattern_size ] = static_cast< std::uint8_t >( strtoul( byte_str, nullptr, 16 ) );
                mask[ pattern_size++ ] = 'x';
                ptr += 2;
            }

            if ( *ptr == ' ' ) ptr++;
        }

        mask[ pattern_size ] = 0;

        for ( std::size_t i = 0; i < pattern_size; i++ ) {
            if ( mask[ i ] == '?' ) mask[ i ] = '?';
            else mask[ i ] = 'x';
        }

        return find_signature( base, size, pattern, mask );
    }

    std::uintptr_t scan_text_section( const char* pattern, const char* mask ) {
        std::uint64_t text_base = 0;
        std::uint64_t text_size = 0;

        if ( !get_section( obf( ".text" ), &text_base, &text_size ) )
            return 0;

        return find_pattern( text_base, text_size, pattern, mask );
    }
}