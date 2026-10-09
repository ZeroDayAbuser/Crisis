#include <deps/includes.h>

namespace gui::esp::health {
            void render(
                const actor_state_t&             st,
                float                            now_s,
                layout_t&                        layout,
                bypass::overlay::point_t         head_pt,
                bool                             is_alive )
            {
                if ( !st.init ) return;

                const auto& box = layout.box;

                constexpr float bar_gap = 2.f;
                const float hp_frac = std::clamp( st.health / 100.f, 0.f, 1.f );

                // Health-bar colour: green → yellow → red gradient.
                color_t hp_col;
                if ( hp_frac > 0.5f ) {
                    const float t = ( hp_frac - 0.5f ) * 2.f;
                    hp_col = { static_cast<std::uint8_t>( ( 1.f - t ) * 255.f ), 255, 0, 255 };
                } else {
                    const float t = hp_frac * 2.f;
                    hp_col = { 255, static_cast<std::uint8_t>( t * 255.f ), 0, 255 };
                }

                // Position-aware bar renderer. Handles all 4 sides:
                //   0 = LEFT  (vertical, fill grows upward)
                //   1 = RIGHT (vertical, fill grows upward)
                //   2 = TOP   (horizontal, fill grows left→right)
                //   3 = BOTTOM(horizontal, fill grows left→right)
                // `edge_gap` is the distance from the box edge to the
                // box-facing edge of the bar — the caller passes a
                // larger value for a stacked-second bar on the same
                // side so it sits outside the first one instead of
                // overlapping it. Text-side layout offsets are bumped
                // by the OUTER edge of this bar so texts on the same
                // side always dock beyond every bar there.
                auto draw_bar = [ & ]( int position, float edge_gap, float frac,
                                       color_t fill_col, bool is_shield ) {
                    if ( position < 0 || position > 3 ) position = 0;
                    const float thickness = is_shield ? 4.f : 3.f;
                    const float total_off = edge_gap + thickness + 2.f;
                    rect_t back {}, fill {};
                    switch ( position ) {
                    case 0: {   // LEFT
                        const float x = box.x - edge_gap - thickness;
                        back = { x - 1.f, box.y - 1.f, thickness + 2.f, box.height + 2.f };
                        const float fh = box.height * frac;
                        fill = { x, box.y + box.height - fh, thickness, fh };
                        layout.left_hb_offset = ( std::max )( layout.left_hb_offset, total_off );
                        break;
                    }
                    case 1: {   // RIGHT
                        const float x = box.x + box.width + edge_gap;
                        back = { x - 1.f, box.y - 1.f, thickness + 2.f, box.height + 2.f };
                        const float fh = box.height * frac;
                        fill = { x, box.y + box.height - fh, thickness, fh };
                        layout.right_hb_offset = ( std::max )( layout.right_hb_offset, total_off );
                        break;
                    }
                    case 2: {   // TOP
                        const float y = box.y - edge_gap - thickness;
                        back = { box.x - 1.f, y - 1.f, box.width + 2.f, thickness + 2.f };
                        fill = { box.x, y, box.width * frac, thickness };
                        layout.top_offset = ( std::max )( layout.top_offset, total_off );
                        break;
                    }
                    default: {  // 3 = BOTTOM
                        const float y = box.y + box.height + edge_gap;
                        back = { box.x - 1.f, y - 1.f, box.width + 2.f, thickness + 2.f };
                        fill = { box.x, y, box.width * frac, thickness };
                        layout.bottom_offset = ( std::max )( layout.bottom_offset, total_off );
                        break;
                    }
                    }
                    bypass::overlay::filled_rectangle_async( back, { 0, 0, 0, 180 } );
                    if ( is_shield )
                        bypass::overlay::filled_rectangle_async(
                            { fill.x, fill.y, ( position <= 1 ) ? thickness : box.width,
                              ( position <= 1 ) ? box.height : thickness },
                            { 0, 0, 0, 80 } );
                    if ( fill.width > 0.f && fill.height > 0.f )
                        bypass::overlay::filled_rectangle_async( fill, fill_col );
                };

                // Compute per-bar edge gaps so a shield sharing the
                // health bar's side stacks OUTSIDE it. Health always
                // sits at the innermost slot; shield jumps out by
                // (health thickness + bar_gap) when they collide.
                const int hp_pos_cfg = config::player::stats::health_bar_position;
                const int sh_pos_cfg = config::player::stats::shield_bar_position;
                constexpr float hp_thick_ref = 3.f;
                const float hp_edge = bar_gap;
                const float sh_edge = ( sh_pos_cfg == hp_pos_cfg )
                    ? bar_gap + hp_thick_ref + bar_gap
                    : bar_gap;

                // Health bar — always drawn when the outer gate passes.
                draw_bar( hp_pos_cfg, hp_edge, hp_frac, hp_col, false );

                // Dead-marker bar segment on same side as the health bar.
                // Uses hp_edge so it sits inside the (possibly stacked)
                // health slot.
                if ( hp_frac <= 0.f && is_alive ) {
                    const int pos = hp_pos_cfg;
                    constexpr float marker_len = 3.f;
                    constexpr float thickness  = 3.f;
                    rect_t marker {};
                    switch ( pos ) {
                    case 0:  marker = { box.x - hp_edge - thickness, box.y + box.height - marker_len, thickness, marker_len }; break;
                    case 1:  marker = { box.x + box.width + hp_edge, box.y + box.height - marker_len, thickness, marker_len }; break;
                    case 2:  marker = { box.x, box.y - hp_edge - thickness, marker_len, thickness }; break;
                    default: marker = { box.x, box.y + box.height + hp_edge, marker_len, thickness }; break;
                    }
                    bypass::overlay::filled_rectangle_async( marker, { 255, 40, 40, 230 } );
                }

                // Shield bar — independent position from health, but
                // stacked outside if it collides with health's side.
                if ( st.shield > 0.f ) {
                    const float sh_frac = std::clamp( st.shield / 100.f, 0.f, 1.f );
                    draw_bar( sh_pos_cfg, sh_edge, sh_frac,
                        { 100, 180, 255, 230 }, true );
                }

                if ( config::player::stats::hit_markers
                    && box.width > 0.f
                    && !st.hitmarkers.empty( ) )
                {
                    constexpr float k_lifetime   = 1.8f;
                    constexpr float k_rise_speed = 55.f; 
                    constexpr float k_text_sz    = 12.f;
                    constexpr float k_rect_w     = 52.f;
                    constexpr float k_side_gap   = 4.f;
                    constexpr float k_lane_h     = 15.f;
                    constexpr float k_top_offset = 6.f;
                    constexpr float k_fade_at    = 0.25f;

                    const float tx      = box.x + box.width + k_side_gap;
                    const float ty_base = box.y + box.height;

                    const int hm_count = static_cast<int>( st.hitmarkers.size( ) );
                    for ( int hm_idx = 0; hm_idx < hm_count; ++hm_idx ) {
                        const auto& hm = st.hitmarkers[ hm_idx ];
                        const float age = now_s - hm.time_s;
                        if ( age < 0.f || age >= k_lifetime ) continue;

                        const float fade_start = k_lifetime * k_fade_at;
                        const float alpha = std::clamp( ( age < fade_start )
                            ? 1.f
                            : 1.f - ( age - fade_start ) / ( k_lifetime - fade_start ), 0.f, 1.f );

                        const int   slot   = hm_count - 1 - hm_idx;
                        const float y_rise = age * k_rise_speed + static_cast<float>( slot ) * k_lane_h;
                        const float ty     = ( std::max )(
                            box.y - k_top_offset, ty_base - y_rise - k_text_sz );

                        const std::wstring text = std::format( L"-{}", static_cast<int>( hm.damage + 0.5f ) );

                        const std::uint8_t a8 = static_cast<std::uint8_t>( alpha * 255.f );
                        const color_t outline { 0, 0, 0,
                            static_cast<std::uint8_t>( alpha * 217.f ) };
                        const color_t dmg_col = ( hm.zone == e_hit_zone::head )
                            ? color_t { 255, 215, 0, a8 }
                            : color_t { 255, 77, 26, a8 };

                        const auto mk_area = [&]( float dx, float dy ) -> bypass::overlay::rect_t {
                            return { tx + dx, ty + dy, k_rect_w, k_text_sz * 1.25f };
                        };

                        bypass::overlay::text_async( text.c_str( ), mk_area( -1.f,  0.f ), outline, k_text_sz, bypass::overlay::text_weight_t::bold, nullptr, bypass::overlay::text_align_t::center );
                        bypass::overlay::text_async( text.c_str( ), mk_area( +1.f,  0.f ), outline, k_text_sz, bypass::overlay::text_weight_t::bold, nullptr, bypass::overlay::text_align_t::center );
                        bypass::overlay::text_async( text.c_str( ), mk_area(  0.f, -1.f ), outline, k_text_sz, bypass::overlay::text_weight_t::bold, nullptr, bypass::overlay::text_align_t::center );
                        bypass::overlay::text_async( text.c_str( ), mk_area(  0.f, +1.f ), outline, k_text_sz, bypass::overlay::text_weight_t::bold, nullptr, bypass::overlay::text_align_t::center );
                        bypass::overlay::text_async( text.c_str( ), mk_area(  0.f,  0.f ), dmg_col, k_text_sz, bypass::overlay::text_weight_t::bold, nullptr, bypass::overlay::text_align_t::center );
                    }
                }
            }
                c_health_thread::~c_health_thread( ) { stop( ); }
                void c_health_thread::start( ) {
                    m_running.store( true, std::memory_order_release );
                    m_thread = std::thread( [ this ] { run( ); } );
                }
                void c_health_thread::stop( ) {
                    m_running.store( false, std::memory_order_release );
                    if ( m_thread.joinable( ) ) m_thread.join( );
                }
                actor_state_t c_health_thread::get_state( std::uint64_t actor ) const {
                    std::shared_lock lock( m_lock );
                    const auto it = m_actors.find( actor );
                    return ( it != m_actors.end( ) ) ? it->second : actor_state_t{};
                }
                float c_health_thread::now_s( ) const {
                    return m_now_s.load( std::memory_order_relaxed );
                }
                void c_health_thread::run( ) {
                    priority::c_gate gate;

                    while ( m_running.load( std::memory_order_acquire ) ) {
                        const auto frame_start = ch::steady_clock::now( );
                        gate.tick( );
                        const float now = ch::duration<float>(
                            frame_start.time_since_epoch( ) ).count( );
                        m_now_s.store( now, std::memory_order_relaxed );

                        const auto players = g_cache->get_players( );
                        const bool do_weapon_reads = gate.on( priority::level::medium );

                        for ( const auto& pawn : players ) {
                            if ( !m_running.load( std::memory_order_acquire ) ) return;
                            process( pawn, players, now, do_weapon_reads );
                        }

                        evict( players );

                        const auto elapsed = ch::steady_clock::now( ) - frame_start;
                        if ( elapsed < k_loop_interval )
                            std::this_thread::sleep_for( k_loop_interval - elapsed );
                    }
                }
            namespace detail {
                std::mutex s_dmg_cache_mutex;
                std::unordered_map<std::uint64_t, weapon_dmg_t> s_dmg_cache;
                std::string compact_weapon_name( std::string name ) {
                    name.erase( std::remove_if( name.begin( ), name.end( ),
                        [ ]( unsigned char c ) { return std::isspace( c ) != 0; } ), name.end( ) );
                    return name;
                }
                [[nodiscard]] healing_t healing_for_weapon( std::string name ) {
                    std::transform( name.begin( ), name.end( ), name.begin( ),
                        [ ]( unsigned char c ) { return static_cast<char>( std::tolower( c ) ); } );
                    if ( name.find( "chug jug" )   != std::string::npos ) return { 100.f, 100.f, 100.f };
                    if ( name.find( "flowberry" )  != std::string::npos ) return {   0.f, 100.f, 100.f };
                    if ( name.find( "chug splash" ) != std::string::npos ) return {  20.f,  20.f, 100.f };
                    if ( name.find( "slurpfish" )  != std::string::npos ) return {  50.f,  50.f, 100.f };
                    if ( name.find( "slurp" )      != std::string::npos ) return {  75.f,  75.f, 100.f };
                    if ( name.find( "mini" ) != std::string::npos
                        && ( name.find( "shield" ) != std::string::npos
                          || name.find( "potion" ) != std::string::npos ) )
                        return { 0.f, 25.f, 100.f };
                    if ( name.find( "shield" )     != std::string::npos ) return {  0.f,  50.f, 100.f };
                    if ( name.find( "potion" )     != std::string::npos ) return {  0.f,  25.f, 100.f };
                    if ( name.find( "bandage" )    != std::string::npos ) return { 15.f,   0.f,  75.f };
                    if ( name.find( "first aid" )  != std::string::npos
                      || name.find( "med" )        != std::string::npos ) return { 100.f,  0.f, 100.f };
                    if ( name.find( "campfire" )   != std::string::npos ) return {  50.f,  0.f, 100.f };
                    if ( name.find( "apple" )      != std::string::npos
                      || name.find( "pepper" )     != std::string::npos ) return {   5.f,  0.f, 100.f };
                    if ( name.find( "small fry" )  != std::string::npos ) return {  25.f,  0.f,  75.f };
                    if ( name.find( "flopper" )    != std::string::npos ) return {  50.f,  0.f, 100.f };
                    if ( name.find( "fish" )       != std::string::npos ) return {  25.f,  0.f,  75.f };
                    return {};
                }
                [[nodiscard]] bool is_ammo_tracked_heal( const std::string& name ) {
                    return name.find( "chug jug" )  != std::string::npos
                        || name.find( "flowberry" ) != std::string::npos;
                }
                [[nodiscard]] int ammo_charges_for_heal( const std::string& name ) {
                    if ( name.find( "flowberry" ) != std::string::npos ) return 100;
                    return 200;
                }
                [[nodiscard]] float heal_hold_threshold_s( const std::string& name ) {
                    if ( name.find( "medkit" )    != std::string::npos
                      || name.find( "first aid" ) != std::string::npos ) return  8.0f;
                    if ( name.find( "bandage" )   != std::string::npos ) return  3.2f;
                    if ( name.find( "campfire" )  != std::string::npos ) return  0.f;
                    return 1.6f;
                }
                [[nodiscard]] float get_damage(
                    engine::u_fort_weapon_item_definition* item_def,
                    float                                  dist_cm )
                {
                    if ( !item_def ) return 0.f;
                    const auto key = reinterpret_cast<std::uint64_t>( item_def );

                    weapon_dmg_t d;
                    {
                        std::lock_guard lock( s_dmg_cache_mutex );
                        const auto it = s_dmg_cache.find( key );
                        if ( it != s_dmg_cache.end( ) ) {
                            d = it->second;
                        } else {
                            auto* stats = item_def->get_weapon_stats( );
                            if ( stats ) {
                                auto cd = []( float v ) { return ( v > 0.f && v < 10000.f )     ? v : 0.f; };
                                auto cr = []( float v ) { return ( v > 0.f && v < 1'000'000.f ) ? v : 0.f; };
                                auto cm = []( float v, float def ) {
                                    return ( v > 0.5f && v <= 10.f ) ? v : def;
                                };
                                d.dmg_pb      = cd( stats->dmg_pb( ) );
                                d.dmg_mid     = cd( stats->dmg_mid( ) );
                                d.dmg_long    = cd( stats->dmg_long( ) );
                                d.dmg_max     = cd( stats->dmg_max_range( ) );
                                d.rng_pb      = cr( stats->rng_pb( ) );
                                d.rng_mid     = cr( stats->rng_mid( ) );
                                d.rng_long    = cr( stats->rng_long( ) );
                                d.rng_max     = cr( stats->rng_max( ) );
                                d.zone_crit   = cm( stats->dmg_zone_critical(  ), 2.0f );
                                d.zone_normal = cm( stats->dmg_zone_normal(    ), 1.0f );
                                d.zone_resist = cm( stats->dmg_zone_resistant( ), 1.0f );
                            }
                            s_dmg_cache.try_emplace( key, d );
                        }
                    }

                    if ( d.dmg_pb <= 0.f ) return 0.f;
                    if ( d.rng_pb  > 0.f && dist_cm <= d.rng_pb  ) return d.dmg_pb;
                    if ( d.rng_mid > 0.f && dist_cm <= d.rng_mid  ) return d.dmg_mid  > 0.f ? d.dmg_mid  : d.dmg_pb;
                    if ( d.rng_long > 0.f && dist_cm <= d.rng_long ) return d.dmg_long > 0.f ? d.dmg_long : d.dmg_mid;
                    if ( d.dmg_max  > 0.f )                          return d.dmg_max;
                    return d.dmg_pb;
                }
                [[nodiscard]] e_hit_zone classify_hit_zone(
                    const engine::f_vector&      origin,
                    const engine::f_vector&      dir,
                    const cache::c_player_pawn&  victim )
                {
                    const auto ray_d2 = [&]( const engine::f_vector& p ) -> double {
                        const auto   dv = p - origin;
                        const double t  = max( 0.0, dv.vector_scalar( dir ) );
                        const auto   cp = origin + dir * t;
                        const auto   df = p - cp;
                        return df.vector_scalar( df );
                    };

                    double     best_d2   = 1.0e300;
                    e_hit_zone best_zone = e_hit_zone::body;

                    const auto probe_bone = [&]( const engine::f_vector& p, e_hit_zone z ) {
                        const double d = ray_d2( p );
                        if ( d < best_d2 ) { best_d2 = d; best_zone = z; }
                    };

                    probe_bone( victim.m_head_location,   e_hit_zone::head );
                    probe_bone( victim.m_bounds_head_top, e_hit_zone::head );

                    probe_bone( victim.m_skeleton_positions[ 0 ].first,  e_hit_zone::body );
                    probe_bone( victim.m_skeleton_positions[ 0 ].second, e_hit_zone::body );
                    probe_bone( victim.m_bounds_pelvis,                  e_hit_zone::body );

                    probe_bone( victim.m_skeleton_positions[ 1 ].second, e_hit_zone::limb );
                    probe_bone( victim.m_skeleton_positions[ 2 ].second, e_hit_zone::limb );
                    probe_bone( victim.m_bounds_elbow_l,                 e_hit_zone::limb );
                    probe_bone( victim.m_bounds_elbow_r,                 e_hit_zone::limb );
                    probe_bone( victim.m_bounds_hand_l,                  e_hit_zone::limb );
                    probe_bone( victim.m_bounds_hand_r,                  e_hit_zone::limb );

                    probe_bone( victim.m_skeleton_positions[  8 ].second, e_hit_zone::limb );
                    probe_bone( victim.m_skeleton_positions[  9 ].second, e_hit_zone::limb );
                    probe_bone( victim.m_skeleton_positions[ 10 ].second, e_hit_zone::limb );
                    probe_bone( victim.m_skeleton_positions[ 12 ].second, e_hit_zone::limb );
                    probe_bone( victim.m_bounds_foot_l,                   e_hit_zone::limb );
                    probe_bone( victim.m_bounds_foot_r,                   e_hit_zone::limb );

                    return best_zone;
                }
                [[nodiscard]] float get_zone_multiplier(
                    engine::u_fort_weapon_item_definition* item_def,
                    e_hit_zone                             zone )
                {
                    if ( !item_def ) {
                        return zone == e_hit_zone::head ? 2.0f : 1.0f;
                    }

                    weapon_dmg_t d;
                    {
                        std::lock_guard lock( s_dmg_cache_mutex );
                        const auto it = s_dmg_cache.find(
                            reinterpret_cast<std::uint64_t>( item_def ) );
                        if ( it != s_dmg_cache.end( ) )
                            d = it->second;
                    }

                    switch ( zone ) {
                        case e_hit_zone::head: return d.zone_crit;
                        case e_hit_zone::limb: return d.zone_resist;
                        default:               return d.zone_normal;
                    }
                }
            }
                void c_health_thread::process(
                    const cache::c_player_pawn& pawn,
                    const std::vector<cache::c_player_pawn>& all_players,
                    float now,
                    bool do_weapon_reads )
                {
                    const auto actor = reinterpret_cast<std::uint64_t>( pawn.m_player );
                    if ( !actor ) return;

                    // Route reads through the SDK — last_damaged_time is a
                    // declare_member on a_actor, current_gun_fire_index / ammo_count
                    // are on a_fort_weapon. The raw addresses (`actor`,
                    // `weapon_actor`) are kept as std::uint64_t map keys and
                    // reinterpreted only long enough to call the accessors.
                    const float last_damaged =
                        reinterpret_cast< engine::a_actor* >( actor )->last_damaged_time( );
                    const auto weapon_actor =
                        reinterpret_cast<std::uint64_t>( pawn.m_weapon );

                    int fire_index = 0, ammo_count = 0;
                    if ( weapon_actor && do_weapon_reads ) {
                        auto weapon_sdk = reinterpret_cast< engine::a_fort_weapon* >( weapon_actor );
                        fire_index = weapon_sdk->current_gun_fire_index( );
                        ammo_count = weapon_sdk->ammo_count( );
                    } else if ( weapon_actor ) {
                        std::shared_lock rlock( m_lock );
                        const auto it = m_actors.find( actor );
                        if ( it != m_actors.end( ) ) {
                            fire_index = it->second.last_fire_index;
                            ammo_count = it->second.last_ammo_count;
                        }
                    }

                    actor_state_t st;
                    {
                        std::shared_lock rlock( m_lock );
                        const auto it = m_actors.find( actor );
                        if ( it != m_actors.end( ) ) st = it->second;
                    }

                    constexpr float k_hm_expire = 2.0f;
                    if ( !st.hitmarkers.empty( ) ) {
                        st.hitmarkers.erase(
                            std::remove_if( st.hitmarkers.begin( ), st.hitmarkers.end( ),
                                [&]( const hitmarker_t& hm ) {
                                    return ( now - hm.time_s ) > k_hm_expire;
                                } ),
                            st.hitmarkers.end( ) );
                    }

                    if ( !st.init ) {
                        st.init              = true;
                        st.last_damaged_time = last_damaged;
                        st.health            = 100.f;
                        st.shield            = 0.f;
                        st.weapon_actor      = weapon_actor;
                        st.last_weapon       = pawn.m_weapon_name;
                        st.last_fire_index   = fire_index;
                        st.last_ammo_count   = ammo_count;
                        st.weapon_equip_time = now;
                        st.last_weapon_fire_time = weapon_actor
                            ? reinterpret_cast< engine::a_fort_weapon* >( weapon_actor )->last_fire_time( )
                            : 0.f;
                        std::unique_lock wlock( m_lock );
                        m_actors[ actor ] = st;
                        return;
                    }

                    if ( weapon_actor != st.weapon_actor ) {
                        const bool was_ammo_tracked =
                            detail::is_ammo_tracked_heal( st.last_weapon );

                        if ( st.weapon_actor && !was_ammo_tracked ) {
                            const auto old_healing =
                                detail::healing_for_weapon( st.last_weapon );
                            const bool was_heal =
                                old_healing.health > 0.f || old_healing.shield > 0.f;

                            if ( was_heal ) {
                                const float cur_fire_time =
                                    reinterpret_cast< engine::a_fort_weapon* >( st.weapon_actor )->last_fire_time( );
                                const bool fire_advanced =
                                    cur_fire_time > st.last_weapon_fire_time + 0.001f;

                                const float hold_s    = now - st.weapon_equip_time;
                                const float threshold = detail::heal_hold_threshold_s( st.last_weapon );

                                logging::print(
                                    "[health] SWAP actor=0x%llx  item='%s'"
                                    "  fire_time_delta=%.4f  hold=%.2fs/%.2fs  C=%s",
                                    actor, st.last_weapon.c_str( ),
                                    cur_fire_time - st.last_weapon_fire_time,
                                    hold_s, threshold,
                                    fire_advanced ? "YES" : "no" );

                                if ( fire_advanced ) {
                                    const float old_hp = st.health, old_sh = st.shield;
                                    st.health = ( std::min )( old_healing.health_cap,
                                        st.health + old_healing.health );
                                    st.shield = ( std::min )( 100.f,
                                        st.shield + old_healing.shield );
                                    logging::print(
                                        "[health] HEAL(C) actor=0x%llx item='%s'"
                                        "  +%.1fhp +%.1fsh  health=%.1f->%.1f shield=%.1f->%.1f",
                                        actor, st.last_weapon.c_str( ),
                                        old_healing.health, old_healing.shield,
                                        old_hp, st.health, old_sh, st.shield );
                                } else if ( threshold > 0.f && st.weapon_equip_time >= 0.f
                                         && hold_s >= threshold ) {
                                    const float old_hp = st.health, old_sh = st.shield;
                                    st.health = ( std::min )( old_healing.health_cap,
                                        st.health + old_healing.health );
                                    st.shield = ( std::min )( 100.f,
                                        st.shield + old_healing.shield );
                                    logging::print(
                                        "[health] HEAL(B) actor=0x%llx item='%s'"
                                        "  held=%.2fs>=%.2fs  +%.1fhp +%.1fsh"
                                        "  health=%.1f->%.1f shield=%.1f->%.1f",
                                        actor, st.last_weapon.c_str( ),
                                        hold_s, threshold,
                                        old_healing.health, old_healing.shield,
                                        old_hp, st.health, old_sh, st.shield );
                                } else {
                                    logging::print(
                                        "[health] SWAP-SKIP actor=0x%llx item='%s'"
                                        "  hold=%.2fs<%.2fs  no fire  → not consumed",
                                        actor, st.last_weapon.c_str( ), hold_s, threshold );
                                }
                            }
                        }

                        st.weapon_actor    = weapon_actor;
                        st.last_weapon     = pawn.m_weapon_name;
                        st.last_fire_index = fire_index;
                        st.last_ammo_count = ammo_count;
                        st.weapon_equip_time = now;
                        st.last_weapon_fire_time = weapon_actor
                            ? reinterpret_cast< engine::a_fort_weapon* >( weapon_actor )->last_fire_time( )
                            : 0.f;
                    }
                    else if ( weapon_actor ) {
                        const bool consumed =
                            fire_index != st.last_fire_index
                            || ( ammo_count >= 0 && ammo_count < st.last_ammo_count );

                        const auto healing =
                            detail::healing_for_weapon( pawn.m_weapon_name );

                        if ( consumed && ( healing.health > 0.f || healing.shield > 0.f ) ) {
                            float hp_gain = healing.health;
                            float sh_gain = healing.shield;

                            const int ammo_delta = st.last_ammo_count - ammo_count;
                            if ( ammo_delta > 1 && st.last_ammo_count > 1 ) {
                                const float charges =
                                    static_cast<float>( detail::ammo_charges_for_heal( pawn.m_weapon_name ) );
                                const float frac = static_cast<float>( ammo_delta ) / charges;
                                hp_gain = healing.health * frac;
                                sh_gain = healing.shield * frac;
                            }

                            const float old_hp = st.health, old_sh = st.shield;
                            st.health = ( std::min )( healing.health_cap, st.health + hp_gain );
                            st.shield = ( std::min )( 100.f, st.shield + sh_gain );
                            logging::print(
                                "[health] HEAL actor=0x%llx item='%s' +%.1fhp +%.1fsh  health=%.1f->%.1f shield=%.1f->%.1f",
                                actor, pawn.m_weapon_name.c_str( ),
                                hp_gain, sh_gain, old_hp, st.health, old_sh, st.shield );
                        }
                        st.last_fire_index = fire_index;
                        st.last_ammo_count = ammo_count;
                        st.last_weapon     = pawn.m_weapon_name;
                    }

                    if ( last_damaged <= st.last_damaged_time + 0.0001f ) {
                        st.last_damaged_time = last_damaged;
                        std::unique_lock wlock( m_lock );
                        m_actors[ actor ] = st;
                        return;
                    }

                    constexpr double CONE_COS = 0.9063;
                    const auto& victim_head = pawn.m_head_location;

                    engine::u_fort_weapon_item_definition* best_item_def  = nullptr;
                    std::string      best_name;
                    float            best_dist_cm    = 0.f;
                    double           best_cos        = CONE_COS;
                    engine::f_vector best_fire_origin { };
                    engine::f_vector best_fire_dir    { };

                    const auto check = [&](
                        const engine::f_vector& origin,
                        const engine::f_vector& fired_dir,
                        const std::string&      name,
                        engine::u_fort_weapon_item_definition* item_def )
                    {
                        if ( !fired_dir.is_valid( ) ) return;
                        const auto delta = victim_head - origin;
                        const auto dist  = delta.length( );
                        if ( dist < 1.0 ) return;
                        const double dot = ( delta / dist ).vector_scalar( fired_dir );
                        if ( dot > best_cos ) {
                            best_cos         = dot;
                            best_item_def    = item_def;
                            best_name        = name;
                            best_dist_cm     = static_cast<float>( dist );
                            best_fire_origin = origin;
                            best_fire_dir    = fired_dir;
                        }
                    };

                    for ( const auto& p : all_players ) {
                        if ( reinterpret_cast<std::uint64_t>( p.m_player ) == actor ) continue;
                        if ( p.m_is_dying || p.m_is_downed ) continue;
                        check( p.m_head_location, p.m_last_fired_direction,
                               p.m_player_name, p.m_item_definition );
                    }

                    {
                        const auto local     = g_cache->m_acknowledged_pawn.load( );
                        const auto local_ptr = reinterpret_cast<std::uint64_t>( local );
                        if ( local && local_ptr != actor ) {
                            const auto fired_loc = bypass::read<engine::f_vector>(
                                local_ptr + engine::offsets::last_fired_location );
                            const auto fired_dir = bypass::read<engine::f_vector>(
                                local_ptr + engine::offsets::last_fired_direction );

                            std::string local_name = "LocalPlayer";
                            if ( const auto ps = local->player_state( ) )
                                local_name = ps->get_player_name( );

                            engine::u_fort_weapon_item_definition* local_item_def = nullptr;
                            if ( const auto wep = g_cache->m_current_weapon.load( ) )
                                local_item_def = wep->weapon_data( );

                            check( fired_loc, fired_dir, local_name, local_item_def );
                        }
                    }

                    e_hit_zone hit_zone = e_hit_zone::body;
                    if ( best_fire_dir.is_valid( ) )
                        hit_zone = detail::classify_hit_zone(
                            best_fire_origin, best_fire_dir, pawn );

                    float damage = 0.f;
                    if ( best_item_def ) {
                        damage = detail::get_damage( best_item_def, best_dist_cm )
                               * detail::get_zone_multiplier( best_item_def, hit_zone );
                    }

                    if ( damage > 0.f ) {
                        const float sh_dmg = ( std::min )( st.shield, damage );
                        st.shield -= sh_dmg;
                        st.health  = ( std::max )( 0.f, st.health - ( damage - sh_dmg ) );
                    }

                    const double angle_deg =
                        std::acos( std::clamp( best_cos, -1.0, 1.0 ) ) *
                        180.0 / std::numbers::pi;

                    const char* zone_str =
                        hit_zone == e_hit_zone::head ? "HEAD" :
                        hit_zone == e_hit_zone::limb ? "LIMB" : "BODY";

                    logging::print(
                        "[health] HIT  victim=0x%llx  attacker='%s'  dist=%.1fm  zone=%s  damage=%.1f  health=%.1f  shield=%.1f  angle=%.1f  dt=%.4f->%.4f",
                        actor,
                        best_name.empty( ) ? "unknown" : best_name.c_str( ),
                        best_dist_cm / 100.f,
                        zone_str,
                        damage, st.health, st.shield,
                        angle_deg, st.last_damaged_time, last_damaged );

                    if ( damage > 0.5f && damage < 500.f )
                        st.hitmarkers.push_back( { damage, now, hit_zone } );
                    st.last_damaged_time = last_damaged;

                    std::unique_lock wlock( m_lock );
                    m_actors[ actor ] = st;
                }
                void c_health_thread::evict( const std::vector<cache::c_player_pawn>& players ) {
                    std::unordered_set<std::uint64_t> active;
                    active.reserve( players.size( ) );
                    for ( const auto& p : players )
                        active.insert( reinterpret_cast<std::uint64_t>( p.m_player ) );

                    std::unique_lock lock( m_lock );
                    for ( auto it = m_actors.begin( ); it != m_actors.end( ); )
                        it = active.count( it->first ) ? std::next( it ) : m_actors.erase( it );
                }
}
