#pragma once

namespace gui {
	using bypass::overlay::point_t;
	using bypass::overlay::rect_t;
	using bypass::overlay::color_t;
	using bypass::overlay::text_weight_t;
	using bypass::overlay::text_align_t;

	namespace fonts {
		inline constexpr auto regular = L"TikTokSans-Regular";
		inline constexpr auto bold = L"TikTokSans-Bold";
		inline constexpr auto monospace = L"SourceCodePro-Black";
		inline constexpr auto icons = L"Font Awesome";
	}

	struct text_style_t {
		float size = 13.0f;
		text_weight_t weight = text_weight_t::medium;
		const wchar_t* font = fonts::regular;
		text_align_t alignment = text_align_t::left;
		bool wrap = false;
	};

	namespace draw {
		inline bool text(
			const wchar_t* value,
			rect_t area,
			color_t color,
			text_style_t style = { }
		) {
			return bypass::overlay::text_ex(
				value,
				area,
				color,
				style.size,
				style.weight,
				style.font,
				style.alignment,
				style.wrap
			);
		}

		inline bool icon(
			const wchar_t* value,
			rect_t area,
			color_t color,
			float size = 13.0f
		) {
			return text( value, area, color, {
				.size = size,
				.weight = text_weight_t::regular,
				.font = fonts::icons,
				.alignment = text_align_t::center
				} );
		}
	}

	inline std::int32_t screen_width = 0;
	inline std::int32_t screen_height = 0;

	inline std::int32_t screen_width_center = 0;
	inline std::int32_t screen_height_center = 0;
}
