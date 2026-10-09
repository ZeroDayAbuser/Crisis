#include <deps/includes.h>

namespace gui::widgets::radar {

    void render( ) {
        if ( !config::widgets::radar )
            return;

        const float dpi = gui::esp::dpi_scale( );

        const auto col_accent = menu::current_accent( );
        constexpr bypass::overlay::color_t bg_col       { 22, 21, 40, 240 };
        constexpr bypass::overlay::color_t outline_col  { 62, 66, 84, 200 };
        constexpr bypass::overlay::color_t text_light   { 245, 245, 250, 250 };
        constexpr bypass::overlay::color_t text_soft    { 200, 200, 220, 200 };
        constexpr bypass::overlay::color_t divider_col  { 90, 92, 118, 140 };

        const float top_margin  = 10.0f * dpi;
        const float left_margin = 18.0f * dpi;
        const float radius      = 10.0f * dpi;
        const float outline_t   = 2.0f  * dpi;

        const float size_scale = ( std::clamp )( config::widgets::radar_size, 0.5f, 1.6f );
        const float radar_w = 220.0f * dpi * size_scale;
        const float radar_h = 240.0f * dpi * size_scale;
        const float radar_x = left_margin;
        const float radar_y = std::floor( top_margin );

        bypass::overlay::filled_rounded_rectangle(
            { radar_x - outline_t, radar_y - outline_t,
              radar_w + outline_t * 2.0f, radar_h + outline_t * 2.0f },
            radius + outline_t, outline_col );
        bypass::overlay::filled_rounded_rectangle(
            { radar_x, radar_y, radar_w, radar_h }, radius, bg_col );

        const float hdr_h    = 38.0f * dpi;
        const float hdr_pad  = 14.0f * dpi;
        const float title_pt = 14.0f * dpi;

        static const wchar_t s_radar_icon[ 2 ] = { L'\xF7C0', L'\0' };
        const float icon_sz = 16.0f * dpi;
        const float icon_x  = radar_x + hdr_pad;
        const float icon_y  = radar_y + ( hdr_h - icon_sz ) * 0.5f;
        bypass::overlay::text_ex(
            s_radar_icon,
            { icon_x, icon_y, icon_sz, icon_sz },
            bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 255 },
            title_pt,
            bypass::overlay::text_weight_t::regular,
            L"Font Awesome",
            bypass::overlay::text_align_t::center, false );

        const float title_x = icon_x + icon_sz + 8.0f * dpi;
        const float title_w = radar_w - ( title_x - radar_x ) - hdr_pad - 44.0f * dpi;
        bypass::overlay::text_ex(
            L"Radar",
            { title_x, radar_y, title_w, hdr_h },
            text_light,
            title_pt,
            bypass::overlay::text_weight_t::semi_bold,
            L"Geist-Bold",
            bypass::overlay::text_align_t::left, false );

        {
            const auto  fwd     = cache::camera_axis_forward;
            const float yaw_rad = std::atan2( static_cast< float >( fwd.y ),
                                              static_cast< float >( fwd.x ) );
            const float pi      = 3.14159265358979323846f;
            float deg = yaw_rad * ( 180.0f / pi );
            if ( deg < 0.0f ) deg += 360.0f;
            const int   idx = static_cast< int >( std::floor( ( deg + 22.5f ) / 45.0f ) ) & 7;
            static const wchar_t* const dirs[ 8 ] = {
                L"E", L"NE", L"N", L"NW", L"W", L"SW", L"S", L"SE"
            };
            const float dir_pt = 12.5f * dpi;
            const float dir_w  = 40.0f * dpi;
            const float dir_x  = radar_x + radar_w - hdr_pad - dir_w;
            bypass::overlay::text_ex(
                dirs[ idx ],
                { dir_x, radar_y, dir_w, hdr_h },
                bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 235 },
                dir_pt,
                bypass::overlay::text_weight_t::bold,
                L"Geist-Bold",
                bypass::overlay::text_align_t::right, false );
        }

        const float div_y = radar_y + hdr_h;
        const float div_x = radar_x + hdr_pad;
        const float div_w = radar_w - hdr_pad * 2.0f;
        if ( div_w > 1.0f ) {
            constexpr int   div_segments  = 64;
            constexpr float div_edge_fade = 0.38f;
            constexpr float div_max_alpha = 200.0f;
            const float div_thickness = 1.0f * dpi;
            for ( int i = 0; i < div_segments; ++i ) {
                const float x0 = div_x + div_w * ( static_cast< float >( i )     / div_segments );
                const float x1 = div_x + div_w * ( static_cast< float >( i + 1 ) / div_segments );
                const float pos = ( static_cast< float >( i ) + 0.5f ) / div_segments;
                float fade = ( std::min )( pos, 1.0f - pos ) / div_edge_fade;
                fade = ( std::clamp )( fade, 0.0f, 1.0f );
                fade = fade * fade * ( 3.0f - 2.0f * fade );
                const auto a = static_cast< std::uint8_t >( div_max_alpha * fade );
                if ( a > 0 )
                    bypass::overlay::filled_rectangle(
                        { x0, div_y, x1 - x0, div_thickness },
                        bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, a } );
            }
        }

        const float body_y = div_y + 1.0f;
        const float body_h = radar_h - hdr_h - 1.0f;
        const float body_pad = 12.0f * dpi;
        const float body_inner_x = radar_x + body_pad;
        const float body_inner_w = radar_w - body_pad * 2.0f;
        const float body_inner_y = body_y + body_pad;
        const float body_inner_h = body_h - body_pad * 2.0f;

        constexpr bypass::overlay::color_t body_fill    { 26, 23, 42, 220 };
        constexpr bypass::overlay::color_t body_stroke  { 70, 66, 92,  90 };
        const float body_radius = 12.0f * dpi;

        bypass::overlay::filled_rounded_rectangle(
            { body_inner_x, body_inner_y, body_inner_w, body_inner_h },
            body_radius, body_fill );
        bypass::overlay::rounded_rectangle(
            { body_inner_x, body_inner_y, body_inner_w, body_inner_h },
            body_radius, body_stroke, 1.0f * dpi );

        {
            const float dot_spacing       = 20.0f * dpi;
            const float outer_dot_radius  = 3.0f  * dpi;
            const float inner_dot_radius  = 2.0f  * dpi;
            const float pattern_cx        = body_inner_w * 0.5f;
            const float pattern_cy        = body_inner_h * 0.5f;
            const float max_dist          = std::sqrt( pattern_cx * pattern_cx + pattern_cy * pattern_cy );
            const float anim_time         = std::chrono::duration< float >(
                std::chrono::steady_clock::now( ).time_since_epoch( ) ).count( );

            constexpr bypass::overlay::color_t inner_dot_col { 18, 12, 36, 255 };

            for ( float dy_pos = 0.0f; dy_pos < body_inner_h; dy_pos += dot_spacing ) {
                for ( float dx_pos = 0.0f; dx_pos < body_inner_w; dx_pos += dot_spacing ) {
                    const float dx = dx_pos - pattern_cx;
                    const float dy = dy_pos - pattern_cy;
                    const float dist = std::sqrt( dx * dx + dy * dy );
                    const float distance_fade = ( std::max )( 0.0f, 1.0f - dist / max_dist );
                    const float pulse = 0.5f + 0.5f * std::sin(
                        anim_time * 2.0f + ( dx_pos + dy_pos ) * 0.05f );
                    const float alpha_f = 0.22f * distance_fade * pulse;
                    if ( alpha_f <= 0.005f ) continue;

                    const auto alpha_byte = static_cast< std::uint8_t >( alpha_f * 255.0f );
                    const float px = body_inner_x + dx_pos + 1.0f * dpi;
                    const float py = body_inner_y + dy_pos + 1.0f * dpi;
                    bypass::overlay::filled_circle(
                        { px, py }, outer_dot_radius,
                        bypass::overlay::color_t {
                            col_accent.r, col_accent.g, col_accent.b, alpha_byte } );
                    bypass::overlay::filled_circle(
                        { px, py }, inner_dot_radius,
                        bypass::overlay::color_t {
                            inner_dot_col.r, inner_dot_col.g, inner_dot_col.b, alpha_byte } );
                }
            }
        }

        {
            auto snap = cache::get_visual_snapshot( );
            if ( snap ) {
                const auto  cam       = cache::camera_location;
                const auto  cam_fwd   = cache::camera_axis_forward;
                const auto  cam_right = cache::camera_axis_right;
                const float world_per_m = 100.0f;
                const float max_world   = config::widgets::radar_range * world_per_m;

                const float cx   = body_inner_x + body_inner_w * 0.5f;
                const float cy   = body_inner_y + body_inner_h * 0.5f;
                const float edge_pad = 6.0f * dpi;
                const float half_w = body_inner_w * 0.5f - edge_pad;
                const float half_h = body_inner_h * 0.5f - edge_pad;
                const float max_extent   = ( std::min )( half_w, half_h );
                const float px_per_world = max_extent / max_world;

                if ( config::widgets::radar_tracers ) {
                    auto tracers = g_cache->get_bullet_tracers( );
                    const float tracer_max_world = max_world;
                    const auto inactive = menu::inactive_accent( );
                    const float base_thick = ( std::max )(
                        config::bullet_tracers::thickness * dpi, 2.0f * dpi );

                    const float pad_lo_x = body_inner_x + edge_pad;
                    const float pad_hi_x = body_inner_x + body_inner_w - edge_pad;
                    const float pad_lo_y = body_inner_y + edge_pad;
                    const float pad_hi_y = body_inner_y + body_inner_h - edge_pad;

                    for ( const auto& t : tracers ) {
                        const auto ds  = t.m_start_location - cam;
                        const auto de  = t.m_end_location   - cam;
                        const float dist_s = std::sqrt( static_cast< float >(
                            ds.vector_scalar( ds ) ) );
                        const float dist_e = std::sqrt( static_cast< float >(
                            de.vector_scalar( de ) ) );
                        if ( dist_s > tracer_max_world && dist_e > tracer_max_world )
                            continue;

                        engine::f_vector head = t.m_end_location;
                        if ( config::bullet_tracers::travel_anim ) {
                            const float travel = ( std::clamp )( t.m_travel_t, 0.0f, 1.0f );
                            head.x = t.m_start_location.x + ( t.m_end_location.x - t.m_start_location.x ) * travel;
                            head.y = t.m_start_location.y + ( t.m_end_location.y - t.m_start_location.y ) * travel;
                            head.z = t.m_start_location.z + ( t.m_end_location.z - t.m_start_location.z ) * travel;
                        }

                        const auto  dhead     = head - cam;
                        const float fwd_s     = static_cast< float >( ds.vector_scalar( cam_fwd ) );
                        const float right_s   = static_cast< float >( ds.vector_scalar( cam_right ) );
                        const float fwd_h     = static_cast< float >( dhead.vector_scalar( cam_fwd ) );
                        const float right_h   = static_cast< float >( dhead.vector_scalar( cam_right ) );

                        const float raw_sx = cx + right_s * px_per_world;
                        const float raw_sy = cy - fwd_s   * px_per_world;
                        const float raw_hx = cx + right_h * px_per_world;
                        const float raw_hy = cy - fwd_h   * px_per_world;

                        const float sx = ( std::clamp )( raw_sx, pad_lo_x, pad_hi_x );
                        const float sy = ( std::clamp )( raw_sy, pad_lo_y, pad_hi_y );
                        const float hx = ( std::clamp )( raw_hx, pad_lo_x, pad_hi_x );
                        const float hy = ( std::clamp )( raw_hy, pad_lo_y, pad_hi_y );

                        const float seg_dx = hx - sx;
                        const float seg_dy = hy - sy;
                        if ( seg_dx * seg_dx + seg_dy * seg_dy < 1.0f ) continue;

                        const float alpha_f  = ( std::clamp )( t.m_alpha, 0.0f, 1.0f );
                        const auto  alpha    = static_cast< std::uint8_t >( alpha_f * 255.0f );
                        if ( alpha == 0 ) continue;

                        const auto  outline_alpha = static_cast< std::uint8_t >( alpha_f * 200.0f );
                        bypass::overlay::line(
                            { sx, sy }, { hx, hy },
                            bypass::overlay::color_t { 0, 0, 0, outline_alpha },
                            base_thick + 1.6f * dpi );

                        bypass::overlay::line(
                            { sx, sy }, { hx, hy },
                            bypass::overlay::color_t { inactive.r, inactive.g, inactive.b, alpha },
                            base_thick );
                    }
                }

                const auto col_pastel = menu::pastel_accent( col_accent );
                constexpr bypass::overlay::color_t teammate_col { 255, 177, 109, 255 };
                constexpr bypass::overlay::color_t downed_col   { 150, 145, 160, 220 };
                constexpr bypass::overlay::color_t dot_outline  { 0, 0, 0, 205 };

                const std::uint8_t local_team_index =
                    g_cache->m_local_team_index.load( std::memory_order_acquire );

                const float dot_r      = 3.5f * dpi;
                const float dot_ring_r = dot_r + 1.5f * dpi;

                for ( const auto& p : snap->players ) {
                    if ( p.m_is_dying ) continue;

                    const auto delta      = p.m_root_location - cam;
                    const float fwd_ax    = static_cast< float >( delta.vector_scalar( cam_fwd ) );
                    const float right_ax  = static_cast< float >( delta.vector_scalar( cam_right ) );
                    const float dist_flat = std::sqrt( fwd_ax * fwd_ax + right_ax * right_ax );
                    if ( dist_flat > max_world ) continue;

                    const float raw_x = cx + right_ax * px_per_world;
                    const float raw_y = cy - fwd_ax   * px_per_world;

                    const float px = ( std::clamp )( raw_x,
                        body_inner_x + edge_pad, body_inner_x + body_inner_w - edge_pad );
                    const float py = ( std::clamp )( raw_y,
                        body_inner_y + edge_pad, body_inner_y + body_inner_h - edge_pad );

                    const bool is_teammate =
                        local_team_index != 0 && p.m_team_index == local_team_index;
                    const bool is_downed = p.m_is_downed;

                    bypass::overlay::color_t dot_col;
                    if ( is_teammate )      dot_col = teammate_col;
                    else if ( is_downed )   dot_col = downed_col;
                    else if ( p.m_is_visible )
                        dot_col = bypass::overlay::color_t {
                            col_accent.r, col_accent.g, col_accent.b, 255 };
                    else
                        dot_col = bypass::overlay::color_t {
                            col_pastel.r, col_pastel.g, col_pastel.b, 210 };

                    bypass::overlay::filled_circle( { px, py }, dot_ring_r, dot_outline );
                    bypass::overlay::filled_circle( { px, py }, dot_r,      dot_col );
                }

                const float viewer_r    = 4.5f * dpi;
                const float viewer_ring = viewer_r + 1.5f * dpi;
                bypass::overlay::filled_circle( { cx, cy }, viewer_ring, dot_outline );
                bypass::overlay::filled_circle( { cx, cy }, viewer_r,
                    bypass::overlay::color_t { 255, 255, 255, 250 } );
            }
        }

        ( void ) text_soft;
    }
}
