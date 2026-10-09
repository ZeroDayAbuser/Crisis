#pragma once

#include <cstdint>

namespace render {
	struct color_t {
		float r = 0.0f;
		float g = 0.0f;
		float b = 0.0f;
		float a = 1.0f;

		static constexpr color_t from_rgba( std::uint8_t red, std::uint8_t green, std::uint8_t blue, std::uint8_t alpha = 255 ) {
			constexpr auto scale = 1.0f / 255.0f;
			return { red * scale, green * scale, blue * scale, alpha * scale };
		}
	};

	struct point_t {
		float x = 0.0f;
		float y = 0.0f;
	};

	struct size_t {
		float width = 0.0f;
		float height = 0.0f;
	};

	struct rect_t {
		float x = 0.0f;
		float y = 0.0f;
		float width = 0.0f;
		float height = 0.0f;
	};

	enum class text_align_t {
		left,
		center,
		right
	};
}
