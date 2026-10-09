#pragma once

namespace communication {
    class c_communication {
    public:
        bool setup( ) {
            if ( m_worker_thread )
                return true;

            if ( !m_channel.create( obf( L"Global\\" ), obf( L"crisis.memory" ), 0x1,
                     transport::k_sddl_global ) ) {
                utility::print( "memory channel create failed: gle=0x%x stage=%u security_ok=%d",
                    m_channel.last_error( ), m_channel.last_stage( ),
                    m_channel.security_ok( ) ? 1 : 0 );
                return false;
            }

            utility::print( "memory channel ready: name_hash=0x%llx security_ok=%d",
                m_channel.name_hash( ), m_channel.security_ok( ) ? 1 : 0 );

            m_section = m_channel.section( );

            m_worker_thread = utility::thread::create( thread_entry, this );
            if ( !m_worker_thread ) {
                m_channel.close( );
                m_section = nullptr;
                return false;
            }

            SetThreadPriority( m_worker_thread, THREAD_PRIORITY_ABOVE_NORMAL );

            InterlockedExchange( const_cast< LONG* >( &m_section->ready ), 1 );
            m_channel.beat( );

			utility::print( "memory framework initialized" );
            return true;
        }

        void shutdown( ) {
            if ( m_section )
                InterlockedExchange( const_cast< LONG* >( &m_section->stop ), 1 );

            m_channel.signal( );

            if ( m_worker_thread ) {
                for ( auto waited = 0u;
                      waited < 5000 && InterlockedCompareExchange( &m_exited, 0, 0 ) == 0;
                      waited += 10 )
                    Sleep( 10 );

                if ( InterlockedCompareExchange( &m_exited, 0, 0 ) )
                    CloseHandle( m_worker_thread );

                m_worker_thread = nullptr;
            }

            if ( InterlockedCompareExchange( &m_exited, 0, 0 ) )
                m_channel.close( );
            else
                m_channel.abandon( );

            m_section = nullptr;
        }

    private:
        transport::channel_t  m_channel{ };
        transport::section_t* m_section = nullptr;
        HANDLE                m_worker_thread = nullptr;
        volatile LONG         m_exited = 0;

        static DWORD WINAPI thread_entry( void* context ) {
            auto self = reinterpret_cast< c_communication* >( context );
            if ( !self )
                return 0;

            const auto result = self->worker( );
            InterlockedExchange( &self->m_exited, 1 );
            return result;
        }

        DWORD worker( ) {
            auto* section = m_section;
            if ( !section )
                return 0;

            while ( !section->stop ) {
                m_channel.beat( );

                const auto read_index = static_cast< std::uint64_t >( section->read_index );
                const auto write_index = static_cast< std::uint64_t >( section->write_index );

                if ( read_index == write_index ) {
                    m_channel.wait( transport::k_wait_ms );
                    continue;
                }

                auto& packet = section->packets[ read_index & transport::k_capacity_mask ];

                if ( packet.status == transport::packet_abandoned ) {
                    InterlockedExchange( const_cast< LONG* >( &packet.status ), transport::packet_empty );
                    InterlockedIncrement64( const_cast< LONG64* >( &section->read_index ) );
                    continue;
                }

                if ( packet.status != transport::packet_pending ||
                     packet.sequence != read_index ) {
                    YieldProcessor( );
                    continue;
                }

                const auto command = static_cast< command_id >( packet.command );
                void* command_buffer = reinterpret_cast< void* >( packet.buffer );

                if ( command == command_id::read_memory ||
                     command == command_id::write_memory ||
                     command == command_id::query_mapping )
                    command_buffer = packet.payload;

                if ( ( command == command_id::read_memory ||
                       command == command_id::write_memory ||
                       command == command_id::query_mapping ) &&
                     packet.size > transport::k_payload ) {
                    packet.result = 0;
                    MemoryBarrier( );
                    InterlockedExchange( const_cast< LONG* >( &packet.status ), transport::packet_complete );
                    InterlockedIncrement64( const_cast< LONG64* >( &section->read_index ) );
                    continue;
                }

                const auto handled = commands::dispatch( {
                    command,
                    packet.addr,
                    command_buffer,
                    static_cast< std::size_t >( packet.size )
                } );

                packet.result = handled ? 1 : 0;
                MemoryBarrier( );
                InterlockedExchange( const_cast< LONG* >( &packet.status ), transport::packet_complete );
                InterlockedIncrement64( const_cast< LONG64* >( &section->read_index ) );
            }

            return 0;
        }
    };
}
