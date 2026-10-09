#include <deps/includes.h>

namespace gui {
    namespace world_detail {
		void render_world_marker(
			const engine::f_vector& location,
			float max_distance,
			const wchar_t* label,
			const bypass::overlay::color_t& color ) {

			const float dist = static_cast< float >(
				cache::camera_location.distance_to( location ) );
			if ( dist > max_distance )
				return;

			const auto screen = engine::world_to_screen( location );
			if ( !screen )
				return;

			if ( screen->x < 0.0 || screen->x > static_cast< double >( gui::screen_width ) ||
				screen->y < 0.0 || screen->y > static_cast< double >( gui::screen_height ) )
				return;

			const float sx = static_cast< float >( screen->x );
			const float sy = static_cast< float >( screen->y );

			wchar_t text[ 128 ];
			_snwprintf_s( text, _TRUNCATE, L"%s \xB7 %.0fm", label, dist );

			bypass::overlay::text(
				text,
				bypass::overlay::rect_t{ sx - 60.f, sy - 5.f, 120.f, 10.f },
				color, 10.f, bypass::overlay::text_weight_t::bold,
				nullptr, bypass::overlay::text_align_t::center, false );
		}
    }
    namespace loot {
		void render( const cache::c_pickup_pawn& pawn ) {
			if ( !pawn.m_root_component )
				return;
			constexpr bypass::overlay::color_t grey { 190, 190, 190, 255 };
			const wchar_t* name = pawn.m_pickup_name_wide.empty( )
				? L"loot"
				: pawn.m_pickup_name_wide.c_str( );
			world_detail::render_world_marker(
				pawn.m_relative_location, config::loot::pickup_distance, name, grey );
		}
    }
    namespace chest {
		void render( const cache::c_chest_pawn& pawn ) {
			if ( !config::loot::chest || !pawn.m_root_component )
				return;
			constexpr bypass::overlay::color_t gold { 255, 215, 0, 255 };
			world_detail::render_world_marker(
				pawn.m_relative_location, config::loot::chest_distance, L"chest", gold );
		}
    }
    namespace weakspot {
		void render( const cache::c_weakspot_pawn& pawn ) {
			if ( !config::loot::weakspot || !pawn.m_root_component )
				return;
			constexpr bypass::overlay::color_t red { 255, 60, 60, 255 };
			world_detail::render_world_marker(
				pawn.m_relative_location, config::loot::weakspot_distance, L"weakspot", red );
		}
    }
}
