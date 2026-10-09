#pragma once

namespace aimbot {
	enum class curve_type : std::uint8_t {
		normal = 0,
		bezier = 1,
		bspline = 2,
		chaikin = 3
	};
}