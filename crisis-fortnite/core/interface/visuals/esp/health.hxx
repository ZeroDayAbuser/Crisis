#pragma once

// ── Per-actor hit-detection cache ──────────────────────────────────────────────
//
// Logic runs on a dedicated background thread (c_health_thread, ~120 Hz) so the
// render thread stays unblocked.
//
// Damage is read directly from FortBaseWeaponStats via
//   attacker->item_def->get_weapon_stats()->damage_zone_normal()
// eliminating network round-trips and weapon-name lookup failures.
//
// Healing is still detected via weapon-name keyword matching (health items don't
// have weapon stats rows).
//
// Thread-safety
//   Background thread: exclusive write lock (unique_lock<shared_mutex>).
//   Render thread:     shared read lock (shared_lock<shared_mutex>) in get_state().

namespace gui {
    namespace esp {
        namespace health {
            enum class e_hit_zone : std::uint8_t {
                head,
                body,
                limb,
            };

            struct hitmarker_t {
                float      damage = 0.f;
                float      time_s = -99.f;
                e_hit_zone zone   = e_hit_zone::body;
            };

            struct actor_state_t {
                float         last_damaged_time    = 0.f;
                float         health               = 100.f;
                float         shield               = 0.f;
                std::string   last_weapon;
                std::uint64_t weapon_actor         = 0;
                int           last_fire_index      = 0;
                int           last_ammo_count      = 0;
                bool          init                 = false;
                std::vector<hitmarker_t> hitmarkers;

                float weapon_equip_time      = -1.f;
                float last_weapon_fire_time  = 0.f;
            };

            namespace detail {


                struct healing_t {
                    float health     = 0.f;
                    float shield     = 0.f;
                    float health_cap = 100.f;
                };





                struct weapon_dmg_t {
                    float dmg_pb   = 0.f;
                    float dmg_mid  = 0.f;
                    float dmg_long = 0.f;
                    float dmg_max  = 0.f;
                    float rng_pb   = 0.f;
                    float rng_mid  = 0.f;
                    float rng_long = 0.f;
                    float rng_max  = 0.f;
                    float zone_crit   = 2.0f;
                    float zone_normal = 1.0f;
                    float zone_resist = 1.0f;
                };

                extern std::mutex s_dmg_cache_mutex;
                extern std::unordered_map< std::uint64_t, weapon_dmg_t >
                    s_dmg_cache;



                std::string compact_weapon_name( std::string name );
                [[nodiscard]] healing_t healing_for_weapon( std::string name );
                [[nodiscard]] bool is_ammo_tracked_heal(
                    const std::string& name );
                [[nodiscard]] int ammo_charges_for_heal(
                    const std::string& name );
                [[nodiscard]] float heal_hold_threshold_s(
                    const std::string& name );
                [[nodiscard]] float get_damage(
                    engine::u_fort_weapon_item_definition* item_definition,
                    float distance_cm );
                [[nodiscard]] e_hit_zone classify_hit_zone(
                    const engine::f_vector& origin,
                    const engine::f_vector& direction,
                    const cache::c_player_pawn& victim );
                [[nodiscard]] float get_zone_multiplier(
                    engine::u_fort_weapon_item_definition* item_definition,
                    e_hit_zone zone );
            }


            void render(
                const actor_state_t& state,
                float now_s,
                layout_t& layout,
                bypass::overlay::point_t head_point,
                bool is_alive = true );
            class c_health_thread {
            public:





                ~c_health_thread( );
                void start( );
                void stop( );
                [[nodiscard]] actor_state_t get_state(
                    std::uint64_t actor ) const;
                [[nodiscard]] float now_s( ) const;
            private:
                static constexpr auto k_loop_interval = ch::milliseconds( 8 );

                std::atomic<bool>      m_running{ false };
                std::thread            m_thread;
                mutable std::shared_mutex m_lock;
                std::unordered_map<std::uint64_t, actor_state_t> m_actors;
                std::atomic<float>     m_now_s{ 0.f };


                void run( );

                void process(
                    const cache::c_player_pawn& pawn,
                    const std::vector< cache::c_player_pawn >& all_players,
                    float now,
                    bool do_weapon_reads );
                void evict(
                    const std::vector< cache::c_player_pawn >& players );
            };

        }
    }
}
