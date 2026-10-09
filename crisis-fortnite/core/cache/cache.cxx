#include <deps/includes.h>

namespace cache {
	engine::f_vector camera_location { };
	engine::f_rotator camera_rotation { };
	float camera_fov { };
	engine::f_vector camera_axis_right { };
	engine::f_vector camera_axis_up { };
	engine::f_vector camera_axis_forward { };
	float camera_projection_scale { };
	std::shared_mutex camera_snapshot_mutex;
	camera_snapshot_t camera_snapshot { };
	std::shared_ptr< const visual_snapshot_t > visual_snapshot =
		std::make_shared< const visual_snapshot_t >( );
	std::shared_ptr< const items_snapshot_t > items_snapshot =
		std::make_shared< const items_snapshot_t >( );
	std::atomic< std::uint32_t > monitor_refresh_rate{ 60 };

	void initialize_monitor_refresh_rate( ) {
		const auto monitors = bypass::overlay::enumerate_monitors( );

		for ( const auto& m : monitors ) {
			if ( !m.is_primary )
				continue;
			RECT r { m.left, m.top, m.right, m.bottom };
			const HMONITOR hmon = MonitorFromRect( &r, MONITOR_DEFAULTTONEAREST );
			if ( !hmon ) break;
			MONITORINFOEXW mi { }; mi.cbSize = sizeof( mi );
			if ( !GetMonitorInfoW( hmon, &mi ) ) break;
			DEVMODEW dm { }; dm.dmSize = sizeof( dm );
			if ( !EnumDisplaySettingsW( mi.szDevice, ENUM_CURRENT_SETTINGS, &dm ) ) break;
			const auto hz = static_cast< std::uint32_t >( dm.dmDisplayFrequency );
			if ( hz > 1 && hz <= 1000 )
				monitor_refresh_rate.store( hz, std::memory_order_relaxed );
			return;
		}

		const HMONITOR hmon = MonitorFromWindow( GetDesktopWindow( ), MONITOR_DEFAULTTOPRIMARY );
		if ( !hmon ) return;
		MONITORINFOEXW mi { }; mi.cbSize = sizeof( mi );
		if ( !GetMonitorInfoW( hmon, &mi ) ) return;
		DEVMODEW dm { }; dm.dmSize = sizeof( dm );
		if ( !EnumDisplaySettingsW( mi.szDevice, ENUM_CURRENT_SETTINGS, &dm ) ) return;
		const auto hz = static_cast< std::uint32_t >( dm.dmDisplayFrequency );
		if ( hz > 1 && hz <= 1000 )
			monitor_refresh_rate.store( hz, std::memory_order_relaxed );
	}

	[[nodiscard]] ch::milliseconds perf_sleep( ch::milliseconds baseline ) {
		auto refresh_rate = static_cast< float >( monitor_refresh_rate.load( std::memory_order_relaxed ) );
		const float fps = config::settings::frame_limiter
			? max( config::settings::framerate_cap, 30.f )
			: refresh_rate;
		const auto frame_ms = ch::milliseconds(
			static_cast< long long >( 1000.f / fps ) );
		return frame_ms > baseline ? frame_ms : baseline;
	}
	[[nodiscard]] camera_snapshot_t get_camera_snapshot( ) {
		std::shared_lock lock( camera_snapshot_mutex );
		return camera_snapshot;
	}
	engine::f_vector get_published_camera_location( ) {
		std::shared_lock lock( camera_snapshot_mutex );
		return camera_snapshot.location;
	}
	bool apply_camera_snapshot( const camera_snapshot_t& snapshot ) {
		if ( !snapshot.valid )
			return false;
		camera_location = snapshot.location;
		camera_rotation = snapshot.rotation;
		camera_axis_right = snapshot.axis_right;
		camera_axis_up = snapshot.axis_up;
		camera_axis_forward = snapshot.axis_forward;
		camera_fov = snapshot.fov;
		camera_projection_scale = snapshot.projection_scale;
		return true;
	}
	[[nodiscard]] std::shared_ptr< const items_snapshot_t > get_items_snapshot( ) {
		return std::atomic_load_explicit(
			&items_snapshot, std::memory_order_acquire );
	}
	[[nodiscard]] std::shared_ptr< const visual_snapshot_t > get_visual_snapshot( ) {
		return std::atomic_load_explicit(
			&visual_snapshot, std::memory_order_acquire );
	}
		void c_cache::pace(
			std::chrono::steady_clock::time_point iteration_start,
			std::chrono::steady_clock::duration interval ) {
			const auto elapsed =
				std::chrono::steady_clock::now( ) - iteration_start;
			std::this_thread::sleep_for(
				elapsed < interval ? interval - elapsed : minimum_sleep );
		}
			void c_cache::ptr_staleness_t::mark_invalid( const char* name ) {
				const auto now = std::chrono::steady_clock::now( );
				if ( m_since == std::chrono::steady_clock::time_point { } )
					m_since = now;
				if ( now - m_since >= invalid_warn_threshold ) {
					logging::print( "%s has been invalid for 15s!", name );
					m_since = now;
				}
			}
			void c_cache::ptr_staleness_t::mark_valid( ) {
				m_since = { };
			}
		c_cache::~c_cache( ) {
			m_running.store( false, std::memory_order_release );
			if ( m_world_thread.joinable( ) )
				m_world_thread.join( );
			if ( m_players_thread.joinable( ) )
				m_players_thread.join( );
			if ( m_local_thread.joinable( ) )
				m_local_thread.join( );
			if ( m_items_thread.joinable( ) )
				m_items_thread.join( );
		}
		void c_cache::setup( ) {
			if ( m_running.exchange( true, std::memory_order_acq_rel ) )
				return;
			// Priority strategy: producer workers run at the SAME priority
			// as the render thread. Lowering them starves the cache and
			// the render thread ends up drawing stale positions — that
			// looks like a stutter but is really data staleness. Keeping
			// them equal (all ABOVE_NORMAL) lets the OS round-robin them
			// on a per-quantum basis, which produces smoother data flow
			// than a strict hierarchy for this workload.
			//
			// The only exception is `items` — loot rarely moves so it can
			// afford to lose CPU-scheduling races, and dropping it below
			// the four latency-sensitive workers keeps it from stealing
			// quanta from them under contention.
			m_world_thread = std::thread( [ this ] {
				SetThreadPriority( GetCurrentThread( ), THREAD_PRIORITY_ABOVE_NORMAL );
				update_world( );
			} );
			m_players_thread = std::thread( [ this ] {
				SetThreadPriority( GetCurrentThread( ), THREAD_PRIORITY_ABOVE_NORMAL );
				update_players( );
			} );
			m_local_thread = std::thread( [ this ] {
				SetThreadPriority( GetCurrentThread( ), THREAD_PRIORITY_ABOVE_NORMAL );
				update_local( );
			} );
			m_items_thread = std::thread( [ this ] {
				SetThreadPriority( GetCurrentThread( ), THREAD_PRIORITY_BELOW_NORMAL );
				update_items( );
			} );
		}
		bool c_cache::initialize( ) {
			auto fort_library = engine::u_object::find_object< engine::u_fort_kismet_library* >( obf( L"FortniteGame.Default__FortKismetLibrary" ) );
			m_fort_library.store(
				fort_library, std::memory_order_release );
			if ( !fort_library ) {
				logging::print( obf( "failed to fetch fort kismet library." ) );
				return false;
			}

			auto string_library = engine::u_object::find_object< engine::u_kismet_string_library* >( obf( L"Engine.Default__KismetStringLibrary" ) );
			m_string_library.store(
				string_library, std::memory_order_release );
			if ( !string_library ) {
				logging::print( obf( "failed to fetch string kismet library." ) );
				return false;
			}

			auto offset = this->find_offset( obf( L"Engine.World" ), obf( L"DefaultPhysicsVolume" ) );
			logging::print( obf( "fetched offset: 0x%x" ), offset );
			return true;
		}
		bool c_cache::sample_camera( camera_snapshot_t& next ) {
			const auto world = m_world.load( std::memory_order_acquire );
			if ( !world )
				return false;

			engine::t_array< engine::cached_view_t > cached_views { };
			const auto cached_views_address =
				reinterpret_cast< std::uintptr_t >( world ) +
				engine::offsets::cached_view_info_rendered_last_frame;
			if ( !bypass::target::read_memory(
				cached_views_address, &cached_views, sizeof( cached_views ) ) ) {
				m_stale_camera_view_states.mark_invalid( "camera_cached_views_read" );
				return false;
			}

			if ( !cached_views.is_valid( ) ||
				cached_views.count > cached_views.max_count ) {
				m_stale_camera_view_states.mark_invalid( "camera_cached_views_invalid" );
				return false;
			}

			engine::cached_view_t view_info { };
			if ( !cached_views.read_range( 0, 1, &view_info ) ) {
				m_stale_camera_matrices.mark_invalid( "camera_cached_view_read" );
				return false;
			}
			m_stale_camera_view_states.mark_valid( );

			const auto& view_to_world = view_info.view_to_world;
			const auto& projection_matrix = view_info.projection_matrix;

			next = { };
			next.axis_right = {
				view_to_world.m[ 0 ][ 0 ],
				view_to_world.m[ 0 ][ 1 ],
				view_to_world.m[ 0 ][ 2 ]
			};
			next.axis_up = {
				view_to_world.m[ 1 ][ 0 ],
				view_to_world.m[ 1 ][ 1 ],
				view_to_world.m[ 1 ][ 2 ]
			};
			next.axis_forward = {
				view_to_world.m[ 2 ][ 0 ],
				view_to_world.m[ 2 ][ 1 ],
				view_to_world.m[ 2 ][ 2 ]
			};
			next.location = {
				view_to_world.m[ 3 ][ 0 ],
				view_to_world.m[ 3 ][ 1 ],
				view_to_world.m[ 3 ][ 2 ]
			};

			const auto pitch_value =
				std::clamp( view_to_world.m[ 2 ][ 2 ], -1.0, 1.0 );
			next.rotation.pitch =
				asin( pitch_value ) * 180.0 / std::numbers::pi;
			next.rotation.yaw =
				atan2( view_to_world.m[ 2 ][ 1 ], view_to_world.m[ 2 ][ 0 ] ) *
				180.0 / std::numbers::pi;
			next.rotation.roll = 0.0;

			const auto scale = projection_matrix.m[ 0 ][ 0 ];
			const auto fov_radians = 2.0 * atan( 1.0 / scale );
			next.fov = static_cast< float >(
				fov_radians * 180.0 / std::numbers::pi );
			next.projection_scale =
				static_cast< float >( gui::screen_width_center ) *
				static_cast< float >( scale );

			const double location_squared =
				next.location.x * next.location.x +
				next.location.y * next.location.y +
				next.location.z * next.location.z;
			if ( !std::isfinite( scale ) || scale <= 0.0 ||
				!std::isfinite( location_squared ) || location_squared < 1.0 ||
				!std::isfinite( next.fov ) ||
				!std::isfinite( next.projection_scale ) ||
				!( next.projection_scale > 1.0f ) ) {
				m_stale_camera_matrices.mark_invalid( "camera_cached_view_invalid" );
				return false;
			}

			next.captured_at = std::chrono::steady_clock::now( );
			next.valid = true;
			m_stale_camera_matrices.mark_valid( );
			return true;
		}
		std::vector< c_player_pawn > c_cache::get_players( ) {
			auto snapshot = get_visual_snapshot( );
			return snapshot ? snapshot->players : std::vector< c_player_pawn > { };
		}
		std::shared_ptr< const items_snapshot_t > c_cache::get_items( ) {
			return get_items_snapshot( );
		}
		std::vector< c_bullet_tracer > c_cache::get_bullet_tracers( ) {
			return m_bullet_tracers.get_front_buffer_copy( );
		}
}
