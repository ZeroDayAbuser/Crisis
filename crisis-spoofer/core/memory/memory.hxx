#pragma once

namespace memory {
    bool is_page_contained( std::uint64_t address, std::size_t size ) {
        return ( address & paging::page_4kb_mask ) + size <= paging::page_4kb_size;
    }

    bool is_covered_by_runtime_function(
        std::uint64_t module_base,
        nt_headers_t* nt_headers,
        std::uint64_t address,
        std::size_t size ) {
        if ( !module_base || !nt_headers || !nt_headers->m_exception_table.m_virtual_address ||
            !nt_headers->m_exception_table.m_size )
            return false;

        const auto start_rva = static_cast< std::uint32_t >( address - module_base );
        const auto end_rva = static_cast< std::uint32_t >( start_rva + size );
        auto functions = nt_headers->m_exception_table.as_rva< runtime_function_t* >( module_base );
        const auto function_count = nt_headers->m_exception_table.m_size / sizeof( runtime_function_t );

        for ( auto idx = 0; idx < function_count; idx++ ) {
            const auto begin = functions[ idx ].m_begin_address;
            const auto end = functions[ idx ].m_end_address;
            if ( !begin || !end || begin >= end )
                continue;

            if ( start_rva < end && end_rva > begin )
                return true;
        }

        return false;
    }

    bool is_suitable_cave(
        std::uint64_t module_base,
        nt_headers_t* nt_headers,
        std::uint64_t address,
        std::size_t size ) {
        return is_page_contained( address, size ) &&
            !is_covered_by_runtime_function( module_base, nt_headers, address, size );
    }

    std::uint64_t find_cave(
        std::uint64_t module_base,
        std::size_t size
    ) {
        if ( !module_base || size == 0 )
            return 0;

        auto dos_header = reinterpret_cast< dos_header_t* >( module_base );
        if ( !dos_header->is_valid( ) )
            return 0;

        auto nt_headers = reinterpret_cast< nt_headers_t* >( module_base + dos_header->m_lfanew );
        if ( !nt_headers->is_valid( ) )
            return 0;

        auto section = reinterpret_cast< section_header_t* >(
            reinterpret_cast< std::uintptr_t >( nt_headers ) +
            nt_headers->m_size_of_optional_header + 0x18 );

        const auto section_count = nt_headers->m_number_of_sections;
        if ( !section || section_count == 0 || section_count > 96 )
            return 0;

        for ( auto i = 0; i < section_count; i++ ) {
            if ( ( section[ i ].m_characteristics & 0x20000000 ) == 0 )
                continue;
            if ( section[ i ].m_virtual_address == 0 || section[ i ].m_virtual_size == 0 )
                continue;
            if ( section[ i ].m_virtual_address + section[ i ].m_virtual_size >
                nt_headers->m_size_of_image )
                continue;

            const auto section_start = module_base + section[ i ].m_virtual_address;
            const auto section_end = section_start + section[ i ].m_virtual_size;

            if ( section_start < 0xFFFF000000000000 )
                continue;
            if ( section[ i ].m_virtual_size < size )
                continue;

            auto* start_ptr = reinterpret_cast< std::uint8_t* >( section_start );
            auto* end_ptr = reinterpret_cast< std::uint8_t* >( section_end );

            std::uint8_t* cave_start = nullptr;
            std::size_t   cave_size = 0;

            for ( auto* ptr = start_ptr; ptr < end_ptr; ptr++ ) {
                const auto b = *ptr;
                if ( b == 0xCC || b == 0x00 || b == 0x90 ) {
                    if ( !cave_start )
                        cave_start = ptr;
                    cave_size++;
                    if ( cave_size >= size ) {
                        const auto cave_va = reinterpret_cast< std::uint64_t >( cave_start );
                        if ( is_suitable_cave( module_base, nt_headers, cave_va, size ) )
                            return cave_va;
                        cave_start = nullptr;
                        cave_size = 0;
                    }
                }
                else {
                    cave_start = nullptr;
                    cave_size = 0;
                }
            }

            const auto image_end = module_base + nt_headers->m_size_of_image;
            if ( section_end + size <= image_end ) {
                auto* post = reinterpret_cast< std::uint8_t* >( section_end );
                bool suitable = true;
                for ( std::size_t j = 0; j < size; j++ ) {
                    if ( reinterpret_cast< std::uint64_t >( post + j ) >= image_end )
                        break;
                    const auto b = post[ j ];
                    if ( b != 0x00 && b != 0xCC && b != 0x90 && b != 0xFF ) {
                        suitable = false;
                        break;
                    }
                }
                const auto post_va = reinterpret_cast< std::uint64_t >( post );
                if ( suitable && is_suitable_cave( module_base, nt_headers, post_va, size ) )
                    return post_va;
            }

        }

        return 0;
    }

    std::uint64_t find_cave( std::size_t size) {
        const auto page_base = kernel::m_image_base & ~( paging::page_2mb_size - 1 );
        const auto page_end = page_base + paging::page_2mb_size;
        const auto search_start = kernel::m_image_base + kernel::m_image_size;
        const auto search_end = page_end - size;

        if ( search_start >= page_end ) {
            kernel::dbg_print( obf( "no space in large page.\n" ) );
            return 0;
        }

        auto* base = reinterpret_cast< std::uint8_t* >( search_start );
        const auto scan_size = search_end - search_start;

        for ( auto idx = 0; idx <= scan_size; ) {
            bool found = true;
            for ( auto j = 0; j < size; j++ ) {
                if ( base[ idx + j ] != 0x00 ) {
                    found = false;
                    idx += j + 1;
                    break;
                }
            }

            if ( !found )
                continue;

            const std::uint64_t cave_va = search_start + idx;

            paging::pt_entries_t entries;
            if ( !paging::hyperspace_entries( entries, cave_va ) ||
                !entries.m_pte.hard.present ) {
                idx++;
                continue;
            }

            if ( entries.m_pte.hard.no_execute ) {
                pte forced = entries.m_pte;
                forced.hard.no_execute = 0;
                forced.hard.read_write = 1;

                const auto pt_phys =
                    ( entries.m_pde.hard.pfn << paging::page_shift ) +
                    ( virt_addr_t{ cave_va }.pte_index * sizeof( pte ) );
                if ( !paging::dpm::write_physical( pt_phys, &forced, sizeof( pte ) ) ) {
                    idx++;
                    continue;
                }

                kernel::flush_caches( cave_va );
            }

            return cave_va;
        }

        kernel::dbg_print( obf( "no cave found (needed 0x%zx)\n" ), size );
        return 0;
    }

    bool get_image_file_name( void* eprocess, char* image_file_name, std::size_t image_file_name_size ) {
        if ( !image_file_name || !image_file_name_size )
            return false;

        RtlSecureZeroMemory( image_file_name, image_file_name_size );

        if ( !kernel::mm_is_address_valid( eprocess ) )
            return false;

        auto image_file_name_offset = kernel::get_image_file_name_offset( );
        auto image_file_name_ptr = reinterpret_cast< const char* >(
            reinterpret_cast< std::uintptr_t >( eprocess ) + image_file_name_offset
            );

        if ( !kernel::mm_is_address_valid( const_cast< char* >( image_file_name_ptr ) ) )
            return false;

        __try {
            for ( std::size_t i = 0; i < image_file_name_size - 1; i++ ) {
                image_file_name[ i ] = image_file_name_ptr[ i ];
                if ( image_file_name[ i ] == '\0' )
                    break;
            }
            image_file_name[ image_file_name_size - 1 ] = '\0';
        }
        __except ( 1 ) {
            RtlSecureZeroMemory( image_file_name, image_file_name_size );
            return false;
        }

        return true;
    }

}
