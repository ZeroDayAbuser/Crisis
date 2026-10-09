#pragma once

namespace communication {
    namespace commands {
#define QUERY_MAPPING_DBG( fmt, ... )                                                   \
    do {                                                                                \
        char query_mapping_msg[ 512 ]{};                                                \
        sprintf_s( query_mapping_msg, "[crisis-module] query_mapping: " fmt "\n", __VA_ARGS__ ); \
        OutputDebugStringA( query_mapping_msg );                                       \
    } while ( false )

        bool map_memory( std::uint64_t addr, void* buffer, std::size_t size ) {
            if ( !g_memory ) return false;
            if ( !buffer || !size ) return false;
            return g_memory->map_range( { addr, size },
                reinterpret_cast< std::uint64_t >( buffer ) );
        }

        bool read_memory( std::uint64_t addr, void* buffer, std::size_t size ) {
            if ( !g_memory ) return false;
            if ( !addr || !buffer || !size ) return false;

            return g_memory->read( addr, buffer, size );
        }

        bool write_memory( std::uint64_t addr, void* buffer, std::size_t size ) {
            if ( !g_memory ) return false;
            if ( !addr || !buffer || !size ) return false;

            return g_memory->write( addr, buffer, size );
        }

        bool query_mapping( void* buffer, std::size_t size ) {
            QUERY_MAPPING_DBG( "enter buffer=0x%p size=0x%llx required=0x%llx",
                buffer,
                static_cast< unsigned long long >( size ),
                static_cast< unsigned long long >( sizeof( mapping_info_t ) ) );

            if ( !g_memory ) {
                QUERY_MAPPING_DBG( "failed: g_memory is null buffer=0x%p size=0x%llx",
                    buffer,
                    static_cast< unsigned long long >( size ) );
                return false;
            }

            if ( !buffer ) {
                QUERY_MAPPING_DBG( "failed: buffer is null size=0x%llx",
                    static_cast< unsigned long long >( size ) );
                return false;
            }

            if ( size < sizeof( mapping_info_t ) ) {
                QUERY_MAPPING_DBG( "failed: buffer too small size=0x%llx required=0x%llx",
                    static_cast< unsigned long long >( size ),
                    static_cast< unsigned long long >( sizeof( mapping_info_t ) ) );
                return false;
            }

            auto info = reinterpret_cast< mapping_info_t* >( buffer );
            info->physical_mask = g_memory->physical_mask( );
            info->max_ranges = memory::c_mapped_memory::max_ranges;
            info->range_count = g_memory->range_count( );

            const auto count = ( info->range_count < max_mapping_ranges )
                ? info->range_count
                : max_mapping_ranges;

            QUERY_MAPPING_DBG( "summary physical_mask=0x%llx max_ranges=%llu range_count=%llu copied=%llu",
                static_cast< unsigned long long >( info->physical_mask ),
                static_cast< unsigned long long >( info->max_ranges ),
                static_cast< unsigned long long >( info->range_count ),
                static_cast< unsigned long long >( count ) );

            if ( info->range_count > max_mapping_ranges ) {
                QUERY_MAPPING_DBG( "warning: range_count truncated from %llu to %llu",
                    static_cast< unsigned long long >( info->range_count ),
                    static_cast< unsigned long long >( max_mapping_ranges ) );
            }

            for ( auto idx = 0ull; idx < count; ++idx ) {
                const auto range = g_memory->range_at( static_cast< std::size_t >( idx ) );
                info->ranges[ idx ] = {
                    range.base_pa,
                    range.size,
                    range.va_base
                };

                QUERY_MAPPING_DBG( "range[%llu] base_pa=0x%llx size=0x%llx va_base=0x%llx",
                    static_cast< unsigned long long >( idx ),
                    static_cast< unsigned long long >( range.base_pa ),
                    static_cast< unsigned long long >( range.size ),
                    static_cast< unsigned long long >( range.va_base ) );
            }

            QUERY_MAPPING_DBG( "success buffer=0x%p copied=%llu",
                buffer,
                static_cast< unsigned long long >( count ) );

            return true;
        }

        bool dispatch( const command_t& command ) {
            switch ( command.id ) {
                case command_id::map_memory:
                    return map_memory( command.addr, command.buffer, command.size );
                case command_id::read_memory:
                    return read_memory( command.addr, command.buffer, command.size );
                case command_id::write_memory:
                    return write_memory( command.addr, command.buffer, command.size );
                case command_id::query_mapping:
                    return query_mapping( command.buffer, command.size );
                default:
                    return false;
            }
        }

#undef QUERY_MAPPING_DBG
    }
}

extern "C" __declspec( dllexport ) bool __stdcall dispatch_command(
    std::uint32_t command,
    std::uint64_t addr,
    void* buffer,
    std::size_t   size ) {
    return communication::commands::dispatch( {
        static_cast< communication::command_id >( command ),
        addr,
        buffer,
        size
        } );
}
