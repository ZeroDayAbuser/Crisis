#pragma once

namespace aimbot {
    namespace prediction {
        enum class e_type : std::uint8_t {
            none,
            velocity,
            bullet
        };




        engine::f_vector base_offset(
            const cache::c_player_pawn& target,
            double delta_time );
        engine::f_vector velocity_offset(
            const cache::c_player_pawn& target,
            double delta_time,
            const engine::f_vector& camera_velocity );
        engine::f_vector bullet_offset(
            const cache::c_player_pawn& target );
        engine::f_vector apply(
            const cache::c_player_pawn& target,
            const engine::f_vector& aim_position );
    }
}
