#pragma once

#include "discord.hxx"

namespace menu {
	inline constexpr std::uint32_t logo_id = 5;
	inline constexpr std::uint32_t profile_image_id = 6;
	inline constexpr std::uint32_t icon_id = 7;

	inline std::wstring g_active_profile      = L"default";
	inline std::wstring g_discord_name        = L"user";
	inline bool         g_profile_image_ready = false;

	namespace notifications {
		void push( std::wstring header, std::wstring description,
			std::wstring icon = L"\uf0f3", bypass::overlay::color_t color = {},
			bool pinned = false, bool centered = false );
		void finish_pinned( );
		void render_pass( );
	}

	void set_profile( std::wstring display_name, std::wstring username,
		bool image_registered, std::wstring discord_id, std::wstring role,
		std::int64_t expires_unix );
	void set_logo_registered( bool image_registered );
	[[nodiscard]] int menu_key( ) noexcept;
	[[nodiscard]] bypass::overlay::color_t current_accent( );
	[[nodiscard]] bypass::overlay::color_t inactive_accent( );
	[[nodiscard]] bypass::overlay::color_t pastel_accent(
		const bypass::overlay::color_t& color );
	void render( bool menu_open = true );
}
