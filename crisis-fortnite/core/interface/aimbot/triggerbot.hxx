#pragma once

namespace triggerbot {
	class c_triggerbot {
	public:


		void reset( );
		void tick( const std::optional< cache::c_player_pawn >& player );
	private:
		enum class e_state_t : std::uint8_t { idle, pre_fire, cooldown };
		e_state_t m_state { e_state_t::idle };
		std::chrono::steady_clock::time_point m_time { };
	};
}
