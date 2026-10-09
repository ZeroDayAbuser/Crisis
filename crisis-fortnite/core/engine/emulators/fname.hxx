#pragma once

namespace engine {
	namespace emulator {
		struct emu_fstring {
			std::uint64_t data {};
			std::int32_t count {};
			std::int32_t capacity {};
		};


		std::string wide_to_utf8_lossy(
			const wchar_t* text,
			std::int32_t count );
		std::string fname_to_string(
			const f_name& name,
			bool display_errors = true );
	}
}