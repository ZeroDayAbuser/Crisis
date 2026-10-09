#pragma once

namespace gui::widgets {

    enum class anchor_t : std::uint8_t {
        top_left,
        top_right,
        bottom_left,
        bottom_right
    };

    struct slot_t {
        float x = 0.0f;
        float y = 0.0f;
        float w = 0.0f;
        float h = 0.0f;
    };

    void begin_frame( );

    slot_t place( anchor_t anchor, float width, float height, float pad = 8.0f );

    void render( );
}

namespace gui::widgets::radar {
    void render( );
}

namespace gui::widgets::top_bar {
    void render( );
}
