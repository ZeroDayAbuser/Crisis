#pragma once

namespace gui {
	namespace world_detail {
		void render_world_marker(
			const engine::f_vector& location,
			float max_distance,
			const wchar_t* label,
			const bypass::overlay::color_t& color );
	}

	namespace loot {
		void render( const cache::c_pickup_pawn& pawn );
	}

	namespace chest {
		void render( const cache::c_chest_pawn& pawn );
	}

	namespace weakspot {
		void render( const cache::c_weakspot_pawn& pawn );
	}
}
