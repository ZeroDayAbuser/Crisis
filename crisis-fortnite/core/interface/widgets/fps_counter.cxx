#include <deps/includes.h>

namespace gui::widgets::fps_counter {

    static float       s_last_time = -1.0f;
    static float       s_ema_fps   = 0.0f;
    static float       s_frame_ms  = 0.0f;

    static float now_seconds( ) {
        using clock = std::chrono::steady_clock;
        return std::chrono::duration< float >( clock::now( ).time_since_epoch( ) ).count( );
    }

    void render( ) {
        if ( !config::widgets::fps_counter )
            return;

        const float t = now_seconds( );
        if ( s_last_time >= 0.0f ) {
            const float dt = t - s_last_time;
            if ( dt > 1e-5f && dt < 1.0f ) {
                const float inst = 1.0f / dt;
                s_ema_fps = s_ema_fps <= 0.0f ? inst : s_ema_fps + 0.10f * ( inst - s_ema_fps );
                s_frame_ms = dt * 1000.0f;
            }
        }
        s_last_time = t;

        const float dpi = gui::esp::dpi_scale( );
        const auto slot = widgets::place(
            widgets::anchor_t::top_right, 132.0f, 46.0f );

        const auto col_accent = menu::current_accent( );
        constexpr bypass::overlay::color_t bg_col     { 15, 13, 26, 210 };
        constexpr bypass::overlay::color_t bg_inner   { 32, 29, 42,  60 };
        constexpr bypass::overlay::color_t border_col { 62, 66, 84, 150 };
        constexpr bypass::overlay::color_t text_col   { 240, 236, 250, 245 };
        constexpr bypass::overlay::color_t sub_col    { 190, 186, 210, 190 };

        const float radius = 8.0f * dpi;

        bypass::overlay::filled_rounded_rectangle(
            { slot.x, slot.y, slot.w, slot.h }, radius, bg_col );
        bypass::overlay::filled_rounded_rectangle(
            { slot.x, slot.y, slot.w, slot.h }, radius, bg_inner );
        bypass::overlay::rounded_rectangle(
            { slot.x, slot.y, slot.w, slot.h }, radius, border_col, 1.0f );

        const float accent_h = 3.0f * dpi;
        bypass::overlay::filled_rounded_rectangle(
            { slot.x + radius, slot.y, slot.w - radius * 2.0f, accent_h },
            accent_h * 0.5f,
            bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 220 } );

        wchar_t fps_buf[ 24 ];
        wchar_t ms_buf [ 24 ];
        std::swprintf( fps_buf, std::size( fps_buf ), L"%.0f FPS", s_ema_fps );
        std::swprintf( ms_buf,  std::size( ms_buf  ), L"%.2f ms",  s_frame_ms );

        const float fps_pt = 15.0f * dpi;
        const float sub_pt = 10.0f * dpi;
        const float pad_x  = 12.0f * dpi;

        bypass::overlay::text_ex(
            fps_buf,
            { slot.x + pad_x, slot.y + accent_h + 4.0f * dpi,
              slot.w - pad_x * 2.0f, fps_pt + 4.0f },
            text_col, fps_pt,
            bypass::overlay::text_weight_t::bold,
            L"Geist-Bold",
            bypass::overlay::text_align_t::left, false );

        bypass::overlay::text_ex(
            ms_buf,
            { slot.x + pad_x, slot.y + accent_h + fps_pt + 6.0f * dpi,
              slot.w - pad_x * 2.0f, sub_pt + 4.0f },
            sub_col, sub_pt,
            bypass::overlay::text_weight_t::regular,
            L"FiraCode-Light",
            bypass::overlay::text_align_t::left, false );

        static const wchar_t s_gauge_glyph[ 2 ] = { L'\xF625', L'\0' };
        const float gauge_pt = 14.0f * dpi;
        bypass::overlay::text_ex(
            s_gauge_glyph,
            { slot.x + slot.w - pad_x - gauge_pt, slot.y + accent_h + 6.0f * dpi,
              gauge_pt, gauge_pt },
            bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 220 },
            gauge_pt,
            bypass::overlay::text_weight_t::regular,
            L"Font Awesome",
            bypass::overlay::text_align_t::center, false );
    }
}
