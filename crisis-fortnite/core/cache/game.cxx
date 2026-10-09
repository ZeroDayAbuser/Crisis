#include <deps/includes.h>

namespace engine {
	f_matrix to_rotation_matrix( const f_rotator& rotation ) {
		f_matrix matrix = {};

		auto rad_pitch = ( rotation.pitch * std::numbers::pi / 180.f );
		auto rad_yaw = ( rotation.yaw * std::numbers::pi / 180.f );
		auto rad_roll = ( rotation.roll * std::numbers::pi / 180.f );

		auto sin_pitch = sin( rad_pitch );
		auto cos_pitch = cos( rad_pitch );

		auto sin_yaw = sin( rad_yaw );
		auto cos_yaw = cos( rad_yaw );

		auto sin_roll = sin( rad_roll );
		auto cos_roll = cos( rad_roll );

		matrix.x_plane.x = cos_pitch * cos_yaw;
		matrix.x_plane.y = cos_pitch * sin_yaw;
		matrix.x_plane.z = sin_pitch;
		matrix.x_plane.w = 0.f;

		matrix.y_plane.x = sin_roll * sin_pitch * cos_yaw - cos_roll * sin_yaw;
		matrix.y_plane.y = sin_roll * sin_pitch * sin_yaw + cos_roll * cos_yaw;
		matrix.y_plane.z = -sin_roll * cos_pitch;
		matrix.y_plane.w = 0.f;

		matrix.z_plane.x = -( cos_roll * sin_pitch * cos_yaw + sin_roll * sin_yaw );
		matrix.z_plane.y = cos_yaw * sin_roll - cos_roll * sin_pitch * sin_yaw;
		matrix.z_plane.z = cos_roll * cos_pitch;
		matrix.z_plane.w = 0.f;

		matrix.w_plane.w = 1.f;
		return matrix;
	}

	f_rotator find_look_at_rotation( const f_vector& start, const f_vector& target ) {
		const f_vector delta = target - start;

		const double dist_xy = std::sqrt( delta.x * delta.x + delta.y * delta.y );
		const double pitch = std::atan2( delta.z, dist_xy ) * ( 180.0 / std::numbers::pi );
		const double yaw = std::atan2( delta.y, delta.x ) * ( 180.0 / std::numbers::pi );

		return f_rotator( pitch, yaw, 0.0 );
	}

	std::optional < f_vector2d > world_to_screen_axes(
		f_vector world_location, const f_vector& camera_loc,
		const f_vector& axis_right, const f_vector& axis_up, const f_vector& axis_forward,
		float fov ) {

		auto delta = ( world_location - camera_loc );

		auto transform = f_vector(
			delta.vector_scalar( axis_right ),
			delta.vector_scalar( axis_up ),
			delta.vector_scalar( axis_forward )
		);
		if ( transform.z < 50.f )
			return std::nullopt;

		auto fov_radians = tan( fov * std::numbers::pi / 360.f );

		return std::optional( f_vector2d(
			gui::screen_width_center + transform.x * ( gui::screen_width_center / fov_radians ) / transform.z,
			gui::screen_height_center - transform.y * ( gui::screen_width_center / fov_radians ) / transform.z
		) );
	}

	std::optional < f_vector2d > world_to_screen( f_vector world_location, const f_vector& camera_loc, const f_rotator& camera_rot, float fov ) {
		auto matrix = to_rotation_matrix( camera_rot );

		auto axis_x = f_vector( matrix.x_plane.x, matrix.x_plane.y, matrix.x_plane.z );
		auto axis_y = f_vector( matrix.y_plane.x, matrix.y_plane.y, matrix.y_plane.z );
		auto axis_z = f_vector( matrix.z_plane.x, matrix.z_plane.y, matrix.z_plane.z );

		return world_to_screen_axes( world_location, camera_loc, axis_y, axis_z, axis_x, fov );
	}

	std::optional < f_vector2d > world_to_screen( f_vector world_location ) {
		//g_cache->update_camera( ); // DON'T DO THIS NIGGER VOODOO

		const auto delta = world_location - cache::camera_location;
		const auto transform = f_vector(
			delta.vector_scalar( cache::camera_axis_right ),
			delta.vector_scalar( cache::camera_axis_up ),
			delta.vector_scalar( cache::camera_axis_forward )
		);
		if ( transform.z < 50.f )
			return std::nullopt;

		const auto inv_depth = cache::camera_projection_scale / transform.z;
		return f_vector2d(
			gui::screen_width_center + transform.x * inv_depth,
			gui::screen_height_center - transform.y * inv_depth
		);
	}
}
