#pragma once

namespace exception {
	namespace communication {
		inline constexpr std::uint32_t k_magic = 0x51435243u;
		inline constexpr std::uint64_t k_transfer_limit = 0x10000ull;
		inline constexpr std::uint64_t k_user_max = 0x00007FFFFFFFFFFFull;

		// get_ / find_ prefixed: each names the operation, not a bare noun, so a request
		// reads as an action and cannot be mistaken for one of its own fields.
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

		// Answer to is_running. A caller compares this against the reply so it knows the
		// channel is served by this driver and not by anything else that might sit on the
		// same instruction.
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

		inline volatile long m_logged_magic {};
		inline volatile long m_logged_fault {};
		inline volatile long m_logged_reject {};
		inline volatile long m_logged_served {};
		inline volatile long m_requests {};
		inline volatile long m_failures {};
	}
}
