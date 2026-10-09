#pragma once

namespace shared {
	static constexpr std::uint64_t max_mapping_ranges = 64;

	static constexpr std::uint32_t capacity            = transport::k_capacity;
	static constexpr std::uint32_t capacity_mask       = transport::k_capacity_mask;
	static constexpr std::size_t   packet_payload_size = transport::k_payload;

	enum class module_command : std::uint32_t {
		none,
		map_memory,
		read_memory,
		write_memory,
		query_mapping
	};

	struct mapping_range_t {
		std::uint64_t base_pa { };
		std::uint64_t size { };
		std::uint64_t va_base { };
	};

	struct mapping_info_t {
		std::uint64_t physical_mask { };
		std::uint64_t max_ranges { };
		std::uint64_t range_count { };
		mapping_range_t ranges[ max_mapping_ranges ] { };
	};

	using transport::packet_empty;
	using transport::packet_pending;
	using transport::packet_complete;
	using transport::packet_abandoned;

	using packet_t = transport::packet_t;
	using shared_state_t = transport::section_t;
}
