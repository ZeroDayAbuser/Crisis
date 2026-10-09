#pragma once

namespace cache {
    list_entry_t* read_list_entry( std::uint64_t address ) {
        list_entry_t* entry = nullptr;
        memory::read_virtual( address, &entry, sizeof( entry ) );
        return entry;
    }

    piddb_cache_entry_t* lookup_piddb_entry( avl_table_t* table, std::uint32_t timestamp, const wchar_t* name ) {
        wchar_t name_with_ext[ 64 ]{};
        if ( swprintf_s( name_with_ext, L"%ls%ls", name ? name : L"", static_cast< wchar_t* >( obf( L".log" ) ) ) <= 0 )
            return nullptr;

        piddb_cache_entry_t local_entry{};
        local_entry.m_driver_name.m_length = static_cast< std::uint16_t >( wcslen( name_with_ext ) * 2 );
        local_entry.m_driver_name.m_maximum_length = local_entry.m_driver_name.m_length + 2;
        local_entry.m_driver_name.m_buffer = name_with_ext;
        local_entry.m_timestamp = timestamp;
        return reinterpret_cast< piddb_cache_entry_t* >( kernel::rtl_lookup_element_generic_table_avl( table, &local_entry ) );
    }

    bool clear_piddb_cache_table( const wchar_t* driver_name, std::uint32_t timestamp ) {
        auto lock_addr = g_pdb->get_symbol_address( obf( "PiDDBLock" ) );
        auto table_addr = g_pdb->get_symbol_address( obf( "PiDDBCacheTable" ) );
        if ( !lock_addr || !table_addr ) {
            logging::print( obf( "failed to get piddb symbols" ) );
            return false;
        }

        auto lock = reinterpret_cast< void* >( lock_addr );
        auto table = reinterpret_cast< avl_table_t* >( table_addr );

        if ( !kernel::ex_acquire_resource_exclusive_lite( lock, true ) ) {
            logging::print( obf( "failed to acquire piddb lock" ) );
            return false;
        }

        auto entry = lookup_piddb_entry( table, timestamp, driver_name );
        if ( !entry ) {
            logging::print( obf( "failed to find piddb entry" ) );
            kernel::ex_release_resource_lite( lock );
            return false;
        }

        auto list_base = reinterpret_cast< std::uint64_t >( entry ) + offsetof( piddb_cache_entry_t, m_list );
        auto prev = memory::read< list_entry_t* >( list_base + offsetof( list_entry_t, m_blink ) );
        auto next = memory::read< list_entry_t* >( list_base + offsetof( list_entry_t, m_flink ) );

        if ( prev )
            memory::write< list_entry_t* >( reinterpret_cast< std::uint64_t >( prev ) + offsetof( list_entry_t, m_flink ), next );
        if ( next )
            memory::write< list_entry_t* >( reinterpret_cast< std::uint64_t >( next ) + offsetof( list_entry_t, m_blink ), prev );

        if ( !kernel::rtl_delete_element_generic_table_avl( table, entry ) ) {
            logging::print( "unlinked driver entry from piddb but failed to delete it.\n" );
            kernel::ex_release_resource_lite( lock );
            return false;
        }

        auto delete_count = memory::read< std::uint32_t >( reinterpret_cast< std::uint64_t >( table ) + offsetof( avl_table_t, m_delete_count ) );
        if ( delete_count > 0 )
            memory::write< std::uint32_t >( reinterpret_cast< std::uint64_t >( table ) + offsetof( avl_table_t, m_delete_count ), delete_count - 1 );

        kernel::ex_release_resource_lite( lock );
        logging::print( obf( "piddb cache table cleared" ) );
        return true;
    }

    bool clear_mm_unloaded_drivers( const wchar_t* driver_name ) {
        auto mm_unloaded_drivers = g_pdb->get_symbol_address( obf( "MmUnloadedDrivers" ) );
        if ( !mm_unloaded_drivers ) {
            logging::print( obf( "failed to get unloaded driver symbols" ) );
            return false;
        }

        std::uint64_t unloaded_drivers = 0;
        if ( !memory::read_virtual( mm_unloaded_drivers, unloaded_drivers ) ) {
            logging::print( obf( "failed to read MmUnloadedDrivers at 0x%llx" ), mm_unloaded_drivers );
            return false;
        }
        if ( !unloaded_drivers ) {
            logging::print( obf( "no entries found in unloaded drivers" ) );
            return false;
        }

        for ( auto idx = 0u; idx < 50u; ++idx ) {
            const auto unloaded_driver_addr = unloaded_drivers + idx * 0x28u;
            auto unloaded_driver = memory::read< unicode_string_t >( unloaded_driver_addr );
            if ( !unloaded_driver.m_length || !unloaded_driver.m_buffer )
                continue;

            auto name_buf = std::make_unique< wchar_t[ ] >( unloaded_driver.m_length / 2 + 1 );
            if ( !memory::read_virtual( reinterpret_cast< std::uint64_t >( unloaded_driver.m_buffer ), name_buf.get( ), unloaded_driver.m_length ) )
                continue;

            if ( std::wstring( name_buf.get( ) ).find( driver_name ) == std::wstring::npos )
                continue;

            std::array< std::uint8_t, 0x28u > zeroes{};
            memory::write_virtual( unloaded_driver_addr, zeroes.data( ), 0x28u );
            return true;
        }

        return false;
    }

    bool clear_kernel_hash_bucket_list( const wchar_t* driver_name, const wchar_t* driver_path ) {
        auto ci_pdb = std::make_unique< pdb::c_pdb >( "ci.dll" );
        if ( !ci_pdb || !ci_pdb->load( ) ) {
            logging::print( obf( "failed to load ci pdb" ) );
            return false;
        }

        auto lock_addr = ci_pdb->get_symbol_address( obf( "g_HashCacheLock" ) );
        auto list_addr = ci_pdb->get_symbol_address( obf( "g_KernelHashBucketList" ) );
        if ( !lock_addr || !list_addr ) {
            logging::print( obf( "failed to get ci symbols: lock=%llx list=%llx" ), lock_addr, list_addr );
            return false;
        }

        auto prev_ptr = list_addr;
        auto entry = memory::read< hash_bucket_entry_t* >( prev_ptr );

        if ( !entry ) {
            logging::print( obf( "hash bucket list is empty" ) );
            return true;
        }

        while ( entry ) {
            auto module_name = memory::read< unicode_string_t >(
                reinterpret_cast< std::uint64_t >( entry ) + offsetof( hash_bucket_entry_t, m_driver_name ) );

            if ( module_name.m_length == 0 ) {
                prev_ptr = reinterpret_cast< std::uint64_t >( entry );
                entry = memory::read< hash_bucket_entry_t* >( entry );
                continue;
            }

            auto name_ptr = memory::read< wchar_t* >(
                reinterpret_cast< std::uint64_t >( entry ) +
                offsetof( hash_bucket_entry_t, m_driver_name ) +
                offsetof( unicode_string_t, m_buffer ) );

            if ( !name_ptr ) {
                prev_ptr = reinterpret_cast< std::uint64_t >( entry );
                entry = memory::read< hash_bucket_entry_t* >( entry );
                continue;
            }

            auto name_buffer = std::make_unique< wchar_t[ ] >( module_name.m_length / 2 + 1 );
            if ( !memory::read_virtual( reinterpret_cast< std::uint64_t >( name_ptr ), name_buffer.get( ), module_name.m_length ) ) {
                prev_ptr = reinterpret_cast< std::uint64_t >( entry );
                entry = memory::read< hash_bucket_entry_t* >( entry );
                continue;
            }

            if ( std::wstring( name_buffer.get( ) ).find( driver_name ) != std::wstring::npos ) {
                auto next = memory::read< hash_bucket_entry_t* >( entry );
                memory::write< hash_bucket_entry_t* >( prev_ptr, next );
                kernel::free_pool( reinterpret_cast< std::uint64_t >( entry ) );
                logging::print( obf( "kernel hash bucket cleared" ) );
                return true;
            }

            prev_ptr = reinterpret_cast< std::uint64_t >( entry );
            entry = memory::read< hash_bucket_entry_t* >( entry );
        }

        logging::print( obf( "hash bucket entry not found" ) );
        return false;
    }

    bool clear_wd_filter_driver_list( ) {
        auto wd_pdb = std::make_unique< pdb::c_pdb >( "WdFilter.sys" );
        if ( !wd_pdb ) {
            logging::print( obf( "failed to create wd pdb" ) );
            return true;
        }

        if ( wd_pdb->load( ) ) {
            logging::print( obf( "filter driver found!" ) );
            logging::print( obf( "please disable anti-virus and restart your computer." ) );
            return false;
        }

        logging::print( obf( "filter driver not found" ) );
        return true;
    }


    inline bool get_phys_ranges( std::vector< std::pair< uint64_t, uint64_t > >& ranges ) {
        MEMORYSTATUSEX status{ .dwLength = sizeof( MEMORYSTATUSEX ) };

        if ( !GlobalMemoryStatusEx( &status ) )
            return false;

        const auto total = status.ullTotalPhys / ( 1024ULL * 1024 * 1024 );

        ranges.push_back( { 0x2000, 0x9FFFF } );
        ranges.push_back( { 0x100000, 0xDAF0FFF } );
        ranges.push_back( { 0xDAF2000, 0xFB6EFFF } );
        ranges.push_back( { 0xFBFF000, 0xBFFFFFFF } );

        if ( total >= 8 )
            ranges.push_back( { 0x100000000, 0x1FFFFFFFF } );

        if ( total >= 16 )
            ranges.push_back( { 0x200000000, 0x3FFFFFFFF } );

        if ( total >= 32 )
            ranges.push_back( { 0x400000000, 0x7FFFFFFFF } );

        if ( total >= 64 )
            ranges.push_back( { 0x800000000, 0xFFFFFFFFF } );

        return true;
    }

    inline bool clear_driver_image( uint32_t timestamp ) {
        bool success{ true };
        std::vector< std::pair< uint64_t, uint64_t > > ranges{ };

        if ( !get_phys_ranges( ranges ) )
            return false;

        for ( size_t i{ }; i < ranges.size( ); i++ ) {
            const auto range = ranges[ i ];

            for ( uintptr_t address{ range.first }; address <= range.second; address += 0x1000 ) {
                IMAGE_DOS_HEADER dos_header{ };

                if ( !memory::read_physical( address, &dos_header, sizeof( IMAGE_DOS_HEADER ) ) )
                    continue;

                if ( dos_header.e_magic != 0x5a4d )
                    continue;

                IMAGE_NT_HEADERS nt_headers{ };

                if ( !memory::read_physical( address + dos_header.e_lfanew, &nt_headers, sizeof( IMAGE_NT_HEADERS ) ) )
                    continue;

                if ( nt_headers.Signature != 0x4550 )
                    continue;

                const auto current_timestamp = ( uint32_t ) nt_headers.FileHeader.TimeDateStamp;

                if ( current_timestamp == timestamp ) {
                    for ( uintptr_t offset{ }; offset < nt_headers.OptionalHeader.SizeOfImage; offset += 0x1000 ) {
                        const auto size = min( 0x1000, nt_headers.OptionalHeader.SizeOfImage - offset );
                        const auto phys = address + size;

                        std::vector< uint8_t > buffer( size, 0 );

                        if ( !memory::write_physical( phys, buffer.data( ), size ) ) {
                            success = false;
                            break;
                        }
                    }

                    break;
                }
            }
        }

        return success;
    }
}
