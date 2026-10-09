#pragma once

namespace gui {
    namespace esp {
        // Three size tiers — Regular (15), Medium (13.5), Small (11.5).
        // Legacy names alias to the two tiers historically used by
        // player.cxx call sites.
        constexpr float text_size_regular = 15.f;
        constexpr float text_size_medium  = 13.5f;
        constexpr float text_size_small   = 11.5f;
        constexpr float text_medium_size  = text_size_regular;
        constexpr float text_small_size   = text_size_medium;

        // Map the per-slot enum (config::text_sizes[slot]) to pixel size.
        //   0 = Small, 1 = Medium, 2 = Regular
        inline float size_for_enum( int e ) {
            switch ( e ) {
            case 0:  return text_size_small;
            case 2:  return text_size_regular;
            default: return text_size_medium;
            }
        }

        struct pending_text_t {
            std::wstring value;
            color_t      color { 255, 255, 255, 255 };
            float        size { text_small_size };
            float        alpha_scale { 1.f };
            bool         valid { false };
            // Optional per-slot horizontal expansion of the frosted
            // background plate. Set by the caller when adornments
            // (team-color circle on the left, streamsnipe icon on the
            // right) sit outside the text bounds and should be tucked
            // inside the plate with a gap.
            float        bg_left_pad  { 0.f };
            float        bg_right_pad { 0.f };
        };
        inline pending_text_t g_pending[ config::player::stats::slot_count ] {};

        // 4-anchor enum — text stacks can attach to any side of the box.
        enum class anchor_t : std::uint8_t {
            top    = 0,   // stacked above the box, centred horizontally
            bottom = 1,   // stacked below the box, centred horizontally
            left   = 2,   // right-aligned column to the left of the box
            right  = 3,   // left-aligned column to the right of the box
        };

        struct layout_t {
            rect_t box{};
            float  top_offset    = 0.f;
            float  bottom_offset = 0.f;
            // Vertical cursors for the side columns — primed to top_gap
            // by draw_text on first hit so side stacks align with the
            // top-column start.
            float  left_offset   = 0.f;
            float  right_offset  = 0.f;
            // Horizontal shift applied to the LEFT column's right edge
            // when the health bar sits on the left; health.cxx sets it.
            float  left_hb_offset  = 0.f;
            // Same for the RIGHT column when a bar is on the right side.
            float  right_hb_offset = 0.f;
        };

        // Forward-declared here so draw_text can call it; body lives
        // below after the layout/geometry helpers.
        namespace helper {
            inline constexpr float line_gap    = 2.f;
            inline constexpr float bottom_gap  = 2.f;
            inline constexpr float top_gap     = 6.f;
            inline constexpr float area_width  = 160.f;
            inline constexpr float side_gap    = 4.f;    // gap between box edge and side columns

            // Frosted background plate.
            //   - Slightly rounded rectangle (3 px radius = "slightly smoothed").
            //   - Interior: dark-grey frosted blur (matches the menu chrome
            //     transparent-dark background rather than the text tint).
            //   - Outline: 1 px, coloured with the text colour so each plate
            //     is visually keyed to its label.
            //   - Sprinkles: a small fixed pattern of ~1 px dots in the text
            //     colour, giving the plate a "frosted" texture that reads as
            //     a coloured particulate under the blur.
            //   - Width follows the text width; caller may extend the plate
            //     left/right via bg_left_pad / bg_right_pad to tuck
            //     adornments (team circle, brand icon) inside the plate with
            //     an implicit gap between the adornment and the plate edge.
            inline void draw_text_background( rect_t area, bool enabled,
                                              color_t text_color, float alpha_scale,
                                              anchor_t anchor, const wchar_t* value,
                                              float font_size,
                                              float bg_left_pad  = 0.f,
                                              float bg_right_pad = 0.f ) {
                if ( !enabled || !value ) return;
                std::size_t n = 0;
                while ( value[ n ] ) ++n;
                const float text_w = static_cast< float >( n ) * font_size * 0.60f;
                if ( text_w <= 0.f ) return;

                float text_x = area.x;
                if ( anchor == anchor_t::top || anchor == anchor_t::bottom )
                    text_x = area.x + ( area.width - text_w ) * 0.5f;
                else if ( anchor == anchor_t::left )
                    text_x = area.x + area.width - text_w;

                // Base plate hugs the text (~3 px horizontal padding on each
                // side), then extends by whatever the caller requested to
                // wrap left/right adornments.
                const rect_t plate {
                    text_x - 3.f - bg_left_pad,
                    area.y + 1.f,
                    text_w + 6.f + bg_left_pad + bg_right_pad,
                    area.height - 2.f };

                // Alpha scaling for the whole stack — the fade-out that
                // distant / occluded players use also fades the plate.
                const std::uint8_t frost_a  = static_cast< std::uint8_t >( 160.f * alpha_scale );
                const std::uint8_t border_a = static_cast< std::uint8_t >( 200.f * alpha_scale );
                const std::uint8_t streak_a = static_cast< std::uint8_t >( 145.f * alpha_scale );

                constexpr float k_corner_radius = 3.f;

                bypass::overlay::frosted_panel( plate, k_corner_radius, 5.5f,
                    color_t { 18, 18, 22, frost_a } );

                struct frost_dash_t { float rx, ry, w_frac; };
                constexpr frost_dash_t dashes[] = {
                    { 0.06f, 0.22f, 0.16f },
                    { 0.30f, 0.22f, 0.10f },
                    { 0.50f, 0.22f, 0.18f },
                    { 0.78f, 0.22f, 0.13f },
                    { 0.10f, 0.52f, 0.12f },
                    { 0.32f, 0.52f, 0.20f },
                    { 0.62f, 0.52f, 0.13f },
                    { 0.84f, 0.52f, 0.09f },
                    { 0.05f, 0.78f, 0.17f },
                    { 0.34f, 0.78f, 0.13f },
                    { 0.60f, 0.78f, 0.15f },
                    { 0.85f, 0.78f, 0.08f },
                };
                const color_t streak_col {
                    text_color.r, text_color.g, text_color.b, streak_a };
                for ( const auto& d : dashes ) {
                    bypass::overlay::filled_rectangle(
                        { plate.x + d.rx * plate.width,
                          plate.y + d.ry * plate.height,
                          d.w_frac * plate.width,
                          1.0f },
                        streak_col );
                }

                bypass::overlay::rounded_rectangle( plate, k_corner_radius,
                    color_t { text_color.r, text_color.g, text_color.b, border_a }, 1.0f );
            }

            inline void draw_text( layout_t& layout, const wchar_t* value, color_t color,
                                    float size, anchor_t anchor, float alpha_scale = 1.f,
                                    float outline_width = 0.f, bool background_enabled = false,
                                    text_weight_t weight = text_weight_t::semi_bold,
                                    float bg_left_pad = 0.f, float bg_right_pad = 0.f ) {
                const float center_x = layout.box.x + layout.box.width * 0.5f;
                const float step = size + line_gap;

                rect_t area {};
                text_align_t align = text_align_t::center;

                if ( anchor == anchor_t::top ) {
                    if ( layout.top_offset == 0.f ) layout.top_offset = top_gap;
                    const float y = layout.box.y - layout.top_offset - size;
                    layout.top_offset += step;
                    area = { center_x - area_width * 0.5f, y, area_width, size + 4.f };
                }
                else if ( anchor == anchor_t::bottom ) {
                    if ( layout.bottom_offset == 0.f ) layout.bottom_offset = bottom_gap;
                    const float y = layout.box.y + layout.box.height + layout.bottom_offset;
                    layout.bottom_offset += step;
                    area = { center_x - area_width * 0.5f, y, area_width, size + 4.f };
                }
                else if ( anchor == anchor_t::left ) {
                    if ( layout.left_offset == 0.f ) layout.left_offset = top_gap;
                    const float y_raw = layout.box.y + layout.left_offset;
                    const float y_max = layout.box.y + layout.box.height - size - 4.f;
                    const float y     = y_raw > y_max ? ( std::max )( layout.box.y, y_max ) : y_raw;
                    layout.left_offset += step;
                    const float right_edge = layout.box.x - side_gap - layout.left_hb_offset;
                    area = { right_edge - area_width, y, area_width, size + 4.f };
                    align = text_align_t::right;
                }
                else {  // anchor_t::right
                    if ( layout.right_offset == 0.f ) layout.right_offset = top_gap;
                    const float y_raw = layout.box.y + layout.right_offset;
                    const float y_max = layout.box.y + layout.box.height - size - 4.f;
                    const float y     = y_raw > y_max ? ( std::max )( layout.box.y, y_max ) : y_raw;
                    layout.right_offset += step;
                    const float left_edge = layout.box.x + layout.box.width + side_gap + layout.right_hb_offset;
                    area = { left_edge, y, area_width, size + 4.f };
                    align = text_align_t::left;
                }

                color.a = static_cast< std::uint8_t >( color.a * alpha_scale );

                // Background plate first (behind everything).
                draw_text_background( area, background_enabled, color, alpha_scale,
                    anchor, value, size, bg_left_pad, bg_right_pad );

                const wchar_t* font = ( size < text_medium_size ) ? L"FiraCode-Light" : L"FiraCode-Medium";

                // Outline stamp — 4× at cardinal offsets in the text's
                // own colour (dimmed to 55%) so the outline reads as a
                // soft glow rather than a hard dark stamp.
                if ( outline_width > 0.f ) {
                    color_t oc { color.r, color.g, color.b,
                                 static_cast< std::uint8_t >( color.a * 0.55f ) };
                    const float w = outline_width;
                    const rect_t off[ 4 ] = {
                        { area.x - w, area.y,     area.width, area.height },
                        { area.x + w, area.y,     area.width, area.height },
                        { area.x,     area.y - w, area.width, area.height },
                        { area.x,     area.y + w, area.width, area.height },
                    };
                    for ( const auto& r : off )
                        bypass::overlay::text_async( value, r, oc, size,
                            weight, font, align, false );
                }

                bypass::overlay::text_async( value, area, color, size,
                    text_weight_t::semi_bold, font, align, false );
            }
        }

        // ── Slot-based ordering (2026-08-25, extended 2026-08-29) ────
        // Per-frame lookup tables — populated by begin_frame(). Each
        // slot gets a display position (index within its anchor group
        // after removing disabled slots) AND a y offset (distance from
        // box edge where its text should sit).
        inline int   g_slot_top_pos  [ config::player::stats::slot_count ]{};
        inline int   g_slot_bot_pos  [ config::player::stats::slot_count ]{};
        inline int   g_slot_left_pos [ config::player::stats::slot_count ]{};
        inline int   g_slot_right_pos[ config::player::stats::slot_count ]{};
        inline float g_slot_y_offset [ config::player::stats::slot_count ]{};

        inline bool slot_enabled( int slot ) {
            using namespace config::player::stats;
            switch ( slot ) {
            case slot_name:          return player_name;
            case slot_platform:      return platform;
            case slot_rank:          return rank;
            case slot_kd:            return kill_deaths;
            case slot_winrate:       return winrate;
            case slot_battle_pass:   return battle_pass;
            case slot_streamsnipe:   return streamsnipe;
            case slot_state:         return state;
            case slot_distance:      return distance;
            case slot_weapon_name:   return weapon_name;
            case slot_weapon_ammo:   return weapon_ammo;
            case slot_weapon_damage: return weapon_damage;
            case slot_reloading:     return weapon_reloading;
            default:                 return false;
            }
        }

        // Per-slot text size — reads config::text_sizes[] via size_for_enum.
        inline float size_for_slot( int slot ) {
            using namespace config::player::stats;
            if ( slot < 0 || slot >= slot_count ) return text_size_medium;
            return size_for_enum( text_sizes[ slot ] );
        }

        // Rebuild the position + y-offset tables from config anchors +
        // orders + individual toggles. 4 anchor groups now.
        inline void begin_frame( ) {
            using namespace config::player::stats;
            for ( int i = 0; i < slot_count; ++i ) {
                g_pending[ i ].valid = false;
                g_pending[ i ].value.clear( );
            }
            struct entry_t { int slot; int order; };
            entry_t groups[ 4 ][ slot_count ]{};
            int ngroups[ 4 ] { 0, 0, 0, 0 };
            for ( int i = 0; i < slot_count; ++i ) {
                g_slot_top_pos  [ i ] = -1;
                g_slot_bot_pos  [ i ] = -1;
                g_slot_left_pos [ i ] = -1;
                g_slot_right_pos[ i ] = -1;
                g_slot_y_offset [ i ] = 0.f;
                if ( !slot_enabled( i ) ) continue;
                int a = text_anchors[ i ];
                if ( a < 0 || a > 3 ) a = 0;
                groups[ a ][ ngroups[ a ]++ ] = { i, text_orders[ i ] };
            }
            auto sort_entries = [ ] ( entry_t* arr, int n ) {
                for ( int i = 1; i < n; ++i ) {
                    entry_t k = arr[ i ];
                    int j = i - 1;
                    while ( j >= 0 && arr[ j ].order > k.order ) { arr[ j + 1 ] = arr[ j ]; --j; }
                    arr[ j + 1 ] = k;
                }
            };
            for ( int a = 0; a < 4; ++a ) sort_entries( groups[ a ], ngroups[ a ] );

            // Top: y_render = box.y - y_offset (goes UP).
            {
                float cursor = helper::top_gap;
                for ( int i = 0; i < ngroups[ 0 ]; ++i ) {
                    const int slot = groups[ 0 ][ i ].slot;
                    const float sz = size_for_slot( slot );
                    g_slot_top_pos [ slot ] = i;
                    g_slot_y_offset[ slot ] = cursor + sz;
                    cursor += sz + helper::line_gap;
                }
            }
            // Bottom: y_render = box.y + box.height + y_offset.
            {
                float cursor = helper::bottom_gap;
                for ( int i = 0; i < ngroups[ 1 ]; ++i ) {
                    const int slot = groups[ 1 ][ i ].slot;
                    const float sz = size_for_slot( slot );
                    g_slot_bot_pos [ slot ] = i;
                    g_slot_y_offset[ slot ] = cursor;
                    cursor += sz + helper::line_gap;
                }
            }
            // Left column: y_render = box.y + y_offset (goes DOWN).
            {
                float cursor = helper::top_gap;
                for ( int i = 0; i < ngroups[ 2 ]; ++i ) {
                    const int slot = groups[ 2 ][ i ].slot;
                    const float sz = size_for_slot( slot );
                    g_slot_left_pos[ slot ] = i;
                    g_slot_y_offset[ slot ] = cursor;
                    cursor += sz + helper::line_gap;
                }
            }
            // Right column: y_render = box.y + y_offset.
            {
                float cursor = helper::top_gap;
                for ( int i = 0; i < ngroups[ 3 ]; ++i ) {
                    const int slot = groups[ 3 ][ i ].slot;
                    const float sz = size_for_slot( slot );
                    g_slot_right_pos[ slot ] = i;
                    g_slot_y_offset [ slot ] = cursor;
                    cursor += sz + helper::line_gap;
                }
            }
        }

        // Compute a slot's screen rect WITHOUT drawing. 4-anchor aware.
        inline rect_t rect_for_slot( const layout_t& layout, int slot, float size ) {
            using namespace config::player::stats;
            const int anchor = text_anchors[ slot ];
            int pos = -1;
            switch ( anchor ) {
            case 0: pos = g_slot_top_pos  [ slot ]; break;
            case 1: pos = g_slot_bot_pos  [ slot ]; break;
            case 2: pos = g_slot_left_pos [ slot ]; break;
            default: pos = g_slot_right_pos[ slot ]; break;
            }
            if ( pos < 0 ) return { 0.f, 0.f, 0.f, 0.f };
            const float center_x = layout.box.x + layout.box.width * 0.5f;
            if ( anchor == 0 )
                return { center_x - helper::area_width * 0.5f,
                         layout.box.y - g_slot_y_offset[ slot ],
                         helper::area_width, size + 4.f };
            if ( anchor == 1 )
                return { center_x - helper::area_width * 0.5f,
                         layout.box.y + layout.box.height + g_slot_y_offset[ slot ],
                         helper::area_width, size + 4.f };
            if ( anchor == 2 ) {
                const float y = layout.box.y + g_slot_y_offset[ slot ];
                const float right_edge = layout.box.x - helper::side_gap - layout.left_hb_offset;
                return { right_edge - helper::area_width, y, helper::area_width, size + 4.f };
            }
            // anchor == 3 (right)
            const float y = layout.box.y + g_slot_y_offset[ slot ];
            const float left_edge = layout.box.x + layout.box.width + helper::side_gap + layout.right_hb_offset;
            return { left_edge, y, helper::area_width, size + 4.f };
        }

        // ── Deferred slot draw ──────────────────────────
        inline void text_small_slot( const layout_t&, int slot, const wchar_t* value,
                                      color_t color, float alpha_scale = 1.f,
                                      float bg_left_pad = 0.f, float bg_right_pad = 0.f ) {
            using namespace config::player::stats;
            if ( slot < 0 || slot >= slot_count ) return;
            if ( text_color_override_enabled[ slot ] ) {
                const auto& oc = text_color_override[ slot ];
                color = color_t { oc.r, oc.g, oc.b, oc.a };
            }
            g_pending[ slot ].value        = value;
            g_pending[ slot ].color        = color;
            g_pending[ slot ].size         = size_for_slot( slot );
            g_pending[ slot ].alpha_scale  = alpha_scale;
            g_pending[ slot ].valid        = true;
            g_pending[ slot ].bg_left_pad  = bg_left_pad;
            g_pending[ slot ].bg_right_pad = bg_right_pad;
        }
        inline void text_medium_slot( const layout_t&, int slot, const wchar_t* value,
                                       color_t color, float alpha_scale = 1.f,
                                       float bg_left_pad = 0.f, float bg_right_pad = 0.f ) {
            text_small_slot( layout_t{}, slot, value, color, alpha_scale,
                bg_left_pad, bg_right_pad );
        }

        // Sort deferred slots by config order per anchor, then hand
        // each off to draw_text — which accumulates the layout offsets
        // so ONLY drawn texts occupy space.
        inline void flush_texts( layout_t& layout ) {
            struct entry_t { int slot; int order; };
            entry_t groups[ 4 ][ config::player::stats::slot_count ]{};
            int ngroups[ 4 ] { 0, 0, 0, 0 };
            for ( int i = 0; i < config::player::stats::slot_count; ++i ) {
                if ( !g_pending[ i ].valid ) continue;
                int a = config::player::stats::text_anchors[ i ];
                if ( a < 0 || a > 3 ) a = 0;
                groups[ a ][ ngroups[ a ]++ ] = { i, config::player::stats::text_orders[ i ] };
            }
            auto sort_entries = [ ] ( entry_t* arr, int n ) {
                for ( int i = 1; i < n; ++i ) {
                    entry_t k = arr[ i ];
                    int j = i - 1;
                    while ( j >= 0 && arr[ j ].order > k.order ) { arr[ j + 1 ] = arr[ j ]; --j; }
                    arr[ j + 1 ] = k;
                }
            };
            for ( int a = 0; a < 4; ++a ) sort_entries( groups[ a ], ngroups[ a ] );

            for ( int i = 0; i < ngroups[ 0 ]; ++i ) {
                const int slot = groups[ 0 ][ i ].slot;
                const auto& p = g_pending[ slot ];
                const float pre = layout.top_offset == 0.f ? helper::top_gap : layout.top_offset;
                g_slot_y_offset[ slot ] = pre + p.size;
                helper::draw_text( layout, p.value.c_str( ), p.color, p.size,
                    anchor_t::top, p.alpha_scale,
                    config::player::stats::text_outline_enabled[ slot ]
                        ? config::player::stats::text_outline_width[ slot ] : 0.f,
                    config::player::stats::text_background_enabled[ slot ],
                    slot == config::player::stats::slot_state
                        ? text_weight_t::regular : text_weight_t::semi_bold,
                    p.bg_left_pad, p.bg_right_pad );
            }
            for ( int i = 0; i < ngroups[ 1 ]; ++i ) {
                const int slot = groups[ 1 ][ i ].slot;
                const auto& p = g_pending[ slot ];
                const float pre = layout.bottom_offset == 0.f ? helper::bottom_gap : layout.bottom_offset;
                g_slot_y_offset[ slot ] = pre;
                helper::draw_text( layout, p.value.c_str( ), p.color, p.size,
                    anchor_t::bottom, p.alpha_scale,
                    config::player::stats::text_outline_enabled[ slot ]
                        ? config::player::stats::text_outline_width[ slot ] : 0.f,
                    config::player::stats::text_background_enabled[ slot ],
                    slot == config::player::stats::slot_state
                        ? text_weight_t::regular : text_weight_t::semi_bold,
                    p.bg_left_pad, p.bg_right_pad );
            }
            for ( int i = 0; i < ngroups[ 2 ]; ++i ) {
                const int slot = groups[ 2 ][ i ].slot;
                const auto& p = g_pending[ slot ];
                const float pre = layout.left_offset == 0.f ? helper::top_gap : layout.left_offset;
                g_slot_y_offset[ slot ] = pre;
                helper::draw_text( layout, p.value.c_str( ), p.color, p.size,
                    anchor_t::left, p.alpha_scale,
                    config::player::stats::text_outline_enabled[ slot ]
                        ? config::player::stats::text_outline_width[ slot ] : 0.f,
                    config::player::stats::text_background_enabled[ slot ],
                    slot == config::player::stats::slot_state
                        ? text_weight_t::regular : text_weight_t::semi_bold,
                    p.bg_left_pad, p.bg_right_pad );
            }
            for ( int i = 0; i < ngroups[ 3 ]; ++i ) {
                const int slot = groups[ 3 ][ i ].slot;
                const auto& p = g_pending[ slot ];
                const float pre = layout.right_offset == 0.f ? helper::top_gap : layout.right_offset;
                g_slot_y_offset[ slot ] = pre;
                helper::draw_text( layout, p.value.c_str( ), p.color, p.size,
                    anchor_t::right, p.alpha_scale,
                    config::player::stats::text_outline_enabled[ slot ]
                        ? config::player::stats::text_outline_width[ slot ] : 0.f,
                    config::player::stats::text_background_enabled[ slot ],
                    slot == config::player::stats::slot_state
                        ? text_weight_t::regular : text_weight_t::semi_bold,
                    p.bg_left_pad, p.bg_right_pad );
            }
        }

        void text_medium(
            layout_t& layout,
            const wchar_t* value,
            color_t color,
            anchor_t anchor,
            float alpha_scale = 1.f );

        void text_small(
            layout_t& layout,
            const wchar_t* value,
            color_t color,
            anchor_t anchor,
            float alpha_scale = 1.f );
    }
}
