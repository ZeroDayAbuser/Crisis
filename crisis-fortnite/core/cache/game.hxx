#pragma once

namespace engine {
	[[nodiscard]] f_matrix to_rotation_matrix( const f_rotator& rotation );
	[[nodiscard]] f_rotator find_look_at_rotation(
		const f_vector& start, const f_vector& target );
	[[nodiscard]] std::optional< f_vector2d > world_to_screen_axes(
		f_vector world_location, const f_vector& camera_location,
		const f_vector& axis_right, const f_vector& axis_up,
		const f_vector& axis_forward, float fov );
	[[nodiscard]] std::optional< f_vector2d > world_to_screen(
		f_vector world_location, const f_vector& camera_location,
		const f_rotator& camera_rotation, float fov );
	[[nodiscard]] std::optional< f_vector2d > world_to_screen(
		f_vector world_location );
}
