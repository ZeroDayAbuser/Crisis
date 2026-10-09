#pragma once

#include "triggerbot.hxx"
#include "memory.hxx"

namespace aimbot {

	struct target_candidate_t {
		std::optional< cache::c_player_pawn > m_player;
		engine::f_vector2d m_aim_position { };
		engine::f_vector2d m_root_position { };
		double m_world_distance { 0.0 };
		bool m_is_visible { false };
		bool m_projection_valid { false };
	};

	class c_aimbot {
	public:

		void start( );
		bool is_active( ) const {
			return m_active;
		}

		float get_fov( ) const {
			return m_aimbot_fov;
		}


		std::optional< cache::c_player_pawn > get_target( );

		void render( );

		bool target( );

		void add( const cache::c_player_pawn& player_pawn );

		bool is_valid(
			const cache::c_player_pawn& player_pawn ) const;

		void trigger( );
	private:
		enum class e_curve_algorithm : std::uint8_t {
			none,
			bezier,
			bspline,
			chaikin,
			staggered
		};

		struct curve_visual_snapshot_t {
			curve::c_curve_state state { };
			double target_x { 0.0 };
			double target_y { 0.0 };
			double progress { 0.0 };
			e_curve_algorithm algorithm { e_curve_algorithm::none };
			bool valid { false };
			engine::f_vector predicted_position { };
			bool has_prediction { false };
		};

		std::mutex m_mutex;

		std::uint32_t m_candidate_count { 0 };
		static constexpr std::uint32_t m_max_candidates = 100;
		target_candidate_t m_candidates[ m_max_candidates ] { };

		float m_aimbot_fov { 0.0f };
		engine::a_fort_player_pawn_athena* m_last_target { nullptr };
		triggerbot::c_triggerbot m_triggerbot;
		bool m_active { false };
		bool m_was_holding_key { false };

		curve::c_curve_state m_curve_state { };
		std::chrono::steady_clock::time_point m_last_rotation_update { };
		bool m_rotation_timing_initialized { false };

		double m_curve_visual_target_x { 0.0 };
		double m_curve_visual_target_y { 0.0 };
		double m_curve_visual_progress { 0.0 };
		e_curve_algorithm m_curve_visual_algorithm { e_curve_algorithm::none };
		bool m_curve_visual_valid { false };

		engine::f_vector m_predicted_position { };
		bool m_has_prediction { false };









		bool is_valid_sticky(
			const cache::c_player_pawn& player_pawn ) const;
		std::optional< cache::c_player_pawn > get_targeted_player( );
		e_curve_algorithm active_curve_algorithm( );
		bool evaluate_curve_point(
			e_curve_algorithm algorithm,
			const curve::c_curve_state& state,
			double target_x,
			double target_y,
			double t,
			double& output_x,
			double& output_y );
		void draw_curve_path(
			const bypass::overlay::point_t& start,
			const bypass::overlay::point_t& end,
			const curve_visual_snapshot_t& snapshot,
			bool show_progress );
		static std::optional< engine::f_rotator >
			camera_space_rotation_error(
				const engine::f_vector& aim_location );
		double rotation_delta_time( );
		engine::f_rotator smooth_rotation_error(
			const engine::f_vector& aim_location,
			const engine::f_rotator& angular_error,
			bool target_visible );
		bool apply_rotation_input(
			const engine::f_vector& aim_location,
			bool target_visible );
	};
}
