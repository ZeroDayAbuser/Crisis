#include <deps/includes.h>

namespace cache {
	void c_cache::update_local( ) {
			priority::c_gate gate;

			while ( m_running.load( std::memory_order_acquire ) ) {
				const auto iteration_start = std::chrono::steady_clock::now( );
				gate.tick( );

				auto world = m_world.load( std::memory_order_acquire );
				if ( !world ) {
					pace( iteration_start, local_sleep );
					continue;
				}

				auto player_controller =
					m_player_controller.load( std::memory_order_acquire );
				if ( !player_controller ) {
					pace( iteration_start, local_sleep );
					continue;
				}

				auto local_player =
					m_local_player.load( std::memory_order_acquire );
				if ( !local_player ) {
					pace( iteration_start, local_sleep );
					continue;
				}

				//auto player_camera_manager = bypass::read( player_controller + engine::offsets::player_camera_manager );
				//if ( player_camera_manager ) { 
				//	logging::print( obf( "reset camera manager" ) );
				//	bypass::write( player_controller + engine::offsets::player_camera_manager, 0 );
				//}

				if ( gate.on( priority::level::low ) ) {
					auto client_settings_record =
						local_player->client_settings_record( );
					if ( client_settings_record )
						m_mouse_sensitivity.store(
							client_settings_record->mouse_sensitivity( ),
							std::memory_order_release );
				}

				auto acknowledged_pawn =
					player_controller->acknowledged_pawn( );
				m_acknowledged_pawn.store(
					acknowledged_pawn, std::memory_order_release );

				if ( acknowledged_pawn ) {
					auto current_weapon = acknowledged_pawn->current_weapon( );
					m_current_weapon.store(
						current_weapon, std::memory_order_release );
					if ( current_weapon ) {
						const auto current_gun_fire_index = current_weapon->current_gun_fire_index( );
						m_is_firing.store( current_gun_fire_index != m_last_gun_fire_index.load( ) );
						m_last_gun_fire_index.store( current_gun_fire_index );

						{
							// All reads route through the SDK — last_fired_time /
							// last_fired_location / last_fired_direction are declared on
							// a_fort_player_pawn_athena in classes.h.
							const auto fired_time = acknowledged_pawn->last_fired_time( );

							if ( fired_time > m_last_local_fired_time + 0.001f ) {
								m_last_local_fired_time = fired_time;

								if ( config::bullet_tracers::enabled ) {
									const auto fired_loc = acknowledged_pawn->last_fired_location( );
									const auto fired_dir = acknowledged_pawn->last_fired_direction( );
									const float impact_dist =
										current_weapon->current_projected_impact_distance( );

									if ( fired_dir.is_valid( ) && impact_dist > 0.f ) {
										c_bullet_tracer tr;
										tr.m_start_location = fired_loc;
										tr.m_end_location   = fired_loc +
											fired_dir * static_cast< double >( impact_dist );

										if ( !tr.is_duplicate( m_active_tracers ) ) {
											if ( static_cast< int >( m_active_tracers.size( ) ) <
												config::bullet_tracers::max_count )
												m_active_tracers.emplace_back( std::move( tr ) );
										}
									}
								}
							}
						}

						if ( !config::bullet_tracers::enabled )
							m_active_tracers.clear( );

					}

					if ( gate.on( priority::level::low ) ) {
						// SDK route — a_pawn::player_state() → a_player_state::team_index()
						// replaces the previous two raw bypass::read calls that walked
						// the same offsets by hand.
						std::uint8_t team_idx = 0;
						if ( auto local_ps = acknowledged_pawn->player_state( ) )
							team_idx = local_ps->team_index( );
						m_local_team_index.store( team_idx, std::memory_order_release );
					}

					// ── Exploits · no_spread ─────────────────────────────────────
					// Zero-reticle walker. Rewrites the weapon's encrypted
					// spread value across every known spread offset so the
					// reticle bloom stays pinned to config::exploits::
					// spread_value. Runs on the local thread which already
					// touches acknowledged_pawn once per tick, so no extra
					// scheduling is needed — the walker is the only new
					// cost per iteration.
					//
					// Per offset:
					//   1. read the 1-byte obfuscated selector, deobfuscate
					//      (xor 0x39, & 3) and route it through aim_idx_table
					//      to pick which of the four 4-byte value slots is
					//      the live one this frame;
					//   2. read the 4-byte counter and derive the game's
					//      current XOR key from it — the same formula the
					//      game uses to decrypt the slot at read time;
					//   3. read + decrypt the current spread float. Reject
					//      it if it's out of the sane [0, 1000] range (torn
					//      read / wrong slot / not-yet-initialised weapon);
					//   4. skip when spread_max > 0 and the current value
					//      is already at/below the cap — no wasted writes;
					//   5. write ( key ^ target_bits ) into the slot so
					//      the game's next decrypt yields exactly the
					//      user-selected spread value;
					//   6. re-read the counter; if it still matches the
					//      sample we derived the key from, our write
					//      landed atomically against the game's own key
					//      derivation and we're done for this offset.
					//      Otherwise retry — capped at 4 tries so a
					//      weapon whose counter is being spun every read
					//      never blocks the rest of update_local.
					if ( config::exploits::no_spread &&
					     current_weapon != nullptr ) {
						const auto weapon =
							reinterpret_cast< std::uintptr_t >( current_weapon );
						const float target = config::exploits::spread_value;
						std::uint32_t target_bits{ };
						std::memcpy( &target_bits, &target, sizeof( target_bits ) );

						for ( auto off : engine::offsets::spread_offset ) {
							const std::uintptr_t value_base = weapon + off - 0x10;

							const auto raw_idx = bypass::read< std::uint8_t >(
								weapon + off );
							const auto idx = static_cast< std::uint8_t >(
								( raw_idx ^ 0x39 ) & 3 );
							const auto slot = engine::offsets::aim_idx_table[ idx ];
							const std::uintptr_t slot_addr =
								value_base + 4u * slot;

							for ( int attempt = 0; attempt < 4; ++attempt ) {
								const auto ctr_raw = bypass::read< std::uint32_t >(
									weapon + off + 4 );
								const std::uint32_t key =
									( ( ( ctr_raw ^ 0x5C7F1880u ) - 0x44B0u )
										& 0x8EBD8270u )
									| 0x40427D8Fu;

								const auto enc = bypass::read< std::uint32_t >( slot_addr );
								const std::uint32_t dec = enc ^ key;
								float cur_val{ };
								std::memcpy( &cur_val, &dec, sizeof( cur_val ) );

								if ( cur_val < 0.0f || cur_val > 1000.0f )
									break;

								if ( config::exploits::spread_max > 0.0f &&
								     cur_val <= config::exploits::spread_max )
									break;

								bypass::write< std::uint32_t >(
									slot_addr, key ^ target_bits );

								if ( bypass::read< std::uint32_t >( weapon + off + 4 )
								     == ctr_raw )
									break;
							}
						}
					}

					// ── Exploits ─────────────────────────────────────────────────
					// magic_bullet — inflate the local pawn's head bone by writing
					// an oversized scale into the SkeletalMesh's FinalRefBonePose
					// [head]. The head bone index (5) matches the original snippet;
					// the client uses this ref pose for both rendering and hit
					// detection on skinned collisions, so a large head bone widens
					// the effective head hitbox around the pawn every frame.
					//
					// Every step goes through the SDK: class_private → default_object
					// → mesh → skinned_asset (with skeletal_mesh fallback) →
					// final_ref_bone_pose. See classes.h for the declare_member
					// wirings; every offset lives in offsets.h.
					if ( config::exploits::magic_bullet ) {
						if ( auto class_ptr = acknowledged_pawn->class_private( ) ) {
							if ( auto cdo = class_ptr->default_object( ) ) {
								// The pawn's CDO is a Character-derived UObject; cast
								// to a_character so its mesh() accessor is in scope.
								auto cdo_character = reinterpret_cast< engine::a_character* >( cdo );
								if ( auto mesh_component = cdo_character->mesh( ) ) {
									engine::u_skeletal_mesh* asset =
										mesh_component->skinned_asset( );
									if ( !asset ) {
										// Older builds populate SkeletalMesh instead of
										// SkinnedAsset. skeletal_mesh() returns u_object*,
										// so reinterpret to the asset SDK shape.
										asset = reinterpret_cast< engine::u_skeletal_mesh* >(
											mesh_component->skeletal_mesh( ) );
									}

									if ( asset ) {
										auto pose = asset->final_ref_bone_pose( );
										constexpr std::uintptr_t head_bone_index = 5;
										if ( pose.data && pose.count > head_bone_index ) {
											auto head_transform = pose.get( head_bone_index );
											// scale is engine::f_vector (three doubles).
											head_transform.scale = { 10.0, 10.0, 22.0 };
											pose.set( head_bone_index, head_transform );
										}
									}
								}
							}
						}
					}
				}
				else {
					m_local_team_index.store( 0, std::memory_order_release );
				}

				{
					const auto tracer_now = std::chrono::steady_clock::now( );
					float tracer_dt = 0.0f;
					if ( m_last_tracer_tick.time_since_epoch( ).count( ) != 0 ) {
						tracer_dt = std::clamp(
							std::chrono::duration< float >(
								tracer_now - m_last_tracer_tick ).count( ),
							0.0f, 0.1f );
					}
					m_last_tracer_tick = tracer_now;

					{
						std::vector< c_bullet_tracer > drained;
						{
							std::lock_guard< std::mutex > lock( m_pending_enemy_tracers_mutex );
							drained.swap( m_pending_enemy_tracers );
						}
						for ( auto& tr : drained ) {
							if ( static_cast< int >( m_active_tracers.size( ) ) >=
								config::bullet_tracers::max_count )
								break;
							if ( !tr.is_duplicate( m_active_tracers ) )
								m_active_tracers.emplace_back( std::move( tr ) );
						}
					}

					for ( auto& tr : m_active_tracers )
						tr.tick( tracer_dt );

					m_active_tracers.erase(
						std::remove_if(
							m_active_tracers.begin( ), m_active_tracers.end( ),
							[]( const c_bullet_tracer& tr ) {
								return tr.is_expired( );
							} ),
						m_active_tracers.end( ) );

					m_bullet_tracers.update_buffer( m_active_tracers );
				}

				pace( iteration_start, local_sleep );
			}
		}
}
