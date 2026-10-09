#pragma once

namespace cache {
	static constexpr auto world_sleep     = ch::milliseconds( 500 );
	static constexpr auto player_sleep   = ch::milliseconds( 8 );
	static constexpr auto local_sleep    = ch::milliseconds( 8 );
	static constexpr auto items_sleep    = ch::milliseconds( 50 );
	extern std::atomic< std::uint32_t > monitor_refresh_rate;
	void initialize_monitor_refresh_rate( );
	[[nodiscard]] ch::milliseconds perf_sleep( ch::milliseconds baseline );
	extern engine::f_vector camera_location;
	extern engine::f_rotator camera_rotation;
	extern float camera_fov;

	extern engine::f_vector camera_axis_right;
	extern engine::f_vector camera_axis_up;
	extern engine::f_vector camera_axis_forward;
	extern float camera_projection_scale;

	struct camera_snapshot_t {
		engine::f_vector location { };
		engine::f_rotator rotation { };
		engine::f_vector axis_right { };
		engine::f_vector axis_up { };
		engine::f_vector axis_forward { };
		float fov { };
		float projection_scale { };
		std::chrono::steady_clock::time_point captured_at { };
		bool valid { };
	};

	extern std::shared_mutex camera_snapshot_mutex;
	extern camera_snapshot_t camera_snapshot;




	[[nodiscard]] camera_snapshot_t get_camera_snapshot( );
	engine::f_vector get_published_camera_location( );
	bool apply_camera_snapshot( const camera_snapshot_t& snapshot );
	struct visual_snapshot_t {
		std::vector< c_player_pawn > players;
	};

	struct items_snapshot_t {
		std::vector< c_pickup_pawn >   pickups;
		std::vector< c_chest_pawn >    chests;
		std::vector< c_weakspot_pawn > weakspots;
	};

	extern std::shared_ptr< const visual_snapshot_t > visual_snapshot;
	extern std::shared_ptr< const items_snapshot_t > items_snapshot;

	[[nodiscard]] std::shared_ptr< const items_snapshot_t >
		get_items_snapshot( );
	[[nodiscard]] std::shared_ptr< const visual_snapshot_t >
		get_visual_snapshot( );
	struct holo_scalar_parameter_t {
		const wchar_t* label { };
		float value { };
		engine::f_name name { };
	};

	struct holo_vector_parameter_t {
		const wchar_t* label { };
		engine::flinear_color value { };
		engine::f_name name { };
	};

	class c_cache {
	public:







		~c_cache( );
		void setup( );
		bool initialize( );
		bool sample_camera( camera_snapshot_t& next );
		std::vector< c_player_pawn > get_players( );
		std::shared_ptr< const items_snapshot_t > get_items( );
		std::vector< c_bullet_tracer > get_bullet_tracers( );
		std::atomic< engine::u_fort_kismet_library* >
			m_fort_library { nullptr };
		std::atomic< engine::u_kismet_string_library* >
			m_string_library { nullptr };
		std::atomic< engine::u_world* > m_world { nullptr };
		std::atomic< engine::a_game_state_base* > m_game_state { nullptr };
		std::atomic< engine::cached_view_t > m_camera { };
		std::atomic< engine::u_game_instance* > m_game_instance { nullptr };
		std::atomic< engine::u_localplayer* > m_local_player { nullptr };
		std::atomic< engine::a_player_controller* >
			m_player_controller { nullptr };
		std::atomic< engine::a_fort_player_pawn_athena* >
			m_acknowledged_pawn { nullptr };
		std::atomic< engine::a_fort_weapon* > m_current_weapon { nullptr };
		std::atomic< float > m_mouse_sensitivity { 1.0f };
		std::atomic< std::uint8_t > m_local_team_index { 0 };
		std::atomic< bool > m_is_firing { 0 };
		std::atomic< bool > m_last_gun_fire_index { 0 };
		std::int32_t find_offset( const wchar_t* path, const wchar_t* field );

	private:
		static constexpr auto minimum_sleep = ch::milliseconds( 1 );
		static constexpr std::uint32_t maximum_levels = 2048;
		static constexpr std::uint32_t maximum_actors_per_level = 262144;
		static constexpr std::uint32_t maximum_single_read_fallback = 16;


		static void pace(
			std::chrono::steady_clock::time_point iteration_start,
			std::chrono::steady_clock::duration interval );
		template< typename type, typename callback_type >
		static bool for_each_array(
			engine::t_array< type > array,
			std::uint32_t maximum_count,
			callback_type&& callback ) {
			if ( !array.is_valid( ) )
				return false;

			const auto count = array.size( );
			if ( !count )
				return true;
			if ( count > maximum_count )
				return false;

			constexpr std::uint32_t batch_capacity = 128;
			type values[ batch_capacity ] { };

			for ( std::uint32_t first = 0; first < count; ) {
				const auto remaining = count - first;
				const auto number = remaining < batch_capacity
					? remaining
					: batch_capacity;

				if ( array.read_range( first, number, values ) ) {
					for ( std::uint32_t index = 0; index < number; ++index )
						callback( values[ index ] );
				}
				else {
					if ( number > maximum_single_read_fallback )
						return false;
					for ( std::uint32_t index = 0; index < number; ++index ) {
						type value { };
						if ( !array.read_range( first + index, 1, &value ) )
							return false;
						callback( value );
					}
				}

				first += number;
			}

			return true;
		}

		std::atomic< bool > m_running { false };
		std::thread m_world_thread;
		std::thread m_players_thread;
		std::thread m_local_thread;
		std::thread m_items_thread;

		static constexpr auto invalid_warn_threshold = ch::seconds( 15 );

		struct ptr_staleness_t {
			std::chrono::steady_clock::time_point m_since { };


			void mark_invalid( const char* name );
			void mark_valid( );
		};

		ptr_staleness_t m_stale_world { };
		ptr_staleness_t m_stale_game_state { };
		ptr_staleness_t m_stale_game_instance { };
		ptr_staleness_t m_stale_local_player { };
		ptr_staleness_t m_stale_player_controller { };
		// Camera-side staleness. Silent failures in sample_camera()
		// cascade into "no players render" because the fallback
		// snapshot is stale — the flags below surface the reason
		// (missing view state, bad matrix read, out-of-bounds
		// values) once it's been broken for the threshold window.
		ptr_staleness_t m_stale_camera_view_states { };
		ptr_staleness_t m_stale_camera_matrices    { };

		c_double_buffer< std::pair< engine::a_actor*, actor_type > > m_actors;

		// Bullet tracers — written only by update_local, published via double buffer.
		c_double_buffer< c_bullet_tracer >    m_bullet_tracers;
		std::vector< c_bullet_tracer >        m_active_tracers;
		std::chrono::steady_clock::time_point m_last_tracer_tick { };
		float                                 m_last_local_fired_time { 0.f };  // baseline for last_fired_time delta

		// Cross-thread inbox for enemy-fired tracer emissions. Producers = per-pawn
		// actors tick (c_player_pawn::update). Consumer = update_local, which
		// drains this into m_active_tracers under max_count.
		std::mutex                            m_pending_enemy_tracers_mutex;
		std::vector< c_bullet_tracer >        m_pending_enemy_tracers;

		void update_world( );
		void update_local( );
		void update_players( );
		void update_items( );





	};
}
