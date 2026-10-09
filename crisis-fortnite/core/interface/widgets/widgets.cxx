#include <deps/includes.h>

namespace gui::widgets {

    struct cursor_t {
        float x = 0.0f;
        float y = 0.0f;
        float row_extent = 0.0f;
    };

    static cursor_t s_cursor[ 4 ] {};

    void begin_frame( ) {
        for ( int i = 0; i < 4; ++i )
            s_cursor[ i ] = {};
    }

    slot_t place( anchor_t anchor, float width, float height, float pad ) {
        const float sw = static_cast< float >( gui::screen_width );
        const float sh = static_cast< float >( gui::screen_height );
        const float dpi = gui::esp::dpi_scale( );
        const float w = width  * dpi;
        const float h = height * dpi;
        const float p = pad    * dpi;
        const float margin = 18.0f * dpi;

        auto& cur = s_cursor[ static_cast< int >( anchor ) ];

        float base_x = 0.0f, base_y = 0.0f;
        switch ( anchor ) {
        case anchor_t::top_left:
            base_x = margin + cur.x;
            base_y = margin + cur.y;
            break;
        case anchor_t::top_right:
            base_x = sw - margin - w - cur.x;
            base_y = margin + cur.y;
            break;
        case anchor_t::bottom_left:
            base_x = margin + cur.x;
            base_y = sh - margin - h - cur.y;
            break;
        case anchor_t::bottom_right:
            base_x = sw - margin - w - cur.x;
            base_y = sh - margin - h - cur.y;
            break;
        }

        cur.x += w + p;
        if ( h > cur.row_extent ) cur.row_extent = h;

        return { base_x, base_y, w, h };
    }

    void render( ) {
        begin_frame( );
        widgets::top_bar::render( );
        widgets::radar::render( );
    }
}
