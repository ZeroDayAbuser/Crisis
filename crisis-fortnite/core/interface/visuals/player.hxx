#pragma once

namespace gui::player {
	[[nodiscard]] color_t team_color( std::uint8_t index );
	void render(
		cache::c_player_pawn& player_pawn,
		const std::shared_ptr< player_stats::player_stats_cache_t >& stats_cache = { } );
}