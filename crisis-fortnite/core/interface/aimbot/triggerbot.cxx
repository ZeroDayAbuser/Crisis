#include <deps/includes.h>

namespace triggerbot {
		void c_triggerbot::reset( ) {
			m_state = e_state_t::idle;
		}
		void c_triggerbot::tick( const std::optional< cache::c_player_pawn >& player ) {
			if ( !config::triggerbot::enabled ) {
				m_state = e_state_t::idle;
				return;
			}

			const bool aim_key_down = config::aimbot::aim_key != 0 &&
				( GetAsyncKeyState( config::aimbot::aim_key ) & 0x8000 ) != 0;
			const bool gate_ok = config::triggerbot::fire_without_aim || aim_key_down;
			if ( !gate_ok ) {
				m_state = e_state_t::idle;
				return;
			}

			if ( !player ) {
				m_state = e_state_t::idle;
				return;
			}

			if ( config::triggerbot::visible_only && !player->m_is_visible ) {
				m_state = e_state_t::idle;
				return;
			}

			const float dist_m = static_cast< float >(
				cache::camera_location.distance_to( player->m_head_location ) );
			if ( dist_m > config::triggerbot::max_distance_m ) {
				m_state = e_state_t::idle;
				return;
			}

			const auto head_screen = engine::world_to_screen( player->m_head_location );
			if ( !head_screen ||
				!std::isfinite( head_screen->x ) ||
				!std::isfinite( head_screen->y ) ) {
				m_state = e_state_t::idle;
				return;
			}

			const float dx = static_cast< float >( head_screen->x ) - static_cast< float >( gui::screen_width_center );
			const float dy = static_cast< float >( head_screen->y ) - static_cast< float >( gui::screen_height_center );
			if ( dx * dx + dy * dy > config::triggerbot::deadzone_px * config::triggerbot::deadzone_px ) {
				m_state = e_state_t::idle;
				return;
			}

			const auto weapon = g_cache->m_current_weapon.load( std::memory_order_acquire );
			if ( !weapon ) return;

			const auto core_anim = bypass::read< std::uint8_t >(
				reinterpret_cast< std::uint64_t >( weapon ) + engine::offsets::weapon_core_animation );
			if ( !( config::triggerbot::weapon_mask & ( std::uint64_t( 1 ) << core_anim ) ) )
				return;

			const auto pawn = g_cache->m_acknowledged_pawn.load( std::memory_order_acquire );
			if ( !pawn ) return;

			if ( bypass::read< std::uint8_t >(
					reinterpret_cast< std::uint64_t >( pawn ) + engine::offsets::b_is_reloading_weapon ) )
				return;

			if ( bypass::read< int >(
					reinterpret_cast< std::uint64_t >( weapon ) + engine::offsets::ammo_count ) <= 0 )
				return;

			const auto now = std::chrono::steady_clock::now( );

			switch ( m_state ) {
			case e_state_t::idle:
				m_state = e_state_t::pre_fire;
				m_time  = now;
				break;
			case e_state_t::pre_fire:
				if ( std::chrono::duration_cast< std::chrono::milliseconds >( now - m_time ).count( )
						>= static_cast< long long >( config::triggerbot::pre_fire_ms ) ) {
					bypass::mouse::click( );
					m_state = e_state_t::cooldown;
					m_time  = now;
				}
				break;
			case e_state_t::cooldown:
				if ( std::chrono::duration_cast< std::chrono::milliseconds >( now - m_time ).count( )
						>= static_cast< long long >( config::triggerbot::post_fire_ms ) )
					m_state = e_state_t::idle;
				break;
			}
		}
}
