#pragma once

namespace player_stats {
    // Plain snapshot of everything the render thread used to read directly
    // from g_api / raycast. Populated by c_player_stats_thread on its own
    // cadence; the render thread only ever loads a shared_ptr to one of
    // these, so it never blocks on stats.hxx's shared_mutex or the raycast
    // geometry_mutex. Defaults are all "safe to render as if data is
    // missing" so a first-frame miss (no entry yet for a newly seen player)
    // just draws without stats instead of stalling.
    struct player_stats_cache_t {
        bool          is_visible{ false };

        // has_brief mirrors g_api->brief()'s optional -- distinguishes "not
        // primed yet" (nullopt, brief absent) from "primed with a genuine
        // 0/0 KD" (brief present, fields zero). Without it a fresh account
        // and an unprimed one would render identically.
        bool          has_brief{ false };
        std::uint32_t kills{ 0 };
        std::uint32_t deaths{ 0 };
        double        win_rate{ 0.0 };
        std::uint32_t battle_pass_level{ 0 };
        std::wstring  brief_text;
        bypass::overlay::color_t brief_color{ };
        std::wstring  battle_pass_text;
        bypass::overlay::color_t battle_pass_color{ };

        // weapon_name records which weapon these were resolved for --
        // player.hxx must compare it against the frame's current weapon
        // name before trusting magazine/damage, since the cache can lag one
        // background cycle behind a weapon swap.
        std::string   weapon_name;
        std::uint32_t weapon_magazine{ 0 };

        // has_weapon_damage mirrors g_api->weapon_damage()'s optional --
        // the original render code gated the damage line on the optional
        // itself, not on the value, so a weapon genuinely recorded at 0
        // damage still rendered "0 dmg" while an unmatched name rendered
        // nothing.
        bool          has_weapon_damage{ false };
        double        weapon_damage{ 0.0 };
        std::wstring  weapon_damage_text;
    };

    // Keyed by player_key (reinterpret_cast<uint64_t>(c_player_pawn::m_mesh),
    // same identity used by player.hxx's fade_states and inventory's
    // g_inventories). atomic<shared_ptr<T>> gives the render thread a
    // lock-free load(acquire); g_player_cache_map_mutex only guards the map's
    // own structure (insertion/erasure), which only the background thread
    // ever performs.
    inline std::unordered_map<
        std::uint64_t,
        std::atomic<std::shared_ptr<player_stats_cache_t>>
    > g_player_cache;

    inline std::mutex g_player_cache_map_mutex;
}
