#pragma once

namespace gui {
    namespace esp {
        namespace inventory {
            struct weapon_slot_t {
                std::string             name;
                engine::a_fort_weapon*  address{ nullptr };
                std::uint32_t           icon_id{ 0 };
            };

            struct player_inventory_t {
                static constexpr std::size_t max_weapons = 5;
                std::vector< weapon_slot_t > weapons;
                std::string last_equipped;
            };

            void track(
                std::uint64_t player_key,
                const std::string& current_weapon,
                engine::a_fort_weapon* weapon_actor );
            void prune( const std::unordered_set< std::uint64_t >& active_keys );
            void render(
                std::uint64_t player_key,
                layout_t& layout,
                float fade_alpha );
        }
    }
}
