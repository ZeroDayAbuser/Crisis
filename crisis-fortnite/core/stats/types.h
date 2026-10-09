#pragma once

namespace stats {
    enum class mode_t : std::uint8_t {
        overall,
        solo,
        duo,
        squad
    };

    struct br_mode_stats_t {
        std::uint64_t score{ };
        std::uint32_t wins{ };
        std::uint32_t top3{ };
        std::uint32_t top5{ };
        std::uint32_t top6{ };
        std::uint32_t top10{ };
        std::uint32_t top12{ };
        std::uint32_t top25{ };
        std::uint32_t kills{ };
        std::uint32_t deaths{ };
        std::uint32_t matches{ };
        double        kd{ };
        double        win_rate{ };
        std::uint64_t minutes_played{ };
        std::uint32_t players_outlived{ };
    };

    struct player_brief_t {
        std::uint32_t kills{ };
        std::uint32_t deaths{ };
        std::uint32_t wins{ };
        double        win_rate{ };
        std::uint32_t battle_pass_level{ };
    };

    struct br_player_stats_t {
        bool             valid{ false };
        std::string      account_id;
        std::string      display_name;
        std::uint32_t    battle_pass_level{ };
        br_mode_stats_t  overall;
        br_mode_stats_t  solo;
        br_mode_stats_t  duo;
        br_mode_stats_t  squad;
    };

    struct weapon_rarity_stats_t {
        std::string   rarity;
        double        dps{ };
        double        damage{ };
        double        fire_rate{ };
        double        reload_time{ };
        std::uint32_t magazine_size{ };
        double        structure_damage{ };
        // per-rarity icon path from the scraped page. relative to the html
        // file's directory (e.g. "./Weapons - Fortnite.GG_files/foo.jpg").
        std::string   icon_relative_path;
    };

    struct weapon_info_t {
        std::string                        name;
        std::string                        type;
        std::vector<weapon_rarity_stats_t> rarities;
    };

    struct weapon_disk_t {
        std::string   name;
        double        damage{ };
        std::uint32_t magazine_size{ };
        std::string   icon_path;
    };
}
