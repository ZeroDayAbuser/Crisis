#include <deps/includes.h>

namespace gui::widgets::top_bar {

    struct seg_params_t {
        float x_start;
        float x_end;
        float y_start;
        float y_end;
        float mid_y;
        float half_h;
        float radius;
        float arrow_w;
        bool  round_left;
        bool  round_right;
    };

    static bool seg_scan( const seg_params_t& p, float y, float& lx, float& rx ) {
        if ( y < p.y_start - 0.5f || y > p.y_end + 0.5f ) return false;

        float dy = y - p.mid_y;
        if ( dy < -p.half_h ) dy = -p.half_h;
        if ( dy >  p.half_h ) dy =  p.half_h;

        const float t    = 1.0f - std::fabs( dy ) / p.half_h;
        const float chev = p.arrow_w * ( t < 0.0f ? 0.0f : t );

        const float cap_dx = std::sqrt( ( std::max )( 0.0f, p.radius * p.radius - dy * dy ) );
        const float cap_in = p.radius - cap_dx;

        lx = p.round_left  ? p.x_start + cap_in : p.x_start + chev;
        rx = p.round_right ? p.x_end   - cap_in : p.x_end   - p.arrow_w + chev;
        return rx > lx;
    }

    static void fill_row_aa( float lx, float rx, float y, float y_cov, bypass::overlay::color_t col ) {
        if ( rx - lx <= 1e-4f ) return;
        if ( y_cov <= 1e-4f ) return;

        auto with_alpha = [ & ] ( float cov ) {
            bypass::overlay::color_t c = col;
            const float a = static_cast< float >( col.a ) * cov * y_cov;
            c.a = static_cast< std::uint8_t >( ( std::clamp )( a, 0.0f, 255.0f ) );
            return c;
        };

        const float ceil_lx  = std::ceil ( lx );
        const float floor_rx = std::floor( rx );

        if ( ceil_lx >= floor_rx ) {
            const int start = static_cast< int >( std::floor( lx ) );
            const int end   = static_cast< int >( std::ceil ( rx ) );
            for ( int px = start; px < end; ++px ) {
                const float x0 = static_cast< float >( px );
                const float x1 = x0 + 1.0f;
                const float cov = ( std::min )( x1, rx ) - ( std::max )( x0, lx );
                if ( cov > 1e-4f )
                    bypass::overlay::filled_rectangle(
                        { x0, y, 1.0f, 1.0f }, with_alpha( cov ) );
            }
            return;
        }

        const float left_cov = ceil_lx - lx;
        if ( left_cov > 1e-4f )
            bypass::overlay::filled_rectangle(
                { ceil_lx - 1.0f, y, 1.0f, 1.0f }, with_alpha( left_cov ) );

        if ( floor_rx > ceil_lx )
            bypass::overlay::filled_rectangle(
                { ceil_lx, y, floor_rx - ceil_lx, 1.0f }, with_alpha( 1.0f ) );

        const float right_cov = rx - floor_rx;
        if ( right_cov > 1e-4f )
            bypass::overlay::filled_rectangle(
                { floor_rx, y, 1.0f, 1.0f }, with_alpha( right_cov ) );
    }

    static float now_seconds( ) {
        using clock = std::chrono::steady_clock;
        return std::chrono::duration< float >( clock::now( ).time_since_epoch( ) ).count( );
    }

    static void draw_stat_cell(
        float xL, float xR, float bar_y, float bar_h, float dpi,
        const wchar_t* fa_icon, const wchar_t* caption, const wchar_t* value,
        bypass::overlay::color_t col ) {
        const float pad_l    = 12.0f * dpi;
        const float pad_r    = 8.0f  * dpi;
        const float ic_size  = 22.0f * dpi;
        const float ic_gap   = 8.0f  * dpi;
        const float cap_pt   = 10.5f * dpi;
        const float val_pt   = 15.0f * dpi;

        const float ic_x = xL + pad_l;
        const float ic_y = bar_y + ( bar_h - ic_size ) * 0.5f;
        bypass::overlay::text_ex(
            fa_icon,
            { ic_x, ic_y, ic_size, ic_size },
            col,
            18.0f * dpi,
            bypass::overlay::text_weight_t::regular,
            L"Font Awesome",
            bypass::overlay::text_align_t::center, false );

        const float txt_x  = ic_x + ic_size + ic_gap;
        const float txt_w  = xR - pad_r - txt_x;
        const float cap_y  = bar_y + bar_h * 0.20f;
        const float val_y  = bar_y + bar_h * 0.44f;

        const bypass::overlay::color_t caption_col { 255, 255, 255, 245 };
        bypass::overlay::text_ex(
            caption,
            { txt_x, cap_y, txt_w, cap_pt + 4.0f },
            caption_col,
            cap_pt,
            bypass::overlay::text_weight_t::regular,
            L"Inter-Medium",
            bypass::overlay::text_align_t::left, false );

        bypass::overlay::text_ex(
            value,
            { txt_x, val_y, txt_w, val_pt + 4.0f },
            col,
            val_pt,
            bypass::overlay::text_weight_t::semi_bold,
            L"Geist-Bold",
            bypass::overlay::text_align_t::left, false );
    }

    void render( ) {
        if ( !config::widgets::top_bar )
            return;

        static float         s_fps_window_start = -1.0f;
        static std::uint32_t s_fps_frame_count  = 0;
        static float         s_ema_fps          = 60.0f;
        const float now = now_seconds( );
        if ( s_fps_window_start < 0.0f )
            s_fps_window_start = now;
        ++s_fps_frame_count;
        const float window_sec = now - s_fps_window_start;
        if ( window_sec >= 0.5f ) {
            s_ema_fps = static_cast< float >( s_fps_frame_count ) / window_sec;
            s_fps_frame_count  = 0;
            s_fps_window_start = now;
        }

        static std::uint64_t s_last_ops_snapshot = 0;
        static ULONGLONG     s_last_ops_tick     = 0;
        static float         s_ops_smoothed      = 0.0f;
        const ULONGLONG now_ms = ::GetTickCount64( );
        if ( s_last_ops_tick == 0 ) {
            s_last_ops_tick     = now_ms;
            s_last_ops_snapshot = bypass::g_ops_counter.load( std::memory_order_relaxed );
        }
        const ULONGLONG ops_dt_ms = now_ms - s_last_ops_tick;
        if ( ops_dt_ms >= 500 ) {
            const auto current_ops = bypass::g_ops_counter.load( std::memory_order_relaxed );
            std::uint64_t delta = 0;
            if ( current_ops >= s_last_ops_snapshot )
                delta = current_ops - s_last_ops_snapshot;
            else
                delta = current_ops;
            const float ops_now = static_cast< float >( delta ) * 1000.0f / static_cast< float >( ops_dt_ms );
            s_ops_smoothed = s_ops_smoothed <= 0.0f ? ops_now : s_ops_smoothed + 0.30f * ( ops_now - s_ops_smoothed );
            s_last_ops_snapshot = current_ops;
            s_last_ops_tick     = now_ms;
        }

        const float dpi = gui::esp::dpi_scale( );
        const float sw  = static_cast< float >( gui::screen_width );

        const auto col_accent = menu::current_accent( );
        const bypass::overlay::color_t seg_dark   { 22, 21, 40, 255 };
        const bypass::overlay::color_t seg_accent { col_accent.r, col_accent.g, col_accent.b, 255 };
        const bypass::overlay::color_t outline_col{ 62, 66, 84, 200 };

        const float bar_h      = 50.0f * dpi;
        const float top_margin = 10.0f * dpi;
        const float arrow_w    = bar_h * 0.22f;
        const float half_h     = bar_h * 0.5f;
        const float radius     = half_h;
        const float gap_w      = 6.0f * dpi;
        const float close_gap  = 1.0f * dpi;
        const float outline_t  = 2.0f * dpi;

        const float w_left  = 180.0f * dpi;
        const float w_mid   = 245.0f * dpi;
        const float w_new   = 100.0f * dpi;
        const float w_right = 110.0f * dpi;
        const float total_w = w_left + close_gap + w_mid + close_gap + w_new + close_gap + w_right;

        const float bar_x = std::floor( ( sw - total_w ) * 0.5f );
        const float bar_y = std::floor( top_margin );
        const float mid_y = bar_y + half_h;

        const float xL_start = bar_x;
        const float xL_end   = xL_start + w_left;
        const float xM_start = xL_end + close_gap;
        const float xM_end   = xM_start + w_mid;
        const float xN_start = xM_end + close_gap;
        const float xN_end   = xN_start + w_new;
        const float xR_start = xN_end + close_gap;
        const float xR_end   = xR_start + w_right;

        const seg_params_t seg_L {
            xL_start, xL_end, bar_y, bar_y + bar_h,
            mid_y, half_h, radius, arrow_w,
            true, false
        };
        const seg_params_t seg_M {
            xM_start, xM_end, bar_y, bar_y + bar_h,
            mid_y, half_h, radius, arrow_w,
            false, false
        };
        const seg_params_t seg_N {
            xN_start, xN_end, bar_y, bar_y + bar_h,
            mid_y, half_h, radius, arrow_w,
            false, false
        };
        const seg_params_t seg_R {
            xR_start, xR_end, bar_y, bar_y + bar_h,
            mid_y, half_h, radius, arrow_w,
            false, true
        };

        auto expand = [ & ] ( const seg_params_t& s ) {
            seg_params_t e = s;
            e.x_start -= outline_t;
            e.x_end   += outline_t;
            e.y_start -= outline_t;
            e.y_end   += outline_t;
            e.mid_y    = mid_y;
            e.half_h   = half_h + outline_t;
            e.radius   = radius + outline_t;
            e.arrow_w  = arrow_w + outline_t;
            return e;
        };

        const seg_params_t seg_L_o = expand( seg_L );
        const seg_params_t seg_M_o = expand( seg_M );
        const seg_params_t seg_N_o = expand( seg_N );
        const seg_params_t seg_R_o = expand( seg_R );

        const float y_top_o = bar_y - outline_t;
        const float y_bot_o = bar_y + bar_h + outline_t;

        for ( int iy = static_cast< int >( std::floor( y_top_o ) );
              iy <= static_cast< int >( std::ceil ( y_bot_o ) ); ++iy ) {
            const float y  = static_cast< float >( iy );
            const float y1 = y + 1.0f;
            const float y_cov = ( std::min )( y1, y_bot_o ) - ( std::max )( y, y_top_o );
            if ( y_cov <= 1e-4f ) continue;

            const float y_sample = ( ( std::max )( y, y_top_o ) + ( std::min )( y1, y_bot_o ) ) * 0.5f;

            float lx, rx;
            if ( seg_scan( seg_L_o, y_sample, lx, rx ) )
                fill_row_aa( lx, rx, y, y_cov, outline_col );
            if ( seg_scan( seg_M_o, y_sample, lx, rx ) )
                fill_row_aa( lx, rx, y, y_cov, outline_col );
            if ( seg_scan( seg_N_o, y_sample, lx, rx ) )
                fill_row_aa( lx, rx, y, y_cov, outline_col );
            if ( seg_scan( seg_R_o, y_sample, lx, rx ) )
                fill_row_aa( lx, rx, y, y_cov, outline_col );
        }

        const float y_top = bar_y;
        const float y_bot = bar_y + bar_h;

        for ( int iy = static_cast< int >( std::floor( y_top ) );
              iy <= static_cast< int >( std::ceil ( y_bot ) ); ++iy ) {
            const float y  = static_cast< float >( iy );
            const float y1 = y + 1.0f;
            const float y_cov = ( std::min )( y1, y_bot ) - ( std::max )( y, y_top );
            if ( y_cov <= 1e-4f ) continue;

            const float y_sample = ( ( std::max )( y, y_top ) + ( std::min )( y1, y_bot ) ) * 0.5f;

            float lx, rx;
            if ( seg_scan( seg_L, y_sample, lx, rx ) )
                fill_row_aa( lx, rx, y, y_cov, seg_dark );
            if ( seg_scan( seg_M, y_sample, lx, rx ) )
                fill_row_aa( lx, rx, y, y_cov, seg_dark );
            if ( seg_scan( seg_N, y_sample, lx, rx ) )
                fill_row_aa( lx, rx, y, y_cov, seg_dark );
            if ( seg_scan( seg_R, y_sample, lx, rx ) )
                fill_row_aa( lx, rx, y, y_cov, seg_dark );
        }

        {
            static bool s_logo_registered = false;
            if ( !s_logo_registered )
                s_logo_registered = bypass::overlay::register_image(
                    menu::logo_id, L"C:\\crisis\\logo.png" );

            if ( s_logo_registered ) {
                constexpr float logo_native_aspect = 158.0f / 30.0f;
                const float logo_h = bar_h * 0.56f;
                const float logo_w = logo_h * logo_native_aspect;
                const float logo_x = xL_start + ( w_left - logo_w ) * 0.5f;
                const float logo_y = bar_y + ( bar_h - logo_h ) * 0.5f;
                bypass::overlay::image( menu::logo_id,
                    { logo_x, logo_y, logo_w, logo_h },
                    seg_accent, 1.0f, 0.0f );
            }
        }

        {
            static const wchar_t s_speed[ 2 ] = { L'\xF3FD', L'\0' };
            wchar_t val_buf[ 16 ];
            std::swprintf( val_buf, 16, L"%.0f", s_ema_fps );
            const float fps_shift = 2.0f * dpi;
            draw_stat_cell(
                xN_start + arrow_w - fps_shift, xN_end - arrow_w - fps_shift,
                bar_y, bar_h, dpi,
                s_speed, L"FPS", val_buf, seg_accent );
        }

        {
            static const wchar_t s_user[ 2 ] = { L'\xF007', L'\0' };
            const float ic_size  = 35.0f * dpi;
            const float ic_gap   = 12.0f * dpi;
            const float est_text_w = 108.0f * dpi;
            const float group_w  = ic_size + ic_gap + est_text_w;

            const float ic_x = xM_start + ( w_mid - group_w ) * 0.5f;
            const float ic_y = bar_y + ( bar_h - ic_size ) * 0.5f;

            if ( menu::g_profile_image_ready ) {
                const float ic_r = ic_size * 0.5f;
                const float ic_cx = ic_x + ic_r;
                const float ic_cy = ic_y + ic_r;
                bypass::overlay::filled_circle(
                    { ic_cx, ic_cy }, ic_r + 2.0f * dpi,
                    bypass::overlay::color_t { seg_accent.r, seg_accent.g, seg_accent.b, 220 } );
                bypass::overlay::filled_circle(
                    { ic_cx, ic_cy }, ic_r + 1.0f * dpi,
                    bypass::overlay::color_t { 22, 21, 40, 255 } );
                bypass::overlay::image( menu::profile_image_id,
                    { ic_x, ic_y, ic_size, ic_size },
                    bypass::overlay::color_t { 255, 255, 255, 255 },
                    1.0f, ic_r );
            }
            else {
                bypass::overlay::text_ex(
                    s_user,
                    { ic_x, ic_y, ic_size, ic_size },
                    seg_accent,
                    18.0f * dpi,
                    bypass::overlay::text_weight_t::regular,
                    L"Font Awesome",
                    bypass::overlay::text_align_t::center, false );
            }

            const float txt_x = ic_x + ic_size + ic_gap;
            const float txt_w = est_text_w;
            const float hi_pt = 15.0f * dpi;
            const float pf_pt = 12.0f * dpi;

            wchar_t hello_buf[ 128 ];
            std::swprintf( hello_buf, 128, L"Hello, %s", menu::g_discord_name.c_str( ) );

            bypass::overlay::text_ex(
                hello_buf,
                { txt_x, bar_y + bar_h * 0.18f, txt_w, hi_pt + 4.0f },
                seg_accent,
                hi_pt,
                bypass::overlay::text_weight_t::semi_bold,
                L"Geist-Bold",
                bypass::overlay::text_align_t::left, false );

            static ULONGLONG s_session_start = 0;
            if ( s_session_start == 0 ) s_session_start = ::GetTickCount64( );
            const ULONGLONG uptime_ms = ::GetTickCount64( ) - s_session_start;
            const int up_sec = static_cast< int >( uptime_ms / 1000ULL );
            const int up_h = up_sec / 3600;
            const int up_m = ( up_sec / 60 ) % 60;
            const int up_s = up_sec % 60;

            wchar_t up_buf[ 32 ];
            if ( up_h > 0 )
                std::swprintf( up_buf, 32, L"Loaded %d:%02d:%02d", up_h, up_m, up_s );
            else
                std::swprintf( up_buf, 32, L"Loaded %d:%02d", up_m, up_s );

            const bypass::overlay::color_t sub_col { 255, 255, 255, 210 };
            bypass::overlay::text_ex(
                up_buf,
                { txt_x, bar_y + bar_h * 0.50f, txt_w, pf_pt + 4.0f },
                sub_col,
                pf_pt,
                bypass::overlay::text_weight_t::regular,
                L"Inter-Medium",
                bypass::overlay::text_align_t::left, false );
        }

        {
            static const wchar_t s_cogs[ 2 ] = { L'\xF085', L'\0' };
            wchar_t ops_buf[ 24 ];
            if ( s_ops_smoothed >= 1'000'000.0f )
                std::swprintf( ops_buf, 24, L"%.1fM", s_ops_smoothed / 1'000'000.0f );
            else if ( s_ops_smoothed >= 1'000.0f )
                std::swprintf( ops_buf, 24, L"%.1fk", s_ops_smoothed / 1'000.0f );
            else
                std::swprintf( ops_buf, 24, L"%.0f", s_ops_smoothed );
            draw_stat_cell(
                xR_start + arrow_w, xR_end - radius * 0.5f,
                bar_y, bar_h, dpi,
                s_cogs, L"OPS", ops_buf, seg_accent );
        }
    }
}
