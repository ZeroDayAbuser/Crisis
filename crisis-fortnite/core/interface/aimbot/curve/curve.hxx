#pragma once

namespace curve {
	struct c_curve_state {
		double ctrl_x       { 0.0 };
		double ctrl_y       { 0.0 };
		double elapsed      { 0.0 };
		double duration     { 1.0 };
		bool   initialized  { false };
		double m_ctrl_along { 0.0 };
		double m_ctrl_perp  { 0.0 };

		void reset( ) {
			initialized  = false;
			elapsed      = 0.0;
			m_ctrl_along = 0.0;
			m_ctrl_perp  = 0.0;
		}
	};

	namespace helper {
		void init_control(
			c_curve_state& state,
			double from_x, double from_y,
			double to_x, double to_y,
			double noise );
		void rescale_ctrl(
			c_curve_state& state,
			double value_x, double value_y,
			double target_x, double target_y );
		void redirect(
			double& step_x, double& step_y,
			double delta_x, double delta_y );
		int chaikin_refine(
			double point0_x, double point0_y,
			double point1_x, double point1_y,
			double point2_x, double point2_y,
			double* output_x, double* output_y );
	}

	void bezier_point(
		double point0_x, double point0_y,
		double point1_x, double point1_y,
		double point2_x, double point2_y,
		double t,
		double& output_x, double& output_y );
	void bspline_span(
		double point0_x, double point0_y,
		double point1_x, double point1_y,
		double point2_x, double point2_y,
		double point3_x, double point3_y,
		double t,
		double& output_x, double& output_y );
	void bspline_at(
		const c_curve_state& state,
		double from_x, double from_y,
		double to_x, double to_y,
		double t,
		double& output_x, double& output_y );
	void apply_bezier(
		double& step_x, double& step_y,
		double value_x, double value_y,
		double target_x, double target_y,
		c_curve_state& state,
		double delta_time,
		double noise );
	void apply_bspline(
		double& step_x, double& step_y,
		double value_x, double value_y,
		double target_x, double target_y,
		c_curve_state& state,
		double delta_time,
		double noise );
	void apply_chaikin(
		double& step_x, double& step_y,
		double value_x, double value_y,
		double target_x, double target_y,
		c_curve_state& state,
		double delta_time,
		double noise );
	void curve_at(
		double point0_x, double point0_y,
		double point1_x, double point1_y,
		double point2_x, double point2_y,
		double t,
		double& output_x, double& output_y );
	void apply(
		double& step_x, double& step_y,
		double value_x, double value_y,
		double target_x, double target_y,
		c_curve_state& state,
		double delta_time,
		bool target_visible );
}
