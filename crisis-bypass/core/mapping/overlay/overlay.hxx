#pragma once
#include <algorithm>

namespace overlay {
	class c_overlay {
	public:
		~c_overlay( ) { close( ); }

		auto open( ) -> bool {
			for ( auto attempt = 0; attempt < 50; ++attempt ) {
				if ( m_channel.open( L"Local\\", obf( L"crisis.overlay" ), 0x2 ) )
					break;
				Sleep( 100 );
			}
			if ( !m_channel.valid( ) ) return false;
			if ( !m_channel.alive( ) ) { m_channel.close( ); return false; }

			m_state = m_channel.section( );
			m_pending.reserve( packet_payload_size );

			m_handshake_done = false;
			m_batching_enabled = false;
			return true;
		}

		auto is_connected( ) const -> bool { return m_channel.valid( ) && m_state; }

		auto close( ) -> void {
			m_channel.close( );
			m_state = nullptr;
			m_registered_fonts.clear( );
			m_registered_images.clear( );
			m_pending.clear( );
			m_handshake_done = false;
			m_batching_enabled = false;
		}

		auto clear( ) -> bool {
			return buffer_or_send( command_id::render_clear, nullptr, 0 );
		}

		auto present( ) -> bool {
			if ( !m_state ) return false;
			ensure_handshake( );
			if ( !m_batching_enabled )
				return send_control( command_id::render_present, nullptr, 0 );

			std::lock_guard< std::mutex > lock( m_mutex );
			if ( !buffer_record_locked( command_id::render_present, nullptr, 0 ) )
				return false;
			return flush_locked( );
		}

		auto minimize( )     -> bool { return sync_op( command_id::render_minimize,     nullptr, 0 ); }
		auto bring_to_top( ) -> bool { return sync_op( command_id::render_bring_to_top, nullptr, 0 ); }

		auto set_monitor( std::uint32_t index ) -> bool {
			set_monitor_request_t r { index };
			return sync_op( command_id::render_set_monitor, &r, sizeof( r ) );
		}

		auto get_size( std::int32_t& width, std::int32_t& height ) -> bool {
			size_response_t r {};
			if ( !sync_op_with_reply( command_id::render_get_size, &r, sizeof( r ) ) ) return false;
			width = r.width; height = r.height;
			return true;
		}

		auto line( point_t from, point_t to, color_t color, float thickness = 1.0f ) -> bool {
			line_request_t r { from, to, color, thickness };
			return buffer_or_send( command_id::render_line, &r, sizeof( r ) );
		}
		auto rectangle( rect_t area, color_t color, float thickness = 1.0f ) -> bool {
			rectangle_request_t r { area, color, thickness };
			return buffer_or_send( command_id::render_rectangle, &r, sizeof( r ) );
		}
		auto filled_rectangle( rect_t area, color_t color ) -> bool {
			rectangle_request_t r { area, color, 0.0f };
			return buffer_or_send( command_id::render_filled_rectangle, &r, sizeof( r ) );
		}
		auto circle( point_t center, float radius, color_t color, float thickness = 1.0f ) -> bool {
			circle_request_t r { center, radius, color, thickness };
			return buffer_or_send( command_id::render_circle, &r, sizeof( r ) );
		}
		auto filled_circle( point_t center, float radius, color_t color ) -> bool {
			circle_request_t r { center, radius, color, 0.0f };
			return buffer_or_send( command_id::render_filled_circle, &r, sizeof( r ) );
		}
		auto text(
			const wchar_t* value, rect_t area, color_t color,
			float size = 14.0f, text_weight_t weight = text_weight_t::bold,
			const wchar_t* font = nullptr, text_align_t alignment = text_align_t::left,
			bool wrap = false ) -> bool {
			if ( !value ) return false;
			text_request_t r {};
			wcsncpy_s( r.value, value, _TRUNCATE );
			wcsncpy_s( r.font, ( font && *font ) ? font : L"Segoe UI", _TRUNCATE );
			r.area = area; r.color = color; r.size = size;
			r.weight = weight; r.alignment = alignment; r.wrap = wrap;
			return buffer_or_send( command_id::render_text, &r, sizeof( r ) );
		}
		auto interactive_area( rect_t area ) -> bool {
			return buffer_or_send( command_id::render_interactive_area, &area, sizeof( area ) );
		}
		auto rounded_rectangle( rect_t area, float radius, color_t color, float thickness = 1.0f ) -> bool {
			rounded_rectangle_request_t r { area, color, radius, thickness };
			return buffer_or_send( command_id::render_rounded_rectangle, &r, sizeof( r ) );
		}
		auto filled_rounded_rectangle( rect_t area, float radius, color_t color ) -> bool {
			rounded_rectangle_request_t r { area, color, radius, 0.0f };
			return buffer_or_send( command_id::render_filled_rounded_rectangle, &r, sizeof( r ) );
		}
		auto aurora( rect_t area, float corner_radius, float sigma,
			float grain_strength, const aurora_blob_t* blobs, std::uint32_t blob_count ) -> bool {
			aurora_request_t r {};
			r.area = area; r.corner_radius = corner_radius; r.sigma = sigma;
			r.grain_strength = grain_strength;
			r.blob_count = ( std::min )( blob_count, static_cast<std::uint32_t>( max_aurora_blobs ) );
			for ( std::uint32_t i = 0; i < r.blob_count; ++i ) r.blobs[ i ] = blobs[ i ];
			return buffer_or_send( command_id::render_aurora, &r, sizeof( r ) );
		}
		auto frosted_panel( rect_t area, float corner_radius, float sigma, color_t tint ) -> bool {
			frosted_panel_request_t r { area, corner_radius, sigma, tint };
			return buffer_or_send( command_id::render_frosted_panel, &r, sizeof( r ) );
		}
		auto gradient_rectangle( rect_t area, color_t color_a, color_t color_b, float corner_radius = 0.0f ) -> bool {
			gradient_rectangle_request_t r { area, color_a, color_b, corner_radius };
			return buffer_or_send( command_id::render_gradient_rectangle, &r, sizeof( r ) );
		}
		auto gradient_rectangle_h( rect_t area, color_t color_a, color_t color_b, float corner_radius = 0.0f ) -> bool {
			gradient_rectangle_request_t r { area, color_a, color_b, corner_radius };
			return buffer_or_send( command_id::render_gradient_rectangle_h, &r, sizeof( r ) );
		}
		auto push_clip( rect_t area ) -> bool {
			return buffer_or_send( command_id::render_push_clip, &area, sizeof( area ) );
		}
		auto pop_clip( ) -> bool {
			return buffer_or_send( command_id::render_pop_clip, nullptr, 0 );
		}
		auto image( std::uint32_t id, rect_t area, color_t tint,
			float opacity, float corner_radius ) -> bool {
			if ( !id ) return false;
			image_request_t r {};
			r.id = id; r.area = area; r.tint = tint; r.opacity = opacity; r.corner_radius = corner_radius;
			return buffer_or_send( command_id::render_image, &r, sizeof( r ) );
		}

		auto scene3d( rect_t area, color_t tint,
			float rotation_x, float rotation_y, float rotation_z,
			float opacity = 1.0f ) -> bool {
			scene3d_request_t r {};
			r.area       = area;
			r.tint       = tint;
			r.rotation_x = rotation_x;
			r.rotation_y = rotation_y;
			r.rotation_z = rotation_z;
			r.opacity    = opacity;
			return buffer_or_send( command_id::render_scene3d, &r, sizeof( r ) );
		}

		auto register_font( const wchar_t* name, const wchar_t* path ) -> bool {
			if ( !name || !*name || !path || !*path ) return false;
			std::wstring key( name );
			for ( auto& ch : key ) ch = static_cast< wchar_t >( ::towlower( ch ) );
			if ( !m_registered_fonts.insert( key ).second ) return false;

			font_register_request_t r {};
			wcsncpy_s( r.name, name, _TRUNCATE );
			wcsncpy_s( r.path, path, _TRUNCATE );
			const auto ok = sync_op( command_id::render_register_font, &r, sizeof( r ) );
			if ( !ok ) m_registered_fonts.erase( key );
			return ok;
		}

		auto register_image( std::uint32_t id, const wchar_t* path ) -> bool {
			if ( !id || !path || !*path ) return false;
			if ( !m_registered_images.insert( id ).second ) return true;

			image_register_request_t r {}; r.id = id;
			wcsncpy_s( r.path, path, _TRUNCATE );
			const auto ok = sync_op( command_id::render_register_image, &r, sizeof( r ) );
			if ( !ok ) m_registered_images.erase( id );
			return ok;
		}

		struct monitor_info_t { RECT bounds {}; bool is_primary { false }; };

		static auto enumerate_monitors( ) -> std::vector< monitor_info_t > {
			std::vector< monitor_info_t > monitors;
			EnumDisplayMonitors( nullptr, nullptr,
				[ ]( HMONITOR h, HDC, LPRECT, LPARAM lp ) -> BOOL {
					auto* list = reinterpret_cast< std::vector< monitor_info_t >* >( lp );
					MONITORINFO info { sizeof( info ) };
					if ( !GetMonitorInfoW( h, &info ) ) return TRUE;
					monitor_info_t e {};
					e.bounds = info.rcMonitor;
					e.is_primary = ( info.dwFlags & MONITORINFOF_PRIMARY ) != 0;
					list->push_back( e );
					return TRUE;
				}, reinterpret_cast< LPARAM >( &monitors ) );

			std::sort( monitors.begin( ), monitors.end( ),
				[ ]( const monitor_info_t& a, const monitor_info_t& b ) {
					if ( a.is_primary != b.is_primary ) return a.is_primary;
					if ( a.bounds.top  != b.bounds.top  ) return a.bounds.top  < b.bounds.top;
					return a.bounds.left < b.bounds.left;
				} );
			return monitors;
		}

	private:
		transport::channel_t m_channel { };
		shared_state_t*      m_state   = nullptr;
		std::unordered_set< std::wstring >    m_registered_fonts;
		std::unordered_set< std::uint32_t >   m_registered_images;
		std::mutex                             m_mutex {};

		std::vector< std::uint8_t > m_pending;

		bool m_handshake_done   = false;
		bool m_batching_enabled = false;

		static void adaptive_wait( std::uint32_t& attempts ) {
			if ( attempts++ < 4096 )
				YieldProcessor( );
			else
				SwitchToThread( );
		}

		auto enqueue_locked( command_id command, const void* buffer, std::size_t size ) -> LONG64 {
			if ( !m_state ) return -1;
			const auto deadline   = GetTickCount64( ) + 1000;
			auto       write_index = m_state->write_index;
			std::uint32_t wait_attempts = 0;

			while ( write_index - m_state->read_index >= capacity ) {
				if ( GetTickCount64( ) >= deadline ) return -1;
				adaptive_wait( wait_attempts );
				write_index = m_state->write_index;
			}

			auto& packet = m_state->packets[ write_index & capacity_mask ];
			while ( packet.status == packet_pending || packet.status == packet_abandoned ) {
				if ( GetTickCount64( ) >= deadline ) return -1;
				adaptive_wait( wait_attempts );
			}

			packet.command  = static_cast< std::uint32_t >( command );
			packet.addr     = 0;
			packet.buffer   = 0;
			packet.size     = size;
			packet.sequence = write_index;
			packet.result   = 0;

			if ( buffer && size ) std::memcpy( packet.payload, buffer, size );

			MemoryBarrier( );
			InterlockedExchange( const_cast< LONG* >( &packet.status ), packet_pending );
			InterlockedExchange64( const_cast< LONG64* >( &m_state->write_index ), write_index + 1 );

			m_channel.signal( );
			return write_index;
		}

		struct wait_result_t { bool ok; bool result; };
		auto enqueue_and_wait_locked( command_id command, void* buffer, std::size_t size ) -> wait_result_t {
			const auto idx = enqueue_locked( command, buffer, size );
			if ( idx < 0 ) return { false, false };

			auto& packet = m_state->packets[ idx & capacity_mask ];
			const auto deadline = GetTickCount64( ) + 1000;
			std::uint32_t wait_attempts = 0;
			while ( packet.status != packet_complete || packet.sequence != idx ) {
				if ( GetTickCount64( ) >= deadline ) {
					if ( InterlockedCompareExchange( const_cast< LONG* >( &packet.status ),
							packet_abandoned, packet_pending ) == packet_pending )
						m_channel.signal( );
					return { false, false };
				}
				if ( wait_attempts < transport::k_spin_iters ) {
					YieldProcessor( );
					++wait_attempts;
				}
				else {
					m_channel.wait( 1 );
				}
			}
			const auto result = packet.result != 0;
			if ( result && buffer && size ) std::memcpy( buffer, packet.payload, size );
			InterlockedExchange( const_cast< LONG* >( &packet.status ), packet_empty );
			return { true, result };
		}

		auto ensure_handshake( ) -> void {
			if ( m_handshake_done ) return;
			std::lock_guard< std::mutex > lock( m_mutex );
			if ( m_handshake_done ) return;
			m_handshake_done = true;

			if ( !m_state ) { m_batching_enabled = false; return; }

			const auto ping = enqueue_and_wait_locked( command_id::render_ping, nullptr, 0 );
			if ( !ping.ok || !ping.result ) {
				logging::print( obf( "overlay handshake: ping FAILED (ok=%d result=%d) -> batching OFF, fallback to per-packet" ),
					ping.ok ? 1 : 0, ping.result ? 1 : 0 );
				m_batching_enabled = false; return;
			}

			batch_record_t hdr { static_cast< std::uint32_t >( command_id::render_ping ), 0u };
			std::uint8_t probe_payload[ sizeof( hdr ) ];
			std::memcpy( probe_payload, &hdr, sizeof( hdr ) );
			const auto batch_probe = enqueue_and_wait_locked(
				command_id::render_batch, probe_payload, sizeof( probe_payload ) );
			if ( !batch_probe.ok || !batch_probe.result ) {
				logging::print( obf( "overlay handshake: batch probe FAILED (ok=%d result=%d) -> batching OFF, fallback to per-packet" ),
					batch_probe.ok ? 1 : 0, batch_probe.result ? 1 : 0 );
				m_batching_enabled = false; return;
			}

			m_batching_enabled = true;
		}

		auto buffer_record_locked( command_id command, const void* buffer, std::size_t size ) -> bool {
			const auto record_bytes = sizeof( batch_record_t ) + size;
			if ( record_bytes > packet_payload_size ) return false;

			if ( m_pending.size( ) + record_bytes > packet_payload_size ) {
				if ( !flush_locked( ) ) return false;
			}

			const batch_record_t hdr {
				static_cast< std::uint32_t >( command ),
				static_cast< std::uint32_t >( size )
			};
			const auto old_size = m_pending.size( );
			m_pending.resize( old_size + record_bytes );
			std::memcpy( m_pending.data( ) + old_size, &hdr, sizeof( hdr ) );
			if ( buffer && size )
				std::memcpy( m_pending.data( ) + old_size + sizeof( hdr ), buffer, size );
			return true;
		}

		auto flush_locked( ) -> bool {
			if ( m_pending.empty( ) ) return true;
			const auto idx = enqueue_locked(
				command_id::render_batch,
				m_pending.data( ),
				m_pending.size( ) );
			m_pending.clear( );
			return idx >= 0;
		}

		auto buffer_or_send( command_id command, const void* buffer, std::size_t size ) -> bool {
			if ( !m_state || size > packet_payload_size ) return false;
			ensure_handshake( );
			if ( !m_batching_enabled )
				return send_control( command, buffer, size );

			std::lock_guard< std::mutex > lock( m_mutex );
			return buffer_record_locked( command, buffer, size );
		}

		auto send_control( command_id command, const void* buffer, std::size_t size ) -> bool {
			if ( !m_state || size > packet_payload_size ) return false;
			std::lock_guard< std::mutex > lock( m_mutex );
			return enqueue_locked( command, buffer, size ) >= 0;
		}

		auto sync_op( command_id command, const void* buffer, std::size_t size ) -> bool {
			if ( !m_state || size > packet_payload_size ) return false;
			ensure_handshake( );

			std::lock_guard< std::mutex > lock( m_mutex );
			if ( m_batching_enabled ) flush_locked( );

			const auto res = enqueue_and_wait_locked(
				command, const_cast< void* >( buffer ), size );
			return res.ok && res.result;
		}

		auto sync_op_with_reply( command_id command, void* buffer, std::size_t size ) -> bool {
			if ( !m_state || size > packet_payload_size ) return false;
			ensure_handshake( );

			std::lock_guard< std::mutex > lock( m_mutex );
			if ( m_batching_enabled ) flush_locked( );

			const auto res = enqueue_and_wait_locked( command, buffer, size );
			return res.ok && res.result;
		}
	};
}
