#include <deps/includes.h>

namespace gui::esp {

    struct threat_anim_t {
        ULONGLONG first_seen = 0;
        ULONGLONG last_active = 0;
    };

    static std::unordered_map< std::uint64_t, threat_anim_t > s_anim;
    static ULONGLONG s_last_prune = 0;

    void tick_threat_arrow_anim( ) {
        const ULONGLONG now = ::GetTickCount64( );
        if ( now - s_last_prune < 200 ) return;
        s_last_prune = now;
        for ( auto it = s_anim.begin( ); it != s_anim.end( ); ) {
            if ( now - it->second.last_active > 250 ) it = s_anim.erase( it );
            else ++it;
        }
    }

    float dpi_scale( ) {
        static UINT s_cached_dpi = 0;
        static ULONGLONG s_last_query = 0;
        const ULONGLONG now = ::GetTickCount64( );
        if ( s_cached_dpi == 0 || now - s_last_query > 2000 ) {
            const HWND hwnd = bypass::target::m_window_handle;
            UINT dpi = 96;
            if ( hwnd && ::IsWindow( hwnd ) )
                dpi = ::GetDpiForWindow( hwnd );
            if ( dpi == 0 )
                dpi = ::GetDpiForSystem( );
            if ( dpi < 72 )   dpi = 72;
            if ( dpi > 288 )  dpi = 288;
            s_cached_dpi = dpi;
            s_last_query = now;
        }
        return static_cast< float >( s_cached_dpi ) / 96.0f;
    }

    struct pt_t { float x = 0.0f, y = 0.0f; };

    static void build_rounded_triangle_polygon(
        pt_t v0, pt_t v1, pt_t v2,
        float corner_r,
        std::array< pt_t, 48 >& out,
        std::size_t& out_n ) {
        out_n = 0;
        const pt_t verts[ 3 ] = { v0, v1, v2 };
        constexpr int kBezSamples = 12;

        for ( int i = 0; i < 3; ++i ) {
            const pt_t& prev = verts[ ( i + 2 ) % 3 ];
            const pt_t& p    = verts[ i ];
            const pt_t& next = verts[ ( i + 1 ) % 3 ];

            const float in_dx  = p.x - prev.x;
            const float in_dy  = p.y - prev.y;
            const float in_len = std::sqrt( in_dx * in_dx + in_dy * in_dy );
            const float udx_in = in_len > 1e-6f ? in_dx / in_len : 0.0f;
            const float udy_in = in_len > 1e-6f ? in_dy / in_len : 0.0f;

            const float out_dx  = next.x - p.x;
            const float out_dy  = next.y - p.y;
            const float out_len = std::sqrt( out_dx * out_dx + out_dy * out_dy );
            const float udx_out = out_len > 1e-6f ? out_dx / out_len : 0.0f;
            const float udy_out = out_len > 1e-6f ? out_dy / out_len : 0.0f;

            const float r_use = ( std::min )( { corner_r, in_len * 0.42f, out_len * 0.42f } );

            const pt_t before { p.x - udx_in  * r_use, p.y - udy_in  * r_use };
            const pt_t after  { p.x + udx_out * r_use, p.y + udy_out * r_use };

            for ( int s = 0; s <= kBezSamples && out_n < out.size( ); ++s ) {
                const float t = static_cast< float >( s ) / static_cast< float >( kBezSamples );
                const float u = 1.0f - t;
                const float x = u * u * before.x + 2.0f * u * t * p.x + t * t * after.x;
                const float y = u * u * before.y + 2.0f * u * t * p.y + t * t * after.y;
                out[ out_n++ ] = { x, y };
            }
        }
    }

    static void polygon_fill(
        const std::array< pt_t, 48 >& poly, std::size_t n,
        bypass::overlay::color_t fill ) {
        if ( n < 3 ) return;
        float ymin = poly[ 0 ].y;
        float ymax = poly[ 0 ].y;
        for ( std::size_t i = 1; i < n; ++i ) {
            if ( poly[ i ].y < ymin ) ymin = poly[ i ].y;
            if ( poly[ i ].y > ymax ) ymax = poly[ i ].y;
        }
        std::array< float, 64 > xs {};
        for ( float y = std::floor( ymin ); y <= std::ceil( ymax ); y += 1.0f ) {
            std::size_t xc = 0;
            for ( std::size_t i = 0; i < n && xc < xs.size( ); ++i ) {
                const pt_t& p0 = poly[ i ];
                const pt_t& p1 = poly[ ( i + 1 ) % n ];
                const float y_lo = ( std::min )( p0.y, p1.y );
                const float y_hi = ( std::max )( p0.y, p1.y );
                if ( y <  y_lo ) continue;
                if ( y >= y_hi ) continue;
                const float dy = p1.y - p0.y;
                if ( std::fabs( dy ) < 1e-6f ) continue;
                const float t = ( y - p0.y ) / dy;
                xs[ xc++ ] = p0.x + t * ( p1.x - p0.x );
            }
            if ( xc < 2 ) continue;
            for ( std::size_t i = 1; i < xc; ++i ) {
                float k = xs[ i ];
                std::size_t j = i;
                while ( j > 0 && xs[ j - 1 ] > k ) { xs[ j ] = xs[ j - 1 ]; --j; }
                xs[ j ] = k;
            }
            for ( std::size_t i = 0; i + 1 < xc; i += 2 ) {
                const float x0 = xs[ i ];
                const float x1 = xs[ i + 1 ];
                if ( x1 > x0 )
                    bypass::overlay::filled_rectangle(
                        { x0, y, x1 - x0, 1.0f }, fill );
            }
        }
    }

    static void polygon_outline(
        const std::array< pt_t, 48 >& poly, std::size_t n,
        bypass::overlay::color_t stroke, float thickness ) {
        if ( n < 2 ) return;
        for ( std::size_t i = 0; i < n; ++i ) {
            const pt_t& p0 = poly[ i ];
            const pt_t& p1 = poly[ ( i + 1 ) % n ];
            bypass::overlay::line(
                { p0.x, p0.y }, { p1.x, p1.y }, stroke, thickness );
        }
    }

    void draw_threat_arrow(
        std::uint64_t player_key,
        float edge_x, float edge_y,
        float center_x, float center_y,
        float distance_m,
        float dpi_scale ) {
        const ULONGLONG now = ::GetTickCount64( );

        auto& anim = s_anim[ player_key ];
        if ( anim.first_seen == 0 ) anim.first_seen = now;
        anim.last_active = now;

        constexpr ULONGLONG anim_duration = 560;
        const ULONGLONG elapsed = now - anim.first_seen;
        const float t = elapsed >= anim_duration
            ? 1.0f
            : static_cast< float >( elapsed ) / static_cast< float >( anim_duration );

        const float et = 1.0f - std::pow( 1.0f - t, 5.0f );

        const float dvx0 = edge_x - center_x;
        const float dvy0 = edge_y - center_y;
        const float d0 = std::sqrt( dvx0 * dvx0 + dvy0 * dvy0 );
        if ( d0 < 1e-4f ) return;
        const float dirx0 = dvx0 / d0;
        const float diry0 = dvy0 / d0;

        const float edge_offset = 34.0f * dpi_scale;
        const float target_x = edge_x - dirx0 * edge_offset;
        const float target_y = edge_y - diry0 * edge_offset;

        const float mid_x = ( edge_x + center_x ) * 0.5f;
        const float mid_y = ( edge_y + center_y ) * 0.5f;

        const float cur_x = mid_x + ( target_x - mid_x ) * et;
        const float cur_y = mid_y + ( target_y - mid_y ) * et;

        constexpr float scale_start = 1.55f;
        constexpr float scale_end   = 1.0f;
        const float anim_scale = scale_start + ( scale_end - scale_start ) * et;
        const float s = anim_scale * dpi_scale;

        const float dvx = cur_x - center_x;
        const float dvy = cur_y - center_y;
        const float d_len = std::sqrt( dvx * dvx + dvy * dvy );
        if ( d_len < 1e-4f ) return;
        const float dir_x = dvx / d_len;
        const float dir_y = dvy / d_len;
        const float perp_x = -dir_y;
        const float perp_y =  dir_x;

        const float depth      = 54.0f * s;
        const float base_half  = 22.0f * s;
        const float corner_r   = 5.0f  * s;
        const float outline_t  = 2.0f  * dpi_scale;
        const float circle_r   = 13.0f * dpi_scale;
        const float circle_gap = 9.0f  * dpi_scale;
        const float icon_pt    = 16.0f * dpi_scale;

        const float tip_x   = cur_x;
        const float tip_y   = cur_y;
        const float base_cx = cur_x - dir_x * depth;
        const float base_cy = cur_y - dir_y * depth;
        const float lx = base_cx + perp_x * base_half;
        const float ly = base_cy + perp_y * base_half;
        const float rx = base_cx - perp_x * base_half;
        const float ry = base_cy - perp_y * base_half;

        const float circle_cx = base_cx - dir_x * ( circle_gap + circle_r );
        const float circle_cy = base_cy - dir_y * ( circle_gap + circle_r );

        const auto col_accent = menu::current_accent( );
        const auto col_pastel = menu::pastel_accent( col_accent );

        const float fade = ( std::clamp )( t * 2.5f, 0.0f, 1.0f );
        auto with_a = [ & ]( bypass::overlay::color_t c, float extra_scale = 1.0f ) {
            const float a = static_cast< float >( c.a ) * fade * extra_scale;
            c.a = static_cast< std::uint8_t >( ( std::clamp )( a, 0.0f, 255.0f ) );
            return c;
        };

        constexpr bypass::overlay::color_t bg_fill { 15, 13, 26, 255 };
        const bypass::overlay::color_t fill_col    = bg_fill;
        const bypass::overlay::color_t outline_col {
            col_accent.r, col_accent.g, col_accent.b, 255 };
        const bypass::overlay::color_t bg_circle   = bg_fill;
        const bypass::overlay::color_t icon_col {
            col_accent.r, col_accent.g, col_accent.b, 255 };

        std::array< pt_t, 48 > poly {};
        std::size_t poly_n = 0;
        build_rounded_triangle_polygon(
            { tip_x, tip_y }, { lx, ly }, { rx, ry },
            corner_r, poly, poly_n );

        polygon_fill(    poly, poly_n, with_a( fill_col ) );
        polygon_outline( poly, poly_n, with_a( outline_col ), outline_t );

        bypass::overlay::filled_circle(
            { circle_cx, circle_cy }, circle_r,
            with_a( bg_circle ) );
        bypass::overlay::circle(
            { circle_cx, circle_cy }, circle_r,
            with_a( outline_col ),
            outline_t );

        static const wchar_t s_warn_glyph[ 2 ] = { L'\xF12A', L'\0' };
        const float icon_box = icon_pt + 6.0f;
        bypass::overlay::text_ex(
            s_warn_glyph,
            { circle_cx - icon_box * 0.5f, circle_cy - icon_box * 0.5f,
              icon_box, icon_box },
            with_a( icon_col ),
            icon_pt,
            bypass::overlay::text_weight_t::bold,
            L"Font Awesome",
            bypass::overlay::text_align_t::center,
            false );

        if ( distance_m > 0.0f ) {
            wchar_t dist_buf[ 16 ];
            std::swprintf( dist_buf, std::size( dist_buf ), L"%.0fm", distance_m );
            const float lbl_pt  = 13.0f * dpi_scale;
            const float lbl_off = circle_r + 20.0f * dpi_scale;
            const float lbl_x   = circle_cx - dir_x * lbl_off - 30.0f * dpi_scale;
            const float lbl_y   = circle_cy - dir_y * lbl_off - lbl_pt * 0.5f;
            const bypass::overlay::color_t dist_col {
                col_accent.r, col_accent.g, col_accent.b, 245 };
            bypass::overlay::text_ex(
                dist_buf,
                { lbl_x, lbl_y, 60.0f * dpi_scale, lbl_pt + 4.0f },
                with_a( dist_col ),
                lbl_pt,
                bypass::overlay::text_weight_t::bold,
                L"FiraCode-Light",
                bypass::overlay::text_align_t::center,
                false );
        }
    }
}
