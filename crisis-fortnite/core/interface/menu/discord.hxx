#pragma once

namespace discord {
	struct profile_t {
		std::wstring id;
		std::wstring username;
		std::wstring display_name;
		std::wstring avatar_url;
	};

	[[nodiscard]] std::optional< profile_t > current_profile( );
}
