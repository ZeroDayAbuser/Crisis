#include <deps/includes.h>

namespace gui::esp {
        void skeleton(
            const std::array<std::pair<engine::f_vector, engine::f_vector>, 14>& pairs,
            color_t color,
            float thickness,
            bool outline
        ) {
            struct projected_bone_t {
                engine::f_vector world{};
                std::optional<engine::f_vector2d> screen{};
            };
            std::array<projected_bone_t, 15> projected{};
            std::size_t projected_count = 0;
            std::array<bypass::overlay::skeleton_segment_t, 14> segments{};
            std::uint32_t segment_count = 0;

            const auto project = [&]( const engine::f_vector& point )
                -> const std::optional<engine::f_vector2d>& {
                for ( std::size_t i = 0; i < projected_count; ++i ) {
                    const auto& cached = projected[ i ].world;
                    if ( cached.x == point.x && cached.y == point.y && cached.z == point.z )
                        return projected[ i ].screen;
                }

                auto& entry = projected[ projected_count++ ];
                entry.world = point;
                entry.screen = world_to_screen( point );
                return entry.screen;
            };

            for ( const auto& [ from, to ] : pairs ) {
                const auto& s0 = project( from );
                const auto& s1 = project( to );
                if ( !s0 || !s1 )
                    continue;

                const bypass::overlay::point_t screen_from{
                    static_cast< float >( s0->x ), static_cast< float >( s0->y ) };
                const bypass::overlay::point_t screen_to{
                    static_cast< float >( s1->x ), static_cast< float >( s1->y ) };

                segments[ segment_count++ ] = { screen_from, screen_to };
            }

            if ( segment_count )
                bypass::overlay::skeleton_async(
                    segments.data( ), segment_count, color, thickness, outline );
        }
        // Catmull-Rom smoothed skeleton (2026-08-26). Chain topology
        // is baked from cache::actors.h::skeleton_pairs order:
        //   [0]  spine  → neck
        //   [1]  spine  → uarm_r
        //   [2]  spine  → uarm_l
        //   [3]  uarm_l → larm_l
        //   [4]  larm_l → hand_l
        //   [5]  uarm_r → larm_r
        //   [6]  larm_r → hand_r
        //   [7]  spine  → pelvis
        //   [8]  pelvis → thigh_r
        //   [9]  pelvis → thigh_l
        //   [10] thigh_r → calf_r
        //   [11] calf_r  → foot_r
        //   [12] thigh_l → calf_l
        //   [13] calf_l  → foot_l
        //
        // Five chains, each walking a bone strand end-to-end:
        //   head/spine → neck, spine, pelvis                     (3 joints)
        //   right arm  → spine, uarm_r, larm_r, hand_r           (4 joints)
        //   left arm   → spine, uarm_l, larm_l, hand_l           (4 joints)
        //   right leg  → pelvis, thigh_r, calf_r, foot_r         (4 joints)
        //   left leg   → pelvis, thigh_l, calf_l, foot_l         (4 joints)
        // Total: 5 chains, 3 + 4×4 = 19 joint entries (with spine
        // and pelvis shared, ~11 unique bones — same set the straight
        // skeleton() call above projects).
        //
        // Rendering: project each joint once (world→screen), then walk
        // every consecutive [a,b] joint pair in the chain and evaluate
        // a Catmull-Rom spline in SCREEN space. Screen-space smoothing
        // keeps curvature stable regardless of the camera angle and
        // avoids re-projecting sub-samples. 12 sub-segments per
        // joint-pair — enough to look smooth at 4K without a noticeable
        // batch-size hit (14 chain segments × 12 = 168 lines per
        // skeleton, vs. 14 for straight).
        void skeleton_curved(
            const std::array<std::pair<engine::f_vector, engine::f_vector>, 14>& pairs,
            color_t color,
            float thickness,
            bool outline
        ) {
            struct joint_t {
                engine::f_vector world {};
                std::optional<engine::f_vector2d> screen {};
            };
            std::array<joint_t, 15> projected {};
            std::size_t projected_count = 0;

            const auto project = [ & ]( const engine::f_vector& p )
                -> const std::optional<engine::f_vector2d>& {
                for ( std::size_t i = 0; i < projected_count; ++i ) {
                    const auto& c = projected[ i ].world;
                    if ( c.x == p.x && c.y == p.y && c.z == p.z )
                        return projected[ i ].screen;
                }
                auto& entry = projected[ projected_count++ ];
                entry.world  = p;
                entry.screen = world_to_screen( p );
                return entry.screen;
            };

            // Chain references — { pair_index, endpoint } where
            // endpoint 0 = pair.first, 1 = pair.second. The fifth slot
            // in each row is padding for the head chain (3 joints, 2
            // segments) — its 4th entry is ignored via chain_lens.
            struct chain_ref_t { std::uint8_t pair; std::uint8_t end; };
            static constexpr std::array<std::array<chain_ref_t, 4>, 5> chains {{
                //  head/spine  → neck (0.s) → spine (0.f) → pelvis (7.s)
                {{ { 0, 1 }, { 0, 0 }, { 7, 1 }, { 0, 0 } }},
                //  right arm   → spine (1.f) → uarm_r (1.s) → larm_r (5.s) → hand_r (6.s)
                {{ { 1, 0 }, { 1, 1 }, { 5, 1 }, { 6, 1 } }},
                //  left arm    → spine (2.f) → uarm_l (2.s) → larm_l (3.s) → hand_l (4.s)
                {{ { 2, 0 }, { 2, 1 }, { 3, 1 }, { 4, 1 } }},
                //  right leg   → pelvis (8.f) → thigh_r (8.s) → calf_r (10.s) → foot_r (11.s)
                {{ { 8, 0 }, { 8, 1 }, { 10, 1 }, { 11, 1 } }},
                //  left leg    → pelvis (9.f) → thigh_l (9.s) → calf_l (12.s) → foot_l (13.s)
                {{ { 9, 0 }, { 9, 1 }, { 12, 1 }, { 13, 1 } }},
            }};
            static constexpr std::array<std::uint8_t, 5> chain_lens {{ 3, 4, 4, 4, 4 }};

            auto get_world = [ & ]( const std::array<chain_ref_t, 4>& c, int j ) -> const engine::f_vector& {
                const auto& p = pairs[ c[ j ].pair ];
                return c[ j ].end == 0 ? p.first : p.second;
            };

            // 12 sub-segments per chain-segment. 14 chain-segments
            // total (2 + 3 + 3 + 3 + 3) → 168 lines max.
            constexpr int kSamples = 12;

            // Cardinal-spline tension (0 = piecewise-linear, 0.5 =
            // uniform Catmull-Rom, 1.0 = maximum bulge). Driven by
            // the menu Smoothness slider so users can dial the curve
            // strength without touching code. Clamp defensively — a
            // stale/corrupted config value shouldn't crash the walk.
            float t_slider = config::player::skeleton::curve_smoothness;
            if ( !std::isfinite( t_slider ) ) t_slider = 0.5f;
            if ( t_slider < 0.0f ) t_slider = 0.0f;
            if ( t_slider > 1.0f ) t_slider = 1.0f;
            const double tension = static_cast< double >( t_slider );
            std::array<bypass::overlay::skeleton_segment_t, 14 * kSamples> segments {};
            std::uint32_t seg_count = 0;

            for ( std::size_t c = 0; c < chains.size( ); ++c ) {
                const int len = chain_lens[ c ];

                std::optional<engine::f_vector2d> sp[ 4 ] {};
                for ( int j = 0; j < len; ++j )
                    sp[ j ] = project( get_world( chains[ c ], j ) );

                // Walk each consecutive joint pair in the chain.
                for ( int j = 0; j + 1 < len; ++j ) {
                    if ( !sp[ j ] || !sp[ j + 1 ] )
                        continue;

                    // Catmull-Rom needs 4 control points. At the chain
                    // ENDS we mirror the boundary joint (duplicate it)
                    // so the spline starts / stops exactly at the bone
                    // endpoint with zero tangent.
                    const engine::f_vector2d p0 = ( j - 1 >= 0 && sp[ j - 1 ] ) ? *sp[ j - 1 ] : *sp[ j ];
                    const engine::f_vector2d p1 = *sp[ j ];
                    const engine::f_vector2d p2 = *sp[ j + 1 ];
                    const engine::f_vector2d p3 = ( j + 2 <  len && sp[ j + 2 ] ) ? *sp[ j + 2 ] : *sp[ j + 1 ];

                    bypass::overlay::point_t prev {
                        static_cast< float >( p1.x ),
                        static_cast< float >( p1.y ) };

                    for ( int s = 1; s <= kSamples; ++s ) {
                        const double t  = static_cast< double >( s ) / kSamples;
                        const double t2 = t * t;
                        const double t3 = t2 * t;
                        // Cardinal-spline basis with tension `c`.
                        // Interpolates through P1 and P2 exactly (so
                        // adjacent chain-segments still meet). c=0 →
                        // straight lines (P0/P3 have no effect);
                        // c=0.5 → standard Catmull-Rom; c=1.0 →
                        // heavier bulge. `tension` is driven by the
                        // Smoothness slider above.
                        const double c  = tension;
                        const double a0 = -c        * t  + ( 2.0 * c )         * t2 - c              * t3;
                        const double a1 = 1.0             + ( c - 3.0 )        * t2 + ( 2.0 - c )    * t3;
                        const double a2 =  c        * t  + ( 3.0 - 2.0 * c )   * t2 + ( c - 2.0 )    * t3;
                        const double a3 =                  ( -c )              * t2 + c              * t3;

                        const double x = a0 * p0.x + a1 * p1.x + a2 * p2.x + a3 * p3.x;
                        const double y = a0 * p0.y + a1 * p1.y + a2 * p2.y + a3 * p3.y;

                        bypass::overlay::point_t cur {
                            static_cast< float >( x ),
                            static_cast< float >( y ) };

                        segments[ seg_count++ ] = { prev, cur };
                        prev = cur;
                    }
                }
            }

            if ( seg_count )
                bypass::overlay::skeleton_async(
                    segments.data( ), seg_count, color, thickness, outline );
        }

        void skeleton_cpu(
            const std::array<std::pair<engine::f_vector, engine::f_vector>, 14>& pairs,
            color_t color,
            float thickness,
            bool outline
        ) {
            struct projected_bone_t {
                engine::f_vector world {};
                std::optional<engine::f_vector2d> screen {};
            };
            std::array<projected_bone_t, 15> projected {};
            std::size_t projected_count = 0;

            const auto project = [ & ]( const engine::f_vector& point )
                -> const std::optional<engine::f_vector2d>& {
                for ( std::size_t i = 0; i < projected_count; ++i ) {
                    const auto& cached = projected[ i ].world;
                    if ( cached.x == point.x && cached.y == point.y && cached.z == point.z )
                        return projected[ i ].screen;
                }
                auto& entry = projected[ projected_count++ ];
                entry.world = point;
                entry.screen = world_to_screen( point );
                return entry.screen;
            };

            const color_t outline_col { 0, 0, 0, color.a };
            const float outline_th = thickness + 1.5f;

            for ( const auto& [ from, to ] : pairs ) {
                const auto& s0 = project( from );
                const auto& s1 = project( to );
                if ( !s0 || !s1 ) continue;
                const bypass::overlay::point_t p0 {
                    static_cast<float>( s0->x ), static_cast<float>( s0->y ) };
                const bypass::overlay::point_t p1 {
                    static_cast<float>( s1->x ), static_cast<float>( s1->y ) };
                if ( outline )
                    bypass::overlay::line_async( p0, p1, outline_col, outline_th );
                bypass::overlay::line_async( p0, p1, color, thickness );
            }
        }

        void skeleton_curved_cpu(
            const std::array<std::pair<engine::f_vector, engine::f_vector>, 14>& pairs,
            color_t color,
            float thickness,
            bool outline
        ) {
            struct joint_t {
                engine::f_vector world {};
                std::optional<engine::f_vector2d> screen {};
            };
            std::array<joint_t, 15> projected {};
            std::size_t projected_count = 0;

            const auto project = [ & ]( const engine::f_vector& p )
                -> const std::optional<engine::f_vector2d>& {
                for ( std::size_t i = 0; i < projected_count; ++i ) {
                    const auto& c = projected[ i ].world;
                    if ( c.x == p.x && c.y == p.y && c.z == p.z )
                        return projected[ i ].screen;
                }
                auto& entry = projected[ projected_count++ ];
                entry.world  = p;
                entry.screen = world_to_screen( p );
                return entry.screen;
            };

            struct chain_ref_t { std::uint8_t pair; std::uint8_t end; };
            static constexpr std::array<std::array<chain_ref_t, 4>, 5> chains {{
                {{ { 0, 1 }, { 0, 0 }, { 7, 1 }, { 0, 0 } }},
                {{ { 1, 0 }, { 1, 1 }, { 5, 1 }, { 6, 1 } }},
                {{ { 2, 0 }, { 2, 1 }, { 3, 1 }, { 4, 1 } }},
                {{ { 8, 0 }, { 8, 1 }, { 10, 1 }, { 11, 1 } }},
                {{ { 9, 0 }, { 9, 1 }, { 12, 1 }, { 13, 1 } }},
            }};
            static constexpr std::array<std::uint8_t, 5> chain_lens {{ 3, 4, 4, 4, 4 }};

            auto get_world = [ & ]( const std::array<chain_ref_t, 4>& c, int j ) -> const engine::f_vector& {
                const auto& p = pairs[ c[ j ].pair ];
                return c[ j ].end == 0 ? p.first : p.second;
            };

            constexpr int kSamples = 12;

            float t_slider = config::player::skeleton::curve_smoothness;
            if ( !std::isfinite( t_slider ) ) t_slider = 0.5f;
            if ( t_slider < 0.0f ) t_slider = 0.0f;
            if ( t_slider > 1.0f ) t_slider = 1.0f;
            const double tension = static_cast<double>( t_slider );

            const color_t outline_col { 0, 0, 0, color.a };
            const float outline_th = thickness + 1.5f;

            for ( std::size_t c = 0; c < chains.size( ); ++c ) {
                const int len = chain_lens[ c ];

                std::optional<engine::f_vector2d> sp[ 4 ] {};
                for ( int j = 0; j < len; ++j )
                    sp[ j ] = project( get_world( chains[ c ], j ) );

                for ( int j = 0; j + 1 < len; ++j ) {
                    if ( !sp[ j ] || !sp[ j + 1 ] ) continue;

                    const engine::f_vector2d p0 = ( j - 1 >= 0 && sp[ j - 1 ] ) ? *sp[ j - 1 ] : *sp[ j ];
                    const engine::f_vector2d p1 = *sp[ j ];
                    const engine::f_vector2d p2 = *sp[ j + 1 ];
                    const engine::f_vector2d p3 = ( j + 2 < len && sp[ j + 2 ] ) ? *sp[ j + 2 ] : *sp[ j + 1 ];

                    bypass::overlay::point_t prev {
                        static_cast<float>( p1.x ), static_cast<float>( p1.y ) };

                    for ( int s = 1; s <= kSamples; ++s ) {
                        const double t = static_cast<double>( s ) / kSamples;
                        const double t2 = t * t;
                        const double t3 = t2 * t;
                        const double cc = tension;
                        const double a0 = -cc        * t  + ( 2.0 * cc )         * t2 - cc              * t3;
                        const double a1 = 1.0             + ( cc - 3.0 )         * t2 + ( 2.0 - cc )    * t3;
                        const double a2 =  cc        * t  + ( 3.0 - 2.0 * cc )   * t2 + ( cc - 2.0 )    * t3;
                        const double a3 =                  ( -cc )               * t2 + cc              * t3;
                        const double x = a0 * p0.x + a1 * p1.x + a2 * p2.x + a3 * p3.x;
                        const double y = a0 * p0.y + a1 * p1.y + a2 * p2.y + a3 * p3.y;
                        bypass::overlay::point_t cur {
                            static_cast<float>( x ), static_cast<float>( y ) };
                        if ( outline )
                            bypass::overlay::line_async( prev, cur, outline_col, outline_th );
                        bypass::overlay::line_async( prev, cur, color, thickness );
                        prev = cur;
                    }
                }
            }
        }

        void fingers(
            const std::array<std::pair<engine::f_vector, engine::f_vector>, 38>& pairs,
            color_t color,
            float thickness,
            bool outline
        ) {
            struct projected_bone_t {
                engine::f_vector world{};
                std::optional<engine::f_vector2d> screen{};
            };
            std::array<projected_bone_t, 42> projected{};
            std::size_t projected_count = 0;
            std::array<bypass::overlay::skeleton_segment_t, 38> segments{};
            std::uint32_t segment_count = 0;

            const auto project = [&]( const engine::f_vector& point )
                -> const std::optional<engine::f_vector2d>& {
                for ( std::size_t i = 0; i < projected_count; ++i ) {
                    const auto& cached = projected[ i ].world;
                    if ( cached.x == point.x && cached.y == point.y && cached.z == point.z )
                        return projected[ i ].screen;
                }

                auto& entry = projected[ projected_count++ ];
                entry.world = point;
                entry.screen = world_to_screen( point );
                return entry.screen;
            };

            for ( const auto& [ from, to ] : pairs ) {
                const auto& s0 = project( from );
                const auto& s1 = project( to );
                if ( !s0 || !s1 )
                    continue;

                const bypass::overlay::point_t screen_from{
                    static_cast< float >( s0->x ), static_cast< float >( s0->y ) };
                const bypass::overlay::point_t screen_to{
                    static_cast< float >( s1->x ), static_cast< float >( s1->y ) };

                segments[ segment_count++ ] = { screen_from, screen_to };
            }

            if ( segment_count )
                bypass::overlay::skeleton_async(
                    segments.data( ), segment_count, color, thickness, outline );
        }
}
