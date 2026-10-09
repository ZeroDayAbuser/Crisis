#pragma once

namespace exception {
	inline constexpr std::uint32_t k_magic = 0x51435243u;
	inline constexpr std::uint64_t k_transfer_limit = 0x10000ull;
	inline constexpr std::uint64_t k_user_max = 0x00007FFFFFFFFFFFull;

	enum class e_data_type : std::uint32_t {
		none = 0,
		get_eprocess,
		get_base_address,
		get_directory_table_base,
		translate_linear,
		read_memory,
		write_memory,
		is_running
	};

	inline constexpr std::uint64_t k_alive = 0x445256414C495645ull;

	enum class e_data_status : std::int32_t {
		ok = 0,
		bad_magic = -1,
		bad_type = -2,
		not_ready = -3,
		no_process = -4,
		bad_address = -5,
		too_large = -6,
		transfer_failed = -7,
		not_running = -8
	};

	struct data_request_t {
		std::uint32_t m_magic {};
		std::uint32_t m_type {};
		std::int32_t  m_status {};
		std::uint32_t m_flags {};
		std::uint64_t m_process_id {};
		std::uint64_t m_address {};
		std::uint64_t m_buffer {};
		std::uint64_t m_size {};
		std::uint64_t m_result {};
	};
}