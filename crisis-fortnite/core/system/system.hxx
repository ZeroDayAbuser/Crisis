#pragma once

namespace xp_system {
	class c_system {
	public:
		~c_system( );

		void start( );

		void update_points( );
	private:
		void reset_match( bool count_as_loss );

		std::thread m_update_thread;
		std::atomic<bool> m_running { false };
		std::atomic<bool> m_win_notified { false };
		std::atomic<bool> m_had_enemies  { false };  // armed once a living enemy is seen; prevents false win at match start
		std::atomic<bool> m_match_active { false };
		std::atomic<bool> m_match_finished { false };
		std::atomic<std::int32_t> m_player_kills { 0 };
		std::uint32_t m_profile_refresh_ticks = 0;
		static constexpr auto update_sleep = ch::seconds( 1 );
	};
}
