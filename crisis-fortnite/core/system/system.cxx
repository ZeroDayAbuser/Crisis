#include <deps/includes.h>

namespace xp_system {
	c_system::~c_system( ) {
			m_running.store( false, std::memory_order_release );
			if ( m_update_thread.joinable( ) )
				m_update_thread.join( );
		}
	void c_system::start( ) {
			m_running.store( true, std::memory_order_release );
			m_update_thread = std::thread( [ this ] { update_points( ); } );
		}
	void c_system::update_points( ) {
			while ( m_running.load( std::memory_order_acquire ) ) {
				if ( ++m_profile_refresh_ticks >= 30 ) {
					crisis::profile::client.refresh( );
					m_profile_refresh_ticks = 0;
				}
				auto game_state = g_cache->m_game_state.load( );
				if ( !game_state ) {
					reset_match( true );
					std::this_thread::sleep_for( update_sleep );
					continue;
				}

				auto acknowledged_pawn = g_cache->m_acknowledged_pawn.load( );
				if ( !acknowledged_pawn ) {
					reset_match( true );
					std::this_thread::sleep_for( update_sleep );
					continue;
				}

				auto player_state = acknowledged_pawn->player_state( );
				if ( !player_state ) {
					reset_match( true );
					std::this_thread::sleep_for( update_sleep );
					continue;
				}

				if ( !m_match_active.load( ) && !m_match_finished.load( ) ) {
					m_match_active.store( crisis::profile::client.start_match( "non_ranked" ) );
					m_player_kills.store( 0 );
				}

				const auto kill_score = player_state->kill_score( );
				const auto previous_kills = m_player_kills.load( );
				if ( kill_score > previous_kills && m_match_active.load( ) ) {
					static const wchar_t* kill_descriptions[ ] = {
						L"Sent to the lobby.",
						L"They got clipped.",
						L"Clean elimination.",
						L"Not even close.",
					};
					static std::mt19937 rng( std::random_device { }( ) );
					static std::uniform_int_distribution<std::size_t> dist( 0, std::size( kill_descriptions ) - 1 );
					for ( auto value = previous_kills; value < kill_score; ++value ) {
						if ( crisis::profile::client.elimination( ) ) {
							const auto profile = crisis::profile::client.snapshot( );
							wchar_t title[ 48 ] {};
							std::swprintf( title, std::size( title ), L"Kill Confirmed - +%u XP", profile.elimination_xp );
							menu::notifications::push( title, kill_descriptions[ dist( rng ) ], L"\uf54c" );
						}
					}
				}
				m_player_kills.store( kill_score );

				const auto local_team = g_cache->m_local_team_index.load( std::memory_order_acquire );
				const auto players_snap = g_cache->get_players( );

				if ( !m_had_enemies.load( ) ) {
					for ( const auto& player : players_snap ) {
						if ( !player.m_is_dying &&
							( local_team == 0 || player.m_team_index != local_team ) ) {
							m_had_enemies.store( true );
							break;
						}
					}
				}

				const auto winning_team         = game_state->winning_team( );
				const auto winning_player_state = game_state->winning_player_state( );

				const bool won_game =
					m_had_enemies.load( )          &&
					winning_team > 0               &&
					winning_team == local_team      &&
					winning_player_state != nullptr;

				if ( won_game && !m_win_notified.load( ) && m_match_active.load( ) ) {
					if ( crisis::profile::client.end_match( true ) ) {
						const auto profile = crisis::profile::client.snapshot( );
						wchar_t title[ 48 ] {};
						std::swprintf( title, std::size( title ), L"Victory Royale! - +%u XP", profile.victory_xp );
						menu::notifications::push( title, L"You won the game.", L"\uf521" );
						m_win_notified.store( true );
						m_match_active.store( false );
						m_match_finished.store( true );
					}
				}

				std::this_thread::sleep_for( update_sleep );
			}
		}
	void c_system::reset_match( bool count_as_loss ) {
			if ( count_as_loss && m_match_active.exchange( false ) && !crisis::profile::client.end_match( false ) ) {
				m_match_active.store( true );
				return;
			}
			m_win_notified.store( false );
			m_had_enemies.store( false );
			m_match_finished.store( false );
			m_player_kills.store( 0 );
		}
}
