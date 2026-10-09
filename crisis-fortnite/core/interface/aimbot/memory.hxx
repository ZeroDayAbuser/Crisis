#pragma once

namespace memory_aim {
	static constexpr double k_double_small_number = 1.0e-8;
	static constexpr double k_invalid_control_rotation_magnitude = 8388608.0;

	double safe_fmod( double x, double y );
	bool is_nan( double value );
	bool contains_nan( const engine::f_rotator& rotation );
	bool is_valid_control_rotation( const engine::f_rotator& rotation );
	double normalize_axis( double angle );

	bool set_control_rotation_obfuscated(
		std::uint64_t controller,
		const engine::f_rotator new_rotation );
	bool apply_rotation_input(
		std::uint64_t controller,
		double pitch_delta,
		double yaw_delta );

	bool set_control_rotation( std::uint64_t controller, double pitch, double yaw );
	engine::f_rotator read_control_rotation( std::uint64_t controller );
}