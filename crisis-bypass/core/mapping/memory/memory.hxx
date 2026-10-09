#pragma once
#include <mutex>

namespace shared {
	class c_shared {
	public:
		~c_shared( ) { close( ); }

		auto open( ) -> bool {
			for ( auto attempt = 0; attempt < 50; ++attempt ) {
				if ( m_channel.open( L"Global\\", obf( L"crisis.memory" ), 0x1 ) )
					break;
				Sleep( 100 );
			}

			if ( !m_channel.valid( ) ) {
			logging::print( obf( "[memory] open failed gle=0x%x name_hash=0x%llx" ), m_channel.last_error( ), m_channel.name_hash( ) );
			return false;
		}
			if ( !m_channel.alive( ) ) {
			logging::print( obf( "[memory] stale channel name_hash=0x%llx" ), m_channel.name_hash( ) );
			m_channel.close( );
			return false;
		}

			m_state = m_channel.section( );
			return true;
		}

		auto is_connected( ) const -> bool {
			return m_channel.valid( ) && m_state;
		}

		auto close( ) -> void {
			m_channel.close( );
			m_state = nullptr;
		}

		auto read( std::uint64_t addr, void* buffer, std::size_t size ) -> bool {
			if ( !buffer || !size || size > packet_payload_size ) return false;
			return request( module_command::read_memory, addr, buffer, size );
		}

		auto write( std::uint64_t addr, void* buffer, std::size_t size ) -> bool {
			if ( !buffer || !size || size > packet_payload_size ) return false;
			return request( module_command::write_memory, addr, buffer, size );
		}

		auto map( std::uint64_t pa, void* va, std::size_t size ) -> bool {
			return request( module_command::map_memory, pa, va, size );
		}

		auto query_mapping( mapping_info_t& info ) -> bool {
			return request( module_command::query_mapping, 0, &info, sizeof( info ) );
		}

	private:
		transport::channel_t m_channel { };
		shared_state_t*      m_state   = nullptr;
		std::mutex      m_mutex { };
		std::atomic< std::uint64_t > m_request_count { 0 };
		std::atomic< std::uint64_t > m_mutex_wait_us { 0 };
		std::atomic< std::uint64_t > m_completion_wait_us { 0 };
		std::atomic< std::uint64_t > m_timeout_count { 0 };
		std::atomic< std::uint64_t > m_last_metrics_tick { 0 };

		static void adaptive_wait( std::uint32_t& attempts ) {
			if ( attempts++ < 4096 )
				YieldProcessor( );
			else
				SwitchToThread( );
		}

		void report_metrics( ) {
			const auto now = GetTickCount64( );
			auto last = m_last_metrics_tick.load( std::memory_order_relaxed );
			if ( now - last < 5000 ||
				!m_last_metrics_tick.compare_exchange_strong( last, now, std::memory_order_relaxed ) )
				return;

			m_request_count.exchange( 0, std::memory_order_relaxed );
			m_mutex_wait_us.exchange( 0, std::memory_order_relaxed );
			m_completion_wait_us.exchange( 0, std::memory_order_relaxed );
			m_timeout_count.exchange( 0, std::memory_order_relaxed );
		}

		auto request( module_command command, std::uint64_t addr, void* buffer, std::size_t size ) -> bool {
			if ( !m_state ) return false;

			if ( command != module_command::map_memory && size > packet_payload_size )
				return false;

			const auto mutex_start = std::chrono::steady_clock::now( );
			std::unique_lock<std::mutex> lock( m_mutex );
			m_mutex_wait_us.fetch_add( static_cast< std::uint64_t >(
				std::chrono::duration_cast< std::chrono::microseconds >(
					std::chrono::steady_clock::now( ) - mutex_start ).count( ) ),
				std::memory_order_relaxed );
			m_request_count.fetch_add( 1, std::memory_order_relaxed );

			const auto deadline   = GetTickCount64( ) + 1000;
			auto       write_index = m_state->write_index;

			while ( write_index - m_state->read_index >= capacity ) {
				if ( GetTickCount64( ) >= deadline ) {
					m_timeout_count.fetch_add( 1, std::memory_order_relaxed );
					report_metrics( );
					return false;
				}
				m_channel.wait( 1 );
				write_index = m_state->write_index;
			}

			auto& packet = m_state->packets[ write_index & capacity_mask ];
			while ( packet.status == packet_pending || packet.status == packet_abandoned ) {
				if ( GetTickCount64( ) >= deadline ) {
					m_timeout_count.fetch_add( 1, std::memory_order_relaxed );
					report_metrics( );
					return false;
				}
				m_channel.wait( 1 );
			}

			packet.command  = static_cast< std::uint32_t >( command );
			packet.addr     = addr;
			packet.buffer   = reinterpret_cast< std::uint64_t >( buffer );
			packet.size     = size;
			packet.sequence = write_index;
			packet.result   = 0;

			if ( command == module_command::write_memory )
				std::memcpy( packet.payload, buffer, size );

			MemoryBarrier( );
			InterlockedExchange( const_cast< LONG* >( &packet.status ), packet_pending );
			InterlockedExchange64( const_cast< LONG64* >( &m_state->write_index ), write_index + 1 );
			m_channel.signal( );

			const auto completion_start = std::chrono::steady_clock::now( );
			std::uint32_t wait_attempts = 0;
			while ( packet.status != packet_complete || packet.sequence != write_index ) {
				if ( GetTickCount64( ) >= deadline ) {
					if ( InterlockedCompareExchange( const_cast< LONG* >( &packet.status ),
							packet_abandoned, packet_pending ) == packet_pending )
						m_channel.signal( );
					m_timeout_count.fetch_add( 1, std::memory_order_relaxed );
					report_metrics( );
					return false;
				}
				if ( wait_attempts < transport::k_spin_iters ) {
					YieldProcessor( );
					++wait_attempts;
				}
				else {
					m_channel.wait( 1 );
				}
			}
			m_completion_wait_us.fetch_add( static_cast< std::uint64_t >(
				std::chrono::duration_cast< std::chrono::microseconds >(
					std::chrono::steady_clock::now( ) - completion_start ).count( ) ),
				std::memory_order_relaxed );

			const auto result = packet.result != 0;
			if ( result && ( command == module_command::read_memory ||
				command == module_command::query_mapping ) )
				std::memcpy( buffer, packet.payload, size );

			InterlockedExchange( const_cast< LONG* >( &packet.status ), packet_empty );
			report_metrics( );
			return result;
		}
	};
}
