#pragma once

namespace gui::esp {
    void draw_threat_arrow(
        std::uint64_t player_key,
        float edge_x, float edge_y,
        float center_x, float center_y,
        float distance_m,
        float dpi_scale );

    void tick_threat_arrow_anim( );

    float dpi_scale( );
}
