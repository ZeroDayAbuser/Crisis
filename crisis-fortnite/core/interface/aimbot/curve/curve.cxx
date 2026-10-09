#include <deps/includes.h>

namespace curve {
    namespace helper {
        void init_control(
            c_curve_state& state,
            double from_x, double from_y,
            double to_x, double to_y,
            double noise ) {
            const auto delta_x = to_x - from_x;
            const auto delta_y = to_y - from_y;
            const auto length = ::hypot( delta_x, delta_y );
            state.elapsed = 0.0;
            state.duration = ( 0.04 > length / 800.0 ) ? 0.04 : length / 800.0;
            state.initialized = true;
            if ( length < 0.01 ) {
                state.ctrl_x = from_x;
                state.ctrl_y = from_y;
                state.m_ctrl_along = 0.0;
                state.m_ctrl_perp = 0.0;
                return;
            }

            const auto unit_x = delta_x / length;
            const auto unit_y = delta_y / length;
            double jitter = 0.0;
            if ( noise > 0.0 ) {
                static std::uint32_t seed = 0xDEADBEEFu;
                seed = seed * 1664525u + 1013904223u;
                jitter = ( static_cast< double >( seed & 0xFFFF ) /
                    65535.0 * 2.0 - 1.0 ) * noise;
            }

            state.ctrl_x = ( from_x + to_x ) * 0.5 +
                ( -unit_y ) * jitter * length * 0.25;
            state.ctrl_y = ( from_y + to_y ) * 0.5 +
                unit_x * jitter * length * 0.25;
            const double relative_x = state.ctrl_x - from_x;
            const double relative_y = state.ctrl_y - from_y;
            state.m_ctrl_along =
                ( relative_x * unit_x + relative_y * unit_y ) / length;
            state.m_ctrl_perp =
                ( -relative_x * unit_y + relative_y * unit_x ) / length;
        }

        void rescale_ctrl(
            c_curve_state& state,
            double value_x, double value_y,
            double target_x, double target_y ) {
            const double relative_target_x = target_x - value_x;
            const double relative_target_y = target_y - value_y;
            const double current_length =
                ::hypot( relative_target_x, relative_target_y );
            if ( current_length < 1e-9 )
                return;

            const double unit_x = relative_target_x / current_length;
            const double unit_y = relative_target_y / current_length;
            state.ctrl_x = value_x +
                ( state.m_ctrl_along * unit_x - state.m_ctrl_perp * unit_y ) *
                current_length;
            state.ctrl_y = value_y +
                ( state.m_ctrl_along * unit_y + state.m_ctrl_perp * unit_x ) *
                current_length;
        }

        void redirect(
            double& step_x, double& step_y,
            double delta_x, double delta_y ) {
            const auto original_length = ::hypot( step_x, step_y );
            const auto delta_length = ::hypot( delta_x, delta_y );
            if ( original_length < 1e-9 || delta_length < 1e-9 )
                return;

            constexpr double minimum_ratio = 0.15;
            double normalized_x = delta_x / delta_length;
            double normalized_y = delta_y / delta_length;
            if ( std::fabs( normalized_x ) < minimum_ratio )
                normalized_x = ( normalized_x < 0.0 ? -1.0 : 1.0 ) *
                    minimum_ratio;
            if ( std::fabs( normalized_y ) < minimum_ratio )
                normalized_y = ( normalized_y < 0.0 ? -1.0 : 1.0 ) *
                    minimum_ratio;

            const auto normalized_length =
                ::hypot( normalized_x, normalized_y );
            step_x = normalized_x / normalized_length * original_length;
            step_y = normalized_y / normalized_length * original_length;
        }

        int chaikin_refine(
            double point0_x, double point0_y,
            double point1_x, double point1_y,
            double point2_x, double point2_y,
            double* output_x, double* output_y ) {
            double points_x[ 16 ] { point0_x, point1_x, point2_x };
            double points_y[ 16 ] { point0_y, point1_y, point2_y };
            int count = 3;

            for ( int iteration = 0; iteration < 2; ++iteration ) {
                double next_x[ 16 ] { };
                double next_y[ 16 ] { };
                int next_count = 0;
                next_x[ next_count ] = points_x[ 0 ];
                next_y[ next_count++ ] = points_y[ 0 ];
                for ( int index = 0; index < count - 1; ++index ) {
                    next_x[ next_count ] =
                        0.75 * points_x[ index ] + 0.25 * points_x[ index + 1 ];
                    next_y[ next_count++ ] =
                        0.75 * points_y[ index ] + 0.25 * points_y[ index + 1 ];
                    next_x[ next_count ] =
                        0.25 * points_x[ index ] + 0.75 * points_x[ index + 1 ];
                    next_y[ next_count++ ] =
                        0.25 * points_y[ index ] + 0.75 * points_y[ index + 1 ];
                }
                next_x[ next_count ] = points_x[ count - 1 ];
                next_y[ next_count++ ] = points_y[ count - 1 ];
                count = next_count;
                for ( int index = 0; index < count; ++index ) {
                    points_x[ index ] = next_x[ index ];
                    points_y[ index ] = next_y[ index ];
                }
            }

            const int output_count = count < 13 ? count : 13;
            for ( int index = 0; index < output_count; ++index ) {
                output_x[ index ] = points_x[ index ];
                output_y[ index ] = points_y[ index ];
            }
            return output_count;
        }
    }

    void bezier_point(
        double point0_x, double point0_y,
        double point1_x, double point1_y,
        double point2_x, double point2_y,
        double t,
        double& output_x, double& output_y ) {
        const auto inverse_t = 1.0 - t;
        output_x = inverse_t * inverse_t * point0_x +
            2.0 * inverse_t * t * point1_x + t * t * point2_x;
        output_y = inverse_t * inverse_t * point0_y +
            2.0 * inverse_t * t * point1_y + t * t * point2_y;
    }

    void bspline_span(
        double point0_x, double point0_y,
        double point1_x, double point1_y,
        double point2_x, double point2_y,
        double point3_x, double point3_y,
        double t,
        double& output_x, double& output_y ) {
        const auto squared_t = t * t;
        const auto cubed_t = squared_t * t;
        const auto basis0 =
            ( 1.0 - 3.0 * t + 3.0 * squared_t - cubed_t ) / 6.0;
        const auto basis1 = ( 4.0 - 6.0 * squared_t + 3.0 * cubed_t ) / 6.0;
        const auto basis2 =
            ( 1.0 + 3.0 * t + 3.0 * squared_t - 3.0 * cubed_t ) / 6.0;
        const auto basis3 = cubed_t / 6.0;
        output_x = basis0 * point0_x + basis1 * point1_x +
            basis2 * point2_x + basis3 * point3_x;
        output_y = basis0 * point0_y + basis1 * point1_y +
            basis2 * point2_y + basis3 * point3_y;
    }

    void bspline_at(
        const c_curve_state& state,
        double from_x, double from_y,
        double to_x, double to_y,
        double t,
        double& output_x, double& output_y ) {
        const auto ghost_start_x = 2.0 * from_x - state.ctrl_x;
        const auto ghost_start_y = 2.0 * from_y - state.ctrl_y;
        const auto ghost_end_x = 2.0 * to_x - state.ctrl_x;
        const auto ghost_end_y = 2.0 * to_y - state.ctrl_y;
        double span1_x, span1_y, span2_x, span2_y;
        bspline_span(
            ghost_start_x, ghost_start_y,
            from_x, from_y,
            state.ctrl_x, state.ctrl_y,
            to_x, to_y,
            t * 0.5,
            span1_x, span1_y );
        bspline_span(
            from_x, from_y,
            state.ctrl_x, state.ctrl_y,
            to_x, to_y,
            ghost_end_x, ghost_end_y,
            0.5 + t * 0.5,
            span2_x, span2_y );
        output_x = span1_x * ( 1.0 - t ) + span2_x * t;
        output_y = span1_y * ( 1.0 - t ) + span2_y * t;
    }

    void apply_bezier(
        double& step_x, double& step_y,
        double value_x, double value_y,
        double target_x, double target_y,
        c_curve_state& state,
        double delta_time,
        double noise ) {
        if ( !state.initialized )
            helper::init_control(
                state, value_x, value_y, target_x, target_y, noise );
        else
            helper::rescale_ctrl(
                state, value_x, value_y, target_x, target_y );
        state.elapsed = state.elapsed + delta_time < state.duration
            ? state.elapsed + delta_time
            : state.duration;
        const auto t = state.duration > 0.0
            ? ( state.elapsed / state.duration < 1.0
                ? state.elapsed / state.duration
                : 1.0 )
            : 1.0;
        const auto inverse_t = 1.0 - t;
        helper::redirect(
            step_x,
            step_y,
            2.0 * ( inverse_t * ( state.ctrl_x - value_x ) +
                t * ( target_x - state.ctrl_x ) ),
            2.0 * ( inverse_t * ( state.ctrl_y - value_y ) +
                t * ( target_y - state.ctrl_y ) ) );
    }

    void apply_bspline(
        double& step_x, double& step_y,
        double value_x, double value_y,
        double target_x, double target_y,
        c_curve_state& state,
        double delta_time,
        double noise ) {
        if ( !state.initialized )
            helper::init_control(
                state, value_x, value_y, target_x, target_y, noise );
        else
            helper::rescale_ctrl(
                state, value_x, value_y, target_x, target_y );
        state.elapsed = state.elapsed + delta_time < state.duration
            ? state.elapsed + delta_time
            : state.duration;
        const auto t = state.duration > 0.0
            ? ( state.elapsed / state.duration < 1.0
                ? state.elapsed / state.duration
                : 1.0 )
            : 1.0;
        double spline_x, spline_y;
        bspline_at(
            state,
            value_x,
            value_y,
            target_x,
            target_y,
            t,
            spline_x,
            spline_y );
        helper::redirect(
            step_x,
            step_y,
            spline_x - value_x,
            spline_y - value_y );
    }

    void apply_chaikin(
        double& step_x, double& step_y,
        double value_x, double value_y,
        double target_x, double target_y,
        c_curve_state& state,
        double delta_time,
        double noise ) {
        if ( !state.initialized )
            helper::init_control(
                state, value_x, value_y, target_x, target_y, noise );
        else
            helper::rescale_ctrl(
                state, value_x, value_y, target_x, target_y );
        state.elapsed = state.elapsed + delta_time < state.duration
            ? state.elapsed + delta_time
            : state.duration;
        const auto t = state.duration > 0.0
            ? ( state.elapsed / state.duration < 1.0
                ? state.elapsed / state.duration
                : 1.0 )
            : 1.0;
        double points_x[ 13 ] { };
        double points_y[ 13 ] { };
        const int count = helper::chaikin_refine(
            value_x,
            value_y,
            state.ctrl_x,
            state.ctrl_y,
            target_x,
            target_y,
            points_x,
            points_y );
        if ( count < 2 )
            return;

        const auto segment_t = t * static_cast< double >( count - 1 );
        const int segment = static_cast< int >( segment_t ) < count - 2
            ? static_cast< int >( segment_t )
            : count - 2;
        const auto local_t = segment_t - static_cast< double >( segment );
        const auto delta_x =
            points_x[ segment ] * ( 1.0 - local_t ) +
            points_x[ segment + 1 ] * local_t - value_x;
        const auto delta_y =
            points_y[ segment ] * ( 1.0 - local_t ) +
            points_y[ segment + 1 ] * local_t - value_y;
        helper::redirect( step_x, step_y, delta_x, delta_y );
    }

    void curve_at(
        double point0_x, double point0_y,
        double point1_x, double point1_y,
        double point2_x, double point2_y,
        double t,
        double& output_x, double& output_y ) {
        if ( config::aimbot::bezier_curve ) {
            bezier_point(
                point0_x, point0_y,
                point1_x, point1_y,
                point2_x, point2_y,
                t,
                output_x, output_y );
        }
        else if ( config::aimbot::bspline_curve ) {
            c_curve_state state { };
            state.ctrl_x = point1_x;
            state.ctrl_y = point1_y;
            bspline_at(
                state,
                point0_x,
                point0_y,
                point2_x,
                point2_y,
                t,
                output_x,
                output_y );
        }
        else if ( config::aimbot::chaikin_curve ) {
            double points_x[ 13 ] { };
            double points_y[ 13 ] { };
            const int count = helper::chaikin_refine(
                point0_x,
                point0_y,
                point1_x,
                point1_y,
                point2_x,
                point2_y,
                points_x,
                points_y );
            if ( count < 2 ) {
                output_x = point2_x;
                output_y = point2_y;
                return;
            }
            const auto segment_t = t * static_cast< double >( count - 1 );
            const int segment = static_cast< int >( segment_t ) < count - 2
                ? static_cast< int >( segment_t )
                : count - 2;
            const auto local_t = segment_t - static_cast< double >( segment );
            output_x = points_x[ segment ] * ( 1.0 - local_t ) +
                points_x[ segment + 1 ] * local_t;
            output_y = points_y[ segment ] * ( 1.0 - local_t ) +
                points_y[ segment + 1 ] * local_t;
        }
        else {
            output_x = point0_x + t * ( point2_x - point0_x );
            output_y = point0_y + t * ( point2_y - point0_y );
        }
    }

    void apply(
        double& step_x, double& step_y,
        double value_x, double value_y,
        double target_x, double target_y,
        c_curve_state& state,
        double delta_time,
        bool target_visible ) {
        if ( !config::aimbot::curve )
            return;
        if ( !config::aimbot::curve_behind_walls && !target_visible )
            return;

        const auto noise =
            static_cast< double >( config::aimbot::curve_noise );
        if ( config::aimbot::bezier_curve )
            apply_bezier(
                step_x, step_y,
                value_x, value_y,
                target_x, target_y,
                state, delta_time, noise );
        else if ( config::aimbot::bspline_curve )
            apply_bspline(
                step_x, step_y,
                value_x, value_y,
                target_x, target_y,
                state, delta_time, noise );
        else if ( config::aimbot::chaikin_curve )
            apply_chaikin(
                step_x, step_y,
                value_x, value_y,
                target_x, target_y,
                state, delta_time, noise );
    }
}
