#include <deps/includes.h>

namespace memory_aim {
	double safe_fmod( double x, double y ) {
		const double abs_y = y < 0.0 ? -y : y;
		if ( abs_y <= k_double_small_number )
			return 0.0;
		return std::fmod( x, y );
	}

	bool is_nan( double value ) {
		const auto bits = std::bit_cast< std::uint64_t >( value );
		const std::uint64_t exponent = ( bits >> 52 ) & 0x7FF;
		const std::uint64_t mantissa = bits & 0x000FFFFFFFFFFFFF;
		return exponent == 0x7FF && mantissa != 0;
	}

	bool contains_nan( const engine::f_rotator& rot ) {
		return is_nan( rot.pitch ) || is_nan( rot.yaw ) || is_nan( rot.roll );
	}

	bool is_valid_control_rotation( const engine::f_rotator& check_rotation ) {
		if ( contains_nan( check_rotation ) )
			return false;

		const double abs_pitch = check_rotation.pitch < 0.0 ? -check_rotation.pitch : check_rotation.pitch;
		const double abs_yaw = check_rotation.yaw < 0.0 ? -check_rotation.yaw : check_rotation.yaw;
		const double abs_roll = check_rotation.roll < 0.0 ? -check_rotation.roll : check_rotation.roll;

		if ( abs_pitch >= k_invalid_control_rotation_magnitude || abs_yaw >= k_invalid_control_rotation_magnitude || abs_roll >= k_invalid_control_rotation_magnitude )
			return false;

		return true;
	}

	double normalize_axis( double angle ) {
		angle = safe_fmod( angle, 360.0 );
		if ( angle < 0.0 )
			angle += 360.0;
		if ( angle > 180.0 )
			angle -= 360.0;
		return angle;
	}

	constexpr std::uint64_t aim_idx_table_rva = 0x13874A48;
	constexpr std::uint64_t aim_xmm_add_rva  = 0x12FACDE0;
	constexpr std::uint64_t aim_xmm_xor_rva  = 0x12CCD260;

	constexpr std::uint32_t control_rotation_offset       = 0x04B0;
	constexpr std::uint32_t target_control_rotation_offset = 0x0860;
	constexpr std::uint32_t aim_signal_array_offset       = 0x2080;
	constexpr std::uint32_t aim_idx_offset                = 0x2090;
	constexpr std::uint32_t aim_counter_offset            = 0x2094;

	constexpr std::uint32_t aim_counter_mask   = 0xAE64C583u;
	constexpr std::uint32_t aim_counter_step   = 0x50FDu;
	constexpr std::uint8_t  aim_idx_write_mask = 0x0F;

	bool set_control_rotation_obfuscated( std::uint64_t controller, const engine::f_rotator new_rotation )
	{
		if ( !utility::is_valid_pointer( reinterpret_cast< void* >( controller ) ) )
			return false;

		if ( !is_valid_control_rotation( new_rotation ) )
			return false;

		const auto image_base = bypass::target::m_base_address;
		if ( !image_base )
			return false;

		std::uint8_t aim_idx_table[ 4 ] = { };
		for ( int i = 0; i < 4; ++i )
			aim_idx_table[ i ] = bypass::read< std::uint8_t >( image_base + aim_idx_table_rva + i );

		const auto aim_xmm_add = bypass::read< __m128i >( image_base + aim_xmm_add_rva );
		const auto aim_xmm_xor = bypass::read< __m128i >( image_base + aim_xmm_xor_rva );

		bypass::write< engine::f_rotator >( controller + control_rotation_offset,        new_rotation );
		bypass::write< engine::f_rotator >( controller + target_control_rotation_offset, new_rotation );

		const std::uint8_t  idx_enc     = bypass::read< std::uint8_t  >( controller + aim_idx_offset );
		const std::uint32_t counter_enc = bypass::read< std::uint32_t >( controller + aim_counter_offset );

		const std::uint8_t  idx     = static_cast< std::uint8_t >( ( -static_cast< int >( idx_enc ) ) & 3 );
		const std::uint32_t counter = counter_enc ^ aim_counter_mask;

		bypass::write< std::uint8_t  >( controller + aim_idx_offset,     static_cast< std::uint8_t >( idx ^ aim_idx_write_mask ) );
		bypass::write< std::uint32_t >( controller + aim_counter_offset, ( counter + aim_counter_step ) ^ aim_counter_mask );

		const std::uint8_t slot = aim_idx_table[ idx & 3 ];

		const std::uint32_t result = static_cast< std::uint8_t >( counter + 4u );

		__m128i v = _mm_cvtsi32_si128( static_cast< int >( result ) );
		v = _mm_unpacklo_epi8( v, v );
		v = _mm_shufflelo_epi16( v, 0 );
		v = _mm_add_epi8( v, aim_xmm_add );
		v = _mm_xor_si128( v, aim_xmm_xor );
		const std::uint32_t encoded = static_cast< std::uint32_t >( _mm_cvtsi128_si32( v ) );

		bypass::write< std::uint32_t >(
			controller + aim_signal_array_offset + 4u * static_cast< std::uint32_t >( slot ),
			encoded );

		return true;
	}

	bool apply_rotation_input(
		std::uint64_t controller,
		double pitch_delta,
		double yaw_delta )
	{
		if ( !utility::is_valid_pointer( reinterpret_cast< void* >( controller ) ) )
			return false;

		if ( !std::isfinite( pitch_delta ) || !std::isfinite( yaw_delta ) )
			return false;

		const engine::f_rotator input(
			normalize_axis( pitch_delta ),
			normalize_axis( yaw_delta ),
			0.0 );

		set_control_rotation_obfuscated( controller, input );

		/*bypass::write< engine::f_rotator >(
			controller + 0x04B0,
			input );*/
		return true;
	}

	engine::f_rotator read_control_rotation( std::uint64_t controller )
	{
		if ( !utility::is_valid_pointer( reinterpret_cast< void* >( controller ) ) )
			return { };
		return bypass::read< engine::f_rotator >( controller + control_rotation_offset );
	}

	bool set_control_rotation( std::uint64_t controller, double pitch, double yaw )
	{
		if ( !utility::is_valid_pointer( reinterpret_cast< void* >( controller ) ) )
			return false;

		if ( !std::isfinite( pitch ) || !std::isfinite( yaw ) )
			return false;

		return set_control_rotation_obfuscated(
			controller, engine::f_rotator( normalize_axis( pitch ), normalize_axis( yaw ), 0.0 ) );
	}
}