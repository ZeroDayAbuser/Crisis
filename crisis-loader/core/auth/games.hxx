#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace loader_games {
	struct module_t {
		std::string_view id;
		std::string_view display_name;
		std::string_view filename;
	};

	std::span< const module_t > catalog( ) noexcept;
	const module_t* find( std::string_view id ) noexcept;
	std::vector< const module_t* > entitled_modules( std::span< const std::string > entitlements );
}
