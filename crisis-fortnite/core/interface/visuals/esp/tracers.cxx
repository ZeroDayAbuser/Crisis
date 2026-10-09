#include <deps/includes.h>

namespace gui::esp::tracers {

    struct sample_t {
        engine::f_vector pos {};
        ULONGLONG        stamp = 0;
    };

    static constexpr std::size_t kMaxSamplesPerPlayer = 96;

    struct trail_t {
        std::array< sample_t, kMaxSamplesPerPlayer > buf {};
        std::size_t head = 0;
        std::size_t count = 0;
        ULONGLONG   last_stamp = 0;
    };

    static std::unordered_map< std::uint64_t, trail_t > s_trails;
    static ULONGLONG s_last_prune = 0;

    void sample( std::uint64_t player_key, const engine::f_vector& world_pos ) {
        if ( !config::player::tracers::enabled )
            return;
        const ULONGLONG now = ::GetTickCount64( );

        const auto sample_ms = static_cast< ULONGLONG >(
            ( std::clamp )( config::player::tracers::sample_interval_ms, 8.0f, 500.0f ) );

        auto& trail = s_trails[ player_key ];
        if ( trail.last_stamp != 0 && now - trail.last_stamp < sample_ms )
            return;
        trail.last_stamp = now;

        trail.buf[ trail.head ] = { world_pos, now };
        trail.head = ( trail.head + 1 ) % kMaxSamplesPerPlayer;
        if ( trail.count < kMaxSamplesPerPlayer )
            ++trail.count;
    }

    void render_player_trail( std::uint64_t player_key, color_t color ) {
        if ( !config::player::tracers::enabled )
            return;
        auto it = s_trails.find( player_key );
        if ( it == s_trails.end( ) ) return;
        const auto& trail = it->second;
        if ( trail.count < 2 ) return;

        const ULONGLONG now = ::GetTickCount64( );
        const auto life_ms = static_cast< ULONGLONG >(
            ( std::max )( config::player::tracers::fade_ms, 100.0f ) );

        struct proj_t {
            float x = 0.0f, y = 0.0f;
            float alpha = 0.0f;
            bool  valid = false;
        };
        std::array< proj_t, kMaxSamplesPerPlayer > pts {};

        const std::size_t n = trail.count;
        for ( std::size_t i = 0; i < n; ++i ) {
            const std::size_t idx = ( trail.head + kMaxSamplesPerPlayer - n + i ) % kMaxSamplesPerPlayer;
            const auto& s = trail.buf[ idx ];
            const ULONGLONG age = now - s.stamp;
            if ( age > life_ms ) continue;

            const auto scr = engine::world_to_screen( s.pos );
            if ( !scr || !std::isfinite( scr->x ) || !std::isfinite( scr->y ) )
                continue;

            const float u = 1.0f - static_cast< float >( age ) / static_cast< float >( life_ms );
            const float alpha = std::pow( ( std::clamp )( u, 0.0f, 1.0f ), 3.0f );

            pts[ i ] = { static_cast< float >( scr->x ),
                         static_cast< float >( scr->y ),
                         alpha, true };
        }

        const float thickness = ( std::max )( config::player::tracers::thickness, 0.5f );
        const bool  outline_on = config::player::tracers::outline;

        for ( std::size_t i = 0; i + 1 < n; ++i ) {
            const auto& a = pts[ i ];
            const auto& b = pts[ i + 1 ];
            if ( !a.valid || !b.valid ) continue;
            const float alpha = ( a.alpha + b.alpha ) * 0.5f;
            if ( alpha <= 0.002f ) continue;

            const std::uint8_t A = static_cast< std::uint8_t >(
                ( std::clamp )( alpha * 255.0f, 0.0f, 255.0f ) );
            const std::uint8_t OA = static_cast< std::uint8_t >(
                ( std::clamp )( alpha * 200.0f, 0.0f, 255.0f ) );

            const bypass::overlay::point_t p0 { a.x, a.y };
            const bypass::overlay::point_t p1 { b.x, b.y };

            if ( outline_on ) {
                bypass::overlay::line_async(
                    p0, p1,
                    bypass::overlay::color_t { 0, 0, 0, OA },
                    thickness + 2.0f );
            }
            bypass::overlay::line_async(
                p0, p1,
                bypass::overlay::color_t { color.r, color.g, color.b, A },
                thickness );
        }
    }

    void tick( ) {
        const ULONGLONG now = ::GetTickCount64( );
        if ( now - s_last_prune < 500 ) return;
        s_last_prune = now;
        const auto life_ms = static_cast< ULONGLONG >(
            ( std::max )( config::player::tracers::fade_ms, 100.0f ) );
        for ( auto it = s_trails.begin( ); it != s_trails.end( ); ) {
            if ( it->second.count == 0 ||
                 now - it->second.last_stamp > life_ms + 2000 ) {
                it = s_trails.erase( it );
            }
            else {
                ++it;
            }
        }
    }
}

namespace tracer {

    void render( const std::vector< cache::c_bullet_tracer >& tracers ) {
        const auto col_accent = menu::current_accent( );
        const auto beam_col = config::bullet_tracers::use_accent
            ? bypass::overlay::color_t { col_accent.r, col_accent.g, col_accent.b, 255 }
            : bypass::overlay::color_t { 255, 220, 90, 255 };

        bool has_muzzle = false;
        bypass::overlay::point_t muzzle_p {};
        if ( config::bullet_tracers::muzzle_line ) {
            const auto weapon = g_cache->m_current_weapon.load( std::memory_order_acquire );
            if ( weapon ) {
                const auto muzzle_world = weapon->get_muzzle_location( 0 );
                const auto muzzle_screen = engine::world_to_screen( muzzle_world );
                if ( muzzle_screen &&
                     std::isfinite( muzzle_screen->x ) && std::isfinite( muzzle_screen->y ) ) {
                    muzzle_p  = { static_cast< float >( muzzle_screen->x ),
                                  static_cast< float >( muzzle_screen->y ) };
                    has_muzzle = true;
                }
            }
        }

        if ( !config::bullet_tracers::enabled )
            return;

        const auto outline_col = bypass::overlay::color_t { 0, 0, 0, 220 };

        for ( const auto& t : tracers ) {
            if ( t.m_is_enemy ) continue;
            engine::f_vector head_world = t.m_start_location;
            if ( config::bullet_tracers::travel_anim ) {
                const float travel = ( std::clamp )( t.m_travel_t, 0.0f, 1.0f );
                head_world.x = t.m_start_location.x + ( t.m_end_location.x - t.m_start_location.x ) * travel;
                head_world.y = t.m_start_location.y + ( t.m_end_location.y - t.m_start_location.y ) * travel;
                head_world.z = t.m_start_location.z + ( t.m_end_location.z - t.m_start_location.z ) * travel;
            }
            else {
                head_world = t.m_end_location;
            }

            const auto s0 = engine::world_to_screen( t.m_start_location );
            const auto s1 = engine::world_to_screen( head_world );
            if ( !s0 || !s1 ) continue;
            if ( !std::isfinite( s0->x ) || !std::isfinite( s0->y ) ) continue;
            if ( !std::isfinite( s1->x ) || !std::isfinite( s1->y ) ) continue;

            const bypass::overlay::point_t p0 = has_muzzle
                ? muzzle_p
                : bypass::overlay::point_t { static_cast< float >( s0->x ), static_cast< float >( s0->y ) };
            const bypass::overlay::point_t p1 {
                static_cast< float >( s1->x ), static_cast< float >( s1->y ) };

            const float alpha = ( std::clamp )( t.m_alpha, 0.0f, 1.0f );
            const std::uint8_t A  = static_cast< std::uint8_t >( alpha * 255.0f );
            const std::uint8_t OA = static_cast< std::uint8_t >( alpha * 220.0f );
            const float thickness = ( std::max )( config::bullet_tracers::thickness, 0.5f );

            if ( config::bullet_tracers::outline ) {
                bypass::overlay::line_async(
                    p0, p1,
                    bypass::overlay::color_t { outline_col.r, outline_col.g, outline_col.b, OA },
                    thickness + 2.0f );
            }
            bypass::overlay::line_async(
                p0, p1,
                bypass::overlay::color_t { beam_col.r, beam_col.g, beam_col.b, A },
                thickness );
        }
    }
}
