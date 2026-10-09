#pragma once
#include <cmath>
#include <cwchar>

namespace cache {
	class c_timer {
	private:
		std::chrono::high_resolution_clock::time_point m_start {};
		double  m_avg_us { 0.0 };
		int     m_count { 0 };

	public:
		c_timer( ) = default;
		~c_timer( ) = default;

		void start( ) {
			m_start = std::chrono::high_resolution_clock::now( );
		}

		void end( const std::string& function, int max_ittr, bool debug = false ) {
			const auto end_time = std::chrono::high_resolution_clock::now( );
			const auto duration = std::chrono::duration_cast< std::chrono::microseconds >(
				end_time - m_start ).count( );

			m_avg_us = ( m_avg_us * m_count + static_cast< double >( duration ) )
				/ static_cast< double >( m_count + 1 );
			++m_count;

			// only report once per averaging window instead of every tick --
			// logging every call floods the overlay console and the resulting
			// I/O stalls are far larger than anything being measured.
			if ( m_count >= max_ittr ) {
				if ( debug )
					logging::print(
						obf( "[%s] avg: %.3f ms, last: %.3f ms, samples: %d" ),
						function.c_str( ),
						m_avg_us / 1000.0,
						static_cast< double >( duration ) / 1000.0,
						m_count
					);

				m_count = 0;
			}
		}

		void        reset( ) { m_avg_us = 0.0; m_count = 0; }
		double      get_average( )    const { return m_avg_us; }
		double      get_average_ms( ) const { return m_avg_us / 1000.0; }
		int         count( )          const { return m_count; }
	};

	template<typename T>
	class c_double_buffer {
	private:
		class c_shared_lock {
		public:
			explicit c_shared_lock( SRWLOCK& lock ): m_lock( lock ) {
				AcquireSRWLockShared( &m_lock );
			}

			~c_shared_lock( ) {
				ReleaseSRWLockShared( &m_lock );
			}

			c_shared_lock( const c_shared_lock& ) = delete;
			c_shared_lock& operator=( const c_shared_lock& ) = delete;

		private:
			SRWLOCK& m_lock;
		};

		class c_exclusive_lock {
		public:
			explicit c_exclusive_lock( SRWLOCK& lock ): m_lock( lock ) {
				AcquireSRWLockExclusive( &m_lock );
			}

			~c_exclusive_lock( ) {
				ReleaseSRWLockExclusive( &m_lock );
			}

			c_exclusive_lock( const c_exclusive_lock& ) = delete;
			c_exclusive_lock& operator=( const c_exclusive_lock& ) = delete;

		private:
			SRWLOCK& m_lock;
		};

	public:
		c_double_buffer( )
			: m_buffers( std::make_unique<std::array<std::vector<T>, 2>>( ) )
			, m_front_index( 0 ) {
		}

		std::vector<T> get_front_buffer_copy( ) const {
			c_shared_lock lock( m_lock );
			int front_idx = m_front_index.load( std::memory_order_relaxed );
			return ( *m_buffers )[ front_idx ];
		}

		void swap_buffers( ) {
			c_exclusive_lock lock( m_lock );
			swap_buffers_unlocked( );
		}

		void reset( ) {
			c_exclusive_lock lock( m_lock );
			m_buffers = std::make_unique<std::array<std::vector<T>, 2>>( );
			m_front_index.store( 0, std::memory_order_relaxed );
		}

		void update_buffer( std::vector<T>&& new_data ) {
			c_exclusive_lock lock( m_lock );
			get_back_buffer_unlocked( ) = std::move( new_data );
			swap_buffers_unlocked( );
		}

		void update_buffer( const std::vector<T>& new_data ) {
			c_exclusive_lock lock( m_lock );
			get_back_buffer_unlocked( ) = new_data;
			swap_buffers_unlocked( );
		}

		size_t size( ) const {
			c_shared_lock lock( m_lock );
			int front_idx = m_front_index.load( std::memory_order_relaxed );
			return ( *m_buffers )[ front_idx ].size( );
		}

		bool empty( ) const {
			c_shared_lock lock( m_lock );
			int front_idx = m_front_index.load( std::memory_order_relaxed );
			return ( *m_buffers )[ front_idx ].empty( );
		}

		bool has_data( ) const {
			return !empty( );
		}

	private:
		std::vector<T>& get_back_buffer_unlocked( ) {
			int front_idx = m_front_index.load( std::memory_order_relaxed );
			return ( *m_buffers )[ 1 - front_idx ];
		}

		void swap_buffers_unlocked( ) {
			int current_front = m_front_index.load( std::memory_order_relaxed );
			m_front_index.store( 1 - current_front, std::memory_order_relaxed );
		}

		std::unique_ptr<std::array<std::vector<T>, 2>> m_buffers;
		std::atomic<int> m_front_index;
		mutable SRWLOCK m_lock = SRWLOCK_INIT;
	};

	enum class actor_type : std::uint8_t {
		player = 0,
		pickup,
		chest,
		weakspot
	};

	class c_player_pawn {
	private:
		struct bone_binding {
			std::uint32_t bone;
			engine::f_vector c_player_pawn::* target;
		};

		static constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 14> skeleton_pairs { {
			{ engine::skeleton::spine_05,    engine::skeleton::neck_01 },
			{ engine::skeleton::spine_05,    engine::skeleton::upperarm_r },
			{ engine::skeleton::spine_05,    engine::skeleton::upperarm_l },
			{ engine::skeleton::upperarm_l,  engine::skeleton::lowerarm_l },
			{ engine::skeleton::lowerarm_l,  engine::skeleton::hand_l },
			{ engine::skeleton::upperarm_r,  engine::skeleton::lowerarm_r },
			{ engine::skeleton::lowerarm_r,  engine::skeleton::hand_r },
			{ engine::skeleton::spine_05,    engine::skeleton::pelvis },
			{ engine::skeleton::pelvis,      engine::skeleton::thigh_r },
			{ engine::skeleton::pelvis,      engine::skeleton::thigh_l },
			{ engine::skeleton::thigh_r,     engine::skeleton::calf_r },
			{ engine::skeleton::calf_r,      engine::skeleton::ik_foot_r },
			{ engine::skeleton::thigh_l,     engine::skeleton::calf_l },
			{ engine::skeleton::calf_l,      engine::skeleton::ik_foot_l },
		} };

		static constexpr std::uint32_t skeleton_bone_span = [ ]( ) {
			std::uint32_t max_index = 0;
			for ( const auto& p : skeleton_pairs ) {
				max_index = p.first > max_index ? p.first : max_index;
				max_index = p.second > max_index ? p.second : max_index;
			}
			return max_index + 1;
			}( );

		static constexpr auto skeleton_unique_bones = [ ]( ) {
			std::array<std::uint32_t, 15> out {};
			std::size_t n = 0;
			for ( const auto& p : skeleton_pairs ) {
				auto add = [ & ]( std::uint32_t b ) {
					for ( std::size_t i = 0; i < n; ++i )
						if ( out[ i ] == b ) return;
					out[ n++ ] = b;
					};
				add( p.first );
				add( p.second );
			}
			return std::make_pair( out, n );
			}( );

		static constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 38> finger_pairs { {
			{ engine::skeleton::hand_l,              engine::skeleton::thumb_01_l },
			{ engine::skeleton::thumb_01_l,          engine::skeleton::thumb_02_l },
			{ engine::skeleton::thumb_02_l,          engine::skeleton::thumb_03_l },
			{ engine::skeleton::hand_l,              engine::skeleton::index_metacarpal_l },
			{ engine::skeleton::index_metacarpal_l,  engine::skeleton::index_01_l },
			{ engine::skeleton::index_01_l,          engine::skeleton::index_02_l },
			{ engine::skeleton::index_02_l,          engine::skeleton::index_03_l },
			{ engine::skeleton::hand_l,              engine::skeleton::middle_metacarpal_l },
			{ engine::skeleton::middle_metacarpal_l, engine::skeleton::middle_01_l },
			{ engine::skeleton::middle_01_l,         engine::skeleton::middle_02_l },
			{ engine::skeleton::middle_02_l,         engine::skeleton::middle_03_l },
			{ engine::skeleton::hand_l,              engine::skeleton::ring_metacarpal_l },
			{ engine::skeleton::ring_metacarpal_l,   engine::skeleton::ring_01_l },
			{ engine::skeleton::ring_01_l,           engine::skeleton::ring_02_l },
			{ engine::skeleton::ring_02_l,           engine::skeleton::ring_03_l },
			{ engine::skeleton::hand_l,              engine::skeleton::pinky_metacarpal_l },
			{ engine::skeleton::pinky_metacarpal_l,  engine::skeleton::pinky_01_l },
			{ engine::skeleton::pinky_01_l,          engine::skeleton::pinky_02_l },
			{ engine::skeleton::pinky_02_l,          engine::skeleton::pinky_03_l },
			{ engine::skeleton::hand_r,              engine::skeleton::thumb_01_r },
			{ engine::skeleton::thumb_01_r,          engine::skeleton::thumb_02_r },
			{ engine::skeleton::thumb_02_r,          engine::skeleton::thumb_03_r },
			{ engine::skeleton::hand_r,              engine::skeleton::index_metacarpal_r },
			{ engine::skeleton::index_metacarpal_r,  engine::skeleton::index_01_r },
			{ engine::skeleton::index_01_r,          engine::skeleton::index_02_r },
			{ engine::skeleton::index_02_r,          engine::skeleton::index_03_r },
			{ engine::skeleton::hand_r,              engine::skeleton::middle_metacarpal_r },
			{ engine::skeleton::middle_metacarpal_r, engine::skeleton::middle_01_r },
			{ engine::skeleton::middle_01_r,         engine::skeleton::middle_02_r },
			{ engine::skeleton::middle_02_r,         engine::skeleton::middle_03_r },
			{ engine::skeleton::hand_r,              engine::skeleton::ring_metacarpal_r },
			{ engine::skeleton::ring_metacarpal_r,   engine::skeleton::ring_01_r },
			{ engine::skeleton::ring_01_r,           engine::skeleton::ring_02_r },
			{ engine::skeleton::ring_02_r,           engine::skeleton::ring_03_r },
			{ engine::skeleton::hand_r,              engine::skeleton::pinky_metacarpal_r },
			{ engine::skeleton::pinky_metacarpal_r,  engine::skeleton::pinky_01_r },
			{ engine::skeleton::pinky_01_r,          engine::skeleton::pinky_02_r },
			{ engine::skeleton::pinky_02_r,          engine::skeleton::pinky_03_r },
		} };

		static constexpr std::uint32_t finger_bone_span = [ ]( ) {
			std::uint32_t max_index = 0;
			for ( const auto& p : finger_pairs ) {
				max_index = p.first > max_index ? p.first : max_index;
				max_index = p.second > max_index ? p.second : max_index;
			}
			return max_index + 1;
			}( );

		static constexpr auto finger_unique_bones = [ ]( ) {
			std::array<std::uint32_t, 42> out {};
			std::size_t n = 0;
			for ( const auto& p : finger_pairs ) {
				auto add = [ & ]( std::uint32_t b ) {
					for ( std::size_t i = 0; i < n; ++i )
						if ( out[ i ] == b ) return;
					out[ n++ ] = b;
					};
				add( p.first );
				add( p.second );
			}
			return std::make_pair( out, n );
			}( );

	public:
		c_player_pawn( ) = default;

		c_player_pawn( engine::a_actor* actor ):
			m_player( reinterpret_cast< engine::a_fort_player_pawn_athena* >( actor ) ) {
		}

		bool update( ) {
			if ( !m_player )
				return false;

			m_gate.tick( );
			const auto now = std::chrono::steady_clock::now( );
			if ( !update_pose( now ) )
				return false;

			const bool refresh_info =
				( now - m_last_info_update ) >= std::chrono::milliseconds( 500 );
			const bool tick_medium = m_gate.on( priority::level::medium );
			const bool tick_low    = m_gate.on( priority::level::low );

			if ( m_weapon ) {
				const auto fire_idx = m_weapon->current_gun_fire_index( );
				m_is_firing         = ( fire_idx != m_gun_fire_index );
				m_gun_fire_index    = fire_idx;
			} else {
				m_is_firing = false;
			}

			if ( tick_medium ) {
				this->m_weapon = m_player->current_weapon( );
				if ( m_weapon ) {
					this->m_ammo_count  = m_weapon->ammo_count( );
					this->m_weapon_core = m_weapon->weapon_core_animation( );
					// bIsReloadingWeapon : 1 at a_fort_weapon + 0x0359,
					// bit 0. Read the containing byte and mask.
					const auto weapon_addr = reinterpret_cast< std::uint64_t >( m_weapon );
					const auto reload_byte = bypass::read< std::uint8_t >( weapon_addr + 0x0359 );
					this->m_is_reloading = ( reload_byte & 0x1u ) != 0;
					if ( m_weapon != m_last_weapon || refresh_info ) {
						this->m_item_definition = m_weapon->weapon_data( );

						if ( m_item_definition ) {
							this->m_weapon_name = m_item_definition->item_name( )->get( );
							while ( !m_weapon_name.empty( ) && std::isspace( static_cast< unsigned char >( m_weapon_name.back( ) ) ) )
								m_weapon_name.pop_back( );

							if ( m_weapon_name.empty( ) ) {
								const auto def_addr  = reinterpret_cast<std::uint64_t>( m_item_definition );
								const auto item_fname = bypass::read<engine::f_name>( def_addr + 0x8 );
								this->m_weapon_name   = engine::emulator::fname_to_string( item_fname, false );
							}

							this->m_weapon_name_wide = utility::ansi_to_wstring( m_weapon_name );
						}
						else {
							this->m_weapon_name.clear( );
							this->m_weapon_name_wide.clear( );
						}

						m_last_weapon = m_weapon;
					}
				}
				else {
					this->m_ammo_count = 0;
					this->m_weapon_core = 34;
					this->m_is_reloading = false;
					this->m_item_definition = nullptr;
					this->m_weapon_name.clear( );
					this->m_weapon_name_wide.clear( );
					m_last_weapon = nullptr;
				}

				this->m_is_in_safe_zone = m_player->b_is_in_safe_zone( );
				this->m_is_skydiving    = m_player->b_is_skydiving( );

				const auto fired_dir = m_player->last_fired_direction( );
				if ( fired_dir.is_valid( ) )
					m_last_fired_direction = fired_dir;
			}

			if ( tick_low ) {
				this->m_skeletal_meshes = m_player->skeletal_meshes( );
				this->m_building_state  = m_player->building_state( );
			}

			if ( refresh_info || !m_player_state ) {
				this->m_player_state = m_player->player_state( );
				if ( m_player_state ) {
					this->m_player_name = m_player_state->get_player_name( );
					this->m_player_name_wide = utility::ansi_to_wstring( m_player_name );
					this->m_platform_name = m_player_state->get_platform_name( );

					{
						std::string lower_name = m_player_name;
						for ( auto& c : lower_name ) c = static_cast< char >( std::tolower( c ) );

						const auto word_match = [ & ]( const char* word ) -> bool {
							const auto wlen = std::strlen( word );
							auto pos = lower_name.find( word );
							while ( pos != std::string::npos ) {
								const bool start_ok = pos == 0 || !std::isalpha( lower_name[ pos - 1 ] );
								const bool end_ok   = pos + wlen >= lower_name.size( ) || !std::isalpha( lower_name[ pos + wlen ] );
								if ( start_ok && end_ok ) return true;
								pos = lower_name.find( word, pos + 1 );
							}
							return false;
						};

						if ( lower_name.find( "tiktok" ) != std::string::npos || word_match( "tt" ) )
							this->m_streamer = streamer_platform::tiktok;
						else if ( lower_name.find( "twitch" ) != std::string::npos || word_match( "ttv" ) )
							this->m_streamer = streamer_platform::twitch;
						else if ( lower_name.find( "youtube" ) != std::string::npos || word_match( "yt" ) )
							this->m_streamer = streamer_platform::youtube;
						else
							this->m_streamer = streamer_platform::none;
					}
					this->m_habanero_component = m_player_state->habanero_component( );

					if ( m_habanero_component ) {
						this->m_rank_progress = m_habanero_component->rank_progress( );
						//this->m_highest_rank = m_habanero_component->highest_rank( );
						//this->m_current_mmr = m_habanero_component->current_mmr( );
						//this->m_current_position_rank = m_habanero_component->current_position_rank( );
					}
					this->m_team_index = m_player_state->team_index( );
				}

				m_last_info_update = now;
			}

			return true;
		}

		bool update_pose( std::chrono::steady_clock::time_point now ) {
			std::tie( m_is_dying, m_is_downed ) = m_player->dying_and_dbno( );

			m_mesh = m_player->mesh( );
			if ( !m_mesh )
				return false;

			m_transforms_array = m_mesh->component_space_transforms_array( );
			if ( !m_transforms_array.is_valid( ) || !update_bones( ) )
				return false;

			update_bounds( );
			m_pose_captured_at = now;

			const auto vel_dt = std::chrono::duration< double >(
				now - m_last_vel_update ).count( );
			const auto raw_vel = m_mesh->component_velocity( );
			if ( m_last_vel_update.time_since_epoch( ).count( ) &&
				vel_dt > 0.0 && vel_dt < 0.5 ) {
				const auto source = raw_vel.is_valid( )
					? raw_vel
					: ( m_root_location - m_prev_root_location ) / vel_dt;
				constexpr float alpha = 0.35f;
				const auto prev_vel = m_velocity;
				m_velocity = source * alpha + m_velocity * ( 1.0 - alpha );
				const float dv_z = static_cast< float >(
					( m_velocity.z - prev_vel.z ) / vel_dt );
				m_accel_z = std::clamp( dv_z, -5000.0f, 500.0f );
			}
			else {
				m_velocity = raw_vel.is_valid( ) ? raw_vel : engine::f_vector { };
				m_accel_z = 0.0f;
			}
			m_prev_root_location = m_root_location;
			m_last_vel_update = now;
			return true;
		}

		bool update_bones( ) {
			if ( !m_mesh )
				return false;

			// bounds_bones keeps pointer-to-member as a local static constexpr so the
			// compiler never evaluates it at class scope (class-scope pointer-to-member
			// constexpr is tricky on some MSVC versions).  read_bone_span and the bone
			// buffer itself live here too — see comment below.
			static constexpr std::array<bone_binding, 9> bounds_bones { {
				{ engine::skeleton::head,       &c_player_pawn::m_head_location  },
				{ engine::skeleton::root,       &c_player_pawn::m_root_location  },
				{ engine::skeleton::pelvis,     &c_player_pawn::m_bounds_pelvis  },
				{ engine::skeleton::foot_l,     &c_player_pawn::m_bounds_foot_l  },
				{ engine::skeleton::foot_r,     &c_player_pawn::m_bounds_foot_r  },
				{ engine::skeleton::hand_l,     &c_player_pawn::m_bounds_hand_l  },
				{ engine::skeleton::hand_r,     &c_player_pawn::m_bounds_hand_r  },
				{ engine::skeleton::lowerarm_l, &c_player_pawn::m_bounds_elbow_l },
				{ engine::skeleton::lowerarm_r, &c_player_pawn::m_bounds_elbow_r },
			} };

			// Span constants derived here from bounds_bones — no class-scope statics needed.
			// bones is a plain stack array: NOT thread_local, NOT static.  That makes it
			// always valid inside a reflectively injected DLL where TLS slots are never
			// populated.  It is also never copied when c_player_pawn objects are copied,
			// eliminating ~9 KB of heap traffic per player per frame.
			static constexpr std::uint32_t bounds_bone_span = [ ]( ) {
				std::uint32_t mx = 0;
				for ( const auto& b : bounds_bones )
					mx = b.bone > mx ? b.bone : mx;
				return mx + 1;
			}( );
			static constexpr std::uint32_t read_bone_span =
				bounds_bone_span > skeleton_bone_span ? bounds_bone_span : skeleton_bone_span;

			if ( read_bone_span > m_transforms_array.size( ) )
				return false;

			engine::f_transform bones[ read_bone_span ] {};
			if ( !m_transforms_array.read_range( 0, read_bone_span, bones ) )
				return false;

			auto component_to_world = m_mesh->component_to_world( );
			component_to_world.fix_scaler( );
			const auto to_world = component_to_world.to_matrix( );

			std::array<engine::f_vector, bounds_bones.size( )> world_locations;
			bool all_sane = true;
			for ( std::size_t i = 0; i < bounds_bones.size( ); ++i ) {
				world_locations[ i ] = to_world.transform_position( bones[ bounds_bones[ i ].bone ].translation );
				all_sane &= is_sane( world_locations[ i ] );
			}

			static constexpr double head_top_padding = 12.0;
			this->m_bounds_head_top =
				world_locations[ 0 ] + engine::f_vector( 0.0, 0.0, head_top_padding );
			all_sane &= is_sane( m_bounds_head_top );
			if ( !all_sane )
				return false;

			for ( std::size_t i = 0; i < bounds_bones.size( ); ++i )
				this->*bounds_bones[ i ].target = world_locations[ i ];

			return update_skeleton( bones, read_bone_span, to_world );
		}

		bool update_skeleton(
			const engine::f_transform* skel_bones,
			std::size_t bone_count,
			const engine::f_matrix& to_world ) {
			if ( !skel_bones || skeleton_bone_span > bone_count )
				return false;

			const auto& unique = skeleton_unique_bones.first;
			const auto  unique_count = skeleton_unique_bones.second;
			engine::f_vector world_cache[ 15 ] {};
			for ( std::size_t i = 0; i < unique_count; ++i )
				world_cache[ i ] = to_world.transform_position( skel_bones[ unique[ i ] ].translation );

			auto lookup = [ & ]( std::uint32_t bone ) -> const engine::f_vector& {
				for ( std::size_t i = 0; i < unique_count; ++i )
					if ( unique[ i ] == bone ) return world_cache[ i ];
				return world_cache[ 0 ];
				};

			for ( std::size_t i = 0; i < skeleton_pairs.size( ); ++i ) {
				m_skeleton_positions[ i ] = {
					lookup( skeleton_pairs[ i ].first ),
					lookup( skeleton_pairs[ i ].second )
				};
			}

			if ( finger_bone_span <= bone_count ) {
				const auto& funique       = finger_unique_bones.first;
				const auto  funique_count = finger_unique_bones.second;
				engine::f_vector finger_cache[ 42 ] {};
				for ( std::size_t i = 0; i < funique_count; ++i )
					finger_cache[ i ] = to_world.transform_position( skel_bones[ funique[ i ] ].translation );

				auto flookup = [ & ]( std::uint32_t bone ) -> const engine::f_vector& {
					for ( std::size_t i = 0; i < funique_count; ++i )
						if ( funique[ i ] == bone ) return finger_cache[ i ];
					return finger_cache[ 0 ];
					};

				for ( std::size_t i = 0; i < finger_pairs.size( ); ++i ) {
					m_finger_positions[ i ] = {
						flookup( finger_pairs[ i ].first ),
						flookup( finger_pairs[ i ].second )
					};
				}
			}

			return true;
		}

		engine::a_fort_player_pawn_athena* m_player { nullptr };
		engine::a_player_state* m_player_state { nullptr };
		engine::u_fort_player_state_component_habanero* m_habanero_component { nullptr };
		engine::u_skeletal_mesh_component* m_mesh { nullptr };
		engine::a_fort_weapon* m_weapon { nullptr };
		engine::u_fort_weapon_item_definition* m_item_definition { nullptr };
		engine::t_array< engine::u_skeletal_mesh_component* > m_skeletal_meshes { };
		priority::c_gate m_gate;

		std::uint8_t m_building_state;
		std::int32_t m_rank_progress;
		std::int32_t m_highest_rank { 0 };
		std::int32_t m_current_mmr { 0 };
		std::int32_t m_current_position_rank { 0 };
		std::uint8_t m_team_index { 0 };
		bool m_is_in_safe_zone { true };
		bool m_is_skydiving { false };
		std::uint8_t m_weapon_core { 34 };
		enum class streamer_platform : std::uint8_t {
			none = 0,
			tiktok,
			twitch,
			youtube,
		};

		std::string m_platform_name;
		std::string m_player_name;
		std::wstring m_player_name_wide;
		streamer_platform m_streamer { streamer_platform::none };
		bool m_is_visible = true;
		bool m_is_dying = false;
		bool m_is_downed = false;

		// ── Firing state (mirrors cache.hxx logic used for the local player) ────
		bool             m_is_firing           { false };    // true for one frame when gun_fire_index advances
		int              m_gun_fire_index      { 0 };        // previous-frame value for delta detection
		// Reloading — bit 0 of the byte at a_fort_weapon + 0x0359
		// (`bIsReloadingWeapon : 1`). Populated every medium tick.
		bool             m_is_reloading        { false };
		engine::f_vector m_last_fired_direction { };         // last bullet direction from this pawn
		float            m_last_seen_fired_time { 0.0f };    // baseline for detecting new shots per-pawn

		std::string m_weapon_name;
		std::wstring m_weapon_name_wide;
		std::uint32_t m_ammo_count;
		std::uint32_t m_magazine_size;
		engine::e_fort_rarity m_rarity;
		engine::f_vector m_velocity { };
		float            m_accel_z  { 0.0f };

		engine::t_array<engine::f_transform> m_transforms_array { };
		engine::f_rotator m_root_rotation { };
		engine::f_vector m_head_location { };
		engine::f_vector m_root_location { };
		engine::f_vector m_bounds_head_top { };
		engine::f_vector m_bounds_pelvis { };
		engine::f_vector m_bounds_foot_l { };
		engine::f_vector m_bounds_foot_r { };
		engine::f_vector m_bounds_hand_l { };
		engine::f_vector m_bounds_hand_r { };
		engine::f_vector m_bounds_elbow_l { };
		engine::f_vector m_bounds_elbow_r { };
		engine::fbox_sphere_bounds m_bounds { };

		std::array<std::pair<engine::f_vector, engine::f_vector>, 14> m_skeleton_positions {};
		std::array<std::pair<engine::f_vector, engine::f_vector>, 38> m_finger_positions {};
		std::chrono::steady_clock::time_point m_last_seen { std::chrono::steady_clock::now( ) };
		std::chrono::steady_clock::time_point m_pose_captured_at {};
		std::chrono::steady_clock::time_point m_last_info_update {};
		std::chrono::steady_clock::time_point m_last_vel_update   {};
		engine::f_vector                      m_prev_root_location { };
		engine::a_fort_weapon*             m_last_weapon   { nullptr };
		engine::u_skeletal_mesh_component* m_material_mesh { nullptr };
		std::vector<engine::u_material_interface*> m_original_materials {};
		bool m_material_applied { false };

	private:
		static bool is_sane( const engine::f_vector& v ) {
			if ( !std::isfinite( v.x ) || !std::isfinite( v.y ) || !std::isfinite( v.z ) )
				return false;
			return v.x != 0.0 || v.y != 0.0 || v.z != 0.0;
		}

		void update_bounds( ) {
			const auto min_d = [ ]( double a, double b ) { return a < b ? a : b; };
			const auto max_d = [ ]( double a, double b ) { return a > b ? a : b; };

			const engine::f_vector* bound_points[ ] = {
				&m_bounds_head_top, &m_root_location, &m_bounds_pelvis,
				&m_bounds_foot_l, &m_bounds_foot_r,
				&m_bounds_hand_l, &m_bounds_hand_r,
				&m_bounds_elbow_l, &m_bounds_elbow_r,
			};

			engine::f_vector lo = *bound_points[ 0 ];
			engine::f_vector hi = *bound_points[ 0 ];
			for ( const auto* point : bound_points ) {
				lo.x = min_d( lo.x, point->x ); hi.x = max_d( hi.x, point->x );
				lo.y = min_d( lo.y, point->y ); hi.y = max_d( hi.y, point->y );
				lo.z = min_d( lo.z, point->z ); hi.z = max_d( hi.z, point->z );
			}

			static constexpr double top_padding = 5.0;
			hi.z += top_padding;

			static constexpr double horizontal_extent_ratio = 0.165;
			const double bounds_height = hi.z - lo.z;
			const double horizontal_extent = bounds_height * horizontal_extent_ratio;

			lo.x = min_d( lo.x, m_bounds_pelvis.x - horizontal_extent );
			hi.x = max_d( hi.x, m_bounds_pelvis.x + horizontal_extent );
			lo.y = min_d( lo.y, m_bounds_pelvis.y - horizontal_extent );
			hi.y = max_d( hi.y, m_bounds_pelvis.y + horizontal_extent );

			this->m_bounds.orgin = engine::f_vector(
				( lo.x + hi.x ) * 0.5, ( lo.y + hi.y ) * 0.5, ( lo.z + hi.z ) * 0.5 );
			this->m_bounds.box_extent = engine::f_vector(
				( hi.x - lo.x ) * 0.5, ( hi.y - lo.y ) * 0.5, ( hi.z - lo.z ) * 0.5 );
			this->m_bounds.sphere_radius = std::sqrt(
				m_bounds.box_extent.x * m_bounds.box_extent.x +
				m_bounds.box_extent.y * m_bounds.box_extent.y +
				m_bounds.box_extent.z * m_bounds.box_extent.z );
		}
	};

	class c_pickup_pawn {
	public:
		c_pickup_pawn( ) = default;

		c_pickup_pawn( engine::a_actor* actor ):
			m_pickup( reinterpret_cast< engine::a_fort_pickup* >( actor ) ) {
		}

		bool update( ) {
			if ( !m_pickup )
				return false;

			this->m_root_component = m_pickup->root_component( );
			if ( !m_root_component )
				return false;

			this->m_relative_location = m_root_component->relative_location( );

			if ( m_pickup_name.empty( ) ) {
				auto* item_entry = m_pickup->primary_pickup_item_entry( );
				if ( item_entry ) {
					auto* item_definition = item_entry->item_definition( );
					if ( item_definition ) {
						auto* ftext = item_definition->item_name( );
						if ( ftext ) {
							m_pickup_name = ftext->get( );
							m_pickup_name_wide = std::wstring(
								m_pickup_name.begin( ), m_pickup_name.end( ) );
						}
					}
				}
			}

			return true;
		}

		engine::a_fort_pickup* m_pickup { nullptr };
		engine::u_scene_component* m_root_component { nullptr };
		engine::f_vector m_relative_location { };
		std::string m_pickup_name { };
		std::wstring m_pickup_name_wide { };
	private:

	};

	class c_chest_pawn {
	public:
		c_chest_pawn( ) = default;

		c_chest_pawn( engine::a_actor* actor ):
			m_chest( reinterpret_cast< engine::a_building_container* >( actor ) ) {
		}

		bool update( ) {
			if ( !m_chest )
				return false;

			this->m_root_component = m_chest->root_component( );
			if ( !m_root_component )
				return false;

			this->m_relative_location = m_root_component->relative_location( );
			return true;
		}

		engine::a_building_container* m_chest { nullptr };
		engine::u_scene_component* m_root_component { nullptr };
		engine::f_vector m_relative_location { };
	private:

	};

	class c_weakspot_pawn {
	public:
		c_weakspot_pawn( ) = default;

		c_weakspot_pawn( engine::a_actor* actor ):
			m_weakspot( reinterpret_cast< engine::a_building_weakspot* >( actor ) ) {
		}

		bool update( ) {
			if ( !m_weakspot )
				return false;

			this->m_root_component = m_weakspot->root_component( );
			if ( !m_root_component )
				return false;

			this->m_relative_location = m_root_component->relative_location( );
			return true;
		}

		engine::a_building_weakspot* m_weakspot { nullptr };
		engine::u_scene_component* m_root_component { nullptr };
		engine::f_vector m_relative_location { };
	private:

	};

	// Plain data struct — populated at shot-detection time, animated by tick().
	// No raw pointers to actors/weapons; all game reads happen before construction.
	class c_bullet_tracer {
	public:
		bool is_duplicate( const std::vector< c_bullet_tracer >& bullet_tracers, double threshold = 0.30 ) const {
			for ( const auto& tracer : bullet_tracers ) {
				if ( tracer.m_start_location.distance_to( m_start_location ) < threshold &&
					tracer.m_end_location.distance_to( m_end_location ) < threshold )
					return true;
			}
			return false;
		}

		// Advance the tracer by dt seconds.  Returns true while still alive.
		bool tick( float dt ) {
			m_lifetime += dt;

			// Head travel: fraction of the beam that has been "drawn" so far.
			const float head_arrive =
				config::bullet_tracers::lifetime * 0.45f /
				max( config::bullet_tracers::travel_speed, 0.1f );

			m_travel_t = ( head_arrive > 0.0f )
				? min( 1.0f, m_lifetime / head_arrive )
				: 1.0f;

			// Alpha fade: full opacity, then eases out over fade_duration seconds
			// before the tracer expires.
			if ( config::bullet_tracers::fade_out ) {
				const float dur   = max( config::bullet_tracers::fade_duration, 0.05f );
				const float start = config::bullet_tracers::lifetime - dur;
				if ( m_lifetime >= start ) {
					const float t = std::clamp(
						( m_lifetime - start ) / dur, 0.0f, 1.0f );
					// quadratic ease-out: smooth deceleration into full transparency
					m_alpha = ( 1.0f - t ) * ( 1.0f - t );
				} else {
					m_alpha = 1.0f;
				}
			}

			return !is_expired( );
		}

		bool is_expired( ) const {
			return m_lifetime >= config::bullet_tracers::lifetime;
		}

		engine::f_vector m_start_location { };
		engine::f_vector m_end_location   { };
		float            m_lifetime { 0.0f };
		float            m_alpha    { 1.0f };
		float            m_travel_t { 0.0f };  // [0,1] how far the beam head has advanced
		bool             m_is_enemy { false }; // true = fired by a remote player, false = local shot
	};
}
