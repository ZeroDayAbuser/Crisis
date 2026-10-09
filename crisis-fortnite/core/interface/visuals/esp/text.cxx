#include <deps/includes.h>

namespace gui::esp {
    void text_medium(
        layout_t& layout,
        const wchar_t* value,
        color_t color,
        anchor_t anchor,
        float alpha_scale ) {
        helper::draw_text(
            layout,
            value,
            color,
            text_medium_size,
            anchor,
            alpha_scale );
    }

    void text_small(
        layout_t& layout,
        const wchar_t* value,
        color_t color,
        anchor_t anchor,
        float alpha_scale ) {
        helper::draw_text(
            layout,
            value,
            color,
            text_small_size,
            anchor,
            alpha_scale );
    }
}
