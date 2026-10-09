#include <deps/includes.h>

namespace cache {
	// [nexus removed 2026-09-01] The KismetStringLibrary lookup
	// machinery lived here — try_find_by_name / safe_find_string_library_raw
	// / find_string_library_context / find_string_library_worker /
	// g_pending_ctx / safe_find_string_library — a two-name fallback
	// (Engine.Default__... and /Script/Engine.Default__...), an SEH
	// wrapper around engine::u_object::find_object, a worker thread with
	// 2s watchdog, a pending-context slot for post-hang recovery, and a
	// 32-attempt exhaustion cap. All of it depended on nexus_bridge::call
	// (find_object → nexus_bridge::call<>) which is gone. Removed with
	// Nexus; the world-update loop below no longer performs the lookup.

	void c_cache::update_world( ) {
			priority::c_gate gate;
			std::vector< std::pair< engine::a_actor*, actor_type > >
				cached_player_actors;

			// [nexus removed 2026-09-01] A per-tick poll that checked
			// engine::nexus_bridge::is_callable(static_find_object) and
			// invoked safe_find_string_library() once the page was mirrored
			// lived here. Nexus is gone; the lookup no longer runs.

			while ( m_running.load( std::memory_order_acquire ) ) {
				const auto iteration_start = std::chrono::steady_clock::now( );
				gate.tick( );

				if ( gate.on( priority::level::low ) ) {

					auto gengine = engine::u_engine::get( );
					if ( !gengine ) {
						pace( iteration_start, world_sleep );
						continue;
					}

					auto viewport_client = gengine->viewport_client( );
					if ( !viewport_client ) {
						pace( iteration_start, world_sleep );
						continue;
					}

				auto world = viewport_client->world( );
				m_world.store( world, std::memory_order_release );
				if ( !world ) {
					m_stale_world.mark_invalid( "world" );
					pace( iteration_start, world_sleep );
					continue;
				}
				m_stale_world.mark_valid( );

				auto game_instance = world->owning_game_instance( );
				m_game_instance.store(
					game_instance, std::memory_order_release );
				if ( !game_instance ) {
					m_stale_game_instance.mark_invalid( "game_instance" );
					pace( iteration_start, world_sleep );
					continue;
				}
				m_stale_game_instance.mark_valid( );

				auto game_state = world->game_state( );
				m_game_state.store(
					game_state, std::memory_order_release );
				if ( !game_state ) {
					m_stale_game_state.mark_invalid( "game_state" );
					pace( iteration_start, world_sleep );
					continue;
				}
				m_stale_game_state.mark_valid( );

				auto local_player = game_instance->get_localplayer( );
				m_local_player.store( local_player, std::memory_order_release );
				if ( !local_player ) {
					m_stale_local_player.mark_invalid( "local_player" );
					pace( iteration_start, world_sleep );
					continue;
				}
				m_stale_local_player.mark_valid( );

				auto player_controller = local_player->player_controller( );
				m_player_controller.store(
					player_controller, std::memory_order_release );
				if ( !player_controller ) {
					m_stale_player_controller.mark_invalid( "player_controller" );
					pace( iteration_start, world_sleep );
					continue;
				}
				m_stale_player_controller.mark_valid( );

				cached_player_actors.clear( );

				bool traversal_valid = true;
				auto levels = world->levels( );
				const bool levels_valid = for_each_array(
					levels,
					maximum_levels,
					[ & ]( engine::u_level* level ) {
						if ( !level || !utility::is_valid_pointer( level ) )
							return;

						auto actors = level->actor_clustor( );
						if ( !for_each_array(
							actors,
							maximum_actors_per_level,
							[ & ]( engine::a_actor* actor ) {
								if ( !actor ||
									!utility::is_valid_pointer( actor ) )
									return;

								if ( actor->is_player( ) ) {
									cached_player_actors.emplace_back(
										actor,
										actor_type::player );
								}

								if ( actor->is_pickup( ) ) {
									cached_player_actors.emplace_back(
										actor,
										actor_type::pickup );
								}

								if ( actor->is_container( ) ) {
									cached_player_actors.emplace_back(
										actor,
										actor_type::chest );
								}

								if ( actor->is_weakspot( ) ) {
									cached_player_actors.emplace_back(
										actor,
										actor_type::weakspot );
								}
							} ) ) {
							traversal_valid = false;
						}
					} );

				traversal_valid = traversal_valid && levels_valid;
				if ( traversal_valid )
					m_actors.update_buffer( cached_player_actors );
				}

				pace( iteration_start, world_sleep );
			}
		}
}
