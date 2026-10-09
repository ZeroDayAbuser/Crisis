#pragma once

namespace config {
	namespace aimbot {
		inline bool enabled = true;
		inline bool curve = true;
		inline bool curve_behind_walls = true;
		inline bool bezier_curve = true;
		inline bool bspline_curve = true;
		inline bool chaikin_curve = true;
		inline bool staggered_curve = false;

		inline bool aim_at_npcs = true;
		inline bool aim_at_knocked = false;
		inline bool sticky_target = true;
		inline bool visible_only = false;
		inline bool memory_aim = false;
		inline int  aim_key = VK_RBUTTON;
		inline int  prediction_type = 1;
		inline int  target_priority = 0;

		inline float field_of_view = 45.f;
		inline float curve_noise = 2.f;
		inline float deadzone = 4.f;
		inline float smoothing = 3.f;
		inline float acceleration = 2.f;
		inline float acceleration_threshold = 290.f;
		inline float max_distance = 300.f;

		namespace visuals {
			inline bool curve_line = true;
			inline bool field_of_view = true;
			inline bool crosshair = false;
			inline bool fov_arrows = true;
			inline bool prediction_dot = true;
			inline bool  threat_warning       = false;
			inline bool  aim_indicator        = false;
			inline float aim_indicator_angle  = 8.0f;
			inline float threat_distance      = 60.0f;
			inline float threat_fov           = 30.0f;
		}
	}

	namespace player {
		inline bool material = false;

		namespace chams {
			inline bool enabled = true;
		}

		namespace box {
			inline bool  normal = true;
			inline bool  corner = false;
			inline bool  filled = true;
			inline bool  outline = true;
			inline float corner_smoothness = 0.18f;
		}

		namespace skeleton {
			inline bool normal = true;
			inline bool curved = false;
			inline bool outline = true;
			inline bool fingers = false;
			inline float curve_smoothness = 0.15f;
		}

		namespace snaplines {
			inline bool  enabled = false;
			inline int   position = 2;
			inline bool  outline = true;
			inline bool  curved = false;
			inline float smoothing = 0.0f;
		}

		namespace tracers {
			inline bool  enabled            = false;
			inline bool  outline            = true;
			inline float fade_ms            = 1600.0f;
			inline float sample_interval_ms = 40.0f;
			inline float thickness          = 1.6f;
		}

		namespace stats {
			inline bool player_name = true;
			inline bool battle_pass = false;
			inline bool kill_deaths = true;
			inline bool winrate = false;
			inline bool rank = true;
			inline bool platform = true;
			inline bool team_marker = true;
			inline bool streamsnipe = true;

			inline bool state = true;
			inline bool distance = true;
			inline bool inventory = false;
			inline bool weapon_name = true;
			inline bool weapon_damage = true;
			inline bool weapon_ammo = true;
			inline bool weapon_reloading = false;
			inline bool health_bar = true;
			inline int  health_bar_position = 0;
			inline int  shield_bar_position = 3;
			inline bool hit_markers = true;

			enum text_slot_t : int {
				slot_name = 0,
				slot_platform,
				slot_rank,
				slot_kd,
				slot_winrate,
				slot_battle_pass,
				slot_streamsnipe,
				slot_state,
				slot_distance,
				slot_weapon_name,
				slot_weapon_ammo,
				slot_weapon_damage,
				slot_reloading,
				slot_count
			};

			// Size tier per slot (see esp/text.hxx: 0 = small ~11.5,
			// 1 = medium ~13.5, 2 = regular ~15). slot_state (index 7)
			// sits at MEDIUM so the ambient status text reads quieter
			// than the primary stats. Weight for that slot is handled at
			// the draw site (regular, not semi_bold).
			inline int text_sizes[ slot_count ] = { 2, 1, 1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 1 };

			struct rgba8_t { std::uint8_t r, g, b, a; };
			inline bool  text_color_override_enabled[ slot_count ] { };
			inline rgba8_t text_color_override[ slot_count ] {
				{ 255, 255, 255, 255 }, { 255, 255, 255, 255 }, { 255, 255, 255, 255 },
				{ 255, 255, 255, 255 }, { 255, 255, 255, 255 }, { 255, 255, 255, 255 },
				{ 255, 255, 255, 255 }, { 255, 255, 255, 255 }, { 255, 255, 255, 255 },
				{ 255, 255, 255, 255 }, { 255, 255, 255, 255 }, { 255, 255, 255, 255 },
				{ 255, 255, 255, 255 },
			};

			inline bool  text_outline_enabled[ slot_count ] {
				true, true, true, true, true, true, true, true, true, true, true, true, true,
			};
			inline float text_outline_width  [ slot_count ] {
				1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f,
			};

			inline bool text_background_enabled[ slot_count ] { };

			inline int text_anchors[ slot_count ] = { 0, 0, 0, 0, 0, 0, 0,   3, 1, 1, 1, 1,   3 };
			inline int text_orders [ slot_count ] = { 6, 3, 4, 5, 2, 1, 0,   1, 5, 0, 1, 4,   0 };
		}
	}

	namespace loot {
		inline bool pickup = true;
		inline float pickup_distance = 200.0f;
		inline bool chest = true;
		inline float chest_distance = 150.0f;
		inline bool weakspot = true;
		inline float weakspot_distance = 120.0f;
	}

	namespace settings {
		inline bool performance_mode = true;
		inline bool frame_limiter = false;
		inline float framerate_cap = 120.f;
	}

	namespace triggerbot {
		inline bool enabled          = false;
		inline bool visible_only     = true;
		inline bool fire_without_aim = false;
		inline float deadzone_px     = 15.f;
		inline float max_distance_m  = 50.f;
		inline float pre_fire_ms     = 20.f;
		inline float post_fire_ms    = 120.f;
		inline std::uint64_t weapon_mask = ~std::uint64_t( 0 );
	}

	namespace widgets {
		inline bool  radar          = true;
		inline bool  top_bar        = true;
		inline bool  radar_tracers  = true;
		inline float radar_range    = 120.0f;
		inline float radar_size     = 1.0f;
	}

	namespace exploits {
		// Casino-unlockable exploits. All bools default to FALSE so a
		// fresh account has every feature locked; winning the feature
		// via the crate in the Profile → Casino tab flips the bool to
		// true (see menu.cxx crate spin-landing block). Existing
		// features (magic_bullet, no_spread) sit in this same list so
		// the unlock sync handler can address them all uniformly.
		inline bool spin_bot      = false;
		inline bool rapid_fire    = false;
		inline bool wall_pierce   = false;
		inline bool silent_aim    = false;
		inline bool fov_changer   = false;
		inline bool car_fly       = false;
		inline bool infinite_fuel = false;
		inline bool chams         = false;
		inline bool no_recoil     = false;
		inline bool fire_rate     = false;

		inline bool magic_bullet = false;

		// no_spread — force the current weapon's reticle-bloom / spread
		// token to `spread_value` every local-thread iteration. Walker
		// in cache/local.cxx covers all six known spread offsets so
		// per-weapon variance is handled without weapon classification.
		//   spread_value : target value written into the decrypted slot
		//                  (0.0 = perfect accuracy / no reticle bloom).
		//   spread_max   : upper bound gate — only overrides the slot
		//                  when the currently-decrypted spread exceeds
		//                  this. 0.0 (default) = always override; set
		//                  higher to allow some natural spread up to
		//                  the cap while still preventing large blooms.
		inline bool  no_spread    = false;
		inline float spread_value = 0.0f;
		inline float spread_max   = 0.0f;
	}

	namespace bullet_tracers {
		inline bool  enabled       = true;
		inline float lifetime      = 1.5f;
		inline int   max_count     = 48;
		inline float thickness     = 1.5f;
		inline bool  outline       = true;
		inline bool  fade_out      = true;
		inline float fade_duration = 0.7f;
		inline bool  travel_anim   = true;
		inline float travel_speed  = 1.5f;
		inline bool  use_accent    = true;
		inline bool  muzzle_line   = true;
	}

}
