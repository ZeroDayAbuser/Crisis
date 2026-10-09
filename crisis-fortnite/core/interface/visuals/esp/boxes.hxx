#pragma once

namespace gui {
    namespace esp {


        void box_filled(
            const rect_t& area,
            color_t color,
            std::uint8_t alpha = 70,
            float corner_radius = 0.0f );
        void box_outlined(
            const rect_t& area,
            color_t inner,
            float thickness = 1.5f,
            float alpha_scale = 1.f,
            bool outline = true,
            float corner_radius = 0.0f );
        void box_corners(
            const rect_t& area,
            color_t inner,
            float length = 0.25f,
            float thickness = 1.5f,
            bool outline = true );
    }
}
