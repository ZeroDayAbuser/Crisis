#pragma once

namespace module {
    bool c_memory::open( ) {
        if ( m_state )
            return true;

        for ( auto attempt = 0; attempt < 50; ++attempt ) {
            if ( m_channel.open( L"Global\\", obf( L"crisis.memory" ), 0x1 ) )
                break;
            Sleep( 100 );
        }

        if ( !m_channel.valid( ) )
            return false;

        if ( !m_channel.alive( ) ) {
            m_channel.close( );
            return false;
        }

        m_state = m_channel.section( );
        return true;
    }

    void c_memory::close( ) {
        m_channel.close( );
        m_state = nullptr;
    }

    bool c_memory::request( memory_request_type command, std::uint64_t addr,
        void* buffer, std::size_t size ) {
        if ( !m_state || ( command != memory_request_type::map_memory && size > k_payload_size ) )
            return false;

        std::lock_guard<std::mutex> lock( m_mutex );

        const auto deadline = GetTickCount64( ) + 1000;
        auto write_idx = static_cast< std::uint64_t >( m_state->write_index );

        while ( write_idx - static_cast< std::uint64_t >( m_state->read_index ) >=
                transport::k_capacity ) {
            if ( GetTickCount64( ) >= deadline )
                return false;
            Sleep( 0 );
            write_idx = static_cast< std::uint64_t >( m_state->write_index );
        }

        auto& packet = m_state->packets[ write_idx & transport::k_capacity_mask ];
        while ( packet.status == transport::packet_pending ||
                packet.status == transport::packet_abandoned ) {
            if ( GetTickCount64( ) >= deadline )
                return false;
            Sleep( 0 );
        }

        packet.command = static_cast< std::uint32_t >( command );
        packet.addr = addr;
        packet.buffer = reinterpret_cast< std::uint64_t >( buffer );
        packet.size = size;
        packet.sequence = write_idx;
        packet.result = 0;

        if ( command == memory_request_type::write_memory && buffer && size )
            std::memcpy( packet.payload, buffer, size );

        MemoryBarrier( );

        InterlockedExchange( const_cast< LONG* >( &packet.status ), transport::packet_pending );
        InterlockedExchange64( const_cast< LONG64* >( &m_state->write_index ),
            static_cast< LONG64 >( write_idx + 1 ) );
        m_channel.signal( );

        std::uint32_t spin = 0;
        LONG current = InterlockedCompareExchange(
            const_cast< LONG* >( &packet.status ), 0, 0 );
        while ( current != transport::packet_complete ) {
            if ( GetTickCount64( ) >= deadline ) {
                if ( InterlockedCompareExchange( const_cast< LONG* >( &packet.status ),
                        transport::packet_abandoned, transport::packet_pending ) ==
                     transport::packet_pending )
                    m_channel.signal( );
                return false;
            }

            if ( spin < transport::k_spin_iters ) {
                YieldProcessor( );
                ++spin;
            }
            else {
                m_channel.wait( 1 );
            }

            current = InterlockedCompareExchange(
                const_cast< LONG* >( &packet.status ), 0, 0 );
        }

        const auto ok = packet.result != 0;
        if ( ok && buffer && size && ( command == memory_request_type::read_memory ||
            command == memory_request_type::query_mapping ) )
            std::memcpy( buffer, packet.payload, size );

        InterlockedExchange( const_cast< LONG* >( &packet.status ), transport::packet_empty );
        return ok;
    }

    bool c_memory::ping( ) {
        if ( !open( ) ) {
            logging::print( obf( "failed to open memory module" ) );
            logging::print( obf( "restart your computer and try again." ) );
            return false;
        }

        mapping_info_t info{ };
        if ( !request( memory_request_type::query_mapping, 0, &info, sizeof( info ) ) ) {
            logging::print( obf( "memory module did not respond" ) );
            logging::print( obf( "restart your computer and try again." ) );
            return false;
        }

        std::uint64_t mapped_total = 0;
        for ( auto i = 0ull; i < info.range_count && i < info.max_ranges; ++i )
            mapped_total += info.ranges[ i ].size;

        const auto mapped_ratio = static_cast< double >( mapped_total ) / ( 1024.0 * 1024.0 * 1024.0 );
        if ( !mapped_ratio ) {
            logging::print( obf( "failed to retrieve mapped memory" ) );
            logging::print( obf( "restart your computer and try again." ) );
            return false;
        }

        logging::print( obf( "mapped memory: %.2f GB" ), mapped_ratio );
        return true;
    }

    void c_memory::benchmark( ) {
        auto read_physical = g_paging->translate_linear( g_pdb->m_module_base );
        auto read_buffer = 0ull;
        auto ops = 0ull;

        for ( auto i = 0; i < 10000; i++ )
            read( read_physical, &read_buffer, sizeof( std::uint64_t ) );

        auto start = std::chrono::steady_clock::now( );
        auto deadline = start + std::chrono::seconds( 1 );

        while ( std::chrono::steady_clock::now( ) < deadline ) {
            for ( auto i = 0; i < 1000; i++ ) {
                if ( read( read_physical, &read_buffer, sizeof( std::uint64_t ) ) )
                    ++ops;
            }
        }

        auto elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now( ) - start ).count( );
        auto rps = static_cast< std::uint64_t >(
            static_cast< double >( ops ) / elapsed );

        logging::print( obf( "performance: %s ops/s in %.fs\n" ),
            utility::add_commas( rps ).c_str( ), elapsed );
    }

    bool c_memory::read( std::uint64_t addr, void* buffer, std::size_t size ) {
        if ( !buffer || !size || size > k_payload_size )
            return false;

        return request( memory_request_type::read_memory, addr, buffer, size );
    }

    bool c_memory::write( std::uint64_t addr, const void* buffer, std::size_t size ) {
        if ( !buffer || !size || size > k_payload_size )
            return false;

        return request( memory_request_type::write_memory, addr,
            const_cast< void* >( buffer ), size );
    }

    bool c_memory::map( std::uint64_t pa, void* va, std::size_t size ) {
        return request( memory_request_type::map_memory, pa, va, size );
    }

    bool c_memory::query_mapping( mapping_info_t& info ) {
        return request( memory_request_type::query_mapping, 0, &info, sizeof( info ) );
    }

    bool c_overlay::open( ) {
        if ( m_state )
            return true;

        for ( auto attempt = 0; attempt < 50; ++attempt ) {
            if ( m_channel.open( L"Local\\", obf( L"crisis.overlay" ), 0x2 ) )
                break;
            Sleep( 100 );
        }

        if ( !m_channel.valid( ) )
            return false;

        if ( !m_channel.alive( ) ) {
            m_channel.close( );
            return false;
        }

        m_state = m_channel.section( );
        return true;
    }

    bool c_overlay::set_monitor( std::uint32_t index ) {
        set_monitor_request_t request_data{ index };
        return request( overlay_request_type::render_set_monitor, &request_data, sizeof( request_data ) );
    }

    bool c_overlay::bring_to_top( ) {
        return request( overlay_request_type::render_bring_to_top, nullptr, 0 );
    }

    bool c_overlay::minimize( ) {
        return request( overlay_request_type::render_minimize, nullptr, 0 );
    }

    void c_overlay::close( ) {
        m_channel.close( );
        m_state = nullptr;
    }

    bool c_overlay::request( overlay_request_type command, void* buffer, std::size_t size ) {
        if ( !m_state || size > k_payload_size )
            return false;

        std::lock_guard<std::mutex> lock( m_mutex );

        const auto deadline = GetTickCount64( ) + 1000;
        auto write_idx = static_cast< std::uint64_t >( m_state->write_index );

        while ( write_idx - static_cast< std::uint64_t >( m_state->read_index ) >=
                transport::k_capacity ) {
            if ( GetTickCount64( ) >= deadline ) return false;
            Sleep( 0 );
            write_idx = static_cast< std::uint64_t >( m_state->write_index );
        }

        auto& packet = m_state->packets[ write_idx & transport::k_capacity_mask ];
        while ( packet.status == transport::packet_pending ||
                packet.status == transport::packet_abandoned ) {
            if ( GetTickCount64( ) >= deadline ) return false;
            Sleep( 0 );
        }

        packet.command = static_cast< std::uint32_t >( command );
        packet.addr = 0;
        packet.buffer = 0;
        packet.size = size;
        packet.sequence = write_idx;
        packet.result = 0;

        if ( buffer && size )
            std::memcpy( packet.payload, buffer, size );

        MemoryBarrier( );

        InterlockedExchange( const_cast< LONG* >( &packet.status ), transport::packet_pending );
        InterlockedExchange64( const_cast< LONG64* >( &m_state->write_index ),
            static_cast< LONG64 >( write_idx + 1 ) );
        m_channel.signal( );

        std::uint32_t spin = 0;
        LONG current = InterlockedCompareExchange( const_cast< LONG* >( &packet.status ), 0, 0 );
        while ( current != transport::packet_complete ) {
            if ( GetTickCount64( ) >= deadline ) {
                if ( InterlockedCompareExchange( const_cast< LONG* >( &packet.status ),
                        transport::packet_abandoned, transport::packet_pending ) ==
                     transport::packet_pending )
                    m_channel.signal( );
                return false;
            }

            if ( spin < transport::k_spin_iters ) {
                YieldProcessor( );
                ++spin;
            }
            else {
                m_channel.wait( 1 );
            }

            current = InterlockedCompareExchange( const_cast< LONG* >( &packet.status ), 0, 0 );
        }

        const auto ok = packet.result != 0;
        if ( ok && buffer && size )
            std::memcpy( buffer, packet.payload, size );

        InterlockedExchange( const_cast< LONG* >( &packet.status ), transport::packet_empty );
        return ok;
    }

    bool c_overlay::ping( ) {
        if ( !open( ) ) {
            logging::print( obf( "failed to open overlay module." ) );
            logging::print( obf( "restart your computer and try again." ) );
            return false;
        }

        size_response_t size_resp{ };
        if ( !request( overlay_request_type::render_get_size, &size_resp, sizeof( size_resp ) ) ) {
            logging::print( obf( "overlay module did not respond" ) );
            logging::print( obf( "restart your computer and try again." ) );
            return false;
        }

        logging::print( obf( "connected to overlay module." ) );

        using clock_t = std::chrono::steady_clock;
        std::uint64_t ops = 0;
        std::uint32_t failed = 0;
        const auto probe_start = clock_t::now( );
        const auto probe_deadline = probe_start + std::chrono::seconds( 1 );
        while ( clock_t::now( ) < probe_deadline ) {
            for ( int i = 0; i < 1000; ++i ) {
                size_response_t probe_resp{ };
                if ( request( overlay_request_type::render_get_size,
                        &probe_resp, sizeof( probe_resp ) ) )
                    ++ops;
                else
                    ++failed;
            }
        }
        const auto probe_elapsed = std::chrono::duration< double >(
            clock_t::now( ) - probe_start ).count( );

        if ( !ops ) {
            logging::print( obf( "overlay module did not respond" ) );
            logging::print( obf( "restart your computer and try again." ) );
            return false;
        }

        const auto ops_per_sec = static_cast< std::uint64_t >(
            static_cast< double >( ops ) / probe_elapsed );
        logging::print(
            obf( "performance: %s ops/s in %.fs" ),
            utility::add_commas( ( unsigned long long )ops_per_sec ).c_str( ),
            probe_elapsed );

        return true;
    }
}
