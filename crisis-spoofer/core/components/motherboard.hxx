#pragma once

namespace components {
    namespace firmware {
        namespace registry {
            bool set_smbios_data( const void* data, unsigned long size );
        }
    }

    namespace motherboard {
        GUID g_spoofed_smbios_uuid = { 0 };

        struct smbios_header_t {
            std::uint8_t m_type;
            std::uint8_t m_length;
            std::uint16_t m_handle;
        };

#pragma pack( push, 1 )
        struct smbios_type1_t {
            smbios_header_t m_header;
            std::uint8_t m_manufacturer;
            std::uint8_t m_product_name;
            std::uint8_t m_version;
            std::uint8_t m_serial_number;
            std::uint8_t m_uuid[ 16 ];
        };

        struct smbios_type2_t {
            smbios_header_t m_header;
            std::uint8_t m_manufacturer;
            std::uint8_t m_product_name;
            std::uint8_t m_version;
            std::uint8_t m_serial_number;
        };

        struct smbios_type3_t {
            smbios_header_t m_header;
            std::uint8_t m_manufacturer;
            std::uint8_t m_chassis_type;
            std::uint8_t m_version;
            std::uint8_t m_serial_number;
        };
#pragma pack( pop )

        struct smbios_range_t {
            PHYSICAL_ADDRESS* m_physical_address;
            unsigned long m_size;
        };

        struct boot_environment_information_t {
            GUID m_boot_identifier;
            std::uint32_t m_firmware_type;
            std::uint64_t m_boot_flags;
        };

        constexpr std::uint8_t m_physical_address_pattern[ ] = {
            0x48, 0x8B, 0x0D, 0x00, 0x00, 0x00, 0x00,
            0x48, 0x85, 0xC9,
            0x74, 0x00,
            0x8B, 0x15, 0x00, 0x00, 0x00, 0x00,
            0x44, 0x8D, 0x43, 0x00,
            0xE8
        };

        constexpr std::uint8_t m_physical_address_pattern_fallback[ ] = {
            0x48, 0x8B, 0x0D, 0x00, 0x00, 0x00, 0x00,
            0x48, 0x85, 0xC9,
            0x74, 0x00,
            0x8B, 0x15, 0x00, 0x00, 0x00, 0x00,
            0x44, 0x8D, 0x43
        };

        constexpr std::uint8_t m_size_pattern[ ] = {
            0x8B, 0x1D, 0x00, 0x00, 0x00, 0x00,
            0x48, 0x8B, 0xD0,
            0x44, 0x8B, 0xC3,
            0x48, 0x8B, 0xCD,
            0xE8, 0x00, 0x00, 0x00, 0x00,
            0x8B, 0xD3,
            0x48, 0x8B
        };

        constexpr std::uint8_t m_size_pattern_fallback_1[ ] = {
            0x44, 0x8B, 0x05, 0x00, 0x00, 0x00, 0x00,
            0x48, 0x8B, 0xD0,
            0x48, 0x8B, 0xCE,
            0xE8, 0x00, 0x00, 0x00, 0x00,
            0x8B, 0x15
        };

        constexpr std::uint8_t m_size_pattern_fallback_2[ ] = {
            0x44, 0x8B, 0x05, 0x00, 0x00, 0x00, 0x00,
            0x48, 0x8B, 0xD0,
            0x48, 0x8B, 0xCE,
            0xE8, 0x00, 0x00, 0x00, 0x00,
            0x8B, 0x15, 0x00, 0x00, 0x00, 0x00,
            0x48, 0x8B, 0xCB,
            0xE8
        };

        std::uint64_t resolve_relative( std::uint64_t instruction, std::uint32_t instruction_size, std::uint32_t displacement_offset ) {
            if ( !instruction )
                return 0;

            return instruction + instruction_size +
                *reinterpret_cast< std::int32_t* >( instruction + displacement_offset );
        }

        bool is_placeholder( const char* string ) {
            if ( !string || !string[ 0 ] )
                return true;

            return !std::strcmp( string, obf( "Default string" ) ) ||
                !std::strcmp( string, obf( "To be filled by O.E.M." ) ) ||
                !std::strcmp( string, obf( "To be filled By O.E.M." ) ) ||
                !std::strcmp( string, obf( "System Serial Number" ) );
        }

        char* get_string( smbios_header_t* header, std::uint8_t string_index, char* table_end ) {
            if ( !header || !string_index || !table_end )
                return nullptr;

            auto string = reinterpret_cast< char* >( header ) + header->m_length;
            if ( string >= table_end )
                return nullptr;

            for ( auto index = 1u; string < table_end && string[ 0 ]; index++ ) {
                if ( index == string_index )
                    return string;

                while ( string < table_end && string[ 0 ] )
                    string++;

                string++;
            }

            return nullptr;
        }

        GUID* get_guid( std::uint8_t* uuid ) {
            if ( !uuid )
                return nullptr;

            auto all_zero = true;
            auto all_ff = true;
            for ( auto idx = 0u; idx < sizeof( GUID ); idx++ ) {
                all_zero &= uuid[ idx ] == 0x00;
                all_ff &= uuid[ idx ] == 0xFF;
            }

            if ( all_zero || all_ff )
                return nullptr;

            return reinterpret_cast< GUID* >( uuid );
        }

        void print_guid( const char* label, GUID* guid ) {
            if ( !guid ) {
                disk::print( obf( "%s -> <null>" ), label );
                return;
            }

            disk::print( obf( "%s -> {%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}" ),
                label,
                guid->Data1,
                guid->Data2,
                guid->Data3,
                guid->Data4[ 0 ], guid->Data4[ 1 ],
                guid->Data4[ 2 ], guid->Data4[ 3 ], guid->Data4[ 4 ],
                guid->Data4[ 5 ], guid->Data4[ 6 ], guid->Data4[ 7 ] );
        }

        void randomize_string( char* string ) {
            if ( !string || !string[ 0 ] )
                return;

            for ( auto i = 0u; string[ i ]; ++i )
                string[ i ] = static_cast< char >( 'A' + ( kernel::rand( ) % 26 ) );
        }

        void randomize_guid( GUID& guid ) {
            auto* b = reinterpret_cast< std::uint8_t* >( &guid );

            for ( auto i = 0u; i < sizeof( GUID ); ++i )
                b[ i ] = static_cast< std::uint8_t >( kernel::rand( ) );
        }

        NTSTATUS process_table( smbios_header_t* header, char* table_end ) {
            if ( !header || !header->m_length )
                return STATUS_UNSUCCESSFUL;

            if ( header->m_type == 1 && header->m_length >= sizeof( smbios_type1_t ) ) {
                auto* type1 = reinterpret_cast< smbios_type1_t* >( header );

                auto* serial = get_string( header, type1->m_serial_number, table_end );
                if ( serial && !is_placeholder( serial ) )
                    randomize_string( serial );

                if ( get_guid( type1->m_uuid ) ) {
                    randomize_guid( *reinterpret_cast< GUID* >( type1->m_uuid ) );
                    g_spoofed_smbios_uuid = *reinterpret_cast< GUID* >( type1->m_uuid );
                }
            }

            if ( header->m_type == 2 && header->m_length >= sizeof( smbios_type2_t ) ) {
                auto* type2 = reinterpret_cast< smbios_type2_t* >( header );

                auto* serial = get_string( header, type2->m_serial_number, table_end );
                if ( serial && !is_placeholder( serial ) )
                    randomize_string( serial );
            }

            if ( header->m_type == 3 && header->m_length >= sizeof( smbios_type3_t ) ) {
                auto* type3 = reinterpret_cast< smbios_type3_t* >( header );

                auto* serial = get_string( header, type3->m_serial_number, table_end );
                if ( serial && !is_placeholder( serial ) )
                    randomize_string( serial );
            }

            return STATUS_SUCCESS;
        }

        NTSTATUS loop_tables( void* mapped, unsigned long size ) {
            if ( !mapped || !size )
                return STATUS_INVALID_PARAMETER;

            auto current = static_cast< char* >( mapped );
            const auto end = current + size;

            while ( current + sizeof( smbios_header_t ) <= end ) {
                auto header = reinterpret_cast< smbios_header_t* >( current );
                if ( header->m_length < sizeof( smbios_header_t ) )
                    break;

                if ( current + header->m_length > end )
                    break;

                auto table_end = current + header->m_length;
                auto found_terminator = false;

                while ( table_end + 1 < end ) {
                    if ( table_end[ 0 ] == 0 && table_end[ 1 ] == 0 ) {
                        table_end += 2;
                        found_terminator = true;
                        break;
                    }

                    table_end++;
                }

                process_table( header, table_end );

                if ( header->m_type == 127 || !found_terminator )
                    break;

                current = table_end;
            }

            return STATUS_SUCCESS;
        }

        bool find_physical_address( PHYSICAL_ADDRESS** physical_address ) {
            if ( !physical_address )
                return false;

            *physical_address = nullptr;

            auto address = module::scan(
                kernel::m_ntoskrnl_base,
                kernel::m_ntoskrnl_size,
                m_physical_address_pattern,
                obf( "xxx????xxxx?xx????xxx?x" ) );

            if ( !address ) {
                address = module::scan(
                    kernel::m_ntoskrnl_base,
                    kernel::m_ntoskrnl_size,
                    m_physical_address_pattern_fallback,
                    obf( "xxx????xxxx?xx????xxx" ) );
            }

            if ( !address )
                return false;

            *physical_address = reinterpret_cast< PHYSICAL_ADDRESS* >(
                resolve_relative( address, 7, 3 ) );

            return *physical_address != nullptr;
        }

        bool find_size( unsigned long* size ) {
            if ( !size )
                return false;

            *size = 0;

            auto address = module::scan(
                kernel::m_ntoskrnl_base,
                kernel::m_ntoskrnl_size,
                m_size_pattern,
                obf( "xx????xxxxxxxxxx????xxxx" ) );
            auto displacement_offset = 2u;
            auto instruction_size = 6u;

            if ( !address ) {
                address = module::scan(
                    kernel::m_ntoskrnl_base,
                    kernel::m_ntoskrnl_size,
                    m_size_pattern_fallback_1,
                    obf( "xxx????xxxxxxx????xx" ) );
                displacement_offset = 3;
                instruction_size = 7;
            }

            if ( !address ) {
                address = module::scan(
                    kernel::m_ntoskrnl_base,
                    kernel::m_ntoskrnl_size,
                    m_size_pattern_fallback_2,
                    obf( "xxx????xxxxxxx????xx????xxxx" ) );
                displacement_offset = 3;
                instruction_size = 7;
            }

            if ( !address )
                return false;

            const auto size_address = resolve_relative( address, instruction_size, displacement_offset );
            if ( !size_address )
                return false;

            *size = *reinterpret_cast< unsigned long* >( size_address );
            return *size != 0;
        }

        bool find_range( smbios_range_t* range ) {
            if ( !range )
                return false;

            range->m_physical_address = nullptr;
            range->m_size = 0;

            if ( !find_physical_address( &range->m_physical_address ) )
                return false;

            if ( !find_size( &range->m_size ) )
                return false;

            return range->m_physical_address && range->m_size;
        }

        void debug_boot_environment( ) {
            constexpr auto system_boot_environment_information = 0x5Aul;
            constexpr auto pool_tag = 'BMSR';
            constexpr auto needed_size = 8 * 1024 * 1024;

            auto zw_query_system_information = reinterpret_cast< NTSTATUS( * )( std::uint32_t, void*, unsigned long, unsigned long* ) >(
                kernel::get_export( kernel::m_ntoskrnl_base, obf( "ZwQuerySystemInformation" ) ) );
            if ( !zw_query_system_information ) {
                disk::print( obf( "boot environment -> ZwQuerySystemInformation not found" ) );
                return;
            }

            auto boot_info = reinterpret_cast< boot_environment_information_t* >(
                ExAllocatePoolWithTag( NonPagedPool, needed_size, pool_tag ) );

            if ( !boot_info ) {
                disk::print( obf( "boot environment -> allocation failed" ) );
                return;
            }

            RtlZeroMemory( boot_info, needed_size );

            const auto status = zw_query_system_information(
                system_boot_environment_information,
                boot_info,
                needed_size,
                nullptr
            );

            if ( NT_SUCCESS( status ) ) {
                GUID newGuid;
                ExUuidCreate( &newGuid );
                boot_info->m_boot_identifier = newGuid;

                print_guid( obf( "boot identifier" ), &boot_info->m_boot_identifier );
            }
            else {
                disk::print( obf( "boot environment query failed -> 0x%x" ), status );
            }

            ExFreePoolWithTag( boot_info, pool_tag );
        }

        bool spoof( ) {
            smbios_range_t range{ };
            if ( !find_range( &range ) ) {
                disk::print( obf( "could not find smbios range" ) );
                return false;
            }

            auto mapped = MmMapIoSpace( *range.m_physical_address, range.m_size, MmNonCached );
            if ( !mapped ) {
                disk::print( obf( "could not map smbios table" ) );
                return false;
            }

            const auto status = loop_tables( mapped, range.m_size );
                  
            MmUnmapIoSpace( mapped, range.m_size );
            debug_boot_environment( );
            return true;
        }
    }
}