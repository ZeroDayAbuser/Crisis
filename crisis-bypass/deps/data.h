#pragma once
#include <cstdint>
#include <type_traits>

namespace bypass {
	inline constexpr std::uint32_t magic_code = 0x54494E43;
	inline constexpr std::uint16_t version = 3;

	struct alignas( 8 ) startup_t {
		std::uint32_t m_magic;
		std::uint16_t m_version;
		std::uint16_t m_size;
		std::uint64_t m_system_dtb;
		std::uint32_t m_pdb_blob_size;
		std::uint32_t m_pad;
	};

	static_assert( std::is_standard_layout_v< startup_t > );
	static_assert( std::is_trivially_copyable_v< startup_t > );
	static_assert( sizeof( startup_t ) == 24 );
}
