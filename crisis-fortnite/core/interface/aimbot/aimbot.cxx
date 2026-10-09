#include <deps/includes.h>

namespace aimbot {
	void c_aimbot::start( ) {
		std::lock_guard< std::mutex > lock( m_mutex );

		m_candidate_count = 0;

		const auto camera_fov = static_cast< double >( cache::camera_fov );
		if ( std::isfinite( camera_fov ) && camera_fov > 0.0 ) {
			m_aimbot_fov = static_cast< float >(
				( static_cast< double >( config::aimbot::field_of_view ) *
					static_cast< double >( gui::screen_width_center ) /
					camera_fov ) * 0.5 );
		}
		else {
			m_aimbot_fov = 0.0f;
		}

		const auto acknowledged_pawn =
			g_cache->m_acknowledged_pawn.load( std::memory_order_acquire );
		const auto player_controller =
			g_cache->m_player_controller.load( std::memory_order_acquire );

		m_active = acknowledged_pawn &&
			player_controller &&
			utility::is_valid_pointer( player_controller ) &&
			config::aimbot::enabled;
	}
	std::optional< cache::c_player_pawn > c_aimbot::get_target( ) {
		std::lock_guard< std::mutex > lock( m_mutex );
		return get_targeted_player( );
	}
	void c_aimbot::render( ) {
		std::optional< cache::c_player_pawn > player;
		curve_visual_snapshot_t curve_snapshot { };
		float aimbot_fov = 0.0f;
		bool active = false;

		{
			std::lock_guard< std::mutex > lock( m_mutex );
			active = m_active;
			aimbot_fov = m_aimbot_fov;

			if ( active )
				player = get_targeted_player( );

			curve_snapshot.state = m_curve_state;
			curve_snapshot.target_x = m_curve_visual_target_x;
			curve_snapshot.target_y = m_curve_visual_target_y;
			curve_snapshot.progress = m_curve_visual_progress;
			curve_snapshot.algorithm = m_curve_visual_algorithm;
			curve_snapshot.valid = m_curve_visual_valid;
			curve_snapshot.predicted_position = m_predicted_position;
			curve_snapshot.has_prediction = m_has_prediction;
		}

		if ( !active )
			return;

		const auto col_accent = menu::current_accent( );
		const auto col_pastel = menu::pastel_accent( col_accent );

		if ( config::aimbot::visuals::field_of_view ) {
			const bypass::overlay::color_t fov_color = menu::pastel_accent( col_accent );

			bypass::overlay::circle(
				bypass::overlay::point_t {
					static_cast< float >( gui::screen_width_center ),
					static_cast< float >( gui::screen_height_center )
				},
				aimbot_fov,
				fov_color,
				1.5f );
		}

		if ( !config::aimbot::visuals::curve_line || !player )
			return;

		const auto head_screen =
			engine::world_to_screen( player->m_head_location );

		if ( !head_screen ||
			!std::isfinite( head_screen->x ) ||
			!std::isfinite( head_screen->y ) )
			return;

		const bypass::overlay::point_t head_point {
			static_cast< float >( head_screen->x ),
			static_cast< float >( head_screen->y )
		};

		// Resolve where the aimbot is actually targeting this frame.
		// When prediction is active project the predicted world position;
		// fall back to the raw head if the projection is invalid.
		bypass::overlay::point_t target_point = head_point;
		if ( curve_snapshot.has_prediction ) {
			const auto pred_screen =
				engine::world_to_screen( curve_snapshot.predicted_position );
			if ( pred_screen &&
				std::isfinite( pred_screen->x ) &&
				std::isfinite( pred_screen->y ) ) {
				target_point = bypass::overlay::point_t {
					static_cast< float >( pred_screen->x ),
					static_cast< float >( pred_screen->y )
				};

				// Thin connector: raw head → predicted point.
				const bypass::overlay::color_t connector_color { col_pastel.r, col_pastel.g, col_pastel.b, 80 };
				bypass::overlay::line(
					head_point, target_point, connector_color, 1.0f );

				if ( config::aimbot::visuals::prediction_dot ) {
					constexpr int   n_teeth = 7;
					constexpr float outer_r = 6.0f;
					constexpr float inner_r = 3.8f;
					constexpr float tooth_frac = 0.42f;
					const bypass::overlay::color_t gear_color { col_accent.r, col_accent.g, col_accent.b, 190 };
					constexpr float two_pi = 6.28318530717958f;
					constexpr float period = two_pi / n_teeth;

					using pt = bypass::overlay::point_t;
					pt v[ n_teeth ], r[ n_teeth ], f[ n_teeth ];

					for ( int t = 0; t < n_teeth; ++t ) {
						const float a0 = t * period;
						const float a1 = a0 + period * ( 1.f - tooth_frac ) * 0.5f;
						const float a2 = a0 + period * ( 1.f + tooth_frac ) * 0.5f;
						v[ t ] = { target_point.x + inner_r * std::cos( a0 ), target_point.y + inner_r * std::sin( a0 ) };
						r[ t ] = { target_point.x + outer_r * std::cos( a1 ), target_point.y + outer_r * std::sin( a1 ) };
						f[ t ] = { target_point.x + outer_r * std::cos( a2 ), target_point.y + outer_r * std::sin( a2 ) };
					}

					for ( int t = 0; t < n_teeth; ++t ) {
						bypass::overlay::line( v[ t ], r[ t ], gear_color, 1.0f );
						bypass::overlay::line( r[ t ], f[ t ], gear_color, 1.0f );
						bypass::overlay::line( f[ t ], v[ ( t + 1 ) % n_teeth ], gear_color, 1.0f );
					}
				}
			}
		}

		const bypass::overlay::point_t camera_center {
			static_cast< float >( gui::screen_width_center ),
			static_cast< float >( gui::screen_height_center )
		};

		bypass::overlay::point_t line_origin = camera_center;
		{
			const auto weapon = g_cache->m_current_weapon.load( std::memory_order_acquire );
			if ( weapon ) {
				const auto muzzle_world  = weapon->get_muzzle_location( 0 );
				const auto muzzle_screen = engine::world_to_screen( muzzle_world );
				if ( muzzle_screen &&
					std::isfinite( muzzle_screen->x ) &&
					std::isfinite( muzzle_screen->y ) ) {
					line_origin = {
						static_cast< float >( muzzle_screen->x ),
						static_cast< float >( muzzle_screen->y )
					};
				}
			}
		}

		if ( curve_snapshot.valid ) {
			draw_curve_path(
				line_origin,
				target_point,
				curve_snapshot,
				true );
		}
		else if ( config::aimbot::curve ) {
			const bypass::overlay::color_t line_color { col_accent.r, col_accent.g, col_accent.b, 200 };
			bypass::overlay::line( line_origin, target_point, line_color, 1.2f );
		}
	}
	bool c_aimbot::target( ) {
		std::lock_guard< std::mutex > lock( m_mutex );

		if ( !m_active )
			return false;

		const bool key_down = config::aimbot::aim_key != 0 &&
			( GetAsyncKeyState( config::aimbot::aim_key ) & 0x8000 ) != 0;
		const bool pressed_this_frame = key_down && !m_was_holding_key;
		m_was_holding_key = key_down;

		if ( !key_down ) {
			m_last_target = nullptr;
			m_curve_state.reset( );
			m_curve_visual_valid = false;
			m_curve_visual_algorithm = e_curve_algorithm::none;
			m_rotation_timing_initialized = false;
			m_has_prediction = false;
			return true;
		}

		auto player = get_targeted_player( );
		if ( !player )
			return false;

		const bool target_changed =
			pressed_this_frame ||
			( m_last_target && m_last_target != player->m_player );

		if ( target_changed ) {
			m_curve_state.reset( );
			m_curve_visual_valid = false;
			m_curve_visual_algorithm = e_curve_algorithm::none;
			m_rotation_timing_initialized = false;
		}

		m_last_target = player->m_player;
		const auto predicted = prediction::apply( *player, player->m_head_location );
		m_predicted_position = predicted;
		m_has_prediction =
			config::aimbot::prediction_type !=
			static_cast< int >( prediction::e_type::none );
		return apply_rotation_input( predicted, player->m_is_visible );
	}
	void c_aimbot::add( const cache::c_player_pawn& player_pawn ) {
		std::lock_guard< std::mutex > lock( m_mutex );

		if ( m_candidate_count >= m_max_candidates )
			return;

		if ( !utility::is_valid_pointer( player_pawn.m_player ) )
			return;

		const auto distance = cache::camera_location.distance_to(
			player_pawn.m_head_location );

		if ( !std::isfinite( distance ) )
			return;

		const auto head_screen =
			engine::world_to_screen( player_pawn.m_head_location );
		const auto root_screen =
			engine::world_to_screen( player_pawn.m_root_location );

		const bool head_valid =
			head_screen &&
			std::isfinite( head_screen->x ) &&
			std::isfinite( head_screen->y );

		const bool root_valid =
			root_screen &&
			std::isfinite( root_screen->x ) &&
			std::isfinite( root_screen->y );

		m_candidates[ m_candidate_count++ ] = {
			std::optional( player_pawn ),
			head_valid ? *head_screen : engine::f_vector2d { },
			root_valid ? *root_screen :
				( head_valid ? *head_screen : engine::f_vector2d { } ),
			distance,
			player_pawn.m_is_visible,
			head_valid
		};
	}
	bool c_aimbot::is_valid(
		const cache::c_player_pawn& player_pawn ) const {
		if ( player_pawn.m_is_dying )
			return false;

		const auto head_screen =
			engine::world_to_screen( player_pawn.m_head_location );

		if ( !head_screen ||
			!std::isfinite( head_screen->x ) ||
			!std::isfinite( head_screen->y ) )
			return false;

		return utility::is_on_screen(
			head_screen->x,
			head_screen->y ) &&
			utility::in_fov_circle(
				head_screen->x,
				head_screen->y,
				m_aimbot_fov );
	}
	void c_aimbot::trigger( ) {
		std::lock_guard< std::mutex > lock( m_mutex );
		if ( !m_active )
			m_triggerbot.reset( );
		else
			m_triggerbot.tick( get_targeted_player( ) );
	}
	bool c_aimbot::is_valid_sticky(
		const cache::c_player_pawn& player_pawn ) const {
		return !player_pawn.m_is_dying &&
			( config::aimbot::aim_at_knocked || !player_pawn.m_is_downed ) &&
			utility::is_valid_pointer( player_pawn.m_player );
	}
	std::optional< cache::c_player_pawn >
		c_aimbot::get_targeted_player( ) {
		if ( m_candidate_count == 0 )
			return std::nullopt;

		if ( m_last_target ) {
			for ( std::uint32_t i = 0; i < m_candidate_count; ++i ) {
				const auto& candidate = m_candidates[ i ];
				if ( !candidate.m_player )
					continue;
				if ( candidate.m_player->m_player != m_last_target )
					continue;
				if ( config::aimbot::sticky_target ) {
					if ( utility::is_valid_pointer( candidate.m_player->m_player ) &&
						( config::aimbot::aim_at_knocked || !candidate.m_player->m_is_downed ) &&
						candidate.m_world_distance <= config::aimbot::max_distance ) {
						if ( !config::aimbot::visible_only || candidate.m_is_visible )
							return candidate.m_player;
					}
					return std::nullopt;
				}
				if ( is_valid_sticky( *candidate.m_player ) )
					return candidate.m_player;
				break; // dying + non-sticky: fall through to reselect
			}
			// Sticky: target not in candidate list — don't auto-switch.
			if ( config::aimbot::sticky_target )
				return std::nullopt;
		}

		auto best_score = DBL_MAX;
		std::optional< cache::c_player_pawn > best_player;

		for ( std::uint32_t i = 0; i < m_candidate_count; ++i ) {
			const auto& candidate = m_candidates[ i ];

			if ( !candidate.m_player || !candidate.m_projection_valid )
				continue;
			if ( !utility::is_valid_pointer( candidate.m_player->m_player ) ||
				candidate.m_player->m_is_dying )
				continue;
			if ( ( !config::aimbot::aim_at_knocked && candidate.m_player->m_is_downed ) ||
				( config::aimbot::visible_only && !candidate.m_is_visible ) ||
				candidate.m_world_distance > config::aimbot::max_distance )
				continue;
			if ( !utility::is_on_screen(
				candidate.m_aim_position.x,
				candidate.m_aim_position.y ) )
				continue;
			if ( !utility::in_fov_circle(
				candidate.m_aim_position.x,
				candidate.m_aim_position.y,
				m_aimbot_fov ) )
				continue;

			const auto distance_to_center = utility::distance_to_center(
				candidate.m_aim_position.x,
				candidate.m_aim_position.y );
			double score = distance_to_center;
			if ( config::aimbot::target_priority == 1 ) {
				score = candidate.m_world_distance;
			}
			else if ( config::aimbot::target_priority == 2 ) {
				const double distance_score = candidate.m_world_distance /
					( std::max ) ( 1.0, static_cast< double >( config::aimbot::max_distance ) );
				const double crosshair_score = distance_to_center /
					( std::max ) ( 1.0, static_cast< double >( m_aimbot_fov ) );
				score = distance_score * 0.57 + crosshair_score * 0.27;
			}

			if ( score < best_score ) {
				best_score = score;
				best_player = candidate.m_player;
			}
		}

		return best_player;
	}
	c_aimbot::e_curve_algorithm c_aimbot::active_curve_algorithm( ) {
		if ( config::aimbot::bezier_curve )
			return e_curve_algorithm::bezier;
		if ( config::aimbot::bspline_curve )
			return e_curve_algorithm::bspline;
		if ( config::aimbot::chaikin_curve )
			return e_curve_algorithm::chaikin;
		if ( config::aimbot::staggered_curve )
			return e_curve_algorithm::staggered;
		return e_curve_algorithm::none;
	}
	bool c_aimbot::evaluate_curve_point(
		const e_curve_algorithm algorithm,
		const curve::c_curve_state& state,
		const double target_x,
		const double target_y,
		double t,
		double& out_x,
		double& out_y ) {
		if ( t < 0.0 ) t = 0.0;
		if ( t > 1.0 ) t = 1.0;

		switch ( algorithm ) {
		case e_curve_algorithm::bezier:
			curve::bezier_point(
				0.0, 0.0,
				state.ctrl_x, state.ctrl_y,
				target_x, target_y,
				t,
				out_x, out_y );
			break;

		case e_curve_algorithm::bspline:
			curve::bspline_at(
				state,
				0.0, 0.0,
				target_x, target_y,
				t,
				out_x, out_y );
			break;

		case e_curve_algorithm::chaikin: {
			double points_x[ 13 ] { };
			double points_y[ 13 ] { };
			const int count = curve::helper::chaikin_refine(
				0.0, 0.0,
				state.ctrl_x, state.ctrl_y,
				target_x, target_y,
				points_x, points_y );

			if ( count < 2 ) {
				out_x = target_x;
				out_y = target_y;
				break;
			}

			const double segment_t =
				t * static_cast< double >( count - 1 );
			int segment = static_cast< int >( segment_t );
			if ( segment > count - 2 )
				segment = count - 2;
			const double local_t =
				segment_t - static_cast< double >( segment );

			out_x = points_x[ segment ] * ( 1.0 - local_t ) +
				points_x[ segment + 1 ] * local_t;
			out_y = points_y[ segment ] * ( 1.0 - local_t ) +
				points_y[ segment + 1 ] * local_t;
			break;
		}

		case e_curve_algorithm::staggered: {
			// Phase 1 (t in [0, 0.5]): yaw (X) corrects, pitch (Y) stays at zero.
			// Phase 2 (t in [0.5, 1]): yaw holds at target, pitch (Y) corrects.
			// Smoothstep easing within each phase gives natural decel/accel at
			// the phase boundary so the direction change doesn't feel abrupt.
			const double phase_t = t <= 0.5 ? t * 2.0 : ( t - 0.5 ) * 2.0;
			const double smooth = phase_t * phase_t * ( 3.0 - 2.0 * phase_t );
			if ( t <= 0.5 ) {
				out_x = target_x * smooth;
				out_y = 0.0;
			}
			else {
				out_x = target_x;
				out_y = target_y * smooth;
			}
			break;
		}

		default:
			out_x = target_x * t;
			out_y = target_y * t;
			break;
		}

		return std::isfinite( out_x ) && std::isfinite( out_y );
	}
	void c_aimbot::draw_curve_path(
		const bypass::overlay::point_t& start,
		const bypass::overlay::point_t& end,
		const curve_visual_snapshot_t& snapshot,
		const bool show_progress ) {
		const double target_x =
			static_cast< double >( end.x ) - static_cast< double >( start.x );
		const double target_y =
			static_cast< double >( end.y ) - static_cast< double >( start.y );

		if ( !std::isfinite( target_x ) || !std::isfinite( target_y ) )
			return;

		// State is already in screen-offset space; no coordinate remap needed.
		const auto& mapped_state = snapshot.state;

		constexpr int segment_count = 24;
		const auto col_accent_c = menu::current_accent( );
		const auto col_pastel_c = menu::pastel_accent( col_accent_c );
		const bypass::overlay::color_t completed_color { col_accent_c.r, col_accent_c.g, col_accent_c.b, 255 };
		const bypass::overlay::color_t remaining_color { col_pastel_c.r, col_pastel_c.g, col_pastel_c.b, 105 };
		const bypass::overlay::color_t path_color { col_accent_c.r, col_accent_c.g, col_accent_c.b, 255 };

		double progress = snapshot.progress;
		if ( progress < 0.0 ) progress = 0.0;
		if ( progress > 1.0 ) progress = 1.0;

		for ( int i = 0; i < segment_count; ++i ) {
			const double t0 =
				static_cast< double >( i ) /
				static_cast< double >( segment_count );
			const double t1 =
				static_cast< double >( i + 1 ) /
				static_cast< double >( segment_count );

			double x0 = 0.0, y0 = 0.0;
			double x1 = 0.0, y1 = 0.0;
			if ( !evaluate_curve_point(
				snapshot.algorithm,
				mapped_state,
				target_x,
				target_y,
				t0,
				x0,
				y0 ) ||
				!evaluate_curve_point(
					snapshot.algorithm,
					mapped_state,
					target_x,
					target_y,
					t1,
					x1,
					y1 ) )
				return;

			const auto color = show_progress
				? ( t1 <= progress ? completed_color : remaining_color )
				: path_color;

			bypass::overlay::line(
				bypass::overlay::point_t {
					static_cast< float >( static_cast< double >( start.x ) + x0 ),
					static_cast< float >( static_cast< double >( start.y ) + y0 )
				},
				bypass::overlay::point_t {
					static_cast< float >( static_cast< double >( start.x ) + x1 ),
					static_cast< float >( static_cast< double >( start.y ) + y1 )
				},
				color,
				show_progress ? 1.35f : 1.5f );
		}

		if ( show_progress ) {
			double marker_x = 0.0, marker_y = 0.0;
			if ( evaluate_curve_point(
				snapshot.algorithm,
				mapped_state,
				target_x,
				target_y,
				progress,
				marker_x,
				marker_y ) ) {
				bypass::overlay::circle(
					bypass::overlay::point_t {
						static_cast< float >( static_cast< double >( start.x ) + marker_x ),
						static_cast< float >( static_cast< double >( start.y ) + marker_y )
					},
					3.0f,
					completed_color,
					1.5f );
			}
		}
	}
	static void extract_camera_angles(
		const engine::f_vector& fwd,
		double& out_pitch,
		double& out_yaw )
	{
		const double fx = static_cast< double >( fwd.x );
		const double fy = static_cast< double >( fwd.y );
		const double fz = static_cast< double >( fwd.z );
		const double horiz = std::hypot( fx, fy );

		// Raw, no negation. set_control_rotation decides what sign means what.
		out_pitch = std::atan2( fz, horiz ) * ( 180.0 / std::numbers::pi );
		out_yaw = std::atan2( fy, fx ) * ( 180.0 / std::numbers::pi );
	}

	std::optional<engine::f_rotator>
		c_aimbot::camera_space_rotation_error( const engine::f_vector& aim_location ) {
		const auto& fwd = cache::camera_axis_forward;
		const auto delta = aim_location - cache::camera_location;

		const double dx = static_cast< double >( delta.x );
		const double dy = static_cast< double >( delta.y );
		const double dz = static_cast< double >( delta.z );

		// Must be in front
		const double dot = dx * static_cast< double >( fwd.x )
			+ dy * static_cast< double >( fwd.y )
			+ dz * static_cast< double >( fwd.z );
		if ( dot <= 0.25 )
			return std::nullopt;

		const double dist_xy = std::sqrt( dx * dx + dy * dy );
		if ( dist_xy < 1e-9 )
			return std::nullopt;

		constexpr double rad_to_deg = 180.0 / std::numbers::pi;

		// Absolute world-space look-at — exactly what your original did
		const double target_pitch = std::atan2( dz, dist_xy ) * rad_to_deg;
		const double target_yaw = std::atan2( dy, dx ) * rad_to_deg;

		if ( !std::isfinite( target_pitch ) || !std::isfinite( target_yaw ) )
			return std::nullopt;

		// Return as absolute, not as error
		return engine::f_rotator( target_pitch, target_yaw, 0.0 );
	}
	double c_aimbot::rotation_delta_time( ) {
		const auto now = std::chrono::steady_clock::now( );
		double delta_time = 1.0 / 60.0;

		if ( m_rotation_timing_initialized ) {
			delta_time = std::chrono::duration< double >(
				now - m_last_rotation_update ).count( );
		}
		else {
			m_rotation_timing_initialized = true;
		}

		m_last_rotation_update = now;

		if ( !std::isfinite( delta_time ) || delta_time <= 0.0 )
			return 1.0 / 60.0;

		return delta_time;
	}
	engine::f_rotator c_aimbot::smooth_rotation_error(
		const engine::f_vector& aim_location,
		const engine::f_rotator& angular_error,
		const bool target_visible ) {
		m_curve_visual_valid = false;
		m_curve_visual_algorithm = e_curve_algorithm::none;

		// Project to screen once — shared by deadzone, acceleration, and the curve path.
		const double cx = static_cast< double >( gui::screen_width_center );
		const double cy = static_cast< double >( gui::screen_height_center );
		const auto target_screen = engine::world_to_screen( aim_location );
		if ( !target_screen ||
			!std::isfinite( target_screen->x ) ||
			!std::isfinite( target_screen->y ) )
			return { 0.0, 0.0, 0.0 };

		const double offset_tx = target_screen->x - cx;
		const double offset_ty = target_screen->y - cy;
		const double screen_dist = std::hypot( offset_tx, offset_ty );

		// Deadzone: stop adjusting when the crosshair is already within N pixels.
		if ( screen_dist <= static_cast< double >( config::aimbot::deadzone ) )
			return { 0.0, 0.0, 0.0 };

		// Acceleration: divide the effective smoothing by a distance-based factor so
		// large corrections arrive faster.  acceleration=0 disables this entirely.
		// Above the threshold the speed boost is at its maximum.
		double smoothing = ( std::max ) (
			static_cast< double >( config::aimbot::smoothing ), 1.0 );
		const double accel = static_cast< double >( config::aimbot::acceleration );
		if ( accel > 0.0 ) {
			const double threshold = ( std::max ) (
				static_cast< double >( config::aimbot::acceleration_threshold ), 1.0 );
			const double ratio = ( std::min ) ( screen_dist / threshold, 1.0 );
			smoothing /= ( 1.0 + accel * ratio );
		}

		const auto algorithm = active_curve_algorithm( );
		const bool curve_active =
			config::aimbot::curve &&
			algorithm != e_curve_algorithm::none &&
			( config::aimbot::curve_behind_walls || target_visible );

		if ( !curve_active ) {
			return engine::f_rotator(
				angular_error.pitch / smoothing,
				angular_error.yaw / smoothing, 0.0 );
		}

		// Curve path — offset_tx/ty already computed above.
		const double noise = static_cast< double >( config::aimbot::curve_noise );

		if ( !m_curve_state.initialized )
			curve::helper::init_control( m_curve_state, 0.0, 0.0, offset_tx, offset_ty, noise );
		else
			curve::helper::rescale_ctrl( m_curve_state, 0.0, 0.0, offset_tx, offset_ty );

		const double dt = rotation_delta_time( );
		m_curve_state.elapsed =
			( std::min ) ( m_curve_state.elapsed + dt, m_curve_state.duration );
		const double t = m_curve_state.duration > 0.0
			? ( std::min ) ( m_curve_state.elapsed / m_curve_state.duration, 1.0 )
			: 1.0;

		// Evaluate the chosen curve algorithm at the current progress t to
		// obtain the desired screen-space offset (dx, dy) from screen centre.
		double dx = 0.0, dy = 0.0;
		if ( !evaluate_curve_point(
			algorithm, m_curve_state, offset_tx, offset_ty, t, dx, dy ) )
			return { 0.0, 0.0, 0.0 };

		// Deproject the desired screen point back to a camera-space ray and
		// convert to angular error, which is what rotation_input() expects.
		//
		// From world_to_screen:
		//   screen.x = cx + local_right * proj_scale / local_fwd
		//   screen.y = cy - local_up   * proj_scale / local_fwd
		// Inverting (local_fwd = 1):
		//   local_right =  dx / proj_scale
		//   local_up    = -dy / proj_scale   (screen Y is down, world Y is up)
		const double proj_scale =
			std::fabs( static_cast< double >( cache::camera_projection_scale ) );
		if ( proj_scale < 1e-9 ) return { 0.0, 0.0, 0.0 };

		const double local_right = dx / proj_scale;
		const double local_up = -dy / proj_scale;

		constexpr double rad_to_deg = 180.0 / std::numbers::pi;
		const double yaw_error = std::atan2( local_right, 1.0 ) * rad_to_deg;
		const double pitch_error = std::atan2( local_up, 1.0 ) * rad_to_deg;

		if ( !std::isfinite( yaw_error ) || !std::isfinite( pitch_error ) )
			return { 0.0, 0.0, 0.0 };

		if ( std::isfinite( m_curve_state.ctrl_x ) &&
			std::isfinite( m_curve_state.ctrl_y ) ) {
			m_curve_visual_target_x = offset_tx;
			m_curve_visual_target_y = offset_ty;
			m_curve_visual_progress = t;
			m_curve_visual_algorithm = algorithm;
			m_curve_visual_valid = true;
		}

		return engine::f_rotator( pitch_error / smoothing, yaw_error / smoothing, 0.0 );
	}
	bool c_aimbot::apply_rotation_input(
		const engine::f_vector& aim_location,
		const bool target_visible )
	{
		auto player_controller =
			g_cache->m_player_controller.load( std::memory_order_acquire );
		if ( !player_controller ||
			!utility::is_valid_pointer( player_controller ) )
			return false;

		const auto delta = aim_location - cache::camera_location;
		const double dx = static_cast< double >( delta.x );
		const double dy = static_cast< double >( delta.y );
		const double dz = static_cast< double >( delta.z );

		// Forward-check: target must be in front of camera
		const auto& fwd = cache::camera_axis_forward;
		const double dot = dx * static_cast< double >( fwd.x )
			+ dy * static_cast< double >( fwd.y )
			+ dz * static_cast< double >( fwd.z );
		if ( dot <= 0.25 )
			return false;

		const double dist_xy = std::sqrt( dx * dx + dy * dy );
		if ( dist_xy < 1e-9 )
			return false;

		constexpr double rad_to_deg = 180.0 / std::numbers::pi;

		// Absolute world-space look-at — identical to your original find_look_at_rotation
		const double target_pitch = std::atan2( dz, dist_xy ) * rad_to_deg;
		const double target_yaw = std::atan2( dy, dx ) * rad_to_deg;

		const auto ctrl_rot = memory_aim::read_control_rotation(
			reinterpret_cast< std::uint64_t >( player_controller ) );
		if ( !memory_aim::is_valid_control_rotation( ctrl_rot ) )
			return false;
		const double cur_pitch = ctrl_rot.pitch;
		const double cur_yaw   = ctrl_rot.yaw;

		// True angular delta — small value, correct sign
		const engine::f_rotator angular_error(
			memory_aim::normalize_axis( target_pitch - cur_pitch ),
			memory_aim::normalize_axis( target_yaw - cur_yaw ),
			0.0 );

		// Smooth operates on the delta correctly
		const auto rotation_delta =
			smooth_rotation_error( aim_location, angular_error, target_visible );
		if ( !std::isfinite( rotation_delta.pitch ) ||
			!std::isfinite( rotation_delta.yaw ) )
			return false;

		// Clamp so we never overshoot
		const double safe_pitch = std::clamp(
			rotation_delta.pitch,
			-std::abs( angular_error.pitch ),
			std::abs( angular_error.pitch ) );
		const double safe_yaw = std::clamp(
			rotation_delta.yaw,
			-std::abs( angular_error.yaw ),
			std::abs( angular_error.yaw ) );

		// Write absolute: step current toward target
		const double next_pitch = memory_aim::normalize_axis( cur_pitch + safe_pitch );
		const double next_yaw = memory_aim::normalize_axis( cur_yaw + safe_yaw );

		return memory_aim::set_control_rotation(
			reinterpret_cast< std::uint64_t >( player_controller ),
			next_pitch, next_yaw );
	}
}
