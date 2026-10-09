#include <deps/includes.h>

namespace aimbot::prediction {
        engine::f_vector base_offset(
            const cache::c_player_pawn& target,
            double delta_time )
        {
            if ( !target.m_velocity.is_valid( ) )
                return { };
            if ( delta_time > 0.1 )
                delta_time = 0.1;
            return target.m_velocity * delta_time;
        }
        engine::f_vector velocity_offset(
            const cache::c_player_pawn& target,
            double delta_time,
            const engine::f_vector& camera_velocity )
        {
            if ( !target.m_velocity.is_valid( ) )
                return { };

            const float t = std::clamp(
                static_cast< float >( config::aimbot::smoothing ) *
                static_cast< float >( delta_time ),
                0.016f, 0.120f );

            const auto relative_vel = target.m_velocity - camera_velocity;

            auto offset = relative_vel * static_cast< double >( t );
            if ( target.m_accel_z < -100.0f )
                offset.z += 0.5 * static_cast< double >( target.m_accel_z ) *
                             static_cast< double >( t * t );

            return offset;
        }
        engine::f_vector bullet_offset( const cache::c_player_pawn& target ) {
            ( void )target;
            return { };
        }
        engine::f_vector apply(
            const cache::c_player_pawn& target,
            const engine::f_vector& aim_position )
        {
            if ( config::aimbot::prediction_type == static_cast< int >( e_type::none ) )
                return aim_position;

            static auto            s_last_time   = std::chrono::steady_clock::now( );
            const auto             now           = std::chrono::steady_clock::now( );
            double                 delta_time    =
                std::chrono::duration< double >( now - s_last_time ).count( );
            s_last_time = now;

            if ( delta_time <= 0.0 || delta_time > 0.1 )
                delta_time = 1.0 / 60.0;

            static engine::f_vector s_last_camera { };
            static bool             s_camera_init  = false;

            engine::f_vector camera_vel { };
            if ( s_camera_init ) {
                const auto raw = ( cache::camera_location - s_last_camera ) *
                                 ( 1.0 / delta_time );

                constexpr double max_speed = 2500.0;
                if ( std::isfinite( raw.x ) && std::isfinite( raw.y ) &&
                     std::isfinite( raw.z ) && raw.length( ) <= max_speed )
                    camera_vel = raw;
            }
            s_last_camera = cache::camera_location;
            s_camera_init = true;

            engine::f_vector offset = base_offset( target, delta_time );

            switch ( static_cast< e_type >( config::aimbot::prediction_type ) ) {
                case e_type::velocity:
                    offset = offset + velocity_offset( target, delta_time, camera_vel );
                    break;
                case e_type::bullet:
                    offset = offset + bullet_offset( target );
                    break;
                default:
                    break;
            }

            return aim_position + offset;
        }
}
