#pragma once

namespace gui::esp::tracers {

    void sample( std::uint64_t player_key, const engine::f_vector& world_pos );

    void render_player_trail( std::uint64_t player_key, color_t color );

    void tick( );
}

namespace tracer {

    void render( const std::vector< cache::c_bullet_tracer >& tracers );
}
