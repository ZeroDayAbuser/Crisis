#include <deps/includes.h>

namespace gui::esp::inventory {
    namespace {
        std::mutex inventories_mutex;
        std::unordered_map< std::uint64_t, player_inventory_t > inventories;
    }

    void track(
        std::uint64_t player_key,
        const std::string& current_weapon,
        engine::a_fort_weapon* weapon_actor ) {
        if ( current_weapon.empty( ) )
            return;

        {
            std::lock_guard lock( inventories_mutex );
            const auto it = inventories.find( player_key );
            if ( it != inventories.end( ) && current_weapon == it->second.last_equipped )
                return;
        }

        g_stats->push_icon_request( { player_key, current_weapon, weapon_actor } );
    }

    void prune( const std::unordered_set< std::uint64_t >& active_keys ) {
        std::lock_guard lock( inventories_mutex );
        for ( auto it = inventories.begin( ); it != inventories.end( ); )
            it = active_keys.count( it->first ) ? std::next( it ) : inventories.erase( it );
    }

    void render( std::uint64_t player_key, layout_t& layout, float fade_alpha ) {
        std::lock_guard lock( inventories_mutex );

        const auto found = inventories.find( player_key );
        if ( found == inventories.end( ) || found->second.weapons.empty( ) )
            return;

        constexpr float icon_size = 30.f;
        constexpr float icon_gap  = 4.f;
        constexpr float row_gap   = 4.f;

        const auto& weapons = found->second.weapons;
        const float total_width = weapons.size( ) * icon_size + ( weapons.size( ) - 1 ) * icon_gap;

        const float center_x = layout.box.x + layout.box.width * 0.5f;
        if ( layout.bottom_offset == 0.f )
            layout.bottom_offset = helper::bottom_gap;
        const float row_y = layout.box.y + layout.box.height + layout.bottom_offset;

        float x = center_x - total_width * 0.5f;
        for ( const auto& slot : weapons ) {
            bypass::overlay::image( slot.icon_id, rect_t{ x, row_y, icon_size, icon_size },
                { 255, 255, 255, 255 }, fade_alpha );
            x += icon_size + icon_gap;
        }

        layout.bottom_offset += icon_size + row_gap - 2;
    }

    void resolve_icon( const player_stats::icon_resolve_request_t& request ) {
        if ( request.weapon_name.empty( ) )
            return;

        const auto icon_id = g_api->weapon_icon_id( request.weapon_name );
        if ( !icon_id )
            return;

        std::lock_guard lock( inventories_mutex );
        auto& inventory = inventories[ request.player_key ];

        if ( request.weapon_name == inventory.last_equipped )
            return;

        auto it = std::find_if(
            inventory.weapons.begin( ),
            inventory.weapons.end( ),
            [ & ]( const weapon_slot_t& slot ) {
                return slot.name == request.weapon_name;
            } );
        if ( it != inventory.weapons.end( ) ) {
            it->address = request.weapon_actor;
            std::rotate( inventory.weapons.begin( ), it, std::next( it ) );
            inventory.last_equipped = request.weapon_name;
            return;
        }

        if ( inventory.weapons.size( ) < player_inventory_t::max_weapons ) {
            inventory.weapons.insert( inventory.weapons.begin( ),
                weapon_slot_t{ request.weapon_name, request.weapon_actor, icon_id } );
            inventory.last_equipped = request.weapon_name;
            return;
        }

        auto invalid_it = std::find_if(
            inventory.weapons.begin( ),
            inventory.weapons.end( ),
            [ ]( const weapon_slot_t& slot ) {
                return !utility::is_valid_pointer( slot.address );
            } );
        if ( invalid_it == inventory.weapons.end( ) )
            return;

        inventory.weapons.erase( invalid_it );
        inventory.weapons.insert( inventory.weapons.begin( ),
            weapon_slot_t{ request.weapon_name, request.weapon_actor, icon_id } );
        inventory.last_equipped = request.weapon_name;
    }
}

void player_stats::c_player_stats_thread::resolve_icon(
    const icon_resolve_request_t& request ) {
    gui::esp::inventory::resolve_icon( request );
}

void player_stats::c_player_stats_thread::prune_inventories(
    const std::unordered_set< std::uint64_t >& active_keys ) {
    gui::esp::inventory::prune( active_keys );
}
