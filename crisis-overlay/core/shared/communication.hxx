#pragma once

namespace communication {
    class c_communication {
    public:
        bool setup( ) {
            if ( m_worker_thread )
                return true;

            if ( !m_channel.create( obf( L"Local\\" ), obf( L"crisis.overlay" ), 0x2,
                     transport::k_sddl_session ) ) {
                utility::print( "failed to create overlay mapping\n" );
                return false;
            }

            m_state = m_channel.section( );

            m_worker_thread = utility::thread::create( thread_entry, this );
            if ( !m_worker_thread ) {
                utility::print( "failed to create communication thread: %lu\n", GetLastError( ) );
                m_channel.close( );
                m_state = nullptr;
                return false;
            }

            SetThreadPriority( m_worker_thread, THREAD_PRIORITY_ABOVE_NORMAL );

            InterlockedExchange( const_cast< LONG* >( &m_state->ready ), 1 );
            m_channel.beat( );
            return true;
        }

        void shutdown( ) {
            if ( m_state )
                InterlockedExchange( const_cast< LONG* >( &m_state->stop ), 1 );

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

            m_state = nullptr;
        }

    private:
        transport::channel_t  m_channel{ };
        transport::section_t* m_state = nullptr;
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
            auto* section = m_state;
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

                void* command_buffer = nullptr;
                if ( packet.size ) {
                    if ( packet.size > transport::k_payload ) {
                        packet.result = 0;
                        MemoryBarrier( );
                        InterlockedExchange( const_cast< LONG* >( &packet.status ), transport::packet_complete );
                        InterlockedIncrement64( const_cast< LONG64* >( &section->read_index ) );
                        continue;
                    }

                    command_buffer = packet.payload;
                }

                const auto handled = commands::dispatch( {
                    static_cast< command_id >( packet.command ),
                    command_buffer,
                    static_cast< std::size_t >( packet.size )
                } );

                packet.result = handled ? 1 : 0;
                MemoryBarrier( );
                InterlockedExchange( const_cast< LONG* >( &packet.status ), transport::packet_complete );
                InterlockedIncrement64( const_cast< LONG64* >( &section->read_index ) );

                if ( section->read_index == section->write_index )
                    m_channel.signal( );
            }

            return 0;
        }
    };
}
