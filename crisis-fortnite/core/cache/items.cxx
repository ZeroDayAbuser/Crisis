#include <deps/includes.h>

namespace cache {
	void c_cache::update_items( ) {
			std::unordered_map< engine::a_actor*, c_pickup_pawn >   pickups_by_actor;
			std::unordered_map< engine::a_actor*, c_chest_pawn >    chests_by_actor;
			std::unordered_map< engine::a_actor*, c_weakspot_pawn > weakspots_by_actor;
			std::unordered_set< engine::a_actor* > live_pickups, live_chests, live_weakspots;
			priority::c_gate gate;

			while ( m_running.load( std::memory_order_acquire ) ) {
				const auto iteration_start = std::chrono::steady_clock::now( );
				gate.tick( );
				auto actors = m_actors.get_front_buffer_copy( );

				live_pickups.clear( );
				live_chests.clear( );
				live_weakspots.clear( );
				for ( const auto& [actor, type] : actors ) {
					if ( type == actor_type::pickup )
						live_pickups.insert( actor );
					else if ( type == actor_type::chest )
						live_chests.insert( actor );
					else if ( type == actor_type::weakspot )
						live_weakspots.insert( actor );
				}

				for ( auto it = pickups_by_actor.begin( ); it != pickups_by_actor.end( ); )
					it = live_pickups.contains( it->first ) ? ++it : pickups_by_actor.erase( it );
				for ( auto it = chests_by_actor.begin( ); it != chests_by_actor.end( ); )
					it = live_chests.contains( it->first ) ? ++it : chests_by_actor.erase( it );
				for ( auto it = weakspots_by_actor.begin( ); it != weakspots_by_actor.end( ); )
					it = live_weakspots.contains( it->first ) ? ++it : weakspots_by_actor.erase( it );

				for ( const auto& [actor, type] : actors ) {
					if ( type == actor_type::pickup ) {
						auto [entry, inserted] = pickups_by_actor.try_emplace( actor, actor );
						( void ) inserted;
						entry->second.update( );
					}
				}

				if ( gate.on( priority::level::low ) ) {
					for ( const auto& [actor, type] : actors ) {
						if ( type == actor_type::chest ) {
							auto [entry, inserted] = chests_by_actor.try_emplace( actor, actor );
							( void ) inserted;
							entry->second.update( );
						}
						else if ( type == actor_type::weakspot ) {
							auto [entry, inserted] = weakspots_by_actor.try_emplace( actor, actor );
							( void ) inserted;
							entry->second.update( );
						}
					}
				} else {
					for ( const auto& [actor, type] : actors ) {
						if ( type == actor_type::chest )
							chests_by_actor.try_emplace( actor, actor );
						else if ( type == actor_type::weakspot )
							weakspots_by_actor.try_emplace( actor, actor );
					}
				}

				auto next = std::make_shared< items_snapshot_t >( );
				next->pickups.reserve( pickups_by_actor.size( ) );
				for ( const auto& [actor, pawn] : pickups_by_actor )
					next->pickups.emplace_back( pawn );
				next->chests.reserve( chests_by_actor.size( ) );
				for ( const auto& [actor, pawn] : chests_by_actor )
					next->chests.emplace_back( pawn );
				next->weakspots.reserve( weakspots_by_actor.size( ) );
				for ( const auto& [actor, pawn] : weakspots_by_actor )
					next->weakspots.emplace_back( pawn );
				std::atomic_store_explicit(
					&items_snapshot,
					std::shared_ptr< const items_snapshot_t >( std::move( next ) ),
					std::memory_order_release );
				pace( iteration_start, perf_sleep( items_sleep ) );
			}
		}
}
