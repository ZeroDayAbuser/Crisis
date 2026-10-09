#pragma once

namespace components::monitor {
    namespace gpu {
        struct nvidia_scan_t {
            std::uint64_t m_module_base;
            std::uint64_t m_module_size;
            std::uint64_t m_signature;
            std::uint64_t m_gpu_mgr_get_gpu_from_id;
            std::uint64_t m_uuid_walk_start;
            std::uint32_t m_uuid_valid_offset;
        };

        constexpr std::uint8_t m_nvidia_gpu_pattern[] = {
            0xE8, 0x00, 0x00, 0x00, 0x00,
            0x48, 0x8B, 0xD8,
            0x48, 0x85, 0xC0,
            0x0F, 0x84, 0x00, 0x00, 0x00, 0x00,
            0x44, 0x8B, 0x80, 0x00, 0x00, 0x00, 0x00,
            0x48, 0x8D, 0x15
        };

        constexpr const char* m_nvidia_gpu_mask = "x????xxxxxxxx????xxx????xxx";

        std::uint64_t resolve_relative_call( std::uint64_t call_instruction ) {
            if ( !call_instruction || *reinterpret_cast< std::uint8_t* >( call_instruction ) != 0xE8 )
                return 0;

            const auto displacement = *reinterpret_cast< std::int32_t* >( call_instruction + 1 );
            return call_instruction + sizeof( std::uint8_t ) + sizeof( std::int32_t ) + displacement;
        }

        std::uint64_t find_call_near( std::uint64_t address, std::uint64_t first_offset, std::uint64_t last_offset ) {
            if ( !address || first_offset > last_offset )
                return 0;

            for ( auto offset = first_offset; offset <= last_offset; offset++ ) {
                auto candidate = address + offset;
                if ( *reinterpret_cast< std::uint8_t* >( candidate ) == 0xE8 )
                    return candidate;
            }

            return 0;
        }

        bool resolve_uuid_valid_offset( std::uint64_t walk_start, std::uint32_t* uuid_valid_offset ) {
            if ( !walk_start || !uuid_valid_offset )
                return false;

            *uuid_valid_offset = 0;

            auto address = walk_start;
            for ( auto instruction_count = 0; instruction_count < 50; instruction_count++ ) {
                hde64s hde{ };
                hde64_disasm( reinterpret_cast< void* >( address ), &hde );

                if ( hde.flags & F_ERROR )
                    return false;

                const auto op = *reinterpret_cast< std::uint32_t* >( address ) & 0xFFFFFF;
                if ( op == 0x818D4C ) {
                    *uuid_valid_offset = *reinterpret_cast< std::uint32_t* >( address + 3 ) - 1;
                    return *uuid_valid_offset != 0;
                }

                address += hde.len;
            }

            return false;
        }

        bool scan( nvidia_scan_t* out ) {
            if ( !out )
                return false;

            out->m_module_base = 0;
            out->m_module_size = 0;
            out->m_signature = 0;
            out->m_gpu_mgr_get_gpu_from_id = 0;
            out->m_uuid_walk_start = 0;
            out->m_uuid_valid_offset = 0;

            out->m_module_base = module::find_base(
                obf( L"nvlddmkm.sys" ),
                &out->m_module_size
            );

            if ( !out->m_module_base ) {
                disk::print( obf( "no nvidia gpu module found" ) );
                return false;
            }

            out->m_signature = module::scan(
                out->m_module_base,
                out->m_module_size,
                m_nvidia_gpu_pattern,
                m_nvidia_gpu_mask
            );

            if ( !out->m_signature ) {
                disk::print( obf( "nvidia gpu signature not found" ) );
                return false;
            }

            out->m_gpu_mgr_get_gpu_from_id = resolve_relative_call( out->m_signature );
            if ( !out->m_gpu_mgr_get_gpu_from_id ) {
                disk::print( obf( "failed to resolve GpuMgrGetGpuFromId candidate" ) );
                return false;
            }

            const auto uuid_call = find_call_near( out->m_signature, 0x3B, 0x3C );
            if ( !uuid_call ) {
                disk::print( obf( "failed to find uuid offset walk call" ) );
                return false;
            }

            out->m_uuid_walk_start = resolve_relative_call( uuid_call );
            if ( !out->m_uuid_walk_start ) {
                disk::print( obf( "failed to resolve uuid offset walk target" ) );
                return false;
            }

            if ( !resolve_uuid_valid_offset( out->m_uuid_walk_start, &out->m_uuid_valid_offset ) ) {
                disk::print( obf( "failed to resolve uuid valid offset" ) );
                return false;
            }

            disk::print( obf( "nvlddmkm.sys -> 0x%llx, size -> 0x%llx" ),
                out->m_module_base, out->m_module_size );
            return true;
        }

        bool spoof_each( const nvidia_scan_t* scan ) {
            if ( !scan || !scan->m_gpu_mgr_get_gpu_from_id || !scan->m_uuid_valid_offset )
                return false;

            uint64_t( *gpu_mgr_get_gpu_from_id )( int ) = decltype( gpu_mgr_get_gpu_from_id )( scan->m_gpu_mgr_get_gpu_from_id );
            for ( auto gpu_idx = 0; gpu_idx < 32; gpu_idx++ ) {
                auto gpu = gpu_mgr_get_gpu_from_id( gpu_idx );
                if ( !gpu ) continue;
                if ( !*( char* )( gpu + scan->m_uuid_valid_offset ) )
                    continue;

                auto* old_uuid = reinterpret_cast< GUID* >( gpu + scan->m_uuid_valid_offset + 1 );
                disk::print( obf( "gpu [%d]: before -> {%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}" ),
                    gpu_idx,
                    old_uuid->Data1, old_uuid->Data2, old_uuid->Data3,
                    old_uuid->Data4[ 0 ], old_uuid->Data4[ 1 ],
                    old_uuid->Data4[ 2 ], old_uuid->Data4[ 3 ], old_uuid->Data4[ 4 ],
                    old_uuid->Data4[ 5 ], old_uuid->Data4[ 6 ], old_uuid->Data4[ 7 ] );

                auto xorshift32 = [ ]( ULONG x ) -> ULONG {
                    x ^= x << 13;
                    x ^= x >> 17;
                    x ^= x << 5;
                    return x;
                    };

                ULONGLONG t;
                KeQuerySystemTime( ( LARGE_INTEGER* )&t );
                ULONG seed = ( ULONG )t ^ ( ULONG )( t >> 32 );

                for ( int j = 0; j < ( int )sizeof( GUID ); j++ ) {
                    seed = xorshift32( seed );
                    *( uint8_t* )( gpu + scan->m_uuid_valid_offset + 1 + j ) = ( uint8_t )( seed >> 16 );
                }

                auto* new_uuid = reinterpret_cast< GUID* >( gpu + scan->m_uuid_valid_offset + 1 );
                disk::print( obf( "gpu [%d]: after -> {%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}" ),
                    gpu_idx,
                    new_uuid->Data1, new_uuid->Data2, new_uuid->Data3,
                    new_uuid->Data4[ 0 ], new_uuid->Data4[ 1 ],
                    new_uuid->Data4[ 2 ], new_uuid->Data4[ 3 ], new_uuid->Data4[ 4 ],
                    new_uuid->Data4[ 5 ], new_uuid->Data4[ 6 ], new_uuid->Data4[ 7 ] );
            }

            return true;
        }
    }

	bool spoof( ) {
        gpu::nvidia_scan_t nvidia{ };
        if ( !gpu::scan( &nvidia ) )
            return false;

        return gpu::spoof_each( &nvidia );
	}
}
