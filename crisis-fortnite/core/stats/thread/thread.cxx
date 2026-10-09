#include <deps/includes.h>

namespace player_stats {
        c_player_stats_thread::~c_player_stats_thread( ) { stop( ); }
        void c_player_stats_thread::start( ) {
            m_running.store( true, std::memory_order_release );
            m_thread = std::thread( [ this ] { run( ); } );
        }
        void c_player_stats_thread::stop( ) {
            m_running.store( false, std::memory_order_release );
            if ( m_thread.joinable( ) )
                m_thread.join( );
        }
        void c_player_stats_thread::update_player_list( const std::vector<cache::c_player_pawn>& players ) {
            const auto now = std::chrono::steady_clock::now( );
            if ( now - m_last_snapshot_update < loop_period )
                return;
            m_last_snapshot_update = now;

            std::lock_guard lock( m_snapshot_mutex );
            m_snapshot = players;
        }
        void c_player_stats_thread::push_icon_request( icon_resolve_request_t req ) {
            std::lock_guard lock( m_icon_queue_mutex );
            m_icon_queue.push_back( std::move( req ) );
        }
        void c_player_stats_thread::run( ) {
            std::unordered_map<std::uint64_t, player_stats_cache_t> cached_entries;
            std::unordered_map<std::uint64_t, std::chrono::steady_clock::time_point> last_brief_update;
            std::unordered_map<std::uint64_t, std::chrono::steady_clock::time_point> last_weapon_update;

            while ( m_running.load( std::memory_order_acquire ) ) {
                const auto now = std::chrono::steady_clock::now( );
                std::vector<cache::c_player_pawn> players;
                {
                    std::lock_guard lock( m_snapshot_mutex );
                    players = m_snapshot;
                }

                std::vector<icon_resolve_request_t> icon_reqs;
                {
                    std::lock_guard lock( m_icon_queue_mutex );
                    icon_reqs.swap( m_icon_queue );
                }

                for ( const auto& req : icon_reqs )
                    resolve_icon( req );

                std::unordered_set<std::uint64_t> active_keys;
                active_keys.reserve( players.size( ) );

                for ( const auto& pawn : players ) {
                    if ( !m_running.load( std::memory_order_acquire ) )
                        return;

                    const auto player_key = reinterpret_cast<std::uint64_t>( pawn.m_mesh );
                    active_keys.insert( player_key );

                    auto& entry = cached_entries[ player_key ];
                    if ( !pawn.m_player_name.empty( ) &&
                        now - last_brief_update[ player_key ] >= brief_period ) {
                        if ( const auto brief = g_api->brief( pawn.m_player_name ) ) {
                            entry.has_brief         = true;
                            entry.kills             = brief->kills;
                            entry.deaths            = brief->deaths;
                            entry.win_rate          = brief->win_rate;
                            entry.battle_pass_level = brief->battle_pass_level;

                            const auto kd = brief->deaths
                                ? static_cast< double >( brief->kills ) / static_cast< double >( brief->deaths )
                                : static_cast< double >( brief->kills );

                            static constexpr bypass::overlay::color_t tier_colors[ ] = {
                                { 36, 153, 255, 255 },
                                { 162, 183, 210, 255 },
                                { 255, 103, 36, 255 },
                                { 255, 36, 43, 255 }
                            };
                            static constexpr const wchar_t* tier_names[ ] = { L"Dogshit", L"Average", L"Sweat", L"Pro" };
                            const auto tier = kd < 0.8 ? 0 : kd < 1.8 ? 1 : kd < 3.0 ? 2 : 3;

                            auto stat_text = std::format( L"{:.2f}kd", kd );
                            if ( config::player::stats::winrate )
                                stat_text += std::format( L" {:.0f}%win", brief->win_rate );
                            entry.brief_text = std::format( L"{} {}", tier_names[ tier ], stat_text );
                            entry.brief_color = tier_colors[ tier ];

                            entry.battle_pass_text.clear( );
                            if ( brief->battle_pass_level ) {
                                static constexpr bypass::overlay::color_t level_colors[ ] = {
                                    { 83, 144, 217, 255 },
                                    { 105, 189,  69, 255 },
                                    { 80, 200, 120, 255 },
                                    { 226, 146, 226, 255 }
                                };
                                static constexpr const wchar_t* level_names[ ] = {
                                    L"Locked B. pass", L"Unlocked B. pass",
                                    L"Grinding B. pass", L"Completed B. pass"
                                };
                                const auto level = brief->battle_pass_level;
                                const auto level_tier = level < 25 ? 0
                                    : level < 50 ? 1
                                    : level < 75 ? 2 : 3;
                                entry.battle_pass_text = std::format( L"{} ({})", level_names[ level_tier ], level );
                                entry.battle_pass_color = level_colors[ level_tier ];
                            }
                        }
                        last_brief_update[ player_key ] = now;
                    }

                    const bool weapon_changed = entry.weapon_name != pawn.m_weapon_name;
                    if ( !pawn.m_weapon_name.empty( ) &&
                        ( weapon_changed || now - last_weapon_update[ player_key ] >= weapon_period ) ) {
                        entry.weapon_name        = pawn.m_weapon_name;
                        entry.weapon_magazine    = g_api->weapon_magazine_size( pawn.m_weapon_name ).value_or( 0 );
                        entry.has_weapon_damage  = false;
                        entry.weapon_damage      = 0.0;
                        if ( const auto dmg = g_api->weapon_damage( pawn.m_weapon_name ) ) {
                            entry.has_weapon_damage = true;
                            entry.weapon_damage     = *dmg;
                            entry.weapon_damage_text = std::format( L"{:.0f} dmg", *dmg );
                        }
                        last_weapon_update[ player_key ] = now;
                    }
                    else if ( pawn.m_weapon_name.empty( ) ) {
                        entry.weapon_name.clear( );
                        entry.weapon_magazine = 0;
                        entry.has_weapon_damage = false;
                        entry.weapon_damage = 0.0;
                        entry.weapon_damage_text.clear( );
                    }

                    publish( player_key, std::make_shared<player_stats_cache_t>( entry ) );
                }

                prune_locked( active_keys );
                prune_inventories( active_keys );
                prune_local( cached_entries, active_keys );
                prune_local( last_brief_update, active_keys );
                prune_local( last_weapon_update, active_keys );

                std::this_thread::sleep_for( loop_period );
            }
        }
        void c_player_stats_thread::publish( std::uint64_t player_key, std::shared_ptr<player_stats_cache_t> entry ) {
            std::lock_guard lock( g_player_cache_map_mutex );
            auto [ it, inserted ] = g_player_cache.try_emplace( player_key );
            it->second.store( std::move( entry ), std::memory_order_release );
        }
        void c_player_stats_thread::prune_locked( const std::unordered_set<std::uint64_t>& active_keys ) {
            std::lock_guard lock( g_player_cache_map_mutex );
            for ( auto it = g_player_cache.begin( ); it != g_player_cache.end( ); )
                it = active_keys.count( it->first ) ? std::next( it ) : g_player_cache.erase( it );
        }
}
