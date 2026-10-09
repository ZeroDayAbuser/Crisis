#include <deps/includes.h>

namespace cache {
	void c_cache::update_players( ) {
			static constexpr std::size_t publish_batch_size = 4;
			std::unordered_map< engine::a_actor*, c_player_pawn >
				pawns_by_actor;
			std::unordered_set< engine::a_actor* > live_actors;
			std::vector< c_player_pawn > player_pawns;

			while ( m_running.load( std::memory_order_acquire ) ) {
				const auto iteration_start = std::chrono::steady_clock::now( );
				auto actors = m_actors.get_front_buffer_copy( );

				live_actors.clear( );
				for ( const auto& [actor, type] : actors ) {
					if ( type == actor_type::player )
						live_actors.insert( actor );
				}

				const auto now = std::chrono::steady_clock::now( );
				for ( auto iterator = pawns_by_actor.begin( );
					iterator != pawns_by_actor.end( ); ) {
					if ( !live_actors.contains( iterator->first ) )
						iterator = pawns_by_actor.erase( iterator );
					else
						++iterator;
				}

				std::size_t updated_since_publish = 0;
				const auto publish_players = [ & ] {
					player_pawns.clear( );
					player_pawns.reserve( pawns_by_actor.size( ) );
					const auto local_pawn =
						m_acknowledged_pawn.load( std::memory_order_acquire );
					for ( const auto& [actor, pawn] : pawns_by_actor ) {
						( void ) actor;
						if ( pawn.m_player != local_pawn &&
							pawn.m_pose_captured_at.time_since_epoch( ).count( ) )
							player_pawns.emplace_back( pawn );
					}

					auto next = std::make_shared< visual_snapshot_t >( );
					next->players = player_pawns;
					std::atomic_store_explicit(
						&visual_snapshot,
						std::shared_ptr< const visual_snapshot_t >( std::move( next ) ),
						std::memory_order_release );
				};

				const auto local_pawn_for_shots =
					m_acknowledged_pawn.load( std::memory_order_acquire );

				for ( const auto& [actor, type] : actors ) {
					if ( type != actor_type::player )
						continue;

					auto [entry, inserted] =
						pawns_by_actor.try_emplace( actor, actor );
					( void ) inserted;
					entry->second.m_last_seen = now;
					entry->second.update( );

					const auto fort_library = m_fort_library.load( std::memory_order_acquire );
					if ( fort_library && entry->second.m_player ) {
						const auto actor_va = reinterpret_cast< std::uint64_t >( entry->second.m_player );
						const auto sub_ptr  = bypass::read< std::uint64_t >( actor_va + 0x08 );
						if ( actor_va >= 0x10000 && actor_va <= 0x7FFFFFFFFFFull &&
							 bypass::target::translate_linear( actor_va ) &&
							 sub_ptr >= 0x10000 && sub_ptr <= 0x7FFFFFFFFFFull &&
							 bypass::target::translate_linear( sub_ptr ) ) {
							entry->second.m_is_visible = fort_library->check_line_of_sight_to_actor_with_channel(
								get_published_camera_location( ),
								entry->second.m_player,
								engine::e_collision_channel::visibility,
								local_pawn_for_shots
							);
						}
					}

					if ( config::bullet_tracers::enabled ) {
						auto& pawn = entry->second;
						if ( pawn.m_player && pawn.m_player != local_pawn_for_shots ) {
							const auto fired_time = bypass::read< float >(
								reinterpret_cast< std::uintptr_t >( pawn.m_player ) +
								engine::offsets::last_fired_time );

							if ( fired_time > pawn.m_last_seen_fired_time + 0.001f ) {
								const bool first_seen =
									( pawn.m_last_seen_fired_time <= 0.001f );
								pawn.m_last_seen_fired_time = fired_time;

								if ( !first_seen ) {
									const auto fired_loc = bypass::read< engine::f_vector >(
										reinterpret_cast< std::uintptr_t >( pawn.m_player ) +
										engine::offsets::last_fired_location );

									if ( pawn.m_last_fired_direction.is_valid( ) &&
										fired_loc.is_valid( ) ) {
										constexpr double kFallbackTraceDist = 6000.0;
										c_bullet_tracer tr;
										tr.m_start_location = fired_loc;
										tr.m_end_location   = fired_loc +
											pawn.m_last_fired_direction * kFallbackTraceDist;
										tr.m_is_enemy = true;

										std::lock_guard< std::mutex > lock(
											m_pending_enemy_tracers_mutex );
										if ( static_cast< int >(
											m_pending_enemy_tracers.size( ) ) <
											config::bullet_tracers::max_count )
											m_pending_enemy_tracers.emplace_back(
												std::move( tr ) );
									}
								}
							}
						}
					}

					if ( ++updated_since_publish >= publish_batch_size ) {
						publish_players( );
						updated_since_publish = 0;
					}
				}

				if ( updated_since_publish || pawns_by_actor.empty( ) )
					publish_players( );

				pace( iteration_start, player_sleep );
			}
		}
}
