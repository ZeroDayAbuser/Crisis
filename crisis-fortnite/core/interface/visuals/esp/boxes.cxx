#include <deps/includes.h>

namespace gui::esp {
        void box_filled( const rect_t& area, color_t color, std::uint8_t alpha,
                         float corner_radius ) {
            color.a = alpha;
            if ( corner_radius > 0.5f ) {
                const float max_r = ( area.width < area.height ? area.width : area.height ) * 0.5f;
                const float r = corner_radius > max_r ? max_r : corner_radius;
                bypass::overlay::filled_rounded_rectangle( area, r, color );
            }
            else {
                bypass::overlay::filled_rectangle_async( area, color );
            }
        }
        void box_outlined( const rect_t& area, color_t inner, float thickness,
                                  float alpha_scale, bool outline,
                                  float corner_radius ) {
            inner.a = static_cast< std::uint8_t >( inner.a * alpha_scale );
            color_t outline_color{ 0, 0, 0, inner.a };

            if ( corner_radius > 0.5f ) {
                const float max_r = ( area.width < area.height ? area.width : area.height ) * 0.5f;
                const float r = corner_radius > max_r ? max_r : corner_radius;
                if ( outline )
                    bypass::overlay::rounded_rectangle( area, r, outline_color, thickness + 1.5f );
                bypass::overlay::rounded_rectangle( area, r, inner, thickness );
                return;
            }

            const point_t tl{ area.x,               area.y };
            const point_t tr{ area.x + area.width,  area.y };
            const point_t bl{ area.x,               area.y + area.height };
            const point_t br{ area.x + area.width,  area.y + area.height };

            auto stroke = [ & ]( const color_t& col, float t ) {
                bypass::overlay::line_async( tl, tr, col, t );
                bypass::overlay::line_async( bl, br, col, t );
                bypass::overlay::line_async( tl, bl, col, t );
                bypass::overlay::line_async( tr, br, col, t );
                };

            if ( outline )
                stroke( outline_color, 1.f );
            stroke( inner, thickness );
        }
        void box_corners( const rect_t& area, color_t inner, float length,
                                 float thickness, bool outline ) {
            const color_t outline_color{ 0, 0, 0, inner.a };

            const float len = ( area.width < area.height ? area.width : area.height ) * length;

            const float l = area.x;
            const float t = area.y;
            const float r = area.x + area.width;
            const float b = area.y + area.height;

            auto stroke = [ & ]( const color_t& col, float th ) {
                // top-left
                bypass::overlay::line_async( { l, t }, { l + len, t }, col, th );
                bypass::overlay::line_async( { l, t }, { l, t + len }, col, th );
                // top-right
                bypass::overlay::line_async( { r, t }, { r - len, t }, col, th );
                bypass::overlay::line_async( { r, t }, { r, t + len }, col, th );
                // bottom-left
                bypass::overlay::line_async( { l, b }, { l + len, b }, col, th );
                bypass::overlay::line_async( { l, b }, { l, b - len }, col, th );
                // bottom-right
                bypass::overlay::line_async( { r, b }, { r - len, b }, col, th );
                bypass::overlay::line_async( { r, b }, { r, b - len }, col, th );
                };

            if ( outline )
                stroke( outline_color, 1.f );
            stroke( inner, thickness );
        }
}
